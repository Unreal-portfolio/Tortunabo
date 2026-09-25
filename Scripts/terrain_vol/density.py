"""Campo de densidad del mapa: D(x, y, z) > 0 es roca o arena, < 0 es aire.

1. Campos 2D del mapa entero (una vez): relieve de cada zona mezclado por peso, suelo de los
   caminos, cercania a las paredes y densidad del bosque.
2. Erosion (una vez, sobre el relieve 2D): gotas de agua que arrastran y depositan sedimento
   (carcavas en las paredes, derrubios al pie, bordes gastados) y un paso termico que tumba
   las puntas. Los caminos se protegen: su eje queda como se diseno.
3. Densidad 3D por trozo: el relieve erosionado, ruido 3D suave en las paredes (voladizos) y
   los tuneles del canon excavados dentro de una dorsal.

Los campos 2D se calculan sobre el mapa entero, asi que cada trozo lee exactamente los mismos
valores en su borde que su vecino.
"""

from __future__ import annotations

import math
from dataclasses import dataclass, fields as dataclass_fields

import numpy as np
from scipy import ndimage
from scipy.spatial import cKDTree

from .layout import CELL_M, CELL_SAMPLES, MAP_MAX_M, MAP_MIN_M, STEP_XY_M, WATER_M, Z_MAX_M
from .network import Network, build_network
from .noise import Fbm2D, ValueNoise2D, ValueNoise3D
from .route import ZONE_NAMES, Route, ZoneField, build_route

# ── Parametros ────────────────────────────────────────────────────────────────────
CLIFF_LEVEL_M = 4.5                     # separacion entre niveles del laberinto de acantilados
CLIFF_LEVELS = 3
MARSH_PATH_TOP_M = WATER_M + 1.3        # crestas por las que se cruza la zona encharcada
END_PLAZA_M = WATER_M + 1.8             # plaza de llegada, seca, sobre la playa
MARSH_JUMP_EVERY_M = 26.0               # cada cuanto la cresta tiene un hueco que se salta
TUNNEL_HALF_WIDTH_M = 5.0
TUNNEL_HEIGHT_M = 5.5
TUNNEL_LENGTH_M = (26.0, 36.0)
OUTER_WALL_M = 15.0
EDGE_WALL_M = 20.0
TOP_LIMIT_M = Z_MAX_M - 2.0
GRID_PAD = 1                            # muestras de margen alrededor del mapa (normales del borde)

EROSION_DROPS = 60000
EROSION_STEPS = 40
THERMAL_TALUS = 3.0                     # desnivel maximo entre vecinas (1 m): solo tumba puntas, no paredes


def smooth(e0: float, e1: float, x):
    t = np.clip((x - e0) / (e1 - e0), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def strata(h, step: float):
    """Repisas de canto vivo (arenisca)."""
    q = h / step
    f = q - np.floor(q)
    return (np.floor(q) + smooth(0.7, 1.0, f)) * step


@dataclass
class Fields:
    """Campos 2D (forma (nx, ny)) del mapa entero o de un trozo."""
    weights: dict[str, np.ndarray]
    d_route: np.ndarray
    s_route: np.ndarray
    height: np.ndarray          # relieve base (m)
    floor: np.ndarray           # cota del suelo de los caminos (m), base de tuneles y paredes
    wall_band: np.ndarray       # 0..1: cerca de una pared de roca (voladizos)
    foliage: np.ndarray         # densidad del bosque de algas 0..1
    tunnel: np.ndarray          # 0..1: dentro del tramo de tunel de la ruta
    path: np.ndarray            # 0..1: eje de un camino (protegido de la erosion)

    def window(self, i0: int, i1: int, j0: int, j1: int) -> "Fields":
        out = {}
        for f in dataclass_fields(self):
            v = getattr(self, f.name)
            out[f.name] = {k: a[i0:i1, j0:j1] for k, a in v.items()} if isinstance(v, dict) else v[i0:i1, j0:j1]
        return Fields(**out)


class MapModel:
    def __init__(self, seed: int, erode: bool = True):
        self.seed = seed
        rng = np.random.default_rng(seed)
        self.route: Route = build_route(rng)
        self.zones = ZoneField(rng, self.route)
        self.nets: dict[str, Network] = {
            "cliffs": build_network(rng, self.zones, "cliffs", 17.0, 0.15, level_step=CLIFF_LEVEL_M,
                                    level_count=CLIFF_LEVELS, decoys=True),
            "canyon": build_network(rng, self.zones, "canyon", 28.0, 0.0, reach_share=0.75, bend=0.25),
            "algae": build_network(rng, self.zones, "algae", 14.0, 0.5, loops_anywhere=True, decoys=True),
        }
        self.net_samples = {z: n.sample() for z, n in self.nets.items()}
        self.net_trees = {z: cKDTree(pts) for z, (pts, _) in self.net_samples.items()}
        self.clearings = {z: self._pick_clearings(rng, n) for z, n in self.nets.items() if z != "algae"}
        self.tunnels = self._plan_tunnels(rng)

        self.n_small = Fbm2D(rng, 22.0, 2)
        self.n_mid = Fbm2D(rng, 60.0, 3)
        self.n_big = Fbm2D(rng, 160.0, 2)
        self.n_width = Fbm2D(rng, 35.0, 2)
        self.n_buttress = Fbm2D(rng, 9.0, 2)
        self.n_crack = ValueNoise2D(rng, 16.0)
        self.n_crack_mask = Fbm2D(rng, 40.0, 2)
        self.n_talus = Fbm2D(rng, 12.0, 2)
        self.n_dune_warp = Fbm2D(rng, 70.0, 2)
        self.n_dune_amp = Fbm2D(rng, 90.0, 2)
        self.n_edge = Fbm2D(rng, 45.0, 3)
        self.n_wall3d = ValueNoise3D(rng, 6.5)
        self.dune_angle = float(rng.uniform(0.0, math.pi))
        self.dune_angle_2 = self.dune_angle + float(rng.uniform(0.6, 1.1))
        self.n_dune_mix = Fbm2D(rng, 120.0, 2)
        self.n_pond = Fbm2D(rng, 34.0, 3)
        # Todo lo anadido despues de la version que Rodrigo dio por buena (2026-09-25) sale de
        # un generador aparte: la misma semilla sigue dando el mismo mapa.
        extra = np.random.default_rng(seed + 7)
        self.tunnel_climb = [float(extra.choice([-1.0, 1.0]) * extra.uniform(1.5, 3.0)) for _ in self.tunnels]
        self.n_tunnel = Fbm2D(extra, 9.0, 2)
        self.n_crest = Fbm2D(extra, 11.0, 2)
        self.n_beach_hills = Fbm2D(extra, 48.0, 3)

        # Campos 2D del mapa entero, con una muestra de margen alrededor.
        count = int(round((MAP_MAX_M - MAP_MIN_M) / STEP_XY_M)) + 1
        self.axis = MAP_MIN_M + STEP_XY_M * np.arange(-GRID_PAD, count + GRID_PAD)
        X, Y = np.meshgrid(self.axis, self.axis, indexing="ij")
        self.grid = self._fields(X, Y)
        if erode:
            self.grid.height = self._erode(self.grid, np.random.default_rng(seed + 1))

    # ── Planificacion ────────────────────────────────────────────────────────────
    @staticmethod
    def _pick_clearings(rng: np.random.Generator, net: Network) -> np.ndarray:
        """Claros en algunos nodos (x, y, radio, cota): plazas para puzles y respiro."""
        nodes = [k for k in range(len(net.nodes)) if k not in set(net.chain)]
        count = max(1, len(nodes) // 8)
        picked = rng.choice(nodes, size=min(count, len(nodes)), replace=False) if nodes else []
        levels = net.levels if net.levels is not None else np.zeros(len(net.nodes))
        return np.array([[net.nodes[k][0], net.nodes[k][1], rng.uniform(5.5, 8.5), levels[k]]
                         for k in picked]).reshape(-1, 4)

    def _plan_tunnels(self, rng: np.random.Generator) -> list[tuple[float, float]]:
        """2-3 tramos de tunel en el canon, en tramos casi rectos de la ruta y separados."""
        s0, s1 = self.route.zone_range("canyon")
        tunnels: list[tuple[float, float]] = []
        wanted = int(rng.integers(2, 4))
        for _ in range(300):
            if len(tunnels) == wanted:
                break
            length = float(rng.uniform(*TUNNEL_LENGTH_M))
            if s1 - s0 - 30.0 - length <= 0:
                break
            a = float(rng.uniform(s0 + 15.0, s1 - 15.0 - length))
            b = a + length
            if any(not (b + 20.0 < ta or a - 20.0 > tb) for ta, tb in tunnels):
                continue
            # Se admiten curvas (tunel natural), no un giro cerrado.
            d0, d1 = self.route.direction_at(a), self.route.direction_at(b)
            if float(np.dot(d0, d1)) < math.cos(math.radians(70.0)):
                continue
            tunnels.append((a, b))
        return sorted(tunnels)

    # ── Utilidades de campo ──────────────────────────────────────────────────────
    def _net(self, zone: str, X, Y, smooth_m: float = 0.0):
        """(distancia a la red, cota del suelo de la red). smooth_m > 0: la cota es una media
        pesada de los puntos cercanos, sin saltos en la bisectriz entre dos caminos de niveles
        distintos (para la meseta); con 0, la del punto mas cercano (para el suelo del camino)."""
        pts = np.stack([X.ravel(), Y.ravel()], axis=1)
        floors = self.net_samples[zone][1]
        if smooth_m <= 0.0:
            d, k = self.net_trees[zone].query(pts)
            return d.reshape(X.shape), floors[k].reshape(X.shape)
        d, k = self.net_trees[zone].query(pts, k=32)
        weight = np.exp(-0.5 * ((d - d[:, :1]) / smooth_m) ** 2)
        level = (floors[k] * weight).sum(axis=1) / weight.sum(axis=1)
        return d[:, 0].reshape(X.shape), level.reshape(X.shape)

    def _clearing(self, zone: str, X, Y):
        weight, level = np.zeros_like(X), np.zeros_like(X)
        for cx, cy, r, lv in self.clearings.get(zone, np.zeros((0, 4))):
            m = 1.0 - smooth(r, r + 2.0, np.hypot(X - cx, Y - cy))
            level = np.where(m > weight, lv, level)
            weight = np.maximum(weight, m)
        return weight, level

    def _dune_set(self, X, Y, angle: float, wave: float):
        along = X * math.cos(angle) + Y * math.sin(angle)
        across = -X * math.sin(angle) + Y * math.cos(angle)
        phase = (along + 0.9 * wave * self.n_dune_warp(X, Y) + 0.25 * across * self.n_dune_mix(X, Y)) / wave
        frac = phase - np.floor(phase)
        profile = np.where(frac < 0.72, frac / 0.72, (1.0 - frac) / 0.28)
        return smooth(0.0, 1.0, profile)

    def _dunes(self, X, Y, amp, wave: float):
        """Dunas con cara de avalancha, mezcla de dos direcciones de viento y crestas que
        crecen y se apagan: sin rayas paralelas regulares."""
        mix = smooth(0.35, 0.65, self.n_dune_mix.unit(X, Y))
        field = self._dune_set(X, Y, self.dune_angle, wave) * (1 - mix) \
            + self._dune_set(X, Y, self.dune_angle_2, wave * 1.3) * mix
        strength = smooth(0.2, 0.8, self.n_dune_amp.unit(X, Y))
        return amp * field * (0.15 + 0.85 * strength)

    def _tunnel_mask(self, s, d):
        out = np.zeros_like(s)
        for a, b in self.tunnels:
            out = np.maximum(out, smooth(a - 1.0, a + 1.0, s) * (1.0 - smooth(b - 1.0, b + 1.0, s)))
        return out * (1.0 - smooth(TUNNEL_HALF_WIDTH_M + 6.0, TUNNEL_HALF_WIDTH_M + 12.0, d))

    def _ridge(self, s, d):
        out = np.zeros_like(s)
        for a, b in self.tunnels:
            out = np.maximum(out, smooth(a - 12.0, a + 2.0, s) * (1.0 - smooth(b - 2.0, b + 12.0, s)))
        return 4.0 * out * (1.0 - smooth(18.0, 35.0, d))

    def _rock_walls(self, X, Y, d, hw, f, top):
        """Pasillo en roca con aspecto erosionado: contrafuertes, grietas que se meten en la
        pared y un talud de derrubios al pie. Devuelve (relieve, banda de pared, eje)."""
        face = d - hw                                   # > 0: detras de la cara de la pared
        corridor = 1.0 - smooth(0.0, 1.1, face)
        # Grietas: lineas de ruido (pasos por cero) que cortan la pared casi hasta el suelo,
        # solo cerca de la cara y solo en parte de las paredes.
        crack = (1.0 - smooth(0.04, 0.13, np.abs(self.n_crack(X, Y)))) * smooth(0.45, 0.6, self.n_crack_mask.unit(X, Y))
        crack = crack * (1.0 - smooth(1.0, 9.0, face)) * smooth(-1.0, 0.5, face)
        if crack.ndim == 2 and min(crack.shape) > 8:
            crack = ndimage.gaussian_filter(crack, 1.0)     # sin rendijas de una celda (aletas)
        top = top - crack * np.maximum(top - f - 0.8, 0.0) * 0.9
        # Talud de derrubios: rampa de arena al pie de la pared, irregular.
        talus = (top - f) * 0.28 * (0.4 + 0.6 * self.n_talus.unit(X, Y)) * smooth(-2.6, 0.4, face)
        floor = f + talus
        height = top * (1.0 - corridor) + floor * corridor
        band = 1.0 - smooth(0.0, 3.5, np.abs(face - 0.3))
        axis = 1.0 - smooth(0.25 * hw, 0.6 * hw, d)
        return height, band, axis

    # ── Campos 2D ────────────────────────────────────────────────────────────────
    def _fields(self, X, Y) -> Fields:
        d_route, s_route = self.zones.nearest(X, Y)
        w = self.zones.weights(X, Y, s_route, d_route)
        small, mid, big = self.n_small(X, Y), self.n_mid(X, Y), self.n_big(X, Y)
        width = self.n_width.unit(X, Y)
        heights, floors, bands, axes = {}, {}, {}, {}

        # Acantilados: laberinto por niveles en roca arenosa, con claros y plaza de salida.
        d, level = self._net("cliffs", X, Y)
        _, level_soft = self._net("cliffs", X, Y, smooth_m=10.0)
        clear_w, clear_level = self._clearing("cliffs", X, Y)
        start = self.route.points[0]
        start_w = 1.0 - smooth(9.0, 13.0, np.hypot(X - start[0], Y - start[1]))
        hw = 3.2 + 1.3 * width + 0.9 * self.n_buttress(X, Y)
        f = (level * (1.0 - clear_w) + clear_level * clear_w) * (1.0 - start_w) + 0.3 * small
        top = strata(level_soft * (1.0 - start_w) + 9.5 + 4.0 * self.n_mid.unit(X, Y) + 1.2 * small, 3.5)
        d_eff = d * (1.0 - np.maximum(clear_w, start_w))
        heights["cliffs"], bands["cliffs"], axes["cliffs"] = self._rock_walls(X, Y, d_eff, hw, f, top)
        # Lejos de los pasillos el suelo de referencia es el nivel suavizado: el muro exterior
        # que sube desde el no marca la bisectriz entre dos caminos de niveles distintos.
        near_corridor = 1.0 - smooth(hw + 1.0, hw + 9.0, d_eff)
        floors["cliffs"] = f * near_corridor + (level_soft * (1.0 - start_w)) * (1.0 - near_corridor)

        # Canon: pasillo algo mas recto y ancho por la ruta, callejones a los lados y tuneles.
        d_side, _ = self._net("canyon", X, Y)
        hw_main = 5.0 + 1.8 * width + 0.7 * self.n_buttress(X, Y)
        d_can = np.minimum(d_route - (hw_main - 4.0), d_side)      # misma cara para los dos
        tunnel = self._tunnel_mask(s_route, d_route)
        d_can = np.where(tunnel > 0.5, np.maximum(d_can, 30.0), d_can)
        f = 0.8 + 0.8 * mid
        top = strata(f + 11.0 + 4.0 * self.n_mid.unit(X, Y) + self._ridge(s_route, d_route), 3.5)
        heights["canyon"], bands["canyon"], axes["canyon"] = self._rock_walls(X, Y, d_can, 4.0 + width, f, top)
        floors["canyon"] = f

        # Zona encharcada: dunas cuyas vaguadas quedan bajo el agua (laguitos); se cruza por
        # una cresta seca que sigue la ruta, con algun hueco que se salta.
        # Laguitos en las hondonadas de un relieve de manchas (no en franjas) y dunas encima.
        base = WATER_M - 0.6 + 3.0 * self.n_pond(X, Y) + self._dunes(X, Y, 2.4, 22.0)
        crest_w = 1.3 + 1.0 * self.n_width.unit(X, Y)
        crest = 1.0 - smooth(crest_w, crest_w + 1.6, d_route + 1.2 * self.n_crest(X, Y))
        gap = 1.0 - smooth(0.5, 0.9, np.abs(np.mod(s_route, MARSH_JUMP_EVERY_M) - MARSH_JUMP_EVERY_M / 2))
        crest = crest * (1.0 - gap * smooth(0.85, 0.95, w["marsh"]))
        path_top = MARSH_PATH_TOP_M + 0.3 * small + 0.35 * self.n_crest(Y, X)
        heights["marsh"] = base * (1.0 - crest) + np.maximum(base, path_top) * crest
        floors["marsh"] = np.full_like(X, MARSH_PATH_TOP_M)
        bands["marsh"] = np.zeros_like(X)
        axes["marsh"] = 1.0 - smooth(0.5, 1.5, d_route)

        # Algas: lomas y hondonadas, red densa de sendas; el laberinto lo hace la espesura.
        d, _ = self._net("algae", X, Y)
        hw = 1.8 + 0.8 * width
        f = 1.0 + 3.5 * self.n_big.unit(X, Y) + 2.4 * mid
        heights["algae"], floors["algae"] = f, f
        bands["algae"] = np.zeros_like(X)
        axes["algae"] = 1.0 - smooth(0.5 * hw, hw, d)
        foliage = smooth(hw + 0.2, hw + 1.1, d) * (0.92 + 0.08 * small)

        # Playa final: arena que baja hasta el mar por el borde norte del mapa.
        u = MAP_MAX_M - X + 18.0 * self.n_edge(X, Y)
        hills = 9.0 * np.clip(self.n_beach_hills(X, Y) + 0.35, 0.0, None) + 0.8 * self._dunes(X, Y, 1.6, 20.0)
        beach = WATER_M - 2.5 + 5.0 * smooth(4.0, 70.0, u) + hills * smooth(40.0, 95.0, u)
        heights["beach"], floors["beach"] = beach, beach
        bands["beach"] = np.zeros_like(X)
        axes["beach"] = np.zeros_like(X)

        height = np.sum([w[z] * heights[z] for z in ZONE_NAMES], axis=0)
        floor = np.sum([w[z] * floors[z] for z in ZONE_NAMES], axis=0)
        band = np.sum([w[z] * bands[z] for z in ZONE_NAMES], axis=0)
        path = np.sum([w[z] * axes[z] for z in ZONE_NAMES], axis=0)
        foliage = w["algae"] * foliage

        # Muro exterior (fuera del alcance de cada zona), con relieve: el mismo en todas las
        # zonas, asi que no marca la frontera entre ellas.
        reach = self.zones.reach(w, X, Y)
        outer = smooth(reach, reach + 22.0, d_route)
        relief = 4.0 * mid + 3.0 * big + self._dunes(X, Y, 2.5, 34.0)
        rock = np.maximum(height, floor + OUTER_WALL_M + relief)
        dune_wall = np.maximum(height, 8.0 + relief)
        soft = w["marsh"] + w["algae"]
        wall = rock * (1.0 - soft) + dune_wall * soft
        height = height * (1 - outer) + wall * outer
        foliage = foliage * (1 - outer)
        path = path * (1 - outer)

        # Borde del mapa: muro de silueta irregular al sur, este y oeste.
        warp = 22.0 * self.n_edge(X, Y)
        edges = [X - MAP_MIN_M + warp, Y - MAP_MIN_M + warp, MAP_MAX_M - Y + warp]
        near = np.maximum.reduce([1.0 - smooth(4.0, 28.0, e) for e in edges])
        edge_wall = np.maximum(height, EDGE_WALL_M + relief)
        height = height * (1 - near) + edge_wall * near
        # Final del mapa: todo el borde norte es playa que baja al mar (sin muro).
        shore = 1.0 - smooth(70.0, 120.0, u)
        height = height * (1 - shore) + beach * shore
        foliage = foliage * (1 - shore)
        end = self.route.points[-1]
        plaza = 1.0 - smooth(10.0, 16.0, np.hypot(X - end[0], Y - end[1]))
        height = height * (1 - plaza) + np.maximum(height, END_PLAZA_M) * plaza
        height = np.minimum(height, TOP_LIMIT_M)
        tunnel_zone = w["canyon"] * self._tunnel_mask(s_route, d_route)
        return Fields(w, d_route, s_route, height, floor, np.clip(band, 0, 1), np.clip(foliage, 0, 1), tunnel_zone,
                      np.clip(path, 0, 1))

    # ── Erosion ──────────────────────────────────────────────────────────────────
    def _erode(self, f: Fields, rng: np.random.Generator) -> np.ndarray:
        """Gotas de agua (arrastre y deposito de sedimento) + paso termico. Solo en roca y
        paredes: agua, playa, bosque y el eje de los caminos quedan como estaban."""
        h0 = f.height.copy()
        rocky = np.clip(f.weights["cliffs"] + f.weights["canyon"], 0.0, 1.0)
        erodible = rocky * (1.0 - f.path) * (1.0 - f.tunnel)
        h = hydraulic_erosion(h0, erodible, rng)
        h = thermal_erosion(h, erodible, THERMAL_TALUS, 8)
        # El eje de los caminos vuelve a su cota: la erosion estrecha y ensucia sus bordes
        # (derrubios) pero no los tapa.
        keep = np.maximum(f.path, 1.0 - erodible)
        h = h * (1.0 - keep) + h0 * keep
        return despike(h, rocky)

    # ── Acceso por trozo ─────────────────────────────────────────────────────────
    def chunk_fields(self, col: int, row: int, pad: int = 0) -> tuple[Fields, np.ndarray, np.ndarray]:
        """Campos del trozo (con pad muestras de margen) y sus coordenadas X, Y."""
        i0 = int(round((row * CELL_M - CELL_M / 2.0 - MAP_MIN_M) / STEP_XY_M)) + GRID_PAD - pad
        j0 = int(round((col * CELL_M - CELL_M / 2.0 - MAP_MIN_M) / STEP_XY_M)) + GRID_PAD - pad
        n = CELL_SAMPLES + 2 * pad
        X, Y = np.meshgrid(self.axis[i0:i0 + n], self.axis[j0:j0 + n], indexing="ij")
        return self.grid.window(i0, i0 + n, j0, j0 + n), X, Y

    # ── Densidad 3D ──────────────────────────────────────────────────────────────
    def density(self, X, Y, Z, f: Fields) -> np.ndarray:
        """D en la rejilla X, Y (2D) por las cotas Z (1D): forma (nx, ny, nz)."""
        X3, Y3, Z3 = X[..., None], Y[..., None], Z[None, None, :]
        D = f.height[..., None] - Z3
        band = f.wall_band[..., None]
        if np.any(band > 0.0):
            # Paredes con voladizos suaves; nada por debajo del suelo del camino.
            above_floor = smooth(0.8, 2.5, Z3 - f.floor[..., None])
            D = D + 1.1 * band * above_floor * self.n_wall3d(X3, Y3, Z3)
        if np.any(f.tunnel > 0.0):
            carve = self._tunnel_carve(f, Z3, X, Y)
            # Paredes y techo rugosos, no una boveda perfecta.
            carve = carve + 0.6 * self.n_wall3d(X3 * 1.7, Y3 * 1.7, Z3 * 1.7) * (carve > -0.5)
            D = np.minimum(D, -carve)
        return D

    def _tunnel_carve(self, f: Fields, Z3, X=None, Y=None):
        """> 0 dentro del hueco del tunel: boveda irregular (anchura y altura que cambian a lo
        largo) sobre un suelo que sube o baja por dentro, con paredes rugosas."""
        span = np.zeros_like(f.s_route)
        climb = np.zeros_like(f.s_route)
        for (a, b), amp in zip(self.tunnels, self.tunnel_climb):
            span = np.maximum(span, smooth(a - 6.0, a - 3.0, f.s_route) * (1.0 - smooth(b + 3.0, b + 6.0, f.s_route)))
            t = np.clip((f.s_route - a) / (b - a), 0.0, 1.0)
            climb = climb + amp * np.sin(np.pi * t) * ((f.s_route > a - 6.0) & (f.s_route < b + 6.0))
        span = span * (f.weights["canyon"] > 0.5)
        if X is not None:
            wobble = self.n_tunnel(X, Y)
            half = TUNNEL_HALF_WIDTH_M * (1.0 + 0.25 * wobble)
            height = TUNNEL_HEIGHT_M * (1.0 + 0.2 * self.n_tunnel(Y, X))
        else:
            half, height = np.full_like(span, TUNNEL_HALF_WIDTH_M), np.full_like(span, TUNNEL_HEIGHT_M)
        u = f.d_route[..., None]
        v = Z3 - (f.floor + climb)[..., None]
        shape = np.broadcast_shapes(u.shape, v.shape)
        arch = height[..., None] * np.sqrt(np.clip(1.0 - (u / half[..., None]) ** 2, 0.0, 1.0)) + 0.8
        inside = np.minimum.reduce([np.broadcast_to(arch - v, shape), np.broadcast_to(v + 0.25, shape),
                                    np.broadcast_to(half[..., None] + 0.8 - u, shape)])
        return np.where(span[..., None] > 0.5, inside, -1.0)


# ── Erosion (funciones puras sobre una rejilla de 1 m) ─────────────────────────────
def _bilinear(h, px, py):
    i0 = np.clip(np.floor(px).astype(np.int64), 0, h.shape[0] - 2)
    j0 = np.clip(np.floor(py).astype(np.int64), 0, h.shape[1] - 2)
    fx, fy = px - i0, py - j0
    a, b, c, d = h[i0, j0], h[i0 + 1, j0], h[i0, j0 + 1], h[i0 + 1, j0 + 1]
    value = a * (1 - fx) * (1 - fy) + b * fx * (1 - fy) + c * (1 - fx) * fy + d * fx * fy
    gx = (b - a) * (1 - fy) + (d - c) * fy
    gy = (c - a) * (1 - fx) + (d - b) * fx
    return value, gx, gy, i0, j0, fx, fy


def hydraulic_erosion(h: np.ndarray, erodible: np.ndarray, rng: np.random.Generator,
                      drops: int = EROSION_DROPS, steps: int = EROSION_STEPS, batch: int = 6000) -> np.ndarray:
    """Erosion por gotas (vectorizada por lotes): cada gota baja por la pendiente, arranca
    sedimento donde corre y lo deja donde frena. Devuelve el relieve nuevo."""
    h = h.copy()
    inertia, capacity_k, min_capacity, deposit_k, erode_k, evaporate, gravity = 0.05, 4.0, 0.01, 0.3, 0.3, 0.02, 4.0
    n, m = h.shape
    for start in range(0, drops, batch):
        count = min(batch, drops - start)
        px = rng.uniform(1.0, n - 2.0, count)
        py = rng.uniform(1.0, m - 2.0, count)
        dx = np.zeros(count)
        dy = np.zeros(count)
        speed = np.ones(count)
        water = np.ones(count)
        sediment = np.zeros(count)
        alive = np.ones(count, dtype=bool)
        for _ in range(steps):
            height, gx, gy, i0, j0, fx, fy = _bilinear(h, px, py)
            dx = dx * inertia - gx * (1 - inertia)
            dy = dy * inertia - gy * (1 - inertia)
            norm = np.hypot(dx, dy)
            alive &= norm > 1e-6
            dx = np.where(alive, dx / np.maximum(norm, 1e-9), 0.0)
            dy = np.where(alive, dy / np.maximum(norm, 1e-9), 0.0)
            nx, ny = px + dx, py + dy
            alive &= (nx > 0) & (nx < n - 1) & (ny > 0) & (ny < m - 1)
            new_height = _bilinear(h, np.clip(nx, 0, n - 1.001), np.clip(ny, 0, m - 1.001))[0]
            dh = new_height - height
            capacity = np.maximum(-dh * speed * water * capacity_k, min_capacity)
            deposit = np.where(dh > 0, np.minimum(dh, sediment), (sediment - capacity) * deposit_k)
            depositing = alive & ((sediment > capacity) | (dh > 0))
            eroding = alive & ~depositing
            amount = np.where(depositing, deposit, 0.0)
            take = np.where(eroding, np.minimum((capacity - sediment) * erode_k, -dh), 0.0)
            take = take * erodible[i0, j0]
            delta = amount - take                       # + deposita, - arranca
            corners = ((0, 0, (1 - fx) * (1 - fy)), (1, 0, fx * (1 - fy)), (0, 1, (1 - fx) * fy), (1, 1, fx * fy))
            for ci, cj, wgt in corners:
                np.add.at(h, (i0 + ci, j0 + cj), delta * wgt)
            sediment = sediment - amount + take
            speed = np.sqrt(np.maximum(speed * speed + dh * gravity, 0.0))
            water = water * (1 - evaporate)
            px, py = np.where(alive, nx, px), np.where(alive, ny, py)
    return h


def despike(h: np.ndarray, where: np.ndarray, limit: float = 1.2, passes: int = 3) -> np.ndarray:
    """Quita picos y pozos de una celda (aletas finas en la malla): donde la celda se aparta
    de la mediana de sus vecinas mas de limit metros, toma la mediana. Luego un desenfoque
    leve solo en la roca."""
    h = h.copy()
    for _ in range(passes):
        med = ndimage.median_filter(h, size=3)
        spike = (np.abs(h - med) > limit) & (where > 0.05)
        h = np.where(spike, med, h)
    soft = ndimage.gaussian_filter(h, 0.6)
    w = np.clip(where, 0.0, 1.0) * 0.6
    return h * (1.0 - w) + soft * w


def thermal_erosion(h: np.ndarray, erodible: np.ndarray, talus: float, iterations: int) -> np.ndarray:
    """Tumba las puntas: donde el desnivel con una vecina supera talus, pasa parte del exceso
    hacia abajo."""
    h = h.copy()
    for _ in range(iterations):
        for axis, shift in ((0, 1), (0, -1), (1, 1), (1, -1)):
            neighbour = np.roll(h, shift, axis=axis)
            excess = np.maximum(h - neighbour - talus, 0.0) * 0.25 * erodible
            h -= excess
            h += np.roll(excess, -shift, axis=axis)
    return h
