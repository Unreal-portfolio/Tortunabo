"""Terreno barato alrededor del mapa: una corona de rejilla simple (sin volumen, sin colision) que
extiende el paisaje hasta OUTER_M de cada borde para que desde dentro no se vea el final.

Parte de la cota del borde del mapa (muestreada de la rejilla del modelo, asi que casa con la
malla de dentro), se funde en BLEND_M con dunas bajas y, al norte, baja al mar. Se exporta en
trozos TNTM2 de CELL_OUT_M (misma rejilla de 200 m alineada con el mapa) que el cargador crea sin
colision (campo "collision": false en el manifest)."""

from __future__ import annotations

from types import SimpleNamespace

import numpy as np
from scipy import ndimage

from terrain_vol.density import smooth
from terrain_vol.export import write_chunk
from terrain_vol.layout import UU_PER_M

from . import field
from .layout import MAP_MAX_M, MAP_MIN_M, STEP_XY_M, WATER_M

OUTER_M = 1000.0          # alcance de la corona desde cada borde del mapa
CELL_OUT_M = 200.0        # trozo de la corona
STEP_OUT_M = 5.0          # paso de la rejilla (barata: 41 x 41 vertices por trozo)
BLEND_M = 80.0            # tramo en el que la cota del borde pasa a las dunas de fuera
TUCK_M = 3.0              # la fila del borde de la corona se mete esto bajo el mapa
TUCK_DROP_M = 0.35        # ... y queda esto por debajo de su suelo
SEAM_DROP_M = 0.0        # la fila del borde, a la misma cota que la malla de dentro (con 0,4 m se veia la rendija)
DECIMATE_M = 0.25         # error maximo de la decimacion de la corona (sin colision, a 50 m o mas casi siempre)


def outer_height(model, X: np.ndarray, Y: np.ndarray) -> np.ndarray:
    xc = np.clip(X, MAP_MIN_M, MAP_MAX_M)
    yc = np.clip(Y, MAP_MIN_M, MAP_MAX_M)
    dist = np.hypot(X - xc, Y - yc)
    border = ndimage.map_coordinates(model.grid.height, [(xc - model.axis[0]) / STEP_XY_M,
                                                         (yc - model.axis[0]) / STEP_XY_M], order=1, mode="nearest")
    dunes = field.dune_field(model, X, Y, model.dune_angle, model.style.vista_dune_wave_m[1])
    far = WATER_M + 1.5 + 2.2 * dunes + 3.0 * model.n_big.unit(X, Y) + 6.0 * smooth(250.0, OUTER_M, dist)
    # Al norte (X por encima del mapa) esta el mar: la corona baja bajo el agua.
    sea = smooth(0.0, 40.0, X - MAP_MAX_M)
    far = far * (1.0 - sea) + (WATER_M - 4.0) * sea
    t = smooth(0.0, BLEND_M, dist)
    h = border * (1.0 - t) + far * t - SEAM_DROP_M * (dist < STEP_OUT_M)
    if model.canyon_field is not None:
        h = model.canyon_field.carve(X, Y, h, np.zeros(X.shape))
    return h


def _tuck_under_map(X: np.ndarray, Y: np.ndarray):
    """La fila de la corona que cae sobre el borde del mapa se mete TUCK_M hacia dentro y baja
    TUCK_DROP_M: el borde de la malla del mapa (marching cubes, 1 m) y el de la corona (5 m) no
    comparten vertices y entre los dos quedaban rendijas negras. Asi, bajo la rendija hay arena."""
    tol = 1e-6
    along_x = (Y >= MAP_MIN_M - tol) & (Y <= MAP_MAX_M + tol)
    along_y = (X >= MAP_MIN_M - tol) & (X <= MAP_MAX_M + tol)
    on = np.zeros(X.shape, dtype=bool)
    X, Y = X.copy(), Y.copy()
    for edge, sign in ((MAP_MIN_M, 1.0), (MAP_MAX_M, -1.0)):
        hit = (np.abs(X - edge) < tol) & along_x
        X[hit] += sign * TUCK_M
        on |= hit
        hit = (np.abs(Y - edge) < tol) & along_y
        Y[hit] += sign * TUCK_M
        on |= hit
    return X, Y, TUCK_DROP_M * on


def _cell_mesh(model, x0: float, y0: float, decimate_m: float = 0.0):
    n = int(round(CELL_OUT_M / STEP_OUT_M)) + 1
    # Una muestra de margen: gradiente centrado tambien en el borde (normales iguales en el vecino).
    xs = x0 + STEP_OUT_M * np.arange(-1, n + 1)
    ys = y0 + STEP_OUT_M * np.arange(-1, n + 1)
    Xp, Yp = np.meshgrid(xs, ys, indexing="ij")
    Xp, Yp, sink = _tuck_under_map(Xp, Yp)
    Hp = outer_height(model, Xp, Yp) - sink
    gx, gy = (g[1:-1, 1:-1] for g in np.gradient(Hp, STEP_OUT_M))
    X, Y, H = Xp[1:-1, 1:-1], Yp[1:-1, 1:-1], Hp[1:-1, 1:-1]
    normals = np.stack([-gx, -gy, np.ones_like(H)], axis=-1)
    normals /= np.linalg.norm(normals, axis=-1, keepdims=True)
    world = np.stack([X, Y, H], axis=-1).reshape(-1, 3)
    normals = normals.reshape(-1, 3)
    idx = np.arange(n * n).reshape(n, n)
    a, b, c, d = idx[:-1, :-1].ravel(), idx[1:, :-1].ravel(), idx[:-1, 1:].ravel(), idx[1:, 1:].ravel()
    tris = np.concatenate([np.stack([a, b, c], 1), np.stack([b, d, c], 1)])
    # Cara visible de Unreal: la opuesta a (B-A)x(C-A); con la normal hacia arriba, esa z < 0.
    cross = np.cross(world[tris[:, 1]] - world[tris[:, 0]], world[tris[:, 2]] - world[tris[:, 0]])
    flip = cross[:, 2] > 0.0
    tris[flip] = tris[flip][:, [0, 2, 1]]
    from terrain_vol.mesh import vertex_colors
    colors = vertex_colors(model, world, normals)
    center = np.array([x0 + CELL_OUT_M / 2.0, y0 + CELL_OUT_M / 2.0])
    local = (world - np.array([center[0], center[1], 0.0])) * UU_PER_M
    normals, tris = normals.astype(np.float32), tris.astype(np.uint32)
    if decimate_m > 0.0:
        # Dunas lejanas casi llanas: sobran la mayoria de los triangulos. El borde del trozo (y la
        # fila metida bajo el mapa) no se mueve.
        from terrain_vol.decimate import decimate_mesh
        local, normals, colors, tris = decimate_mesh(local, normals, colors, tris, max_error_m=decimate_m)
    return center, SimpleNamespace(vertices=local.astype(np.float32), normals=normals,
                                   colors=colors, triangles=tris,
                                   instances=np.zeros((0, 11), np.float32))


def write_outer(model, out, name: str, decimate_m: float = 0.0) -> list[dict]:
    """Escribe la corona en out/Outer/ y devuelve sus entradas de "cells" para el manifest.
    decimate_m > 0: decima cada trozo con ese error maximo (m)."""
    (out / "Outer").mkdir(exist_ok=True)
    cells = []
    start = MAP_MIN_M - OUTER_M
    count = int(round((MAP_MAX_M - MAP_MIN_M + 2 * OUTER_M) / CELL_OUT_M))
    for i in range(count):
        for j in range(count):
            x0, y0 = start + i * CELL_OUT_M, start + j * CELL_OUT_M
            inside = MAP_MIN_M <= x0 < MAP_MAX_M and MAP_MIN_M <= y0 < MAP_MAX_M
            if inside:
                continue
            center, mesh = _cell_mesh(model, x0, y0, decimate_m)
            file = f"Outer/o{i}_{j}.bin"
            write_chunk(out / file, mesh)
            cells.append({"name": f"M_{name}_outer_{i}_{j}", "file": file, "collision": False,
                          "center_uu": [center[0] * UU_PER_M, center[1] * UU_PER_M],
                          "vertices": int(len(mesh.vertices)), "triangles": int(len(mesh.triangles)), "instances": 0})
    return cells
