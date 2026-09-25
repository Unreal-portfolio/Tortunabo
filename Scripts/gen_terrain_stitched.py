"""Genera un MAPA PREPARADO COSIDO: un modulo de la libreria (mismo disenador que
gen_terrain_modules.py) en cada celda del camino, cosidos en un unico terreno continuo.

Se ejecuta FUERA del editor:
    uv run --with numpy --with pillow --with scipy --with scikit-image \
        python Scripts/gen_terrain_stitched.py [--seed N] [--name Mapa01] [--grid 6]
        [--min-path 9] [--max-path 12]

Escribe lo mismo que gen_terrain_preset.py (Scripts/terrain_presets/<name>/) y se importa
igual, con Scripts/import_terrain_modules.py.

Diseno:
  1. Camino de celdas: uno solo, de la primera fila a la ultima (sin desvios).
  2. Cada celda del camino recibe un modulo disenado para sus salidas: pasillo entre
     paredes, bifurcaciones con isla, atajos, rutas altas con puente, plazas, caminitos,
     charcos, repisas... segun el estilo que toque (desierto, canon, montana, laberinto).
     Sin tuneles ni monolitos.
  3. Las celdas fuera del camino son ambiente: meseta con colinas y, lejos del camino,
     lagunas de mar.
  4. Costura gradual: cada celda se extiende 40 m por reflexion y las vecinas se mezclan con
     una particion de la unidad; las bocas coinciden (cota 0 en el centro del lado comun).
  5. Deformacion suave del mapa entero (hasta 25 m): las bocas dejan de caer en el centro
     del lado y los tramos rectos de la cuadricula se curvan.
"""

from __future__ import annotations

import argparse

import numpy as np
from scipy import ndimage

from gen_terrain_preset import (SIZE_M, RES, STEP_M, WATER_M, blur, fbm, generate_path, index_of, reachable,
                                smoothstep, unit_fbm, write_preset)
from terrain_gen.core import (HEIGHT_ZERO, UNITS_PER_M, BRIDGE_THICKNESS_M, bridge_problem,
                              walkable_exits_connected)
from terrain_gen.design import compose_module

BAND = 20                   # muestras de la costura a cada lado del borde (40 m)
WARP_M = 25.0               # desplazamiento maximo de la deformacion final
MODULE_ATTEMPTS = 16
SIDE_STEP = {"N": (0, 1), "E": (1, 0), "S": (0, -1), "W": (-1, 0)}   # (dcol, dfila)


def cell_exits(cells, index: int) -> tuple[str, ...]:
    """Lados de la celda index que conectan con la anterior y la siguiente del camino."""
    col, row = cells[index]
    exits = []
    for side, (dc, dr) in SIDE_STEP.items():
        neighbour = (col + dc, row + dr)
        if (index > 0 and cells[index - 1] == neighbour) or (index + 1 < len(cells) and cells[index + 1] == neighbour):
            exits.append(side)
    return tuple(exits)


def design_cell(rng, exits: tuple[str, ...]):
    """Modulo accesible, con los arcos apoyados y sin tuneles. (metros, arcos, plazas)."""
    for _ in range(MODULE_ATTEMPTS):
        seed = int(rng.integers(1, 2 ** 31 - 1))
        heights, _stats, bridges, flat_areas, _monoliths, _mask, _coast = compose_module(
            seed, exits, "sand", "sand", None, None)
        meters = (heights.astype(np.float64) - HEIGHT_ZERO) / UNITS_PER_M
        if any(b.kind == "tunnel" for b in bridges):
            continue
        if walkable_exits_connected(meters, exits) and bridge_problem(meters, bridges) is None:
            return meters, bridges, flat_areas
    raise RuntimeError(f"ningun modulo valido para las salidas {exits}")


def ambient_field(rng, XX, YY, path_distance):
    """Ambiente fuera del camino: meseta con colinas y, lejos del camino, lagunas de mar."""
    plateau = 7.0 + 5.0 * unit_fbm(rng, XX, YY, 220.0) \
        + unit_fbm(rng, XX, YY, 170.0, 3) * (6.0 + 20.0 * unit_fbm(rng, XX, YY, 500.0)) \
        + 0.5 * fbm(rng, XX, YY, 24.0, 2)
    lagoon = smoothstep(0.45, 0.6, unit_fbm(rng, XX, YY, 380.0)) * smoothstep(150.0, 260.0, path_distance)
    sea_floor = WATER_M - 1.5 - 1.0 * unit_fbm(rng, XX, YY, 80.0)
    return plateau * (1.0 - lagoon) + sea_floor * lagoon


def partition_weight(length: int) -> np.ndarray:
    """Peso 1D de una celda extendida BAND muestras por cada lado: 0,5 en el borde, 1 dentro,
    0 a BAND muestras fuera. Con la vecina suma 1 en toda la franja."""
    index = np.arange(length + 2 * BAND, dtype=np.float64) - BAND
    inside = np.minimum(index, (length - 1) - index)
    return smoothstep(-BAND, BAND, inside)


def stitch(fields: dict, grid: int, samples: int) -> np.ndarray:
    """Mezcla los campos de cada celda (ya extendidos BAND muestras) en un terreno continuo."""
    total = np.zeros((samples + 2 * BAND, samples + 2 * BAND))
    weight_sum = np.zeros_like(total)
    weight_1d = partition_weight(RES)
    weight = np.outer(weight_1d, weight_1d)
    for (col, row), field in fields.items():
        i0, j0 = row * (RES - 1), col * (RES - 1)
        total[i0:i0 + RES + 2 * BAND, j0:j0 + RES + 2 * BAND] += weight * field
        weight_sum[i0:i0 + RES + 2 * BAND, j0:j0 + RES + 2 * BAND] += weight
    inner = (slice(BAND, BAND + samples), slice(BAND, BAND + samples))
    return total[inner] / np.maximum(weight_sum[inner], 1e-9)


def warp_offsets(rng, XX, YY):
    return WARP_M * fbm(rng, XX, YY, 170.0, 2), WARP_M * fbm(rng, XX, YY, 170.0, 2)


def warp(terrain, offset_x, offset_y):
    """terrain'(p) = terrain(p + offset(p)), en muestras."""
    rows, cols = np.meshgrid(np.arange(terrain.shape[0]), np.arange(terrain.shape[1]), indexing="ij")
    coords = np.array([rows + offset_x / STEP_M, cols + offset_y / STEP_M])
    return ndimage.map_coordinates(terrain, coords, order=1, mode="nearest")


def unwarp_point(p, offset_x, offset_y, half: float):
    """Donde queda tras la deformacion un punto p del terreno original (p - offset, iterado)."""
    q = p
    for _ in range(3):
        i, j = index_of(q, half)
        i = min(max(i, 0), offset_x.shape[0] - 1)
        j = min(max(j, 0), offset_x.shape[1] - 1)
        q = (p[0] - float(offset_x[i, j]), p[1] - float(offset_y[i, j]))
    return q


def main() -> None:
    parser = argparse.ArgumentParser(description="Genera un mapa preparado cosiendo modulos de la libreria.")
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
    ext_axis = np.linspace(-half - BAND * STEP_M, grid * SIZE_M - half + BAND * STEP_M, samples + 2 * BAND)
    EX, EY = np.meshgrid(ext_axis, ext_axis, indexing="ij")

    cells = generate_path(rng, grid, args.min_path, args.max_path)
    centers = [(row * SIZE_M, col * SIZE_M) for col, row in cells]

    # Ambiente sobre la rejilla extendida: la distancia al camino se mide a los centros.
    path_distance = np.full_like(EX, np.inf)
    for x, y in centers:
        path_distance = np.minimum(path_distance, np.maximum(np.abs(EX - x), np.abs(EY - y)) - half)
    ambient = ambient_field(rng, EX, EY, np.maximum(path_distance, 0.0))

    fields, bridges_world, flats_world = {}, [], []
    for col in range(grid):
        for row in range(grid):
            i0, j0 = row * (RES - 1), col * (RES - 1)
            fields[(col, row)] = ambient[i0:i0 + RES + 2 * BAND, j0:j0 + RES + 2 * BAND]
    for index, (col, row) in enumerate(cells):
        meters, bridges, flat_areas = design_cell(rng, cell_exits(cells, index))
        fields[(col, row)] = np.pad(meters, BAND, mode="reflect")
        cx, cy = row * SIZE_M, col * SIZE_M
        bridges_world += [(b, (b.x + cx, b.y + cy)) for b in bridges]
        flats_world += [(f["x_m"] + cx, f["y_m"] + cy, f["radius_m"]) for f in flat_areas]

    terrain = stitch(fields, grid, samples)
    offset_x, offset_y = warp_offsets(rng, XX, YY)
    terrain = blur(warp(terrain, offset_x, offset_y))

    start, goal = index_of(centers[0], half), index_of(centers[-1], half)
    if not reachable(terrain, start, goal):
        raise AssertionError("el final no se alcanza a pie desde el inicio")

    # Arcos y plazas siguen al terreno deformado; un arco que ya no apoya bien se quita.
    bridges_by_cell, dropped = {}, 0
    for bridge, p in bridges_world:
        x, y = unwarp_point(p, offset_x, offset_y, half)
        col, row = int(round(y / SIZE_M)), int(round(x / SIZE_M))
        cx, cy = row * SIZE_M, col * SIZE_M
        moved = type(bridge)(x - cx, y - cy, bridge.yaw_deg, bridge.length_m, bridge.width_m, bridge.deck_m, bridge.kind)
        window = terrain[row * (RES - 1):row * (RES - 1) + RES, col * (RES - 1):col * (RES - 1) + RES]
        if bridge_problem(window, [moved]) is not None:
            dropped += 1
            continue
        bridges_by_cell.setdefault((col, row), []).append({
            "x_m": round(moved.x, 2), "y_m": round(moved.y, 2), "yaw_deg": moved.yaw_deg,
            "length_m": moved.length_m, "width_m": moved.width_m, "deck_m": moved.deck_m,
            "kind": moved.kind, "thickness_m": BRIDGE_THICKNESS_M})
    flat_areas = [(*unwarp_point((x, y), offset_x, offset_y, half), r) for x, y, r in flats_world]

    mask = np.zeros(terrain.shape, dtype=np.uint16)
    on_path = np.zeros(terrain.shape, dtype=bool)
    for col, row in cells:
        on_path[row * (RES - 1):row * (RES - 1) + RES, col * (RES - 1):col * (RES - 1) + RES] = True
    out = write_preset(args.name, args.seed, grid, terrain, mask, cells, flat_areas, bridges_by_cell,
                       secondary="sand", shade=(~on_path).astype(np.float64))
    kept = sum(len(v) for v in bridges_by_cell.values())
    print(f"{args.name}: camino de {len(cells)} celdas {cells}; {kept} puentes ({dropped} quitados); "
          f"{len(flat_areas)} plazas; cota [{terrain.min():.1f}, {terrain.max():.1f}] m; {grid * grid} modulos en {out}")


if __name__ == "__main__":
    main()
