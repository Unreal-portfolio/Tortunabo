"""Genera el MAPA VOLUMETRICO fijo: terreno 3D (densidad + marching cubes) de 600 x 600 m,
cortado en 36 trozos de 100 m, con el recorrido por zonas:

    acantilados de roca arenosa (laberinto por niveles) -> canon con tuneles -> zona encharcada
    (dunas con laguitos, por las crestas) -> bosque de algas -> playa final abierta al mar.

Se ejecuta FUERA del editor, una vez (el mapa no se regenera en juego):
    uv run --with numpy --with scipy --with pillow --with scikit-image \
        python Scripts/gen_terrain_volume.py [--seed N] [--name Mapa01]

Escribe Scripts/terrain_volumes/<name>/ (Chunks/*.bin, manifest.json, preview.png) y lo
importa Scripts/import_terrain_mesh.py dentro del editor.
"""

from __future__ import annotations

import argparse
import time
from collections import deque
from pathlib import Path

import numpy as np

from terrain_vol.density import MapModel
from terrain_vol.export import global_top, write_map
from terrain_vol.layout import CELL_M, CELL_SAMPLES, GRID, STEP_Z_M, WATER_M, Z_MIN_M, Z_SAMPLES
from terrain_vol.mesh import ChunkMesh, build_chunk

OUTPUT_ROOT = Path(__file__).resolve().parent / "terrain_volumes"
CLIMB_STEPS = 2                  # desnivel caminable entre columnas vecinas (1 m por 1 m de avance)
JUMP_CELLS = 3                   # salto en linea recta: hasta 3 m de hueco, sin subir mas de 0,5 m


def build_all(model: MapModel, grid: int = GRID) -> dict[tuple[int, int], ChunkMesh]:
    return {(col, row): build_chunk(model, col, row) for row in range(grid) for col in range(grid)}


def global_standable(chunks: dict[tuple[int, int], ChunkMesh], grid: int = GRID) -> np.ndarray:
    size = grid * (CELL_SAMPLES - 1) + 1
    levels = next(iter(chunks.values())).standable.shape[2] if chunks else Z_SAMPLES
    out = np.zeros((size, size, levels), dtype=bool)
    for (col, row), chunk in chunks.items():
        i0, j0 = row * (CELL_SAMPLES - 1), col * (CELL_SAMPLES - 1)
        out[i0:i0 + CELL_SAMPLES, j0:j0 + CELL_SAMPLES] |= chunk.standable
    return out


def world_index(p) -> tuple[int, int]:
    origin = -CELL_M / 2.0
    return int(round(p[0] - origin)), int(round(p[1] - origin))


def ground_level(standable: np.ndarray, i: int, j: int) -> int:
    levels = np.nonzero(standable[i, j])[0]
    return int(levels.max()) if len(levels) else -1


def walk(standable: np.ndarray, start: tuple[int, int, int], dry_only: bool = True, z_min_m: float = Z_MIN_M) -> np.ndarray:
    """Celdas alcanzables desde start andando (desnivel <= CLIMB_STEPS entre vecinas) o saltando
    en linea recta hasta JUMP_CELLS celdas (sin subir mas de una muestra). z_min_m: cota del nivel 0."""
    dry_k = int(np.ceil((WATER_M + 0.2 - z_min_m) / STEP_Z_M)) if dry_only else 0
    seen = np.zeros_like(standable)
    if not standable[start]:
        return seen
    seen[start] = True
    queue = deque([start])
    ni, nj, nk = standable.shape
    while queue:
        i, j, k = queue.popleft()
        for di, dj in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            a, b = i + di, j + dj
            if not (0 <= a < ni and 0 <= b < nj):
                continue
            for dk in range(-CLIMB_STEPS, CLIMB_STEPS + 1):
                c = k + dk
                if dry_k <= c < nk and standable[a, b, c] and not seen[a, b, c]:
                    seen[a, b, c] = True
                    queue.append((a, b, c))
            for reach in range(2, JUMP_CELLS + 1):
                a, b = i + di * reach, j + dj * reach
                if not (0 <= a < ni and 0 <= b < nj):
                    break
                for dk in range(-4, 2):
                    c = k + dk
                    if dry_k <= c < nk and standable[a, b, c] and not seen[a, b, c]:
                        seen[a, b, c] = True
                        queue.append((a, b, c))
    return seen


def zone_map(model: MapModel, chunks, grid: int = GRID) -> dict[str, np.ndarray]:
    size = grid * (CELL_SAMPLES - 1) + 1
    out = {z: np.zeros((size, size)) for z in chunks[(0, 0)].fields.weights}
    for (col, row), chunk in chunks.items():
        i0, j0 = row * (CELL_SAMPLES - 1), col * (CELL_SAMPLES - 1)
        for z, w in chunk.fields.weights.items():
            out[z][i0:i0 + CELL_SAMPLES, j0:j0 + CELL_SAMPLES] = w
    return out


def main() -> None:
    parser = argparse.ArgumentParser(description="Genera el mapa volumetrico fijo (trozos de 100 m).")
    parser.add_argument("--seed", type=int, default=20260925)
    parser.add_argument("--name", default="Mapa01")
    args = parser.parse_args()

    t0 = time.time()
    model = MapModel(args.seed)
    chunks = build_all(model)
    standable = global_standable(chunks)
    start_ij, end_ij = world_index(model.route.points[0]), world_index(model.route.points[-1])
    start = (*start_ij, ground_level(standable, *start_ij))
    end = (*end_ij, ground_level(standable, *end_ij))
    reached = walk(standable, start)
    ok = bool(reached[end]) if end[2] >= 0 else False
    top = global_top(chunks)
    start_w = (model.route.points[0][0], model.route.points[0][1], float(top[start_ij]))
    end_w = (model.route.points[-1][0], model.route.points[-1][1], float(top[end_ij]))
    write_map(OUTPUT_ROOT / args.name, args.name, args.seed, chunks, start_w, end_w, zone_map(model, chunks),
              model.route.points)
    tris = sum(len(c.triangles) for c in chunks.values())
    plants = sum(len(c.instances) for c in chunks.values())
    print(f"{args.name}: ruta {model.route.length:.0f} m; {len(model.maze_tunnels)} tuneles; {tris} triangulos; "
          f"{plants} algas; inicio->final a pie: {'SI' if ok else 'NO'}; {time.time() - t0:.1f} s")
    if not ok:
        raise SystemExit("el final no se alcanza a pie desde el inicio")


if __name__ == "__main__":
    main()
