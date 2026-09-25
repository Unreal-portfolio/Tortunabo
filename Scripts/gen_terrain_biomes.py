"""Genera un MAPA DE BIOMAS: modulos de 100 m en un grid de 6x6, con el camino repartido en
tres tramos en orden fijo (arena -> agua -> algas) y cosidos en un unico terreno continuo.

Se ejecuta FUERA del editor:
    uv run --with numpy --with pillow --with scipy --with scikit-image \
        python Scripts/gen_terrain_biomes.py [--seed N] [--name Mapa01] [--grid 6]
        [--min-path 12] [--max-path 18]

Escribe lo mismo que gen_terrain_preset.py (Scripts/terrain_presets/<name>/), mas las capas
del tunel (<celda>_roof.png y <celda>_ceiling.png) en las celdas que lo tienen, y se importa
igual, con Scripts/import_terrain_modules.py.

Pasos:
  1. Camino de celdas por algoritmo (generate_path): uno solo, de la primera fila a la ultima.
  2. Tramos de bioma a lo largo del camino: arena, agua y algas, cada uno de 3 celdas o mas.
     Las celdas fuera del camino toman el bioma del camino mas cercano; las que tocan una celda
     de agua del camino son mar (islitas incluidas).
  3. Un modulo por celda (terrain_gen/biomes.py): laberinto de acantilados, canon con tunel o
     valle de dunas en arena; pasillito de arena por el mar en agua; bosque frondoso en algas.
     Las bocas de cada union caen en el mismo punto, desplazado del centro del lado.
  4. Costura: cada celda se extiende BAND muestras por reflexion y las vecinas se mezclan con
     una particion de la unidad. Las capas del tunel se guardan relativas al suelo final.
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass

import numpy as np
from scipy import ndimage

from gen_terrain_preset import generate_path, index_of, reachable, write_preset
from terrain_gen.biomes import (MOUTH_JITTER_M, ModuleDesign, design_ambient, design_path_cell,
                                mouth_point)
from gen_terrain_preset import fbm as grid_fbm
from terrain_gen.core import HEIGHT_ZERO, RES, SIZE_M, STEP_M, UNITS_PER_M, WATER_M, smoothstep

BIOME_ORDER = ("sand", "water", "algae")
MIN_SEGMENT = 3                  # celdas minimas de cada tramo de bioma
BAND = 20                        # muestras de costura a cada lado del borde
BIOME_BLEND_SIGMA = 6.0          # muestras: anchura del fundido de paleta entre biomas
MODULE_ATTEMPTS = 12
WARP_M = 24.0                    # deformacion maxima del mapa: rompe las rectas de la cuadricula
WARP_WAVE_M = 130.0
TUNNEL_CALM_M = (8.0, 30.0)      # alrededor del tunel la deformacion se apaga (capas intactas)
SIDE_STEP = {"N": (0, 1), "E": (1, 0), "S": (0, -1), "W": (-1, 0)}   # (dcol, dfila)
OPPOSITE = {"N": "S", "S": "N", "E": "W", "W": "E"}


@dataclass
class BiomeMap:
    terrain: np.ndarray                         # suelo final del mapa entero (m)
    foliage: np.ndarray                         # densidad de algas 0..1
    cells: list[tuple[int, int]]                # camino (col, fila)
    path_biomes: list[str]                      # bioma de cada celda del camino
    cell_biomes: dict[tuple[int, int], str]     # bioma de todas las celdas
    designs: dict[tuple[int, int], ModuleDesign]
    layers: dict[tuple[int, int], tuple[np.ndarray, np.ndarray]]   # techo y boveda codificados
    offsets: tuple[np.ndarray, np.ndarray]      # deformacion aplicada (muestras)


# ── Biomas ────────────────────────────────────────────────────────────────────────
def plan_biomes(rng: np.random.Generator, length: int) -> list[str]:
    """Bioma de cada celda del camino: tres tramos consecutivos en BIOME_ORDER."""
    if length < MIN_SEGMENT * len(BIOME_ORDER):
        raise ValueError(f"camino de {length} celdas: hacen falta {MIN_SEGMENT * len(BIOME_ORDER)}")
    spare = length - MIN_SEGMENT * len(BIOME_ORDER)
    # Reparto equilibrado con algo de variacion (Dirichlet concentrada): ningun tramo se come el mapa.
    share = rng.dirichlet([4.0] * len(BIOME_ORDER)) * spare
    extra = np.floor(share).astype(int)
    for k in np.argsort(share - extra)[::-1][:spare - int(extra.sum())]:
        extra[k] += 1
    return [biome for biome, add in zip(BIOME_ORDER, extra) for _ in range(MIN_SEGMENT + int(add))]


def ambient_biomes(cells: list[tuple[int, int]], path_biomes: list[str], grid: int) -> dict[tuple[int, int], str]:
    """Bioma de todas las celdas. Fuera del camino: agua si toca (8 vecinos) una celda de agua
    del camino; si no, el de la celda del camino mas cercana (Chebyshev, la primera en empate)."""
    on_path = dict(zip(cells, path_biomes))
    result = dict(on_path)
    for col in range(grid):
        for row in range(grid):
            if (col, row) in on_path:
                continue
            near_water = any(on_path.get((col + dc, row + dr)) == "water"
                             for dc in (-1, 0, 1) for dr in (-1, 0, 1))
            if near_water:
                result[(col, row)] = "water"
                continue
            nearest = min(range(len(cells)), key=lambda k: max(abs(cells[k][0] - col), abs(cells[k][1] - row)))
            result[(col, row)] = path_biomes[nearest]
    return result


# ── Bocas ─────────────────────────────────────────────────────────────────────────
def side_towards(a: tuple[int, int], b: tuple[int, int]) -> str:
    step = (b[0] - a[0], b[1] - a[1])
    return next(side for side, delta in SIDE_STEP.items() if delta == step)


def plan_mouths(rng: np.random.Generator, cells: list[tuple[int, int]]) -> list[dict[str, tuple[float, float]]]:
    """Bocas de cada celda del camino: una por union, en el mismo punto visto desde las dos."""
    mouths: list[dict[str, tuple[float, float]]] = [{} for _ in cells]
    for k in range(len(cells) - 1):
        side = side_towards(cells[k], cells[k + 1])
        offset = float(rng.uniform(-MOUTH_JITTER_M, MOUTH_JITTER_M))
        mouths[k][side] = mouth_point(side, offset)
        mouths[k + 1][OPPOSITE[side]] = mouth_point(OPPOSITE[side], offset)
    # La primera boca de la lista es la de entrada: route_lane va de ella a la otra.
    ordered = []
    for k, cell_mouths in enumerate(mouths):
        if k > 0:
            entry = OPPOSITE[side_towards(cells[k - 1], cells[k])]
            cell_mouths = {entry: cell_mouths[entry], **{s: p for s, p in cell_mouths.items() if s != entry}}
        ordered.append(cell_mouths)
    return ordered


# ── Costura ───────────────────────────────────────────────────────────────────────
def partition_weight(length: int) -> np.ndarray:
    """Peso 1D de una celda extendida BAND muestras por lado: 0,5 en el borde, 1 dentro,
    0 a BAND muestras fuera. Con la vecina suma 1 en toda la franja."""
    index = np.arange(length + 2 * BAND, dtype=np.float64) - BAND
    inside = np.minimum(index, (length - 1) - index)
    return smoothstep(-BAND, BAND, inside)


def stitch(fields: dict, grid: int, samples: int) -> np.ndarray:
    total = np.zeros((samples + 2 * BAND, samples + 2 * BAND))
    weight_sum = np.zeros_like(total)
    weight_1d = partition_weight(RES)
    weight = np.outer(weight_1d, weight_1d)
    for (col, row), values in fields.items():
        i0, j0 = row * (RES - 1), col * (RES - 1)
        total[i0:i0 + RES + 2 * BAND, j0:j0 + RES + 2 * BAND] += weight * values
        weight_sum[i0:i0 + RES + 2 * BAND, j0:j0 + RES + 2 * BAND] += weight
    inner = (slice(BAND, BAND + samples), slice(BAND, BAND + samples))
    return total[inner] / np.maximum(weight_sum[inner], 1e-9)


def window(col: int, row: int) -> tuple[slice, slice]:
    return slice(row * (RES - 1), row * (RES - 1) + RES), slice(col * (RES - 1), col * (RES - 1) + RES)


def warp_offsets(rng: np.random.Generator, samples: int, calm: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    """Desplazamiento (muestras) de la deformacion; calm (0..1) la apaga."""
    axis = np.arange(samples) * STEP_M
    px, py = np.meshgrid(axis, axis, indexing="ij")
    scale = WARP_M / STEP_M * calm
    return scale * grid_fbm(rng, px, py, WARP_WAVE_M, 2), scale * grid_fbm(rng, px, py, WARP_WAVE_M, 2)


def warp(values: np.ndarray, offsets: tuple[np.ndarray, np.ndarray]) -> np.ndarray:
    """values'(p) = values(p + offset(p))."""
    rows, cols = np.meshgrid(np.arange(values.shape[0]), np.arange(values.shape[1]), indexing="ij")
    coords = np.array([rows + offsets[0], cols + offsets[1]])
    return ndimage.map_coordinates(values, coords, order=1, mode="nearest")


def calm_mask(designs: dict, samples: int) -> np.ndarray:
    """1 lejos de los tuneles, 0 dentro de su huella y en TUNNEL_CALM_M[0] alrededor."""
    footprint = np.zeros((samples, samples), dtype=bool)
    for (col, row), design in designs.items():
        if design.has_tunnel:
            footprint[window(col, row)] |= ~np.isnan(design.roof_delta)
    if not footprint.any():
        return np.ones_like(footprint, dtype=np.float64)
    distance = ndimage.distance_transform_edt(~footprint) * STEP_M
    return smoothstep(*TUNNEL_CALM_M, distance)


def quantize(meters: np.ndarray) -> np.ndarray:
    return np.clip(np.rint(meters * UNITS_PER_M) + HEIGHT_ZERO, 1, 65535).astype(np.int64)


def encode_layers(floor_q: np.ndarray, design: ModuleDesign) -> tuple[np.ndarray, np.ndarray]:
    """Techo y boveda codificados sobre el suelo final (0 = sin techo). Relativos al suelo
    ya cosido: en el borde lateral de la huella coinciden con el exactamente."""
    assert design.roof_delta is not None and design.ceil_delta is not None
    valid = ~np.isnan(design.roof_delta)
    roof_q = floor_q + np.rint(np.nan_to_num(design.roof_delta) * UNITS_PER_M).astype(np.int64)
    ceil_q = floor_q + np.rint(np.nan_to_num(design.ceil_delta) * UNITS_PER_M).astype(np.int64)
    ceil_q = np.minimum(ceil_q, roof_q)
    roof = np.where(valid, np.clip(roof_q, 1, 65535), 0).astype(np.uint16)
    ceiling = np.where(valid, np.clip(ceil_q, 1, 65535), 0).astype(np.uint16)
    return roof, ceiling


# ── Mapa ──────────────────────────────────────────────────────────────────────────
def design_cells(rng: np.random.Generator, cells, path_biomes, cell_biomes, grid: int) -> dict:
    mouths = plan_mouths(rng, cells)
    designs: dict[tuple[int, int], ModuleDesign] = {}
    sand = [k for k, b in enumerate(path_biomes) if b == "sand" and len(mouths[k]) == 2]
    tunnel_target = sand[len(sand) // 2] if sand else -1
    for k, cell in enumerate(cells):
        designs[cell] = design_path_cell(rng, path_biomes[k], mouths[k], force_tunnel=(k == tunnel_target))
    for col in range(grid):
        for row in range(grid):
            if (col, row) not in designs:
                designs[(col, row)] = design_ambient(rng, cell_biomes[(col, row)])
    return designs


def build_map(seed: int, grid: int = 6, min_path: int = 12, max_path: int = 18) -> BiomeMap:
    rng = np.random.default_rng(seed)
    samples = grid * (RES - 1) + 1
    cells = generate_path(rng, grid, min_path, max_path)
    path_biomes = plan_biomes(rng, len(cells))
    cell_biomes = ambient_biomes(cells, path_biomes, grid)

    for _ in range(MODULE_ATTEMPTS):
        designs = design_cells(rng, cells, path_biomes, cell_biomes, grid)
        terrain = stitch({c: np.pad(d.floor, BAND, mode="reflect") for c, d in designs.items()}, grid, samples)
        foliage = stitch({c: np.pad(d.foliage, BAND, mode="reflect") for c, d in designs.items()}, grid, samples)
        offsets = warp_offsets(rng, samples, calm_mask(designs, samples))
        terrain = ndimage.gaussian_filter(warp(terrain, offsets), 0.8, mode="nearest")
        foliage = warp(foliage, offsets)
        # En seco: el agua no cuenta como paso (aun no se nada).
        dry = np.where(terrain > WATER_M + 0.2, terrain, 1e6)
        half = SIZE_M / 2.0
        start = index_of((cells[0][1] * SIZE_M, cells[0][0] * SIZE_M), half)
        goal = index_of((cells[-1][1] * SIZE_M, cells[-1][0] * SIZE_M), half)
        if reachable(dry, start, goal):
            break
    else:
        raise RuntimeError(f"semilla {seed}: el final no se alcanza en seco tras {MODULE_ATTEMPTS} intentos")

    floor_q = quantize(terrain)
    layers = {c: encode_layers(floor_q[window(*c)], d) for c, d in designs.items() if d.has_tunnel}
    return BiomeMap(terrain, np.clip(foliage, 0.0, 1.0), cells, path_biomes, cell_biomes, designs, layers, offsets)


def biome_mask(result: BiomeMap) -> tuple[np.ndarray, dict]:
    """Mascara uint16 del mapa entero (byte alto: peso del bioma secundario de la celda; byte
    bajo: densidad de algas) y {(col, fila): (bioma, secundario)}."""
    shape = result.terrain.shape
    weights = {}
    for biome in BIOME_ORDER:
        one_hot = np.zeros(shape)
        for (col, row), b in result.cell_biomes.items():
            if b == biome:
                one_hot[window(col, row)] = 1.0
        # Misma deformacion que el relieve: el color sigue a la costa y a las lomas.
        weights[biome] = warp(ndimage.gaussian_filter(one_hot, BIOME_BLEND_SIGMA, mode="nearest"), result.offsets)
    mask = np.zeros(shape, dtype=np.uint16)
    pairs = {}
    for (col, row), own in result.cell_biomes.items():
        win = window(col, row)
        others = [b for b in BIOME_ORDER if b != own]
        second = max(others, key=lambda b: float(weights[b][win].max()))
        if float(weights[second][win].max()) < 0.02:
            second = own
        w_own = weights[own][win]
        w_sec = weights[second][win] if second != own else np.zeros_like(w_own)
        high = np.rint(255.0 * w_sec / np.maximum(w_own + w_sec, 1e-9)).astype(np.uint16)
        low = np.rint(255.0 * result.foliage[win] * weights["algae"][win]).astype(np.uint16)
        mask[win] = (high << 8) | low
        pairs[(col, row)] = (own, second)
    return mask, pairs


def main() -> None:
    parser = argparse.ArgumentParser(description="Genera un mapa de biomas (arena, agua, algas) de modulos de 100 m.")
    parser.add_argument("--seed", type=int, default=20260925)
    parser.add_argument("--name", default="Mapa01")
    parser.add_argument("--grid", type=int, default=6)
    parser.add_argument("--min-path", type=int, default=12)
    parser.add_argument("--max-path", type=int, default=18)
    args = parser.parse_args()

    result = build_map(args.seed, args.grid, args.min_path, args.max_path)
    mask, pairs = biome_mask(result)
    flats = []
    for (col, row), design in result.designs.items():
        cx, cy = row * SIZE_M, col * SIZE_M
        flats += [(f["x_m"] + cx, f["y_m"] + cy, f["radius_m"]) for f in design.flat_areas]
    tunnel = np.zeros(result.terrain.shape, dtype=bool)
    for (col, row), (roof, _) in result.layers.items():
        tunnel[window(col, row)] = roof > 0
    algae = (mask & 0xFF) > 128
    out = write_preset(args.name, args.seed, args.grid, result.terrain, mask, result.cells, flats,
                       shade=None, overlay=[(algae, (0.35, 0.45, 0.2)), (tunnel, (0.55, 0.15, 0.1))],
                       cell_biomes=pairs, cell_layers=result.layers)
    counts = {b: result.path_biomes.count(b) for b in BIOME_ORDER}
    kinds = sorted(d.kind for c, d in result.designs.items() if c in set(result.cells))
    print(f"{args.name}: camino de {len(result.cells)} celdas {counts}; {len(result.layers)} tuneles; "
          f"modulos {dict((k, kinds.count(k)) for k in sorted(set(kinds)))}; "
          f"cota [{result.terrain.min():.1f}, {result.terrain.max():.1f}] m; en {out}")


if __name__ == "__main__":
    main()
