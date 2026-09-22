"""Genera la libreria de modulos de terreno: heightfields de 400 m x 400 m, 50 por topologia.

Se ejecuta FUERA del editor:
    uv run --with numpy --with pillow python Scripts/gen_terrain_modules.py [--count 50]

Escribe en Scripts/terrain_modules/:
    <Topologia>/M_<Topologia>_<NN>.png   heightfield de 16 bits (201 x 201), fuente de verdad
    manifest.json                         modulos con topologia, semilla, puentes y plazas
    preview_<Topologia>.png               hoja de contactos sombreada, para revisar a ojo

Los PNG los importa Scripts/import_terrain_modules.py (dentro del editor) como
UTN_TerrainModuleAsset + BP_Mod_*. Semilla fija => salida identica byte a byte.

Modelo de un modulo (alturas en metros, Z = 0 es el suelo de las salidas):
  - Borde canonico: los cuatro lados comparten el MISMO perfil simetrico (cresta de
    CREST_M con una boca de suelo en el centro). Cualquier modulo casa con cualquier otro
    con cualquier rotacion. Un lado sin salida se cierra con una rampa por dentro.
  - Pasillo principal: polilineas del centro a cada salida, con serpenteo que se apaga
    junto al borde; ancho variable; plazas (zonas llanas para puzzles) en el cruce y a lo
    largo.
  - Rutas secundarias: un atajo a ras de suelo que corta entre dos salidas, y una ruta
    alta que sube por el talud, recorre la meseta y cruza el pasillo por un puente para
    volver a bajar. El puente no cabe en un heightfield: se exporta como tablero al
    manifest y el tile lo coloca como instancia.
  - Paredes: talud corto (no escalable) hasta la meseta, con colinas suaves encima.
    Variante "calzada": el exterior baja bajo el agua y el pasillo queda como una cresta.
  - Agujeros: pozos en las plazas (fuera del carril central) y en la meseta.
  - Ruido de pocas octavas + desenfoque final: sin picos.
"""

from __future__ import annotations

import argparse
import json
import math
from dataclasses import dataclass
from pathlib import Path

import numpy as np
from PIL import Image

# ── Geometria y codificacion (deben casar con UTN_TerrainModuleAsset) ─────────────
SIZE_M = 400.0
HALF_M = SIZE_M / 2.0
RES = 201                      # 2 m entre vertices
STEP_M = SIZE_M / (RES - 1)
HEIGHT_SCALE_UU = 0.25         # uu por unidad del PNG
HEIGHT_ZERO = 32768            # valor del PNG para Z = 0
UU_PER_M = 100.0
UNITS_PER_M = UU_PER_M / HEIGHT_SCALE_UU   # 400 unidades por metro

# ── Borde canonico ────────────────────────────────────────────────────────────────
CREST_M = 10.0                 # cota de la cresta en el borde
OPEN_HALF_M = 18.0             # semiancho de la boca del pasillo en el borde
BANK_M = 12.0                  # anchura del talud de la boca
UNDULATE_FROM_M = 60.0         # desde aqui la cresta ondula...
UNDULATE_TO_M = 170.0          # ...y vuelve a ser plana antes de la esquina
BAND_M = 16.0                  # franja en la que el diseno se funde con el borde
PLUG_M = 22.0                  # altura minima tras una salida cerrada: rampa no escalable

# ── Rutas secundarias ─────────────────────────────────────────────────────────────
SHORTCUT_PROB = 0.6
UPPER_PROB = 0.55
UPPER_OFFSET_M = 110.0         # distancia lateral de la ruta alta al eje del pasillo
UPPER_RAMP_M = 60.0            # longitud de las rampas de subida y bajada
BRIDGE_MIN_M = 15.0
BRIDGE_MAX_M = 110.0
BRIDGE_OVERLAP_M = 4.0         # apoyo del tablero sobre cada labio
BRIDGE_THICKNESS_M = 1.2

BASE_SEED = 20260922
TOPOLOGIES: dict[str, tuple[str, ...]] = {
    "Straight": ("S", "N"),
    "CurveLeft": ("S", "W"),
    "CurveRight": ("S", "E"),
    "TLeft": ("S", "N", "W"),
    "TRight": ("S", "N", "E"),
    "Cross": ("N", "E", "S", "W"),
}
# X = fila (Sur -> Norte), Y = columna (Oeste -> Este), igual que en el asset.
EXIT_DIR = {"N": (1.0, 0.0), "E": (0.0, 1.0), "S": (-1.0, 0.0), "W": (0.0, -1.0)}
EXIT_POINT = {k: (dx * HALF_M, dy * HALF_M) for k, (dx, dy) in EXIT_DIR.items()}

OUTPUT_DIR = Path(__file__).resolve().parent / "terrain_modules"

_AXIS = np.linspace(-HALF_M, HALF_M, RES)
XX, YY = np.meshgrid(_AXIS, _AXIS, indexing="ij")
DIST_TO_EDGE = np.minimum(HALF_M - np.abs(XX), HALF_M - np.abs(YY))

Point = tuple[float, float]


# ── Utilidades ────────────────────────────────────────────────────────────────────
def smoothstep(edge0: float, edge1: float, x):
    t = np.clip((x - edge0) / (edge1 - edge0), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def value_noise(rng: np.random.Generator, wavelength_m: float, pts_x=None, pts_y=None):
    """Value noise en [-1, 1] con interpolacion suave; no tilea (no hace falta)."""
    px = XX if pts_x is None else pts_x
    py = YY if pts_y is None else pts_y
    cells = int(math.ceil(SIZE_M / wavelength_m)) + 4
    lattice = rng.uniform(-1.0, 1.0, (cells, cells))
    u = (px + HALF_M) / wavelength_m + 1.0
    v = (py + HALF_M) / wavelength_m + 1.0
    i0 = np.clip(np.floor(u).astype(int), 0, cells - 2)
    j0 = np.clip(np.floor(v).astype(int), 0, cells - 2)
    fu = smoothstep(0.0, 1.0, u - i0)
    fv = smoothstep(0.0, 1.0, v - j0)
    a = lattice[i0, j0]
    b = lattice[i0 + 1, j0]
    c = lattice[i0, j0 + 1]
    d = lattice[i0 + 1, j0 + 1]
    return (a * (1 - fu) + b * fu) * (1 - fv) + (c * (1 - fu) + d * fu) * fv


def fbm(rng: np.random.Generator, wavelength_m: float, octaves: int = 3, gain: float = 0.5, **kw):
    total = np.zeros_like(XX)
    amplitude = 1.0
    norm = 0.0
    for octave in range(octaves):
        total += amplitude * value_noise(rng, wavelength_m / (2 ** octave), **kw)
        norm += amplitude
        amplitude *= gain
    return total / norm


def blur(field, passes: int = 1):
    """Desenfoque separable 1-4-6-4-1 (aprox. gaussiano, sigma ~ 1 muestra)."""
    kernel = np.array([1.0, 4.0, 6.0, 4.0, 1.0]) / 16.0
    out = field
    for _ in range(passes):
        padded = np.pad(out, 2, mode="edge")
        tmp = sum(kernel[k] * padded[k:k + RES, 2:2 + RES] for k in range(5))
        padded = np.pad(tmp, 2, mode="edge")
        out = sum(kernel[k] * padded[2:2 + RES, k:k + RES] for k in range(5))
    return out


def polyline_distance(px, py, points: tuple[Point, ...]):
    """(distancia, parametro de arco s del punto mas cercano, longitud total)."""
    best_d = np.full_like(px, np.inf)
    best_s = np.zeros_like(px)
    cum = 0.0
    for (ax, ay), (bx, by) in zip(points[:-1], points[1:]):
        abx, aby = bx - ax, by - ay
        length_sq = abx * abx + aby * aby
        length = math.sqrt(length_sq)
        t = 0.0 if length_sq == 0.0 else np.clip(((px - ax) * abx + (py - ay) * aby) / length_sq, 0.0, 1.0)
        d = np.hypot(px - (ax + abx * t), py - (ay + aby * t))
        closer = d < best_d
        best_d = np.where(closer, d, best_d)
        best_s = np.where(closer, cum + t * length, best_s)
        cum += length
    return best_d, best_s, cum


def grid_index(point: Point) -> tuple[int, int]:
    i = int(round((point[0] + HALF_M) / STEP_M))
    j = int(round((point[1] + HALF_M) / STEP_M))
    return min(max(i, 0), RES - 1), min(max(j, 0), RES - 1)


def scaled(p: Point, k: float) -> Point:
    return (p[0] * k, p[1] * k)


def added(a: Point, b: Point) -> Point:
    return (a[0] + b[0], a[1] + b[1])


def perpendicular(d: Point) -> Point:
    return (-d[1], d[0])


# ── Borde canonico ────────────────────────────────────────────────────────────────
def canonical_profile(t_abs):
    """Altura del borde en funcion de |t| (t a lo largo del lado, 0 en el centro)."""
    bank = smoothstep(OPEN_HALF_M, OPEN_HALF_M + BANK_M, t_abs)
    fade = smoothstep(UNDULATE_FROM_M - 20.0, UNDULATE_FROM_M, t_abs) * (1.0 - smoothstep(UNDULATE_TO_M - 25.0, UNDULATE_TO_M, t_abs))
    undulation = 1.3 * np.cos(2.0 * np.pi * t_abs / 61.0) + 0.7 * np.cos(2.0 * np.pi * t_abs / 23.0 + 1.1)
    return CREST_M * bank + undulation * fade


def canonical_border_field():
    """Perfil del lado mas cercano, extendido hacia dentro (constante en la esquina)."""
    nearest_is_ns = (HALF_M - np.abs(XX)) <= (HALF_M - np.abs(YY))
    t_abs = np.where(nearest_is_ns, np.abs(YY), np.abs(XX))
    return canonical_profile(t_abs)


CANONICAL = canonical_border_field()
BORDER_WEIGHT = 1.0 - smoothstep(0.0, BAND_M, DIST_TO_EDGE)


# ── Rutas ─────────────────────────────────────────────────────────────────────────
@dataclass(frozen=True)
class Room:
    x: float
    y: float
    radius: float


@dataclass(frozen=True)
class Lane:
    """Polilinea de una ruta. rise_m > 0: ruta alta que sube esa cota sobre el suelo;
    crossing = indices del tramo que cruza el pasillo por el puente."""
    points: tuple[Point, ...]
    half_width: float
    rise_m: float = 0.0
    crossing: tuple[int, int] = (0, 0)

    def arc_range(self, first: int, last: int) -> tuple[float, float]:
        """Parametro de arco s donde empiezan y acaban los tramos [first, last)."""
        lengths = [math.dist(a, b) for a, b in zip(self.points[:-1], self.points[1:])]
        return sum(lengths[:first]), sum(lengths[:last])


@dataclass(frozen=True)
class Bridge:
    x: float
    y: float
    yaw_deg: float
    length_m: float
    width_m: float
    deck_m: float


def main_lanes(exits: tuple[str, ...]) -> list[Lane]:
    return [Lane(((0.0, 0.0), EXIT_POINT[e]), 0.0) for e in exits]


def pick_shortcut(rng: np.random.Generator, exits: tuple[str, ...]) -> Lane | None:
    """Atajo a ras de suelo entre dos salidas: cuerda de una esquina, o rodeo de la plaza."""
    if len(exits) < 2 or rng.random() > SHORTCUT_PROB:
        return None
    pairs = [(a, b) for i, a in enumerate(exits) for b in exits[i + 1:]]
    perpendicular_pairs = [(a, b) for a, b in pairs if EXIT_DIR[a][0] * EXIT_DIR[b][0] + EXIT_DIR[a][1] * EXIT_DIR[b][1] == 0.0]
    a, b = perpendicular_pairs[int(rng.integers(len(perpendicular_pairs)))] if perpendicular_pairs else pairs[int(rng.integers(len(pairs)))]
    p1 = scaled(EXIT_POINT[a], float(rng.uniform(0.5, 0.7)))
    p2 = scaled(EXIT_POINT[b], float(rng.uniform(0.5, 0.7)))
    half_width = float(rng.uniform(7.0, 10.0))
    if perpendicular_pairs:
        return Lane((p1, p2), half_width)
    side = 1.0 if rng.random() < 0.5 else -1.0
    normal = perpendicular(EXIT_DIR[a])
    mid = added(scaled(added(p1, p2), 0.5), scaled(normal, side * float(rng.uniform(70.0, 100.0))))
    return Lane((p1, mid, p2), half_width)


def upper_lane_candidates(rng: np.random.Generator, exits: tuple[str, ...], rise_m: float) -> list[Lane]:
    """Rutas altas posibles: nacen en la salida A, cruzan el pasillo de X y bajan tras el cruce."""
    candidates = []
    order = list(exits)
    rng.shuffle(order)
    for crossed in order:
        for branch in order:
            if branch == crossed:
                continue
            dir_a, dir_x = EXIT_DIR[branch], EXIT_DIR[crossed]
            p1 = scaled(EXIT_POINT[branch], float(rng.uniform(0.55, 0.75)))
            crossing = scaled(EXIT_POINT[crossed], float(rng.uniform(0.45, 0.62)))
            p2 = scaled(EXIT_POINT[crossed], float(rng.uniform(0.72, 0.8)))
            # Sale del pasillo y vuelve a entrar siempre en perpendicular: asi la rampa
            # arranca en el talud y no hay tramos oblicuos dentro del pasillo.
            if dir_a[0] * dir_x[0] + dir_a[1] * dir_x[1] < 0.0:
                # Salidas opuestas: la ruta corre paralela al pasillo y lo cruza en perpendicular.
                side = 1.0 if rng.random() < 0.5 else -1.0
                normal = scaled(perpendicular(dir_x), side * UPPER_OFFSET_M)
                back = scaled(normal, -1.0)
                candidates.append(Lane((p1, added(p1, normal), added(crossing, normal),
                                        added(crossing, back), added(p2, back), p2), 0.0, rise_m, (2, 3)))
            else:
                # Salidas perpendiculares: sube hacia X, cruza su pasillo y baja al otro lado.
                w1 = added(p1, scaled(dir_x, UPPER_OFFSET_M))
                far = added(crossing, scaled(dir_a, -UPPER_OFFSET_M))
                far2 = added(p2, scaled(dir_a, -UPPER_OFFSET_M))
                candidates.append(Lane((p1, w1, far, far2, p2), 0.0, rise_m, (1, 2)))
    return candidates


def pick_rooms(rng: np.random.Generator, exits: tuple[str, ...]) -> list[Room]:
    rooms = [Room(0.0, 0.0, float(rng.uniform(45.0, 70.0)))]
    for e in exits:
        if rng.random() < 0.7:
            ex, ey = EXIT_POINT[e]
            t = float(rng.uniform(0.42, 0.68))
            rooms.append(Room(ex * t, ey * t, float(rng.uniform(32.0, 58.0))))
    return rooms


def room_mask(rooms: list[Room], feather: float = 6.0):
    mask = np.zeros_like(XX)
    for room in rooms:
        d = np.hypot(XX - room.x, YY - room.y)
        mask = np.maximum(mask, 1.0 - smoothstep(room.radius - feather, room.radius + feather, d))
    return mask


def pits(rng: np.random.Generator, rooms: list[Room], d_corr, hw):
    """Pozos: en las plazas (fuera del carril central) y sueltos en la meseta."""
    depression = np.zeros_like(XX)
    for room in rooms:
        if rng.random() < 0.45:
            angle = float(rng.uniform(0.0, 2.0 * np.pi))
            offset = room.radius * 0.55
            cx, cy = room.x + offset * math.cos(angle), room.y + offset * math.sin(angle)
            radius = float(rng.uniform(5.0, 8.0))
            depth = float(rng.uniform(3.5, 6.0))
            d = np.hypot(XX - cx, YY - cy)
            depression += depth * (1.0 - smoothstep(radius * 0.45, radius, d))
    for _ in range(int(rng.integers(0, 3))):
        cx, cy = rng.uniform(-140.0, 140.0, 2)
        radius = float(rng.uniform(8.0, 16.0))
        depth = float(rng.uniform(4.0, 9.0))
        d = np.hypot(XX - cx, YY - cy)
        bowl = depth * (1.0 - smoothstep(radius * 0.45, radius, d))
        # Solo en la meseta: nunca muerde el pasillo ni su talud.
        depression += bowl * smoothstep(hw + 14.0, hw + 26.0, d_corr)
    return depression


# ── Puentes ───────────────────────────────────────────────────────────────────────
def dilate(mask: np.ndarray, radius: int) -> np.ndarray:
    """Dilatacion binaria con un cuadrado de lado 2*radius+1."""
    padded = np.pad(mask, radius, mode="constant", constant_values=False)
    out = np.zeros_like(mask)
    for di in range(2 * radius + 1):
        for dj in range(2 * radius + 1):
            out |= padded[di:di + RES, dj:dj + RES]
    return out


def connected_components(mask: np.ndarray) -> list[np.ndarray]:
    """Componentes 4-conexas de una mascara booleana; cada una como array (n, 2) de indices."""
    visited = np.zeros_like(mask, dtype=bool)
    components = []
    for start in zip(*np.nonzero(mask)):
        if visited[start]:
            continue
        stack = [start]
        visited[start] = True
        cells = []
        while stack:
            i, j = stack.pop()
            cells.append((i, j))
            for ni, nj in ((i + 1, j), (i - 1, j), (i, j + 1), (i, j - 1)):
                if 0 <= ni < RES and 0 <= nj < RES and mask[ni, nj] and not visited[ni, nj]:
                    visited[ni, nj] = True
                    stack.append((ni, nj))
        components.append(np.array(cells))
    return components


def bridges_for_lane(lane_mask, core, lane_height, terrain, half_width: float, s, crossing_range) -> list[Bridge] | None:
    """Un tablero por hueco que el pasillo abre en la ruta alta. None si no es viable.

    Donde la ruta nace y muere esta a ras del pasillo: esos tramos tambien caen dentro
    del pasillo, pero no son huecos (la ruta esta a la cota del suelo) y no llevan puente.
    """
    # Umbral bajo del nucleo: los taludes finos entre dos pasillos que se tocan (atajo que
    # nace del principal) no parten el hueco en dos: un solo tablero salva la horquilla.
    gap = dilate((lane_mask > 0.5) & (core > 0.15), radius=2)
    components = [c for c in connected_components(gap) if len(c) >= 3]
    components = [c for c in components if float((lane_height - terrain)[c[:, 0], c[:, 1]].mean()) >= 4.0]
    components = [c for c in components if crossing_range[0] <= float(s[c[:, 0], c[:, 1]].mean()) <= crossing_range[1]]
    if not (1 <= len(components) <= 3):
        return None
    bridges = []
    for cells in components:
        xs = cells[:, 0] * STEP_M - HALF_M
        ys = cells[:, 1] * STEP_M - HALF_M
        cx, cy = float(xs.mean()), float(ys.mean())
        cov = np.cov(np.stack([xs - cx, ys - cy]))
        eigen_values, eigen_vectors = np.linalg.eigh(cov)
        axis = eigen_vectors[:, int(np.argmax(eigen_values))]
        along = (xs - cx) * axis[0] + (ys - cy) * axis[1]
        length = float(along.max() - along.min()) + STEP_M + 2.0 * BRIDGE_OVERLAP_M
        if not BRIDGE_MIN_M <= length <= BRIDGE_MAX_M:
            return None
        # Centro en mitad del recorrido a lo largo del eje, no en el centroide: un hueco en
        # L (pasillo oblicuo) desplazaria el centroide fuera de la ruta.
        mid = float(along.max() + along.min()) * 0.5
        cx, cy = cx + axis[0] * mid, cy + axis[1] * mid
        deck = float(lane_height[cells[:, 0], cells[:, 1]].mean())
        bridges.append(Bridge(round(cx, 2), round(cy, 2), round(math.degrees(math.atan2(axis[1], axis[0])), 1),
                              round(length, 2), round(2.0 * half_width + 2.0, 2), round(deck, 2)))
    return bridges


# ── Diseno del interior ───────────────────────────────────────────────────────────
def lane_profile_height(floor, d_core, lane: Lane, s, total: float):
    """Cota de una ruta alta: a ras del pasillo mientras esta dentro de el, rampa desde el
    talud (en funcion de la distancia al nucleo del pasillo) y +rise en la meseta. En el
    tramo de cruce se mantiene alta pase lo que pase debajo: ahi va el puente."""
    i1, j1 = grid_index(lane.points[0])
    i2, j2 = grid_index(lane.points[-1])
    f1, f2 = float(floor[i1, j1]), float(floor[i2, j2])
    s_cross0, s_cross1 = lane.arc_range(*lane.crossing)
    in_crossing = (s >= s_cross0) & (s <= s_cross1)
    ramp = np.where(in_crossing, 1.0, smoothstep(0.0, UPPER_RAMP_M, d_core))
    return f1 + (f2 - f1) * (s / max(total, 1.0)) + lane.rise_m * ramp


def design_module(rng: np.random.Generator, exits: tuple[str, ...]):
    """Heightfield del interior (metros) antes de fundirlo con el borde canonico."""
    edge_fade = smoothstep(0.0, 50.0, DIST_TO_EDGE)
    is_causeway = rng.random() < 0.2
    wall_h = float(rng.uniform(8.0, 14.0))

    # Pasillo con serpenteo (deformacion del dominio) que se apaga junto al borde. Todas
    # las rutas se miden en el mismo espacio deformado, asi que casan entre si.
    warp_amp = float(rng.uniform(35.0, 65.0)) * smoothstep(0.0, 70.0, DIST_TO_EDGE)
    wx = XX + warp_amp * fbm(rng, 180.0, octaves=2)
    wy = YY + warp_amp * fbm(rng, 180.0, octaves=2)

    d_main = np.full_like(XX, np.inf)
    for lane in main_lanes(exits):
        d_main = np.minimum(d_main, polyline_distance(wx, wy, lane.points)[0])

    base_hw = float(rng.uniform(18.0, 30.0))
    hw = base_hw * (1.0 + 0.3 * value_noise(rng, 110.0))
    hw = OPEN_HALF_M + (hw - OPEN_HALF_M) * edge_fade   # en el borde, la boca canonica
    bank = float(rng.uniform(6.0, 9.0))
    bank = BANK_M + (bank - BANK_M) * edge_fade

    rooms = pick_rooms(rng, exits)
    rmask = room_mask(rooms)
    corridor = np.maximum(1.0 - smoothstep(hw, hw + bank, d_main), rmask)
    # Nucleo duro del pasillo (sin talud): la ruta alta corta el talud entero y acaba en
    # un labio casi vertical, y el tablero salva exactamente el hueco entre labios.
    core = np.maximum(1.0 - smoothstep(hw - 5.0, hw - 2.0, d_main), room_mask(rooms, 2.0))
    d_core = d_main - (hw - 2.0)
    for room in rooms:
        d_core = np.minimum(d_core, np.hypot(XX - room.x, YY - room.y) - room.radius)

    shortcut = pick_shortcut(rng, exits)
    if shortcut:
        d_sc = polyline_distance(wx, wy, shortcut.points)[0]
        corridor = np.maximum(corridor, 1.0 - smoothstep(shortcut.half_width, shortcut.half_width + 5.0, d_sc))
        core = np.maximum(core, 1.0 - smoothstep(shortcut.half_width - 4.0, shortcut.half_width - 1.0, d_sc))
        d_core = np.minimum(d_core, d_sc - (shortcut.half_width - 1.0))

    # Suelo: cota que ondula despacio y se aplana en las plazas; 0 junto a las salidas.
    elev_amp = float(rng.uniform(4.0, 9.0))
    elev = elev_amp * fbm(rng, 260.0, octaves=2) * edge_fade
    floor = elev.copy()
    for room in rooms:
        i, j = grid_index((room.x, room.y))
        m = 1.0 - smoothstep(room.radius - 6.0, room.radius + 6.0, np.hypot(XX - room.x, YY - room.y))
        floor = floor * (1.0 - m) + elev[i, j] * m
    flat_areas = [{"x_m": round(r.x, 2), "y_m": round(r.y, 2), "radius_m": round(r.radius, 2),
                   "height_m": round(float(floor[grid_index((r.x, r.y))]), 2)} for r in rooms]
    floor += 0.30 * fbm(rng, 18.0, octaves=2)                                  # arena
    off_lane = smoothstep(8.0, 13.0, d_main) * (1.0 - 0.6 * rmask)
    rocks = float(rng.uniform(1.5, 3.0)) * smoothstep(0.58, 0.76, value_noise(rng, 22.0) * 0.5 + 0.5) * off_lane
    floor += rocks * edge_fade
    floor -= pits(rng, rooms, d_main, hw)

    # Meseta: cresta sobre el suelo local + colinas suaves. Variante calzada: exterior hundido.
    hills = (fbm(rng, 170.0, octaves=3) * 0.5 + 0.5) * float(rng.uniform(12.0, 30.0))
    if is_causeway:
        high = -12.0 + 0.35 * hills + 2.0 * fbm(rng, 60.0, octaves=2)
    else:
        high = elev + wall_h + hills
    # Junto al borde la meseta tiende a la cresta canonica; las salidas cerradas se tapan.
    high = CREST_M + (high - CREST_M) * edge_fade
    closed = np.zeros_like(XX, dtype=bool)
    for side, (ex, ey) in EXIT_POINT.items():
        if side in exits:
            continue
        along = np.abs(YY) if side in ("N", "S") else np.abs(XX)
        toward = np.hypot(XX - ex, YY - ey)
        closed |= (along < OPEN_HALF_M + BANK_M + 6.0) & (toward < 44.0)
    high = np.where(closed, np.maximum(high, PLUG_M), high)

    terrain = high * (1.0 - corridor) + floor * corridor

    # Ruta alta: sube por el talud, recorre la meseta y cruza el pasillo por un puente.
    bridges: list[Bridge] = []
    has_upper = False
    if not is_causeway and rng.random() < UPPER_PROB:
        for lane in upper_lane_candidates(rng, exits, wall_h + 2.0):
            d_up, s_up, total = polyline_distance(wx, wy, lane.points)
            lane_half = float(rng.uniform(5.0, 7.0))
            lane_mask = 1.0 - smoothstep(lane_half, lane_half + 5.0, d_up)
            lane_height = lane_profile_height(floor, d_core, lane, s_up, total)
            s_cross0, s_cross1 = lane.arc_range(*lane.crossing)
            found = bridges_for_lane(lane_mask, core, lane_height, terrain, lane_half, s_up, (s_cross0, s_cross1))
            if found is None:
                continue
            weight = lane_mask * (1.0 - core)
            terrain = terrain * (1.0 - weight) + lane_height * weight
            bridges = found
            has_upper = True
            break

    terrain = blur(terrain)
    stats = {
        "causeway": is_causeway,
        "rooms": len(rooms),
        "shortcut": shortcut is not None,
        "upper_route": has_upper,
        "wall_h": round(wall_h, 2),
        "base_half_width": round(base_hw, 2),
    }
    return terrain, stats, bridges, flat_areas


def compose_module(seed: int, exits: tuple[str, ...]):
    rng = np.random.default_rng(seed)
    design, stats, bridges, flat_areas = design_module(rng, exits)
    heights_m = design * (1.0 - BORDER_WEIGHT) + CANONICAL * BORDER_WEIGHT
    quantized = np.rint(heights_m * UNITS_PER_M) + HEIGHT_ZERO
    return np.clip(quantized, 0, 65535).astype(np.uint16), stats, bridges, flat_areas


# ── Validacion y salida ───────────────────────────────────────────────────────────
def canonical_edge_vector() -> np.ndarray:
    values = np.rint(canonical_profile(np.abs(_AXIS)) * UNITS_PER_M) + HEIGHT_ZERO
    return values.astype(np.uint16)


def check_border(heights: np.ndarray, expected: np.ndarray, name: str) -> None:
    edges = (heights[0, :], heights[-1, :], heights[:, 0], heights[:, -1])
    for k, edge in enumerate(edges):
        if not np.array_equal(edge, expected):
            bad = int(np.argmax(edge != expected))
            raise AssertionError(f"{name}: lado {k} rompe el borde canonico en la muestra {bad}")
    if not np.array_equal(expected, expected[::-1]):
        raise AssertionError("el perfil canonico no es simetrico")


def check_bridges(heights: np.ndarray, bridges: list[Bridge], name: str) -> None:
    """Bajo el centro del tablero hay hueco (pasillo) y en los apoyos el terreno llega al tablero."""
    meters = (heights.astype(np.float64) - HEIGHT_ZERO) / UNITS_PER_M
    for b in bridges:
        ax, ay = math.cos(math.radians(b.yaw_deg)), math.sin(math.radians(b.yaw_deg))
        inner = int(b.length_m / 2.0 - BRIDGE_OVERLAP_M)
        under = min(meters[grid_index((b.x + ax * k, b.y + ay * k))] for k in range(-inner, inner + 1, 2))
        if b.deck_m - under < 4.0:
            raise AssertionError(f"{name}: el puente en ({b.x}, {b.y}) no salva ningun hueco ({b.deck_m - under:.1f} m)")
        # Apoyos: mas alla de cada extremo, en algun punto del ancho del tablero, el terreno
        # llega al tablero (puede sobrepasarlo: el tablero se entierra un poco en la ladera).
        half_w = int(b.width_m / 2.0)
        for sign in (1.0, -1.0):
            reach = b.length_m / 2.0 + 3.0
            end = max(meters[grid_index((b.x + sign * ax * reach - ay * o, b.y + sign * ay * reach + ax * o))]
                      for o in range(-half_w, half_w + 1, 2))
            if end < b.deck_m - 4.0:
                raise AssertionError(f"{name}: apoyo del puente a {end - b.deck_m:.1f} m del tablero")


def slope_degrees(heights: np.ndarray) -> np.ndarray:
    meters = (heights.astype(np.float64) - HEIGHT_ZERO) / UNITS_PER_M
    gx, gy = np.gradient(meters, STEP_M)
    return np.degrees(np.arctan(np.hypot(gx, gy)))


def hillshade(heights: np.ndarray, bridges: list[Bridge]) -> np.ndarray:
    meters = (heights.astype(np.float64) - HEIGHT_ZERO) / UNITS_PER_M
    gx, gy = np.gradient(meters, STEP_M)
    nx, ny, nz = -gx, -gy, np.ones_like(gx)
    norm = np.sqrt(nx * nx + ny * ny + nz * nz)
    light = np.array([-0.5, 0.35, 0.79])
    shade = np.clip((nx * light[0] + ny * light[1] + nz * light[2]) / norm, 0.0, 1.0)
    # Tinte por cota: azul bajo el agua, arena en el suelo, oscuro en la meseta.
    water = smoothstep(-2.0, -5.0, meters)
    high = smoothstep(4.0, 9.0, meters)
    r = 0.85 * (1 - high) + 0.35 * high
    g = 0.70 * (1 - high) + 0.32 * high
    b = 0.45 * (1 - high) + 0.28 * high
    rgb = np.stack([r, g, b], axis=-1) * (0.35 + 0.65 * shade)[..., None]
    rgb = rgb * (1 - water)[..., None] + np.array([0.15, 0.35, 0.65]) * water[..., None]
    for bridge in bridges:
        ax, ay = math.cos(math.radians(bridge.yaw_deg)), math.sin(math.radians(bridge.yaw_deg))
        along = (XX - bridge.x) * ax + (YY - bridge.y) * ay
        across = -(XX - bridge.x) * ay + (YY - bridge.y) * ax
        deck = (np.abs(along) <= bridge.length_m / 2.0) & (np.abs(across) <= bridge.width_m / 2.0 + 1.0)
        rgb[deck] = np.array([0.95, 0.95, 0.85])
    return (np.clip(rgb, 0, 1) * 255).astype(np.uint8)


def write_contact_sheet(path: Path, thumbs: list[np.ndarray], columns: int = 10) -> None:
    size = 120
    rows = int(math.ceil(len(thumbs) / columns))
    sheet = np.zeros((rows * size, columns * size, 3), dtype=np.uint8)
    for k, thumb in enumerate(thumbs):
        img = Image.fromarray(thumb).resize((size, size), Image.Resampling.BILINEAR)
        # El eje X (Sur -> Norte) va hacia ARRIBA en la miniatura.
        arr = np.asarray(img)[::-1, :, :]
        r, c = divmod(k, columns)
        sheet[r * size:(r + 1) * size, c * size:(c + 1) * size] = arr
    Image.fromarray(sheet).save(path)


def main() -> None:
    parser = argparse.ArgumentParser(description="Genera la libreria de modulos de terreno.")
    parser.add_argument("--count", type=int, default=50, help="modulos por topologia")
    parser.add_argument("--out", type=Path, default=OUTPUT_DIR)
    args = parser.parse_args()

    expected_edge = canonical_edge_vector()
    manifest = {
        "size_uu": SIZE_M * UU_PER_M,
        "resolution": RES,
        "height_scale_uu": HEIGHT_SCALE_UU,
        "height_zero": HEIGHT_ZERO,
        "modules": [],
    }
    index = 0
    for topology, exits in TOPOLOGIES.items():
        folder = args.out / topology
        folder.mkdir(parents=True, exist_ok=True)
        thumbs = []
        for n in range(args.count):
            seed = BASE_SEED + index
            index += 1
            name = f"M_{topology}_{n + 1:02d}"
            heights, stats, bridges, flat_areas = compose_module(seed, exits)
            check_border(heights, expected_edge, name)
            check_bridges(heights, bridges, name)
            Image.fromarray(heights).save(folder / f"{name}.png")
            thumbs.append(hillshade(heights, bridges))
            slopes = slope_degrees(heights)
            meters = (heights.astype(np.float64) - HEIGHT_ZERO) / UNITS_PER_M
            manifest["modules"].append({
                "name": name,
                "topology": topology,
                "seed": seed,
                "file": f"{topology}/{name}.png",
                "min_m": round(float(meters.min()), 2),
                "max_m": round(float(meters.max()), 2),
                "slope_p99_deg": round(float(np.percentile(slopes, 99)), 1),
                "bridges": [{"x_m": b.x, "y_m": b.y, "yaw_deg": b.yaw_deg, "length_m": b.length_m,
                             "width_m": b.width_m, "thickness_m": BRIDGE_THICKNESS_M, "deck_m": b.deck_m}
                            for b in bridges],
                "flat_areas": flat_areas,
                **stats,
            })
        write_contact_sheet(args.out / f"preview_{topology}.png", thumbs)
        print(f"{topology}: {args.count} modulos")

    (args.out / "manifest.json").write_text(json.dumps(manifest, indent=1), encoding="utf-8")
    mods = manifest["modules"]
    print(f"total {len(mods)} modulos; cota [{min(m['min_m'] for m in mods)}, {max(m['max_m'] for m in mods)}] m; "
          f"pendiente p99 max {max(m['slope_p99_deg'] for m in mods)} grados; "
          f"atajos {sum(m['shortcut'] for m in mods)}; rutas altas {sum(m['upper_route'] for m in mods)}; "
          f"puentes {sum(len(m['bridges']) for m in mods)}")


if __name__ == "__main__":
    main()
