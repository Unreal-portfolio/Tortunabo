"""Pais entero miniaturizado (decision del director, 2026-09-29): MDE real y contorno real (costa y frontera como
borde, como E01), relieve con la escala vertical de E01 y adaptacion al modo:

  - Rally: calzada tallada de punta a punta (ruta de coste minimo por pendiente, perfil con pendiente acotada,
    corte y relleno con arcenes) y puentes naturales en los cruces reales entre islas (Seto, Kanmon, Seikan...);
  - Todos contra Todos: llanos en terrazas para pelear y puentes naturales entre islas proximas.

Exageracion (regla comun): la escala vertical de E01 (K_E01 = 40 m de juego por 3480 m reales), sin pasar nunca la
exageracion efectiva de E01 (24,7x: pendiente del juego / pendiente real) ni el techo del volumen. En un pais pequeño
(1 m = 1-2 km) sale entre 11x y 23x; en uno grande el tope de 24,7x manda.
"""

from __future__ import annotations

import dataclasses
import math
from dataclasses import dataclass, field

import numpy as np
from scipy import ndimage
from scipy.spatial import cKDTree

from terrain_shapes.bridges import NaturalBridge
from terrain_vol.density import smooth
from terrain_vol.layout import CELL_M, DEFAULT_Z_RANGE, MAP_MIN_M, WATER_M, ZRange

from .geomodel import Calibration, GeoModel, ReliefParams, calibrate
from .region import RASTER_PX_M, GeoRegion
from .sources import ALL_LAND

E01_K = 40.0 / 3480.0               # m de juego por m de cota real en E01 (PEAK_ABOVE_WATER_M / REAL_PEAK_M)
E01_EXAGGERATION = 24.7             # exageracion efectiva de E01 (K_E01 * 2148 m de suelo por m de juego)
CHUNKS = {"rally": 36, "tct": 12}   # trozos por mapa que caben en el presupuesto del catalogo (~30 k triangulos cada uno)
LAND_M = WATER_M + 0.3


@dataclass(frozen=True)
class Crossing:
    """Cruce real entre dos islas (puente o tunel): un puente natural del mapa entre a y b (lon, lat)."""
    name: str
    a: tuple[float, float]
    b: tuple[float, float]


@dataclass(frozen=True)
class CountryPreset:
    key: str                                      # nombre de la variante (index.json)
    country: str | None                           # ISO 3166 alfa-3 (Natural Earth); None = todas las tierras de bbox
    lang: str
    mode: str                                     # "rally" o "tct"
    description: str
    bbox: tuple[float, float, float, float] | None = None     # recorta el pais (territorios lejanos fuera)
    rotation_deg: float | None = None             # None = automatica (eje principal) si mejora la escala
    squash_e: float = 1.0
    max_chunks: int | None = None
    start: tuple[float, float] | None = None      # (lon, lat); None = extremo Sur del territorio principal
    end: tuple[float, float] | None = None        # None = extremo Norte
    crossings: tuple[Crossing, ...] = ()
    auto_bridges: bool | None = None              # None = si en TcT
    bridge_max_m: float = 30.0
    bridge_width_m: float | None = None           # None = 16 m en Rally (calzada de 14), 5 m en TcT
    min_island_m2: float = 80.0
    terrace_m: float | None = None                # None = 2,5 m en TcT, 0 en Rally
    road_width_m: float = 14.0
    road_shoulder_m: float = 10.0
    road_grade_deg: float = 9.0
    frame_m: float = 16.0
    lon_cut: float | None = None                  # meridiano de corte del mapa (mundo); bbox en [lon_cut, lon_cut + 360]
    seed: int = 20260929
    params: ReliefParams = field(default_factory=ReliefParams)

    @property
    def is_rally(self) -> bool:
        return self.mode == "rally"

    def bridges_auto(self) -> bool:
        return (not self.is_rally) if self.auto_bridges is None else self.auto_bridges

    def deck_width(self) -> float:
        return self.bridge_width_m or (16.0 if self.is_rally else 5.0)

    def terrace(self) -> float:
        return (0.0 if self.is_rally else 2.5) if self.terrace_m is None else self.terrace_m


# ── Region: giro y rectangulo de trozos ─────────────────────────────────────────
def main_axis_bearing(x: np.ndarray, y: np.ndarray) -> float:
    """Rumbo (grados, horario desde el Norte) del eje principal de los puntos proyectados (x Este, y Norte)."""
    pts = np.stack([x - x.mean(), y - y.mean()], axis=1)
    _, _, vt = np.linalg.svd(pts, full_matrices=False)
    ve, vn = vt[0]
    return float(np.degrees(np.arctan2(ve, vn)) % 180.0)


def plan_region(preset: CountryPreset, max_chunks: int | None = None) -> GeoRegion:
    """GeoRegion del pais: giro (el dado o el que mejor escala da entre 0 y el eje principal) y el rectangulo de
    trozos (columnas x filas <= max_chunks) con la escala mas fina."""
    if preset.country is None and preset.bbox is None:
        raise ValueError(f"{preset.key}: una region sin pais necesita bbox")
    country = preset.country or ALL_LAND
    base = GeoRegion(preset.key, bbox=preset.bbox, country=country, edge="border", frame_m=preset.frame_m,
                     min_island_m2=preset.min_island_m2, squash_e=preset.squash_e, lon_cut=preset.lon_cut)
    budget = max_chunks or preset.max_chunks or CHUNKS[preset.mode]
    probe = GeoRegion(preset.key, bbox=preset.bbox, country=country, edge="border", grid=1, frame_m=0.0,
                      scale=1.0, fit=(1, 1), lon_cut=preset.lon_cut)
    x, y = probe.game_projection().project(*base.extent_points())
    rotations = [preset.rotation_deg] if preset.rotation_deg is not None else [0.0, main_axis_bearing(x, y)]
    best: dict[float, tuple[float, int, int]] = {}
    for rot in rotations:
        b = math.radians(rot)
        xr = x * math.sin(b) + y * math.cos(b)
        yr = (x * math.cos(b) - y * math.sin(b)) * preset.squash_e
        ext_n, ext_e = float(xr.max() - xr.min()), float(yr.max() - yr.min())
        for rows in range(1, 17):
            for cols in range(1, 17):
                if rows * cols > budget:
                    continue
                scale = max(ext_n / (rows * CELL_M - 2 * preset.frame_m), ext_e / (cols * CELL_M - 2 * preset.frame_m))
                if rot not in best or scale * (1 + 0.001 * rows * cols) < best[rot][0]:
                    best[rot] = (scale * (1 + 0.001 * rows * cols), cols, rows)
    rot = rotations[0]
    if len(rotations) > 1 and best[rotations[1]][0] < 0.87 * best[0.0][0]:     # girar solo si gana mas de un 13 %
        rot = rotations[1]
    _, cols, rows = best[rot]
    return dataclasses.replace(base, rotation_deg=rot, fit=(cols, rows), grid=max(cols, rows))


def e01_calibration(region: GeoRegion, dem: np.ndarray, coverage: np.ndarray, params: ReliefParams) -> Calibration:
    ground = region.game_projection().ground_m_per_game_m()
    e = min(E01_EXAGGERATION, E01_K * ground)
    return calibrate(region, dem, coverage, 0.0, bounds=(0.0, e), params=params, exaggeration=e)


# ── Modelo ──────────────────────────────────────────────────────────────────────
def terraces(height: np.ndarray, step_m: float, base_m: float) -> np.ndarray:
    """Llanos en terrazas sobre la tierra: cada step_m de desnivel, un 75 % llano y un 25 % de talud."""
    rel = height - (WATER_M + base_m)
    t = rel / step_m
    floor = np.floor(t)
    stepped = (floor + smooth(0.75, 1.0, t - floor)) * step_m
    return np.where(rel > 0.0, WATER_M + base_m + stepped, height)


class CountryModel(GeoModel):
    trail_strength = 0.75

    def __init__(self, region: GeoRegion, dem: np.ndarray, coverage: np.ndarray, k: float, seed: int,
                 params: ReliefParams, z_range: ZRange = DEFAULT_Z_RANGE, terrace_m: float = 0.0):
        self.terrace_m = terrace_m
        self.bridges: tuple[NaturalBridge, ...] = ()
        super().__init__(region, dem, coverage, k, seed, params, z_range)
        self.road_mask = np.zeros(self.height.shape)

    def _build_height(self) -> np.ndarray:
        height = super()._build_height()
        return terraces(height, self.terrace_m, self.params.land_base_m) if self.terrace_m > 0.0 else height

    def trail_mask(self, x: np.ndarray, y: np.ndarray) -> np.ndarray:
        return self._sample(self.road_mask, x, y)

    def density(self, X: np.ndarray, Y: np.ndarray, Z: np.ndarray, fields=None) -> np.ndarray:
        out = self.ground_height(X, Y)[..., None] - Z[None, None, :]
        for bridge in self.bridges:                                   # puentes en (e, n) = (Y, X) de juego
            e0, e1, n0, n1 = bridge.bbox()
            if e1 < Y.min() or e0 > Y.max() or n1 < X.min() or n0 > X.max():
                continue
            out = np.maximum(out, bridge.density(Y, X, Z))
        return out


def raster_index(X: float, Y: float) -> tuple[int, int]:
    return int((X - MAP_MIN_M) / RASTER_PX_M), int((Y - MAP_MIN_M) / RASTER_PX_M)


def raster_point(i: int, j: int) -> tuple[float, float]:
    return MAP_MIN_M + (i + 0.5) * RASTER_PX_M, MAP_MIN_M + (j + 0.5) * RASTER_PX_M


# ── Puentes ─────────────────────────────────────────────────────────────────────
def _snap(land: np.ndarray, X: float, Y: float, radius_m: float = 25.0) -> tuple[int, int] | None:
    i, j = raster_index(X, Y)
    r = int(radius_m / RASTER_PX_M)
    i0, j0 = max(0, i - r), max(0, j - r)
    hits = np.argwhere(land[i0:i + r + 1, j0:j + r + 1])
    if len(hits) == 0:
        return None
    k = int(np.argmin(((hits + [i0, j0] - [i, j]) ** 2).sum(axis=1)))
    return int(hits[k][0] + i0), int(hits[k][1] + j0)


def _bridge(model: CountryModel, pa: tuple[int, int], pb: tuple[int, int], width: float, inland_m: float = 2.0) -> NaturalBridge:
    (xa, ya), (xb, yb) = raster_point(*pa), raster_point(*pb)
    length = max(math.hypot(xb - xa, yb - ya), 1e-6)
    ux, uy = (xb - xa) / length, (yb - ya) / length
    xa, ya, xb, yb = xa - ux * inland_m, ya - uy * inland_m, xb + ux * inland_m, yb + uy * inland_m
    za = max(float(model.ground_height(np.array([xa]), np.array([ya]))[0]), WATER_M + 0.8)
    zb = max(float(model.ground_height(np.array([xb]), np.array([yb]))[0]), WATER_M + 0.8)
    return NaturalBridge((ya, xa), (yb, xb), za, zb, width_m=width, rise_m=0.3, pier_m=3.0)


def plan_bridges(model: CountryModel, preset: CountryPreset) -> tuple[list[tuple[str, NaturalBridge]], list[tuple[float, float]]]:
    """Puentes de los cruces reales y, si auto, los mas cortos entre islas (arbol de expansion). Devuelve los puentes
    con nombre y las islas (X, Y) que quedan sin unir al territorio principal (marcadas como inalcanzables)."""
    land = model.height > LAND_M
    labels, count = ndimage.label(land)
    areas = ndimage.sum(land, labels, index=np.arange(1, count + 1)) * RASTER_PX_M ** 2
    big = {k + 1 for k in range(count) if areas[k] >= preset.min_island_m2}
    parent = {k: k for k in range(count + 1)}

    def root(k):
        while parent[k] != k:
            parent[k] = parent[parent[k]]
            k = parent[k]
        return k

    proj = model.region.game_projection()
    out: list[tuple[str, NaturalBridge]] = []
    for crossing in preset.crossings:
        (Xa, Ya), (Xb, Yb) = (tuple(float(v) for v in proj.to_game(*crossing.a)),
                              tuple(float(v) for v in proj.to_game(*crossing.b)))
        pa, pb = _snap(land, Xa, Ya), _snap(land, Xb, Yb)
        if pa is None or pb is None or root(labels[pa]) == root(labels[pb]):
            continue
        parent[root(labels[pa])] = root(labels[pb])
        out.append((crossing.name, _bridge(model, pa, pb, preset.deck_width())))
    if preset.bridges_auto():
        edges = ndimage.binary_erosion(land) ^ land
        trees = {}
        for k in big:
            pts = np.argwhere(edges & (labels == k))[::2]
            if len(pts):
                trees[k] = (cKDTree(pts), pts)
        cands = []
        keys = sorted(trees)
        max_px = preset.bridge_max_m / RASTER_PX_M
        for x, a in enumerate(keys):
            for b in keys[x + 1:]:
                dist, idx = trees[b][0].query(trees[a][1], distance_upper_bound=max_px)
                best = int(np.argmin(dist))
                if np.isfinite(dist[best]):
                    cands.append((float(dist[best]), a, b, tuple(trees[a][1][best]), tuple(trees[b][1][idx[best]])))
        for _, a, b, pa, pb in sorted(cands):
            if root(a) != root(b):
                parent[root(a)] = root(b)
                out.append((f"auto_{a}_{b}", _bridge(model, pa, pb, preset.deck_width())))
    main = root(int(np.argmax(areas)) + 1) if count else 0
    marked = []
    for k in big:
        if root(k) != main:
            depth = ndimage.distance_transform_edt(labels == k)
            i, j = np.unravel_index(int(np.argmax(depth)), depth.shape)
            marked.append(raster_point(int(i), int(j)))
    return out, marked


# ── Calzada (Rally) ─────────────────────────────────────────────────────────────
@dataclass
class Road:
    path: np.ndarray            # (n, 2) indices (i, j) de la rejilla de 1 m de global_top
    profile: np.ndarray         # cota (m) de la calzada en cada punto
    carve: np.ndarray | None = None     # False donde no se talla (dentro de un tunel)


def _grid_1m(model: CountryModel) -> tuple[np.ndarray, np.ndarray]:
    n = model.region.grid * int(CELL_M) + 1
    axis = MAP_MIN_M + np.arange(n, dtype=np.float64)
    X, Y = np.meshgrid(axis, axis, indexing="ij")
    return model.ground_height(X, Y), axis


def _deck_overlay(h: np.ndarray, bridges: list[NaturalBridge]) -> tuple[np.ndarray, np.ndarray]:
    deck = np.full(h.shape, np.nan)
    for bridge in bridges:
        for s in np.linspace(0.0, 1.0, int(bridge.length * 2) + 2):
            e = bridge.a[0] + (bridge.b[0] - bridge.a[0]) * s
            n = bridge.a[1] + (bridge.b[1] - bridge.a[1]) * s
            i, j = int(round(n - MAP_MIN_M)), int(round(e - MAP_MIN_M))
            r = int(bridge.width_m / 2.0 - 1.0)
            deck[max(0, i - r):i + r + 1, max(0, j - r):j + r + 1] = bridge.deck_top(s)
    return np.where(np.isnan(deck), h, deck), ~np.isnan(deck)


def lipschitz(values: np.ndarray, steps: np.ndarray, grade: float, passes: int = 4) -> np.ndarray:
    """Perfil con pendiente <= grade entre puntos (steps = distancia de cada punto al anterior): se recorta hacia
    delante y hacia atras, repetido."""
    out = values.astype(np.float64).copy()
    for _ in range(passes):
        for k in range(1, len(out)):
            limit = grade * steps[k]
            out[k] = min(max(out[k], out[k - 1] - limit), out[k - 1] + limit)
        for k in range(len(out) - 2, -1, -1):
            limit = grade * steps[k + 1]
            out[k] = min(max(out[k], out[k + 1] - limit), out[k + 1] + limit)
    return out


def smooth_path(path: np.ndarray, passable: np.ndarray) -> np.ndarray:
    """Suaviza el trazado del coste minimo (8 vecinos, en zigzag) y lo remuestrea cada metro; baja el suavizado si
    el trazado suave pisa agua."""
    for sigma in (6.0, 4.0, 2.0, 0.0):
        if sigma == 0.0:
            return path
        pts = np.stack([ndimage.gaussian_filter1d(path[:, k].astype(np.float64), sigma, mode="nearest") for k in (0, 1)], 1)
        arc = np.concatenate([[0.0], np.cumsum(np.hypot(*np.diff(pts, axis=0).T))])
        t = np.arange(0.0, arc[-1], 1.0)
        res = np.rint(np.stack([np.interp(t, arc, pts[:, 0]), np.interp(t, arc, pts[:, 1])], 1)).astype(int)
        keep = np.concatenate([[True], (np.diff(res, axis=0) != 0).any(axis=1)])
        res = np.vstack([path[:1], res[keep][1:], path[-1:]])
        if passable[res[:, 0], res[:, 1]].all():
            return res
    return path


def route_road(model: CountryModel, start_xy, end_xy, bridges: list[NaturalBridge], grade_deg: float) -> Road:
    from skimage.graph import MCP_Geometric
    h, _ = _grid_1m(model)
    h_route, on_deck = _deck_overlay(h, bridges)
    passable = (h_route > WATER_M + 0.2) | on_deck
    gy, gx = np.gradient(h_route)
    shore = ndimage.distance_transform_edt(passable)
    cost = 1.0 + 300.0 * (gx * gx + gy * gy) + 200.0 * (1.0 - smooth(4.0, 10.0, shore)) * ~on_deck   # lejos de la orilla
    cost[~passable] = -1.0
    s = (int(round(start_xy[0] - MAP_MIN_M)), int(round(start_xy[1] - MAP_MIN_M)))
    e = (int(round(end_xy[0] - MAP_MIN_M)), int(round(end_xy[1] - MAP_MIN_M)))
    mcp = MCP_Geometric(cost, fully_connected=True)
    costs, _ = mcp.find_costs([s], [e])
    if not np.isfinite(costs[e]):
        raise RuntimeError("no hay ruta a pie del inicio al final (falta un puente?)")
    path = smooth_path(np.array(mcp.traceback(e)), passable)
    raw = h_route[path[:, 0], path[:, 1]]
    steps = np.concatenate([[0.0], np.hypot(*np.diff(path, axis=0).T)])
    smoothed = ndimage.gaussian_filter1d(raw, 10.0, mode="nearest")
    profile = np.maximum(lipschitz(smoothed, steps, math.tan(math.radians(grade_deg))), WATER_M + 0.8)
    return Road(path, profile)


def bridge_zone(model: CountryModel, margin_m: float = 2.0) -> np.ndarray:
    """Pixeles del raster bajo el vano de los puentes (el 60 % central, con margin_m a los lados): ahi la calzada no
    rellena el agua; en los estribos si (escollera hasta el tablero)."""
    axis = MAP_MIN_M + (np.arange(model.height.shape[0]) + 0.5) * RASTER_PX_M
    X, Y = np.meshgrid(axis, axis, indexing="ij")
    zone = np.zeros(model.height.shape, dtype=bool)
    for b in model.bridges:
        (ya, xa), (yb, xb) = b.a, b.b
        vx, vy = xb - xa, yb - ya
        t = ((X - xa) * vx + (Y - ya) * vy) / max(vx * vx + vy * vy, 1e-9)
        tc = np.clip(t, 0.0, 1.0)
        zone |= (np.hypot(X - (xa + tc * vx), Y - (ya + tc * vy)) < b.width_m / 2.0 + margin_m) & (t > 0.2) & (t < 0.8)
    return zone


def carve_road(model: CountryModel, roads: list[Road], width_m: float, shoulder_m: float) -> None:
    """Corta y rellena el campo de alturas a la cota de las calzadas (arcenes fundidos). Donde la calzada toca el
    agua la rellena (escollera), salvo bajo los puentes."""
    px = RASTER_PX_M
    shape = model.height.shape
    marks = np.zeros(shape, dtype=bool)
    values = np.zeros(shape)
    for road in roads:
        carve = road.carve if road.carve is not None else np.ones(len(road.path), dtype=bool)
        for k in range(len(road.path) - 1):
            if not (carve[k] and carve[k + 1]):
                continue
            (i, j), (i2, j2) = road.path[k], road.path[k + 1]
            z, z2 = road.profile[k], road.profile[k + 1]
            for t in np.linspace(0.0, 1.0, 4, endpoint=False):
                a = int((i + (i2 - i) * t) / px)
                b = int((j + (j2 - j) * t) / px)
                if 0 <= a < shape[0] and 0 <= b < shape[1]:
                    marks[a, b] = True
                    values[a, b] = z + (z2 - z) * t
    dist, (ni, nj) = ndimage.distance_transform_edt(~marks, return_indices=True)
    dist = dist * px
    road_h = ndimage.gaussian_filter(values[ni, nj], 4.0)          # sin escalones entre tramos cercanos
    blend = 1.0 - smooth(width_m / 2.0, width_m / 2.0 + shoulder_m, dist)
    land = (model.height > WATER_M) | ((blend > 0.5) & ~bridge_zone(model))
    model.height = np.where(land, model.height + (road_h - model.height) * blend, model.height)
    model.road_mask = (1.0 - smooth(width_m / 2.0 - 1.0, width_m / 2.0 + 0.5, dist)) * land


def fit_bridges_to_road(bridges: list[NaturalBridge], road: Road) -> list[NaturalBridge]:
    """Los puentes que usa la calzada toman la cota de la calzada en sus extremos."""
    tree = cKDTree(road.path + MAP_MIN_M)
    out = []
    for bridge in bridges:
        da, ia = tree.query((bridge.a[1], bridge.a[0]))
        db, ib = tree.query((bridge.b[1], bridge.b[0]))
        if da < bridge.width_m and db < bridge.width_m:
            bridge = dataclasses.replace(bridge, z_a=float(road.profile[ia]), z_b=float(road.profile[ib]))
        out.append(bridge)
    return out


def snap_inland(model: CountryModel, xy: tuple[float, float], radius_m: float = 60.0) -> tuple[float, float]:
    """El punto de tierra (a 3 m o mas de la costa) mas cercano a xy."""
    land = model.height > WATER_M + 1.0
    inner = ndimage.binary_erosion(land, iterations=6)
    hit = _snap(inner if inner.any() else land, xy[0], xy[1], radius_m)
    return raster_point(*hit) if hit is not None else xy


def endpoint(model: CountryModel, north: bool) -> tuple[float, float]:
    """Extremo Sur (o Norte) del territorio principal, unos metros tierra adentro."""
    land = model.height > LAND_M
    labels, count = ndimage.label(land)
    main = labels == (int(np.argmax(ndimage.sum(land, labels, index=np.arange(1, count + 1)))) + 1)
    rows = np.nonzero(main.any(axis=1))[0]
    edge = rows.max() if north else rows.min()
    band = np.zeros_like(main)
    lo, hi = (edge - 60, edge + 1) if north else (edge, edge + 61)
    band[max(lo, 0):hi] = True
    depth = ndimage.distance_transform_edt(main) * (band & main)
    i, j = np.unravel_index(int(np.argmax(depth)), depth.shape)
    return raster_point(int(i), int(j))
