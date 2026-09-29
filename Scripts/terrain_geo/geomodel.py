"""Modelo de densidad de una region real (GeoRegion): relieve del MDE con exageracion calibrada por pendiente y
la costa, la frontera o ambas como borde. Es el modelo de E01 (terrain_geo/model.py) generalizado.

Altura del suelo (m de juego, absoluta):
  tierra   WATER_M + land_base_m + (cota_real - nivel_del_agua) * k + relieve fino
  agua     WATER_M - sea_floor_m - profundizacion segun la batimetria
fundidas con la cobertura suavizada. k = exageracion / (m de suelo por m de juego): con exageracion 1 las
pendientes del juego son las reales (a la resolucion del voxel); E01 tiene 24,7x, el objetivo del Rally 3-6x.
"""

from __future__ import annotations

import math
from dataclasses import dataclass

import numpy as np
from scipy import ndimage

from terrain_vol.density import smooth
from terrain_vol.layout import DEFAULT_Z_RANGE, WATER_M, ZRange
from terrain_vol.noise import Fbm2D

from .heightfield import HeightfieldModel, limit_slope
from .region import RASTER_PX_M, GeoRegion, raster_axis


@dataclass(frozen=True)
class ReliefParams:
    """Parametros del relieve; los valores por defecto son los de E01."""
    land_base_m: float = 0.45            # la costa queda esto sobre el agua: playa, no marisma
    sea_floor_m: float = 1.5             # fondo del agua junto a la costa
    sea_deepen_m: float = 4.0            # el fondo baja hasta esto mas con la profundidad real
    sea_depth_scale_m: float = 250.0     # profundidad real (m) a la que se ha recorrido el 63 % de esa bajada
    border_sigma_px: float = 2.5         # suavizado de la cobertura (pixeles de 0,5 m)
    land_sigma_px: float = 3.0           # suavizado del relieve: lo que el voxel de 1 m no representa
    max_slope: float = 1.4               # pendiente objetivo de la relajacion (m por m)
    slope_iterations: int = 90
    detail_wavelength_m: float = 48.0
    detail_low_m: float = 0.12           # amplitud del relieve fino en llano...
    detail_high_m: float = 1.1           # ... y en sierra alta
    detail_band_real_m: tuple[float, float] = (400.0, 2200.0)    # cotas reales entre las que pasa de una a otra
    frame_taper_m: float = 0.0           # el relieve baja a la costa en esta franja junto al marco (0 = no)


def land_relief(dem: np.ndarray, coverage: np.ndarray, k: float, params: ReliefParams,
                water_level_m: float = 0.0) -> np.ndarray:
    """Relieve de la tierra en m de juego sobre la costa: el real sobre el nivel del agua, sin lo que el voxel no
    representa (suavizado ponderado por la cobertura, para no mezclar con el agua) y con la pendiente acotada."""
    weight = coverage
    land = np.maximum(dem - water_level_m, 0.0)
    smoothed = ndimage.gaussian_filter(land * weight, params.land_sigma_px) / np.maximum(
        ndimage.gaussian_filter(weight, params.land_sigma_px), 1e-3)
    return limit_slope(np.where(weight > 0.0, smoothed, land) * k, params.max_slope * RASTER_PX_M, params.slope_iterations)


def ground_height(dem: np.ndarray, coverage: np.ndarray, relief: np.ndarray, k: float, params: ReliefParams,
                  detail, axis: np.ndarray, water_level_m: float = 0.0) -> np.ndarray:
    X, Y = axis[:, None], axis[None, :]
    land_w = ndimage.gaussian_filter(coverage, params.border_sigma_px, mode="nearest")
    depth = np.maximum(water_level_m - dem, 0.0)
    lo, hi = params.detail_band_real_m
    detail_amp = params.detail_low_m + (params.detail_high_m - params.detail_low_m) * smooth(lo * k, hi * k, relief)
    land_h = WATER_M + params.land_base_m + relief + detail_amp * detail(X, Y)
    sea_h = WATER_M - params.sea_floor_m - params.sea_deepen_m * (1.0 - np.exp(-depth / params.sea_depth_scale_m))
    return sea_h + (land_h - sea_h) * land_w


def real_slopes(dem: np.ndarray, land: np.ndarray, ground_px_m: float, sigma_px: float) -> np.ndarray:
    """Pendientes reales (m por m) de la tierra a la resolucion del voxel (el MDE suavizado como el modelo)."""
    smoothed = ndimage.gaussian_filter(np.maximum(dem, 0.0), sigma_px)
    gy, gx = np.gradient(smoothed, ground_px_m)
    return np.hypot(gx, gy)[land]


def calibrate_exaggeration(slopes: np.ndarray, target_slope_deg: float, percentile: float = 90.0,
                           bounds: tuple[float, float] = (3.0, 6.0)) -> float:
    """Exageracion vertical que lleva el percentil `percentile` de las pendientes reales a target_slope_deg,
    acotada a bounds (las pendientes del juego son las reales por la exageracion)."""
    if len(slopes) == 0:
        return bounds[0]
    real = float(np.percentile(slopes, percentile))
    wanted = math.tan(math.radians(target_slope_deg)) / max(real, 1e-6)
    return float(np.clip(wanted, *bounds))


@dataclass(frozen=True)
class Calibration:
    exaggeration: float           # efectiva: pendiente del juego / pendiente real
    k: float                      # m de juego por m de cota real
    ground_m_per_game_m: float
    z_range: ZRange
    capped: bool                  # la cima no cabia y se bajo la exageracion (o se amplio el rango)


def calibrate(region: GeoRegion, dem: np.ndarray, coverage: np.ndarray, target_slope_deg: float,
              bounds: tuple[float, float] = (3.0, 6.0), params: ReliefParams = ReliefParams(),
              exaggeration: float | None = None, headroom_m: float = 3.0) -> Calibration:
    """Exageracion por pendiente objetivo (o la dada); si la cima no cabe en DEFAULT_Z_RANGE se baja la
    exageracion hasta bounds[0] y, si aun no cabe, se amplia el rango vertical."""
    ground = region.game_projection().ground_m_per_game_m()
    land = coverage > 0.5
    slopes = real_slopes(dem - region.water_level_m, land, ground * RASTER_PX_M, params.land_sigma_px)
    e = exaggeration if exaggeration is not None else calibrate_exaggeration(slopes, target_slope_deg, bounds=bounds)
    peak_real = float((dem[land] - region.water_level_m).max()) if land.any() else 0.0
    room = DEFAULT_Z_RANGE.z_max_m - headroom_m - (WATER_M + params.land_base_m + params.detail_high_m)
    k = e / ground
    capped = False
    if peak_real * k > room and peak_real > 0.0:
        capped = True
        e = max(min(bounds[0], e), room / peak_real * ground)
        k = e / ground
    top = WATER_M + params.land_base_m + params.detail_high_m + peak_real * k
    return Calibration(e, k, ground, ZRange.covering(top, headroom_m=headroom_m), capped)


class GeoModel(HeightfieldModel):
    """Densidad de una region real a partir de sus rasters (dem en m reales y cobertura 0..1, [Norte, Este])."""

    def __init__(self, region: GeoRegion, dem: np.ndarray, coverage: np.ndarray, k: float, seed: int,
                 params: ReliefParams = ReliefParams(), z_range: ZRange = DEFAULT_Z_RANGE):
        self.region, self.seed, self.k, self.params, self.z_range = region, seed, k, params, z_range
        self.dem, self.coverage = dem, coverage
        rng = np.random.default_rng(seed)
        self.n_detail = Fbm2D(rng, params.detail_wavelength_m, octaves=4)
        super().__init__(self._build_height())

    def _relief(self) -> np.ndarray:
        relief = land_relief(self.dem, self.coverage, self.k, self.params, self.region.water_level_m)
        if self.params.frame_taper_m > 0.0:
            relief = relief * smooth(0.0, self.params.frame_taper_m, self.region.frame_mask())
        return relief

    def _build_height(self) -> np.ndarray:
        return ground_height(self.dem, self.coverage, self._relief(), self.k, self.params, self.n_detail,
                             raster_axis(self.region.grid), self.region.water_level_m)

    def to_game(self, lon: float, lat: float) -> tuple[float, float]:
        X, Y = self.region.game_projection().to_game(lon, lat)
        return float(X), float(Y)
