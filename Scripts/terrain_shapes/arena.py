"""Arenas abstractas parametricas de Todos contra Todos (Docs/Catalogo-Mapas-2026-09-29.md §3): diana, donut,
espiral y tablero de mesetas. Parametros en metros de diseño (e, n) desde el centro del volumen, cotas sobre el
agua, semilla. Cada constructor devuelve un ShapeMap (modelo, inicio, puntos y pasarelas a validar).
"""

from __future__ import annotations

import math
from dataclasses import asdict, dataclass

import numpy as np

from terrain_vol.layout import WATER_M

from .bridges import NaturalBridge
from .canvas import SEABED_M, Canvas, plateau, sd_box, smoothstep
from .model import ShapeMap, ShapeModel

SHORE_M = 2.0            # orilla exterior de una arena (de la meseta al agua)
EDGE_M = 1.5             # los puntos de validacion quedan esto dentro de cada meseta


def _polar(e, n):
    return np.hypot(e, n), np.arctan2(n, e)


def _pt(r: float, deg: float) -> tuple[float, float]:
    return r * math.cos(math.radians(deg)), r * math.sin(math.radians(deg))


def _sea(canvas: Canvas, e, n, land_radius: float) -> np.ndarray:
    r = np.hypot(e, n)
    return SEABED_M - 1.5 * smoothstep(land_radius + 4.0, land_radius + 30.0, r)


# ── Diana ────────────────────────────────────────────────────────────────────────
@dataclass(frozen=True)
class DianaSpec:
    """Anillos concentricos (r_in, r_out, cota sobre el agua), el primero es el disco central. Entre anillos:
    rampas radiales (moat_m = 0: anillos pegados, escalon vertical y rampa sobre el anillo de fuera) o puentes
    naturales sobre un foso de agua de moat_m (moat_m > 0). stagger_deg > 0 gira las rampas de cada escalon un
    angulo al azar (semilla) de hasta ese valor."""
    name: str
    seed: int
    grid: int
    rings: tuple[tuple[float, float, float], ...]
    link_angles_deg: tuple[float, ...] = (0.0, 90.0, 180.0, 270.0)
    ramp_width_m: float = 6.0
    ramp_slope_deg: float = 15.0
    moat_m: float = 0.0
    bridge_width_m: float = 4.0
    stagger_deg: float = 0.0
    description: str = ""
    mode: str = "tct"


def diana_a01(seed: int = 3001) -> DianaSpec:
    """A01 del catalogo: 5 anillos (r 0-6, 6-18, 18-32, 32-46, 46-60 a +12, +9, +6, +4, +2) y 4 rampas radiales
    a 0, 90, 180 y 270 grados de 6 m de ancho y 15 grados."""
    return DianaSpec(
        "A01_diana", seed, 2, ((0.0, 6.0, 12.0), (6.0, 18.0, 9.0), (18.0, 32.0, 6.0), (32.0, 46.0, 4.0), (46.0, 60.0, 2.0)),
        description="Diana (Todos contra Todos): 5 anillos concéntricos de 120 m a +2, +4, +6, +9 y +12 m sobre el agua, "
                    "unidos por 4 rampas radiales de 6 m a 15°; el centro manda, caer al agua es la muerte.")


def _diana_rings(spec: DianaSpec) -> list[tuple[float, float, float]]:
    half = spec.moat_m / 2.0
    out = []
    for k, (r_in, r_out, h) in enumerate(spec.rings):
        out.append((r_in + (half if k > 0 else 0.0), r_out - (half if k < len(spec.rings) - 1 else 0.0), h))
    return out


def _link_angles(spec: DianaSpec, rng: np.random.Generator, count: int) -> list[list[float]]:
    return [[a + (float(rng.uniform(-spec.stagger_deg, spec.stagger_deg)) if spec.stagger_deg else 0.0)
             for a in spec.link_angles_deg] for _ in range(count)]


def build_diana(spec: DianaSpec) -> ShapeMap:
    canvas = Canvas(spec.grid)
    e, n = canvas.design_grid()
    r, theta = _polar(e, n)
    rings = _diana_rings(spec)
    outer = rings[-1][1]
    outer_sd = outer - r if spec.moat_m <= 0.0 else np.minimum(r - rings[-1][0], outer - r)
    height = np.maximum(_sea(canvas, e, n, outer), plateau(outer_sd, rings[-1][2], SHORE_M))
    for r_in, r_out, h in rings:
        height = np.where((r >= r_in) & (r < r_out) & (r < outer - SHORE_M), WATER_M + h, height)
    angles = _link_angles(spec, np.random.default_rng(spec.seed), len(rings) - 1)
    bridges, corridors = [], []
    tan = math.tan(math.radians(spec.ramp_slope_deg))
    for k in range(len(rings) - 1):
        (_, edge_hi, h_hi), (edge_lo, r_out_lo, h_lo) = rings[k], rings[k + 1]
        for deg in angles[k]:
            if spec.moat_m > 0.0:
                a, b = _pt(edge_hi - EDGE_M, deg), _pt(edge_lo + EDGE_M, deg)
                bridge = NaturalBridge(a, b, WATER_M + h_hi, WATER_M + h_lo, width_m=spec.bridge_width_m, rise_m=0.3)
                bridges.append(bridge)
                corridors.append(bridge.inner_points(1.5))
                continue
            length = (h_hi - h_lo) / tan
            if length > r_out_lo - edge_lo:
                raise ValueError(f"rampa de {length:.1f} m en un anillo de {r_out_lo - edge_lo:.1f} m: baja la pendiente")
            u = r * np.cos(theta - math.radians(deg)) - edge_lo
            lateral = np.abs(r * np.sin(theta - math.radians(deg)))
            ramp = (u >= -0.5) & (u <= length) & (lateral <= spec.ramp_width_m / 2.0) & (r >= edge_lo - 0.5)
            height = np.where(ramp, np.maximum(height, WATER_M + h_hi - np.clip(u, 0.0, None) * tan), height)
            corridors.append((_pt(edge_lo + length + EDGE_M, deg), _pt(edge_lo - EDGE_M, deg)))
    required = {f"anillo_{k + 1}": _pt(0.5 * (r_in + r_out), 45.0) if r_in > 0 else (0.0, 0.0)
                for k, (r_in, r_out, _) in enumerate(rings)}
    model = ShapeModel(canvas, height, tuple(bridges))
    start = required[f"anillo_{len(rings)}"]
    return ShapeMap(spec.name, spec.seed, spec.mode, spec.description, canvas, model, start, (0.0, 0.0), required,
                    corridors, corridor_width_m=3.0, corridor_slope_deg=20.0, params={"generator": "diana", **asdict(spec)})


# ── Donut ────────────────────────────────────────────────────────────────────────
@dataclass(frozen=True)
class DonutSpec:
    """Anillo de r_in a r_out a height_m con el hueco central de agua; bridge_angles_deg: puentes naturales que
    cruzan el hueco de lado a lado (pares opuestos se cruzan en el centro)."""
    name: str
    seed: int
    grid: int
    r_in: float = 25.0
    r_out: float = 65.0
    height_m: float = 4.0
    bridge_angles_deg: tuple[float, ...] = (0.0, 90.0)
    bridge_width_m: float = 3.0
    description: str = "Dónut (Todos contra Todos): anillo con el hueco central de agua y puentes que lo cruzan."
    mode: str = "tct"


def build_donut(spec: DonutSpec) -> ShapeMap:
    canvas = Canvas(spec.grid)
    e, n = canvas.design_grid()
    r, _ = _polar(e, n)
    sd = np.minimum(r - spec.r_in, spec.r_out - r)
    height = np.maximum(_sea(canvas, e, n, spec.r_out), plateau(sd, spec.height_m, SHORE_M))
    bridges, corridors = [], []
    for deg in spec.bridge_angles_deg:
        a, b = _pt(spec.r_in + SHORE_M + EDGE_M, deg), _pt(spec.r_in + SHORE_M + EDGE_M, deg + 180.0)
        bridge = NaturalBridge(a, b, WATER_M + spec.height_m, WATER_M + spec.height_m, width_m=spec.bridge_width_m,
                               rise_m=0.6)
        bridges.append(bridge)
        corridors.append(bridge.inner_points(1.5))
    mid = 0.5 * (spec.r_in + spec.r_out)
    required = {f"anillo_{int(d)}": _pt(mid, d + 45.0) for d in (0.0, 90.0, 180.0, 270.0)}
    model = ShapeModel(canvas, height, tuple(bridges))
    return ShapeMap(spec.name, spec.seed, spec.mode, spec.description, canvas, model, required["anillo_0"], None,
                    required, corridors, corridor_width_m=min(3.0, spec.bridge_width_m), corridor_slope_deg=20.0,
                    params={"generator": "donut", **asdict(spec)})


# ── Espiral ──────────────────────────────────────────────────────────────────────
@dataclass(frozen=True)
class SpiralSpec:
    """Rampa en espiral de `turns` vueltas de r_start (cota h_top, con una plataforma central) a r_end (cota
    h_bottom), paso pitch_m por vuelta y ancho width_m; entre vueltas, agua."""
    name: str
    seed: int
    grid: int
    r_start: float = 10.0
    turns: float = 2.5
    pitch_m: float = 16.0
    width_m: float = 12.0
    h_top: float = 14.0
    h_bottom: float = 0.8
    description: str = "Espiral (Todos contra Todos): rampa en espiral que sube hacia el centro entre fosos de agua."
    mode: str = "tct"


def build_spiral(spec: SpiralSpec) -> ShapeMap:
    canvas = Canvas(spec.grid)
    e, n = canvas.design_grid()
    r, theta = _polar(e, n)
    phi_max = spec.turns * 2.0 * math.pi
    r_end = spec.r_start + spec.pitch_m * spec.turns
    height = _sea(canvas, e, n, r_end + spec.width_m / 2.0)
    base = np.mod(theta, 2.0 * math.pi)
    for k in range(int(math.ceil(spec.turns)) + 1):
        phi = base + 2.0 * math.pi * k
        centre = spec.r_start + spec.pitch_m * phi / (2.0 * math.pi)
        sd = spec.width_m / 2.0 - np.abs(r - centre)
        sd = np.minimum(sd, np.where(phi <= phi_max, 1e3, -1e3))
        h = spec.h_top - (spec.h_top - spec.h_bottom) * np.clip(phi / phi_max, 0.0, 1.0)
        ribbon = SEABED_M + (WATER_M + h - SEABED_M) * smoothstep(-1.5, 0.8, sd)
        height = np.maximum(height, ribbon)
    hub = spec.r_start                                              # plataforma central: se solapa con el arranque
    height = np.maximum(height, plateau(hub - r, spec.h_top, 1.0))
    tail_phi = phi_max - 4.0 / r_end                               # 4 m antes del final de la cola
    tail = _pt(spec.r_start + spec.pitch_m * tail_phi / (2.0 * math.pi), math.degrees(tail_phi))
    required = {"centro": (0.0, 0.0), "cola": tail}
    model = ShapeModel(canvas, height)
    return ShapeMap(spec.name, spec.seed, spec.mode, spec.description, canvas, model, required["cola"], (0.0, 0.0),
                    required, [(required["cola"], required["centro"])], corridor_width_m=3.0, corridor_slope_deg=20.0,
                    params={"generator": "espiral", **asdict(spec)})


# ── Tablero de mesetas ───────────────────────────────────────────────────────────
@dataclass(frozen=True)
class BoardSpec:
    """Rejilla cells x cells de mesetas cuadradas de size_m separadas gap_m, con cotas de `levels` repartidas
    segun `counts` (semilla) sin que dos vecinas difieran mas de max_step_m; puentes naturales (rampas) entre
    vecinas: un arbol que las une todas mas extras al azar hasta `links`."""
    name: str
    seed: int
    grid: int
    cells: int = 5
    size_m: float = 20.0
    gap_m: float = 9.0
    levels: tuple[float, ...] = (3.0, 6.0, 9.0)
    counts: tuple[int, ...] = (9, 9, 7)
    max_step_m: float = 3.0
    links: int = 30
    bridge_width_m: float = 4.0
    description: str = "Tablero de mesetas (Todos contra Todos): rejilla de mesetas a tres cotas unidas por rampas."
    mode: str = "tct"


def board_heights(spec: BoardSpec) -> np.ndarray:
    """Cotas de la rejilla con el reparto de counts y vecinas a <= max_step_m (vuelta atras con semilla)."""
    if sum(spec.counts) != spec.cells * spec.cells:
        raise ValueError("counts no suma cells * cells")
    rng = np.random.default_rng(spec.seed)
    grid = np.full((spec.cells, spec.cells), np.nan)
    left = list(spec.counts)

    def place(k: int) -> bool:
        if k == spec.cells * spec.cells:
            return True
        i, j = divmod(k, spec.cells)
        for level in rng.permutation(len(spec.levels)):
            h = spec.levels[level]
            if left[level] == 0:
                continue
            if (i > 0 and abs(grid[i - 1, j] - h) > spec.max_step_m) or (j > 0 and abs(grid[i, j - 1] - h) > spec.max_step_m):
                continue
            grid[i, j], left[level] = h, left[level] - 1
            if place(k + 1):
                return True
            grid[i, j], left[level] = np.nan, left[level] + 1
        return False

    if not place(0):
        raise ValueError("no hay reparto de cotas que cumpla max_step_m")
    return grid


def build_board(spec: BoardSpec) -> ShapeMap:
    canvas = Canvas(spec.grid)
    e, n = canvas.design_grid()
    heights = board_heights(spec)
    step = spec.size_m + spec.gap_m
    offset = (spec.cells - 1) * step / 2.0
    centres = {(i, j): (j * step - offset, i * step - offset) for i in range(spec.cells) for j in range(spec.cells)}
    height = _sea(canvas, e, n, offset + spec.size_m)
    for (i, j), (ce, cn) in centres.items():
        sd = sd_box(e, n, ce, cn, spec.size_m / 2.0, spec.size_m / 2.0, corner=2.0)
        height = np.maximum(height, plateau(sd, float(heights[i, j]), 1.0))
    rng = np.random.default_rng(spec.seed + 1)
    pairs = [((i, j), (i + di, j + dj)) for (i, j) in centres for di, dj in ((0, 1), (1, 0))
             if (i + di, j + dj) in centres]
    order = rng.permutation(len(pairs))
    parent = {k: k for k in centres}

    def root(k):
        while parent[k] != k:
            parent[k] = parent[parent[k]]
            k = parent[k]
        return k

    tree, extra = [], []
    for idx in order:
        a, b = pairs[idx]
        if root(a) != root(b):
            parent[root(a)] = root(b)
            tree.append((a, b))
        else:
            extra.append((a, b))
    chosen = tree + extra[:max(0, spec.links - len(tree))]
    bridges, corridors = [], []
    half = spec.size_m / 2.0
    for a, b in chosen:
        (ae, an), (be, bn) = centres[a], centres[b]
        ue, un = (be - ae) / step, (bn - an) / step
        pa = (ae + ue * (half - 1.0), an + un * (half - 1.0))
        pb = (be - ue * (half - 1.0), bn - un * (half - 1.0))
        bridge = NaturalBridge(pa, pb, WATER_M + float(heights[a]), WATER_M + float(heights[b]),
                               width_m=spec.bridge_width_m, rise_m=0.0 if heights[a] != heights[b] else 0.3,
                               pier_m=2.0, overlap_m=1.0)
        bridges.append(bridge)
        corridors.append(bridge.inner_points(1.5))
    required = {f"meseta_{i}{j}": c for (i, j), c in centres.items()}
    model = ShapeModel(canvas, height, tuple(bridges))
    params = {"generator": "tablero", **asdict(spec), "heights": heights.tolist()}
    return ShapeMap(spec.name, spec.seed, spec.mode, spec.description, canvas, model, required["meseta_00"], None,
                    required, corridors, corridor_width_m=3.0, corridor_slope_deg=20.0, params=params)
