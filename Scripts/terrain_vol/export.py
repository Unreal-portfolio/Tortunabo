"""Escritura del mapa volumetrico: un binario por trozo, manifest y vistas cenitales.

Formato del binario (little endian), el que lee UTN_TerrainMeshAsset::LoadFromFile:
    char[4]  'TNTM'
    uint32   version (1)
    uint32   vertices N, triangulos M, instancias K
    float32  N*3 posiciones (uu, locales al centro del trozo)
    float32  N*3 normales
    uint8    N*4 color RGBA (color LINEAL * 255)
    uint32   M*3 indices (cara visible segun Unreal)
    float32  K*11 instancias: forma, x, y, z, yaw, sx, sy, sz, r, g, b
"""

from __future__ import annotations

import json
import shutil
import struct
from pathlib import Path

import numpy as np
from PIL import Image

from .layout import CELL_M, CELL_SAMPLES, GRID, UU_PER_M, WATER_M, cell_center
from .mesh import ChunkMesh

MAGIC = b"TNTM"
VERSION = 1
HEADER = struct.Struct("<4sIIII")


def chunk_name(col: int, row: int) -> str:
    return f"r{row}c{col}"


def write_chunk(path: Path, chunk: ChunkMesh) -> None:
    with open(path, "wb") as handle:
        handle.write(HEADER.pack(MAGIC, VERSION, len(chunk.vertices), len(chunk.triangles), len(chunk.instances)))
        handle.write(chunk.vertices.astype("<f4").tobytes())
        handle.write(chunk.normals.astype("<f4").tobytes())
        handle.write(chunk.colors.astype(np.uint8).tobytes())
        handle.write(chunk.triangles.astype("<u4").tobytes())
        handle.write(chunk.instances.astype("<f4").tobytes())


def read_chunk(path: Path) -> dict:
    data = path.read_bytes()
    magic, version, n, m, k = HEADER.unpack_from(data, 0)
    if magic != MAGIC or version != VERSION:
        raise ValueError(f"{path}: cabecera invalida")
    offset = HEADER.size
    out = {}
    for key, dtype, count, width in (("vertices", "<f4", n, 3), ("normals", "<f4", n, 3), ("colors", np.uint8, n, 4),
                                     ("triangles", "<u4", m, 3), ("instances", "<f4", k, 11)):
        size = np.dtype(dtype).itemsize * count * width
        out[key] = np.frombuffer(data, dtype=dtype, count=count * width, offset=offset).reshape(count, width)
        offset += size
    if offset != len(data):
        raise ValueError(f"{path}: {len(data) - offset} bytes de mas")
    return out


def write_map(out: Path, name: str, seed: int, chunks: dict[tuple[int, int], ChunkMesh], start, end, zone_map,
              route_points) -> None:
    if out.exists():
        shutil.rmtree(out)
    (out / "Chunks").mkdir(parents=True)
    cells = []
    for (col, row), chunk in sorted(chunks.items()):
        file = f"Chunks/{chunk_name(col, row)}.bin"
        write_chunk(out / file, chunk)
        cx, cy = cell_center(col, row)
        cells.append({"name": f"M_{name}_{chunk_name(col, row)}", "col": col, "row": row, "file": file,
                      "center_uu": [cx * UU_PER_M, cy * UU_PER_M], "vertices": int(len(chunk.vertices)),
                      "triangles": int(len(chunk.triangles)), "instances": int(len(chunk.instances))})
    manifest = {"name": name, "seed": seed, "format": "TNTM1", "cell_uu": CELL_M * UU_PER_M, "grid": GRID,
                "water_uu": WATER_M * UU_PER_M, "start_uu": [v * UU_PER_M for v in start],
                "end_uu": [v * UU_PER_M for v in end], "cells": cells}
    (out / "manifest.json").write_text(json.dumps(manifest, indent=1), encoding="utf-8")
    write_preview(out, chunks, zone_map, route_points)


def global_top(chunks: dict[tuple[int, int], ChunkMesh]) -> np.ndarray:
    size = GRID * (CELL_SAMPLES - 1) + 1
    top = np.zeros((size, size))
    for (col, row), chunk in chunks.items():
        i0, j0 = row * (CELL_SAMPLES - 1), col * (CELL_SAMPLES - 1)
        top[i0:i0 + CELL_SAMPLES, j0:j0 + CELL_SAMPLES] = chunk.top
    return top


def write_preview(out: Path, chunks, zone_map, route_points) -> None:
    """Vista cenital sombreada (Norte arriba), agua, tinte por zona y la ruta."""
    top = global_top(chunks)
    gx, gy = np.gradient(top)
    light = np.clip((gx * 0.5 - gy * 0.35 + 1.0) / np.sqrt(gx * gx + gy * gy + 1.0) * 0.8, 0.0, 1.0)
    tint = {"cliffs": (0.80, 0.62, 0.40), "canyon": (0.85, 0.66, 0.42), "marsh": (0.88, 0.80, 0.60),
            "algae": (0.42, 0.50, 0.20), "beach": (0.95, 0.86, 0.62)}
    base = np.zeros(top.shape + (3,))
    for zone, w in zone_map.items():
        base += w[..., None] * np.array(tint[zone])
    rgb = base * (0.35 + 0.65 * light)[..., None]
    water = (top < WATER_M)[..., None]
    rgb = np.where(water, rgb * 0.25 + np.array([0.12, 0.35, 0.65]) * 0.75, rgb)
    contour = (np.floor(top / 2.0) != np.floor(np.roll(top, 1, 0) / 2.0)) | (np.floor(top / 2.0) != np.floor(np.roll(top, 1, 1) / 2.0))
    rgb = np.where(contour[..., None], rgb * 0.85, rgb)
    Image.fromarray((np.clip(rgb, 0, 1) * 255).astype(np.uint8)[::-1]).save(out / "preview.png")
    debug = rgb.copy()
    origin = -CELL_M / 2.0
    for x, y in route_points[::2]:
        i, j = int(round(x - origin)), int(round(y - origin))
        if 0 <= i < debug.shape[0] and 0 <= j < debug.shape[1]:
            debug[max(0, i - 1):i + 2, max(0, j - 1):j + 2] = (0.9, 0.1, 0.1)
    Image.fromarray((np.clip(debug, 0, 1) * 255).astype(np.uint8)[::-1]).save(out / "preview_debug.png")
