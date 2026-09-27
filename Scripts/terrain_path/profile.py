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
LAGOON_HALF_M = (13.0, 19.0)     # semiancho de una laguna
STREAM_HALF_M = (3.5, 5.5)       # semiancho de un arroyo
STREAM_MIN_M = 40.0              # tramo de arroyo minimo (si no cabe con las rampas, el lazo es normal)
PIN_FLAT_M = 8.0                 # tramo llano del camino a cada lado del punto de cruce


class Infeasible(Exception):
    """Un cruce no cabe con la pendiente maxima: hay que rehacer el grafo."""


@dataclass
class LineProfile:
    z: np.ndarray
    half_width: np.ndarray
    biome: np.ndarray
    tunnel: np.ndarray
    lagoon: np.ndarray | None = None     # 0..1: tramo de agua ensanchado en laguna (solo el principal)
    stream: bool = False                 # lazo que es un arroyo vadeable


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
    """Rampas lineales con las rodillas suavizadas (sin pasar de la pendiente ni mover anclajes).
    Ningun camino baja de 0,3 m sobre el agua (si no, quedaria sumergido)."""
    z = np.maximum(z, np.where(np.isin(np.arange(len(z)), list(anchors)), z, WATER_M + 0.3))
    z = clamp_grade(z, arc, grade, anchors)
    smooth = ndimage.gaussian_filter1d(z, 2.0, mode="nearest")
    for i in anchors:
        smooth[i] = z[i]
    return clamp_grade(smooth, arc, grade, anchors)


def _main_biomes(arc: np.ndarray, shares) -> np.ndarray:
    edges = np.cumsum(shares)[:-1]
    return np.searchsorted(edges, arc / arc[-1], side="right").astype(int)


def _random_biomes(rng: np.random.Generator, arc: np.ndarray, shares) -> np.ndarray:
    """Secuencia al azar de 3-6 tramos de acantilado, agua y dunas (nunca dos iguales seguidos, no
    empieza en el agua) y la playa al final. Cada bioma reparte su parte entre sus tramos."""
    kinds = [b for b in (0, 1, 2) if shares[b] > 0.0]
    n = int(rng.integers(3, 7))
    seq = [int(rng.choice([b for b in kinds if b != 1] or kinds))]
    while len(seq) < n:
        seq.append(int(rng.choice([b for b in kinds if b != seq[-1]] or kinds)))
    if 1 in kinds and 1 not in seq:
        seq[int(rng.integers(1, n))] = 1
    weights = np.array([shares[b] / seq.count(b) * float(rng.uniform(0.6, 1.4)) for b in seq])
    beach = float(shares[3]) * float(rng.uniform(0.8, 1.2))
    edges = np.cumsum(weights / weights.sum() * (1.0 - beach))[:-1]
    t = arc / arc[-1]
    biome = np.array(seq)[np.searchsorted(edges, t, side="right")]
    biome[t >= 1.0 - beach] = 3
    return biome.astype(int)


def _lagoons(rng: np.random.Generator, arc: np.ndarray, biome: np.ndarray, chance: float) -> np.ndarray:
    """0..1 a lo largo del principal: tramos de agua que se ensanchan en laguna (rampa de 15 m)."""
    out = np.zeros(len(arc))
    edges = np.flatnonzero(np.diff(np.concatenate([[0], (biome == 1).astype(int), [0]])))
    for k0, k1 in zip(edges[::2], edges[1::2]):
        s0, s1 = float(arc[k0]), float(arc[k1 - 1])
        if s1 - s0 < 50.0 or rng.random() >= chance:
            continue
        out = np.maximum(out, _smooth01(s0, s0 + 15.0, arc) * (1.0 - _smooth01(s1 - 15.0, s1, arc)))
    return out


def _widths(rng: np.random.Generator, arc: np.ndarray, biome: np.ndarray, style: PathStyle) -> np.ndarray:
    lo, mode, hi = style.width_m
    w = knot_noise(rng, arc, (20.0, 60.0), lo, hi, mode=mode)
    river = knot_noise(rng, arc, (20.0, 60.0), *style.river_half_width_m)
    water = ndimage.gaussian_filter1d((biome == 1).astype(float), 5.0, mode="nearest")
    return w * (1.0 - water) + river * water


def main_profile(rng: np.random.Generator, line: PathLine, style: PathStyle) -> LineProfile:
    if style.biome_order == "random":
        biome = _random_biomes(rng, line.arc, style.biome_shares)
    else:
        biome = _main_biomes(line.arc, style.biome_shares)
    target = _knot_levels(rng, line.arc, biome)
    end = line.length
    beach = _smooth01(end - 60.0, end, line.arc)
    target = target * (1.0 - beach) + (WATER_M + 0.3) * beach
    z = _ramps(target, line.arc, _grades(rng, line.arc, style), set())
    w = _widths(rng, line.arc, biome, style)
    w = np.maximum(w, 7.0 * (1.0 - _smooth01(8.0, 16.0, line.arc)))          # salida: ensanche
    w = w + 12.0 * _smooth01(end - 55.0, end, line.arc)                       # final: abanico a la playa
    lagoon = None
    if style.lagoon_chance > 0.0:
        lagoon = _lagoons(rng, line.arc, biome, style.lagoon_chance)
        wide = knot_noise(rng, line.arc, (25.0, 50.0), *LAGOON_HALF_M)
        w = w * (1.0 - lagoon) + wide * lagoon
    return LineProfile(z, w, biome, np.zeros(len(z), dtype=bool), lagoon)


def stream_profile(rng: np.random.Generator, line: PathLine, graph: PathGraph, profiles: dict[int, LineProfile],
                   style: PathStyle) -> LineProfile | None:
    """Lazo que es un arroyo: baja en rampa desde sus dos uniones a la cota de la orilla y el
    tramo del medio es agua vadeable (river.py). None si no cabe un tramo de STREAM_MIN_M."""
    parent, pp = graph.lines[line.parent], profiles[line.parent]
    z_a = float(np.interp(line.s_out, parent.arc, pp.z))
    z_b = float(np.interp(line.s_back, parent.arc, pp.z))
    bank = BIOME_LEVEL_M[1][0]
    r_a = abs(z_a - bank) / style.max_grade + 12.0
    r_b = abs(z_b - bank) / style.max_grade + 12.0
    if line.length - r_a - r_b < STREAM_MIN_M:
        return None
    z = np.interp(line.arc, [0.0, r_a, line.length - r_b, line.length], [z_a, bank, bank, z_b])
    z = _ramps(z, line.arc, np.full(len(z), style.max_grade), {0, len(z) - 1})
    _, k = cKDTree(graph.main.points).query(line.points)
    biome = profiles[0].biome[k].copy()
    wet = (line.arc > r_a - 4.0) & (line.arc < line.length - r_b + 4.0)
    biome[wet] = 1
    w = _widths(rng, line.arc, biome, style)
    narrow = knot_noise(rng, line.arc, (20.0, 50.0), *STREAM_HALF_M)
    water = ndimage.gaussian_filter1d(wet.astype(float), 5.0, mode="nearest")
    w = w * (1.0 - water) + narrow * water
    return LineProfile(z, w, biome, np.zeros(len(z), dtype=bool), None, True)


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
    # Tambien en las dunas (no solo en el acantilado): pocos tramos del acantilado quedan libres
    # de uniones.
    joins += [c.s_upper if c.upper == 0 else c.s_lower for c in crossings if 0 in (c.upper, c.lower)]
    cliffs = main.arc[(profile.biome == 0) | (profile.biome == 2)]
    out: list[tuple[int, float, float]] = []
    for _ in range(200):
        if len(out) >= style.hill_tunnels or len(cliffs) < 2:
            break
        length = float(rng.uniform(25.0, 45.0))
        s0 = float(rng.uniform(max(60.0, cliffs[0]), max(60.0, cliffs[-1] - length)))
        s1 = s0 + length
        k0, k1 = int(np.searchsorted(main.arc, s0)), int(np.searchsorted(main.arc, s1))
        if s1 > cliffs[-1] or any(s0 - 18.0 < j < s1 + 18.0 for j in joins) or 1 in profile.biome[k0:k1 + 1]                 or 3 in profile.biome[k0:k1 + 1]:
            continue
        if any(not (s1 + 30.0 < a or s0 - 30.0 > b) for _, a, b in out):
            continue
        out.append((0, s0, s1))
    return out


def _profiles(rng: np.random.Generator, graph: PathGraph, style: PathStyle) -> PathPlan:
    profiles: dict[int, LineProfile] = {0: main_profile(rng, graph.main, style)}
    crossings: list[Crossing] = []
    by_loop = {c.upper: c for c in graph.crossings}
    streams: set[int] = set()
    if style.streams[1] > 0:
        free = [loop.id for loop in graph.loops() if loop.id not in by_loop]
        n = min(int(rng.integers(style.streams[0], style.streams[1] + 1)), len(free))
        streams = {int(x) for x in rng.permutation(free)[:n]}
    for loop in graph.loops():
        c = by_loop.get(loop.id)
        pin = None
        if loop.id in streams:
            prof = stream_profile(rng, loop, graph, profiles, style)
            if prof is not None:
                profiles[loop.id] = prof
                continue
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
    if len(crossings) < style.crossings[0]:
        raise Infeasible(f"{len(crossings)} cruces, el estilo pide al menos {style.crossings[0]}")
    # Tipo de cada cruce con un generador aparte (no mueve el resto del plan).
    kinds = np.random.default_rng(int(rng.integers(1 << 31)))
    def _kind(c: Crossing) -> str:
        lower = graph.lines[c.lower]
        k = min(int(np.searchsorted(lower.arc, c.s_lower)), len(lower.arc) - 1)
        draw = kinds.random()
        # Sobre el rio, siempre puente: un cerro con tunel taparia el cauce.
        return "bridge" if profiles[c.lower].biome[k] == 1 or draw < style.bridge_share else "tunnel"
    crossings = [replace(c, kind=_kind(c)) for c in crossings]
    if crossings and style.bridge_share > 0.0 and all(c.kind == "tunnel" for c in crossings):
        crossings[0] = replace(crossings[0], kind="bridge")      # al menos un puente por mapa
    for c in crossings:
        upper, lower = graph.lines[c.upper], graph.lines[c.lower]
        w_up = float(np.interp(c.s_upper, upper.arc, profiles[c.upper].half_width))
        half = w_up + 1.5
        if c.kind == "bridge":
            # Puente fino: el camino de abajo va al aire libre; el tablero lo pone bridge.py.
            continue
        # Tunel: sigue mientras el camino de abajo no se ha apartado del de arriba lo bastante
        # para que quepan sus dos paredes (si no, asomaria al lado del de arriba).
        d, _ = cKDTree(upper.points).query(lower.points)
        need = w_up + profiles[c.lower].half_width + 8.0
        close = d < need
        k0 = int(np.searchsorted(lower.arc, c.s_lower))
        lo, hi = k0, k0
        while lo > 0 and close[lo - 1]:
            lo -= 1
        while hi < len(close) - 1 and close[hi + 1]:
            hi += 1
        s0 = min(c.s_lower - half, float(lower.arc[lo]) - 3.0)
        s1 = max(c.s_lower + half, float(lower.arc[hi]) + 3.0)
        profiles[c.lower] = _mark(profiles[c.lower], lower, s0, s1)
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
