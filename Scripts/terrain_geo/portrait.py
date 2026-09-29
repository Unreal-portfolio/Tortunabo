"""Mapa a partir de una foto: la imagen es el relieve (brillo = altura) y su silueta, una isla.

Sirve para retratos y para cualquier imagen con contraste: los claros suben y los oscuros bajan (en un
rostro, la frente, la nariz y los pomulos son montes; los ojos, la boca y las sombras, valles). La isla es un
ovalo sobre la region de interes de la imagen (`focus`), centrado en el volumen y rodeado de mar; fuera del
ovalo no hay nada, asi que el fondo de la foto no entra y la malla queda cerrada. Foto -> mapa no lleva datos externos: la imagen se lee de donde este y no se copia al repositorio.
"""

from __future__ import annotations

from pathlib import Path

import numpy as np
from PIL import Image
from scipy import ndimage

from .heightfield import HeightfieldModel, limit_slope
from .layout import MAP_MAX_M, MAP_MIN_M, RASTER_PX, RASTER_PX_M, WATER_M, raster_axis

SIZE_M = 540.0                   # alto de la isla en el volumen (de 600 m)
FOCUS = (0.5, 0.5, 0.5, 0.5)     # region de interes por defecto: la imagen entera
RELIEF_M = 22.0                  # amplitud del brillo (m de juego)
DOME_M = 5.0                     # la isla sube hacia el centro esto mas: se lee como cabeza, no como lamina
LAND_BASE_M = 0.45               # la costa queda esto sobre el agua
SEA_M = 1.5                      # fondo del mar bajo el agua
TEXTURE_SIGMA_PX = 1.6           # suavizado de la imagen (px de imagen): quita el grano y la piel
LOCAL_SIGMA_PX = 28.0            # escala del contraste local (px de imagen)
LOCAL_SHARE = 0.55               # parte del relieve que sale del contraste local (el resto, del brillo)
COAST_SIGMA_M = 3.0              # suavizado de la costa
SLOPE = 1.6                      # pendiente objetivo (m por m)
SLOPE_ITERATIONS = 60


def load_luminance(path: Path) -> np.ndarray:
    """Brillo de la imagen en [0, 1]; la fila 0 es el Norte (arriba de la foto)."""
    with Image.open(path) as image:
        gray = np.asarray(image.convert("L"), dtype=np.float64) / 255.0
    return gray


def normalize(field: np.ndarray, low: float = 2.0, high: float = 98.0) -> np.ndarray:
    lo, hi = np.percentile(field, [low, high])
    return np.clip((field - lo) / max(hi - lo, 1e-9), 0.0, 1.0)


def enhance(gray: np.ndarray) -> np.ndarray:
    """Relieve en [0, 1] de una foto: brillo suavizado mezclado con el contraste local (la iluminacion de una
    foto casera es desigual y, sola, dejaria media cara como una meseta)."""
    smooth = ndimage.gaussian_filter(gray, TEXTURE_SIGMA_PX)
    base = ndimage.gaussian_filter(smooth, LOCAL_SIGMA_PX)
    spread = ndimage.gaussian_filter(np.abs(smooth - base), LOCAL_SIGMA_PX) + 1e-3
    local = normalize((smooth - base) / spread)
    return LOCAL_SHARE * local + (1.0 - LOCAL_SHARE) * normalize(smooth)


def place(image: np.ndarray, m_per_px: float, center_uv: tuple[float, float]) -> np.ndarray:
    """La imagen en el raster del volumen, con su punto center_uv (0-1) en el centro y m_per_px de escala."""
    height, width = image.shape
    axis = raster_axis()
    mid = (MAP_MIN_M + MAP_MAX_M) / 2.0
    v = center_uv[1] * height + (mid - axis)[:, None] / m_per_px - 0.5           # fila de la imagen (Norte arriba)
    u = center_uv[0] * width + (axis - mid)[None, :] / m_per_px - 0.5            # columna de la imagen
    v, u = np.broadcast_to(v, (RASTER_PX, RASTER_PX)), np.broadcast_to(u, (RASTER_PX, RASTER_PX))
    return ndimage.map_coordinates(image, [v, u], order=1, mode="nearest")


def oval_distance(a_m: float, b_m: float) -> np.ndarray:
    """Distancia con signo (m, positiva dentro) al ovalo de semiejes Este a_m y Norte b_m, centrado en el volumen."""
    axis = raster_axis()
    mid = (MAP_MIN_M + MAP_MAX_M) / 2.0
    r = np.hypot((axis[None, :] - mid) / a_m, (axis[:, None] - mid) / b_m)
    return (1.0 - r) * min(a_m, b_m)


class PortraitModel(HeightfieldModel):
    def __init__(self, image_path: Path, size_m: float = SIZE_M, relief_m: float = RELIEF_M,
                 focus: tuple[float, float, float, float] = FOCUS):
        """focus = (u, v, radio_u, radio_v) de la region de interes, en fracciones del ancho y del alto de la imagen;
        size_m es el alto de la isla en el volumen."""
        gray = load_luminance(image_path)
        height, width = gray.shape
        cu, cv, ru, rv = focus
        self.center_uv, self.size_m = (cu, cv), size_m
        self.m_per_px = size_m / (2.0 * rv * height)
        self.image_size = (width, height)
        relief01 = place(enhance(gray), self.m_per_px, self.center_uv)
        dist = oval_distance(ru * width * self.m_per_px, size_m / 2.0)
        self.coverage = ndimage.gaussian_filter(np.clip(dist / COAST_SIGMA_M + 0.5, 0.0, 1.0), 1.5)
        self.land = dist > 0.0
        edge = np.clip(dist / (0.12 * size_m), 0.0, 1.0)                              # el relieve muere hacia la costa
        dome = DOME_M * np.sqrt(np.clip(dist / (0.5 * size_m * min(1.0, ru * width / (rv * height))), 0.0, 1.0))
        land = limit_slope(relief_m * relief01 * edge + dome, SLOPE * RASTER_PX_M, SLOPE_ITERATIONS)
        sea = WATER_M - SEA_M
        super().__init__(sea + (WATER_M + LAND_BASE_M + land - sea) * self.coverage)

    def to_game(self, u: float, v: float) -> tuple[float, float]:
        """(X Norte, Y Este) del punto de la imagen (u de 0 a 1 hacia el Este, v de 0 a 1 hacia el Sur)."""
        mid = (MAP_MIN_M + MAP_MAX_M) / 2.0
        width, height = self.image_size
        return mid + (self.center_uv[1] - v) * height * self.m_per_px, mid + (u - self.center_uv[0]) * width * self.m_per_px

