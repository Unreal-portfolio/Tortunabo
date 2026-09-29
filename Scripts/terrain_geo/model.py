"""Densidad 3D del mapa de España (E01): el relieve real (modelo digital de elevacion) y la frontera como
costa. Es un GeoModel (terrain_geo/geomodel.py) con la region SPAIN_REGION, los rasters ya guardados en
terrain_geo/data (ES_dem.png, ES_mask.png) y la exageracion fijada por la cima, no calibrada:

  España       WATER_M + LAND_BASE_M + cota_real * K + relieve fino        (K reduce 3480 m a PEAK_ABOVE_WATER_M)
  mar / fuera  WATER_M - SEA_FLOOR_M - profundizacion segun la batimetria  (Portugal, Francia y Marruecos
               son mar poco profundo: el mapa es España, rodeada de agua)

Con K = 40 / 3480 y 2145 m de suelo por m de juego, la exageracion efectiva es 24,7x (el Rally pide 3-6x: ver
GeoModel y calibrate). No cambiar estos valores: E01 ya esta generado y test_terrain_geo comprueba que sale igual.
"""

from __future__ import annotations

from .fetch_spain import load_dem, load_mask
from .geomodel import GeoModel, ReliefParams
from .heightfield import limit_slope  # noqa: F401  (se reexporta)
from .layout import SPAIN_REGION

SEED = 20260929
REAL_PEAK_M = 3480.0             # Mulhacen: la cota real que se lleva a PEAK_ABOVE_WATER_M
PEAK_ABOVE_WATER_M = 40.0        # 3480 m reales sobre el agua; tras suavizar, la cima queda en unos 27 m
SPAIN_PARAMS = ReliefParams()    # los valores por defecto de ReliefParams son los de E01
LAND_BASE_M = SPAIN_PARAMS.land_base_m
MAX_SLOPE = SPAIN_PARAMS.max_slope


class SpainModel(GeoModel):
    def __init__(self, seed: int = SEED, peak_above_water_m: float = PEAK_ABOVE_WATER_M):
        super().__init__(SPAIN_REGION, load_dem(), load_mask(), peak_above_water_m / REAL_PEAK_M, seed, SPAIN_PARAMS)
