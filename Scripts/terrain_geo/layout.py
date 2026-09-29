"""Georreferencia del mapa de España (E01): Web Mercator (la de las teselas de elevacion y la de los mapas
habituales) con el Norte arriba, sobre el mismo volumen de 6 x 6 trozos de 100 m que terrain_vol (X = Norte,
Y = Este, de MAP_MIN_M a MAP_MAX_M). Es la GeoRegion SPAIN_REGION con escala y centro fijos (los de E01).

Un metro del juego son MERC_M_PER_M metros Mercator (a 40 N, 1 m Mercator son 0,766 m de suelo real):
con 2800, el volumen de 600 m cubre 1680 km Mercator, unos 1290 km de suelo entre Galicia y Menorca.
"""

from __future__ import annotations

from pathlib import Path

import numpy as np

from terrain_vol.layout import CELL_M, GRID, MAP_MAX_M, MAP_MIN_M, UU_PER_M, WATER_M  # noqa: F401

from .region import GeoRegion
from .sources import EARTH_R_M, WORLD_M, lonlat_to_mercator, mercator_to_lonlat  # noqa: F401

CENTER_LON, CENTER_LAT = -2.6, 39.9             # punto del mundo que cae en el centro del volumen
MERC_M_PER_M = 2800.0

RASTER_PX_M = 0.5                                # metros de juego por pixel de los rasters de datos
RASTER_PX = int(round((MAP_MAX_M - MAP_MIN_M) / RASTER_PX_M))
ELEVATION_OFFSET_M = 5000                        # los PNG de 16 bits guardan elevacion + este desfase

DATA_DIR = Path(__file__).resolve().parent / "data"
DEM_FILE = DATA_DIR / "ES_dem.png"
MASK_FILE = DATA_DIR / "ES_mask.png"

SPAIN_REGION = GeoRegion("E01_espana", bbox=(-10.0, 35.0, 5.0, 44.5), country="ESP", edge="border",
                         projection="merc", center=(CENTER_LON, CENTER_LAT), scale=MERC_M_PER_M, grid=GRID, frame_m=0.0)


def lonlat_to_game(lon, lat):
    """(X Norte, Y Este) en metros de juego."""
    return SPAIN_REGION.game_projection().to_game(lon, lat)


def game_to_lonlat(X, Y):
    return SPAIN_REGION.game_projection().to_lonlat(X, Y)


def raster_axis() -> np.ndarray:
    """Coordenada de juego del centro de cada pixel de un raster de datos (Norte o Este)."""
    return MAP_MIN_M + (np.arange(RASTER_PX) + 0.5) * RASTER_PX_M
