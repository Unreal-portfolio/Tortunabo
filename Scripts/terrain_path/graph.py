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
    kind: str = "bridge"          # "bridge" (puente fino) o "tunnel" (cerro con tunel)


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
        # Llega al mar de frente (hacia el norte): un punto previo 30 m al sur del final.
        raw = steer_walk(rng, start, 0.0, ways + [end - np.array([30.0, 0.0]), end], [25.0] * n_way + [6.0, 3.0],
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


JOIN_ARC_M = 16.0                 # tramo junto a una union en el que el lazo aun esta pegado al padre


def _required_gap(arc_i: np.ndarray, length: float, sep: float, anchors: list[float]) -> np.ndarray:
    """Separacion exigida a cada punto del lazo: 'sep' lejos de uniones y cruces; cerca, crece
    con la distancia en arco (el lazo se va apartando del padre en V, no de golpe)."""
    gap = np.minimum(arc_i, length - arc_i)
    for a in anchors:
        gap = np.minimum(gap, np.abs(arc_i - a))
    return np.minimum(sep, 0.45 * gap)


def _clear_of_others(line: PathLine, graph: PathGraph, style: PathStyle, cross_s: float | None) -> bool:
    anchors = [cross_s] if cross_s is not None else []
    need = _required_gap(line.arc, line.length, style.path_separation_m, anchors)
    for other in graph.lines:
        d, _ = cKDTree(other.points).query(line.points)
        if np.any(d < need):
            return False
    return True


def _find_crossing(line: PathLine, parent: PathLine) -> Crossing | None:
    d, k = cKDTree(parent.points).query(line.points)
    inner = (line.arc > JOIN_ARC_M) & (line.arc < line.length - JOIN_ARC_M)
    hits = np.nonzero((d < 1.5) & inner)[0]
    if not len(hits) or np.ptp(line.arc[hits]) > 6.0:          # ninguno, o mas de un cruce
        return None
    i = int(hits[np.argmin(d[hits])])
    j = int(k[i])
    tl = line.tangent_at(line.arc[i])
    tp = parent.tangent_at(parent.arc[j])
    if abs(float(np.dot(tl, tp))) > math.cos(math.radians(40.0)):
        return None
    if line.arc[i] < 75.0 or line.length - line.arc[i] < 75.0:
        return None
    return Crossing(line.id, parent.id, line.points[i].copy(), float(line.arc[i]), float(parent.arc[j]))


def make_loop(rng: np.random.Generator, graph: PathGraph, parent_id: int, style: PathStyle, cross: bool,
              line_id: int) -> tuple[PathLine, Crossing | None] | None:
    """Un lazo de 'parent_id': sale en s_out hacia un lado, da la vuelta (a veces retrocede antes)
    y vuelve en s_back. Si 'cross', pasa al otro lado del padre a mitad de camino (cruce)."""
    parent = graph.lines[parent_id]
    nested = parent.parent is not None
    lo, hi = style.loop_span_m if not nested else (30.0, min(70.0, parent.length - 30.0))
    if cross:
        lo = max(lo, 90.0)
    if hi <= lo:
        return None
    margin_lo, margin_hi = (40.0, 70.0) if parent_id == 0 else (12.0, 12.0)
    span = float(rng.uniform(lo, hi))
    if parent.length - margin_lo - margin_hi - span <= 0.0:
        return None
    s_out = float(rng.uniform(margin_lo, parent.length - margin_hi - span))
    s_back = s_out + span
    side = float(rng.choice([-1.0, 1.0]))
    reach = float(rng.uniform(*style.loop_reach_m))
    ways: list[np.ndarray] = []
    radii: list[float] = []
    if not cross and rng.random() < style.backtrack_chance:
        s_b = max(0.0, s_out - float(rng.uniform(10.0, 35.0)))
        ways.append(parent.point_at(s_b) + parent.normal_at(s_b) * side * reach)
        radii.append(12.0)
    if cross:
        s_m1 = s_out + span * float(rng.uniform(0.2, 0.3))
        s_x = s_out + span * 0.5
        s_m2 = s_out + span * float(rng.uniform(0.7, 0.8))
        x_pt, n_x = parent.point_at(s_x), parent.normal_at(s_x)
        ways += [parent.point_at(s_m1) + parent.normal_at(s_m1) * side * reach,
                 x_pt + n_x * side * 18.0, x_pt - n_x * side * 18.0,
                 parent.point_at(s_m2) - parent.normal_at(s_m2) * side * reach]
        radii += [12.0, 6.0, 6.0, 12.0]
        side_back = -side
    else:
        s_m = s_out + span * 0.5
        ways.append(parent.point_at(s_m) + parent.normal_at(s_m) * side * reach)
        radii.append(12.0)
        side_back = side
    b_pt = parent.point_at(s_back)
    # Aproximacion a la vuelta: desde fuera y algo por detras (el radio de giro minimo es de
    # ~11,5 m: un punto previo pegado a la union obliga a dar vueltas sin llegar nunca).
    ways += [b_pt + parent.normal_at(s_back) * side_back * 20.0 - parent.tangent_at(s_back) * 14.0, b_pt]
    radii += [6.0, 4.0]
    t_a, n_a = parent.tangent_at(s_out), parent.normal_at(s_out)
    lean = t_a * math.cos(math.radians(45.0)) + n_a * side * math.sin(math.radians(45.0))
    raw = steer_walk(rng, parent.point_at(s_out), math.atan2(lean[1], lean[0]), ways, radii,
                     span * 4.0 + 200.0, 40.0, (35.0, 60.0))
    if raw is None:
        return None
    pts, arc = resample(raw, STEP_M)
    line = PathLine(line_id, parent_id, pts, arc, s_out, s_back)
    if longest_straight(pts) > 25.0 or not self_separated(pts, arc, style.path_separation_m):
        return None
    crossing = _find_crossing(line, parent) if cross else None
    if cross and crossing is None:
        return None
    if not _clear_of_others(line, graph, style, crossing.s_upper if crossing else None):
        return None
    return line, crossing


def build_graph(rng: np.random.Generator, style: PathStyle) -> PathGraph:
    """Principal y style.loops lazos: los ultimos style.nested_loops cuelgan de un lazo largo
    (>= 110 m); los primeros n del principal lo cruzan, con n al azar en el rango style.crossings. Un lazo que no encaja en
    120 intentos se omite (y un cruce que no encaja en 80 se intenta como lazo normal)."""
    graph = PathGraph([trace_main(rng, style)])
    n_cross = int(rng.integers(style.crossings[0], style.crossings[1] + 1))
    for k in range(style.loops):
        nested = k >= style.loops - style.nested_loops
        cross = (not nested) and k < n_cross
        for attempt in range(120):
            if nested:
                parents = [line.id for line in graph.lines[1:] if line.length >= 110.0]
                if not parents:
                    break
                parent_id = int(rng.choice(parents))
            else:
                parent_id = 0
            result = make_loop(rng, graph, parent_id, style, cross and attempt < 80, len(graph.lines))
            if result is not None:
                line, crossing = result
                graph.lines.append(line)
                if crossing is not None:
                    graph.crossings.append(crossing)
                break
    return graph
