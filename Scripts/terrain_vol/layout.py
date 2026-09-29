"""Dimensiones del mapa volumetrico y convenciones de coordenadas.

Mundo en metros: X = Norte (fila), Y = Este (columna), Z = arriba. La celda (col, fila) tiene su
centro en (fila * CELL_M, col * CELL_M), igual que el generador de Unreal, asi que el mapa va de
MAP_MIN_M a MAP_MAX_M en X e Y. Cada celda es un trozo de malla de CELL_M de lado.
"""

from __future__ import annotations

from dataclasses import dataclass

import numpy as np

GRID = 6
CELL_M = 100.0
MAP_MIN_M = -CELL_M / 2.0
MAP_MAX_M = GRID * CELL_M - CELL_M / 2.0

STEP_XY_M = 1.0                 # voxel horizontal
STEP_Z_M = 0.5                  # voxel vertical
Z_MIN_M = -10.0
# Techo de DISEÑO de los modelos antiguos (Mapa01, C01): lo leen el ruido 3D (tamaño de la red, que decide
# los numeros aleatorios) y TOP_LIMIT_M. No es el rango del voxelizado: cambiarlo cambiaria esos mapas.
Z_MAX_M = 34.0
LEGACY_Z_LEVELS = 89            # rango del voxelizado hasta 2026-09-29: de Z_MIN_M a Z_MAX_M (44,5 m)
Z_LEVELS = 128                  # rango por defecto: 128 niveles de STEP_Z_M (64 m, de -10 a 53,5 m)
Z_TOP_M = Z_MIN_M + (Z_LEVELS - 1) * STEP_Z_M
CELL_SAMPLES = int(CELL_M / STEP_XY_M) + 1          # muestras por lado de un trozo (borde incluido)
Z_SAMPLES = Z_LEVELS

WATER_M = -4.0                  # cota del agua (ModuleWaterLevel del generador = -400 uu)
UU_PER_M = 100.0


def cell_center(col: int, row: int) -> tuple[float, float]:
    return row * CELL_M, col * CELL_M


def cell_bounds(col: int, row: int) -> tuple[float, float, float, float]:
    """(x0, x1, y0, y1) del trozo, bordes incluidos."""
    cx, cy = cell_center(col, row)
    half = CELL_M / 2.0
    return cx - half, cx + half, cy - half, cy + half


@dataclass(frozen=True)
class ZRange:
    """Rango vertical del voxelizado: `levels` niveles de `step_m` desde `z_min_m` (m de juego, absolutos).

    Un modelo puede fijar el suyo con un atributo `z_range`; si no, build_chunk usa DEFAULT_Z_RANGE. Ampliar el
    rango por arriba no cambia la malla de un terreno que ya cabia: solo añade niveles de aire."""
    z_min_m: float = Z_MIN_M
    levels: int = Z_LEVELS
    step_m: float = STEP_Z_M

    def __post_init__(self) -> None:
        if self.levels < 2 or self.step_m <= 0.0:
            raise ValueError(f"rango vertical invalido: {self.levels} niveles de {self.step_m} m")

    @property
    def z_max_m(self) -> float:
        return self.z_min_m + (self.levels - 1) * self.step_m

    def z_values(self) -> np.ndarray:
        return self.z_min_m + self.step_m * np.arange(self.levels)

    def level_of(self, z_m: float) -> int:
        """Indice del nivel mas cercano a la cota z_m (sin acotar)."""
        return int(round((z_m - self.z_min_m) / self.step_m))

    @classmethod
    def covering(cls, top_m: float, z_min_m: float = Z_MIN_M, step_m: float = STEP_Z_M,
                 headroom_m: float = 3.0) -> "ZRange":
        """El rango mas corto (nunca menos de Z_LEVELS) que deja headroom_m de aire sobre top_m."""
        levels = int(np.ceil((top_m + headroom_m - z_min_m) / step_m - 1e-6)) + 1
        return cls(z_min_m, max(Z_LEVELS, levels), step_m)


DEFAULT_Z_RANGE = ZRange()
LEGACY_Z_RANGE = ZRange(levels=LEGACY_Z_LEVELS)
