"""Utilidades de polilineas 2D (en metros)."""

from __future__ import annotations

import numpy as np
from scipy import ndimage


def resample(points: np.ndarray, step: float) -> tuple[np.ndarray, np.ndarray]:
    """Puntos cada 'step' metros de arco (el ultimo, exacto) y su arco."""
    seg = np.linalg.norm(np.diff(points, axis=0), axis=1)
    arc = np.concatenate(([0.0], np.cumsum(seg)))
    s = np.arange(0.0, arc[-1], step)
    if arc[-1] - s[-1] > 1e-6:
        s = np.append(s, arc[-1])
    out = np.stack([np.interp(s, arc, points[:, 0]), np.interp(s, arc, points[:, 1])], axis=1)
    return out, s


def tangents(points: np.ndarray) -> np.ndarray:
    t = np.gradient(points, axis=0)
    return t / np.maximum(np.linalg.norm(t, axis=1, keepdims=True), 1e-9)


def normals(points: np.ndarray) -> np.ndarray:
    t = tangents(points)
    return np.stack([-t[:, 1], t[:, 0]], axis=1)


def knot_noise(rng: np.random.Generator, arc: np.ndarray, spacing: tuple[float, float], lo: float, hi: float,
               mode: float | None = None) -> np.ndarray:
    """Valor suave a lo largo del arco: nudos cada 'spacing' m con valores en [lo, hi] (triangular
    con moda 'mode' si se da) interpolados y suavizados. Siempre dentro de [lo, hi]."""
    knots = [0.0]
    while knots[-1] < arc[-1]:
        knots.append(knots[-1] + float(rng.uniform(*spacing)))
    if mode is None:
        values = rng.uniform(lo, hi, len(knots))
    else:
        values = rng.triangular(lo, mode, hi, len(knots))
    v = np.interp(arc, knots, values)
    step = max(float(np.median(np.diff(arc))) if len(arc) > 1 else 1.0, 1e-6)
    return np.clip(ndimage.gaussian_filter1d(v, 4.0 / step, mode="nearest"), lo, hi)


def longest_straight(points: np.ndarray, window_m: float = 25.0, tol_deg: float = 2.0) -> float:
    """Longitud (m) del tramo recto mas largo: tramos de window_m cuyo rumbo cambia menos de
    tol_deg. 0 si ninguna ventana de window_m es recta. Espera puntos cada ~1 m."""
    pts, _ = resample(points, 1.0)
    t = tangents(pts)
    heading = np.unwrap(np.arctan2(t[:, 1], t[:, 0]))
    w = int(window_m)
    if len(heading) <= w:
        return 0.0
    windows = np.lib.stride_tricks.sliding_window_view(heading, w + 1)
    straight = np.ptp(windows, axis=1) < np.radians(tol_deg)
    best = run = 0
    for flag in straight:
        run = run + 1 if flag else 0
        best = max(best, run)
    return float(best - 1 + w) if best else 0.0
