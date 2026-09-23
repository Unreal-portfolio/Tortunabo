"""Genera la libreria de modulos de terreno: heightfields de 200 m x 200 m por topologia.

Se ejecuta FUERA del editor:
    uv run --with numpy --with pillow python Scripts/gen_terrain_modules.py [--count 100]
        [--out <carpeta>] [--thumb <px>]

Escribe en Scripts/terrain_modules/ (o en --out, para muestras de prueba):
    <Topologia>/M_<Topologia>_<NN>.png   heightfield de 16 bits (101 x 101), fuente de verdad
    manifest.json                         modulos con topologia, semilla, arcos y plazas
    preview_<Topologia>.png               hoja de contactos sombreada, para revisar a ojo

Los PNG los importa Scripts/import_terrain_modules.py (dentro del editor) como
UTN_TerrainModuleAsset + BP_M_*. Semilla fija => salida identica byte a byte.

Modelo de un modulo (alturas en metros, Z = 0 es el suelo de las salidas):
  - Borde canonico: los cuatro lados comparten el MISMO perfil simetrico (cresta de
    CREST_M con una boca de suelo en el centro). Cualquier modulo casa con cualquier otro
    con cualquier rotacion. Un lado sin salida se cierra con una rampa por dentro.
  - Pasillo principal: polilineas del centro a cada salida, con serpenteo que se apaga
    junto al borde; ancho variable; plazas (zonas llanas para puzzles) en el cruce y a lo
    largo.
  - Bifurcacion: un tramo del pasillo se abre alrededor de una isla (roca alta o loma
    escalable) y vuelve a juntarse; dos canales de anchura distinta.
  - Atajo: brazo curvo a ras de suelo entre dos salidas, con la misma anchura relativa y
    el mismo talud que el pasillo, para que se lea como parte del camino.
  - Ruta alta: sube por el talud, recorre la meseta con esquinas redondeadas y cruza el
    pasillo por un arco de roca natural (se camina por arriba y se pasa por debajo).
  - Arco decorativo: arco de roca de pared a pared sobre el pasillo, para pasar por debajo.
    Los arcos no caben en un heightfield: se exportan al manifest y el tile los construye
    como malla (TNTerrainModule::BuildArchMesh).
  - Paredes: talud corto (no escalable) hasta la meseta, con colinas suaves encima.
    Variante "calzada": el exterior baja bajo el agua y el pasillo queda como una cresta.
  - Agujeros: pozos en las plazas (fuera del carril central) y en la meseta.
  - Validacion: borde canonico exacto, arcos apoyados y todas las salidas accesibles a pie;
    un diseno que falla se descarta y se prueba la semilla siguiente.
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
SIZE_M = 200.0
HALF_M = SIZE_M / 2.0
RES = 101                      # 2 m entre vertices
# Escala horizontal respecto al diseno original de 400 m. Longitudes de rasgos grandes
# (serpenteo, plazas, rutas) se multiplican por K; el detalle fino (arena, rocas, taludes)
# y todas las alturas no escalan.
K = SIZE_M / 400.0
STEP_M = SIZE_M / (RES - 1)
HEIGHT_SCALE_UU = 0.25         # uu por unidad del PNG
HEIGHT_ZERO = 32768            # valor del PNG para Z = 0
UU_PER_M = 100.0
UNITS_PER_M = UU_PER_M / HEIGHT_SCALE_UU   # 400 unidades por metro

# ── Borde canonico ────────────────────────────────────────────────────────────────
CREST_M = 10.0                 # cota de la cresta en el borde
OPEN_HALF_M = 12.0             # semiancho de la boca del pasillo en el borde
BANK_M = 8.0                   # anchura del talud de la boca
UNDULATE_FROM_M = 35.0         # desde aqui la cresta ondula...
UNDULATE_TO_M = 85.0           # ...y vuelve a ser plana antes de la esquina
BAND_M = 12.0                  # franja en la que el diseno se funde con el borde
PLUG_M = 22.0                  # altura minima tras una salida cerrada: rampa no escalable

# ── Rutas secundarias ─────────────────────────────────────────────────────────────
SHORTCUT_PROB = 0.6
UPPER_PROB = 0.55
UPPER_OFFSET_M = 110.0 * K     # distancia lateral de la ruta alta al eje del pasillo
UPPER_RAMP_M = 60.0 * K        # longitud de las rampas de subida y bajada
BRIDGE_MIN_M = 12.0
BRIDGE_MAX_M = 110.0 * K
BRIDGE_OVERLAP_M = 4.0         # apoyo del tablero sobre cada labio
BRIDGE_THICKNESS_M = 2.5       # grosor del arco de roca en su centro (el tile lo engorda hacia los apoyos)

# Bifurcacion: el pasillo se abre alrededor de una isla y vuelve a juntarse.
FORK_PROB = 0.55
FORK_LOW_ISLAND_PROB = 0.35    # isla baja (loma escalable) en vez de roca alta
# Arco natural sobre el pasillo, solo para pasar por debajo.
ARCH_PROB = 0.4
ARCH_CLEARANCE_M = 6.0         # altura libre minima bajo el arco
WALKABLE_STEP_M = 1.6          # desnivel maximo entre muestras vecinas (2 m) para caminar: ~39 grados

# ── F3: tuneles, monolitos, zonas hundidas y acantilados ───────────────────────────
# Tunel: arco largo sobre un tramo recto del pasillo; se atraviesa por dentro.
TUNNEL_LENGTH_M = (16.0, 32.0)   # a lo largo del pasillo (el "width" del arco)
TUNNEL_THICKNESS_M = 4.0
# Monolito: pilar de roca (malla del tile, no cabe en un heightfield de 2 m).
MONOLITH_RADIUS_M = (2.5, 5.0)
MONOLITH_HEIGHT_M = (10.0, 26.0)
MONOLITH_LANE_CLEARANCE_M = 7.0  # distancia minima del pie al eje del pasillo
# Zona hundida: plaza lateral rebajada, con rampas caminables alrededor.
SUNKEN_DEPTH_M = (3.5, 6.0)
SUNKEN_FEATHER_M = 11.0          # media anchura de la rampa: 6 m en 11 m -> ~28 grados
# Acantilado: tramo de pared mucho mas alta y casi vertical, con estratos.
CLIFF_EXTRA_M = (12.0, 24.0)
CLIFF_BANK_M = 2.0
CLIFF_STRATA_M = 3.0

# ── F4: biomas ────────────────────────────────────────────────────────────────────
WATER_M = -4.0                   # cota del agua (ModuleWaterLevel del generador = -400 uu)
SEA_DEPTH_M = (1.0, 2.0)         # agua poco profunda: se camina por el fondo
BIOMES = ("sand", "water", "algae")
MIXED_PER_PAIR = 5               # modulos mixtos por pareja de biomas y topologia
FOLIAGE_EDGE_CLEAR_M = 6.0       # sin algas en el nucleo del pasillo ni en su borde inmediato

# ── Estilos (rangos por modulo; "weight" = peso del sorteo dentro de su bioma) ───────
_BASE_FEATURES = {"cliff_prob": 0.25, "monoliths": (0, 2), "sunken_prob": 0.25, "tunnel_prob": 0.25,
                  "arch_prob": 0.3, "foliage": 0.0, "puddles": False, "causeway": False, "fort": False}
STYLES: dict[str, dict] = {
    # Arena: desolada (dunas, paredes bajas) o montanosa (cañon y roca).
    "desert":      {**_BASE_FEATURES, "biome": "sand", "weight": 35, "wall_h": (6.0, 9.0), "bank": (4.5, 6.0),
                    "hills": (8.0, 18.0), "corridor_hw": (22.0, 34.0), "rocks": (0.5, 1.2),
                    "rock_threshold": (0.62, 0.8), "pits": (0, 1), "cliff_prob": 0.1, "monoliths": (1, 3),
                    "tunnel_prob": 0.1, "arch_prob": 0.2},
    "canyon":      {**_BASE_FEATURES, "biome": "sand", "weight": 35, "wall_h": (8.0, 14.0), "bank": (6.0, 9.0),
                    "hills": (12.0, 30.0), "corridor_hw": (18.0, 30.0), "rocks": (1.5, 3.0),
                    "rock_threshold": (0.58, 0.76), "pits": (0, 2), "cliff_prob": 0.35, "tunnel_prob": 0.5},
    "mountain":    {**_BASE_FEATURES, "biome": "sand", "weight": 30, "wall_h": (10.0, 16.0), "bank": (5.0, 7.0),
                    "hills": (20.0, 38.0), "corridor_hw": (16.0, 26.0), "rocks": (2.5, 4.5),
                    "rock_threshold": (0.48, 0.66), "pits": (1, 3), "cliff_prob": 0.6, "monoliths": (0, 3),
                    "tunnel_prob": 0.55},
    # Agua: la muralla entre el mar poco profundo, o un fuerte con foso en la plaza central.
    "causeway":    {**_BASE_FEATURES, "biome": "water", "weight": 55, "wall_h": (8.0, 12.0), "bank": (6.0, 8.0),
                    "hills": (12.0, 24.0), "corridor_hw": (16.0, 26.0), "rocks": (1.0, 2.0),
                    "rock_threshold": (0.6, 0.78), "pits": (0, 1), "causeway": True, "cliff_prob": 0.0,
                    "monoliths": (0, 2), "tunnel_prob": 0.0, "arch_prob": 0.0},
    "fort":        {**_BASE_FEATURES, "biome": "water", "weight": 45, "wall_h": (4.0, 7.0), "bank": (6.0, 8.0),
                    "hills": (6.0, 12.0), "corridor_hw": (20.0, 32.0), "rocks": (0.8, 1.8),
                    "rock_threshold": (0.6, 0.78), "pits": (0, 1), "puddles": True, "fort": True,
                    "cliff_prob": 0.0, "tunnel_prob": 0.0, "arch_prob": 0.15},
    # Algas: bosque frondoso en tierra, cerrado (cañon) o abierto (pradera).
    "kelp_forest": {**_BASE_FEATURES, "biome": "algae", "weight": 55, "wall_h": (7.0, 12.0), "bank": (6.0, 8.0),
                    "hills": (10.0, 24.0), "corridor_hw": (18.0, 30.0), "rocks": (0.8, 2.0),
                    "rock_threshold": (0.6, 0.78), "pits": (0, 1), "foliage": 0.85, "tunnel_prob": 0.45},
    "kelp_meadow": {**_BASE_FEATURES, "biome": "algae", "weight": 45, "wall_h": (5.0, 8.0), "bank": (5.0, 7.0),
                    "hills": (8.0, 16.0), "corridor_hw": (22.0, 34.0), "rocks": (0.5, 1.2),
                    "rock_threshold": (0.62, 0.8), "pits": (0, 1), "foliage": 0.5, "cliff_prob": 0.1,
                    "tunnel_prob": 0.1},
}

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
    undulation = 1.3 * np.cos(2.0 * np.pi * t_abs / 31.0) + 0.7 * np.cos(2.0 * np.pi * t_abs / 13.0 + 1.1)
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
    sunk_m: float = 0.0   # > 0: zona hundida (el suelo de la plaza baja esa cota)


@dataclass(frozen=True)
class Monolith:
    """Pilar de roca. base_m queda enterrado bajo el terreno mas bajo de su huella."""
    x: float
    y: float
    base_m: float
    radius_m: float
    height_m: float
    yaw_deg: float
    lean_deg: float


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
    """Arco de roca. kind "bridge": lleva la ruta alta por encima (se camina por arriba y
    por debajo); kind "arch": solo decorado sobre el pasillo (se pasa por debajo)."""
    x: float
    y: float
    yaw_deg: float
    length_m: float
    width_m: float
    deck_m: float
    kind: str = "bridge"


@dataclass(frozen=True)
class Fork:
    """Isla en mitad de un tramo del pasillo principal: el pasillo se ensancha, se parte en
    dos canales y vuelve a ser uno. along/half_len en metros sobre el eje centro-salida."""
    exit: str
    along: float
    half_len: float
    island_half_w: float
    offset: float
    low: bool
    low_height: float


def pick_fork(rng: np.random.Generator, exits: tuple[str, ...]) -> Fork | None:
    if rng.random() > FORK_PROB:
        return None
    exit_side = exits[int(rng.integers(len(exits)))]
    half_len = float(rng.uniform(16.0, 30.0))
    island_half_w = float(rng.uniform(4.0, 8.0))
    # El tramo cabe entre la plaza central y la franja del borde.
    along = float(rng.uniform(0.42, 0.62)) * HALF_M
    offset = float(rng.uniform(-0.35, 0.35)) * island_half_w
    low = bool(rng.random() < FORK_LOW_ISLAND_PROB)
    return Fork(exit_side, along, half_len, island_half_w, offset, low, float(rng.uniform(1.2, 2.2)))


def fork_fields(rng: np.random.Generator, fork: Fork, wx, wy):
    """(ensanche del pasillo en m, mascara de isla 0..1) medidos en el espacio deformado."""
    dx, dy = EXIT_DIR[fork.exit]
    along = wx * dx + wy * dy
    across = -wx * dy + wy * dx
    taper = 14.0
    bump = 1.0 - smoothstep(fork.half_len, fork.half_len + taper, np.abs(along - fork.along))
    widen = (1.6 * fork.island_half_w + 3.0) * bump
    ell = np.hypot((along - fork.along) / fork.half_len, (across - fork.offset) / fork.island_half_w)
    ell = ell + 0.12 * value_noise(rng, 14.0)
    island = 1.0 - smoothstep(0.78, 1.05, ell)
    return widen, island


def chaikin(points: tuple[Point, ...], iterations: int = 2) -> tuple[Point, ...]:
    """Redondea las esquinas de una polilinea conservando sus extremos."""
    pts = list(points)
    for _ in range(iterations):
        out = [pts[0]]
        for a, b in zip(pts[:-1], pts[1:]):
            out.append((0.75 * a[0] + 0.25 * b[0], 0.75 * a[1] + 0.25 * b[1]))
            out.append((0.25 * a[0] + 0.75 * b[0], 0.25 * a[1] + 0.75 * b[1]))
        out.append(pts[-1])
        pts = out
    return tuple(pts)


def main_lanes(exits: tuple[str, ...]) -> list[Lane]:
    return [Lane(((0.0, 0.0), EXIT_POINT[e]), 0.0) for e in exits]


def pick_shortcut(rng: np.random.Generator, exits: tuple[str, ...], base_half_width: float) -> Lane | None:
    """Atajo a ras de suelo entre dos salidas: cuerda curva de una esquina, o rodeo de la
    plaza. Se abre y se cierra con la misma anchura relativa y el mismo talud que el pasillo
    principal, para que se lea como un brazo del camino y no como una zanja aparte."""
    if len(exits) < 2 or rng.random() > SHORTCUT_PROB:
        return None
    pairs = [(a, b) for i, a in enumerate(exits) for b in exits[i + 1:]]
    perpendicular_pairs = [(a, b) for a, b in pairs if EXIT_DIR[a][0] * EXIT_DIR[b][0] + EXIT_DIR[a][1] * EXIT_DIR[b][1] == 0.0]
    a, b = perpendicular_pairs[int(rng.integers(len(perpendicular_pairs)))] if perpendicular_pairs else pairs[int(rng.integers(len(pairs)))]
    p1 = scaled(EXIT_POINT[a], float(rng.uniform(0.5, 0.72)))
    p2 = scaled(EXIT_POINT[b], float(rng.uniform(0.5, 0.72)))
    half_width = base_half_width * float(rng.uniform(0.55, 0.8))
    if perpendicular_pairs:
        # Curva de Bezier cuadratica con el control hacia la esquina: la cuerda se comba
        # como un camino que ataja siguiendo el terreno, no en linea recta.
        corner = added(EXIT_POINT[a], EXIT_POINT[b])
        mid = scaled(added(p1, p2), 0.5)
        pull = float(rng.uniform(0.1, 0.3))
        control = added(mid, scaled(added(corner, scaled(mid, -1.0)), pull))
    else:
        side = 1.0 if rng.random() < 0.5 else -1.0
        normal = perpendicular(EXIT_DIR[a])
        control = added(scaled(added(p1, p2), 0.5), scaled(normal, side * float(rng.uniform(70.0, 100.0)) * K * 2.0))
    samples = [((1 - t) ** 2 * p1[0] + 2 * (1 - t) * t * control[0] + t * t * p2[0],
                (1 - t) ** 2 * p1[1] + 2 * (1 - t) * t * control[1] + t * t * p2[1])
               for t in np.linspace(0.0, 1.0, 12)]
    return Lane(tuple(samples), half_width)


def smooth_lane(lane: Lane) -> Lane:
    """Redondea las esquinas de una ruta alta sin tocar su tramo de cruce: se suaviza por
    separado lo que va antes y despues del cruce y se recalculan sus indices."""
    first, last = lane.crossing
    before = chaikin(lane.points[:first + 1])
    after = chaikin(lane.points[last:])
    points = before + lane.points[first + 1:last] + after
    crossing = (len(before) - 1, len(before) - 1 + (last - first))
    return Lane(points, lane.half_width, lane.rise_m, crossing)


def upper_lane_candidates(rng: np.random.Generator, exits: tuple[str, ...], rise_m: float) -> list[Lane]:
    """Rutas altas posibles: nacen en la salida A, cruzan el pasillo de X y bajan tras el cruce."""
    candidates = []
    order = list(exits)
    rng.shuffle(order)
    # Varios puntos de cruce por pareja: con el modulo de 200 m las plazas ocupan buena parte
    # del pasillo y el hueco solo es salvable por un arco donde el pasillo va estrecho.
    crossing_fractions = [0.3, 0.42, 0.55, 0.68, 0.8]
    rng.shuffle(crossing_fractions)
    for crossed in order:
        for branch in order:
            if branch == crossed:
                continue
            for fraction in crossing_fractions:
                dir_a, dir_x = EXIT_DIR[branch], EXIT_DIR[crossed]
                p1 = scaled(EXIT_POINT[branch], float(rng.uniform(0.55, 0.75)))
                crossing = scaled(EXIT_POINT[crossed], fraction)
                p2 = scaled(EXIT_POINT[crossed], min(fraction + 0.15, 0.9))
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
    return [smooth_lane(lane) for lane in candidates]


def pick_rooms(rng: np.random.Generator, exits: tuple[str, ...], sunken_prob: float = 0.0,
               free_exit: str | None = None) -> list[Room]:
    """Plaza central y, por salida, otra a mitad de camino. Solo las laterales se hunden: la
    central es el cruce de todas las rutas y debe quedar a la cota del pasillo. free_exit
    queda sin plaza lateral: su tramo se reserva para un tunel."""
    rooms = [Room(0.0, 0.0, float(rng.uniform(45.0, 70.0)) * K)]
    for e in exits:
        if rng.random() < 0.7 and e != free_exit:
            ex, ey = EXIT_POINT[e]
            t = float(rng.uniform(0.42, 0.68))
            sunk = float(rng.uniform(*SUNKEN_DEPTH_M)) if rng.random() < sunken_prob else 0.0
            rooms.append(Room(ex * t, ey * t, float(rng.uniform(32.0, 58.0)) * K, sunk))
    return rooms


def room_mask(rooms: list[Room], feather: float = 6.0):
    mask = np.zeros_like(XX)
    for room in rooms:
        d = np.hypot(XX - room.x, YY - room.y)
        mask = np.maximum(mask, 1.0 - smoothstep(room.radius - feather, room.radius + feather, d))
    return mask


def pick_style(rng: np.random.Generator, biome: str) -> tuple[str, dict]:
    names = [n for n, s in STYLES.items() if s["biome"] == biome]
    weights = np.array([STYLES[n]["weight"] for n in names], dtype=float)
    name = names[int(rng.choice(len(names), p=weights / weights.sum()))]
    return name, STYLES[name]


def lane_frame(exit_side: str, wx, wy):
    """(a lo largo, a traves) del eje centro-salida, medidos en el espacio deformado."""
    dx, dy = EXIT_DIR[exit_side]
    return wx * dx + wy * dy, -wx * dy + wy * dx


def cliff_field(rng: np.random.Generator, exits: tuple[str, ...], wx, wy, hw):
    """Mascara 0..1 de un acantilado: un lado del pasillo de una salida, en un tramo de
    40-70 m, entre el talud y 30 m mas alla. None si el modulo no lleva acantilado."""
    exit_side = exits[int(rng.integers(len(exits)))]
    along, across = lane_frame(exit_side, wx, wy)
    side = 1.0 if rng.random() < 0.5 else -1.0
    center = float(rng.uniform(0.45, 0.7)) * HALF_M
    half_len = float(rng.uniform(20.0, 35.0))
    window = 1.0 - smoothstep(half_len, half_len + 12.0, np.abs(along - center))
    lateral = smoothstep(0.0, 4.0, side * across - hw * 0.6) * (1.0 - smoothstep(30.0, 42.0, side * across - hw))
    return window * lateral


def strata(heights, step_m: float):
    """Terrazas horizontales: la roca se lee por capas en vez de como una rampa lisa."""
    q = heights / step_m
    frac = q - np.floor(q)
    return (np.floor(q) + smoothstep(0.75, 1.0, frac)) * step_m


def biome_blend_field(rng: np.random.Generator):
    """Peso 0..1 del bioma secundario de un modulo mixto: frente difuso que cruza el modulo
    en una direccion al azar. Se apaga en la franja del borde (el borde es comun a todos)."""
    angle = float(rng.uniform(0.0, 2.0 * np.pi))
    ramp = (XX * math.cos(angle) + YY * math.sin(angle)) / HALF_M + 0.35 * fbm(rng, 60.0, octaves=2)
    return smoothstep(-0.3, 0.3, ramp)


def fort_fields(central: Room, d_main, hw):
    """(foso, murallita) alrededor de la plaza central de un fuerte. El pasillo atraviesa
    ambos: entra por un paso a la cota del suelo, como la puerta de un castillo de arena."""
    d = np.hypot(XX - central.x, YY - central.y)
    gate = smoothstep(hw - 2.0, hw + 3.0, d_main)
    moat = smoothstep(central.radius + 1.0, central.radius + 3.0, d) * (1.0 - smoothstep(central.radius + 8.0, central.radius + 11.0, d))
    wall = smoothstep(central.radius - 3.5, central.radius - 2.0, d) * (1.0 - smoothstep(central.radius - 0.5, central.radius + 0.5, d))
    return moat * gate, wall * gate


def foliage_density(rng: np.random.Generator, terrain, core, algae_weight, amount: float):
    """Densidad 0..1 del bosque de algas por vertice (la usa el tile para sembrar
    instancias). Matas agrupadas por ruido; nada en el nucleo del pasillo, bajo el agua ni
    en pendientes que no se podrian pisar."""
    if amount <= 0.0 or not np.any(algae_weight > 0.0):
        return np.zeros_like(XX)
    gx, gy = np.gradient(terrain, STEP_M)
    slope = np.degrees(np.arctan(np.hypot(gx, gy)))
    clumps = smoothstep(0.35, 0.7, fbm(rng, 30.0, octaves=2) * 0.5 + 0.5)
    open_ground = (1.0 - smoothstep(0.02, 0.3, core)) * (1.0 - smoothstep(28.0, 40.0, slope))
    dry = smoothstep(WATER_M + 0.2, WATER_M + 1.0, terrain)
    edge = smoothstep(FOLIAGE_EDGE_CLEAR_M, FOLIAGE_EDGE_CLEAR_M + 6.0, DIST_TO_EDGE)
    return np.clip(amount * (0.25 + 0.75 * clumps) * open_ground * dry * edge * algae_weight, 0.0, 1.0)


def pits(rng: np.random.Generator, rooms: list[Room], d_corr, hw, count_range: tuple[int, int] = (0, 2)):
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
    for _ in range(int(rng.integers(count_range[0], count_range[1] + 1))):
        cx, cy = rng.uniform(-0.7 * HALF_M, 0.7 * HALF_M, 2)
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


def place_arch(rng: np.random.Generator, terrain, d_main, bridges: list[Bridge], kind: str = "arch",
               rooms: list[Room] | None = None) -> Bridge | None:
    """Arco natural de pared a pared sobre el pasillo principal. Se busca un punto del eje
    lejos del borde y de otros puentes, se estima la direccion del pasillo por PCA de su eje
    cercano y se camina en perpendicular hasta coronar cada pared.

    kind "tunnel": el mismo arco, pero largo a lo largo del pasillo (TUNNEL_LENGTH_M). Exige
    un tramo recto (el eje no se aparta del tablero) sin plazas, y paredes que lleguen al
    techo en toda su longitud, para que no quede un extremo volando."""
    is_tunnel = kind == "tunnel"
    # Paso libre bajo la panza: el tile hunde el grosor bajo el techo (deck - 0,5 m).
    min_gap = ARCH_CLEARANCE_M + (TUNNEL_THICKNESS_M if is_tunnel else BRIDGE_THICKNESS_M) + 0.5
    along_len = float(rng.uniform(*TUNNEL_LENGTH_M)) if is_tunnel else float(rng.uniform(5.0, 10.0))
    margin = 30.0 + (along_len * 0.5 if is_tunnel else 0.0)
    axis_cells = np.argwhere((d_main < 1.5) & (DIST_TO_EDGE > margin))
    if len(axis_cells) == 0:
        return None
    for _ in range(30 if is_tunnel else 10):
        i, j = axis_cells[int(rng.integers(len(axis_cells)))]
        px, py = float(XX[i, j]), float(YY[i, j])
        if any(math.hypot(px - b.x, py - b.y) < 30.0 + along_len * 0.5 for b in bridges):
            continue
        if rooms and any(math.hypot(px - r.x, py - r.y) < r.radius + along_len * 0.5 + 6.0 for r in rooms):
            continue
        near = (np.hypot(XX - px, YY - py) < max(12.0, along_len * 0.5)) & (d_main < 1.5)
        xs, ys = XX[near] - px, YY[near] - py
        if len(xs) < 4:
            continue
        eigen_values, eigen_vectors = np.linalg.eigh(np.cov(np.stack([xs, ys])))
        tangent = eigen_vectors[:, int(np.argmax(eigen_values))]
        across = (-float(tangent[1]), float(tangent[0]))
        if is_tunnel and any(float(d_main[grid_index((px + tangent[0] * t, py + tangent[1] * t))]) > 7.0
                             for t in np.linspace(-along_len * 0.5, along_len * 0.5, 7)):
            continue   # el pasillo se curva dentro del tunel

        ground = float(terrain[i, j])
        reach = []
        for sign in (1.0, -1.0):
            best_t, best_h = None, -np.inf
            for t in np.arange(2.0, 60.0, 1.0):
                h = float(terrain[grid_index((px + sign * across[0] * t, py + sign * across[1] * t))])
                if h > best_h + 0.25:
                    best_t, best_h = t, h
                elif best_h >= ground + min_gap:
                    break   # coronada la pared: deja de subir
            reach.append((best_t, best_h))
        (t1, h1), (t2, h2) = reach
        deck = min(h1, h2)
        if t1 is None or t2 is None or deck - ground < min_gap:
            continue
        length = t1 + t2 + 2.0 * BRIDGE_OVERLAP_M
        cx = px + across[0] * (t1 - t2) * 0.5
        cy = py + across[1] * (t1 - t2) * 0.5
        yaw = math.degrees(math.atan2(across[1], across[0]))
        arch = Bridge(round(cx, 2), round(cy, 2), round(yaw, 1), round(length, 2),
                      round(along_len, 2), round(deck - 0.5, 2), kind)
        if not BRIDGE_MIN_M <= length <= BRIDGE_MAX_M or bridge_problem(terrain, [arch]):
            continue
        if is_tunnel and any(bridge_problem(terrain, [Bridge(round(cx + tangent[0] * t, 2), round(cy + tangent[1] * t, 2),
                                                             arch.yaw_deg, arch.length_m, 2.0, arch.deck_m)])
                             for t in (-along_len * 0.4, along_len * 0.4)):
            continue
        return arch
    return None


def place_monoliths(rng: np.random.Generator, terrain, d_main, hw, rooms: list[Room], bridges: list[Bridge],
                    count: int) -> list[Monolith]:
    """Pilares de roca como hitos: en el borde de las plazas (sin tapar el paso) o en la
    meseta. El pie se entierra 1 m bajo el punto mas bajo de su huella."""
    monoliths: list[Monolith] = []
    for _ in range(count * 6):
        if len(monoliths) >= count:
            break
        radius = float(rng.uniform(*MONOLITH_RADIUS_M))
        if rng.random() < 0.6 and rooms:
            room = rooms[int(rng.integers(len(rooms)))]
            angle = float(rng.uniform(0.0, 2.0 * np.pi))
            dist = room.radius * float(rng.uniform(0.55, 0.85))
            x, y = room.x + dist * math.cos(angle), room.y + dist * math.sin(angle)
            # En la plaza: fuera del carril central (60 % del semiancho local del pasillo).
            lane_clear = 0.6 * float(hw[grid_index((x, y))]) + radius + 3.0
        else:
            x, y = (float(v) for v in rng.uniform(-0.75 * HALF_M, 0.75 * HALF_M, 2))
            # En la meseta: mas alla del talud.
            lane_clear = float(hw[grid_index((x, y))]) + radius + 12.0
        if float(d_main[grid_index((x, y))]) < max(lane_clear, MONOLITH_LANE_CLEARANCE_M + radius):
            continue
        if DIST_TO_EDGE[grid_index((x, y))] < 16.0 + radius:
            continue
        if any(math.hypot(x - b.x, y - b.y) < b.length_m * 0.5 + radius + 6.0 for b in bridges):
            continue
        if any(math.hypot(x - m.x, y - m.y) < m.radius_m + radius + 10.0 for m in monoliths):
            continue
        footprint = np.hypot(XX - x, YY - y) < radius + STEP_M
        base = float(terrain[footprint].min()) - 1.0
        monoliths.append(Monolith(round(x, 2), round(y, 2), round(base, 2), round(radius, 2),
                                  round(float(rng.uniform(*MONOLITH_HEIGHT_M)), 2),
                                  round(float(rng.uniform(0.0, 360.0)), 1), round(float(rng.uniform(0.0, 7.0)), 1)))
    return monoliths


def walkable_exits_connected(meters: np.ndarray, exits: tuple[str, ...]) -> bool:
    """Todas las salidas abiertas se alcanzan a pie entre si: pasos de 2 m con desnivel
    <= WALKABLE_STEP_M. Los arcos no cuentan (son un extra, no la unica via)."""
    starts = [grid_index(scaled(EXIT_POINT[e], 0.99)) for e in exits]
    seen = np.zeros(meters.shape, dtype=bool)
    stack = [starts[0]]
    seen[starts[0]] = True
    while stack:
        i, j = stack.pop()
        for ni, nj in ((i + 1, j), (i - 1, j), (i, j + 1), (i, j - 1)):
            if 0 <= ni < RES and 0 <= nj < RES and not seen[ni, nj] \
                    and abs(meters[ni, nj] - meters[i, j]) <= WALKABLE_STEP_M:
                seen[ni, nj] = True
                stack.append((ni, nj))
    return all(seen[s] for s in starts)


def design_module(rng: np.random.Generator, exits: tuple[str, ...], biome: str, secondary: str):
    """Heightfield del interior (metros) antes de fundirlo con el borde canonico.

    biome manda en la forma (estilo); secondary != biome hace un modulo mixto: un frente
    difuso (blend) lleva hacia el otro bioma el color, el bosque de algas y el mar."""
    edge_fade = smoothstep(0.0, 50.0 * K, DIST_TO_EDGE)
    style_name, style = pick_style(rng, biome)
    is_causeway = bool(style["causeway"])
    wall_h = float(rng.uniform(*style["wall_h"]))
    blend = biome_blend_field(rng) if secondary != biome else np.zeros_like(XX)

    def weight_of(name: str):
        return (1.0 - blend) * float(biome == name) + blend * float(secondary == name)

    # Pasillo con serpenteo (deformacion del dominio) que se apaga junto al borde. Todas
    # las rutas se miden en el mismo espacio deformado, asi que casan entre si.
    warp_amp = float(rng.uniform(35.0, 65.0)) * K * smoothstep(0.0, 70.0 * K, DIST_TO_EDGE)
    wx = XX + warp_amp * fbm(rng, 180.0 * K, octaves=2)
    wy = YY + warp_amp * fbm(rng, 180.0 * K, octaves=2)

    # Bifurcacion: ensancha el tramo de su salida antes de medir el pasillo.
    fork = pick_fork(rng, exits)
    fork_widen, island = (fork_fields(rng, fork, wx, wy) if fork else (0.0, np.zeros_like(XX)))
    d_main = np.full_like(XX, np.inf)
    for exit_side, lane in zip(exits, main_lanes(exits)):
        d_lane = polyline_distance(wx, wy, lane.points)[0]
        if fork and exit_side == fork.exit:
            d_lane = d_lane - fork_widen
        d_main = np.minimum(d_main, d_lane)

    base_hw = float(rng.uniform(*style["corridor_hw"])) * K
    hw = base_hw * (1.0 + 0.3 * value_noise(rng, 110.0 * K))
    hw = OPEN_HALF_M + (hw - OPEN_HALF_M) * edge_fade   # en el borde, la boca canonica
    bank = float(rng.uniform(*style["bank"]))
    bank = BANK_M + (bank - BANK_M) * edge_fade

    # Acantilado: en su tramo el talud se vuelve casi vertical (ver la meseta mas abajo).
    cliff = cliff_field(rng, exits, wx, wy, hw) if rng.random() < style["cliff_prob"] else np.zeros_like(XX)
    cliff = cliff * edge_fade
    bank = bank * (1.0 - cliff) + CLIFF_BANK_M * cliff

    # El tunel se decide antes que las plazas: en 200 m no cabe junto a una plaza lateral.
    wants_tunnel = not is_causeway and rng.random() < style["tunnel_prob"]
    tunnel_exit = exits[int(rng.integers(len(exits)))] if wants_tunnel else None
    rooms = pick_rooms(rng, exits, style["sunken_prob"], tunnel_exit)
    rmask = room_mask(rooms)
    corridor = np.maximum(1.0 - smoothstep(hw, hw + bank, d_main), rmask)
    # Nucleo duro del pasillo (sin talud): la ruta alta corta el talud entero y acaba en
    # un labio casi vertical, y el tablero salva exactamente el hueco entre labios.
    core = np.maximum(1.0 - smoothstep(hw - 5.0, hw - 2.0, d_main), room_mask(rooms, 2.0))
    d_core = d_main - (hw - 2.0)
    for room in rooms:
        d_core = np.minimum(d_core, np.hypot(XX - room.x, YY - room.y) - room.radius)

    # Atajo: misma anchura relativa (con el mismo ruido) y mismo talud que el principal.
    shortcut = pick_shortcut(rng, exits, base_hw)
    if shortcut:
        d_sc = polyline_distance(wx, wy, shortcut.points)[0]
        sc_hw = np.minimum(hw * shortcut.half_width / base_hw, hw)
        corridor = np.maximum(corridor, 1.0 - smoothstep(sc_hw, sc_hw + bank, d_sc))
        core = np.maximum(core, 1.0 - smoothstep(sc_hw - 4.0, sc_hw - 1.0, d_sc))
        d_core = np.minimum(d_core, d_sc - (sc_hw - 1.0))

    # La isla de la bifurcacion se queda fuera del pasillo: toma la cota de la meseta (roca
    # alta) o se rebaja despues a una loma escalable.
    corridor = corridor * (1.0 - island)
    core = core * (1.0 - island)

    # Suelo: cota que ondula despacio y se aplana en las plazas; 0 junto a las salidas.
    # Una plaza hundida baja sunk_m con una rampa ancha (SUNKEN_FEATHER_M) que se camina.
    elev_amp = float(rng.uniform(4.0, 9.0))
    elev = elev_amp * fbm(rng, 260.0 * K, octaves=2) * edge_fade
    floor = elev.copy()
    # Fuera del bioma de agua la plaza hundida no baja del agua: seria un charco, no una hondonada.
    sunk_floor = WATER_M + 1.0 if biome != "water" else -np.inf
    sunken_rooms = set()
    for index, room in enumerate(rooms):
        i, j = grid_index((room.x, room.y))
        sunk = max(0.0, min(room.sunk_m, float(elev[i, j]) - sunk_floor))
        if sunk >= 2.0:
            sunken_rooms.add(index)
        else:
            sunk = 0.0
        feather = SUNKEN_FEATHER_M if sunk > 0.0 else 6.0
        m = 1.0 - smoothstep(room.radius - feather, room.radius + feather, np.hypot(XX - room.x, YY - room.y))
        floor = floor * (1.0 - m) + (elev[i, j] - sunk) * m
    flat_areas = [{"x_m": round(r.x, 2), "y_m": round(r.y, 2), "radius_m": round(r.radius, 2),
                   "height_m": round(float(floor[grid_index((r.x, r.y))]), 2), "sunken": k in sunken_rooms}
                  for k, r in enumerate(rooms)]
    floor += 0.30 * fbm(rng, 18.0, octaves=2)                                  # arena
    off_lane = smoothstep(8.0, 13.0, d_main) * (1.0 - 0.6 * rmask)
    rocks = float(rng.uniform(*style["rocks"])) * smoothstep(*style["rock_threshold"], value_noise(rng, 22.0) * 0.5 + 0.5) * off_lane
    floor += rocks * edge_fade
    if style["puddles"]:
        # Charcos de marisma: bajan por debajo del agua fuera del carril central.
        puddles = 5.5 * smoothstep(0.55, 0.75, value_noise(rng, 40.0) * 0.5 + 0.5) * off_lane * edge_fade
        floor -= puddles
    floor -= pits(rng, rooms, d_main, hw, style["pits"])
    # Fuera del agua, pozos y charcos no bajan de la cota del mar: serian lagunas en el desierto.
    floor = floor + (np.maximum(floor, WATER_M + 0.5) - floor) * (1.0 - weight_of("water"))

    # Meseta: cresta sobre el suelo local + colinas suaves. Variante calzada: exterior bajo
    # un agua poco profunda que se puede caminar.
    hills = (fbm(rng, 170.0 * K, octaves=3) * 0.5 + 0.5) * float(rng.uniform(*style["hills"]))
    sea = WATER_M - float(rng.uniform(*SEA_DEPTH_M)) + 0.5 * fbm(rng, 60.0 * K, octaves=2)
    if is_causeway:
        high = sea
    else:
        high = elev + wall_h + hills
        # Acantilado: pared mucho mas alta, en estratos.
        high = high + float(rng.uniform(*CLIFF_EXTRA_M)) * cliff
        high = high * (1.0 - 0.7 * cliff) + strata(high, CLIFF_STRATA_M) * 0.7 * cliff
        # Modulo mixto con agua: el lado del agua se inunda.
        if secondary != biome and "water" in (biome, secondary):
            water_w = weight_of("water")
            high = high * (1.0 - water_w) + sea * water_w
    # Junto al borde la meseta tiende a la cresta canonica; las salidas cerradas se tapan.
    high = CREST_M + (high - CREST_M) * edge_fade
    closed = np.zeros_like(XX, dtype=bool)
    for side, (ex, ey) in EXIT_POINT.items():
        if side in exits:
            continue
        along = np.abs(YY) if side in ("N", "S") else np.abs(XX)
        toward = np.hypot(XX - ex, YY - ey)
        closed |= (along < OPEN_HALF_M + BANK_M + 6.0) & (toward < 30.0)
    high = np.where(closed, np.maximum(high, PLUG_M), high)

    terrain = high * (1.0 - corridor) + floor * corridor
    if fork and fork.low:
        # Loma: poca altura y laderas suaves; se puede subir y cruzar por encima.
        terrain = terrain * (1.0 - island) + (floor + fork.low_height) * island
    elif fork and is_causeway:
        # En la calzada la meseta esta bajo el agua: la isla se levanta como roca propia.
        terrain = terrain * (1.0 - island) + (floor + wall_h) * island

    if style["fort"]:
        # Fuerte de arena: foso de agua y murallita alrededor de la plaza central.
        moat, rampart = fort_fields(rooms[0], d_main, hw)
        terrain = terrain * (1.0 - moat) + (WATER_M - 1.5) * moat
        terrain = terrain + 2.2 * rampart

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
            candidate = terrain * (1.0 - weight) + lane_height * weight
            # Misma validacion que la final, sobre el candidato: si el tablero no apoya
            # bien (p. ej. cae sobre la propia bajada de la ruta) se prueba otro trazado.
            if bridge_problem(blur(candidate), found):
                continue
            terrain = candidate
            bridges = found
            has_upper = True
            break

    terrain = blur(terrain)

    tunnel = None
    if wants_tunnel:
        tunnel = place_arch(rng, terrain, d_main, bridges, "tunnel", rooms)
        if tunnel:
            bridges = bridges + [tunnel]
    arch = None
    if not is_causeway and rng.random() < style["arch_prob"]:
        arch = place_arch(rng, terrain, d_main, bridges)
        if arch:
            bridges = bridges + [arch]

    monoliths = place_monoliths(rng, terrain, d_main, hw, rooms, bridges,
                                int(rng.integers(style["monoliths"][0], style["monoliths"][1] + 1)))
    foliage_amount = style["foliage"] if biome == "algae" else STYLES["kelp_forest"]["foliage"]
    foliage = foliage_density(rng, terrain, core, weight_of("algae"), foliage_amount)

    stats = {
        "style": style_name,
        "biome": biome,
        "secondary_biome": secondary,
        "causeway": is_causeway,
        "rooms": len(rooms),
        "sunken": len(sunken_rooms),
        "shortcut": shortcut is not None,
        "upper_route": has_upper,
        "fork": "none" if not fork else ("low" if fork.low else "high"),
        "arch": arch is not None,
        "tunnel": tunnel is not None,
        "cliff": bool(np.any(cliff > 0.5)),
        "wall_h": round(wall_h, 2),
        "base_half_width": round(base_hw, 2),
    }
    return terrain, stats, bridges, flat_areas, monoliths, blend, foliage


def encode_biome_mask(blend, foliage) -> np.ndarray:
    """Mascara de 16 bits: byte alto = peso del bioma secundario, byte bajo = densidad del
    bosque de algas. Mismo formato de PNG que el heightfield (lo lee el mismo importador)."""
    high = np.rint(np.clip(blend, 0.0, 1.0) * 255.0).astype(np.uint16)
    low = np.rint(np.clip(foliage, 0.0, 1.0) * 255.0).astype(np.uint16)
    return (high << 8) | low


def compose_module(seed: int, exits: tuple[str, ...], biome: str, secondary: str):
    rng = np.random.default_rng(seed)
    design, stats, bridges, flat_areas, monoliths, blend, foliage = design_module(rng, exits, biome, secondary)
    heights_m = design * (1.0 - BORDER_WEIGHT) + CANONICAL * BORDER_WEIGHT
    quantized = np.rint(heights_m * UNITS_PER_M) + HEIGHT_ZERO
    heights = np.clip(quantized, 0, 65535).astype(np.uint16)
    return heights, stats, bridges, flat_areas, monoliths, encode_biome_mask(blend, foliage)


def module_biomes(n: int, count: int) -> tuple[str, str]:
    """(bioma, secundario) del modulo n de una topologia. Los ultimos 3 * MIXED_PER_PAIR son
    mixtos (arena-agua, arena-algas, agua-algas); el resto se reparte a partes iguales."""
    pairs = [(a, b) for i, a in enumerate(BIOMES) for b in BIOMES[i + 1:]]
    mixed = min(len(pairs) * MIXED_PER_PAIR, count // 4)
    pure = count - mixed
    if n < pure:
        biome = BIOMES[n * len(BIOMES) // max(pure, 1)]
        return biome, biome
    a, b = pairs[min((n - pure) * len(pairs) // max(mixed, 1), len(pairs) - 1)]
    return a, b


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


def bridge_problem(meters: np.ndarray, bridges: list[Bridge]) -> str | None:
    """Bajo el tablero hay hueco (pasillo) y en ambos apoyos el terreno llega al tablero.
    Devuelve la descripcion del primer fallo, o None si todos los puentes son validos."""
    for b in bridges:
        ax, ay = math.cos(math.radians(b.yaw_deg)), math.sin(math.radians(b.yaw_deg))
        inner = int(b.length_m / 2.0 - BRIDGE_OVERLAP_M)
        under = min(meters[grid_index((b.x + ax * k, b.y + ay * k))] for k in range(-inner, inner + 1, 2))
        if b.deck_m - under < 4.0:
            return f"el puente en ({b.x}, {b.y}) no salva ningun hueco ({b.deck_m - under:.1f} m)"
        # Apoyos: mas alla de cada extremo, en algun punto del ancho del tablero, el terreno
        # llega al tablero (puede sobrepasarlo: el tablero se entierra un poco en la ladera).
        half_w = int(b.width_m / 2.0)
        for sign in (1.0, -1.0):
            reach = b.length_m / 2.0 + 3.0
            end = max(meters[grid_index((b.x + sign * ax * reach - ay * o, b.y + sign * ay * reach + ax * o))]
                      for o in range(-half_w, half_w + 1, 2))
            if end < b.deck_m - 4.0:
                return f"apoyo del puente a {end - b.deck_m:.1f} m del tablero"
    return None


def check_bridges(heights: np.ndarray, bridges: list[Bridge], name: str) -> None:
    meters = (heights.astype(np.float64) - HEIGHT_ZERO) / UNITS_PER_M
    problem = bridge_problem(meters, bridges)
    if problem:
        raise AssertionError(f"{name}: {problem}")


def slope_degrees(heights: np.ndarray) -> np.ndarray:
    meters = (heights.astype(np.float64) - HEIGHT_ZERO) / UNITS_PER_M
    gx, gy = np.gradient(meters, STEP_M)
    return np.degrees(np.arctan(np.hypot(gx, gy)))


# Colores de suelo por bioma en la hoja de contactos (orientativos; los de juego estan en C++).
PREVIEW_FLOOR = {"sand": (0.85, 0.70, 0.45), "water": (0.80, 0.74, 0.58), "algae": (0.42, 0.58, 0.30)}


def hillshade(heights: np.ndarray, bridges: list[Bridge], monoliths: list[Monolith], mask: np.ndarray,
              biome: str, secondary: str) -> np.ndarray:
    meters = (heights.astype(np.float64) - HEIGHT_ZERO) / UNITS_PER_M
    gx, gy = np.gradient(meters, STEP_M)
    nx, ny, nz = -gx, -gy, np.ones_like(gx)
    norm = np.sqrt(nx * nx + ny * ny + nz * nz)
    light = np.array([-0.5, 0.35, 0.79])
    shade = np.clip((nx * light[0] + ny * light[1] + nz * light[2]) / norm, 0.0, 1.0)
    # Tinte por cota y bioma: azul bajo el agua, suelo del bioma, oscuro en la meseta.
    blend = (mask >> 8).astype(np.float64) / 255.0
    foliage = (mask & 0xFF).astype(np.float64) / 255.0
    floor = np.array(PREVIEW_FLOOR[biome]) * (1 - blend)[..., None] + np.array(PREVIEW_FLOOR[secondary]) * blend[..., None]
    water = smoothstep(WATER_M + 1.0, WATER_M - 0.5, meters)
    high = smoothstep(4.0, 9.0, meters)[..., None]
    rgb = floor * (1 - high) + np.array([0.35, 0.32, 0.28]) * high
    rgb = rgb * (1 - 0.6 * foliage)[..., None] + np.array([0.08, 0.30, 0.10]) * (0.6 * foliage)[..., None]
    rgb = rgb * (0.35 + 0.65 * shade)[..., None]
    rgb = rgb * (1 - 0.7 * water)[..., None] + np.array([0.15, 0.35, 0.65]) * (0.7 * water)[..., None]
    for monolith in monoliths:
        rgb[np.hypot(XX - monolith.x, YY - monolith.y) <= monolith.radius_m] = np.array([0.2, 0.15, 0.3])
    for bridge in bridges:
        ax, ay = math.cos(math.radians(bridge.yaw_deg)), math.sin(math.radians(bridge.yaw_deg))
        along = (XX - bridge.x) * ax + (YY - bridge.y) * ay
        across = -(XX - bridge.x) * ay + (YY - bridge.y) * ax
        deck = (np.abs(along) <= bridge.length_m / 2.0) & (np.abs(across) <= bridge.width_m / 2.0 + 1.0)
        rgb[deck] = np.array([0.95, 0.95, 0.85])
    return (np.clip(rgb, 0, 1) * 255).astype(np.uint8)


def write_contact_sheet(path: Path, thumbs: list[np.ndarray], columns: int = 10, size: int = 120) -> None:
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
    parser.add_argument("--count", type=int, default=100, help="modulos por topologia")
    parser.add_argument("--out", type=Path, default=OUTPUT_DIR)
    parser.add_argument("--thumb", type=int, default=120, help="lado de cada miniatura de la hoja de contactos, en px")
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
    rejected = 0
    for topology, exits in TOPOLOGIES.items():
        folder = args.out / topology
        folder.mkdir(parents=True, exist_ok=True)
        thumbs = []
        for n in range(args.count):
            name = f"M_{topology}_{n + 1:02d}"
            biome, secondary = module_biomes(n, args.count)
            # Un diseno que deja alguna salida sin acceso a pie se descarta y se prueba la
            # semilla siguiente; la semilla final queda en el manifest para reproducirlo.
            for attempt in range(8):
                seed = BASE_SEED + index * 16 + attempt
                heights, stats, bridges, flat_areas, monoliths, mask = compose_module(seed, exits, biome, secondary)
                meters = (heights.astype(np.float64) - HEIGHT_ZERO) / UNITS_PER_M
                # La fusion con el borde canonico puede dejar sin apoyo un arco validado
                # antes de fundir: tambien se descarta.
                if walkable_exits_connected(meters, exits) and bridge_problem(meters, bridges) is None:
                    break
                rejected += 1
            else:
                raise AssertionError(f"{name}: ninguna semilla da un diseno accesible y con arcos apoyados")
            index += 1
            check_border(heights, expected_edge, name)
            check_bridges(heights, bridges, name)
            Image.fromarray(heights).save(folder / f"{name}.png")
            Image.fromarray(mask).save(folder / f"{name}_mask.png")
            thumbs.append(hillshade(heights, bridges, monoliths, mask, biome, secondary))
            slopes = slope_degrees(heights)
            manifest["modules"].append({
                "name": name,
                "topology": topology,
                "seed": seed,
                "file": f"{topology}/{name}.png",
                "mask_file": f"{topology}/{name}_mask.png",
                "min_m": round(float(meters.min()), 2),
                "max_m": round(float(meters.max()), 2),
                "slope_p99_deg": round(float(np.percentile(slopes, 99)), 1),
                "bridges": [{"x_m": b.x, "y_m": b.y, "yaw_deg": b.yaw_deg, "length_m": b.length_m,
                             "width_m": b.width_m, "deck_m": b.deck_m, "kind": b.kind,
                             "thickness_m": TUNNEL_THICKNESS_M if b.kind == "tunnel" else BRIDGE_THICKNESS_M}
                            for b in bridges],
                "monoliths": [{"x_m": m.x, "y_m": m.y, "base_m": m.base_m, "radius_m": m.radius_m,
                               "height_m": m.height_m, "yaw_deg": m.yaw_deg, "lean_deg": m.lean_deg}
                              for m in monoliths],
                "flat_areas": flat_areas,
                **stats,
            })
        write_contact_sheet(args.out / f"preview_{topology}.png", thumbs, size=args.thumb)
        print(f"{topology}: {args.count} modulos")

    (args.out / "manifest.json").write_text(json.dumps(manifest, indent=1), encoding="utf-8")
    mods = manifest["modules"]
    print(f"total {len(mods)} modulos; cota [{min(m['min_m'] for m in mods)}, {max(m['max_m'] for m in mods)}] m; "
          f"pendiente p99 max {max(m['slope_p99_deg'] for m in mods)} grados; "
          f"atajos {sum(m['shortcut'] for m in mods)}; rutas altas {sum(m['upper_route'] for m in mods)}; "
          f"puentes {sum(b['kind'] == 'bridge' for m in mods for b in m['bridges'])}; "
          f"arcos {sum(m['arch'] for m in mods)}; tuneles {sum(m['tunnel'] for m in mods)}; "
          f"monolitos {sum(len(m['monoliths']) for m in mods)}; acantilados {sum(m['cliff'] for m in mods)}; "
          f"plazas hundidas {sum(m['sunken'] for m in mods)}; "
          f"bifurcaciones {sum(m['fork'] != 'none' for m in mods)} (lomas {sum(m['fork'] == 'low' for m in mods)}); "
          f"disenos descartados {rejected}; "
          f"biomas " + ", ".join(f"{b} {sum(m['biome'] == b and m['secondary_biome'] == b for m in mods)}" for b in BIOMES)
          + f", mixtos {sum(m['biome'] != m['secondary_biome'] for m in mods)}; "
          f"estilos " + ", ".join(f"{n} {sum(m['style'] == n for m in mods)}" for n in STYLES))


if __name__ == "__main__":
    main()
