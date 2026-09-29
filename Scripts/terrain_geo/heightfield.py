"""Modelo de densidad de un campo de alturas (un raster de todo el volumen): la base de los mapas que salen de
un MDE o de una imagen. Misma interfaz que terrain_vol.density.MapModel para el resto de la cadena (marching cubes,
color de vertice, exportador TNTM2): chunk_fields(col, row, pad) y density(X, Y, Z, fields).

El raster `height` es la cota absoluta del suelo (m de juego), indexado [Norte, Este] a RASTER_PX_M por pixel.
Densidad positiva = solido: h(X, Y) - Z. Sin ruido 3D de pared: en un campo de alturas escala la ladera en rayas.
"""

from __future__ import annotations

import numpy as np
from scipy import ndimage

from terrain_vol.density import Fields, smooth
from terrain_vol.layout import CELL_SAMPLES, STEP_XY_M

from .layout import CELL_M, MAP_MIN_M, RASTER_PX_M, WATER_M

ZONES = ("cliffs", "canyon", "marsh", "algae", "beach")


def limit_slope(field: np.ndarray, max_diff: float, iterations: int) -> np.ndarray:
    """Relaja el campo hasta que ningun pixel supere max_diff de desnivel con sus 4 vecinos: cada pixel cede
    un cuarto del exceso hacia cada vecino mas bajo (y recibe de los mas altos). Conserva el volumen."""
    out = field.copy()
    for _ in range(iterations):
        delta = np.zeros_like(out)
        for axis in (0, 1):
            for sign in (1, -1):
                diff = out - np.roll(out, sign, axis=axis)
                delta -= 0.25 * np.sign(diff) * np.clip(np.abs(diff) - max_diff, 0.0, None)
        out += delta
    return out


class HeightfieldModel:
    high_tint = 1.0
    wall_color_mix = 0.7
    trail_color = (0.4, 0.28, 0.14)
    trail_strength = 0.0

    def __init__(self, height: np.ndarray):
        self.height = height

    def _sample(self, raster: np.ndarray, X: np.ndarray, Y: np.ndarray) -> np.ndarray:
        coords = [(X - MAP_MIN_M) / RASTER_PX_M - 0.5, (Y - MAP_MIN_M) / RASTER_PX_M - 0.5]
        return ndimage.map_coordinates(raster, coords, order=1, mode="nearest")

    def ground_height(self, X: np.ndarray, Y: np.ndarray) -> np.ndarray:
        return self._sample(self.height, X, Y)

    def chunk_fields(self, col: int, row: int, pad: int = 0) -> tuple[Fields, np.ndarray, np.ndarray]:
        n = CELL_SAMPLES + 2 * pad
        x0 = row * CELL_M - CELL_M / 2.0 - pad * STEP_XY_M
        y0 = col * CELL_M - CELL_M / 2.0 - pad * STEP_XY_M
        X, Y = np.meshgrid(x0 + STEP_XY_M * np.arange(n), y0 + STEP_XY_M * np.arange(n), indexing="ij")
        zeros = np.zeros(X.shape)
        weights = {zone: (np.ones(X.shape) if zone == "cliffs" else zeros.copy()) for zone in ZONES}
        fields = Fields(weights=weights, d_route=zeros, s_route=zeros, height=self.ground_height(X, Y), floor=zeros,
                        wall_band=zeros, foliage=zeros, tunnel=zeros, path=zeros)
        return fields, X, Y

    def color_weights(self, x: np.ndarray, y: np.ndarray) -> dict[str, np.ndarray]:
        """Playa junto al agua, arena en el resto (la paleta se oscurece sola con la altura)."""
        beach = smooth(WATER_M + 3.5, WATER_M + 0.6, self.ground_height(x, y))
        return {zone: {"beach": beach, "cliffs": 1.0 - beach}.get(zone, np.zeros(len(x))) for zone in ZONES}

    def trail_mask(self, x: np.ndarray, y: np.ndarray) -> np.ndarray:
        return np.zeros(len(x))

    def plaza_mask(self, x: np.ndarray, y: np.ndarray) -> np.ndarray:
        return np.zeros(len(x))

    def density(self, X: np.ndarray, Y: np.ndarray, Z: np.ndarray, fields: Fields | None = None) -> np.ndarray:
        return self.ground_height(X, Y)[..., None] - Z[None, None, :]
