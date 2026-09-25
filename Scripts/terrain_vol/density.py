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
from .route import ZONE_NAMES, Route, ZoneField, build_route, catmull_rom, resample
from .style import MapStyle, MAPA01_STYLE

# ── Parametros ────────────────────────────────────────────────────────────────────
CLIFF_LEVEL_M = 4.5                     # separacion entre niveles del laberinto de acantilados
CLIFF_LEVELS = 3
MARSH_PATH_TOP_M = WATER_M + 1.3        # crestas por las que se cruza la zona encharcada
END_PLAZA_M = WATER_M + 1.8             # plaza de llegada, seca, sobre la playa
MARSH_JUMP_EVERY_M = 48.0               # cada cuanto un charco estrecho corta el sendero y se salta
MARSH_WIGGLE_M = 7.0                    # cuanto se aparta el sendero de la ruta a los lados
TUNNEL_HALF_WIDTH_M = 3.6              # semiancho de la seccion del tunel del laberinto
TUNNEL_HEIGHT_M = 5.0
TUNNEL_MIN_COVER_M = TUNNEL_HEIGHT_M + 0.8   # roca minima sobre el suelo del tunel (tramo central)
TUNNEL_LENGTH_M = (26.0, 36.0)         # (solo lo usa el plan antiguo, que se conserva por la semilla)
TUNNEL_EDGE_M = (18.0, 45.0)           # longitud de las aristas del laberinto que se hacen tunel
# Por zona: (tuneles pasantes, cuevas sin salida). Pasante: une dos pasillos abiertos (sus dos
# nodos tienen otras aristas). Cueva: uno de sus nodos es una hoja (no lleva a ningun sitio).
TUNNELS_PER_ZONE = {"cliffs": (3, 1), "canyon": (6, 1)}
TUNNEL_CANDIDATES = {"cliffs": (10, 4), "canyon": (10, 4)}  # (pasantes, cuevas) antes de validar
ARCHES_PER_ZONE = {"cliffs": 2, "canyon_maze": 3}    # puentes de roca que cruzan un pasillo
ARCH_DECK_M = 7.5                        # tablero del puente sobre el suelo del pasillo
GORGE_DEPTH_M = 7.5                      # puente natural: el canon transversal pasa esto por debajo del camino
GORGE_RAMP_M = 15.0                      # rampa del canon transversal hasta la cota de los pasillos
GORGE_HALF_WIDTH_M = 3.2
TUNNEL_HILL_M = 7.0                      # loma de roca que se levanta sobre cada tunel

CANYON_REACH_M = 70.0                  # el laberinto del canon se extiende a los lados como el de la salida
CANYON_REACH_BASE_M = 40.0             # alcance de ZONES para el canon (no se toca: mueve la semilla)
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
    return (np.floor(q) + smooth(0.55, 1.0, f)) * step


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


def _stub_network(zone: str) -> Network:
    """Red minima para una zona practicamente ausente (fraccion ~0 en el estilo): dos nodos
    lejos del mapa (un unico tramo, nunca una red vacia) para que _net()/_pick_clearings no
    revienten. Su peso de zona es ~0 en todo el mapa (ver MapStyle.zones/FLOOR_SHARE), asi
    que no se nota."""
    far = MAP_MAX_M + 500.0
    nodes = np.array([[far, far], [far + 5.0, far]])
    return Network(zone, nodes, [0, 1], [(0, 1)], [nodes], {0: 0, 1: 0}, np.zeros(2), [])


class MapModel:
    def __init__(self, seed: int, style: MapStyle = MAPA01_STYLE, erode: bool = True):
        self.seed = seed
        self.style = style
        rng = np.random.default_rng(seed)
        # Con MAPA01_STYLE estos argumentos son bit a bit los de siempre: la secuencia de
        # numeros aleatorios que sale de rng (todo lo de aqui hasta "extra", mas abajo) no
        # cambia, y con ella tampoco la huella de la semilla aprobada (ver style.py).
        self.route: Route = build_route(rng, zones=style.zones(), meander_cfg=style.meander())
        self.zones = ZoneField(rng, self.route)

        def zone_network(route_zone: str, net_key: str, spacing: float, loop_share: float, **kwargs) -> Network:
            """build_network, salvo que la zona sea casi inexistente (estilo a 0): entonces una
            red minima (_stub_network), sin consumir rng ni arriesgar un Delaunay degenerado."""
            s0, s1 = self.route.zone_range(route_zone)
            if (s1 - s0) < max(30.0, 2.0 * spacing):
                return _stub_network(net_key)
            return build_network(rng, self.zones, route_zone, spacing, loop_share, **kwargs)

        self.nets: dict[str, Network] = {
            "cliffs": zone_network("cliffs", "cliffs", style.cliffs_spacing_m, style.cliffs_loop_share,
                                   level_step=style.cliffs_level_step_m, level_count=style.cliffs_level_count,
                                   decoys=True),
            "canyon": zone_network("canyon", "canyon", 28.0, 0.0, reach_share=0.75, bend=0.25),
            "algae": zone_network("algae", "algae", 14.0, 0.5, loops_anywhere=True, decoys=True),
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
        self.marsh_warp = (Fbm2D(extra, 26.0, 2), Fbm2D(extra, 26.0, 2))
        # Canon como laberinto de acantilados (enrevesado, como el de la salida) y tuneles como
        # tramos del laberinto excavados en la roca, en los acantilados y en el canon. Todo lo
        # de aqui sale de "extra": no afecta a la huella de la semilla aprobada.
        c0, c1 = self.route.zone_range("canyon")
        if (c1 - c0) < max(30.0, 2.0 * style.canyon_maze_spacing_m):
            self.nets["canyon_maze"] = _stub_network("canyon_maze")
        else:
            self.nets["canyon_maze"] = build_network(extra, self.zones, "canyon", style.canyon_maze_spacing_m,
                                                     style.canyon_maze_loop_share, level_step=CLIFF_LEVEL_M,
                                                     level_count=style.canyon_maze_level_count, decoys=True,
                                                     reach_share=0.85, reach_m=style.canyon_maze_reach_m)
        self.maze_tunnels = self._pick_maze_tunnels(extra)
        self._close_tunnel_edges()
        # Plazas llanas grandes en el canon (puzles).
        self._pick_clearings(extra, self.nets["canyon_maze"], every=7, radius=(6.5, 10.0))
        self.clearings["canyon_maze"] = np.zeros((0, 4))       # sin plazas redondas en el canon
        self._build_tunnel_samples()
        self.marsh_warp_2 = (Fbm2D(extra, 40.0, 2), Fbm2D(extra, 40.0, 2))
        self.n_hummock = Fbm2D(extra, 6.0, 2)
        self.n_steep = Fbm2D(extra, 30.0, 2)
        self.n_beach_amp = Fbm2D(extra, 110.0, 2)
        self.n_cliff_height = Fbm2D(extra, 55.0, 3)
        self.n_clearing = Fbm2D(extra, 9.0, 2)
        self.n_top = Fbm2D(extra, 38.0, 3)
        self.canyon_warp = (Fbm2D(extra, 30.0, 2), Fbm2D(extra, 30.0, 2))
        self.arches = self._pick_arches(extra)
        self.n_top_roll = Fbm2D(extra, 17.0, 3)
        self._build_tunnel_samples()
        # Rugosidad del borde exterior (cresta irregular, nunca una ladera lisa de regla) y
        # rios de la zona de agua: todo nuevo, sorteado al final de "extra".
        self.n_border = Fbm2D(extra, 24.0, 3)
        self.n_border_fine = Fbm2D(extra, 8.0, 2)
        self.rivers, self.islands = self._plan_rivers(extra) if style.river_count > 0 else ([], [])
        # Prototipo 2026-09-25: camino principal trenzado, pasarelas, salida/meta y marcas humanas.
        # Generador propio: con todo desactivado no se consume nada y el resto no cambia.
        proto = np.random.default_rng(seed + 13)
        self.trail_tree, self.chain_tree = None, None
        if style.trails:
            self.trails = self._plan_trails(proto)
            self.trail_tree = cKDTree(np.vstack(self.trails))
            chains = [self._chain_polyline(n)[0] for n in ("cliffs", "canyon_maze")
                      if len(self.nets[n].chain) >= 2]
            chains = [c for c in chains if len(c)]
            self.chain_tree = cKDTree(np.vstack(chains)) if chains else None
        self.bridges = self._plan_bridges(proto) if sum(style.land_bridges) > 0 else []
        self.marks = self._plan_marks(proto) if style.human_marks > 0 and style.trails else []
        if self.bridges:
            self._build_tunnel_samples()

        # Campos 2D del mapa entero, con una muestra de margen alrededor.
        count = int(round((MAP_MAX_M - MAP_MIN_M) / STEP_XY_M)) + 1
        self.axis = MAP_MIN_M + STEP_XY_M * np.arange(-GRID_PAD, count + GRID_PAD)
        X, Y = np.meshgrid(self.axis, self.axis, indexing="ij")
        self.grid = self._fields(X, Y)
        # Solo quedan los tuneles con roca de verdad encima; si alguno cae bajo otro pasillo o
        # una plaza (o con poco techo), su arista vuelve a ser pasillo abierto y se recalcula.
        kept = self._covered_tunnels()
        if kept != self.maze_tunnels:
            self.maze_tunnels = kept
            self._close_tunnel_edges()
            self._build_tunnel_samples()
            self.grid = self._fields(X, Y)
        if erode:
            self.grid.height = self._erode(self.grid, np.random.default_rng(seed + 1))

    # ── Planificacion ────────────────────────────────────────────────────────────
    @staticmethod
    def _pick_clearings(rng: np.random.Generator, net: Network, every: int = 8,
                        radius: tuple[float, float] = (5.5, 8.5)) -> np.ndarray:
        """Claros en algunos nodos (x, y, radio, cota): plazas llanas para puzles y respiro."""
        nodes = [k for k in range(len(net.nodes)) if k not in set(net.chain)]
        count = max(1, len(nodes) // every)
        picked = rng.choice(nodes, size=min(count, len(nodes)), replace=False) if nodes else []
        levels = net.levels if net.levels is not None else np.zeros(len(net.nodes))
        return np.array([[net.nodes[k][0], net.nodes[k][1], rng.uniform(*radius), levels[k]]
                         for k in picked]).reshape(-1, 4)

    def _pick_arches(self, rng: np.random.Generator) -> list[tuple[np.ndarray, np.ndarray, float]]:
        """Puentes de roca: (centro, direccion del pasillo, cota del suelo). Una banda de roca
        cruza un pasillo abierto y se excava un arco por debajo: se pasa por abajo y, desde las
        repisas, se cruza por arriba."""
        arches = []
        arches_per_zone = {"cliffs": self.style.arches_cliffs, "canyon_maze": self.style.arches_canyon}
        for net_name, count in arches_per_zone.items():
            net = self.nets[net_name]
            closed = {e for z, e, _ in self.maze_tunnels if z == net_name}
            lengths = [float(np.sum(np.linalg.norm(np.diff(c, axis=0), axis=1))) for c in net.curves]
            options = [k for k in range(len(net.edges)) if k not in closed and lengths[k] >= 16.0]
            rng.shuffle(options)
            placed = 0
            for k in options:
                if placed == count:
                    break
                curve = net.curves[k]
                seg = np.linalg.norm(np.diff(curve, axis=0), axis=1)
                arc = np.concatenate(([0.0], np.cumsum(seg)))
                mid = arc[-1] * 0.5
                c = np.array([np.interp(mid, arc, curve[:, 0]), np.interp(mid, arc, curve[:, 1])])
                ahead = np.array([np.interp(mid + 2.0, arc, curve[:, 0]), np.interp(mid + 2.0, arc, curve[:, 1])]) - c
                d = ahead / max(float(np.linalg.norm(ahead)), 1e-9)
                # Nada de puentes donde el canon ya se abre al agua (sus paredes bajan).
                s_c = float(self.zones.nearest(np.array([[c[0]]]), np.array([[c[1]]]))[1][0, 0])
                c0, c1 = self.route.zone_range("canyon")
                if net_name == "canyon_maze" and (s_c - c0) / max(c1 - c0, 1.0) > 0.4:
                    continue
                arches.append((c, d, float(net.edge_floor(k, 0.5))))
                placed += 1
        return arches

    def _arch_band(self, X, Y):
        """(0..1 banda de roca que cruza el pasillo en cada puente, cota del tablero)."""
        band = np.zeros_like(X)
        deck = np.full_like(X, np.inf)
        for c, d, floor in getattr(self, "arches", []):
            along = (X - c[0]) * d[0] + (Y - c[1]) * d[1]
            across = -(X - c[0]) * d[1] + (Y - c[1]) * d[0]
            m = (1.0 - smooth(2.2, 3.6, np.abs(along))) * (1.0 - smooth(9.0, 12.0, np.abs(across)))
            band = np.maximum(band, m)
            deck = np.where(m > 0.0, np.minimum(deck, floor + ARCH_DECK_M), deck)
        return band, deck

    def _close_tunnel_edges(self) -> None:
        """Las aristas de tunel no abren pasillo en el relieve (queda roca encima)."""
        for zone in ("cliffs", "canyon_maze"):
            closed = {e for z, e, _ in self.maze_tunnels if z == zone}
            self.net_samples[zone] = self.nets[zone].sample(exclude=closed)
            self.net_trees[zone] = cKDTree(self.net_samples[zone][0])

    def _covered_tunnels(self) -> list[tuple[str, int, float]]:
        """Tuneles con al menos TUNNEL_MIN_COVER_M de roca sobre su suelo en el tramo central,
        TUNNELS_PER_ZONE por zona como mucho, primero los de la cadena (el camino bueno)."""
        kept = []
        tunnels_per_zone = {"cliffs": self.style.tunnels_cliffs, "canyon": self.style.tunnels_canyon}
        for net_name, zone in (("cliffs", "cliffs"), ("canyon_maze", "canyon")):
            net = self.nets[net_name]
            chain = {tuple(sorted((net.chain[k], net.chain[k + 1]))) for k in range(len(net.chain) - 1)}
            degree = net.degree()
            good = []
            for name, edge, dip in self.maze_tunnels:
                if name != net_name:
                    continue
                curve = net.curves[edge]
                seg = np.linalg.norm(np.diff(curve, axis=0), axis=1)
                arc = np.concatenate(([0.0], np.cumsum(seg)))
                t = np.linspace(0.35, 0.65, 7)
                x = np.interp(t * arc[-1], arc, curve[:, 0])
                y = np.interp(t * arc[-1], arc, curve[:, 1])
                i = np.clip(np.rint(x - self.axis[0]).astype(int), 0, len(self.axis) - 1)
                j = np.clip(np.rint(y - self.axis[0]).astype(int), 0, len(self.axis) - 1)
                base = net.edge_floor(edge, t) + dip * np.sin(np.pi * t)
                cover = self.grid.height[i, j] - base
                if float(cover.min()) >= TUNNEL_MIN_COVER_M:
                    a, b = net.edges[edge]
                    good.append((min(degree[a], degree[b]) >= 2, (name, edge, dip)))
            n_through, n_caves = tunnels_per_zone[zone]
            kept += [g[1] for g in good if g[0]][:n_through] + [g[1] for g in good if not g[0]][:n_caves]
        return kept

    def _pick_maze_tunnels(self, rng: np.random.Generator) -> list[tuple[str, int, float]]:
        """Aristas del laberinto que se excavan como tunel: (red, arista, desnivel). En cada zona
        al menos una esta en la cadena (el camino bueno pasa por un tunel). El desnivel es sobre
        todo negativo: el tunel baja por dentro y vuelve a subir."""
        picked = []
        for zone, net_name in (("cliffs", "cliffs"), ("canyon", "canyon_maze")):
            net = self.nets[net_name]
            chain_edges = {tuple(sorted((net.chain[k], net.chain[k + 1]))) for k in range(2, len(net.chain) - 3)}
            lengths = [float(np.sum(np.linalg.norm(np.diff(c, axis=0), axis=1))) for c in net.curves]
            ok = [k for k, e in enumerate(net.edges) if TUNNEL_EDGE_M[0] <= lengths[k] <= TUNNEL_EDGE_M[1]]
            on_chain = [k for k in ok if tuple(sorted(net.edges[k])) in chain_edges]
            others = [k for k in ok if tuple(sorted(net.edges[k])) not in chain_edges]
            rng.shuffle(on_chain)
            rng.shuffle(others)
            degree = net.degree()
            through = [k for k in on_chain + others if min(degree[net.edges[k][0]], degree[net.edges[k][1]]) >= 2]
            caves = [k for k in on_chain + others if min(degree[net.edges[k][0]], degree[net.edges[k][1]]) == 1]
            base_through, base_caves = TUNNEL_CANDIDATES[zone]
            want_through, want_caves = {"cliffs": self.style.tunnels_cliffs, "canyon": self.style.tunnels_canyon}[zone]
            n_through, n_caves = max(base_through, 2 * want_through), max(base_caves, 2 * want_caves)
            chosen = through[:n_through] + caves[:n_caves]
            used_nodes: set[int] = set()
            for k in chosen:
                a, b = net.edges[k]
                if a in used_nodes or b in used_nodes:
                    continue
                used_nodes |= {a, b}
                dip = float(rng.uniform(1.5, 3.2)) * (-1.0 if rng.random() < 0.75 else 1.0)
                picked.append((net_name, k, dip))
        return picked

    def _build_tunnel_samples(self) -> None:
        """Puntos de todos los tuneles cada 0,5 m (prolongados 2,5 m por cada boca) con su
        parametro t (0..1 de boca a boca), la cota de su suelo y su desnivel."""
        pts, ts, floors, dips = [], [], [], []
        for net_name, edge, dip in self.maze_tunnels:
            net = self.nets[net_name]
            curve = net.curves[edge]
            seg = np.linalg.norm(np.diff(curve, axis=0), axis=1)
            arc = np.concatenate(([0.0], np.cumsum(seg)))
            s = np.arange(-2.5, arc[-1] + 2.5 + 0.5, 0.5)
            x = np.interp(s, arc, curve[:, 0])
            y = np.interp(s, arc, curve[:, 1])
            # Prolongacion recta por las bocas (interp satura en los extremos).
            head = end_direction(curve)
            tail = -end_direction(curve[::-1])
            before, after = s < 0.0, s > arc[-1]
            x[before] = curve[0, 0] + head[0] * s[before]
            y[before] = curve[0, 1] + head[1] * s[before]
            x[after] = curve[-1, 0] + tail[0] * (s[after] - arc[-1])
            y[after] = curve[-1, 1] + tail[1] * (s[after] - arc[-1])
            t = s / max(arc[-1], 1e-9)
            pts.append(np.stack([x, y], axis=1))
            ts.append(t)
            floors.append(net.edge_floor(edge, t))
            dips.append(np.full_like(t, dip))
        for b in getattr(self, "bridges", []):
            s = np.arange(-8.0, 8.0 + 0.5, 0.5)
            pts.append(np.stack([b["c"][0] + b["n"][0] * s, b["c"][1] + b["n"][1] * s], axis=1))
            ts.append((s + 8.0) / 16.0)
            floors.append(np.full_like(s, b["base"] - GORGE_DEPTH_M))
            dips.append(np.zeros_like(s))
        for c, d, floor in getattr(self, "arches", []):
            s = np.arange(-6.0, 6.0 + 0.5, 0.5)
            pts.append(np.stack([c[0] + d[0] * s, c[1] + d[1] * s], axis=1))
            ts.append((s + 5.0) / 10.0)
            floors.append(np.full_like(s, floor))
            dips.append(np.zeros_like(s))
        if pts:
            self.tunnel_pts = np.vstack(pts)
            self.tunnel_t = np.concatenate(ts)
            self.tunnel_floor = np.concatenate(floors)
            self.tunnel_dip = np.concatenate(dips)
            self.tunnel_tree = cKDTree(self.tunnel_pts)
        else:
            self.tunnel_tree = None

    def _tunnel_query(self, X, Y):
        """(distancia al eje del tunel mas cercano, t, suelo, desnivel), con la forma de X."""
        d, k = self.tunnel_tree.query(np.stack([X.ravel(), Y.ravel()], axis=1))
        shape = X.shape
        return (d.reshape(shape), self.tunnel_t[k].reshape(shape), self.tunnel_floor[k].reshape(shape),
                self.tunnel_dip[k].reshape(shape))

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

    def _plan_rivers(self, rng: np.random.Generator) -> tuple[list[tuple[np.ndarray, np.ndarray]], list]:
        """style.river_count rios que nacen en la zona de agua y serpentean (Catmull-Rom con
        control points al azar, sin tramos rectos) hasta pasar el borde norte del mapa, donde
        se funden con el mar. islotes: peldanos de arena espaciados ~4 m (siempre saltables)
        cuyo tamano crece con style.island_density."""
        s0, s1 = self.route.zone_range("marsh")
        count = self.style.river_count
        rivers, islands = [], []
        starts_s = np.linspace(s0 + (s1 - s0) * 0.2, s1 - (s1 - s0) * 0.1, count)
        for s in starts_s:
            base = self.route.point_at(float(s))
            x, y = base + rng.uniform(-15.0, 15.0, 2)
            ctrl = [np.array([x, y])]
            for _ in range(4):
                x += rng.uniform(28.0, 48.0)
                y += rng.uniform(-32.0, 32.0)
                ctrl.append(np.array([x, y]))
            ctrl.append(np.array([MAP_MAX_M + 50.0, ctrl[-1][1] + rng.uniform(-25.0, 25.0)]))
            curve = catmull_rom(np.array(ctrl), 30)
            pts, arc = resample(curve, 1.0)
            if self.style.river_rapids:
                # Meandro propio (el rio de montana no baja en linea recta): onda lateral de
                # amplitud y longitud variables, que se apaga al llegar al mar.
                tangent = np.gradient(pts, axis=0)
                tangent /= np.maximum(np.linalg.norm(tangent, axis=1, keepdims=True), 1e-9)
                normal = np.stack([-tangent[:, 1], tangent[:, 0]], axis=1)
                wave = float(rng.uniform(4.0, 7.0)) * np.sin(2.0 * np.pi * arc / float(rng.uniform(70.0, 110.0))
                                                             + float(rng.uniform(0.0, 6.28))) \
                    + float(rng.uniform(2.0, 3.5)) * np.sin(2.0 * np.pi * arc / float(rng.uniform(24.0, 38.0))
                                                           + float(rng.uniform(0.0, 6.28)))
                fade = 1.0 - smooth(MAP_MAX_M - 40.0, MAP_MAX_M, pts[:, 0])
                pts, arc = resample(pts + normal * (wave * fade)[:, None], 1.0)
            rivers.append((pts, arc))
            if self.style.river_rapids:
                islands += self._rapid_islands(rng, pts, arc)
                continue
            spacing = 4.0
            s_i = spacing * 0.5
            while s_i < arc[-1] - 5.0:
                idx = min(int(np.searchsorted(arc, s_i)), len(pts) - 1)
                center = pts[idx] + rng.uniform(-1.5, 1.5, 2)
                radius = 1.6 + 2.6 * self.style.island_density * float(rng.uniform(0.7, 1.15))
                islands.append((center, radius))
                s_i += spacing
        return rivers, islands

    def _rapid_islands(self, rng: np.random.Generator, pts: np.ndarray, arc: np.ndarray) -> list:
        """Islitas de un rapido: van alternando de orilla (a veces dos enfrentadas), a distancias
        irregulares y siempre saltables; el agua baja entre ellas en zigzag."""
        tangent = np.gradient(pts, axis=0)
        tangent /= np.maximum(np.linalg.norm(tangent, axis=1, keepdims=True), 1e-9)
        normal = np.stack([-tangent[:, 1], tangent[:, 0]], axis=1)
        out = []
        side = float(rng.choice([-1.0, 1.0]))
        s_i = 3.0
        while s_i < arc[-1] - 5.0:
            idx = min(int(np.searchsorted(arc, s_i)), len(pts) - 1)
            width = self.style.river_width_m * (0.7 + 0.6 * s_i / max(arc[-1], 1.0))
            if rng.random() < 0.7:
                side = -side
            lateral = side * float(rng.uniform(0.15, 0.42)) * width
            radius = 1.4 + 2.2 * self.style.island_density * float(rng.uniform(0.7, 1.2))
            out.append((pts[idx] + normal[idx] * lateral, radius))
            if rng.random() < 0.25:
                out.append((pts[idx] - normal[idx] * lateral * 0.9, radius * 0.8))
            s_i += float(rng.uniform(3.2, 5.2))
        return out

    def _plan_trails(self, rng: np.random.Generator) -> list[np.ndarray]:
        """Camino principal fuera de los laberintos: el tronco es la ruta y, a tramos, se abre en
        2-4 caminos que se separan y se vuelven a juntar (todos convergen en el tronco)."""
        pts, arc = self.route.points, self.route.arc
        tangent = np.gradient(pts, axis=0)
        tangent /= np.maximum(np.linalg.norm(tangent, axis=1, keepdims=True), 1e-9)
        normal = np.stack([-tangent[:, 1], tangent[:, 0]], axis=1)
        m0, _ = self.route.zone_range("marsh")
        c0, c1 = self.route.zone_range("canyon")
        s_begin = min(m0, c0 + 0.6 * (c1 - c0))
        s_end = float(arc[-1]) - 25.0
        trails = [pts[arc >= s_begin - 10.0]]
        lo, hi = self.style.trail_strands
        s = s_begin + float(rng.uniform(10.0, 30.0))
        while s + 60.0 < s_end:
            s_b = min(s + float(rng.uniform(60.0, 110.0)), s_end)
            count = int(rng.integers(lo, hi + 1)) - 1
            offsets: list[float] = []
            for _ in range(60):
                if len(offsets) >= count:
                    break
                a = float(rng.uniform(14.0, 30.0)) * float(rng.choice([-1.0, 1.0]))
                if all(abs(a - o) > 12.0 for o in offsets):
                    offsets.append(a)
            length = s_b - s
            for a in offsets:
                k0 = s + float(rng.uniform(0.0, 0.2)) * length
                k1 = s_b - float(rng.uniform(0.0, 0.2)) * length
                sel = (arc >= k0) & (arc <= k1)
                u = (arc[sel] - k0) / max(k1 - k0, 1e-9)
                split = float(rng.uniform(0.2, 0.35))
                join = float(rng.uniform(0.2, 0.35))
                shape = smooth(0.0, split, u) * (1.0 - smooth(1.0 - join, 1.0, u))
                wobble = 0.25 * a * np.sin(2.0 * np.pi * u * float(rng.uniform(1.0, 2.2)) + float(rng.uniform(0.0, 6.28)))
                line = pts[sel] + normal[sel] * ((a + wobble) * shape)[:, None]
                if np.all((line > MAP_MIN_M + 20.0) & (line < MAP_MAX_M - 20.0)):
                    trails.append(line)
            s = s_b + float(rng.uniform(25.0, 55.0))
        return trails

    def _chain_polyline(self, net_name: str):
        """Polilinea de la cadena (el camino bueno) de un laberinto, cada ~0,5 m: (puntos, cota
        del suelo, arco, arco de cada nodo, grado de cada nodo)."""
        net = self.nets[net_name]
        index = {tuple(sorted(e)): k for k, e in enumerate(net.edges)}
        degree = net.degree()
        pts, floors, node_pos, node_deg = [], [], [], []
        for a, b in zip(net.chain[:-1], net.chain[1:]):
            k = index.get(tuple(sorted((a, b))))
            if k is None:
                continue
            curve = net.curves[k]
            fl = net.edge_floor(k, np.linspace(0.0, 1.0, len(curve)))
            if net.edges[k][0] != a:
                curve, fl = curve[::-1], fl[::-1]
            node_pos.append((len(np.vstack(pts)) if pts else 0, degree[a]))
            pts.append(curve)
            floors.append(np.asarray(fl, dtype=float))
        if not pts:
            return np.zeros((0, 2)), np.zeros(0), np.zeros(0), np.zeros(0), np.zeros(0, dtype=int)
        poly, floor = np.vstack(pts), np.concatenate(floors)
        arc = np.concatenate(([0.0], np.cumsum(np.linalg.norm(np.diff(poly, axis=0), axis=1))))
        node_s = np.array([arc[i] for i, _ in node_pos])
        node_d = np.array([d for _, d in node_pos], dtype=int)
        return poly, floor, arc, node_s, node_d

    def _plan_bridges(self, rng: np.random.Generator) -> list[dict]:
        """Puentes naturales en el camino bueno de los laberintos: el camino sigue llano y un
        canon transversal, GORGE_DEPTH_M mas hondo, lo cruza por debajo (arco excavado en 3D).
        El canon baja en rampa desde otro pasillo del laberinto a la misma cota, asi que se
        puede pasar por arriba (el camino) o por abajo (el canon)."""
        bridges: list[dict] = []
        c0, c1 = self.route.zone_range("canyon")
        for net_name, count in (("cliffs", self.style.land_bridges[0]), ("canyon_maze", self.style.land_bridges[1])):
            if count <= 0 or len(self.nets[net_name].chain) < 4:
                continue
            poly, floor, arc, node_s, node_d = self._chain_polyline(net_name)
            if len(poly) < 10 or arc[-1] < 40.0:
                continue
            _, sample_floor = self.net_samples[net_name]
            tree = self.net_trees[net_name]
            poly_tree = cKDTree(poly)
            candidates = np.arange(15.0, arc[-1] - 15.0, 4.0)
            rng.shuffle(candidates)
            placed = 0
            for s_c in candidates:
                if placed >= count:
                    break
                if np.any((np.abs(node_s - s_c) < 7.0) & (node_d > 2)):
                    continue                               # lejos de los cruces del laberinto
                k = int(np.searchsorted(arc, s_c))
                lo_k = int(np.searchsorted(arc, s_c - 10.0))
                hi_k = min(int(np.searchsorted(arc, s_c + 10.0)), len(poly) - 1)
                base = float(floor[k])
                if np.ptp(floor[lo_k:hi_k + 1]) > 2.0 or base - GORGE_DEPTH_M < WATER_M + 0.6:
                    continue                               # el fondo del canon no puede quedar bajo el agua
                c = poly[k]
                d = poly[min(k + 4, len(poly) - 1)] - poly[max(k - 4, 0)]
                d = d / max(float(np.linalg.norm(d)), 1e-9)
                n = np.array([-d[1], d[0]])
                if net_name == "canyon_maze":
                    s_route = float(self.zones.nearest(np.array([[c[0]]]), np.array([[c[1]]]))[1][0, 0])
                    if (s_route - c0) / max(c1 - c0, 1.0) > 0.4:
                        continue
                if self.tunnel_tree is not None and self.tunnel_tree.query(c)[0] < 20.0:
                    continue
                if any(np.hypot(*(c - b["c"])) < 40.0 for b in bridges):
                    continue
                ends = []
                for sign in (1.0, -1.0):
                    found = None
                    for dist in np.arange(GORGE_RAMP_M + 6.0, 46.0, 1.0):
                        p = c + sign * n * dist
                        dd, kk = tree.query(p)
                        if dd < 2.0 and abs(sample_floor[kk] - base) < 1.2 and poly_tree.query(p)[0] > 8.0:
                            found = float(dist)
                            break
                    ends.append(found)
                if ends[0] is None and ends[1] is None:
                    continue
                bridges.append({"net": net_name, "c": c, "d": d, "n": n, "base": base, "s": float(s_c),
                                "ends": ends})
                placed += 1
        return bridges

    def _plan_marks(self, rng: np.random.Generator) -> list[dict]:
        """Huella humana junto a los caminos (nunca encima): castillos de arena, fosas y zanjas
        cavadas. Se filtran al construir el relieve (solo en arena seca, fuera de laberintos)."""
        pts, arc = self.route.points, self.route.arc
        m0, _ = self.route.zone_range("marsh")
        marks = []
        for s in np.sort(rng.uniform(m0, arc[-1] - 40.0, self.style.human_marks * 3)):
            idx = int(np.searchsorted(arc, s))
            t = pts[min(idx + 2, len(pts) - 1)] - pts[max(idx - 2, 0)]
            t = t / max(float(np.linalg.norm(t)), 1e-9)
            n = np.array([-t[1], t[0]])
            c = pts[idx] + n * float(rng.choice([-1.0, 1.0])) * float(rng.uniform(9.0, 16.0))
            kind = str(rng.choice(["castle", "pit", "trench"], p=[0.45, 0.3, 0.25]))
            angle = float(rng.uniform(0.0, math.pi))
            marks.append({"kind": kind, "c": c, "angle": angle, "size": float(rng.uniform(0.8, 1.2))})
        return marks

    def _castle(self, X, Y, height, center, facing, base: float, radius: float, wall_h: float,
                tower_h: float, moat: bool):
        """Castillo de arena: patio llano, muralla redonda con puerta hacia 'facing' (por donde se
        llega), cuatro torres y, si moat, foso alrededor."""
        dx, dy = X - center[0], Y - center[1]
        dist = np.hypot(dx, dy)
        yaw = math.atan2(facing[1], facing[0])
        ang = np.abs((np.arctan2(dy, dx) - yaw + math.pi) % (2.0 * math.pi) - math.pi)
        yard = 1.0 - smooth(radius - 2.0, radius - 0.8, dist)
        out = height * (1.0 - yard) + base * yard
        gate = smooth(0.8, 1.2, ang / (1.8 / radius))
        wall = (1.0 - smooth(0.5, 1.0, np.abs(dist - radius))) * gate
        out = np.where(wall > 0.0, np.maximum(out, base + wall_h * wall), out)
        for k in range(4):
            a = yaw + math.pi / 4.0 + k * math.pi / 2.0
            tx, ty = center[0] + radius * math.cos(a), center[1] + radius * math.sin(a)
            tower = 1.0 - smooth(1.5, 2.1, np.hypot(X - tx, Y - ty))
            out = np.where(tower > 0.0, np.maximum(out, base + tower_h * tower), out)
        if moat:
            ditch = 1.0 - smooth(0.5, 1.1, np.abs(dist - radius - 2.4))
            out = out - 0.9 * ditch * gate
        return out

    def _apply_bridges(self, X, Y, height, maze_w, small):
        """Canon transversal de cada puente natural: hondo bajo el camino (que queda encima como
        tablero) y en rampa hasta la cota del pasillo que enlaza. Devuelve (relieve, mascara de
        camino para la erosion)."""
        keep = np.zeros_like(X)
        deck_w = self.style.corridor_width_m + 2.5
        for b in self.bridges:
            rel_x, rel_y = X - b["c"][0], Y - b["c"][1]
            along = rel_x * b["d"][0] + rel_y * b["d"][1]
            q = rel_x * b["n"][0] + rel_y * b["n"][1]
            lo = -(b["ends"][1] if b["ends"][1] is not None else 14.0)
            hi = b["ends"][0] if b["ends"][0] is not None else 14.0
            qc = np.clip(q, lo, hi)
            d_t = np.hypot(along, q - qc)
            # Fondo: hondo bajo el camino y en rampa hacia cada pasillo que enlaza (un lado sin
            # enlace acaba en fondo de saco hondo).
            depth = np.ones_like(q)
            for side, reach in ((1.0, b["ends"][0]), (-1.0, b["ends"][1])):
                if reach is None:
                    continue
                ramp = 1.0 - smooth(reach - GORGE_RAMP_M, reach, np.abs(qc))
                depth = np.where(q * side > 0.0, ramp, depth)
            bottom = b["base"] - GORGE_DEPTH_M * depth + 0.3 * small
            # El tablero (el camino encima) no se toca: el paso por debajo es el arco 3D.
            deck = (1.0 - smooth(deck_w, deck_w + 1.5, np.abs(q))) * (1.0 - smooth(4.0, 6.0, np.abs(along)))
            cut = (1.0 - smooth(GORGE_HALF_WIDTH_M, GORGE_HALF_WIDTH_M + 2.5, d_t)) * (1.0 - deck) * maze_w
            height = height * (1.0 - cut) + np.minimum(height, bottom) * cut
            keep = np.maximum(keep, np.maximum(cut, deck * maze_w))
        return height, keep

    def _apply_trails(self, X, Y, height, w, opening, s_route, small):
        """Camino principal marcado fuera de los laberintos: una franja algo hundida y llana que
        sigue el terreno; en el agua es una calzada seca por encima del nivel, con algun corte
        estrecho que se salta. Devuelve (relieve, mascara del camino)."""
        d_trail = self.trail_tree.query(np.stack([X.ravel(), Y.ravel()], axis=1))[0].reshape(X.shape)
        tw = self.style.trail_half_width_m * (0.85 + 0.3 * self.n_width.unit(X, Y))
        open_w = w["marsh"] + w["algae"] + w["beach"] + w["canyon"] * smooth(0.5, 0.9, opening)
        trail = (1.0 - smooth(tw, tw + 2.5, d_trail)) * np.clip(open_w, 0.0, 1.0)
        level = ndimage.gaussian_filter(height, 5.0) if height.ndim == 2 and min(height.shape) > 16 else height
        dry = level - 0.25
        causeway = WATER_M + 0.9 + 0.2 * small
        target = np.maximum(dry, causeway)
        # Cortes que se saltan, solo donde la calzada cruza agua.
        over_water = smooth(WATER_M + 0.3, WATER_M - 0.3, level)
        jump = np.mod(s_route + 3.0 * self.n_crest(Y, X), 42.0) - 21.0
        gap = (1.0 - smooth(0.5, 0.9, np.abs(jump))) * over_water
        target = target * (1.0 - gap) + (WATER_M - 0.6) * gap
        return height * (1.0 - trail) + target * trail, trail

    def _apply_marks(self, X, Y, height, w, trail_mask):
        """Castillos de arena, fosas y zanjas (solo en arena seca, fuera de laberintos y del
        camino). Necesita la rejilla completa del mapa (cota local por indice)."""
        if height.shape != (len(self.axis), len(self.axis)):
            return height
        placed = 0
        for m in self.marks:
            if placed >= self.style.human_marks:
                break
            i = int(np.clip(round(m["c"][0] - self.axis[0]), 0, len(self.axis) - 1))
            j = int(np.clip(round(m["c"][1] - self.axis[0]), 0, len(self.axis) - 1))
            lo_i, hi_i, lo_j, hi_j = max(i - 12, 0), i + 13, max(j - 12, 0), j + 13
            patch = height[lo_i:hi_i, lo_j:hi_j]
            if patch.size == 0 or patch.min() < WATER_M + 0.8 or np.ptp(patch) > 3.5:
                continue
            if trail_mask[lo_i:hi_i, lo_j:hi_j].max() > 0.05:
                continue
            if w["cliffs"][i, j] + w["canyon"][i, j] > 0.2:
                continue
            base = float(np.median(patch))
            sub_x, sub_y = X[lo_i:hi_i, lo_j:hi_j], Y[lo_i:hi_i, lo_j:hi_j]
            size = m["size"]
            if m["kind"] == "castle":
                to_path = -np.array([math.cos(m["angle"]), math.sin(m["angle"])])
                patch = self._castle(sub_x, sub_y, patch, m["c"], to_path, base, 3.6 * size, 1.6, 3.0, True)
            else:
                ca, sa = math.cos(m["angle"]), math.sin(m["angle"])
                u = (sub_x - m["c"][0]) * ca + (sub_y - m["c"][1]) * sa
                v = -(sub_x - m["c"][0]) * sa + (sub_y - m["c"][1]) * ca
                if m["kind"] == "pit":
                    hu, hv, depth = 2.6 * size, 1.7 * size, 1.7
                else:
                    hu, hv, depth = 7.5 * size, 0.7, 1.0
                box = np.maximum(np.abs(u) - hu, 0.0) ** 2 + np.maximum(np.abs(v) - hv, 0.0) ** 2
                hole = 1.0 - smooth(0.0, 1.1, np.sqrt(box))
                spoil = (1.0 - smooth(1.2, 2.6, np.sqrt(box))) * (1.0 - hole)       # arena sacada, al borde
                patch = patch - depth * hole + 0.35 * spoil
            height = height.copy()
            height[lo_i:hi_i, lo_j:hi_j] = patch
            placed += 1
        return height

    def _apply_landmarks(self, X, Y, height):
        """Salida: nido llano rodeado de mojones redondeados, abierto hacia el camino. Meta: un
        castillo de arena grande con el final del recorrido en su patio."""
        start = self.route.points[0]
        ahead = self.route.points[min(20, len(self.route.points) - 1)] - start
        yaw = math.atan2(ahead[1], ahead[0])
        dist = np.hypot(X - start[0], Y - start[1])
        base = 0.3 * self.n_small(X, Y)
        for k in range(8):
            a = yaw + math.radians(55.0 + k * 250.0 / 7.0)
            mx, my = start[0] + 8.0 * math.cos(a), start[1] + 8.0 * math.sin(a)
            mound = 1.0 - smooth(0.6, 2.3, np.hypot(X - mx, Y - my))
            height = np.where(mound > 0.0, np.maximum(height, base + 1.6 * mound), height)
        nest = 1.0 - smooth(4.5, 6.0, dist)
        height = height * (1.0 - nest) + base * nest
        end = self.route.points[-1]
        back = self.route.points[max(len(self.route.points) - 21, 0)] - end
        return self._castle(X, Y, height, end, back, END_PLAZA_M, 8.5, 2.6, 5.0, False)

    def trail_mask(self, x, y) -> np.ndarray:
        """0..1 por punto: camino principal (fuera y dentro de los laberintos), para el color."""
        pts = np.stack([np.ravel(x), np.ravel(y)], axis=1)
        out = np.zeros(len(pts))
        width = self.style.trail_half_width_m
        for tree in (self.trail_tree, self.chain_tree):
            if tree is not None:
                d = tree.query(pts)[0]
                out = np.maximum(out, 1.0 - smooth(width * 0.8, width + 1.2, d))
        return out.reshape(np.shape(x))

    def plaza_mask(self, x, y) -> np.ndarray:
        """0..1: nido de salida y patio de la meta (color propio: se reconocen desde lejos)."""
        if not self.style.landmarks:
            return np.zeros(np.shape(x))
        a, b = self.route.points[0], self.route.points[-1]
        d = np.minimum(np.hypot(x - a[0], y - a[1]), np.hypot(x - b[0], y - b[1]) * 0.75)
        return 1.0 - smooth(5.0, 7.0, d)

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
        d, k = self.net_trees[zone].query(pts, k=min(32, len(floors)))
        if k.ndim == 1:            # solo hay 1 punto en la red: query(k=1) devuelve 1D, no (n, 1)
            d, k = d[:, None], k[:, None]
        weight = np.exp(-0.5 * ((d - d[:, :1]) / smooth_m) ** 2)
        level = (floors[k] * weight).sum(axis=1) / weight.sum(axis=1)
        return d[:, 0].reshape(X.shape), level.reshape(X.shape)

    def _clearing(self, zone: str, X, Y):
        """Plazas llanas de contorno natural: elipse girada (aspecto y giro sacados de su
        posicion) con el borde deformado por ruido, en vez de un circulo."""
        weight, level = np.zeros_like(X), np.zeros_like(X)
        wobble = getattr(self, "n_clearing", None)
        for cx, cy, r, lv in self.clearings.get(zone, np.zeros((0, 4))):
            angle = (cx * 12.9898 + cy * 78.233) % math.pi
            aspect = 1.0 + 0.8 * (((cx * 3.1 + cy * 7.7) % 1.0))
            dx, dy = X - cx, Y - cy
            u = (dx * math.cos(angle) + dy * math.sin(angle)) / aspect
            v = (-dx * math.sin(angle) + dy * math.cos(angle)) * math.sqrt(aspect)
            dist = np.hypot(u, v)
            if wobble is not None:
                dist = dist + 0.35 * r * wobble(X, Y)
            m = 1.0 - smooth(r, r + 3.0, dist)
            level = np.where(m > weight, lv, level)
            weight = np.maximum(weight, m)
        return weight, level

    def _dune_set(self, X, Y, angle: float, wave: float, rise: float = 0.72):
        along = X * math.cos(angle) + Y * math.sin(angle)
        across = -X * math.sin(angle) + Y * math.cos(angle)
        phase = (along + 0.9 * wave * self.n_dune_warp(X, Y) + 0.25 * across * self.n_dune_mix(X, Y)) / wave
        frac = phase - np.floor(phase)
        profile = np.where(frac < rise, frac / rise, (1.0 - frac) / (1.0 - rise))
        return smooth(0.0, 1.0, profile)

    def _beach_dunes(self, X, Y, scale: float = 1.0):
        """Dunas de playa que se pasean: crestas redondeadas, caras de avalancha tendidas
        (~25 grados como mucho) y relieve que cambia por zonas (de casi llano a 7 m)."""
        mix = smooth(0.35, 0.65, self.n_dune_mix.unit(X, Y))
        wave_1, wave_2 = self.style.dune_wave_m
        rise = self.style.dune_rise
        field = self._dune_set(X, Y, self.dune_angle, wave_1, rise) * (1 - mix) \
            + self._dune_set(X, Y, self.dune_angle_2, wave_2, rise) * mix
        amp_lo, amp_hi = self.style.dune_amp_range_m
        amp = scale * (amp_lo + (amp_hi - amp_lo) * smooth(0.25, 0.85, self.n_beach_amp.unit(X, Y)))
        return amp * field

    def _dunes(self, X, Y, amp, wave: float):
        """Dunas con cara de avalancha, mezcla de dos direcciones de viento y crestas que
        crecen y se apagan: sin rayas paralelas regulares."""
        mix = smooth(0.35, 0.65, self.n_dune_mix.unit(X, Y))
        field = self._dune_set(X, Y, self.dune_angle, wave) * (1 - mix) \
            + self._dune_set(X, Y, self.dune_angle_2, wave * 1.3) * mix
        strength = smooth(0.2, 0.8, self.n_dune_amp.unit(X, Y))
        return amp * field * (0.15 + 0.85 * strength)

    def _chain_bonus(self, X, Y):
        """Ensanche del camino bueno de los laberintos (se lee como el camino principal)."""
        if getattr(self, "chain_tree", None) is None:
            return 0.0
        d = self.chain_tree.query(np.stack([X.ravel(), Y.ravel()], axis=1))[0].reshape(X.shape)
        return 1.3 * (1.0 - smooth(3.0, 9.0, d))

    def _rock_walls(self, X, Y, d, hw, f, top, face_width: float = 1.1, rim: float = 0.0, bridge=None):
        """Pasillo en roca con aspecto erosionado: contrafuertes, grietas que se meten en la
        pared y un talud de derrubios al pie. face_width: anchura horizontal de la cara (1 m ~
        pared vertical; 3-4 m, 60-75 grados). rim: fraccion de la altura que se redondea en el
        borde de arriba (0 = arista viva). Devuelve (relieve, banda de pared, eje)."""
        face = d - hw                                   # > 0: detras de la cara de la pared
        if np.any(rim > 0.0):
            # Borde superior gastado: la cima baja en curva hacia el canon.
            top = top - rim * np.maximum(top - f, 0.0) * (1.0 - smooth(0.0, 7.0 + face_width, face))
        # La cara se abre hacia arriba: el pie es estrecho y el borde queda retranqueado.
        corridor = 1.0 - smooth(0.0, face_width, face)
        if bridge is not None:
            # Puente: la roca cruza el pasillo a la cota del tablero (el arco se excava en 3D).
            band, deck = bridge
            corridor = corridor * (1.0 - band)
            top = np.where(band > 0.0, top + (np.minimum(top, deck) - top) * band, top)
        # Grietas: lineas de ruido (pasos por cero) que cortan la pared casi hasta el suelo,
        # solo cerca de la cara y solo en parte de las paredes.
        crack = (1.0 - smooth(0.04, 0.13, np.abs(self.n_crack(X, Y)))) * smooth(0.45, 0.6, self.n_crack_mask.unit(X, Y))
        crack = crack * (1.0 - smooth(1.0, 9.0, face)) * smooth(-1.0, 0.5, face)
        if crack.ndim == 2 and min(crack.shape) > 8:
            crack = ndimage.gaussian_filter(crack, 1.0)     # sin rendijas de una celda (aletas)
        top = top - crack * np.maximum(top - f - 0.8, 0.0) * 0.9
        # Talud de derrubios: rampa de arena al pie de la pared, irregular.
        talus = (top - f) * 0.36 * (0.4 + 0.6 * self.n_talus.unit(X, Y)) * smooth(-3.2, 0.4, face)
        floor = f + talus
        height = top * (1.0 - corridor) + floor * corridor
        band = 1.0 - smooth(0.0, 3.5, np.abs(face - 0.3))
        axis = 1.0 - smooth(0.25 * hw, 0.6 * hw, d)
        return height, band, axis

    # ── Campos 2D ────────────────────────────────────────────────────────────────
    def _fields(self, X, Y) -> Fields:
        d_route, s_route = self.zones.nearest(X, Y)
        w = self.zones.weights(X, Y, s_route, d_route)
        if getattr(self, "tunnel_tree", None) is not None:
            u_tunnel = self._tunnel_query(X, Y)[0]
            tunnel_hill = TUNNEL_HILL_M * (1.0 - smooth(TUNNEL_HALF_WIDTH_M + 2.0, TUNNEL_HALF_WIDTH_M + 13.0, u_tunnel))
        else:
            tunnel_hill = np.zeros_like(X)
        small, mid, big = self.n_small(X, Y), self.n_mid(X, Y), self.n_big(X, Y)
        width = self.n_width.unit(X, Y)
        heights, floors, bands, axes = {}, {}, {}, {}

        # Acantilados: laberinto por niveles en roca arenosa, con claros y plaza de salida.
        d, level = self._net("cliffs", X, Y)
        _, level_soft = self._net("cliffs", X, Y, smooth_m=10.0)
        clear_w, clear_level = self._clearing("cliffs", X, Y)
        start = self.route.points[0]
        start_w = 1.0 - smooth(9.0, 13.0, np.hypot(X - start[0], Y - start[1]))
        hw = self.style.corridor_width_m + 1.3 * width + 0.9 * self.n_buttress(X, Y) + self._chain_bonus(X, Y)
        f = (level * (1.0 - clear_w) + clear_level * clear_w) * (1.0 - start_w) + 0.3 * small
        # Cimas a alturas distintas (no una meseta): style.cliff_height_m sobre el nivel, repisas
        # a medias y una ondulacion extra (n_top_roll) para que la cima nunca quede plana.
        cliff_lo, cliff_hi = self.style.cliff_height_m
        raw_top = level_soft * (1.0 - start_w) + cliff_lo + (cliff_hi - cliff_lo) * smooth(0.15, 0.9, self.n_top.unit(X, Y)) \
            + 2.0 * self.n_mid.unit(X, Y) + 1.2 * small
        # Cima ondulada (lomas de arena sobre la roca): nunca baja, para no quitar cubierta a los tuneles.
        top = 0.45 * raw_top + 0.55 * strata(raw_top, 3.5) + tunnel_hill + 4.0 * np.abs(self.n_top_roll(X, Y))
        d_eff = d * (1.0 - np.maximum(clear_w, start_w))
        bridge = self._arch_band(X, Y)
        heights["cliffs"], bands["cliffs"], axes["cliffs"] = self._rock_walls(X, Y, d_eff, hw, f, top, rim=0.22,
                                                                             bridge=bridge)
        # Lejos de los pasillos el suelo de referencia es el nivel suavizado: el muro exterior
        # que sube desde el no marca la bisectriz entre dos caminos de niveles distintos.
        near_corridor = 1.0 - smooth(hw + 1.0, hw + 9.0, d_eff)
        floors["cliffs"] = f * near_corridor + (level_soft * (1.0 - start_w)) * (1.0 - near_corridor)

        # Canon: otro laberinto de acantilados (2 niveles), de caras algo mas tendidas y cimas
        # onduladas; sus tuneles son tramos del laberinto excavados en la roca.
        # Pasillos ondulados (dominio deformado): sin tramos rectos de regla.
        cwx = X + 4.5 * self.canyon_warp[0](X, Y)
        cwy = Y + 4.5 * self.canyon_warp[1](X, Y)
        d, level = self._net("canyon_maze", cwx, cwy)
        _, level_soft = self._net("canyon_maze", cwx, cwy, smooth_m=10.0)
        clear_w, clear_level = self._clearing("canyon_maze", X, Y)
        # Apertura: a lo largo del tramo los pasillos se ensanchan y las paredes bajan, hasta
        # fundirse con la zona encharcada (el agua entra en el final del laberinto).
        c0, c1 = self.route.zone_range("canyon")
        opening = smooth(0.35, 1.0, np.clip((s_route - c0) / max(c1 - c0, 1.0), 0.0, 1.0))
        hw = self.style.corridor_width_m + 1.4 * width + 0.9 * self.n_buttress(X, Y) + 5.0 * opening \
            + self._chain_bonus(X, Y)
        f = (level * (1.0 - clear_w) + clear_level * clear_w) + 0.3 * small
        # Acantilados de altura muy distinta segun la zona (style.canyon_height_m) y cimas a repisas.
        canyon_lo, canyon_hi = self.style.canyon_height_m
        raw_top = level_soft + canyon_lo + (canyon_hi - canyon_lo) * smooth(0.2, 0.9, self.n_cliff_height.unit(X, Y)) \
            + 1.5 * big + 1.2 * small
        top = 0.35 * raw_top + 0.65 * strata(raw_top, 3.0) + tunnel_hill + 4.0 * np.abs(self.n_top_roll(X, Y))
        top = f + (top - f) * (1.0 - 0.65 * opening)
        d_eff = d * (1.0 - clear_w)
        # Pendiente que cambia por tramos: de pared casi vertical (1 m) a ladera de 55 grados.
        face_w = 1.0 + 3.2 * smooth(0.3, 0.8, self.n_steep.unit(X, Y))
        heights["canyon"], bands["canyon"], axes["canyon"] = self._rock_walls(
            X, Y, d_eff, hw, f, top, face_width=face_w + 3.0 * opening, rim=0.26 + 0.2 * opening, bridge=bridge)
        near_corridor = 1.0 - smooth(hw + 1.0, hw + 9.0, d_eff)
        floors["canyon"] = f * near_corridor + level_soft * (1.0 - near_corridor)

        # Zona encharcada: dunas cuyas vaguadas quedan bajo el agua (laguitos); se cruza por
        # una cresta seca que sigue la ruta, con algun hueco que se salta.
        # Laguitos en las hondonadas de un relieve de manchas (no en franjas) y dunas encima.
        flat = 1.0 - self.style.ground_flatten
        base = WATER_M - 0.6 + self.style.pond_depth_m * self.n_pond(X, Y) + self._dunes(X, Y, 2.4 * (0.4 + 0.6 * flat), 22.0)
        # Sendero de arena natural (no un dique): estrecho, de orillas suaves e irregulares,
        # que serpentea alrededor de la ruta (dominio deformado) y sube y baja con las dunas;
        # los charcos lo bordean. De vez en cuando un charco estrecho lo corta y se salta.
        wx = X + MARSH_WIGGLE_M * self.marsh_warp[0](X, Y)
        wy = Y + MARSH_WIGGLE_M * self.marsh_warp[1](X, Y)
        d_path, s_path = self.zones.nearest(wx, wy)
        bar_w = 1.6 + 2.2 * self.n_width.unit(X, Y)
        crest = 1.0 - smooth(bar_w, bar_w + 3.5, d_path + 1.4 * self.n_crest(X, Y))
        if self.style.trails:
            crest = crest * 0.0          # el camino lo marca _apply_trails (calzada trenzada)
        # Segundo sendero que se separa y se vuelve a juntar (trenzado): no hay una sola linea.
        wx2 = X + 16.0 * self.marsh_warp_2[0](X, Y)
        wy2 = Y + 16.0 * self.marsh_warp_2[1](X, Y)
        d_path_2, _ = self.zones.nearest(wx2, wy2)
        crest = np.maximum(crest, 0.85 * (1.0 - smooth(1.2, 4.0, d_path_2 + 1.4 * self.n_crest(Y, X))))
        channel_s = np.mod(s_path + 4.0 * self.n_crest(Y, X), MARSH_JUMP_EVERY_M) - MARSH_JUMP_EVERY_M / 2
        # El corte solo existe sobre el sendero: no es un rio que cruce toda la zona.
        gap = (1.0 - smooth(0.5, 1.0, np.abs(channel_s))) * smooth(0.85, 0.95, w["marsh"]) * smooth(0.3, 0.8, crest)
        path_top = MARSH_PATH_TOP_M + 0.5 * self._dunes(X, Y, 1.2, 16.0) + 0.3 * small \
            + 0.35 * self.n_hummock(X, Y)                                  # montoncitos: suelo rugoso
        land = base * (1.0 - crest) + np.maximum(base, path_top) * crest
        heights["marsh"] = land * (1.0 - gap) + np.minimum(land, WATER_M - 0.6) * gap
        floors["marsh"] = np.full_like(X, MARSH_PATH_TOP_M)
        # El final del canon se funde con la zona encharcada: el agua entra entre sus paredes.
        into_water = smooth(0.4, 0.85, opening)
        heights["canyon"] = heights["canyon"] * (1.0 - into_water) + heights["marsh"] * into_water
        bands["marsh"] = np.zeros_like(X)
        axes["marsh"] = 1.0 - smooth(0.5, 1.5, d_path)

        # Dunas: la zona mas plana del recorrido, dunas de playa lisas, siempre arena (sin
        # follaje ni verde: PLANT_ALGAE=False y su paleta es "sand", ver mesh.ZONE_PALETTE).
        d, _ = self._net("algae", X, Y)
        hw = 1.8 + 0.8 * width
        f = 0.8 + 2.0 * flat * self.n_big.unit(X, Y) + self.style.dune_amplitude_m * self._beach_dunes(X, Y)
        heights["algae"], floors["algae"] = f, f
        bands["algae"] = np.zeros_like(X)
        axes["algae"] = 1.0 - smooth(0.5 * hw, hw, d)
        foliage = smooth(hw + 0.2, hw + 1.1, d) * (0.92 + 0.08 * small)

        # Playa final: arena que baja hasta el mar por el borde norte del mapa.
        u = MAP_MAX_M - X + 18.0 * self.n_edge(X, Y)
        hills = 3.0 * flat * np.clip(self.n_beach_hills(X, Y) + 0.35, 0.0, None) + self._beach_dunes(X, Y, 1.1)
        beach = WATER_M - 2.5 + 5.0 * smooth(4.0, 70.0, u) + hills * smooth(40.0, 95.0, u)
        heights["beach"], floors["beach"] = beach, beach
        bands["beach"] = np.zeros_like(X)
        axes["beach"] = np.zeros_like(X)

        height = np.sum([w[z] * heights[z] for z in ZONE_NAMES], axis=0)
        floor = np.sum([w[z] * floors[z] for z in ZONE_NAMES], axis=0)
        band = np.sum([w[z] * bands[z] for z in ZONE_NAMES], axis=0)
        path = np.sum([w[z] * axes[z] for z in ZONE_NAMES], axis=0)
        foliage = w["algae"] * foliage
        if getattr(self, "bridges", None):
            height, keep = self._apply_bridges(X, Y, height, np.clip(w["cliffs"] + w["canyon"], 0.0, 1.0), small)
            path = np.maximum(path, keep)
        trail_mask = np.zeros_like(X)
        if getattr(self, "trail_tree", None) is not None:
            height, trail_mask = self._apply_trails(X, Y, height, w, opening, s_route, small)
            path = np.maximum(path, trail_mask)

        if getattr(self, "marks", None):
            height = self._apply_marks(X, Y, height, w, trail_mask)

        # Muro exterior (fuera del alcance de cada zona), con relieve: el mismo en todas las
        # zonas, asi que no marca la frontera entre ellas.
        reach = self.zones.reach(w, X, Y) + w["canyon"] * (self.style.canyon_maze_reach_m - self.style.canyon_reach_m) \
            * (0.65 + 0.7 * self.zones.reach_noise.unit(X, Y))
        outer = smooth(reach, reach + 22.0, d_route)
        # Relieve del muro: bajo (mid/big) mas una rugosidad propia (border_roughness_m, siempre
        # activa) que rompe la ladera lisa y de pendiente constante.
        rough = self.style.border_roughness_m * (0.5 + 0.5 * self.n_border(X, Y)) * (0.4 + 0.6 * np.abs(self.n_border_fine(Y, X)))
        relief = 4.0 * mid + 3.0 * big + self._dunes(X, Y, 2.5, 34.0) + 0.6 * rough
        # Maximo suave: el muro sale del terreno sin arista (antes, un pliegue visible).
        rock = soft_max(height, floor + OUTER_WALL_M + relief, 3.0)
        dune_wall = soft_max(height, 6.0 + relief + self._beach_dunes(X, Y, 0.8), 5.0)
        soft = w["marsh"] + w["algae"]
        wall = rock * (1.0 - soft) + dune_wall * soft
        height = height * (1 - outer) + wall * outer
        foliage = foliage * (1 - outer)
        path = path * (1 - outer)

        # Borde del mapa: muro de silueta irregular al sur, este y oeste, con cresta accidentada
        # (alturas variables, sin laderas planas de regla) que sigue cerrando el mapa.
        warp = self.style.border_roughness_m * self.n_edge(X, Y) + 0.5 * self.style.border_roughness_m * self.n_border_fine(X, Y)
        edges = [X - MAP_MIN_M + warp, Y - MAP_MIN_M + warp, MAP_MAX_M - Y + warp]
        near = np.maximum.reduce([1.0 - smooth(4.0, 28.0, e) for e in edges])
        edge_wall = np.maximum(height, EDGE_WALL_M + relief + rough)
        height = height * (1 - near) + edge_wall * near
        # Final del mapa: todo el borde norte es playa que baja al mar (sin muro).
        shore = 1.0 - smooth(70.0, 120.0, u)
        height = height * (1 - shore) + beach * shore
        # Rios de la zona de agua: cauce por debajo de WATER_M que serpentea hasta el mar, con
        # islitas-peldano (siempre saltables) dentro del cauce. Se aplica a todo el mapa (no por
        # zona): el rio cruza lo que haga falta para llegar al borde norte.
        for pts, arc in getattr(self, "rivers", []):
            tree = cKDTree(pts)
            d_river, k = tree.query(np.stack([X.ravel(), Y.ravel()], axis=1))
            d_river = d_river.reshape(X.shape)
            t = (arc[np.clip(k, 0, len(arc) - 1)] / max(arc[-1], 1e-9)).reshape(X.shape)
            wobble = 0.55 if self.style.river_rapids else 0.3
            river_w = self.style.river_width_m * (0.7 + 0.6 * t) * (1.0 + wobble * self.n_dune_mix.unit(X, Y))
            bank = smooth(river_w * 0.5, river_w * 1.5, d_river)
            bed = WATER_M - self.style.river_depth_m * (0.5 + 0.5 * self.n_pond.unit(X, Y))
            height = height * bank + np.minimum(height, bed) * (1.0 - bank)
        for center, radius in getattr(self, "islands", []):
            dist = np.hypot(X - center[0], Y - center[1])
            m = 1.0 - smooth(radius * 0.6, radius, dist)
            target = np.maximum(height, WATER_M + 1.3 + 0.4 * self.n_hummock(X, Y))
            height = height * (1.0 - m) + target * m

        foliage = foliage * (1 - shore)
        end = self.route.points[-1]
        plaza = 1.0 - smooth(10.0, 16.0, np.hypot(X - end[0], Y - end[1]))
        height = height * (1 - plaza) + np.maximum(height, END_PLAZA_M) * plaza
        if self.style.landmarks:
            height = self._apply_landmarks(X, Y, height)
        height = np.minimum(height, TOP_LIMIT_M)
        if self.tunnel_tree is not None:
            u, _, _, _ = self._tunnel_query(X, Y)
            tunnel_zone = 1.0 - smooth(TUNNEL_HALF_WIDTH_M + 1.5, TUNNEL_HALF_WIDTH_M + 5.0, u)
        else:
            tunnel_zone = np.zeros_like(X)
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
        h = despike(h, rocky)
        # Pasada ligera en todo el mapa: picos sueltos que dejan las mezclas entre zonas.
        return despike(h, np.ones_like(h), limit=2.0, passes=2, blur=0.0)

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
            D = D + 0.6 * band * above_floor * self.n_wall3d(X3, Y3, Z3)
        if np.any(f.tunnel > 0.0):
            carve = self._tunnel_carve(f, Z3, X, Y)
            # Paredes y techo rugosos, no una boveda perfecta.
            carve = carve + 0.6 * self.n_wall3d(X3 * 1.7, Y3 * 1.7, Z3 * 1.7) * (carve > -0.5)
            D = np.minimum(D, -carve)
        return D

    def _tunnel_carve(self, f: Fields, Z3, X, Y):
        """> 0 dentro del hueco del tunel: seccion eliptica redondeada (sin cantos) que cambia
        de ancho y alto a lo largo, sobre un suelo plano que baja (o sube) por dentro y vuelve
        a la cota del laberinto en cada boca."""
        if self.tunnel_tree is None:
            return np.full(np.broadcast_shapes(X.shape + (1,), Z3.shape), -1.0)
        u, t, floor, dip = self._tunnel_query(X, Y)
        tc = np.clip(t, 0.0, 1.0)
        base = floor + dip * np.sin(np.pi * tc)
        half = TUNNEL_HALF_WIDTH_M * (1.0 + 0.22 * self.n_tunnel(X, Y))
        height = TUNNEL_HEIGHT_M * (1.0 + 0.18 * self.n_tunnel(Y, X))
        v = Z3 - base[..., None]
        center = 0.42 * height[..., None]
        radius_v = 0.62 * height[..., None]
        ellipse = 1.0 - np.sqrt((u[..., None] / half[..., None]) ** 2 + ((v - center) / radius_v) ** 2)
        inside = np.minimum(ellipse * half[..., None], v + 0.3)      # suelo plano para caminar
        # Fuera del tramo (mas alla de la prolongacion de las bocas) no se excava.
        active = (t > -0.2) & (t < 1.2)
        return np.where(active[..., None], inside, -1.0)


def soft_max(a, b, k: float):
    """Maximo suave de a y b (transicion de anchura ~k): sin arista donde se cruzan."""
    return 0.5 * (a + b + np.sqrt((a - b) ** 2 + k * k)) - 0.5 * k


def end_direction(curve: np.ndarray, reach: float = 2.0) -> np.ndarray:
    """Direccion unitaria con la que la curva sale de su primer punto, medida hasta el
    primer punto a mas de reach metros (las curvas suavizadas repiten sus extremos)."""
    dist = np.linalg.norm(curve - curve[0], axis=1)
    k = int(np.argmax(dist > reach)) if np.any(dist > reach) else len(curve) - 1
    d = curve[k] - curve[0]
    return d / max(float(np.linalg.norm(d)), 1e-9)


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


def despike(h: np.ndarray, where: np.ndarray, limit: float = 1.2, passes: int = 3, blur: float = 0.6) -> np.ndarray:
    """Quita picos y pozos de una celda (aletas finas en la malla): donde la celda se aparta
    de la mediana de sus vecinas mas de limit metros, toma la mediana. Luego un desenfoque
    leve solo en la roca."""
    h = h.copy()
    for _ in range(passes):
        med = ndimage.median_filter(h, size=3)
        spike = (np.abs(h - med) > limit) & (where > 0.05)
        h = np.where(spike, med, h)
    if blur <= 0.0:
        return h
    soft = ndimage.gaussian_filter(h, blur)
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
