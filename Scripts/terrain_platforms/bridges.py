"""Puentes colgantes del plano: eje, ancho y cotas de cada uno, sacados de la mascara naranja.

Cada mancha naranja del layout es un tramo. Si dos tramos se miran de frente a ambos lados de una
plataforma mas alta que las de sus extremos lejanos (el «cuello» oscuro del plano), son un solo
puente que la atraviesa: el modelo excava un arco en la roca para que el tablero pase.

El tablero une la cima de las plataformas de sus dos extremos y cuelga en curva (catenaria
suave) entre ambos, sin salirse de esos extremos.
"""

from __future__ import annotations

from dataclasses import dataclass
from itertools import combinations

import numpy as np
from scipy import ndimage
from scipy.spatial import cKDTree

from .layout import CLASS_BRIDGE, LAYOUT_ORIGIN_M, PLATFORM_ABOVE_WATER_M, PX_M, WATER_M

STEP_M = 0.5                    # separacion de las muestras del eje
EXTEND_M = 5.0                  # el eje se mete esto dentro de cada plataforma (empotra el tablero)
MIN_PIECE_M2 = 20.0             # menos que esto no es un puente (ruido del plano)
JOIN_MAX_GAP_M = 30.0           # distancia maxima entre los extremos de dos tramos que se unen
JOIN_OFFSET_M = 6.0             # desvio lateral maximo entre el eje de un tramo y el extremo del otro
TANGENT_BINS = 10               # muestras (1 m) con las que se mide la direccion de un extremo
SAG_PER_M = 0.035               # flecha del tablero por metro de luz
SAG_MIN_M, SAG_MAX_M = 0.5, 2.6
WIDTH_MIN_M, WIDTH_MAX_M = 3.6, 9.0


@dataclass(eq=False)
class Bridge:
    points: np.ndarray          # (K, 2) X, Y del eje (m), cada STEP_M
    arc: np.ndarray             # (K,) metros desde el primer extremo
    half_width: np.ndarray      # (K,) semiancho del tablero (m)
    z_ends: tuple[float, float]  # cota de la cara superior en cada extremo (m)
    sag: float                  # flecha maxima (m)
    tree: cKDTree
    span: tuple[float, float] = (0.0, 0.0)   # tramo colgante (m del eje): fuera de el, el tablero va a ras del extremo

    @property
    def length(self) -> float:
        return float(self.arc[-1])

    def deck_top(self, s: np.ndarray) -> np.ndarray:
        t = np.clip((s - self.span[0]) / (self.span[1] - self.span[0]), 0.0, 1.0)
        return self.z_ends[0] + (self.z_ends[1] - self.z_ends[0]) * t - self.sag * 4.0 * t * (1.0 - t)

    def query(self, X: np.ndarray, Y: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
        """(distancia al eje, indice de la muestra mas cercana) en cada (X, Y)."""
        dist, idx = self.tree.query(np.stack([X.ravel(), Y.ravel()], axis=1))
        return dist.reshape(X.shape), idx.reshape(X.shape)


def _piece(mask: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    """Eje (N, E en m locales) y ancho de una mancha alargada: cortes de 1 m por su eje principal."""
    ii, jj = np.nonzero(mask)
    pts = (np.stack([ii, jj], axis=1) + 0.5) * PX_M
    center = pts.mean(axis=0)
    axis = np.linalg.svd(pts - center, full_matrices=False)[2][0]
    perp = np.array([-axis[1], axis[0]])
    t = (pts - center) @ axis
    bins = np.floor(t - t.min()).astype(int)
    axis_pts, widths = [], []
    for b in np.unique(bins):
        sel = pts[bins == b]
        axis_pts.append(sel.mean(axis=0))
        widths.append(np.ptp((sel - center) @ perp) + PX_M)
    return (ndimage.gaussian_filter1d(np.array(axis_pts), 2.0, axis=0, mode="nearest"),
            ndimage.gaussian_filter1d(np.array(widths), 3.0, mode="nearest"))


def _tangent(points: np.ndarray, end: int) -> np.ndarray:
    """Direccion hacia fuera del tramo en su extremo 0 (primero) o -1 (ultimo)."""
    n = min(TANGENT_BINS, len(points) - 1)
    v = points[0] - points[n] if end == 0 else points[-1] - points[-1 - n]
    return v / max(np.linalg.norm(v), 1e-9)


def _platform_class_at(layout: np.ndarray, point: np.ndarray) -> int:
    """Clase (1..3) de la plataforma mas cercana a un punto (N, E en m locales)."""
    platform = np.isin(layout, list(PLATFORM_ABOVE_WATER_M))
    _, (ii, jj) = ndimage.distance_transform_edt(~platform, return_indices=True)
    i, j = np.clip((point / PX_M).astype(int), 0, layout.shape[0] - 1)
    return int(layout[ii[i, j], jj[i, j]])


def _faces(tangent: np.ndarray, offset: np.ndarray) -> bool:
    """El extremo con esa direccion apunta al punto offset (delante y casi sobre su eje)."""
    lateral = abs(tangent[0] * offset[1] - tangent[1] * offset[0])
    return bool(tangent @ offset > 0.0 and lateral <= JOIN_OFFSET_M)


def _try_join(layout, a, b):
    """Une los tramos a y b si se miran de frente a ambos lados de una plataforma mas alta."""
    for ea, eb in ((0, 0), (0, -1), (-1, 0), (-1, -1)):
        pa, pb = a[0][ea], b[0][eb]
        gap = np.linalg.norm(pb - pa)
        if gap > JOIN_MAX_GAP_M or gap < 1e-6:
            continue
        if not (_faces(_tangent(a[0], ea), pb - pa) and _faces(_tangent(b[0], eb), pa - pb)):
            continue
        far_a = _platform_class_at(layout, a[0][-1 if ea == 0 else 0])
        far_b = _platform_class_at(layout, b[0][-1 if eb == 0 else 0])
        near = min(_platform_class_at(layout, pa), _platform_class_at(layout, pb))
        if far_a != far_b or near <= far_a:
            continue
        pts_a, w_a = (a[0][::-1], a[1][::-1]) if ea == 0 else a
        pts_b, w_b = (b[0][::-1], b[1][::-1]) if eb == -1 else b
        return np.concatenate([pts_a, pts_b]), np.concatenate([w_a, w_b])
    return None


def _resample(points: np.ndarray, widths: np.ndarray) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    seg = np.linalg.norm(np.diff(points, axis=0), axis=1)
    arc = np.concatenate([[0.0], np.cumsum(seg)])
    s = np.arange(0.0, arc[-1], STEP_M)
    return (np.stack([np.interp(s, arc, points[:, k]) for k in range(2)], axis=1), np.interp(s, arc, widths), s)


def _extend(points: np.ndarray, widths: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    n = int(EXTEND_M / STEP_M)
    head = points[0] + _tangent(points, 0) * STEP_M * np.arange(n, 0, -1)[:, None]
    tail = points[-1] + _tangent(points, -1) * STEP_M * np.arange(1, n + 1)[:, None]
    return (np.concatenate([head, points, tail]),
            np.concatenate([np.full(n, widths[0]), widths, np.full(n, widths[-1])]))


def _make_bridge(layout: np.ndarray, points: np.ndarray, widths: np.ndarray) -> Bridge:
    z0 = WATER_M + PLATFORM_ABOVE_WATER_M[_platform_class_at(layout, points[0])]
    z1 = WATER_M + PLATFORM_ABOVE_WATER_M[_platform_class_at(layout, points[-1])]
    points, widths = _extend(points, widths)
    points, widths, arc = _resample(points, widths)
    world = points + LAYOUT_ORIGIN_M
    span = (EXTEND_M, float(arc[-1]) - EXTEND_M)
    sag = float(np.clip(SAG_PER_M * (span[1] - span[0]), SAG_MIN_M, SAG_MAX_M))
    half = np.clip(widths, WIDTH_MIN_M, WIDTH_MAX_M) / 2.0
    return Bridge(world, arc, half, (z0, z1), sag, cKDTree(world), span)


def extract_bridges(layout: np.ndarray) -> list[Bridge]:
    lab, count = ndimage.label(layout == CLASS_BRIDGE)
    pieces = [_piece(lab == k) for k in range(1, count + 1) if (lab == k).sum() * PX_M ** 2 >= MIN_PIECE_M2]
    merged = True
    while merged:
        merged = False
        for i, j in combinations(range(len(pieces)), 2):
            joined = _try_join(layout, pieces[i], pieces[j])
            if joined is not None:
                pieces[i] = joined
                pieces.pop(j)
                merged = True
                break
    return [_make_bridge(layout, pts, w) for pts, w in pieces]
