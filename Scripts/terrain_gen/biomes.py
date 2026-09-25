"""Disenadores de modulo por bioma para el mapa de 100 m (arena, agua, algas).

Cada disenador devuelve un ModuleDesign en metros sobre la rejilla local de core (XX = Norte,
YY = Este, origen en el centro del modulo). Las bocas no van en el centro del lado: su
posicion la fija el generador del mapa (un desplazamiento por lado compartido), asi que las
dos celdas de una union la ven en el mismo sitio.

Cotas de referencia:
    arena y algas: suelo del camino a 0 m; paredes y meseta por encima.
    agua: fondo del mar bajo WATER_M; el pasillito de arena a WATER_TOP_M, un palmo sobre el agua.

Tunel (solo arena): capas extra sobre el heightfield del suelo, en la misma rejilla.
roof_delta y ceil_delta son las alturas del techo (cima de la colina) y de la boveda sobre el
suelo, en metros; NaN fuera de la huella. En el borde lateral de la huella las dos valen 0, de
modo que el tile las cose con el suelo en una sola malla (ver TN_TerrainTunnelDecisions.h).
"""

from __future__ import annotations

import math
from dataclasses import dataclass, field

import numpy as np

from .core import (DIST_TO_EDGE, HALF_M, WATER_M, XX, YY, Point, Room, fbm, polyline_distance, smoothstep,
                   value_noise)
from .features import chaikin, dunes, island_chain, maze_lanes, sharp_strata

WATER_TOP_M = WATER_M + 0.8        # cota del pasillito de arena en el agua
SEA_FLOOR_M = WATER_M - 1.6        # fondo del mar junto al pasillo
MOUTH_JITTER_M = 15.0              # desplazamiento maximo de una boca respecto al centro del lado
EDGE_FADE_M = 15.0                 # el ruido del suelo se apaga en el borde: las bocas casan exactas

TUNNEL_EDGE_CLEAR_M = 22.0         # la huella del tunel no se acerca mas al borde del modulo
TUNNEL_LENGTH_M = (22.0, 32.0)     # a lo largo del camino
TUNNEL_CLEAR_M = (5.0, 6.5)        # altura libre en el eje
TUNNEL_MIN_ROOF_M = 2.5            # grosor minimo de roca sobre la boveda

EDGE_FADE = smoothstep(0.0, EDGE_FADE_M, DIST_TO_EDGE)
SIDE_OUT = {"N": (1.0, 0.0), "S": (-1.0, 0.0), "E": (0.0, 1.0), "W": (0.0, -1.0)}


@dataclass
class ModuleDesign:
    floor: np.ndarray                      # suelo (m)
    foliage: np.ndarray                    # densidad de algas 0..1
    kind: str
    roof_delta: np.ndarray | None = None   # techo sobre el suelo (m); NaN = sin techo
    ceil_delta: np.ndarray | None = None   # boveda sobre el suelo (m); NaN = sin techo
    flat_areas: list[dict] = field(default_factory=list)

    @property
    def has_tunnel(self) -> bool:
        return self.roof_delta is not None


# ── Bocas y rutas ─────────────────────────────────────────────────────────────────
def random_point(rng: np.random.Generator, extent: float) -> Point:
    """Punto uniforme en el cuadrado [-extent, extent]^2."""
    x, y = rng.uniform(-extent, extent, 2)
    return float(x), float(y)


def mouth_point(side: str, offset: float) -> Point:
    """Punto de la boca en el borde del lado side, desplazado offset metros a lo largo."""
    ox, oy = SIDE_OUT[side]
    return (ox * HALF_M, offset) if ox else (offset, oy * HALF_M)


def inward(p: Point, side: str, distance: float) -> Point:
    ox, oy = SIDE_OUT[side]
    return (p[0] - ox * distance, p[1] - oy * distance)


def route_lane(rng: np.random.Generator, mouths: dict[str, Point], wander: float = 0.45) -> tuple[Point, ...]:
    """Polilinea suave de boca a boca (o de la unica boca al centro): entra recta 10 m y
    serpentea por 1-3 puntos de paso sorteados."""
    sides = list(mouths)
    a = sides[0]
    start = mouths[a]
    if len(sides) > 1:
        b = sides[1]
        end = mouths[b]
        tail: list[Point] = [inward(end, b, 10.0), end]
    else:
        tail = [(0.0, 0.0)]          # la unica boca lleva al centro (inicio o final del mapa)
    head: list[Point] = [start, inward(start, a, 10.0)]
    mids: list[Point] = []
    count = int(rng.integers(1, 4))
    p0, p1 = head[-1], tail[0]
    for k in range(count):
        t = (k + 1) / (count + 1)
        base = (p0[0] + (p1[0] - p0[0]) * t, p0[1] + (p1[1] - p0[1]) * t)
        jitter = rng.uniform(-wander * HALF_M, wander * HALF_M, 2)
        mids.append((float(np.clip(base[0] + jitter[0], -0.6 * HALF_M, 0.6 * HALF_M)),
                     float(np.clip(base[1] + jitter[1], -0.6 * HALF_M, 0.6 * HALF_M))))
    # Las dos primeras y las dos ultimas se repiten: chaikin deja la entrada recta.
    return chaikin(tuple(head[:1] + head + mids + tail + tail[-1:]), 3)


def lane_distance(rng: np.random.Generator, lanes, jitter_m: float = 2.0):
    """Distancia a la red de rutas, con un temblor de ruido que se apaga en el borde."""
    d = np.full_like(XX, np.inf)
    s_best = np.zeros_like(XX)
    total = 0.0
    for points in lanes:
        dl, sl, tl = polyline_distance(XX, YY, points)
        closer = dl < d
        d = np.where(closer, dl, d)
        s_best = np.where(closer, sl, s_best)
        total = max(total, tl)
    return d + jitter_m * fbm(rng, 18.0, octaves=2) * EDGE_FADE, s_best


def corridor_of(d, hw, bank):
    return 1.0 - smoothstep(hw, hw + bank, d)


# ── Arena ─────────────────────────────────────────────────────────────────────────
def sand_plateau(rng: np.random.Generator, wall_lo: float = 9.0, wall_hi: float = 14.0):
    """Meseta de arenisca en repisas (misma cota en los tres estilos de arena y en el
    ambiente, para que casen)."""
    wall = float(rng.uniform(wall_lo, wall_hi))
    high = wall + 2.5 * fbm(rng, 45.0, octaves=3) + dunes(rng, float(rng.uniform(0.6, 1.2)), float(rng.uniform(16.0, 24.0)))
    return sharp_strata(high, 2.5)


def sand_floor(rng: np.random.Generator, dune_amp: tuple[float, float] = (0.3, 0.7)):
    floor = 0.6 * fbm(rng, 40.0, octaves=2) + dunes(rng, float(rng.uniform(*dune_amp)), float(rng.uniform(12.0, 20.0)))
    return floor * EDGE_FADE


def design_sand_maze(rng: np.random.Generator, mouths: dict) -> ModuleDesign:
    """Laberinto de acantilados: canones estrechos entre repisas, con callejones sin salida."""
    exits = tuple(mouths)
    raw = maze_lanes(rng, exits, cells=4, loop_prob=0.15)
    inner = raw[:-len(exits)]
    lanes = [lane.points for lane in inner]
    for lane, side in zip(raw[-len(exits):], exits):
        cell = lane.points[0]
        lanes.append(chaikin((cell, inward(mouths[side], side, 8.0), mouths[side]), 2))
    d, _ = lane_distance(rng, lanes, 1.5)
    hw = float(rng.uniform(3.5, 5.0)) * (1.0 + 0.25 * value_noise(rng, 25.0))
    corridor = corridor_of(d, hw, float(rng.uniform(1.5, 2.5)))
    # Claro en un cruce del laberinto (plaza para puzles).
    node = inner[int(rng.integers(len(inner)))].points[0] if inner else (0.0, 0.0)
    radius = float(rng.uniform(6.0, 8.0))
    corridor = np.maximum(corridor, 1.0 - smoothstep(radius, radius + 2.0, np.hypot(XX - node[0], YY - node[1])))
    floor = sand_floor(rng)
    terrain = sand_plateau(rng) * (1.0 - corridor) + floor * corridor
    flats = [{"x_m": round(node[0], 2), "y_m": round(node[1], 2), "radius_m": round(radius, 2),
              "height_m": 0.0, "sunken": False}]
    return ModuleDesign(terrain, np.zeros_like(XX), "sand_maze", flat_areas=flats)


def design_sand_dunes(rng: np.random.Generator, mouths: dict) -> ModuleDesign:
    """Valle de dunas: camino ancho entre lomas bajas de arena, dunas normales por todo."""
    lane = route_lane(rng, mouths, 0.5)
    d, _ = lane_distance(rng, [lane], 3.0)
    hw = float(rng.uniform(9.0, 13.0))
    corridor = corridor_of(d, hw, float(rng.uniform(6.0, 9.0)))
    amp, wave = float(rng.uniform(0.8, 1.8)), float(rng.uniform(18.0, 30.0))
    field_dunes = dunes(rng, amp, wave)
    floor = (0.8 * fbm(rng, 50.0, octaves=2) + 0.7 * field_dunes) * EDGE_FADE
    high = float(rng.uniform(3.5, 6.0)) + 1.5 * fbm(rng, 40.0, octaves=2) + 1.6 * field_dunes
    terrain = high * (1.0 - corridor) + floor * corridor
    return ModuleDesign(terrain, np.zeros_like(XX), "sand_dunes")


def tunnel_span(rng: np.random.Generator, lane) -> tuple[float, float] | None:
    """Tramo [s0, s1] del camino que cabe bajo el tunel lejos del borde, o None."""
    total = sum(math.dist(a, b) for a, b in zip(lane[:-1], lane[1:]))
    samples = np.linspace(0.0, total, 200)
    points = [point_on(lane, s) for s in samples]
    inside = [HALF_M - max(abs(x), abs(y)) >= TUNNEL_EDGE_CLEAR_M for x, y in points]
    length = float(rng.uniform(*TUNNEL_LENGTH_M))
    runs, start = [], None
    for k, ok in enumerate(inside + [False]):
        if ok and start is None:
            start = k
        elif not ok and start is not None:
            runs.append((samples[start], samples[k - 1]))
            start = None
    runs = [(a, b) for a, b in runs if b - a >= TUNNEL_LENGTH_M[0]]
    if not runs:
        return None
    a, b = max(runs, key=lambda r: r[1] - r[0])
    length = min(length, b - a)
    s0 = float(rng.uniform(a, b - length))
    return s0, s0 + length


def point_on(lane, s: float) -> tuple[float, float]:
    acc = 0.0
    for a, b in zip(lane[:-1], lane[1:]):
        seg = math.dist(a, b)
        if acc + seg >= s and seg > 0.0:
            t = (s - acc) / seg
            return (a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t)
        acc += seg
    return lane[-1]


def design_sand_tunnel(rng: np.random.Generator, mouths: dict) -> ModuleDesign | None:
    """Canon estrecho que se mete bajo una colina de arenisca: tunel de verdad, en capas
    sobre el suelo (sin piezas sueltas). None si el camino no deja sitio para el tunel."""
    lane = route_lane(rng, mouths, 0.35)
    span = tunnel_span(rng, lane)
    if span is None:
        return None
    s0, s1 = span
    d, s = lane_distance(rng, [lane], 1.0)
    hw = float(rng.uniform(4.5, 6.0))
    bank = float(rng.uniform(2.0, 3.0))
    corridor = corridor_of(d, hw, bank)
    floor = sand_floor(rng, (0.2, 0.4))
    # Colina sobre el tunel: la meseta sube 3-5 m alrededor de su centro.
    cx, cy = point_on(lane, 0.5 * (s0 + s1))
    hill = float(rng.uniform(3.0, 5.0)) * (1.0 - smoothstep(10.0, 28.0, np.hypot(XX - cx, YY - cy)))
    high = sand_plateau(rng, 10.0, 13.0) + hill
    terrain = high * (1.0 - corridor) + floor * corridor

    # Huella: el pasillo (con su talud) en el tramo [s0, s1], mas un anillo de una muestra
    # alrededor donde techo = boveda = suelo, que es la costura lateral.
    in_span = (s >= s0) & (s <= s1)
    carved = in_span & (corridor > 1e-3)
    ring = in_span & ~carved & (d < hw + bank + 2.0)
    footprint = carved | ring
    roof = np.where(footprint, (high - terrain).clip(min=0.0), np.nan)
    roof[ring] = 0.0
    clear = float(rng.uniform(*TUNNEL_CLEAR_M))
    across = np.clip(d / (hw + bank), 0.0, 1.0)
    vault = clear * np.sqrt(np.clip(1.0 - across * across, 0.0, 1.0))
    ceil = np.where(footprint, np.minimum(vault, np.maximum(roof - TUNNEL_MIN_ROOF_M, 0.0)), np.nan)
    ceil[ring] = 0.0
    return ModuleDesign(terrain, np.zeros_like(XX), "sand_tunnel", roof, ceil)


# ── Agua ──────────────────────────────────────────────────────────────────────────
def sea_floor(rng: np.random.Generator, depth: float = 0.0):
    return SEA_FLOOR_M - depth + 0.4 * fbm(rng, 20.0, octaves=2)


def islets(rng: np.random.Generator, rooms: list[Room], top_range=(0.3, 1.5)):
    """Altura de un rosario de islitas de arena (0 fuera), con playa."""
    shape = np.zeros_like(XX)
    for room in rooms:
        top = WATER_M + float(rng.uniform(*top_range))
        d = np.hypot(XX - room.x, YY - room.y) + 1.2 * fbm(rng, 8.0, octaves=2)
        m = 1.0 - smoothstep(room.radius * 0.5, room.radius, d)
        shape = np.maximum(shape, m * (top - SEA_FLOOR_M))
    return shape


def design_water_path(rng: np.random.Generator, mouths: dict) -> ModuleDesign:
    """Pasillito de arena que cruza el mar por en medio, con islitas y algun ramal sin salida."""
    lane = route_lane(rng, mouths, 0.4)
    d, _ = lane_distance(rng, [lane], 1.0)
    hw = float(rng.uniform(2.5, 4.0))
    path = corridor_of(d, hw, 3.0)
    top = WATER_TOP_M + 0.2 * fbm(rng, 25.0, octaves=2) * EDGE_FADE
    base = sea_floor(rng)
    rooms = []
    total = sum(math.dist(a, b) for a, b in zip(lane[:-1], lane[1:]))
    for _ in range(int(rng.integers(1, 3))):
        # Ramal: sale del pasillo, se aleja 12-22 m y acaba en una islita.
        s_branch = float(rng.uniform(0.3, 0.7)) * total
        p = point_on(lane, s_branch)
        angle = float(rng.uniform(0.0, 2.0 * math.pi))
        reach = float(rng.uniform(12.0, 22.0))
        q = (float(np.clip(p[0] + reach * math.cos(angle), -HALF_M + 12, HALF_M - 12)),
             float(np.clip(p[1] + reach * math.sin(angle), -HALF_M + 12, HALF_M - 12)))
        d_branch = polyline_distance(XX, YY, chaikin((p, q), 1))[0]
        path = np.maximum(path, corridor_of(d_branch, float(rng.uniform(1.5, 2.5)), 2.5))
        rooms.append(Room(q[0], q[1], float(rng.uniform(4.0, 6.0))))
    for _ in range(2):
        rooms += island_chain(rng, random_point(rng, 0.7 * HALF_M), random_point(rng, 0.7 * HALF_M),
                              (3.0, 6.0), (4.0, 9.0), 12.0)
    ground = base + islets(rng, rooms)
    terrain = np.maximum(ground, base * (1.0 - path) + top * path)
    return ModuleDesign(terrain, np.zeros_like(XX), "water_path")


# ── Algas ─────────────────────────────────────────────────────────────────────────
def algae_foliage(rng: np.random.Generator, d=None, hw: float = 0.0):
    """Densidad del bosque: espesa fuera del camino, a manchas en su borde, nada en el eje."""
    patches = smoothstep(0.35, 0.7, fbm(rng, 14.0, octaves=2) * 0.5 + 0.5)
    density = 0.8 + 0.2 * patches
    if d is not None:
        density = np.where(d < hw + 2.0, 0.35 * patches, density) * smoothstep(hw - 2.0, hw, d)
    return np.clip(density, 0.0, 1.0)


def design_algae_path(rng: np.random.Generator, mouths: dict) -> ModuleDesign:
    """Bosque de algas: camino que serpentea entre lomas bajas cubiertas de espesura."""
    lane = route_lane(rng, mouths, 0.5)
    d, _ = lane_distance(rng, [lane], 2.0)
    hw = float(rng.uniform(5.0, 8.0))
    corridor = corridor_of(d, hw, float(rng.uniform(4.0, 6.0)))
    total = sum(math.dist(a, b) for a, b in zip(lane[:-1], lane[1:]))
    cx, cy = point_on(lane, float(rng.uniform(0.35, 0.65)) * total)
    radius = float(rng.uniform(7.0, 10.0))
    clearing = 1.0 - smoothstep(radius, radius + 3.0, np.hypot(XX - cx, YY - cy))
    corridor = np.maximum(corridor, clearing)
    floor = 0.5 * fbm(rng, 30.0, octaves=2) * EDGE_FADE
    high = algae_plateau(rng)
    terrain = high * (1.0 - corridor) + floor * corridor
    foliage = algae_foliage(rng, np.minimum(d, np.where(clearing > 0.5, 0.0, np.inf)), hw)
    flats = [{"x_m": round(cx, 2), "y_m": round(cy, 2), "radius_m": round(radius, 2), "height_m": 0.0, "sunken": False}]
    return ModuleDesign(terrain, foliage, "algae_path", flat_areas=flats)


def algae_plateau(rng: np.random.Generator):
    return float(rng.uniform(5.0, 7.5)) + 2.5 * (fbm(rng, 35.0, octaves=3) * 0.5 + 0.5)


# ── Ambiente (celdas fuera del camino) ────────────────────────────────────────────
def design_ambient(rng: np.random.Generator, biome: str) -> ModuleDesign:
    if biome == "water":
        rooms = []
        for _ in range(int(rng.integers(1, 3))):
            rooms += island_chain(rng, random_point(rng, 0.8 * HALF_M), random_point(rng, 0.8 * HALF_M),
                                  (3.0, 8.0), (5.0, 12.0), 15.0)
        base = sea_floor(rng, float(rng.uniform(0.3, 1.2)))
        return ModuleDesign(base + islets(rng, rooms, (0.3, 2.0)), np.zeros_like(XX), "water_ambient")
    if biome == "algae":
        return ModuleDesign(algae_plateau(rng), algae_foliage(rng), "algae_ambient")
    return ModuleDesign(sand_plateau(rng), np.zeros_like(XX), "sand_ambient")


# ── Seleccion ─────────────────────────────────────────────────────────────────────
SAND_STYLES = (("sand_maze", 0.5), ("sand_tunnel", 0.3), ("sand_dunes", 0.2))


def design_path_cell(rng: np.random.Generator, biome: str, mouths: dict, force_tunnel: bool = False) -> ModuleDesign:
    """Modulo de una celda del camino. force_tunnel prueba primero el canon con tunel."""
    if biome == "water":
        return design_water_path(rng, mouths)
    if biome == "algae":
        return design_algae_path(rng, mouths)
    if len(mouths) == 1:
        return design_sand_dunes(rng, mouths)     # inicio del mapa: valle abierto que llega al centro
    names = [n for n, _ in SAND_STYLES]
    weights = np.array([w for _, w in SAND_STYLES])
    style = "sand_tunnel" if force_tunnel else names[int(rng.choice(len(names), p=weights / weights.sum()))]
    if style == "sand_tunnel":
        design = design_sand_tunnel(rng, mouths)
        if design is not None:
            return design
        style = "sand_maze"
    return design_sand_maze(rng, mouths) if style == "sand_maze" else design_sand_dunes(rng, mouths)
