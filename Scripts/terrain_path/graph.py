"""Grafo del camino: principal de inicio a fin, lazos que salen y vuelven a su padre (tambien
de otro lazo) y cruces a distinto nivel. Todo en 2D (metros); la cota va en profile.py."""

from __future__ import annotations

import math
from dataclasses import dataclass, field

import numpy as np
from scipy.spatial import cKDTree

from .curves import longest_straight, resample
from .layout import MAP_MAX_M, MAP_MIN_M
from .style import PathStyle

STEP_M = 1.0
MAX_TURN_DEG_PER_M = 5.0          # 50 grados en 10 m como mucho
EDGE_MARGIN_M = 25.0
SELF_GAP_M = 70.0                 # dos puntos del mismo camino a menos de esto en arco pueden estar cerca


@dataclass
class PathLine:
    id: int
    parent: int | None            # None = camino principal
    points: np.ndarray            # (N, 2), cada STEP_M
    arc: np.ndarray
    s_out: float = 0.0            # arco en el padre donde sale
    s_back: float = 0.0           # arco en el padre donde vuelve

    @property
    def length(self) -> float:
        return float(self.arc[-1])

    def point_at(self, s: float) -> np.ndarray:
        return np.array([np.interp(s, self.arc, self.points[:, 0]), np.interp(s, self.arc, self.points[:, 1])])

    def tangent_at(self, s: float) -> np.ndarray:
        d = self.point_at(min(s + 2.0, self.length)) - self.point_at(max(s - 2.0, 0.0))
        return d / max(float(np.linalg.norm(d)), 1e-9)

    def normal_at(self, s: float) -> np.ndarray:
        t = self.tangent_at(s)
        return np.array([-t[1], t[0]])


@dataclass
class Crossing:
    upper: int                    # camino que pasa por arriba (puente natural)
    lower: int                    # camino que pasa por debajo (tunel)
    point: np.ndarray
    s_upper: float
    s_lower: float


@dataclass
class PathGraph:
    lines: list[PathLine]
    crossings: list[Crossing] = field(default_factory=list)

    @property
    def main(self) -> PathLine:
        return self.lines[0]

    def loops(self) -> list[PathLine]:
        return self.lines[1:]


def _inside(p: np.ndarray, margin: float) -> bool:
    return bool(MAP_MIN_M + margin <= p[0] <= MAP_MAX_M - margin and MAP_MIN_M + margin <= p[1] <= MAP_MAX_M - margin)


def steer_walk(rng: np.random.Generator, start, heading: float, waypoints: list[np.ndarray], radii: list[float],
               max_len: float, noise_deg: float, wave_m: tuple[float, float]) -> np.ndarray | None:
    """Avanza de 'start' hacia cada waypoint (llega al entrar en su radio) con giro acotado y un
    rumbo que serpentea (dos ondas de longitud distinta; se apaga a menos de 20 m del objetivo).
    Al llegar al ultimo se anade el punto exacto. None si sale del mapa o supera max_len."""
    p = np.asarray(start, dtype=float)
    pts = [p.copy()]
    h = float(heading)
    w1 = float(rng.uniform(*wave_m))
    w2 = w1 * float(rng.uniform(0.35, 0.5))
    ph = rng.uniform(0.0, 2.0 * math.pi, 2)
    limit = math.radians(MAX_TURN_DEG_PER_M) * STEP_M
    s, k = 0.0, 0
    while s < max_len:
        to = waypoints[k] - p
        dist = float(np.hypot(*to))
        if dist < radii[k]:
            k += 1
            if k == len(waypoints):
                pts.append(np.asarray(waypoints[-1], dtype=float))
                return np.array(pts)
            continue
        fade = min(1.0, dist / 20.0)
        wobble = math.radians(noise_deg) * (0.7 * math.sin(2 * math.pi * s / w1 + ph[0])
                                            + 0.3 * math.sin(2 * math.pi * s / w2 + ph[1])) * fade
        want = math.atan2(to[1], to[0]) + wobble
        dh = (want - h + math.pi) % (2.0 * math.pi) - math.pi
        h += max(-limit, min(limit, dh))
        p = p + STEP_M * np.array([math.cos(h), math.sin(h)])
        if not _inside(p, EDGE_MARGIN_M):
            return None
        pts.append(p.copy())
        s += STEP_M
    return None


def self_separated(points: np.ndarray, arc: np.ndarray, sep: float) -> bool:
    """Ningun par de puntos lejanos en arco (> SELF_GAP_M) queda a menos de 'sep'."""
    pairs = cKDTree(points).query_pairs(sep, output_type="ndarray")
    return not len(pairs) or bool(np.all(np.abs(arc[pairs[:, 0]] - arc[pairs[:, 1]]) <= SELF_GAP_M))


def trace_main(rng: np.random.Generator, style: PathStyle) -> PathLine:
    """Principal: del borde sur (X minima) al norte (el mar), zigzagueando entre bandas este y
    oeste por 4-5 puntos de paso, con longitud en style.main_length_m."""
    for _ in range(400):
        start = np.array([MAP_MIN_M + 30.0, float(rng.uniform(MAP_MIN_M + 110.0, MAP_MAX_M - 110.0))])
        side = float(rng.choice([-1.0, 1.0]))
        n_way = int(rng.integers(4, 6))
        ways = []
        for k, x in enumerate(np.linspace(MAP_MIN_M + 95.0, MAP_MAX_M - 95.0, n_way)):
            band = float(rng.uniform(90.0, 150.0))
            y = MAP_MIN_M + band if side * (-1.0) ** k < 0 else MAP_MAX_M - band
            ways.append(np.array([x + float(rng.uniform(-15.0, 15.0)), y]))
        end = np.array([MAP_MAX_M - 35.0, float(rng.uniform(MAP_MIN_M + 120.0, MAP_MAX_M - 120.0))])
        raw = steer_walk(rng, start, 0.0, ways + [end], [25.0] * n_way + [3.0],
                         style.main_length_m[1] + 50.0, 55.0, (55.0, 85.0))
        if raw is None:
            continue
        pts, arc = resample(raw, STEP_M)
        if not (style.main_length_m[0] <= arc[-1] <= style.main_length_m[1]):
            continue
        if longest_straight(pts) > 25.0 or not self_separated(pts, arc, style.path_separation_m):
            continue
        return PathLine(0, None, pts, arc)
    raise RuntimeError("no se encontro un camino principal valido en 400 intentos")
