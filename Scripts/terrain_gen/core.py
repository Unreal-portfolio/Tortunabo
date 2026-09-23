"""Geometria, codificacion, ruido, bordes y validacion comunes a todos los modulos."""

from __future__ import annotations

import math
from dataclasses import dataclass
from pathlib import Path

import numpy as np


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
MOUTH_TOLERANCE_M = 1.6        # desnivel admitido en la boca de una salida, en el borde

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
TUNNEL_MAX_LENGTH_M = 80.0       # de pared a pared (la cueva va dentro de una colina)
# Monolito: pilar de roca (malla del tile, no cabe en un heightfield de 2 m).
MONOLITH_RADIUS_M = (2.5, 5.0)
MONOLITH_HEIGHT_M = (10.0, 26.0)
MONOLITH_LANE_CLEARANCE_M = 7.0  # distancia minima del pie al eje del pasillo
# Zona hundida: plaza lateral rebajada, con rampas caminables alrededor.
SUNKEN_DEPTH_M = (3.5, 6.0)
SUNKEN_FEATHER_M = 11.0          # media anchura de la rampa: 6 m en 11 m -> ~28 grados
# Acantilado: tramo de pared mucho mas alta y casi vertical, con estratos.
CLIFF_EXTRA_M = (3.0, 6.0)         # 2026-09-23 (noche): mas bajos (antes 5-10 m)
CLIFF_BANK_M = 8.0                  # cara de arena suelta (~45-55 grados): no se sube a pie
CLIFF_STRATA_M = 3.0

# ── F4: biomas ────────────────────────────────────────────────────────────────────
WATER_M = -4.0                   # cota del agua (ModuleWaterLevel del generador = -400 uu)
SEA_DEPTH_M = (1.0, 2.0)         # agua poco profunda: se camina por el fondo
BIOMES = ("sand", "water", "algae")
MIXED_PER_PAIR = 5               # modulos mixtos por pareja de biomas y topologia
FOLIAGE_EDGE_CLEAR_M = 6.0       # sin algas en el nucleo del pasillo ni en su borde inmediato

# ── Tipos de borde (cada lado del modulo lleva uno; dos vecinos casan si comparten tipo) ─
# crest: cresta de CREST_M con boca de pasillo (el borde de siempre).
# open:  llano a cota 0 con dunas suaves; se cruza por cualquier punto (explanada).
# water: fondo de mar poco profundo; se vadea por cualquier punto.
# Los tres perfiles valen CREST_M a partir de CORNER_FROM_M: en las esquinas, donde se
# tocan lados de tipos distintos, todos coinciden y no hay costura.
EDGE_TYPES = ("crest", "open", "water")
CORNER_FROM_M = 86.0
SEA_EDGE_M = WATER_M - 1.5
OPEN_EDGE_BASE_M = 3.0             # cota media del borde "open" (lomas de 0 a 6 m)
SIDES = ("N", "E", "S", "W")

# ── Variedad ─────────────────────────────────────────────────────────────────────
# Cada parametro principal de un modulo toma uno de LEVELS valores fijos (con un poco de
# jitter dentro del escalon). La libreria reparte los niveles como un hipercubo latino:
# en cada topologia todos los niveles de cada parametro salen el mismo numero de veces y
# las combinaciones no se repiten, en vez de amontonarse como con un sorteo uniforme.
LEVELS = 7
LEVEL_KEYS = ("wall_h", "corridor_hw", "bank", "hills", "warp", "elev", "dunes", "gate")

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

OUTPUT_DIR = Path(__file__).resolve().parent.parent / "terrain_modules"   # Scripts/terrain_modules

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


def edge_profile(kind: str, t_abs):
    """Altura del borde de un lado de tipo kind en funcion de |t| (0 en el centro del lado)."""
    if kind == "crest":
        return canonical_profile(t_abs)
    corner = smoothstep(CORNER_FROM_M - 30.0, CORNER_FROM_M, t_abs)
    if kind == "open":
        # Lomas simetricas fijas (0-6 m): el relieve de las explanadas cruza el borde sin
        # escalon ni valle recto. Pendiente maxima ~23 grados: se cruza por cualquier punto.
        hills = OPEN_EDGE_BASE_M + 2.2 * np.cos(2.0 * np.pi * t_abs / 52.0) + 0.8 * np.cos(2.0 * np.pi * t_abs / 21.0)
        return hills * (1.0 - corner) + CREST_M * corner
    if kind == "water":
        return SEA_EDGE_M + (CREST_M - SEA_EDGE_M) * corner
    raise ValueError(f"tipo de borde desconocido: {kind}")


def border_field(edges: dict[str, str]):
    """Perfil del lado mas cercano, con el tipo de cada lado (N, E, S, W)."""
    if all(kind == "crest" for kind in edges.values()):
        return CANONICAL
    d_n, d_s = HALF_M - XX, HALF_M + XX
    d_e, d_w = HALF_M - YY, HALF_M + YY
    nearest = np.argmin(np.stack([d_n, d_e, d_s, d_w]), axis=0)
    field = np.zeros_like(XX)
    for index, side in enumerate(SIDES):
        t_abs = np.abs(YY) if side in ("N", "S") else np.abs(XX)
        field = np.where(nearest == index, edge_profile(edges[side], t_abs), field)
    return field


def natural_ridge(rng: np.random.Generator, dist_to_side, low_m, reach_m: float = 58.0):
    """(altura, peso 0..1) de un cordon de dunas que cierra el agua contra un lado cresta.

    La linea de costa y la cima serpentean (hasta ~18 m), y la cima (CREST_M + 2-7 m)
    tambien: el cordon no se lee como una recta sobre la linea de la celda. Llega al borde
    tal cual; la fusion de bordes del tile lo casa con el vecino. peso = 0 mar adentro, 1
    en el cordon."""
    shore = dist_to_side + 18.0 * fbm(rng, 55.0, octaves=2) * smoothstep(4.0, 14.0, dist_to_side)
    peak = CREST_M + 2.0 + 5.0 * (fbm(rng, 45.0, octaves=2) * 0.5 + 0.5)
    rise = smoothstep(reach_m, 22.0, shore) ** 1.6
    height = low_m + (peak - low_m) * rise + 1.2 * fbm(rng, 40.0, octaves=2) * smoothstep(34.0, 12.0, shore)
    return height, smoothstep(reach_m + 6.0, reach_m - 22.0, shore)


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


# ── Validacion y salida ───────────────────────────────────────────────────────────
def edge_vector(kind: str = "crest") -> np.ndarray:
    values = np.rint(edge_profile(kind, np.abs(_AXIS)) * UNITS_PER_M) + HEIGHT_ZERO
    return values.astype(np.uint16)


def canonical_edge_vector() -> np.ndarray:
    return edge_vector("crest")


def check_border(heights: np.ndarray, edges: dict[str, str], name: str) -> None:
    """Cada lado reproduce exactamente el perfil de su tipo, y todos los perfiles son
    simetricos y coinciden en las esquinas."""
    rows = {"S": heights[0, :], "N": heights[-1, :], "W": heights[:, 0], "E": heights[:, -1]}
    for side, row in rows.items():
        expected = edge_vector(edges[side])
        if not np.array_equal(row, expected):
            bad = int(np.argmax(row != expected))
            raise AssertionError(f"{name}: lado {side} ({edges[side]}) rompe su borde en la muestra {bad}")
    corners = {int(edge_vector(kind)[0]) for kind in EDGE_TYPES}
    if len(corners) != 1 or any(not np.array_equal(edge_vector(k), edge_vector(k)[::-1]) for k in EDGE_TYPES):
        raise AssertionError("los perfiles de borde no son simetricos o no casan en las esquinas")


def check_mouths(heights: np.ndarray, exits: tuple[str, ...], edges: dict[str, str], name: str) -> None:
    """Los bordes son libres (la fusion de bordes del tile casa dos vecinos cualesquiera);
    solo se exige que cada salida de tierra llegue al borde a la cota del camino, para que
    el pasillo siga en el modulo de al lado. Las salidas por un lado de agua se vadean."""
    meters = (heights.astype(np.float64) - HEIGHT_ZERO) / UNITS_PER_M
    rows = {"S": meters[0, :], "N": meters[-1, :], "W": meters[:, 0], "E": meters[:, -1]}
    mouth = np.abs(_AXIS) <= OPEN_HALF_M - 4.0
    for side in exits:
        if edges.get(side) == "water":
            continue
        worst = float(np.max(np.abs(rows[side][mouth])))
        if worst > MOUTH_TOLERANCE_M:
            raise AssertionError(f"{name}: la boca {side} no llega al borde a cota de camino ({worst:.1f} m)")


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
