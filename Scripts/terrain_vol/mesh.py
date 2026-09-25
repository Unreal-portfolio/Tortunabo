"""Malla de cada trozo: marching cubes sobre la densidad, normales del gradiente global,
color de vertice por zona e instancias del bosque de algas.

Los trozos vecinos comparten el plano de su borde y evaluan la densidad con las mismas
funciones de mundo: los vertices y normales del borde salen identicos en los dos.
"""

from __future__ import annotations

from dataclasses import dataclass

import numpy as np
from scipy import ndimage
from skimage.measure import marching_cubes

from .density import Fields, MapModel, smooth
from .layout import (CELL_SAMPLES, STEP_XY_M, STEP_Z_M, UU_PER_M, WATER_M, Z_MAX_M, Z_MIN_M, Z_SAMPLES,
                     cell_bounds, cell_center)

# Paletas lineales (las de FModuleColors y TNTerrainBiome::ColorsFor): suelo, alto, alto alt,
# pared, pared alt, humedo.
PALETTES = {
    "sand": ((0.62, 0.44, 0.21), (0.48, 0.33, 0.16), (0.56, 0.36, 0.18), (0.40, 0.27, 0.13), (0.58, 0.45, 0.27),
             (0.09, 0.065, 0.035)),
    "beach": ((0.58, 0.47, 0.29), (0.40, 0.31, 0.18), (0.47, 0.35, 0.19), (0.30, 0.23, 0.14), (0.44, 0.36, 0.22),
              (0.12, 0.10, 0.07)),
    "algae": ((0.20, 0.22, 0.06), (0.12, 0.14, 0.045), (0.19, 0.20, 0.05), (0.07, 0.07, 0.03), (0.11, 0.10, 0.04),
              (0.05, 0.06, 0.025)),
}
ZONE_PALETTE = {"cliffs": "sand", "canyon": "sand", "dunes": "sand", "lake": "beach", "algae": "algae",
                "sand_end": "sand"}

FOLIAGE_SPACING_M = 2.8
FOLIAGE_SHAPES = {"bush": 0, "frond": 1, "stalk": 2}      # orden de ISM del tile (EFoliageShape)
FOLIAGE_COLORS = ((0.10, 0.14, 0.03), (0.27, 0.29, 0.07))


@dataclass
class ChunkMesh:
    col: int
    row: int
    vertices: np.ndarray        # (N, 3) float32, uu, locales al centro del trozo
    normals: np.ndarray         # (N, 3) float32
    colors: np.ndarray          # (N, 4) uint8, color LINEAL * 255
    triangles: np.ndarray       # (M, 3) uint32, cara visible segun Unreal
    instances: np.ndarray       # (K, 11) float32: forma, x, y, z (uu), yaw (grados), sx, sy, sz, r, g, b
    top: np.ndarray             # (CELL_SAMPLES, CELL_SAMPLES) cota de la superficie mas alta (m)
    standable: np.ndarray       # (CELL_SAMPLES, CELL_SAMPLES, Z_SAMPLES) bool: se puede estar de pie
    fields: Fields


def chunk_grid(col: int, row: int, pad: int = 0):
    x0, x1, y0, y1 = cell_bounds(col, row)
    xs = x0 + STEP_XY_M * np.arange(-pad, CELL_SAMPLES + pad)
    ys = y0 + STEP_XY_M * np.arange(-pad, CELL_SAMPLES + pad)
    X, Y = np.meshgrid(xs, ys, indexing="ij")
    Z = Z_MIN_M + STEP_Z_M * np.arange(Z_SAMPLES)
    return X, Y, Z


def hash_uniform(ix: np.ndarray, iy: np.ndarray, salt: int) -> np.ndarray:
    """Numero en [0, 1) determinista por celda de la rejilla global (independiente del trozo)."""
    h = (ix.astype(np.uint64) * np.uint64(0x9E3779B97F4A7C15)) ^ (iy.astype(np.uint64) * np.uint64(0xC2B2AE3D27D4EB4F)) \
        ^ np.uint64(salt * 0x165667B19E3779F9 & 0xFFFFFFFFFFFFFFFF)
    h ^= h >> np.uint64(33)
    h *= np.uint64(0xFF51AFD7ED558CCD)
    h ^= h >> np.uint64(33)
    h *= np.uint64(0xC4CEB9FE1A85EC53)
    h ^= h >> np.uint64(33)
    return (h >> np.uint64(11)).astype(np.float64) / float(1 << 53)


def vertex_colors(model: MapModel, world: np.ndarray, normals: np.ndarray) -> np.ndarray:
    x, y, z = world[:, 0], world[:, 1], world[:, 2]
    w = model.zones.weights(x, y)
    vein = 0.5 + 0.5 * np.sin(x / 53.0 + 1.7) * np.cos(y / 41.0 - 0.6)
    cliff = smooth(0.20, 0.55, 1.0 - normals[:, 2])
    cliff = np.maximum(cliff, (normals[:, 2] < -0.2).astype(np.float64))     # techos de tunel y voladizos
    high = smooth(4.0, 9.0, z)
    wet = smooth(WATER_M + 0.6, WATER_M, z)
    out = np.zeros((len(x), 3))
    for zone, palette_name in ZONE_PALETTE.items():
        floor, hi, hi_alt, wall, wall_alt, wet_c = (np.array(c) for c in PALETTES[palette_name])
        c = floor + (hi + (hi_alt - hi) * vein[:, None] - floor) * high[:, None]
        c = c + (wall + (wall_alt - wall) * vein[:, None] - c) * (0.6 * cliff)[:, None]
        c = c + (wet_c - c) * wet[:, None]
        out += w[zone][:, None] * c
    tint = 1.0 + 0.09 * np.sin(x / 9.5) * np.cos(y / 7.4) + 0.2 * cliff * np.sin(z * 2.5)
    out = np.clip(out * tint[:, None], 0.0, 1.0)
    rgba = np.concatenate([out, np.ones((len(x), 1))], axis=1)
    return np.rint(rgba * 255.0).astype(np.uint8)


def top_surface(D: np.ndarray) -> np.ndarray:
    """Cota (m) del paso de solido a aire mas alto de cada columna."""
    solid = D > 0.0
    k = Z_SAMPLES - 1 - np.argmax(solid[:, :, ::-1], axis=2)          # indice del solido mas alto
    k = np.clip(k, 0, Z_SAMPLES - 2)
    i, j = np.indices(k.shape)
    d0, d1 = D[i, j, k], D[i, j, k + 1]
    t = np.clip(d0 / np.maximum(d0 - d1, 1e-9), 0.0, 1.0)
    return Z_MIN_M + STEP_Z_M * (k + t)


def standable_cells(D: np.ndarray, clearance_m: float = 2.0) -> np.ndarray:
    """Solido con aire encima en clearance_m: donde cabe la tortuga de pie."""
    solid = D > 0.0
    free = ~solid
    steps = int(round(clearance_m / STEP_Z_M))
    ok = solid.copy()
    for k in range(1, steps + 1):
        above = np.zeros_like(free)
        above[:, :, :-k] = free[:, :, k:]
        ok &= above
    return ok


def foliage_instances(model: MapModel, col: int, row: int, fields: Fields, top: np.ndarray) -> np.ndarray:
    """Algas sobre una rejilla global de FOLIAGE_SPACING_M con desplazamiento por celda."""
    x0, x1, y0, y1 = cell_bounds(col, row)
    ix = np.arange(int(np.floor(x0 / FOLIAGE_SPACING_M)), int(np.ceil(x1 / FOLIAGE_SPACING_M)))
    iy = np.arange(int(np.floor(y0 / FOLIAGE_SPACING_M)), int(np.ceil(y1 / FOLIAGE_SPACING_M)))
    IX, IY = np.meshgrid(ix, iy, indexing="ij")
    px = (IX + hash_uniform(IX, IY, 1)) * FOLIAGE_SPACING_M
    py = (IY + hash_uniform(IX, IY, 2)) * FOLIAGE_SPACING_M
    inside = (px >= x0) & (px < x1) & (py >= y0) & (py < y1)
    gi = np.clip(np.rint(px - x0).astype(int), 0, CELL_SAMPLES - 1)
    gj = np.clip(np.rint(py - y0).astype(int), 0, CELL_SAMPLES - 1)
    density = fields.foliage[gi, gj]
    ground = top[gi, gj]
    keep = inside & (hash_uniform(IX, IY, 3) < density) & (ground > WATER_M + 0.3)
    shape_roll, size_roll = hash_uniform(IX, IY, 4)[keep], hash_uniform(IX, IY, 5)[keep]
    yaw, tint = hash_uniform(IX, IY, 6)[keep] * 360.0, hash_uniform(IX, IY, 7)[keep]
    cx, cy = cell_center(col, row)
    lx, ly = (px[keep] - cx) * UU_PER_M, (py[keep] - cy) * UU_PER_M
    gz = ground[keep] * UU_PER_M - 20.0
    tall = 400.0 + (1200.0 - 400.0) * size_roll ** 2
    bush = shape_roll < 0.25
    frond = (shape_roll >= 0.25) & (shape_roll < 0.55)
    width = 150.0 + 170.0 * size_roll
    shape = np.where(bush, 0, np.where(frond, 1, 2)).astype(np.float64)
    sx = np.where(bush, width, np.where(frond, 90.0, 45.0)) / 100.0
    sy = np.where(bush, width * 0.8, np.where(frond, 60.0, 45.0)) / 100.0
    sz = np.where(bush, width * 0.55, tall) / 100.0
    pivot = np.where(bush, width * 0.2, tall * 0.5)
    lo, hi = np.array(FOLIAGE_COLORS[0]), np.array(FOLIAGE_COLORS[1])
    color = lo + (hi - lo) * tint[:, None]
    return np.column_stack([shape, lx, ly, gz + pivot, yaw, sx, sy, sz, color]).astype(np.float32)


def build_chunk(model: MapModel, col: int, row: int) -> ChunkMesh:
    # Una muestra de margen para las normales (gradiente centrado tambien en el borde).
    X, Y, Z = chunk_grid(col, row, pad=1)
    fields_pad = model.fields(X, Y)
    D_pad = model.density(X, Y, Z, fields_pad)
    D = D_pad[1:-1, 1:-1, :]
    fields = Fields(**{k: (v[1:-1, 1:-1] if isinstance(v, np.ndarray) else {z: a[1:-1, 1:-1] for z, a in v.items()})
                       for k, v in fields_pad.__dict__.items()})

    verts, faces, _, _ = marching_cubes(D, level=0.0, spacing=(STEP_XY_M, STEP_XY_M, STEP_Z_M))
    x0, _, y0, _ = cell_bounds(col, row)
    world = verts + np.array([x0, y0, Z_MIN_M])

    grad = np.gradient(D_pad, STEP_XY_M, STEP_XY_M, STEP_Z_M)
    coords = np.stack([verts[:, 0] / STEP_XY_M + 1, verts[:, 1] / STEP_XY_M + 1, verts[:, 2] / STEP_Z_M], axis=0)
    g = np.stack([ndimage.map_coordinates(c, coords, order=1, mode="nearest") for c in grad], axis=1)
    normals = -g / np.maximum(np.linalg.norm(g, axis=1, keepdims=True), 1e-9)

    # Cara visible de Unreal: la opuesta a (B-A)x(C-A). Se orienta cada triangulo con la normal.
    a, b, c = world[faces[:, 0]], world[faces[:, 1]], world[faces[:, 2]]
    cross = np.cross(b - a, c - a)
    face_n = normals[faces].mean(axis=1)
    flip = np.einsum("ij,ij->i", cross, face_n) > 0.0
    faces = faces.copy()
    faces[flip] = faces[flip][:, [0, 2, 1]]

    cx, cy = cell_center(col, row)
    local = (world - np.array([cx, cy, 0.0])) * UU_PER_M
    top = top_surface(D)
    return ChunkMesh(col, row, local.astype(np.float32), normals.astype(np.float32), vertex_colors(model, world, normals),
                     faces.astype(np.uint32), foliage_instances(model, col, row, fields, top), top,
                     standable_cells(D), fields)
