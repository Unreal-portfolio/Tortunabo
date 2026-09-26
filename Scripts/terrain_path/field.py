"""Relieve a partir del camino (funciones puras sobre rejillas 2D).

Seccion "cuadrado curvado": suelo llano, esquina inferior redondeada de radio rc (min(1.5 m,
0.2 x semiancho)), pared de 'angulo' grados hasta la cresta H y remate redondeado. Nunca un
cuenco: el suelo no se curva hacia las paredes. Detras de la cresta, una banda de bloqueo y la
bajada (o subida) hasta el fondo de vistas."""

from __future__ import annotations

import math

import numpy as np
from scipy import ndimage

from terrain_vol.density import smooth, soft_max

from .layout import MAP_MAX_M, MAP_MIN_M, WATER_M


def soft_min(a, b, k: float):
    return -soft_max(-a, -b, k)


def rim_arrays(style) -> tuple[np.ndarray, np.ndarray]:
    rims = np.array([style.rim_cliffs_m, style.rim_water_m, style.rim_dunes_m, style.rim_beach_m])
    return rims, np.radians(np.array(style.wall_angle_deg))


FOOT_M = 3.5                # pie de la pared casi vertical: lo justo para cerrar el paso


RIM_FALL_DEG = (18.0, 40.0)  # caida de lo alto de la pared al relieve de fuera (varia): model.rim_envelope


def section(e, zf, z_soft, w, bw, n_rim, n_top, n_floor, style, guard=None, n_wall=None, H_in=None):
    """(relieve, cresta H, distancia e de la cresta). e = distancia al eje - semiancho (< 0 dentro).
    bw: pesos de bioma (..., 4). n_rim en 0..1, n_top y n_floor en -1..1.

    H_in (v2): relieve de fuera ya continuo entre caminos (lomas naturales y remate de la pared de
    todos los caminos cercanos, ver PathModel._fields). Con el, la pared sube desde el suelo hasta
    ese relieve y no se impone ninguna cresta propia del camino (que saltaba en las bisectrices)."""
    rims, angles = rim_arrays(style)
    lo, hi = bw @ rims[:, 0], bw @ rims[:, 1]
    rim = lo + (hi - lo) * n_rim
    tan = np.tan(bw @ angles)
    # Cresta continua (solo depende de la cota suavizada): la bajada a las vistas no marca las
    # bisectrices entre caminos. Junto al camino, nunca por debajo del borde minimo del bioma
    # sobre su propio suelo (si no, un camino vecino mas bajo dejaba una pared que se sube).
    H = z_soft + rim + 1.6 * n_top if H_in is None else H_in
    if guard is not None:
        # guard: suelo del camino mas alto de los cercanos (continuo); la cresta, y con ella la
        # banda que cierra el paso, nunca queda por debajo de su borde minimo.
        H = np.maximum(H, guard + lo)
    Hc = np.maximum(H, zf + np.maximum(lo, 0.6 * rim)) if H_in is None else H
    rc = np.minimum(1.5, 0.2 * w)
    t = e + rc
    fillet = rc - np.sqrt(np.maximum(rc * rc - np.clip(t, 0.0, rc) ** 2, 0.0))
    # Pie: pared casi vertical hasta FOOT_M sobre el suelo (no se puede subir). Encima, ladera de
    # 32 a 57 grados que cambia a lo largo de la pared (antes 45-80: demasiado vertical).
    n = np.zeros_like(e) if n_wall is None else n_wall
    foot = np.minimum(FOOT_M, np.maximum(Hc - zf - rc, 0.0))
    t_foot = rc + foot / np.maximum(tan, 1e-3)
    tan_up = np.tan(np.radians(32.0 + 25.0 * (0.5 + 0.5 * n)))
    upper = rc + foot + np.maximum(t - t_foot, 0.0) * tan_up
    rise = np.where(t <= 0.0, 0.0, np.where(t <= rc, fillet,
                    np.where(t <= t_foot, rc + (t - rc) * tan, upper)))
    floor = zf + 0.1 * n_floor
    height = soft_min(floor + rise, Hc, 0.6)
    crest_e = t_foot - rc + np.maximum(Hc - zf - rc - foot, 0.0) / np.maximum(tan_up, 1e-3)
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
    v = WATER_M - 1.3 + st.vista_pond_m * model.n_pond(X, Y) + amp * dunes + 2.0 * model.n_big.unit(X, Y)
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


def clip_spikes(height, limit: float = 0.4):
    """Recorta picos y pozos de una sola celda (restos de las cunas y cruces): ninguna celda
    queda mas de 'limit' por encima de su vecina mas alta ni por debajo de la mas baja."""
    if height.ndim != 2 or min(height.shape) < 3:
        return height
    ring = np.ones((3, 3), dtype=bool)
    ring[1, 1] = False
    top = ndimage.maximum_filter(height, footprint=ring, mode="nearest")
    low = ndimage.minimum_filter(height, footprint=ring, mode="nearest")
    return np.clip(height, low - limit, top + limit)
