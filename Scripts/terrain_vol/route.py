"""Ruta principal del mapa y reparto en zonas a lo largo de ella.

La ruta es una curva suave de inicio a fin: el camino de celdas (generate_path) solo da el
esqueleto; sus centros se desplazan y se unen con Catmull-Rom. Las zonas se reparten por
longitud de arco y su peso en cualquier punto sale del arco de la ruta mas cercana deformado
con ruido: las fronteras entre zonas son curvas y no dependen de la cuadricula.
"""

from __future__ import annotations

from dataclasses import dataclass

import numpy as np
from scipy.spatial import cKDTree

from terrain_gen.preset import generate_path

from .layout import CELL_M, GRID, MAP_MAX_M, MAP_MIN_M, cell_center
from .noise import Fbm2D

# (zona, fraccion de la ruta, alcance lateral en m: hasta donde llega la zona antes del muro)
ZONES: tuple[tuple[str, float, float], ...] = (
    ("cliffs", 0.15, 70.0),
    ("canyon", 0.17, 40.0),
    ("dunes", 0.18, 60.0),
    ("lake", 0.17, 90.0),
    ("algae", 0.20, 75.0),
    ("sand_end", 0.13, 45.0),
)
ZONE_NAMES = tuple(z[0] for z in ZONES)
ROUTE_STEP_M = 1.0
JITTER_M = 22.0                 # desplazamiento de los puntos de control respecto al centro de celda
EDGE_MARGIN_M = 30.0            # la ruta no se acerca mas al borde del mapa
MIN_SELF_GAP_M = 45.0           # dos tramos de la ruta lejanos en arco no se acercan mas que esto
ZONE_BLEND_M = 18.0             # media anchura del fundido entre zonas, en arco
ZONE_WARP_M = 40.0              # deformacion del arco que curva las fronteras
ZONE_SKEW = 0.8                 # la frontera se tuerce con la distancia lateral a la ruta
ZONE_SOFT_M = 22.0              # vecino mas cercano suave: sin fronteras rectas entre dos tramos
ZONE_QUERY_WARP_M = 25.0        # deformacion del punto de consulta de las zonas
ZONE_SAMPLE_M = 4.0             # separacion de los puntos de la ruta que votan la zona


@dataclass
class Route:
    points: np.ndarray          # (N, 2) en metros, cada ROUTE_STEP_M
    arc: np.ndarray             # (N,) longitud de arco acumulada
    cells: list[tuple[int, int]]

    @property
    def length(self) -> float:
        return float(self.arc[-1])

    def zone_range(self, name: str) -> tuple[float, float]:
        start = 0.0
        for zone, share, _ in ZONES:
            end = start + share * self.length
            if zone == name:
                return start, end
            start = end
        raise KeyError(name)

    def point_at(self, s: float) -> np.ndarray:
        k = int(np.clip(np.searchsorted(self.arc, s), 0, len(self.arc) - 1))
        return self.points[k]

    def direction_at(self, s: float) -> np.ndarray:
        k = int(np.clip(np.searchsorted(self.arc, s), 1, len(self.arc) - 1))
        d = self.points[k] - self.points[k - 1]
        return d / max(float(np.linalg.norm(d)), 1e-9)


def catmull_rom(control: np.ndarray, samples_per_segment: int = 40) -> np.ndarray:
    """Curva Catmull-Rom (centripeta aproximada con parametro uniforme) por los puntos de control."""
    padded = np.vstack([control[0], control, control[-1]])
    out = []
    t = np.linspace(0.0, 1.0, samples_per_segment, endpoint=False)[:, None]
    for k in range(1, len(padded) - 2):
        p0, p1, p2, p3 = padded[k - 1], padded[k], padded[k + 1], padded[k + 2]
        out.append(0.5 * ((2 * p1) + (-p0 + p2) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t ** 2
                          + (-p0 + 3 * p1 - 3 * p2 + p3) * t ** 3))
    out.append(control[-1][None, :])
    return np.vstack(out)


def resample(points: np.ndarray, step: float) -> tuple[np.ndarray, np.ndarray]:
    seg = np.linalg.norm(np.diff(points, axis=0), axis=1)
    arc = np.concatenate(([0.0], np.cumsum(seg)))
    s = np.arange(0.0, arc[-1], step)
    x = np.interp(s, arc, points[:, 0])
    y = np.interp(s, arc, points[:, 1])
    return np.stack([x, y], axis=1), s


def self_gap(points: np.ndarray, arc: np.ndarray, min_arc_apart: float = 150.0) -> float:
    """Distancia minima entre dos puntos de la ruta separados mas de min_arc_apart en arco."""
    tree = cKDTree(points)
    best = np.inf
    for k in range(0, len(points), 5):
        for j in tree.query_ball_point(points[k], MIN_SELF_GAP_M * 2):
            if abs(arc[j] - arc[k]) > min_arc_apart:
                best = min(best, float(np.linalg.norm(points[j] - points[k])))
    return best


def build_route(rng: np.random.Generator, min_cells: int = 12, max_cells: int = 16, attempts: int = 60) -> Route:
    lo, hi = MAP_MIN_M + EDGE_MARGIN_M, MAP_MAX_M - EDGE_MARGIN_M
    for _ in range(attempts):
        cells = generate_path(rng, GRID, min_cells, max_cells)
        control = np.array([cell_center(c, r) for c, r in cells], dtype=np.float64)
        control[1:-1] += rng.uniform(-JITTER_M, JITTER_M, (len(control) - 2, 2))
        # Inicio y final dentro de su celda, algo desplazados del centro.
        control[0] += rng.uniform(-0.2 * CELL_M, 0.2 * CELL_M, 2)
        control[-1] += rng.uniform(-0.2 * CELL_M, 0.2 * CELL_M, 2)
        control = np.clip(control, lo, hi)
        points, arc = resample(catmull_rom(control), ROUTE_STEP_M)
        if points[:, 0].min() < lo - 1 or points[:, 0].max() > hi + 1 or points[:, 1].min() < lo - 1 \
                or points[:, 1].max() > hi + 1:
            continue
        if self_gap(points, arc) >= MIN_SELF_GAP_M:
            return Route(points, arc, cells)
    raise RuntimeError("sin ruta valida para esta semilla")


class ZoneField:
    """Consultas de la ruta en puntos arbitrarios: distancia, arco mas cercano y pesos de zona."""

    def __init__(self, rng: np.random.Generator, route: Route):
        self.route = route
        self.tree = cKDTree(route.points)
        self.warp = Fbm2D(rng, 110.0, 2)
        self.skew = Fbm2D(rng, 150.0, 2)
        self.reach_noise = Fbm2D(rng, 70.0, 3)
        self.query_warp = (Fbm2D(rng, 90.0, 2), Fbm2D(rng, 90.0, 2))
        stride = max(1, int(round(ZONE_SAMPLE_M / ROUTE_STEP_M)))
        self.vote_points = route.points[::stride]
        self.vote_arc = route.arc[::stride]
        self.vote_tree = cKDTree(self.vote_points)
        bounds, start = [], 0.0
        for _, share, _ in ZONES:
            end = start + share * route.length
            bounds.append((start, end))
            start = end
        self.bounds = bounds

    def nearest(self, x, y):
        """(distancia a la ruta, arco del punto de la ruta mas cercano), con la forma de x."""
        pts = np.stack([np.ravel(x), np.ravel(y)], axis=1)
        d, k = self.tree.query(pts)
        shape = np.shape(x)
        return d.reshape(shape), self.route.arc[k].reshape(shape)

    def _zone_of_arc(self, s) -> dict[str, np.ndarray]:
        raw = {}
        last = len(ZONES) - 1
        for k, (name, _, _) in enumerate(ZONES):
            a, b = self.bounds[k]
            rise = 1.0 if k == 0 else _smooth(a - ZONE_BLEND_M, a + ZONE_BLEND_M, s)
            fall = 1.0 if k == last else 1.0 - _smooth(b - ZONE_BLEND_M, b + ZONE_BLEND_M, s)
            raw[name] = rise * fall
        return raw

    def weights(self, x, y, s_near=None, d_near=None) -> dict[str, np.ndarray]:
        """Peso de cada zona (suman 1). Cada punto de la ruta cercano vota la zona de su arco
        (deformado con ruido y torcido con la distancia lateral), pesado por lo cerca que esta
        en relacion con el mas cercano: donde dos tramos de la ruta se acercan, la frontera no
        es la bisectriz recta entre ellos sino una mezcla ondulada."""
        x = np.asarray(x, dtype=np.float64)
        y = np.asarray(y, dtype=np.float64)
        qx = x + ZONE_QUERY_WARP_M * self.query_warp[0](x, y)
        qy = y + ZONE_QUERY_WARP_M * self.query_warp[1](x, y)
        pts = np.stack([qx.ravel(), qy.ravel()], axis=1)
        dist, idx = self.vote_tree.query(pts, k=24)
        rel = dist - dist[:, :1]
        kernel = np.exp(-0.5 * (rel / ZONE_SOFT_M) ** 2)
        warp = (ZONE_WARP_M * self.warp(qx, qy)).ravel()[:, None]
        skew = (ZONE_SKEW * self.skew(qx, qy)).ravel()[:, None]
        arc = self.vote_arc[idx] + warp + skew * dist
        votes = self._zone_of_arc(arc)
        norm = np.maximum(kernel.sum(axis=1), 1e-9)
        raw = {name: ((v * kernel).sum(axis=1) / norm).reshape(x.shape) for name, v in votes.items()}
        total = np.maximum(sum(raw.values()), 1e-9)
        return {name: w / total for name, w in raw.items()}

    def reach(self, weights: dict[str, np.ndarray], x, y) -> np.ndarray:
        """Alcance lateral ponderado, con ruido: mas alla, muro que cierra el mapa. La silueta
        de cada zona es irregular (entrantes y salientes), no un pasillo paralelo a la ruta."""
        base = np.sum([weights[name] * r for name, _, r in ZONES], axis=0)
        return base * (0.65 + 0.7 * self.reach_noise.unit(x, y))


def _smooth(e0: float, e1: float, x):
    t = np.clip((x - e0) / (e1 - e0), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)
