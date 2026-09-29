"""Densidad 3D del mapa de España: el relieve real (modelo digital de elevacion) y la frontera como
costa, con la misma interfaz que terrain_vol.density.MapModel para el resto de la cadena (marching
cubes, color de vertice, exportador TNTM2): chunk_fields(col, row, pad) y density(X, Y, Z, fields).

Altura del suelo (m de juego, absoluta):
  España       WATER_M + LAND_BASE_M + cota_real * K + relieve fino        (K reduce 3480 m a PEAK_ABOVE_WATER_M)
  mar / fuera  WATER_M - SEA_FLOOR_M - profundizacion segun la batimetria  (Portugal, Francia y Marruecos
               son mar poco profundo: el mapa es España, rodeada de agua)
Las dos se funden con la cobertura de la frontera suavizada, asi que la frontera terrestre cae como un
acantilado al mar y la costa baja como una playa. Sin ruido 3D de pared: en un campo de alturas escala en
rayas horizontales. Densidad positiva = solido, en metros.
"""

from __future__ import annotations

import numpy as np
from scipy import ndimage

from terrain_vol.density import Fields, smooth
from terrain_vol.layout import CELL_SAMPLES, STEP_XY_M
from terrain_vol.noise import Fbm2D

from .fetch_spain import load_dem, load_mask
from .layout import CELL_M, MAP_MIN_M, RASTER_PX_M, WATER_M, raster_axis

SEED = 20260929
REAL_PEAK_M = 3480.0             # Mulhacen: la cota real que se lleva a PEAK_ABOVE_WATER_M
PEAK_ABOVE_WATER_M = 40.0        # 3480 m reales sobre el agua; tras suavizar, la cima queda en unos 27 m (Z_MAX_M es 34 m)
LAND_BASE_M = 0.45               # la costa queda esto sobre el agua: playa, no marisma
SEA_FLOOR_M = 1.5                # fondo del mar junto a la costa, bajo el agua
SEA_DEEPEN_M = 4.0               # el fondo baja hasta esto mas con la profundidad real (Z_MIN_M = -10)
SEA_DEPTH_SCALE_M = 250.0        # profundidad real (m) a la que se ha recorrido el 63 % de esa bajada
BORDER_SIGMA_PX = 2.5            # suavizado de la cobertura de la frontera (pixeles de 0,5 m)
LAND_SIGMA_PX = 3.0              # suavizado del relieve real (1,5 m de juego): lo que el voxel de 1 m no puede representar
MAX_SLOPE = 1.4                  # pendiente objetivo de la relajacion (m por m, 54 grados); tras SLOPE_ITERATIONS el p99 queda en ~1,9
SLOPE_ITERATIONS = 90
DETAIL_WAVELENGTH_M = 48.0       # relieve fino: 4 octavas desde 48 m
DETAIL_LOW_M, DETAIL_HIGH_M = 0.12, 1.1     # amplitud (m) en llano y en sierra alta
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


class SpainModel:
    def __init__(self, seed: int = SEED, peak_above_water_m: float = PEAK_ABOVE_WATER_M):
        self.seed = seed
        self.k = peak_above_water_m / REAL_PEAK_M
        rng = np.random.default_rng(seed)
        self.n_detail = Fbm2D(rng, DETAIL_WAVELENGTH_M, octaves=4)
        self.dem = load_dem()
        self.coverage = load_mask()
        self.height = self._build_height()
        self.high_tint = 1.0
        self.wall_color_mix = 0.7
        self.trail_color = (0.4, 0.28, 0.14)
        self.trail_strength = 0.0

    # ── Suelo ────────────────────────────────────────────────────────────────────
    def _relief(self) -> np.ndarray:
        """Relieve de España en m de juego sobre la costa: el real, sin lo que el voxel no representa
        (suavizado ponderado por la cobertura, para no mezclar con el mar ni con Portugal) y con la pendiente
        acotada (relajacion: el material sobrante de una arista baja por las laderas, como un talud)."""
        weight = self.coverage
        land = np.maximum(self.dem, 0.0)
        smoothed = ndimage.gaussian_filter(land * weight, LAND_SIGMA_PX) / np.maximum(
            ndimage.gaussian_filter(weight, LAND_SIGMA_PX), 1e-3)
        return limit_slope(np.where(weight > 0.0, smoothed, land) * self.k, MAX_SLOPE * RASTER_PX_M, SLOPE_ITERATIONS)

    def _build_height(self) -> np.ndarray:
        axis = raster_axis()
        X, Y = axis[:, None], axis[None, :]
        land_w = ndimage.gaussian_filter(self.coverage, BORDER_SIGMA_PX, mode="nearest")
        relief = self._relief()
        depth = np.maximum(-self.dem, 0.0)
        detail_amp = DETAIL_LOW_M + (DETAIL_HIGH_M - DETAIL_LOW_M) * smooth(400.0 * self.k, 2200.0 * self.k, relief)
        land_h = WATER_M + LAND_BASE_M + relief + detail_amp * self.n_detail(X, Y)
        sea_h = WATER_M - SEA_FLOOR_M - SEA_DEEPEN_M * (1.0 - np.exp(-depth / SEA_DEPTH_SCALE_M))
        return sea_h + (land_h - sea_h) * land_w

    def _sample(self, raster: np.ndarray, X: np.ndarray, Y: np.ndarray) -> np.ndarray:
        coords = [(X - MAP_MIN_M) / RASTER_PX_M - 0.5, (Y - MAP_MIN_M) / RASTER_PX_M - 0.5]
        return ndimage.map_coordinates(raster, coords, order=1, mode="nearest")

    def ground_height(self, X: np.ndarray, Y: np.ndarray) -> np.ndarray:
        return self._sample(self.height, X, Y)

    # ── Interfaz de MapModel ─────────────────────────────────────────────────────
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
