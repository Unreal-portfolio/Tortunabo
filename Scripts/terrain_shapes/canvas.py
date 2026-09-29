"""Lienzo de los mapas parametricos (islas ficticias y arenas): coordenadas de diseño y distancias con signo.

Coordenadas de diseño (e, n): metros al Este y al Norte del CENTRO del volumen. El mundo del juego es
X = Norte, Y = Este (terrain_vol/layout.py): X = mid + n, Y = mid + e, con mid el centro del volumen de
`grid` x `grid` trozos de 100 m. Las distancias con signo (sd_*) son positivas dentro, en metros.
"""

from __future__ import annotations

from dataclasses import dataclass

import numpy as np

from terrain_geo.region import RASTER_PX_M, raster_axis
from terrain_vol.layout import CELL_M, MAP_MIN_M, WATER_M

SEABED_M = WATER_M - 2.0         # fondo junto a la costa (cota absoluta)


@dataclass(frozen=True)
class Canvas:
    grid: int
    px_m: float = RASTER_PX_M

    @property
    def mid(self) -> float:
        return MAP_MIN_M + self.grid * CELL_M / 2.0

    @property
    def half_m(self) -> float:
        return self.grid * CELL_M / 2.0

    def design_grid(self) -> tuple[np.ndarray, np.ndarray]:
        """(e, n) de cada pixel del raster del volumen, indexados [Norte, Este]."""
        axis = raster_axis(self.grid, self.px_m) - self.mid
        return np.broadcast_to(axis[None, :], (len(axis), len(axis))), np.broadcast_to(axis[:, None], (len(axis), len(axis)))

    def to_world(self, e: float, n: float) -> tuple[float, float]:
        return self.mid + n, self.mid + e

    def to_ij(self, e: float, n: float) -> tuple[int, int]:
        """Muestra de 1 m (indice de global_top) mas cercana al punto de diseño."""
        X, Y = self.to_world(e, n)
        return int(round(X - MAP_MIN_M)), int(round(Y - MAP_MIN_M))

    def to_design(self, X, Y):
        return np.asarray(Y) - self.mid, np.asarray(X) - self.mid


def smoothstep(e0: float, e1: float, x):
    t = np.clip((np.asarray(x, dtype=np.float64) - e0) / (e1 - e0), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def rotate(e, n, ce: float, cn: float, angle_deg: float):
    a = np.radians(angle_deg)
    de, dn = e - ce, n - cn
    return de * np.cos(a) + dn * np.sin(a), -de * np.sin(a) + dn * np.cos(a)


def sd_circle(e, n, ce: float, cn: float, r: float):
    return r - np.hypot(e - ce, n - cn)


def sd_ellipse(e, n, ce: float, cn: float, a: float, b: float, angle_deg: float = 0.0):
    """Aproximacion de la distancia a una elipse de semiejes a (a lo largo de angle_deg) y b."""
    u, v = rotate(e, n, ce, cn, angle_deg)
    return (1.0 - np.hypot(u / a, v / b)) * min(a, b)


def sd_ring(e, n, ce: float, cn: float, r_in: float, r_out: float):
    r = np.hypot(e - ce, n - cn)
    return np.minimum(r - r_in, r_out - r)


def sd_box(e, n, ce: float, cn: float, half_e: float, half_n: float, corner: float = 0.0, angle_deg: float = 0.0):
    u, v = rotate(e, n, ce, cn, angle_deg)
    qu, qv = np.abs(u) - (half_e - corner), np.abs(v) - (half_n - corner)
    outside = np.hypot(np.maximum(qu, 0.0), np.maximum(qv, 0.0)) + np.minimum(np.maximum(qu, qv), 0.0) - corner
    return -outside


def plateau(sd, height_above_water: float, shore_m: float, drop_m: float = 2.5):
    """Cota absoluta de una meseta de distancia con signo sd: height_above_water dentro (a shore_m del borde) y
    bajando por la orilla hasta el fondo (SEABED_M) drop_m fuera del contorno."""
    return SEABED_M + (WATER_M + height_above_water - SEABED_M) * smoothstep(-drop_m, shore_m, sd)
