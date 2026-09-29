"""Georreferencia del mapa de España: proyeccion Web Mercator (la de las teselas de elevacion y la de
los mapas habituales) con el Norte arriba, sobre el mismo volumen de 6 x 6 trozos de 100 m que
terrain_vol (X = Norte, Y = Este, de MAP_MIN_M a MAP_MAX_M).

Un metro del juego son MERC_M_PER_M metros Mercator (a 40 N, 1 m Mercator son 0,766 m de suelo real):
con 2800, el volumen de 600 m cubre 1680 km Mercator, unos 1290 km de suelo entre Galicia y Menorca.
"""

from __future__ import annotations

import math
from pathlib import Path

import numpy as np

from terrain_vol.layout import CELL_M, GRID, MAP_MAX_M, MAP_MIN_M, UU_PER_M, WATER_M  # noqa: F401

EARTH_R_M = 6378137.0
WORLD_M = 2.0 * math.pi * EARTH_R_M
CENTER_LON, CENTER_LAT = -2.6, 39.9             # punto del mundo que cae en el centro del volumen
MERC_M_PER_M = 2800.0

RASTER_PX_M = 0.5                                # metros de juego por pixel de los rasters de datos
RASTER_PX = int(round((MAP_MAX_M - MAP_MIN_M) / RASTER_PX_M))
ELEVATION_OFFSET_M = 5000                        # los PNG de 16 bits guardan elevacion + este desfase

DATA_DIR = Path(__file__).resolve().parent / "data"
DEM_FILE = DATA_DIR / "ES_dem.png"
MASK_FILE = DATA_DIR / "ES_mask.png"


def lonlat_to_mercator(lon, lat):
    lon, lat = np.asarray(lon, dtype=np.float64), np.asarray(lat, dtype=np.float64)
    return (EARTH_R_M * np.radians(lon), EARTH_R_M * np.log(np.tan(np.pi / 4.0 + np.radians(lat) / 2.0)))


def mercator_to_lonlat(mx, my):
    mx, my = np.asarray(mx, dtype=np.float64), np.asarray(my, dtype=np.float64)
    return np.degrees(mx / EARTH_R_M), np.degrees(2.0 * np.arctan(np.exp(my / EARTH_R_M)) - np.pi / 2.0)


def _center() -> tuple[float, float]:
    cx, cy = lonlat_to_mercator(CENTER_LON, CENTER_LAT)
    return float(cx), float(cy)


def lonlat_to_game(lon, lat):
    """(X Norte, Y Este) en metros de juego."""
    mx, my = lonlat_to_mercator(lon, lat)
    cx, cy = _center()
    mid = (MAP_MIN_M + MAP_MAX_M) / 2.0
    return mid + (my - cy) / MERC_M_PER_M, mid + (mx - cx) / MERC_M_PER_M


def game_to_lonlat(X, Y):
    cx, cy = _center()
    mid = (MAP_MIN_M + MAP_MAX_M) / 2.0
    return mercator_to_lonlat(cx + (np.asarray(Y) - mid) * MERC_M_PER_M, cy + (np.asarray(X) - mid) * MERC_M_PER_M)


def raster_axis() -> np.ndarray:
    """Coordenada de juego del centro de cada pixel de un raster de datos (Norte o Este)."""
    return MAP_MIN_M + (np.arange(RASTER_PX) + 0.5) * RASTER_PX_M
