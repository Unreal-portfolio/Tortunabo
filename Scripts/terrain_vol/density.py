"""Campo de densidad del mapa: D(x, y, z) > 0 es roca o arena, < 0 es aire.

Cada zona da un relieve base (suelo de sus caminos, cima de su masa) y la mezcla de zonas por
peso lo funde sin costuras. Encima van los rasgos 3D: paredes con voladizos y estratos en la
roca, y los tuneles del canon, excavados dentro de una dorsal que cruza el camino.

Todo es funcion pura de la coordenada de mundo: cualquier trozo se evalua por separado.
"""

from __future__ import annotations

import math
from dataclasses import dataclass

import numpy as np
from scipy.spatial import cKDTree

from .layout import MAP_MAX_M, MAP_MIN_M, WATER_M, Z_MAX_M
from .network import Network, build_network
from .noise import Fbm2D, ValueNoise2D, ValueNoise3D
from .route import ZONE_NAMES, Route, ZoneField, build_route

# ── Parametros por zona ───────────────────────────────────────────────────────────
CAUSEWAY_TOP_M = WATER_M + 0.8          # calzada del lago
LAKE_FLOOR_M = WATER_M - 1.8
TUNNEL_HALF_WIDTH_M = 5.0
TUNNEL_HEIGHT_M = 5.5
TUNNEL_LENGTH_M = (26.0, 36.0)
OUTER_WALL_M = 16.0                     # muro que cierra el mapa fuera del alcance de cada zona
EDGE_WALL_M = 22.0                      # muro del borde del mapa
TOP_LIMIT_M = Z_MAX_M - 2.0


def smooth(e0: float, e1: float, x):
    t = np.clip((x - e0) / (e1 - e0), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def strata(h, step: float):
    """Repisas de canto vivo (arenisca)."""
    q = h / step
    f = q - np.floor(q)
    return (np.floor(q) + smooth(0.85, 1.0, f)) * step


@dataclass
class Fields:
    """Campos 2D de un trozo (forma (nx, ny))."""
    weights: dict[str, np.ndarray]
    d_route: np.ndarray
    s_route: np.ndarray
    height: np.ndarray          # relieve base (m)
    floor: np.ndarray           # cota del suelo de los caminos (m), base de los tuneles
    wall_band: np.ndarray       # 0..1: cerca de una pared de roca (voladizos y estratos)
    foliage: np.ndarray         # densidad del bosque de algas 0..1
    tunnel: np.ndarray          # 0..1: dentro del tramo de tunel de la ruta


class MapModel:
    def __init__(self, seed: int):
        self.seed = seed
        rng = np.random.default_rng(seed)
        self.route: Route = build_route(rng)
        self.zones = ZoneField(rng, self.route)
        self.nets: dict[str, Network] = {
            "cliffs": build_network(rng, self.zones, "cliffs", 19.0, 0.15),
            "canyon": build_network(rng, self.zones, "canyon", 28.0, 0.0, reach_share=0.75, bend=0.1),
            "dunes": build_network(rng, self.zones, "dunes", 23.0, 0.2),
            "algae": build_network(rng, self.zones, "algae", 15.0, 0.45, loops_anywhere=True),
        }
        self.net_trees = {z: cKDTree(n.sample()) for z, n in self.nets.items()}
        self.clearings = {z: self._pick_clearings(rng, n) for z, n in self.nets.items() if z != "algae"}
        self.tunnels = self._plan_tunnels(rng)
        self.islets, self.islands = self._plan_lake(rng)

        self.n_small = Fbm2D(rng, 22.0, 2)
        self.n_mid = Fbm2D(rng, 60.0, 3)
        self.n_big = Fbm2D(rng, 160.0, 2)
        self.n_width = Fbm2D(rng, 35.0, 2)
        self.n_outcrop = Fbm2D(rng, 45.0, 2)
        self.n_dune_warp = Fbm2D(rng, 70.0, 2)
        self.n_dune_amp = Fbm2D(rng, 90.0, 2)
        self.n_wall3d = ValueNoise3D(rng, 5.0)
        self.n_wall3d_fine = ValueNoise3D(rng, 2.2)
        self.n_strata = ValueNoise2D(rng, 30.0)
        self.dune_angle = float(rng.uniform(0.0, math.pi))

    # ── Planificacion ────────────────────────────────────────────────────────────
    @staticmethod
    def _pick_clearings(rng: np.random.Generator, net: Network) -> np.ndarray:
        """Claros en algunos nodos (x, y, radio): plazas para puzles y respiro en el laberinto."""
        nodes = [k for k in range(len(net.nodes)) if k not in set(net.chain)]
        count = max(1, len(nodes) // 8)
        picked = rng.choice(nodes, size=min(count, len(nodes)), replace=False) if nodes else []
        return np.array([[net.nodes[k][0], net.nodes[k][1], rng.uniform(5.5, 8.5)] for k in picked]).reshape(-1, 3)

    def _plan_tunnels(self, rng: np.random.Generator) -> list[tuple[float, float]]:
        """2-3 tramos de tunel en el canon, en tramos casi rectos de la ruta y separados."""
        s0, s1 = self.route.zone_range("canyon")
        tunnels: list[tuple[float, float]] = []
        wanted = int(rng.integers(2, 4))
        for _ in range(200):
            if len(tunnels) == wanted:
                break
            length = float(rng.uniform(*TUNNEL_LENGTH_M))
            a = float(rng.uniform(s0 + 20.0, s1 - 20.0 - length))
            b = a + length
            if any(not (b + 30.0 < ta or a - 30.0 > tb) for ta, tb in tunnels):
                continue
            d0, d1 = self.route.direction_at(a), self.route.direction_at(b)
            if float(np.dot(d0, d1)) < math.cos(math.radians(25.0)):
                continue
            tunnels.append((a, b))
        return sorted(tunnels)

    def _plan_lake(self, rng: np.random.Generator):
        """Rosarios de islitas (x, y, radio, cota) paralelos a la calzada e islas grandes (x, y, radio)."""
        s0, s1 = self.route.zone_range("lake")
        islets = []
        for _ in range(int(rng.integers(3, 5))):
            length = float(rng.uniform(35.0, 75.0))
            a = float(rng.uniform(s0 + 10.0, max(s0 + 11.0, s1 - length - 10.0)))
            side = float(rng.choice([-1.0, 1.0]))
            offset = float(rng.uniform(9.0, 22.0))
            s = a
            while s < a + length:
                r = float(rng.uniform(1.2, 2.3))
                p = self.route.point_at(s)
                d = self.route.direction_at(s)
                n = np.array([-d[1], d[0]]) * side
                wobble = offset + 3.0 * math.sin(s / 11.0)
                c = p + n * wobble
                islets.append([c[0], c[1], r, CAUSEWAY_TOP_M + float(rng.uniform(0.0, 0.6))])
                s += 2.0 * r + float(rng.uniform(1.5, 2.8))
        islands = []
        for _ in range(60):
            if len(islands) == 3:
                break
            s = float(rng.uniform(s0 + 15.0, s1 - 15.0))
            p = self.route.point_at(s)
            d = self.route.direction_at(s)
            n = np.array([-d[1], d[0]]) * float(rng.choice([-1.0, 1.0]))
            r = float(rng.uniform(11.0, 20.0))
            c = p + n * float(rng.uniform(r + 12.0, r + 40.0))
            if not (MAP_MIN_M + 40 < c[0] < MAP_MAX_M - 40 and MAP_MIN_M + 40 < c[1] < MAP_MAX_M - 40):
                continue
            w = self.zones.weights(np.array([c[0]]), np.array([c[1]]))["lake"][0]
            if w < 0.8 or any(math.dist(c, (i[0], i[1])) < r + i[2] + 10.0 for i in islands):
                continue
            islands.append([c[0], c[1], r])
        return np.array(islets).reshape(-1, 4), np.array(islands).reshape(-1, 3)

    # ── Campos 2D ────────────────────────────────────────────────────────────────
    def _net_distance(self, zone: str, X, Y):
        d, _ = self.net_trees[zone].query(np.stack([X.ravel(), Y.ravel()], axis=1))
        return d.reshape(X.shape)

    def _clearing(self, zone: str, X, Y):
        out = np.zeros_like(X)
        for cx, cy, r in self.clearings.get(zone, np.zeros((0, 3))):
            out = np.maximum(out, 1.0 - smooth(r, r + 2.0, np.hypot(X - cx, Y - cy)))
        return out

    def _dunes(self, X, Y, amp, wave: float):
        along = X * math.cos(self.dune_angle) + Y * math.sin(self.dune_angle)
        across = -X * math.sin(self.dune_angle) + Y * math.cos(self.dune_angle)
        phase = (along + 0.4 * wave * self.n_dune_warp(X, Y) + 0.15 * across) / wave
        frac = phase - np.floor(phase)
        profile = np.where(frac < 0.72, frac / 0.72, (1.0 - frac) / 0.28)
        return amp * smooth(0.0, 1.0, profile) * (0.5 + 0.5 * self.n_dune_amp.unit(X, Y))

    def _tunnel_mask(self, s, d):
        """0..1 dentro del tramo de tunel de la ruta (a lo largo) y cerca de ella (a lo ancho)."""
        out = np.zeros_like(s)
        for a, b in self.tunnels:
            along = smooth(a - 1.0, a + 1.0, s) * (1.0 - smooth(b - 1.0, b + 1.0, s))
            out = np.maximum(out, along)
        return out * (1.0 - smooth(TUNNEL_HALF_WIDTH_M + 6.0, TUNNEL_HALF_WIDTH_M + 12.0, d))

    def _ridge(self, s, d):
        """Dorsal sobre cada tunel: la roca sube 3-5 m alrededor de su tramo."""
        out = np.zeros_like(s)
        for a, b in self.tunnels:
            along = smooth(a - 12.0, a + 2.0, s) * (1.0 - smooth(b - 2.0, b + 12.0, s))
            out = np.maximum(out, along)
        return 4.0 * out * (1.0 - smooth(18.0, 35.0, d))

    def fields(self, X, Y) -> Fields:
        d_route, s_route = self.zones.nearest(X, Y)
        w = self.zones.weights(X, Y, s_route, d_route)
        small, mid, big = self.n_small(X, Y), self.n_mid(X, Y), self.n_big(X, Y)
        width = self.n_width.unit(X, Y)

        heights, floors, bands = {}, {}, {}
        foliage = np.zeros_like(X)

        # Acantilados: masa de roca alta atravesada por un laberinto de pasillos estrechos.
        d = self._net_distance("cliffs", X, Y)
        hw = 3.3 + 1.7 * width
        c = np.maximum(1.0 - smooth(hw, hw + 1.2, d), self._clearing("cliffs", X, Y))
        f = 0.6 * small
        top = strata(f + 13.0 + 4.0 * self.n_mid.unit(X, Y) + 1.5 * small, 2.5)
        heights["cliffs"], floors["cliffs"] = top * (1 - c) + f * c, f
        bands["cliffs"] = 1.0 - smooth(0.0, 3.5, np.abs(d - (hw + 0.6)))

        # Canon: pasillo por la ruta, algo mas recto y ancho, con callejones y plazas a los lados.
        d_side = self._net_distance("canyon", X, Y)
        hw_main = 5.0 + 2.0 * width
        c_main = 1.0 - smooth(hw_main, hw_main + 1.5, d_route)
        c_side = np.maximum(1.0 - smooth(3.2 + width, 4.7 + width, d_side), self._clearing("canyon", X, Y))
        tunnel = self._tunnel_mask(s_route, d_route)
        c = np.maximum(c_main, c_side) * (1.0 - tunnel)
        f = 0.8 + 0.8 * mid
        top = strata(f + 11.0 + 4.0 * self.n_mid.unit(X, Y) + self._ridge(s_route, d_route), 2.5)
        heights["canyon"], floors["canyon"] = top * (1 - c) + f * c, f
        bands["canyon"] = np.maximum(1.0 - smooth(0.0, 3.5, np.abs(d_route - (hw_main + 0.7))),
                                     1.0 - smooth(0.0, 3.0, np.abs(d_side - (4.0 + width))))

        # Dunas laberinticas: los caminos van por vaguadas entre dunas de ladera empinada; el
        # camino sube y baja con el relieve grande; afloramientos de roca aqui y alla.
        d = self._net_distance("dunes", X, Y)
        hw = 3.2 + 1.8 * width
        c = np.maximum(1.0 - smooth(hw, hw + 2.8, d), self._clearing("dunes", X, Y))
        dune = self._dunes(X, Y, 3.0, 26.0)
        f = 2.5 * big + 0.5 * dune
        outcrop = smooth(0.66, 0.74, self.n_outcrop.unit(X, Y))
        top = f + 4.5 + 1.2 * dune + 1.0 * mid
        top = top * (1 - outcrop) + strata(top + 6.0, 2.0) * outcrop
        heights["dunes"], floors["dunes"] = top * (1 - c) + f * c, f
        bands["dunes"] = outcrop * (1.0 - smooth(0.0, 3.0, np.abs(d - (hw + 1.4))))

        # Lago: cuenca con calzada de arena por la ruta, rosarios de islitas e islas grandes.
        basin = LAKE_FLOOR_M + 0.5 * small
        hw = 2.0 + 1.2 * width
        causeway = 1.0 - smooth(hw, hw + 2.2, d_route)
        lake = basin * (1 - causeway) + (CAUSEWAY_TOP_M + 0.15 * small) * causeway
        lake = np.maximum(lake, self._islets(X, Y, basin))
        lake = np.maximum(lake, self._islands(X, Y, basin, mid))
        heights["lake"], floors["lake"] = lake, np.full_like(X, CAUSEWAY_TOP_M)
        bands["lake"] = np.zeros_like(X)

        # Algas: suelo ondulado y red densa de sendas; el laberinto lo hace la espesura.
        d = self._net_distance("algae", X, Y)
        hw = 2.0 + 1.0 * width
        f = 1.0 + 2.0 * self.n_big.unit(X, Y) + 0.4 * small
        hills = 1.8 * np.clip(mid, 0.0, None) * smooth(hw, hw + 4.0, d)
        heights["algae"], floors["algae"] = f + hills, f
        bands["algae"] = np.zeros_like(X)
        foliage = smooth(hw + 0.3, hw + 1.6, d) * (0.85 + 0.15 * small)

        # Arena final: dunas abiertas y una plaza de llegada al final de la ruta.
        end = self.route.points[-1]
        plaza = 1.0 - smooth(12.0, 18.0, np.hypot(X - end[0], Y - end[1]))
        f = 0.5 + 0.6 * big
        sand = f + self._dunes(X, Y, 1.4, 22.0) * (1 - plaza) * smooth(3.0, 9.0, d_route)
        heights["sand_end"], floors["sand_end"] = sand, f
        bands["sand_end"] = np.zeros_like(X)

        height = np.sum([w[z] * heights[z] for z in ZONE_NAMES], axis=0)
        floor = np.sum([w[z] * floors[z] for z in ZONE_NAMES], axis=0)
        band = np.sum([w[z] * bands[z] for z in ZONE_NAMES], axis=0)
        foliage = w["algae"] * foliage

        # Muro exterior: mas alla del alcance de la zona el terreno sube y cierra el mapa.
        reach = self.zones.reach(w, X, Y)
        outer = smooth(reach, reach + 22.0, d_route)
        # Muro con relieve (lomas y dunas grandes), no una meseta plana.
        relief = 4.0 * mid + 3.0 * self.n_big(X, Y) + self._dunes(X, Y, 2.5, 34.0)
        wall = np.maximum(height, floor + OUTER_WALL_M + relief)
        wall = wall * (1 - w["lake"]) + np.maximum(height, 9.0 + relief) * w["lake"]
        height = height * (1 - outer) + wall * outer
        foliage = foliage * (1 - outer)
        edge = np.minimum.reduce([X - MAP_MIN_M, MAP_MAX_M - X, Y - MAP_MIN_M, MAP_MAX_M - Y])
        near_edge = 1.0 - smooth(4.0, 18.0, edge)
        height = height * (1 - near_edge) + np.maximum(height, EDGE_WALL_M) * near_edge
        height = np.minimum(height, TOP_LIMIT_M)
        tunnel_zone = w["canyon"] * self._tunnel_mask(s_route, d_route)
        return Fields(w, d_route, s_route, height, floor, np.clip(band, 0.0, 1.0), np.clip(foliage, 0.0, 1.0),
                      tunnel_zone)

    def _islets(self, X, Y, basin):
        out = np.full_like(X, -np.inf)
        x0, x1, y0, y1 = X.min() - 3.0, X.max() + 3.0, Y.min() - 3.0, Y.max() + 3.0
        for cx, cy, r, top in self.islets:
            if not (x0 - r <= cx <= x1 + r and y0 - r <= cy <= y1 + r):
                continue
            m = 1.0 - smooth(r * 0.55, r, np.hypot(X - cx, Y - cy))
            out = np.maximum(out, basin + (top - basin) * m)
        return out

    def _islands(self, X, Y, basin, mid):
        out = np.full_like(X, -np.inf)
        for cx, cy, r in self.islands:
            dist = np.hypot(X - cx, Y - cy) + 3.0 * mid
            m = 1.0 - smooth(r * 0.6, r, dist)
            top = CAUSEWAY_TOP_M + 0.8 + self._dunes(X, Y, 2.0, 14.0)
            out = np.maximum(out, basin + (top - basin) * m)
        return out

    # ── Densidad 3D ──────────────────────────────────────────────────────────────
    def density(self, X, Y, Z, fields: Fields | None = None) -> np.ndarray:
        """D en la rejilla X, Y (2D, forma (nx, ny)) por las cotas Z (1D): forma (nx, ny, nz)."""
        f = fields or self.fields(X, Y)
        X3, Y3, Z3 = X[..., None], Y[..., None], Z[None, None, :]
        D = f.height[..., None] - Z3
        # Paredes de roca: voladizos (ruido 3D) y estratos horizontales ondulados.
        band = f.wall_band[..., None]
        if np.any(band > 0.0):
            wobble = 1.6 * self.n_wall3d(X3, Y3, Z3) + 0.5 * self.n_wall3d_fine(X3, Y3, Z3)
            ledges = 0.45 * np.sin(Z3 * (2.0 * math.pi / 2.5) + 2.0 * self.n_strata(X, Y)[..., None])
            # Solo por encima del suelo del camino: el suelo no se llena de bultos.
            above_floor = smooth(0.6, 2.0, Z3 - f.floor[..., None])
            D = D + band * above_floor * (wobble + ledges)
        # Tuneles: boveda excavada a lo largo de la ruta, dentro de la dorsal.
        if np.any(f.tunnel > 0.0):
            D = np.minimum(D, -self._tunnel_carve(f, Z3))
        return D

    def _tunnel_carve(self, f: Fields, Z3):
        """> 0 dentro del hueco del tunel (seccion de boveda sobre el suelo del camino)."""
        u = f.d_route[..., None]
        v = Z3 - f.floor[..., None]
        arch = TUNNEL_HEIGHT_M * np.sqrt(np.clip(1.0 - (u / TUNNEL_HALF_WIDTH_M) ** 2, 0.0, 1.0)) + 0.8
        inside = np.minimum.reduce([np.broadcast_to(arch - v, np.broadcast_shapes(u.shape, v.shape)),
                                    np.broadcast_to(v + 0.25, np.broadcast_shapes(u.shape, v.shape)),
                                    np.broadcast_to(TUNNEL_HALF_WIDTH_M + 0.8 - u, np.broadcast_shapes(u.shape, v.shape))])
        # Solo en el tramo del tunel (y un poco antes y despues, para abrir las bocas).
        span = np.zeros_like(f.s_route)
        for a, b in self.tunnels:
            span = np.maximum(span, smooth(a - 6.0, a - 3.0, f.s_route) * (1.0 - smooth(b + 3.0, b + 6.0, f.s_route)))
        span = span * (f.weights["canyon"] > 0.5)
        return np.where(span[..., None] > 0.5, inside, -1.0)
