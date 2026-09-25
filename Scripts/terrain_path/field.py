"""Relieve a partir del camino (funciones puras sobre rejillas 2D).

Seccion "cuadrado curvado": suelo llano, esquina inferior redondeada de radio rc (min(1.5 m,
0.2 x semiancho)), pared de 'angulo' grados hasta la cresta H y remate redondeado. Nunca un
cuenco: el suelo no se curva hacia las paredes. Detras de la cresta, una banda de bloqueo y la
bajada (o subida) hasta el fondo de vistas."""

from __future__ import annotations

import math

import numpy as np

from terrain_vol.density import smooth, soft_max

from .layout import MAP_MAX_M, MAP_MIN_M, WATER_M


def soft_min(a, b, k: float):
    return -soft_max(-a, -b, k)


def rim_arrays(style) -> tuple[np.ndarray, np.ndarray]:
    rims = np.array([style.rim_cliffs_m, style.rim_water_m, style.rim_dunes_m, style.rim_beach_m])
    return rims, np.radians(np.array(style.wall_angle_deg))


def section(e, zf, z_soft, w, bw, n_rim, n_top, n_floor, style):
    """(relieve, cresta H, distancia e de la cresta). e = distancia al eje - semiancho (< 0 dentro).
    bw: pesos de bioma (..., 4). n_rim en 0..1, n_top y n_floor en -1..1."""
    rims, angles = rim_arrays(style)
    lo, hi = bw @ rims[:, 0], bw @ rims[:, 1]
    rim = lo + (hi - lo) * n_rim
    tan = np.tan(bw @ angles)
    H = np.maximum(z_soft + rim + 0.8 * n_top, zf + 0.6 * rim)
    rc = np.minimum(1.5, 0.2 * w)
    t = e + rc
    fillet = rc - np.sqrt(np.maximum(rc * rc - np.clip(t, 0.0, rc) ** 2, 0.0))
    rise = np.where(t <= 0.0, 0.0, np.where(t <= rc, fillet, rc + (t - rc) * tan))
    floor = zf + 0.1 * n_floor
    height = soft_min(floor + rise, H, 1.0)
    crest_e = np.maximum(H - zf - rc, 0.0) / np.maximum(tan, 1e-3)
    return height, H, crest_e


def dune_field(model, X, Y, angle: float, wave: float, rise: float = 0.62):
    along = X * math.cos(angle) + Y * math.sin(angle)
    across = -X * math.sin(angle) + Y * math.cos(angle)
    phase = (along + 0.9 * wave * model.n_dune_warp(X, Y) + 0.25 * across * model.n_dune_mix(X, Y)) / wave
    frac = phase - np.floor(phase)
    profile = np.where(frac < rise, frac / rise, (1.0 - frac) / (1.0 - rise))
    return smooth(0.0, 1.0, profile)


def vista(model, X, Y):
    """Fondo de vistas: dunas tupidas, lagos y dunas inundadas; lomas naturales en los bordes
    sur, este y oeste (tapan el final del mapa); al norte, el mar."""
    st = model.style
    mix = smooth(0.35, 0.65, model.n_dune_mix.unit(X, Y))
    w1, w2 = st.vista_dune_wave_m
    dunes = dune_field(model, X, Y, model.dune_angle, w1) * (1 - mix) \
        + dune_field(model, X, Y, model.dune_angle_2, w2) * mix
    a_lo, a_hi = st.vista_dune_amp_m
    amp = a_lo + (a_hi - a_lo) * smooth(0.25, 0.85, model.n_dune_amp.unit(X, Y))
    v = WATER_M - 0.4 + st.vista_pond_m * model.n_pond(X, Y) + amp * dunes + 2.0 * model.n_big.unit(X, Y)
    warp = 10.0 * model.n_edge(X, Y)
    near = np.maximum.reduce([1.0 - smooth(4.0, 30.0, d) for d in
                              (X - MAP_MIN_M + warp, Y - MAP_MIN_M + warp, MAP_MAX_M - Y + warp)])
    return v * (1.0 - near) + np.maximum(v, 7.0 + 4.0 * model.n_rim.unit(X, Y)) * near


def shore(model, X, Y, height, protect):
    """Franja norte: playa que baja al mar; baja tambien los bordes del tramo de playa para que el
    abanico final se funda con la orilla. 'protect' (0..1) conserva los bordes de los tramos que
    no son playa aunque pasen cerca de la costa (si no, se saldria del camino a la playa).
    Devuelve (relieve, peso de la franja)."""
    u = MAP_MAX_M - X + 10.0 * model.n_edge(X, Y)
    beach = WATER_M - 1.8 + 5.0 * smooth(4.0, 45.0, u)
    band = (1.0 - smooth(25.0, 55.0, u)) * (1.0 - protect)
    return height * (1.0 - band) + np.minimum(height, beach) * band, band
