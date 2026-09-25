"""Genera un MAPA PREPARADO: un unico terreno continuo para todo el grid, cortado en un
modulo por celda. Los modulos casan exactamente (comparten las muestras del borde), asi
que no hace falta ni borde canonico ni fusion; los disenadores editan cada celda.

Se ejecuta FUERA del editor:
    uv run --with numpy --with pillow --with scipy --with scikit-image \
        python Scripts/gen_terrain_preset.py [--seed N] [--name Mapa01] [--grid 6]
        [--min-path 9] [--max-path 12]

Escribe en Scripts/terrain_presets/<name>/:
    Cells/M_<name>_r<fila>c<col>.png (+ _mask, _coast)   heightfields de 101 x 101
    manifest.json    formato de gen_terrain_modules.py + bloque "preset" con las celdas
    preview.png      vista cenital sombreada del mapa entero

Lo importa Scripts/import_terrain_modules.py con
    TN_MODULES_DIR=Scripts/terrain_presets/<name>  TN_MODULES_ROOT=/Game/Terrain/Presets/<name>
y, al ver el bloque "preset", deja BP_GridMapGenerator con PresetCells.

Diseno (todo en metros, X = Norte, Y = Este, como el generador de mapa). El relieve manda
y el camino se adapta a el:
  1. Camino de celdas: uno solo, de la primera fila a la ultima (sin desvios). Fija por
     que celdas pasa el recorrido de juego.
  2. Relieve a dos escalas (lomas y dorsales grandes, dunas y rugosidad pequena), con
     coordenadas deformadas para que no se note la rejilla del ruido.
  3. Camino: ruta de coste minimo por el relieve dentro de sus celdas (evita pendientes
     y cimas). Su perfil sigue el relieve suavizado y el terreno forma un valle alrededor,
     con laderas cuyo ancho crece con el desnivel; algun tramo corto es acantilado de arena.
  4. Zona jugable: la silueta la marca la distancia al camino, no la cuadricula.
  5. Charcos y lagos en las cuencas reales del relieve: la orilla es una curva de nivel.
     Uno queda junto al camino; donde el camino cruza agua, se vadea.
  6. Caminitos: rutas de coste minimo que salen del camino, pasan por un punto a un lado
     y vuelven, evitando el agua, el propio camino y los caminitos anteriores.
  7. Fuera de la zona jugable y en el borde del mapa: cordon de dunas altas (sin vacio).
Sin tuneles.
"""

from __future__ import annotations

import argparse
import json
import math
import shutil
from pathlib import Path
from typing import NamedTuple

import numpy as np
from PIL import Image
from scipy import ndimage
from skimage.graph import route_through_array

from terrain_gen.core import (HEIGHT_ZERO, RES, SIZE_M, UNITS_PER_M, UU_PER_M, HEIGHT_SCALE_UU, WATER_M,
                              WALKABLE_STEP_M, smoothstep)
from terrain_gen.features import chaikin, sharp_strata

OUTPUT_ROOT = Path(__file__).resolve().parent / "terrain_presets"
STEP_M = SIZE_M / (RES - 1)
COARSE = 2                      # las busquedas de ruta van sobre una rejilla de 4 m
WADE_DEPTH_M = 0.8              # profundidad maxima del agua sobre el camino


# ── Ruido sobre una rejilla arbitraria ─────────────────────────────────────────────
def value_noise(rng: np.random.Generator, px, py, wavelength: float):
    extent = max(float(px.max() - px.min()), float(py.max() - py.min()))
    cells = int(math.ceil(extent / wavelength)) + 4
    lattice = rng.uniform(-1.0, 1.0, (cells, cells))
    u = (px - px.min()) / wavelength + 1.0
    v = (py - py.min()) / wavelength + 1.0
    i0 = np.clip(np.floor(u).astype(int), 0, cells - 2)
    j0 = np.clip(np.floor(v).astype(int), 0, cells - 2)
    fu = smoothstep(0.0, 1.0, u - i0)
    fv = smoothstep(0.0, 1.0, v - j0)
    a, b = lattice[i0, j0], lattice[i0 + 1, j0]
    c, d = lattice[i0, j0 + 1], lattice[i0 + 1, j0 + 1]
    return (a * (1 - fu) + b * fu) * (1 - fv) + (c * (1 - fu) + d * fu) * fv


def fbm(rng, px, py, wavelength: float, octaves: int = 3, gain: float = 0.5):
    total = np.zeros_like(px)
    amplitude, norm = 1.0, 0.0
    for octave in range(octaves):
        total += amplitude * value_noise(rng, px, py, wavelength / (2 ** octave))
        norm += amplitude
        amplitude *= gain
    return total / norm


def unit_fbm(rng, px, py, wavelength: float, octaves: int = 2):
    """fbm llevado a [0, 1]."""
    return fbm(rng, px, py, wavelength, octaves) * 0.5 + 0.5


def blur(field, passes: int = 1):
    kernel = np.array([1.0, 4.0, 6.0, 4.0, 1.0]) / 16.0
    out = field
    rows, cols = field.shape
    for _ in range(passes):
        padded = np.pad(out, 2, mode="edge")
        tmp = sum(kernel[k] * padded[k:k + rows, 2:2 + cols] for k in range(5))
        padded = np.pad(tmp, 2, mode="edge")
        out = sum(kernel[k] * padded[2:2 + rows, k:k + cols] for k in range(5))
    return out


def slope_of(height):
    gx, gy = np.gradient(height, STEP_M)
    return np.hypot(gx, gy)


# ── Geometria de polilineas ───────────────────────────────────────────────────────
def index_of(p, half: float, step: float = STEP_M) -> tuple[int, int]:
    return int(round((p[0] + half) / step)), int(round((p[1] + half) / step))


def polyline_field(polylines, shape, half: float):
    """(distancia en m a las polilineas, parametro de arco del punto mas cercano)."""
    free = np.ones(shape, dtype=bool)
    arc = np.zeros(shape)
    for points in polylines:
        cum = 0.0
        for a, b in zip(points[:-1], points[1:]):
            length = math.dist(a, b)
            t = np.linspace(0.0, 1.0, max(2, int(length / (STEP_M * 0.25)) + 1))
            i = np.clip(np.rint((a[0] + (b[0] - a[0]) * t + half) / STEP_M).astype(int), 0, shape[0] - 1)
            j = np.clip(np.rint((a[1] + (b[1] - a[1]) * t + half) / STEP_M).astype(int), 0, shape[1] - 1)
            free[i, j] = False
            arc[i, j] = cum + t * length
            cum += length
    dist, (ii, jj) = ndimage.distance_transform_edt(free, return_indices=True)
    return dist * STEP_M, arc[ii, jj]


def point_at(points, s: float):
    cum = 0.0
    for a, b in zip(points[:-1], points[1:]):
        length = math.dist(a, b)
        if cum + length >= s:
            t = (s - cum) / max(length, 1e-9)
            return (a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t), ((b[0] - a[0]) / max(length, 1e-9), (b[1] - a[1]) / max(length, 1e-9))
        cum += length
    a, b = points[-2], points[-1]
    length = max(math.dist(a, b), 1e-9)
    return b, ((b[0] - a[0]) / length, (b[1] - a[1]) / length)


def arc_length(points) -> float:
    return sum(math.dist(a, b) for a, b in zip(points[:-1], points[1:]))


# ── Rutas de coste minimo ─────────────────────────────────────────────────────────
def least_cost_route(cost_coarse, a, b, half: float):
    """Puntos (m) de la ruta de coste minimo entre a y b, o None si no hay paso."""
    step = STEP_M * COARSE
    indices, total = route_through_array(cost_coarse, index_of(a, half, step), index_of(b, half, step),
                                         fully_connected=True, geometric=True)
    if not math.isfinite(total):
        return None
    points = [(i * step - half, j * step - half) for i, j in indices]
    points[0], points[-1] = a, b
    return points


def smooth_route(points, stride: int = 5):
    """Quita el dentado de la rejilla: submuestrea y redondea conservando los extremos."""
    kept = points[::stride]
    if kept[-1] != points[-1]:
        kept.append(points[-1])
    return chaikin(tuple(kept), 3)


def meander(rng, points, amplitude: float, wavelength: float):
    """Serpenteo lateral suave (dos senos de fase aleatoria) que se anula en los extremos:
    quita los tramos rectos y las esquinas a 45/90 grados que deja la rejilla de busqueda."""
    length = arc_length(points)
    ss = np.arange(0.0, length, 4.0)
    if len(ss) < 4:
        return points
    offsets = np.zeros_like(ss)
    for factor in (1.0, 0.45):
        period = wavelength * factor * float(rng.uniform(0.8, 1.25))
        offsets += factor * np.sin(2.0 * math.pi * ss / period + float(rng.uniform(0.0, 2.0 * math.pi)))
    offsets *= amplitude * smoothstep(0.0, 30.0, ss) * smoothstep(length, length - 30.0, ss)
    out = []
    for s, o in zip(ss, offsets):
        (x, y), (dx, dy) = point_at(points, float(s))
        out.append((x - dy * o, y + dx * o))
    out.append(points[-1])
    out[0] = points[0]
    return chaikin(tuple(out), 2)


def near_penalty(shape_coarse, points, half: float, weight: float, radius: float):
    step = STEP_M * COARSE
    free = np.ones(shape_coarse, dtype=bool)
    for p in points:
        i, j = index_of(p, half, step)
        if 0 <= i < shape_coarse[0] and 0 <= j < shape_coarse[1]:
            free[i, j] = False
    dist = ndimage.distance_transform_edt(free) * step
    return weight * (1.0 - smoothstep(0.0, radius, dist))


# ── Camino de celdas ──────────────────────────────────────────────────────────────
def generate_path(rng: np.random.Generator, grid: int, min_len: int, max_len: int) -> list[tuple[int, int]]:
    """Camino autoevitante (col, fila) de la fila 0 a la ultima, de longitud [min, max]."""
    for _ in range(200):
        start = (int(rng.integers(grid)), 0)
        path = [start]
        seen = {start}

        def search() -> bool:
            col, row = path[-1]
            if row == grid - 1:
                return min_len <= len(path) <= max_len
            moves = [(0, 1), (1, 0), (-1, 0), (0, -1)]
            rng.shuffle(moves)
            for dc, dr in moves:
                nxt = (col + dc, row + dr)
                if not (0 <= nxt[0] < grid and 0 <= nxt[1] < grid) or nxt in seen:
                    continue
                if (grid - 1 - nxt[1]) > max_len - (len(path) + 1):
                    continue
                path.append(nxt)
                seen.add(nxt)
                if search():
                    return True
                path.pop()
                seen.discard(nxt)
            return False

        if search():
            return path
    raise RuntimeError("sin camino para esos parametros")


# ── Relieve ───────────────────────────────────────────────────────────────────────
def build_relief(rng, XX, YY):
    """Lomas y dorsales grandes + dunas + rugosidad, sobre coordenadas deformadas."""
    WX = XX + 60.0 * fbm(rng, XX, YY, 180.0, 2)
    WY = YY + 60.0 * fbm(rng, XX, YY, 180.0, 2)
    broad = unit_fbm(rng, WX, WY, 320.0, 4)
    ridge_zone = smoothstep(0.4, 0.7, unit_fbm(rng, WX, WY, 420.0))
    ridged = 1.0 - np.abs(fbm(rng, WX, WY, 150.0, 3))
    # Dunas: crestas asimetricas dobladas por una segunda deformacion, solo en parte del mapa.
    wind = float(rng.uniform(0.0, 2.0 * math.pi))
    along = WX * math.cos(wind) + WY * math.sin(wind) + 25.0 * fbm(rng, WX, WY, 90.0, 2)
    phase = along / float(rng.uniform(34.0, 44.0))
    frac = phase - np.floor(phase)
    dunes = smoothstep(0.0, 1.0, np.where(frac < 0.7, frac / 0.7, (1.0 - frac) / 0.3))
    dunes = 1.6 * dunes * smoothstep(0.45, 0.75, unit_fbm(rng, WX, WY, 200.0))
    fine = 0.5 * fbm(rng, WX, WY, 24.0, 2)
    # Mesetas: escalones de 4-7 m donde un ruido lento cruza unos umbrales; el escalon es
    # estrecho (acantilado de arena que no se sube) y sigue un contorno organico.
    mesa_zone = smoothstep(0.25, 0.5, unit_fbm(rng, WX, WY, 380.0))
    mesa_field = fbm(rng, WX, WY, 220.0, 3)
    mesas = np.zeros_like(XX)
    for threshold in (-0.15, 0.05, 0.25):
        mesas += float(rng.uniform(4.0, 7.0)) * smoothstep(threshold - 0.012, threshold + 0.012, mesa_field)
    return 2.0 + 20.0 * broad + 28.0 * ridge_zone * ridged ** 1.5 + dunes + fine + mesas * mesa_zone


# ── Camino principal y su valle ───────────────────────────────────────────────────
def route_main(rng, relief, XX, YY, cells, centers, grid: int, half: float):
    """Ruta de coste minimo por el relieve, restringida a las celdas del camino."""
    row = np.clip(np.floor((XX + half) / SIZE_M).astype(int), 0, grid - 1)
    col = np.clip(np.floor((YY + half) / SIZE_M).astype(int), 0, grid - 1)
    allowed = np.zeros(XX.shape, dtype=bool)
    for c, r in cells:
        allowed |= (row == r) & (col == c)
    # Entre celdas vecinas del camino que no son consecutivas no se pasa: el recorrido las
    # visita todas en orden.
    for k1, (c1, r1) in enumerate(cells):
        for c2, r2 in cells[k1 + 2:]:
            if abs(c1 - c2) + abs(r1 - r2) != 1:
                continue
            if r1 == r2:
                allowed &= ~((np.abs(YY - (max(c1, c2) * SIZE_M - half)) < 4.0) & (row == r1))
            else:
                allowed &= ~((np.abs(XX - (max(r1, r2) * SIZE_M - half)) < 4.0) & (col == c1))
    margin = ndimage.distance_transform_edt(allowed) * STEP_M
    # Un ruido lento de preferencia hace que serpentee tambien donde el relieve es suave.
    cost = (1.0 + 80.0 * slope_of(relief) ** 2 + 0.03 * (relief - relief.min())
            + 6.0 * (1.0 - smoothstep(0.0, 35.0, margin)) + 10.0 * unit_fbm(rng, XX, YY, 110.0, 3))
    cost = np.where(allowed, cost, np.inf)[::COARSE, ::COARSE]
    # Un punto de paso desplazado en cada celda intermedia: en los tramos rectos del
    # recorrido el camino hace eses en vez de ir en linea recta.
    waypoints = [centers[0]]
    for prev, (x, y), nxt in zip(centers[:-2], centers[1:-1], centers[2:]):
        dx, dy = nxt[0] - prev[0], nxt[1] - prev[1]
        norm = math.hypot(dx, dy)
        offset = float(rng.uniform(-45.0, 45.0))
        waypoints.append((x - dy / norm * offset, y + dx / norm * offset))
    waypoints.append(centers[-1])
    route = [waypoints[0]]
    for a, b in zip(waypoints[:-1], waypoints[1:]):
        leg = least_cost_route(cost, a, b, half)
        if leg is None:
            raise RuntimeError("el camino no encuentra paso por sus celdas")
        route += leg[1:]
    return meander(rng, smooth_route(route, 16), 8.0, 140.0)


class Canyon(NamedTuple):
    terrain: np.ndarray
    d_main: np.ndarray       # distancia al camino (m)
    corridor: np.ndarray     # 1 en el suelo transitable (pasillo, plazas, canoncitos)
    flat_areas: list         # plazas (x, y, radio)


def floor_profile(rng, path_len: float):
    """Cota del suelo del pasillo cada 4 m: ondula despacio entre 0 y 5 m, como la cota del
    suelo de los modulos. Con cotas bajas el suelo se pinta de arena y la meseta, mas alta,
    con la paleta de los montones."""
    ss = np.arange(0.0, path_len + 4.0, 4.0)
    knots = rng.uniform(0.0, 5.0, int(path_len / 150.0) + 3)
    heights = np.interp(ss / 150.0, np.arange(len(knots)), knots)
    for _ in range(3):
        heights = np.convolve(np.pad(heights, 7, mode="edge"), np.ones(15) / 15.0, mode="valid")
    return ss, heights


def plan_lakes(rng, path_points, count: int):
    """Lagos a un lado del camino: (x, y, radio) de la plaza que los rodea."""
    path_len = arc_length(path_points)
    lakes = []
    for k in range(count):
        s = path_len * (0.15 + 0.7 * (k + float(rng.uniform(0.2, 0.8))) / count)
        (x, y), (dx, dy) = point_at(path_points, s)
        side = 1.0 if rng.random() < 0.5 else -1.0
        radius = float(rng.uniform(38.0, 55.0))
        offset = radius + float(rng.uniform(8.0, 20.0))
        lakes.append((x - dy * side * offset, y + dx * side * offset, radius))
    return lakes


def play_region(rng, XX, YY, d_main):
    """< 0 dentro de la zona jugable; su contorno sigue al camino con un ancho variable."""
    reach = 115.0 + 35.0 * unit_fbm(rng, XX, YY, 300.0)
    return d_main + 35.0 * fbm(rng, XX, YY, 140.0, 3) - reach


def disc_mask(XX, YY, discs, feather: float, grow: float = 0.0):
    mask = np.zeros_like(XX)
    for x, y, r in discs:
        mask = np.maximum(mask, 1.0 - smoothstep(r + grow - feather, r + grow + feather, np.hypot(XX - x, YY - y)))
    return mask


def blob_mask(rng, XX, YY, discs, feather: float):
    """Como disc_mask, pero con contorno irregular: una plaza no es un circulo."""
    mask = np.zeros_like(XX)
    for x, y, r in discs:
        d = np.hypot(XX - x, YY - y) * (1.0 + 0.3 * fbm(rng, XX, YY, 0.9 * r, 2))
        mask = np.maximum(mask, 1.0 - smoothstep(r - feather, r + feather, d))
    return mask


def build_canyon(rng, XX, YY, path_points, trails, lakes, plaza_count: int, half: float) -> Canyon:
    """Perfil de los modulos: pasillo de 28-48 m entre paredes de arena de 5-8 m de talud,
    meseta alta con colinas detras (repisas de arenisca en parte), plazas llanas, caminitos
    como canoncitos que se meten en la meseta y lagos en plazas laterales."""
    shape = XX.shape
    d_main, s_main = polyline_field([path_points], shape, half)
    path_len = arc_length(path_points)
    ss, profile = floor_profile(rng, path_len)
    # El parametro de arco salta en las curvas cerradas: la cota se suaviza en 2D.
    floor = blur(np.interp(s_main, ss, profile), 10)

    # Pasillo con gargantas: a tramos se estrecha a la mitad.
    gorge = smoothstep(0.62, 0.75, unit_fbm(rng, XX, YY, 300.0))
    hw = (14.0 + 10.0 * unit_fbm(rng, XX, YY, 160.0)) * (1.0 - 0.5 * gorge)
    bank = 5.0 + 3.0 * unit_fbm(rng, XX, YY, 90.0)
    corridor = 1.0 - smoothstep(hw, hw + bank, d_main)

    # Plazas llanas junto al camino (puzles).
    flat_areas = []
    for k in range(plaza_count):
        s = float(np.clip(path_len * (k + 0.5) / plaza_count + rng.uniform(-30.0, 30.0), 0.0, path_len))
        (px, py), (dx, dy) = point_at(path_points, s)
        shift = float(rng.uniform(-15.0, 15.0))
        flat_areas.append((px - dy * shift, py + dx * shift, float(rng.uniform(28.0, 42.0))))
    corridor = np.maximum(corridor, blob_mask(rng, XX, YY, flat_areas, 6.0))
    corridor = np.maximum(corridor, blob_mask(rng, XX, YY, lakes, 6.0))

    # Caminitos: canoncitos estrechos que se meten en la meseta y vuelven.
    if trails:
        d_trail = polyline_field(trails, shape, half)[0]
        trail_hw = 3.5 + 1.5 * unit_fbm(rng, XX, YY, 80.0)
        corridor = np.maximum(corridor, 1.0 - smoothstep(trail_hw, trail_hw + 4.0, d_trail))

    # Meseta: cresta sobre el suelo + colinas; repisas de arenisca en parte del mapa.
    wall = 5.0 + 6.0 * unit_fbm(rng, XX, YY, 200.0)
    hills = unit_fbm(rng, XX, YY, 170.0, 3) * (6.0 + 22.0 * unit_fbm(rng, XX, YY, 500.0))
    high = blur(floor, 10) + wall + hills + 0.5 * fbm(rng, XX, YY, 24.0, 2)
    terraced = smoothstep(0.45, 0.6, unit_fbm(rng, XX, YY, 260.0))
    high = high * (1.0 - terraced) + sharp_strata(high, float(rng.uniform(2.5, 3.5))) * terraced
    terrain = high * (1.0 - corridor) + (floor + 0.3 * fbm(rng, XX, YY, 18.0, 2)) * corridor

    # Lagos: mancha irregular dentro de su plaza, lejos del camino; orilla de arena.
    for x, y, r in lakes:
        d = np.hypot(XX - x, YY - y) / r + 0.25 * fbm(rng, XX, YY, 0.8 * r, 2)
        lake = (1.0 - smoothstep(0.55, 0.8, d)) * smoothstep(hw.min() + 4.0, hw.min() + 12.0, d_main)
        terrain = terrain * (1.0 - lake) + (WATER_M - 1.0 - 1.2 * lake) * lake
    return Canyon(terrain, d_main, corridor, flat_areas)


def plan_trail_spans(rng, path_len: float, count: int):
    """Tramos del camino (s1, s2) de los que sale y a los que vuelve cada caminito."""
    spans = []
    for k in range(count):
        s1 = (0.03 + 0.8 * (k + float(rng.uniform(0.0, 1.0))) / count) * path_len
        s2 = min(s1 + float(rng.uniform(140.0, 280.0)), path_len * 0.97)
        if s2 - s1 >= 100.0:
            spans.append((s1, s2))
    return spans


def route_trails(rng, terrain, XX, YY, d_main, inside, path_points, spans, half: float, blocked=None):
    """Caminitos: del camino a un punto a un lado y de vuelta, por donde menos cuesta.
    blocked (0-1) encarece las zonas reservadas, como los lagos."""
    base = (1.0 + 60.0 * slope_of(terrain) ** 2 + 40.0 * (terrain < WATER_M + 0.3)
            + 40.0 * (1.0 - smoothstep(10.0, 55.0, d_main)) + 6.0 * unit_fbm(rng, XX, YY, 70.0, 1))
    if blocked is not None:
        base = base + 200.0 * blocked
    base = np.where(inside < -15.0, base, np.inf)[::COARSE, ::COARSE]
    avoid = np.zeros_like(base)
    trails = []
    for s1, s2 in spans:
        for _attempt in range(6):
            p1, _ = point_at(path_points, s1)
            p2, _ = point_at(path_points, s2)
            mid, direction = point_at(path_points, (s1 + s2) / 2.0)
            side = 1.0 if rng.random() < 0.5 else -1.0
            offset = float(rng.uniform(65.0, 110.0))
            waypoint = (mid[0] - direction[1] * side * offset, mid[1] + direction[0] * side * offset)
            wi, wj = index_of(waypoint, half, STEP_M * COARSE)
            if not (0 <= wi < base.shape[0] and 0 <= wj < base.shape[1]) or not math.isfinite(base[wi, wj]):
                continue
            going = least_cost_route(base + avoid, p1, waypoint, half)
            if going is None:
                continue
            back = least_cost_route(base + avoid + near_penalty(base.shape, going, half, 30.0, 20.0),
                                    waypoint, p2, half)
            if back is None:
                continue
            raw = going + back[1:]
            trail = meander(rng, smooth_route(raw, 10), 4.0, 90.0)
            # Un caminito que va pegado al camino se lee como un segundo camino: se descarta.
            if np.mean([d_main[index_of(q, half)] < 25.0 for q in trail]) > 0.3:
                continue
            trails.append(trail)
            avoid = avoid + near_penalty(base.shape, raw, half, 25.0, 20.0)
            break
    return trails


def raise_rim(rng, terrain, XX, YY, inside, grid: int, half: float):
    """Exteriores: un cordon de dunas cierra la zona jugable y, mas alla, lagunas de mar,
    dunas altas o mesetas segun la semilla. Junto a una laguna el cordon es bajo (se ve el
    agua desde el camino). El borde del mapa siempre es un cordon de dunas."""
    lo, hi = -half, grid * SIZE_M - half
    edge = np.minimum(np.minimum(XX - lo, hi - XX), np.minimum(YY - lo, hi - YY))
    edge = edge + 45.0 * fbm(rng, XX, YY, 130.0, 3)
    sea = smoothstep(0.5, 0.62, unit_fbm(rng, XX, YY, 420.0, 2)) * smoothstep(60.0, 110.0, edge)
    rim = smoothstep(0.0, 70.0, inside)
    high = 20.0 + 8.0 * unit_fbm(rng, XX, YY, 60.0)
    low = 4.0 + 4.0 * unit_fbm(rng, XX, YY, 60.0)
    rim_height = high * (1.0 - sea) + low * sea
    terrain = terrain * (1.0 - rim) + np.maximum(terrain, rim_height) * rim
    beyond = smoothstep(70.0, 150.0, inside) * sea
    sea_floor = WATER_M - 1.5 - 1.0 * unit_fbm(rng, XX, YY, 80.0)
    terrain = terrain * (1.0 - beyond) + sea_floor * beyond
    border = smoothstep(80.0, 10.0, edge)
    return terrain * (1.0 - border) + np.maximum(terrain, high) * border


def path_mask(rng, terrain, XX, YY, d_main, trails, half: float):
    """Mascara de bioma (16 bits) que marca el camino y los caminitos.

    Byte alto: peso de la paleta secundaria (algas), que tine el camino de arena pisada,
    mas oscura. Byte bajo (bosque de algas): 0, las matas del bosque se leen como postes.
    Nada de eso en el agua."""
    d_trail = polyline_field(trails, terrain.shape, half)[0] if trails else np.full_like(terrain, np.inf)
    road = 3.5 + 1.0 * fbm(rng, XX, YY, 50.0, 2)
    tint = np.maximum(0.75 * (1.0 - smoothstep(road - 1.0, road + 1.5, d_main)),
                      0.55 * (1.0 - smoothstep(0.8, 2.5, d_trail)))
    tint = tint * (0.85 + 0.15 * unit_fbm(rng, XX, YY, 12.0))
    dry = smoothstep(WATER_M + 0.2, WATER_M + 0.8, terrain)
    high = np.rint(np.clip(tint * dry, 0.0, 1.0) * 255.0).astype(np.uint16)
    return high << 8


def tread(terrain, d_main, trails, half: float):
    """Huella del paso: el camino se hunde 0,3 m y los caminitos 0,2 m, con hombro suave."""
    d_trail = polyline_field(trails, terrain.shape, half)[0] if trails else np.full_like(terrain, np.inf)
    sink = np.maximum(0.3 * (1.0 - smoothstep(3.0, 5.5, d_main)), 0.2 * (1.0 - smoothstep(1.2, 3.0, d_trail)))
    return terrain - sink * smoothstep(WATER_M + 0.2, WATER_M + 0.8, terrain)


def reachable(terrain, start, goal) -> bool:
    """Del inicio al final a pie (desnivel entre muestras vecinas <= WALKABLE_STEP_M)."""
    seen = np.zeros(terrain.shape, dtype=bool)
    stack = [start]
    seen[start] = True
    rows, cols = terrain.shape
    while stack:
        i, j = stack.pop()
        for ni, nj in ((i + 1, j), (i - 1, j), (i, j + 1), (i, j - 1)):
            if 0 <= ni < rows and 0 <= nj < cols and not seen[ni, nj] \
                    and abs(terrain[ni, nj] - terrain[i, j]) <= WALKABLE_STEP_M:
                seen[ni, nj] = True
                stack.append((ni, nj))
    return bool(seen[goal])


def main() -> None:
    parser = argparse.ArgumentParser(description="Genera un mapa preparado (terreno continuo cortado en modulos).")
    parser.add_argument("--seed", type=int, default=20260924)
    parser.add_argument("--name", default="Mapa01")
    parser.add_argument("--grid", type=int, default=6)
    parser.add_argument("--min-path", type=int, default=9)
    parser.add_argument("--max-path", type=int, default=12)
    args = parser.parse_args()

    rng = np.random.default_rng(args.seed)
    grid = args.grid
    half = SIZE_M / 2.0
    samples = grid * (RES - 1) + 1
    axis = np.linspace(-half, grid * SIZE_M - half, samples)
    XX, YY = np.meshgrid(axis, axis, indexing="ij")     # X = fila (Norte), Y = columna (Este)

    cells = generate_path(rng, grid, args.min_path, args.max_path)
    centers = [(row * SIZE_M, col * SIZE_M) for col, row in cells]
    relief = build_relief(rng, XX, YY)
    path_points = route_main(rng, relief, XX, YY, cells, centers, grid, half)
    path_len = arc_length(path_points)
    d_main = polyline_field([path_points], XX.shape, half)[0]
    inside = play_region(rng, XX, YY, d_main)
    lakes = plan_lakes(rng, path_points, int(rng.integers(2, 4)))
    spans = plan_trail_spans(rng, path_len, max(4, round(len(cells) * 0.6)))
    trails = route_trails(rng, relief, XX, YY, d_main, inside, path_points, spans, half,
                          blocked=disc_mask(XX, YY, lakes, 6.0, grow=10.0))
    canyon = build_canyon(rng, XX, YY, path_points, trails, lakes, max(3, len(cells) // 3), half)
    flat_areas = canyon.flat_areas
    terrain = tread(blur(raise_rim(rng, canyon.terrain, XX, YY, inside, grid, half)), d_main, trails, half)

    if not reachable(terrain, index_of(centers[0], half), index_of(centers[-1], half)):
        raise AssertionError("el final no se alcanza a pie desde el inicio")
    path_depth = WATER_M - min(float(terrain[index_of(point_at(path_points, s)[0], half)])
                               for s in np.arange(0.0, arc_length(path_points), 2.0))
    if path_depth > WADE_DEPTH_M + 0.3:
        raise AssertionError(f"el camino queda {path_depth:.1f} m bajo el agua")

    # Corte por celdas.
    out = OUTPUT_ROOT / args.name
    if out.exists():
        shutil.rmtree(out)
    (out / "Cells").mkdir(parents=True)
    quantized = np.clip(np.rint(terrain * UNITS_PER_M) + HEIGHT_ZERO, 0, 65535).astype(np.uint16)
    mask = path_mask(rng, terrain, XX, YY, d_main, trails, half)
    zeros = np.zeros((RES, RES), dtype=np.uint16)
    manifest = {"size_uu": SIZE_M * UU_PER_M, "resolution": RES, "height_scale_uu": HEIGHT_SCALE_UU,
                "height_zero": HEIGHT_ZERO, "modules": [], "preset": {"name": args.name, "seed": args.seed, "cells": []}}
    path_set = set(cells)
    for row in range(grid):
        for col in range(grid):
            name = f"M_{args.name}_r{row}c{col}"
            block = quantized[row * (RES - 1):row * (RES - 1) + RES, col * (RES - 1):col * (RES - 1) + RES]
            Image.fromarray(block).save(out / "Cells" / f"{name}.png")
            Image.fromarray(mask[row * (RES - 1):row * (RES - 1) + RES, col * (RES - 1):col * (RES - 1) + RES]).save(
                out / "Cells" / f"{name}_mask.png")
            Image.fromarray(zeros).save(out / "Cells" / f"{name}_coast.png")
            cx, cy = row * SIZE_M, col * SIZE_M
            local_flats = [{"x_m": round(px - cx, 2), "y_m": round(py - cy, 2), "radius_m": round(r, 2),
                            "height_m": round(float(terrain[index_of((px, py), half)]), 2), "sunken": False}
                           for px, py, r in flat_areas if abs(px - cx) < half and abs(py - cy) < half]
            manifest["modules"].append({
                "name": name, "topology": "Cross", "folder": "Cells", "edges": ["crest"] * 4, "seed": args.seed,
                "file": f"Cells/{name}.png", "mask_file": f"Cells/{name}_mask.png", "coast_file": f"Cells/{name}_coast.png",
                "biome": "sand", "secondary_biome": "algae", "bridges": [], "monoliths": [], "flat_areas": local_flats,
            })
            # Lados que dan fuera del mapa (N 1, E 2, S 4, O 8): caja invisible.
            outer = (1 if row == grid - 1 else 0) | (2 if col == grid - 1 else 0) | (4 if row == 0 else 0) | (8 if col == 0 else 0)
            manifest["preset"]["cells"].append({"name": name, "col": col, "row": row, "outer_sides": outer,
                                                "on_path": (col, row) in path_set,
                                                "is_start": (col, row) == cells[0], "is_end": (col, row) == cells[-1]})
    (out / "manifest.json").write_text(json.dumps(manifest, indent=1), encoding="utf-8")

    # Vista cenital sombreada (Norte arriba).
    gx, gy = np.gradient(terrain, STEP_M)
    shade = np.clip((gx * 0.5 - gy * 0.35 + 1.0) / np.sqrt(gx * gx + gy * gy + 1.0) * 0.8, 0.0, 1.0)
    sand = np.array([0.85, 0.70, 0.45])
    rgb = sand * (0.35 + 0.65 * shade)[..., None]
    water = smoothstep(WATER_M + 0.3, WATER_M - 0.3, terrain)[..., None]
    rgb = rgb * (1 - 0.75 * water) + np.array([0.15, 0.4, 0.7]) * 0.75 * water
    Image.fromarray((np.clip(rgb, 0, 1) * 255).astype(np.uint8)[::-1]).save(out / "preview.png")
    # La misma vista con el camino (rojo), los caminitos (naranja), la zona fuera de juego
    # (oscurecida) y curvas de nivel cada 2 m.
    d_trail = polyline_field(trails, terrain.shape, half)[0] if trails else np.full_like(terrain, np.inf)
    debug = rgb * (1.0 - 0.35 * smoothstep(0.0, 40.0, inside))[..., None]
    contour = (np.floor(terrain / 2.0) != np.floor(np.roll(terrain, 1, 0) / 2.0)) \
        | (np.floor(terrain / 2.0) != np.floor(np.roll(terrain, 1, 1) / 2.0))
    debug = np.where(contour[..., None], debug * 0.85, debug)
    debug = np.where((d_trail < 2.0)[..., None], np.array([0.95, 0.55, 0.1]), debug)
    debug = np.where((d_main < 3.0)[..., None], np.array([0.85, 0.15, 0.1]), debug)
    Image.fromarray((np.clip(debug, 0, 1) * 255).astype(np.uint8)[::-1]).save(out / "preview_debug.png")

    print(f"{args.name}: camino de {len(cells)} celdas {cells}; {len(trails)} caminitos; {len(lakes)} lagos; "
          f"{len(flat_areas)} plazas; cota [{terrain.min():.1f}, {terrain.max():.1f}] m; {grid * grid} modulos en {out}")


if __name__ == "__main__":
    main()
