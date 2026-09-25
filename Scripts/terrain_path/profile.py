"""Cota, semiancho, bioma y tramos de tunel de cada camino del grafo.

Cota: nudos cada 15-60 m con valores del rango de su bioma, unidos en linea recta y recortados
a la pendiente maxima (rampas lineales), con las rodillas suavizadas. Los lazos empalman con la
cota de su padre en sus dos uniones; en un cruce, el camino de arriba queda cross_clearance_m
por encima del de abajo (que pasa en tunel)."""

from __future__ import annotations

from dataclasses import dataclass, replace

import numpy as np
from scipy import ndimage
from scipy.spatial import cKDTree

from .curves import knot_noise
from .graph import Crossing, PathGraph, PathLine, build_graph
from .layout import WATER_M
from .style import PathStyle

BIOMES = ("cliffs", "water", "dunes", "beach")
BIOME_LEVEL_M = {0: (1.0, 8.0), 1: (WATER_M + 0.7, WATER_M + 0.7), 2: (0.0, 4.0), 3: (0.5, 1.5)}
EXCLUDE_EXTRA_M = 12.0
PIN_FLAT_M = 8.0                 # tramo llano del camino a cada lado del punto de cruce


class Infeasible(Exception):
    """Un cruce no cabe con la pendiente maxima: hay que rehacer el grafo."""


@dataclass
class LineProfile:
    z: np.ndarray
    half_width: np.ndarray
    biome: np.ndarray
    tunnel: np.ndarray


@dataclass
class PathPlan:
    graph: PathGraph
    profiles: dict[int, LineProfile]
    crossings: list[Crossing]
    hill_tunnels: list[tuple[int, float, float]]


def _smooth01(e0: float, e1: float, x):
    t = np.clip((np.asarray(x, dtype=float) - e0) / (e1 - e0), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def _knot_levels(rng: np.random.Generator, arc: np.ndarray, biome: np.ndarray) -> np.ndarray:
    knots, values = [0.0], []
    while knots[-1] < arc[-1]:
        knots.append(knots[-1] + float(rng.uniform(15.0, 60.0)))
    for s in knots:
        lo, hi = BIOME_LEVEL_M[int(biome[min(int(np.searchsorted(arc, s)), len(arc) - 1)])]
        values.append(float(rng.uniform(lo, hi)))
    return np.interp(arc, knots, values)


def _grades(rng: np.random.Generator, arc: np.ndarray, style: PathStyle) -> np.ndarray:
    grade = np.full(len(arc), style.max_grade)
    s = 0.0
    while s < arc[-1]:
        length = float(rng.uniform(15.0, 60.0))
        if rng.random() < style.steep_chance:
            grade[(arc >= s) & (arc < s + min(length, 20.0))] = style.steep_grade
        s += length
    return grade


def clamp_grade(z: np.ndarray, arc: np.ndarray, grade: np.ndarray, anchors: set[int] = frozenset()) -> np.ndarray:
    """Recorta z para que ninguna pareja de vecinos supere su pendiente; los anclajes no se mueven."""
    z = z.copy()
    ds = np.diff(arc)
    for _ in range(4):
        for i in range(1, len(z)):
            if i not in anchors:
                z[i] = min(max(z[i], z[i - 1] - grade[i] * ds[i - 1]), z[i - 1] + grade[i] * ds[i - 1])
        for i in range(len(z) - 2, -1, -1):
            if i not in anchors:
                z[i] = min(max(z[i], z[i + 1] - grade[i] * ds[i]), z[i + 1] + grade[i] * ds[i])
    return z


def _ramps(z: np.ndarray, arc: np.ndarray, grade: np.ndarray, anchors: set[int]) -> np.ndarray:
    """Rampas lineales con las rodillas suavizadas (sin pasar de la pendiente ni mover anclajes)."""
    z = clamp_grade(z, arc, grade, anchors)
    smooth = ndimage.gaussian_filter1d(z, 2.0, mode="nearest")
    for i in anchors:
        smooth[i] = z[i]
    return clamp_grade(smooth, arc, grade, anchors)


def _main_biomes(arc: np.ndarray, shares) -> np.ndarray:
    edges = np.cumsum(shares)[:-1]
    return np.searchsorted(edges, arc / arc[-1], side="right").astype(int)


def _widths(rng: np.random.Generator, arc: np.ndarray, biome: np.ndarray, style: PathStyle) -> np.ndarray:
    lo, mode, hi = style.width_m
    w = knot_noise(rng, arc, (20.0, 60.0), lo, hi, mode=mode)
    river = knot_noise(rng, arc, (20.0, 60.0), *style.river_half_width_m)
    water = ndimage.gaussian_filter1d((biome == 1).astype(float), 5.0, mode="nearest")
    return w * (1.0 - water) + river * water


def main_profile(rng: np.random.Generator, line: PathLine, style: PathStyle) -> LineProfile:
    biome = _main_biomes(line.arc, style.biome_shares)
    target = _knot_levels(rng, line.arc, biome)
    end = line.length
    beach = _smooth01(end - 60.0, end, line.arc)
    target = target * (1.0 - beach) + (WATER_M + 0.3) * beach
    z = _ramps(target, line.arc, _grades(rng, line.arc, style), set())
    w = _widths(rng, line.arc, biome, style)
    w = np.maximum(w, 7.0 * (1.0 - _smooth01(8.0, 16.0, line.arc)))          # salida: ensanche
    w = w + 38.0 * _smooth01(end - 55.0, end, line.arc)                       # final: abanico a la playa
    return LineProfile(z, w, biome, np.zeros(len(z), dtype=bool))


def loop_profile(rng: np.random.Generator, line: PathLine, graph: PathGraph, profiles: dict[int, LineProfile],
                 style: PathStyle, pin: tuple[float, float] | None) -> LineProfile:
    """Lazo: bioma del principal mas cercano, cota libre empalmada con el padre en s_out y s_back.
    pin = (arco en el lazo, cota) fija un tramo llano de +-PIN_FLAT_M (el del cruce)."""
    _, k = cKDTree(graph.main.points).query(line.points)
    biome = profiles[0].biome[k]
    parent, pp = graph.lines[line.parent], profiles[line.parent]
    z_a = float(np.interp(line.s_out, parent.arc, pp.z))
    z_b = float(np.interp(line.s_back, parent.arc, pp.z))
    free = _knot_levels(rng, line.arc, biome)
    detail = free - np.interp(line.arc, [0.0, line.length], [free[0], free[-1]])
    if pin is None:
        if abs(z_b - z_a) > style.max_grade * line.length:
            raise Infeasible(f"el lazo {line.id} une dos cotas que no caben en pendiente")
        z = np.interp(line.arc, [0.0, line.length], [z_a, z_b]) + detail
        anchors = {0, len(z) - 1}
    else:
        s_x, z_x = pin
        before, after = s_x - PIN_FLAT_M, line.length - s_x - PIN_FLAT_M
        if abs(z_x - z_a) > style.steep_grade * before or abs(z_x - z_b) > style.steep_grade * after:
            raise Infeasible(f"el cruce del lazo {line.id} no cabe en pendiente")
        z = np.interp(line.arc, [0.0, s_x - PIN_FLAT_M, s_x + PIN_FLAT_M, line.length], [z_a, z_x, z_x, z_b]) \
            + 0.5 * detail
        flat = np.abs(line.arc - s_x) <= PIN_FLAT_M
        z[flat] = z_x
        anchors = {0, len(z) - 1} | set(np.nonzero(flat)[0].tolist())
    z[0], z[-1] = z_a, z_b
    # Las rampas de un cruce pueden ser las empinadas (hay que salvar cross_clearance_m).
    grade = style.max_grade if pin is None else style.steep_grade
    z = _ramps(z, line.arc, np.full(len(z), grade), anchors)
    return LineProfile(z, _widths(rng, line.arc, biome, style), biome, np.zeros(len(z), dtype=bool))


def _mark(profile: LineProfile, line: PathLine, s0: float, s1: float) -> LineProfile:
    return replace(profile, tunnel=profile.tunnel | ((line.arc >= s0) & (line.arc <= s1)))


def _hill_tunnels(rng: np.random.Generator, graph: PathGraph, profile: LineProfile, crossings: list[Crossing],
                  style: PathStyle) -> list[tuple[int, float, float]]:
    main = graph.main
    joins = [s for loop in graph.loops() if loop.parent == 0 for s in (loop.s_out, loop.s_back)]
    joins += [c.s_upper if c.upper == 0 else c.s_lower for c in crossings if 0 in (c.upper, c.lower)]
    cliffs = main.arc[profile.biome == 0]
    out: list[tuple[int, float, float]] = []
    for _ in range(200):
        if len(out) >= style.hill_tunnels or len(cliffs) < 2:
            break
        length = float(rng.uniform(25.0, 45.0))
        s0 = float(rng.uniform(max(60.0, cliffs[0]), max(60.0, cliffs[-1] - length)))
        s1 = s0 + length
        if s1 > cliffs[-1] or any(s0 - 25.0 < j < s1 + 25.0 for j in joins):
            continue
        if any(not (s1 + 30.0 < a or s0 - 30.0 > b) for _, a, b in out):
            continue
        out.append((0, s0, s1))
    return out


def _profiles(rng: np.random.Generator, graph: PathGraph, style: PathStyle) -> PathPlan:
    profiles: dict[int, LineProfile] = {0: main_profile(rng, graph.main, style)}
    crossings: list[Crossing] = []
    by_loop = {c.upper: c for c in graph.crossings}
    for loop in graph.loops():
        c = by_loop.get(loop.id)
        pin = None
        if c is not None:
            parent = graph.lines[c.lower]
            z_p = float(np.interp(c.s_lower, parent.arc, profiles[parent.id].z))
            z_a = float(np.interp(loop.s_out, parent.arc, profiles[parent.id].z))
            z_b = float(np.interp(loop.s_back, parent.arc, profiles[parent.id].z))
            ends = max(abs(z_p - z_a), abs(z_p - z_b))
            can_under = z_p - style.cross_clearance_m >= WATER_M + 0.8
            # Arriba o abajo: el que menos rampa pida; al azar si los dos caben igual de bien.
            up_need = max(abs(z_p + style.cross_clearance_m - z_a), abs(z_p + style.cross_clearance_m - z_b))
            down_need = max(abs(z_p - style.cross_clearance_m - z_a), abs(z_p - style.cross_clearance_m - z_b))
            under = can_under and (down_need < up_need - 1.0 or (abs(down_need - up_need) <= 1.0 and rng.random() < 0.5))
            del ends
            pin = (c.s_upper, z_p - style.cross_clearance_m if under else z_p + style.cross_clearance_m)
            if under:
                c = Crossing(parent.id, loop.id, c.point, c.s_lower, c.s_upper)
        profiles[loop.id] = loop_profile(rng, loop, graph, profiles, style, pin)
        if c is not None:
            crossings.append(c)
    for c in crossings:
        upper = graph.lines[c.upper]
        half = float(np.interp(c.s_upper, upper.arc, profiles[c.upper].half_width)) + EXCLUDE_EXTRA_M
        profiles[c.lower] = _mark(profiles[c.lower], graph.lines[c.lower], c.s_lower - half, c.s_lower + half)
    hills = _hill_tunnels(rng, graph, profiles[0], crossings, style)
    for line_id, s0, s1 in hills:
        profiles[line_id] = _mark(profiles[line_id], graph.lines[line_id], s0, s1)
    return PathPlan(graph, profiles, crossings, hills)


def build_plan(rng: np.random.Generator, style: PathStyle) -> PathPlan:
    for _ in range(20):
        graph = build_graph(rng, style)
        try:
            return _profiles(rng, graph, style)
        except Infeasible:
            continue
    raise RuntimeError("20 grafos seguidos con cruces que no caben en pendiente")
