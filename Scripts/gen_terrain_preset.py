"""Genera un MAPA PREPARADO: un unico terreno continuo para todo el grid, cortado en un
modulo por celda. Los modulos casan exactamente (comparten las muestras del borde), asi
que no hace falta ni borde canonico ni fusion; los disenadores editan cada celda.

Se ejecuta FUERA del editor:
    uv run --with numpy --with pillow python Scripts/gen_terrain_preset.py [--seed N]
        [--name Mapa01] [--grid 6] [--min-path 9] [--max-path 12]

Escribe en Scripts/terrain_presets/<name>/:
    Cells/M_<name>_r<fila>c<col>.png (+ _mask, _coast)   heightfields de 101 x 101
    manifest.json    formato de gen_terrain_modules.py + bloque "preset" con las celdas
    preview.png      vista cenital sombreada del mapa entero

Lo importa Scripts/import_terrain_modules.py con
    TN_MODULES_DIR=Scripts/terrain_presets/<name>  TN_MODULES_ROOT=/Game/Terrain/Presets/<name>
y, al ver el bloque "preset", deja BP_GridMapGenerator con PresetCells.

Diseno (todo en metros, X = Norte, Y = Este, como el generador de mapa):
  1. Camino de celdas: uno solo, de la primera fila a la ultima (sin desvios).
  2. Curva suave por las celdas, con los puntos de control desplazados dentro de cada
     celda: el camino no sigue la cuadricula.
  3. Caminitos: senderos estrechos que salen del camino, dan un rodeo y vuelven.
  4. Relieve: dunas y lomas por todo el mapa, montes en algunas zonas; el camino y los
     caminitos se hunden en el. Taludes estrechos (acantilados de arena) o anchos (se
     sube a las lomas) segun un ruido.
  5. Charcos y embalses de costa natural; alguno sobre el propio camino (se vadea).
  6. Borde del mapa: un cordon de dunas altas de contorno irregular (sin vacio).
Sin tuneles.
"""

from __future__ import annotations

import argparse
import json
import math
import shutil
from pathlib import Path

import numpy as np
from PIL import Image

from terrain_gen.core import (HEIGHT_ZERO, RES, SIZE_M, UNITS_PER_M, UU_PER_M, HEIGHT_SCALE_UU, WATER_M,
                              WALKABLE_STEP_M, smoothstep)
from terrain_gen.features import chaikin

OUTPUT_ROOT = Path(__file__).resolve().parent / "terrain_presets"
STEP_M = SIZE_M / (RES - 1)


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


def polyline_distance(px, py, points):
    """(distancia, parametro de arco del punto mas cercano)."""
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
    return best_d, best_s


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

    # 1-2. Camino y curva suave.
    cells = generate_path(rng, grid, args.min_path, args.max_path)
    centers = [(row * SIZE_M, col * SIZE_M) for col, row in cells]
    controls = [centers[0]]
    for index in range(1, len(centers) - 1):
        x, y = centers[index]
        controls.append((x + float(rng.uniform(-40.0, 40.0)), y + float(rng.uniform(-40.0, 40.0))))
    controls.append(centers[-1])
    # Puntos intermedios en cada frontera de celda, desplazados: la curva no la cruza por el medio.
    dense = [controls[0]]
    for a, b in zip(controls[:-1], controls[1:]):
        mid = ((a[0] + b[0]) / 2.0, (a[1] + b[1]) / 2.0)
        normal = (-(b[1] - a[1]), b[0] - a[0])
        norm = max(math.hypot(*normal), 1e-9)
        shift = float(rng.uniform(-45.0, 45.0))
        dense += [(mid[0] + normal[0] / norm * shift, mid[1] + normal[1] / norm * shift), b]
    # Inicio y final en el centro de su celda (ahi se colocan los PlayerStart).
    path_points = chaikin(tuple(dense), 4)
    path_points = (centers[0],) + path_points[1:-1] + (centers[-1],)
    path_len = arc_length(path_points)

    # 3. Caminitos: rodeos que salen del camino y vuelven.
    trails = []
    for _ in range(int(len(cells) * 1.3)):
        s1 = float(rng.uniform(0.0, max(path_len - 130.0, 1.0)))
        s2 = min(s1 + float(rng.uniform(120.0, 300.0)), path_len)
        p1, d1 = point_at(path_points, s1)
        p2, _ = point_at(path_points, s2)
        side = 1.0 if rng.random() < 0.5 else -1.0
        normal = (-d1[1] * side, d1[0] * side)
        bulge = float(rng.uniform(60.0, 140.0))
        mids = []
        for t in (0.3, 0.55, 0.8):
            base = (p1[0] + (p2[0] - p1[0]) * t, p1[1] + (p2[1] - p1[1]) * t)
            out = bulge * math.sin(math.pi * t) * float(rng.uniform(0.7, 1.3))
            jitter = rng.uniform(-18.0, 18.0, 2)
            mids.append((base[0] + normal[0] * out + jitter[0], base[1] + normal[1] * out + jitter[1]))
        trails.append(chaikin((p1, *mids, p2), 4))

    # 4. Relieve.
    broad = fbm(rng, XX, YY, 260.0, 3) * 0.5 + 0.5
    mountain_zone = smoothstep(0.3, 0.6, fbm(rng, XX, YY, 360.0, 2) * 0.5 + 0.5)
    ridged = 1.0 - np.abs(fbm(rng, XX, YY, 120.0, 3))
    wind = float(rng.uniform(0.0, 2.0 * math.pi))
    along = XX * math.cos(wind) + YY * math.sin(wind)
    phase = (along + 22.0 * fbm(rng, XX, YY, 70.0, 2)) / float(rng.uniform(26.0, 34.0))
    frac = phase - np.floor(phase)
    dunes = np.where(frac < 0.75, frac / 0.75, (1.0 - frac) / 0.25)
    dunes = 2.4 * smoothstep(0.0, 1.0, dunes) * smoothstep(0.25, 0.75, fbm(rng, XX, YY, 110.0, 2) * 0.5 + 0.5)
    # Lomas por todo el mapa y montes de hasta ~30 m en algunas zonas.
    relief = 5.0 + 14.0 * broad + 30.0 * mountain_zone * ridged ** 1.5 + dunes

    d_main, _ = polyline_distance(XX, YY, path_points)
    d_trail = np.full_like(XX, np.inf)
    for trail in trails:
        d_trail = np.minimum(d_trail, polyline_distance(XX, YY, trail)[0])

    floor = 1.2 * fbm(rng, XX, YY, 300.0, 2)
    hw = 8.0 + 6.0 * (fbm(rng, XX, YY, 150.0, 2) * 0.5 + 0.5)
    gentle = fbm(rng, XX, YY, 110.0, 2) * 0.5 + 0.5
    bank = 5.0 + 20.0 * smoothstep(0.35, 0.75, gentle)            # 5 m = acantilado de arena
    corridor = 1.0 - smoothstep(hw, hw + bank, d_main)
    trail_bank = 3.0 + 8.0 * smoothstep(0.3, 0.8, gentle)
    trail = 1.0 - smoothstep(3.5, 3.5 + trail_bank, d_trail)
    # Plazas a lo largo del camino (zonas llanas para puzles).
    plazas = np.zeros_like(XX)
    flat_areas = []
    for k in range(max(3, len(cells) // 3)):
        s = path_len * (k + 0.5) / max(3, len(cells) // 3)
        (px, py), _ = point_at(path_points, s)
        radius = float(rng.uniform(15.0, 22.0))
        plazas = np.maximum(plazas, 1.0 - smoothstep(radius - 5.0, radius + 5.0, np.hypot(XX - px, YY - py)))
        flat_areas.append((px, py, radius))
    lowland = np.maximum(np.maximum(corridor, trail), plazas)
    terrain = relief * (1.0 - lowland) + floor * lowland

    # 5. Charcos y embalses: costa ancha y ruidosa (arena que baja al agua).
    ponds = []
    on_path = int(rng.integers(1, 3))
    for k in range(int(rng.integers(5, 9))):
        if k < on_path:
            (cx, cy), _ = point_at(path_points, float(rng.uniform(0.2, 0.85)) * path_len)
            depth = float(rng.uniform(0.6, 0.9))                 # se vadea
            radius = float(rng.uniform(35.0, 50.0))
        else:
            cx = float(rng.uniform(0.0, (grid - 1) * SIZE_M))
            cy = float(rng.uniform(0.0, (grid - 1) * SIZE_M))
            depth = float(rng.uniform(4.0, 7.0))
            radius = float(rng.uniform(45.0, 85.0))
        # Contorno irregular y orilla de ancho variable (playa corta o larga).
        d = np.hypot(XX - cx, YY - cy) * (1.0 + 0.35 * fbm(rng, XX, YY, 0.9 * radius, 2))
        # Cuenca ancha: el terreno baja poco a poco hasta el fondo (playa de arena), con
        # una orilla mas corta o mas larga segun el charco.
        shore = float(rng.uniform(1.2, 2.0))
        weight = 1.0 - smoothstep(radius * 0.3, radius * shore, d)
        terrain = terrain * (1.0 - weight) + (WATER_M - depth) * weight
        ponds.append((cx, cy, radius))

    # 6. Borde del mapa: cordon de dunas altas de contorno irregular.
    lo, hi = -half, grid * SIZE_M - half
    edge = np.minimum(np.minimum(XX - lo, hi - XX), np.minimum(YY - lo, hi - YY))
    edge = edge + 45.0 * fbm(rng, XX, YY, 130.0, 3)
    rim = smoothstep(80.0, 10.0, edge)
    rim_height = 20.0 + 8.0 * (fbm(rng, XX, YY, 60.0, 2) * 0.5 + 0.5)
    terrain = terrain * (1.0 - rim) + np.maximum(terrain, rim_height) * rim
    terrain = blur(terrain)

    # Validacion: del centro de la celda de inicio al de la final, a pie.
    def index_of(p):
        return (int(round((p[0] + half) / STEP_M)), int(round((p[1] + half) / STEP_M)))

    start, goal = index_of(centers[0]), index_of(centers[-1])
    seen = np.zeros(terrain.shape, dtype=bool)
    stack = [start]
    seen[start] = True
    while stack:
        i, j = stack.pop()
        for ni, nj in ((i + 1, j), (i - 1, j), (i, j + 1), (i, j - 1)):
            if 0 <= ni < samples and 0 <= nj < samples and not seen[ni, nj] \
                    and abs(terrain[ni, nj] - terrain[i, j]) <= WALKABLE_STEP_M:
                seen[ni, nj] = True
                stack.append((ni, nj))
    if not seen[goal]:
        raise AssertionError("el final no se alcanza a pie desde el inicio")

    # Corte por celdas.
    out = OUTPUT_ROOT / args.name
    if out.exists():
        shutil.rmtree(out)
    (out / "Cells").mkdir(parents=True)
    quantized = np.clip(np.rint(terrain * UNITS_PER_M) + HEIGHT_ZERO, 0, 65535).astype(np.uint16)
    zeros = np.zeros((RES, RES), dtype=np.uint16)
    manifest = {"size_uu": SIZE_M * UU_PER_M, "resolution": RES, "height_scale_uu": HEIGHT_SCALE_UU,
                "height_zero": HEIGHT_ZERO, "modules": [], "preset": {"name": args.name, "seed": args.seed, "cells": []}}
    path_set = set(cells)
    for row in range(grid):
        for col in range(grid):
            name = f"M_{args.name}_r{row}c{col}"
            block = quantized[row * (RES - 1):row * (RES - 1) + RES, col * (RES - 1):col * (RES - 1) + RES]
            Image.fromarray(block).save(out / "Cells" / f"{name}.png")
            Image.fromarray(zeros).save(out / "Cells" / f"{name}_mask.png")
            Image.fromarray(zeros).save(out / "Cells" / f"{name}_coast.png")
            cx, cy = row * SIZE_M, col * SIZE_M
            local_flats = [{"x_m": round(px - cx, 2), "y_m": round(py - cy, 2), "radius_m": round(r, 2),
                            "height_m": round(float(terrain[index_of((px, py))]), 2), "sunken": False}
                           for px, py, r in flat_areas if abs(px - cx) < half and abs(py - cy) < half]
            manifest["modules"].append({
                "name": name, "topology": "Cross", "folder": "Cells", "edges": ["crest"] * 4, "seed": args.seed,
                "file": f"Cells/{name}.png", "mask_file": f"Cells/{name}_mask.png", "coast_file": f"Cells/{name}_coast.png",
                "biome": "sand", "secondary_biome": "sand", "bridges": [], "monoliths": [], "flat_areas": local_flats,
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

    print(f"{args.name}: camino de {len(cells)} celdas {cells}; {len(trails)} caminitos; {len(ponds)} charcos "
          f"({on_path} en el camino); cota [{terrain.min():.1f}, {terrain.max():.1f}] m; {grid * grid} modulos en {out}")


if __name__ == "__main__":
    main()
