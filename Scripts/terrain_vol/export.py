"""Escritura del mapa volumetrico: un binario por trozo, manifest y vistas cenitales.

Formato del binario (little endian). v1 (el que lee UTN_TerrainMeshAsset::LoadFromFile hoy):
    char[4]  'TNTM'
    uint32   version (1)
    uint32   vertices N, triangulos M, instancias K
    float32  N*3 posiciones (uu, locales al centro del trozo)
    float32  N*3 normales
    uint8    N*4 color RGBA (color LINEAL * 255)
    uint32   M*3 indices (cara visible segun Unreal)
    float32  K*11 instancias: forma, x, y, z, yaw, sx, sy, sz, r, g, b

v2 (el que escribe write_chunk por defecto; el lector C++ se adapta en paralelo), pensado
para pesar menos: cuantiza posiciones y normales y comprime el resto con zlib.
    char[4]  'TNTM'
    uint32   version (2)
    uint32   N, uint32 M, uint32 K
    float32  origin[3] (uu), float32 step[3] (uu por unidad cuantizada; pos = origin + q*step)
    uint32   raw_size (bytes del payload sin comprimir), uint32 zlib_size (bytes que siguen)
    -- payload zlib (cabecera RFC1950, zlib.compress) --
    uint16[N*3]  posiciones cuantizadas
    int16[N*2]   normales octaedricas snorm (valor / 32767)
    uint8[N*4]   color RGBA (igual que v1)
    int32[M*3]   indices en delta sobre el array plano (idx[i] - idx[i-1], idx[-1] = 0)
    float32[K*11] instancias (igual que v1)
El orden de vertices por triangulo es el mismo que v1. read_chunk() acepta v1 y v2.
"""

from __future__ import annotations

import json
import shutil
import struct
import zlib
from pathlib import Path

import numpy as np
from PIL import Image

from .layout import CELL_M, CELL_SAMPLES, GRID, UU_PER_M, WATER_M, cell_center
from .mesh import ChunkMesh

MAGIC = b"TNTM"
VERSION1 = 1
VERSION2 = 2
VERSION = VERSION2                  # version que escribe write_chunk por defecto
HEADER1 = struct.Struct("<4sIIII")
HEADER2 = struct.Struct("<4sIIII3f3fII")


def chunk_name(col: int, row: int) -> str:
    return f"r{row}c{col}"


def _encode_normals_oct(normals: np.ndarray) -> np.ndarray:
    """Normales -> (x, y) octaedrico en [-1, 1] (snorm16 tras cuantizar)."""
    n = normals.astype(np.float64)
    n = n / np.maximum(np.abs(n).sum(axis=1, keepdims=True), 1e-20)
    x, y, z = n[:, 0], n[:, 1], n[:, 2]
    sx, sy = np.where(x >= 0, 1.0, -1.0), np.where(y >= 0, 1.0, -1.0)
    ox = np.where(z < 0, (1.0 - np.abs(y)) * sx, x)
    oy = np.where(z < 0, (1.0 - np.abs(x)) * sy, y)
    return np.clip(np.round(np.stack([ox, oy], axis=1) * 32767.0), -32767, 32767).astype("<i2")


def _decode_normals_oct(enc: np.ndarray) -> np.ndarray:
    xy = enc.astype(np.float64) / 32767.0
    x, y = xy[:, 0], xy[:, 1]
    z = 1.0 - np.abs(x) - np.abs(y)
    t = np.clip(-z, 0.0, None)
    x = x - t * np.where(x >= 0, 1.0, -1.0)
    y = y - t * np.where(y >= 0, 1.0, -1.0)
    n = np.stack([x, y, z], axis=1)
    return n / np.maximum(np.linalg.norm(n, axis=1, keepdims=True), 1e-20)


def write_chunk(path: Path, chunk: ChunkMesh) -> None:
    """Escribe el trozo en TNTM2: posiciones y normales cuantizadas, resto comprimido."""
    n, m, k = len(chunk.vertices), len(chunk.triangles), len(chunk.instances)
    verts = chunk.vertices.astype(np.float64)
    mins = verts.min(axis=0) if n else np.zeros(3)
    maxs = verts.max(axis=0) if n else np.zeros(3)
    step = np.maximum(maxs - mins, 1e-6) / 65535.0
    q = (np.clip(np.round((verts - mins) / step), 0, 65535) if n else np.zeros((0, 3))).astype("<u2")
    normals_enc = _encode_normals_oct(chunk.normals) if n else np.zeros((0, 2), "<i2")
    flat = chunk.triangles.astype(np.int64).ravel()
    prev = np.concatenate([[0], flat[:-1]]) if len(flat) else flat
    delta = (flat - prev).astype("<i4")
    payload = b"".join((q.tobytes(), normals_enc.tobytes(), chunk.colors.astype(np.uint8).tobytes(),
                        delta.tobytes(), chunk.instances.astype("<f4").tobytes()))
    compressed = zlib.compress(payload, level=6)
    with open(path, "wb") as handle:
        handle.write(HEADER2.pack(MAGIC, VERSION2, n, m, k, *mins.astype("<f4"), *step.astype("<f4"),
                                  len(payload), len(compressed)))
        handle.write(compressed)


def _read_chunk_v1(data: bytes, path: Path) -> dict:
    _, _, n, m, k = HEADER1.unpack_from(data, 0)
    offset = HEADER1.size
    out = {}
    for key, dtype, count, width in (("vertices", "<f4", n, 3), ("normals", "<f4", n, 3), ("colors", np.uint8, n, 4),
                                     ("triangles", "<u4", m, 3), ("instances", "<f4", k, 11)):
        size = np.dtype(dtype).itemsize * count * width
        out[key] = np.frombuffer(data, dtype=dtype, count=count * width, offset=offset).reshape(count, width)
        offset += size
    if offset != len(data):
        raise ValueError(f"{path}: {len(data) - offset} bytes de mas")
    return out


def _read_chunk_v2(data: bytes, path: Path) -> dict:
    _, _, n, m, k, ox, oy, oz, sx, sy, sz, raw_size, zlib_size = HEADER2.unpack_from(data, 0)
    offset = HEADER2.size
    if offset + zlib_size != len(data):
        raise ValueError(f"{path}: {len(data) - (offset + zlib_size)} bytes de mas")
    payload = zlib.decompress(data[offset:offset + zlib_size])
    if len(payload) != raw_size:
        raise ValueError(f"{path}: raw_size no coincide tras descomprimir")
    pos = 0
    q = np.frombuffer(payload, dtype="<u2", count=n * 3, offset=pos).reshape(n, 3); pos += n * 3 * 2
    nenc = np.frombuffer(payload, dtype="<i2", count=n * 2, offset=pos).reshape(n, 2); pos += n * 2 * 2
    colors = np.frombuffer(payload, dtype=np.uint8, count=n * 4, offset=pos).reshape(n, 4).copy(); pos += n * 4
    delta = np.frombuffer(payload, dtype="<i4", count=m * 3, offset=pos); pos += m * 3 * 4
    instances = np.frombuffer(payload, dtype="<f4", count=k * 11, offset=pos).reshape(k, 11).copy(); pos += k * 11 * 4
    if pos != len(payload):
        raise ValueError(f"{path}: {len(payload) - pos} bytes de payload de mas")
    origin, step = np.array([ox, oy, oz]), np.array([sx, sy, sz])
    vertices = (origin + q.astype(np.float64) * step).astype(np.float32)
    triangles = (np.cumsum(delta.astype(np.int64)).astype(np.uint32) if m else np.zeros(0, np.uint32)).reshape(m, 3)
    return {"vertices": vertices, "normals": _decode_normals_oct(nenc).astype(np.float32), "colors": colors,
            "triangles": triangles, "instances": instances}


def read_chunk(path: Path) -> dict:
    """Lee un trozo v1 o v2 (autodetectado por la version de la cabecera)."""
    data = path.read_bytes()
    if len(data) < 8 or data[:4] != MAGIC:
        raise ValueError(f"{path}: cabecera invalida")
    version = struct.unpack_from("<I", data, 4)[0]
    if version == VERSION1:
        return _read_chunk_v1(data, path)
    if version == VERSION2:
        return _read_chunk_v2(data, path)
    raise ValueError(f"{path}: version {version} desconocida")


def write_map(out: Path, name: str, seed: int, chunks: dict[tuple[int, int], ChunkMesh], start, end, zone_map,
              route_points, style=None, extra_manifest: dict | None = None) -> None:
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
    manifest = {"name": name, "seed": seed, "format": "TNTM2", "cell_uu": CELL_M * UU_PER_M, "grid": GRID,
                "water_uu": WATER_M * UU_PER_M, "start_uu": [v * UU_PER_M for v in start],
                "end_uu": [v * UU_PER_M for v in end], "cells": cells}
    if style is not None:
        from dataclasses import asdict
        manifest["style"] = asdict(style)
    if extra_manifest:
        manifest.update(extra_manifest)
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
            "algae": (0.87, 0.74, 0.48), "beach": (0.95, 0.86, 0.62)}
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
