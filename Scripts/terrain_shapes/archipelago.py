"""Archipielagos ficticios parametricos: N islas (elipses de costa organica, una puede tener forma de tortuga),
volcan central, lagunas interiores y puentes naturales tallados entre islas, con semilla. Todo en metros de
diseño (e, n) desde el centro del volumen y cotas sobre el agua.

    spec = philippines_rare()                          # I01 del catalogo (Docs/Catalogo-Mapas-2026-09-29.md)
    spec = random_archipelago(seed=7, islands=5)       # uno cualquiera
    shape = build_archipelago(spec)                    # ShapeMap: modelo, inicio, puntos y pasarelas a validar
"""

from __future__ import annotations

import math
from dataclasses import asdict, dataclass, field

import numpy as np

from terrain_vol.layout import WATER_M
from terrain_vol.noise import Fbm2D

from .bridges import NaturalBridge
from .canvas import SEABED_M, Canvas, plateau, sd_circle, sd_ellipse, smoothstep
from .model import ShapeMap, ShapeModel


@dataclass(frozen=True)
class IslandSpec:
    name: str
    center: tuple[float, float]
    radii: tuple[float, float]          # semiejes (m) a lo largo de angle_deg y en perpendicular
    height_m: float                     # meseta sobre el agua
    angle_deg: float = 0.0
    turtle: bool = False                # con cabeza, aletas y cola (mira hacia angle_deg + 90)


@dataclass(frozen=True)
class VolcanoSpec:
    island: str                         # isla que lo lleva (centro = el de la isla salvo offset)
    base_radius_m: float
    top_radius_m: float
    peak_m: float                       # cima sobre el agua
    crater_radius_m: float = 0.0        # 0 = cima plana
    crater_depth_m: float = 0.0
    offset: tuple[float, float] = (0.0, 0.0)

    def slope_deg(self, base_height_m: float) -> float:
        return math.degrees(math.atan((self.peak_m - base_height_m) / (self.base_radius_m - self.top_radius_m)))


@dataclass(frozen=True)
class LagoonSpec:
    center: tuple[float, float]
    radius_m: float
    depth_m: float = 1.5                # bajo el agua


@dataclass(frozen=True)
class ArchipelagoSpec:
    name: str
    seed: int
    grid: int
    description: str
    islands: tuple[IslandSpec, ...]
    volcano: VolcanoSpec | None = None
    lagoons: tuple[LagoonSpec, ...] = ()
    bridge_count: int = 9
    bridge_max_m: float = 60.0
    bridge_width_m: float = 4.0
    bridge_max_slope_deg: float = 20.0
    shore_m: float = 3.0                # ancho de la orilla (de la meseta al agua)
    coast_noise_m: float = 1.6          # ondulacion de la costa
    start_island: str = ""
    mode: str = "tct"
    unreachable_ok: tuple[str, ...] = ()     # islas sin puente a proposito
    extra: dict = field(default_factory=dict)


# ── Formas ───────────────────────────────────────────────────────────────────────
def _local(e, n, island: IslandSpec):
    a = math.radians(island.angle_deg)
    de, dn = e - island.center[0], n - island.center[1]
    return de * math.cos(a) + dn * math.sin(a), -de * math.sin(a) + dn * math.cos(a)


def island_sd(island: IslandSpec, e, n) -> np.ndarray:
    """Distancia con signo (m, positiva dentro) a la isla, sin la ondulacion de la costa."""
    ra, rb = island.radii
    u, v = _local(e, n, island)
    sd = sd_ellipse(u, v, 0.0, 0.0, ra, rb)
    if island.turtle:                                    # cabeza al frente (+v), aletas en diagonal, cola atras
        parts = [sd_circle(u, v, 0.0, 1.08 * rb, 0.24 * rb),
                 sd_ellipse(u, v, 0.95 * ra, 0.5 * rb, 0.55 * ra, 0.16 * rb, 35.0),
                 sd_ellipse(u, v, -0.95 * ra, 0.5 * rb, 0.55 * ra, 0.16 * rb, -35.0),
                 sd_ellipse(u, v, 0.85 * ra, -0.62 * rb, 0.42 * ra, 0.13 * rb, -35.0),
                 sd_ellipse(u, v, -0.85 * ra, -0.62 * rb, 0.42 * ra, 0.13 * rb, 35.0),
                 sd_ellipse(u, v, 0.0, -1.08 * rb, 0.1 * rb, 0.14 * rb)]
        for part in parts:
            sd = np.maximum(sd, part)
    return sd


class Coast:
    """Distancias con signo de todas las islas con la costa ondulada (misma ondulacion en todo el mapa)."""

    def __init__(self, spec: ArchipelagoSpec):
        self.spec = spec
        self.noise = Fbm2D(np.random.default_rng(spec.seed), 22.0, octaves=3)

    def sd(self, island: IslandSpec, e, n) -> np.ndarray:
        return island_sd(island, e, n) + self.spec.coast_noise_m * self.noise(np.asarray(n) + 250.0, np.asarray(e) + 250.0)

    def by_name(self, name: str) -> IslandSpec:
        return next(i for i in self.spec.islands if i.name == name)


def build_height(spec: ArchipelagoSpec, canvas: Canvas, coast: Coast) -> np.ndarray:
    e, n = canvas.design_grid()
    sds = {island.name: coast.sd(island, e, n) for island in spec.islands}
    height = np.full(e.shape, SEABED_M - 1.5)
    for island in spec.islands:
        height = np.maximum(height, plateau(sds[island.name], island.height_m, spec.shore_m))
    nearest = np.max(np.stack(list(sds.values())), axis=0)
    height = height - 1.5 * smoothstep(-4.0, -30.0, nearest)                  # mar mas hondo lejos de la costa
    if spec.volcano is not None:
        height = _add_volcano(spec, height, e, n, sds, coast)
    for lagoon in spec.lagoons:
        r = np.hypot(e - lagoon.center[0], n - lagoon.center[1])
        bowl = WATER_M - lagoon.depth_m + 40.0 * smoothstep(lagoon.radius_m - 3.0, lagoon.radius_m + 1.5, r)
        height = np.minimum(height, bowl)
    return height


def _add_volcano(spec: ArchipelagoSpec, height, e, n, sds, coast: Coast):
    v = spec.volcano
    island = coast.by_name(v.island)
    ce, cn = island.center[0] + v.offset[0], island.center[1] + v.offset[1]
    r = np.hypot(e - ce, n - cn)
    t = np.clip((r - v.top_radius_m) / (v.base_radius_m - v.top_radius_m), 0.0, 1.0)
    cone = WATER_M + v.peak_m - (v.peak_m - island.height_m) * t
    if v.crater_radius_m > 0.0:
        cone = cone - v.crater_depth_m * (1.0 - smoothstep(0.6 * v.crater_radius_m, v.crater_radius_m, r))
    land = smoothstep(0.0, spec.shore_m, sds[v.island])
    return height + np.clip(cone - height, 0.0, None) * land


# ── Puentes ──────────────────────────────────────────────────────────────────────
def _segments_cross(p1, p2, q1, q2) -> bool:
    def orient(a, b, c):
        return (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0])
    d1, d2 = orient(q1, q2, p1), orient(q1, q2, p2)
    d3, d4 = orient(p1, p2, q1), orient(p1, p2, q2)
    return d1 * d2 < 0 and d3 * d4 < 0


def _candidate(spec: ArchipelagoSpec, coast: Coast, height_at, i: IslandSpec, j: IslandSpec) -> NaturalBridge | None:
    """Puente de meseta a meseta a lo largo de la linea entre centros, o None si no vale."""
    inset = spec.shore_m + 1.0
    t = np.linspace(0.0, 1.0, int(math.hypot(j.center[0] - i.center[0], j.center[1] - i.center[1]) * 4) + 2)
    pe = i.center[0] + (j.center[0] - i.center[0]) * t
    pn = i.center[1] + (j.center[1] - i.center[1]) * t
    on_i, on_j = coast.sd(i, pe, pn) >= inset, coast.sd(j, pe, pn) >= inset
    if not on_j.any() or not on_i.any():
        return None
    arrive = int(np.argmax(on_j))                                    # primera muestra en la meseta de j
    before = np.nonzero(on_i[:arrive])[0]
    if len(before) == 0:
        return None
    leave = int(before[-1]) + 1                                      # primera muestra fuera de la meseta de i
    arrive -= 1
    if arrive <= leave:
        return None
    a, b = (float(pe[leave - 1]), float(pn[leave - 1])), (float(pe[arrive + 1]), float(pn[arrive + 1]))
    span = slice(leave, arrive + 1)
    for other in spec.islands:
        if other.name not in (i.name, j.name) and (coast.sd(other, pe[span], pn[span]) > -2.0).any():
            return None
    bridge = NaturalBridge(a, b, float(height_at(*a)), float(height_at(*b)), width_m=spec.bridge_width_m,
                           rise_m=float(np.clip(0.02 * math.hypot(b[0] - a[0], b[1] - a[1]), 0.3, 1.0)))
    if bridge.length > spec.bridge_max_m or bridge.slope_deg() > spec.bridge_max_slope_deg:
        return None
    return bridge


def plan_bridges(spec: ArchipelagoSpec, coast: Coast, height_at) -> list[tuple[str, str, NaturalBridge]]:
    """Arbol de puentes mas cortos que une todas las islas posibles (Kruskal) y, hasta bridge_count, los mas
    cortos que quedan sin cruzarse con otros."""
    names = [i.name for i in spec.islands]
    cands = []
    for x in range(len(spec.islands)):
        for y in range(x + 1, len(spec.islands)):
            i, j = spec.islands[x], spec.islands[y]
            if i.name in spec.unreachable_ok or j.name in spec.unreachable_ok:
                continue
            bridge = _candidate(spec, coast, height_at, i, j)
            if bridge is not None:
                cands.append((bridge.length, i.name, j.name, bridge))
    cands.sort(key=lambda c: c[0])
    parent = {name: name for name in names}

    def root(k):
        while parent[k] != k:
            parent[k] = parent[parent[k]]
            k = parent[k]
        return k

    chosen, rest = [], []
    for _, a, b, bridge in cands:
        if root(a) != root(b):
            parent[root(a)] = root(b)
            chosen.append((a, b, bridge))
        else:
            rest.append((a, b, bridge))
    for a, b, bridge in rest:
        if len(chosen) >= spec.bridge_count:
            break
        if not any(_segments_cross(bridge.a, bridge.b, c.a, c.b) for _, _, c in chosen):
            chosen.append((a, b, bridge))
    return chosen


# ── Montaje ──────────────────────────────────────────────────────────────────────
def plateau_point(island: IslandSpec, coast: Coast, spec: ArchipelagoSpec,
                  avoid: tuple[tuple[float, float, float], ...] = ()) -> tuple[float, float]:
    """Punto de la meseta de la isla lo mas adentro posible, fuera de los circulos avoid (e, n, radio): el volcan
    y las lagunas."""
    ra, rb = island.radii
    span = max(ra, rb) * 1.3
    e, n = np.meshgrid(np.linspace(-span, span, 121) + island.center[0], np.linspace(-span, span, 121) + island.center[1])
    depth = coast.sd(island, e, n)
    for ae, an, ar in avoid:
        depth = np.where(np.hypot(e - ae, n - an) < ar, -1e9, depth)
    k = int(np.argmax(depth))
    return float(e.flat[k]), float(n.flat[k])


def build_archipelago(spec: ArchipelagoSpec) -> ShapeMap:
    canvas = Canvas(spec.grid)
    coast = Coast(spec)
    height = build_height(spec, canvas, coast)
    probe = ShapeModel(canvas, height)

    def height_at(e: float, n: float) -> float:
        X, Y = canvas.to_world(e, n)
        return float(probe.ground_height(np.array([X]), np.array([Y]))[0])

    links = plan_bridges(spec, coast, height_at)
    model = ShapeModel(canvas, height, tuple(b for _, _, b in links))
    required: dict[str, tuple[float, float]] = {}
    avoid = [(lg.center[0], lg.center[1], lg.radius_m + 4.0) for lg in spec.lagoons]
    if spec.volcano is not None:
        v = spec.volcano
        host = coast.by_name(v.island)
        ce, cn = host.center[0] + v.offset[0], host.center[1] + v.offset[1]
        required["cima"] = (ce, cn) if v.crater_radius_m == 0.0 else (ce + 0.5 * (v.crater_radius_m + v.top_radius_m), cn)
        avoid.append((ce, cn, v.base_radius_m))
    for island in spec.islands:
        if island.name not in spec.unreachable_ok:
            required[island.name] = plateau_point(island, coast, spec, tuple(avoid))
    start_island = coast.by_name(spec.start_island or spec.islands[0].name)
    start = required.get(start_island.name) or plateau_point(start_island, coast, spec, tuple(avoid))
    corridors = [b.inner_points(1.5) for _, _, b in links]
    unreachable = [coast.by_name(k).center for k in spec.unreachable_ok]
    params = {"generator": "archipelago", **asdict(spec)}
    return ShapeMap(spec.name, spec.seed, spec.mode, spec.description, canvas, model, start, None, required,
                    corridors, unreachable, corridor_width_m=min(3.0, spec.bridge_width_m),
                    corridor_slope_deg=spec.bridge_max_slope_deg, params=params)


# ── Especificaciones ─────────────────────────────────────────────────────────────
def philippines_rare(seed: int = 1001) -> ArchipelagoSpec:
    """I01 del catalogo: isla mayor «Tortuga» (elipse de 60 x 90 m, +4) con un volcan central de cima plana
    (base de 34 m de radio, cima de 6 m, +14: 19,6 grados), 3 medianas (radio 18 m, +3) y 3 pequeñas (radio 10 m,
    +2) alternas sobre un anillo de 85 m; 9 puentes de 4 m. Los 3 puentes entre islas del anillo miden unos
    60 m: el catalogo pedia 25-45 m, pero con islas de 18 y 10 m a 85 m del centro no caben mas cortos."""
    ring = 85.0
    islands = [IslandSpec("tortuga", (0.0, 0.0), (30.0, 45.0), 4.0, angle_deg=0.0, turtle=True)]
    for k, theta in enumerate((30.0, 150.0, 270.0)):
        rad = math.radians(theta)
        islands.append(IslandSpec(f"mediana_{k + 1}", (ring * math.cos(rad), ring * math.sin(rad)), (18.0, 18.0), 3.0))
    for k, theta in enumerate((90.0, 210.0, 330.0)):
        rad = math.radians(theta)
        islands.append(IslandSpec(f"pequena_{k + 1}", (ring * math.cos(rad), ring * math.sin(rad)), (10.0, 10.0), 2.0))
    return ArchipelagoSpec(
        "I01_filipinas_rara", seed, 3,
        "Filipinas rara («Archipiélago de las Mil Bolas», Todos contra Todos): una isla con forma de tortuga y un "
        "volcán de cima plana en el centro, rodeada de 3 islas medianas y 3 pequeñas unidas por 9 puentes "
        "naturales; caer al agua es la muerte.",
        tuple(islands), VolcanoSpec("tortuga", 34.0, 6.0, 14.0), bridge_count=9, bridge_max_m=70.0,
        start_island="tortuga", extra={"catalogo": "I01"})


def random_archipelago(seed: int, islands: int = 5, grid: int = 3, spread_m: float | None = None,
                       radius_range_m: tuple[float, float] = (10.0, 22.0), height_range_m: tuple[float, float] = (2.0, 5.0),
                       volcano: bool = True, lagoons: int = 1, bridge_count: int | None = None,
                       name: str | None = None) -> ArchipelagoSpec:
    """Archipielago cualquiera: una isla central mayor (con volcan si volcano) y islands - 1 alrededor, sin
    solaparse (muestreo con rechazo), lagunas interiores en las mayores."""
    rng = np.random.default_rng(seed)
    spread = spread_m or grid * 100.0 / 2.0 - 45.0
    big_r = radius_range_m[1] * 1.6
    specs = [IslandSpec("central", (0.0, 0.0), (big_r, big_r * rng.uniform(0.75, 1.0)), height_range_m[1],
                        angle_deg=float(rng.uniform(0, 180)))]
    tries = 0
    while len(specs) < islands and tries < 2000:
        tries += 1
        r = float(rng.uniform(*radius_range_m))
        dist = float(rng.uniform(big_r + r + 14.0, spread - r))
        theta = float(rng.uniform(0, 2 * math.pi))
        c = (dist * math.cos(theta), dist * math.sin(theta))
        if all(math.hypot(c[0] - s.center[0], c[1] - s.center[1]) > r + max(s.radii) + 14.0 for s in specs):
            specs.append(IslandSpec(f"isla_{len(specs)}", c, (r, r * float(rng.uniform(0.7, 1.0))),
                                    float(rng.uniform(*height_range_m)), angle_deg=float(rng.uniform(0, 180))))
    lagoon_specs = []
    for s in sorted(specs[1:], key=lambda s: -min(s.radii))[:lagoons]:
        lagoon_specs.append(LagoonSpec(s.center, 0.35 * min(s.radii)))
    vol = VolcanoSpec("central", 0.8 * big_r * 0.8, 4.0, height_range_m[1] + 9.0, crater_radius_m=3.0,
                      crater_depth_m=2.0) if volcano else None
    return ArchipelagoSpec(name or f"I_archipielago_{seed}", seed, grid, f"Archipiélago ficticio de {len(specs)} islas (semilla {seed}).",
                           tuple(specs), vol, tuple(lagoon_specs), bridge_count=bridge_count or len(specs) + 1,
                           start_island="central")
