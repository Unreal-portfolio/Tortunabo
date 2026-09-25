"""Dimensiones del mapa volumetrico y convenciones de coordenadas.

Mundo en metros: X = Norte (fila), Y = Este (columna), Z = arriba. La celda (col, fila) tiene su
centro en (fila * CELL_M, col * CELL_M), igual que el generador de Unreal, asi que el mapa va de
MAP_MIN_M a MAP_MAX_M en X e Y. Cada celda es un trozo de malla de CELL_M de lado.
"""

from __future__ import annotations

GRID = 6
CELL_M = 100.0
MAP_MIN_M = -CELL_M / 2.0
MAP_MAX_M = GRID * CELL_M - CELL_M / 2.0

STEP_XY_M = 1.0                 # voxel horizontal
STEP_Z_M = 0.5                  # voxel vertical
Z_MIN_M = -10.0
Z_MAX_M = 34.0
CELL_SAMPLES = int(CELL_M / STEP_XY_M) + 1          # muestras por lado de un trozo (borde incluido)
Z_SAMPLES = int(round((Z_MAX_M - Z_MIN_M) / STEP_Z_M)) + 1

WATER_M = -4.0                  # cota del agua (ModuleWaterLevel del generador = -400 uu)
UU_PER_M = 100.0


def cell_center(col: int, row: int) -> tuple[float, float]:
    return row * CELL_M, col * CELL_M


def cell_bounds(col: int, row: int) -> tuple[float, float, float, float]:
    """(x0, x1, y0, y1) del trozo, bordes incluidos."""
    cx, cy = cell_center(col, row)
    half = CELL_M / 2.0
    return cx - half, cx + half, cy - half, cy + half
