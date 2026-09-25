"""Utilidades de los mapas preparados: camino de celdas, ruido sobre una rejilla arbitraria,
comprobacion de paso a pie y escritura del preset (PNG por celda, manifest y vistas).

Un preset es un terreno continuo del mapa entero cortado en un modulo por celda, en
Scripts/terrain_presets/<nombre>/, que importa Scripts/import_terrain_modules.py con
    TN_MODULES_DIR=Scripts/terrain_presets/<nombre>  TN_MODULES_ROOT=/Game/Terrain/Presets/<nombre>
y, al ver el bloque "preset" del manifest, deja BP_GridMapGenerator con PresetCells.
"""

from __future__ import annotations

import json
import math
import shutil
from pathlib import Path

import numpy as np
from PIL import Image

from .core import HEIGHT_SCALE_UU, HEIGHT_ZERO, RES, SIZE_M, UNITS_PER_M, UU_PER_M, WALKABLE_STEP_M, WATER_M, smoothstep

OUTPUT_ROOT = Path(__file__).resolve().parent.parent / "terrain_presets"   # Scripts/terrain_presets
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


def unit_fbm(rng, px, py, wavelength: float, octaves: int = 2):
    """fbm llevado a [0, 1]."""
    return fbm(rng, px, py, wavelength, octaves) * 0.5 + 0.5

def index_of(p, half: float, step: float = STEP_M) -> tuple[int, int]:
    return int(round((p[0] + half) / step)), int(round((p[1] + half) / step))

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

def write_preset(name: str, seed: int, grid: int, terrain, mask, cells, flat_areas, bridges_by_cell=None,
                 secondary: str = "algae", shade=None, overlay=(), cell_biomes=None, cell_layers=None) -> Path:
    """Corta el terreno en una celda por modulo y escribe PNG, manifest y vistas cenitales.

    terrain y mask cubren el mapa entero (grid * (RES - 1) + 1 muestras por lado). flat_areas
    en metros de mundo (x, y, radio). bridges_by_cell: {(col, fila): [dict del manifest con
    coordenadas locales]}. shade (0-1) oscurece la vista de depuracion (zona fuera de juego);
    overlay: [(mascara booleana o None, color RGB)] pintados encima. cell_biomes: {(col, fila):
    (bioma, secundario)} (por defecto arena con secondary). cell_layers: {(col, fila): (techo,
    boveda)} arrays uint16 de RES x RES ya codificados (0 = sin techo) del tunel en capas."""
    half = SIZE_M / 2.0
    out = OUTPUT_ROOT / name
    if out.exists():
        shutil.rmtree(out)
    (out / "Cells").mkdir(parents=True)
    quantized = np.clip(np.rint(terrain * UNITS_PER_M) + HEIGHT_ZERO, 0, 65535).astype(np.uint16)
    zeros = np.zeros((RES, RES), dtype=np.uint16)
    manifest = {"size_uu": SIZE_M * UU_PER_M, "resolution": RES, "height_scale_uu": HEIGHT_SCALE_UU,
                "height_zero": HEIGHT_ZERO, "modules": [], "preset": {"name": name, "seed": seed, "cells": []}}
    path_set = set(cells)
    for row in range(grid):
        for col in range(grid):
            cell_name = f"M_{name}_r{row}c{col}"
            window = (slice(row * (RES - 1), row * (RES - 1) + RES), slice(col * (RES - 1), col * (RES - 1) + RES))
            Image.fromarray(quantized[window]).save(out / "Cells" / f"{cell_name}.png")
            Image.fromarray(mask[window]).save(out / "Cells" / f"{cell_name}_mask.png")
            Image.fromarray(zeros).save(out / "Cells" / f"{cell_name}_coast.png")
            cx, cy = row * SIZE_M, col * SIZE_M
            biome, second = (cell_biomes or {}).get((col, row), ("sand", secondary))
            layers = {}
            if cell_layers and (col, row) in cell_layers:
                roof, ceiling = cell_layers[(col, row)]
                Image.fromarray(roof).save(out / "Cells" / f"{cell_name}_roof.png")
                Image.fromarray(ceiling).save(out / "Cells" / f"{cell_name}_ceiling.png")
                layers = {"roof_file": f"Cells/{cell_name}_roof.png", "ceiling_file": f"Cells/{cell_name}_ceiling.png"}
            local_flats = [{"x_m": round(px - cx, 2), "y_m": round(py - cy, 2), "radius_m": round(r, 2),
                            "height_m": round(float(terrain[index_of((px, py), half)]), 2), "sunken": False}
                           for px, py, r in flat_areas if abs(px - cx) < half and abs(py - cy) < half]
            manifest["modules"].append({
                "name": cell_name, "topology": "Cross", "folder": "Cells", "edges": ["crest"] * 4, "seed": seed,
                "file": f"Cells/{cell_name}.png", "mask_file": f"Cells/{cell_name}_mask.png",
                "coast_file": f"Cells/{cell_name}_coast.png", "biome": biome, "secondary_biome": second,
                "bridges": (bridges_by_cell or {}).get((col, row), []), "monoliths": [], "flat_areas": local_flats,
                **layers,
            })
            # Lados que dan fuera del mapa (N 1, E 2, S 4, O 8): caja invisible.
            outer = (1 if row == grid - 1 else 0) | (2 if col == grid - 1 else 0) | (4 if row == 0 else 0) | (8 if col == 0 else 0)
            manifest["preset"]["cells"].append({"name": cell_name, "col": col, "row": row, "outer_sides": outer,
                                                "on_path": (col, row) in path_set,
                                                "is_start": (col, row) == cells[0], "is_end": (col, row) == cells[-1]})
    (out / "manifest.json").write_text(json.dumps(manifest, indent=1), encoding="utf-8")

    # Vista cenital sombreada (Norte arriba) y la de depuracion con curvas de nivel cada 2 m.
    gx, gy = np.gradient(terrain, STEP_M)
    light = np.clip((gx * 0.5 - gy * 0.35 + 1.0) / np.sqrt(gx * gx + gy * gy + 1.0) * 0.8, 0.0, 1.0)
    rgb = np.array([0.85, 0.70, 0.45]) * (0.35 + 0.65 * light)[..., None]
    water = smoothstep(WATER_M + 0.3, WATER_M - 0.3, terrain)[..., None]
    rgb = rgb * (1 - 0.75 * water) + np.array([0.15, 0.4, 0.7]) * 0.75 * water
    Image.fromarray((np.clip(rgb, 0, 1) * 255).astype(np.uint8)[::-1]).save(out / "preview.png")
    debug = rgb if shade is None else rgb * (1.0 - 0.35 * shade)[..., None]
    contour = (np.floor(terrain / 2.0) != np.floor(np.roll(terrain, 1, 0) / 2.0)) \
        | (np.floor(terrain / 2.0) != np.floor(np.roll(terrain, 1, 1) / 2.0))
    debug = np.where(contour[..., None], debug * 0.85, debug)
    for where, color in overlay:
        if where is not None:
            debug = np.where(where[..., None], np.array(color), debug)
    Image.fromarray((np.clip(debug, 0, 1) * 255).astype(np.uint8)[::-1]).save(out / "preview_debug.png")
    return out
