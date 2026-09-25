"""Rejilla del mapa "camino primero": 4 x 4 trozos de 100 m (400 m de lado).

Mismas convenciones que terrain_vol/layout.py (X = Norte, Y = Este; el trozo (col, fila) tiene
su centro en (fila * CELL_M, col * CELL_M)); solo cambia el numero de trozos."""

from __future__ import annotations

from terrain_vol.layout import (CELL_M, CELL_SAMPLES, STEP_XY_M, STEP_Z_M, UU_PER_M, WATER_M,  # noqa: F401
                                Z_MAX_M, Z_MIN_M)

GRID = 4
MAP_MIN_M = -CELL_M / 2.0
MAP_MAX_M = GRID * CELL_M - CELL_M / 2.0
GRID_PAD = 1
