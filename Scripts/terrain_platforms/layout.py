"""Mapa de plataformas P01: rejilla, clases del plano y alturas.

Mismas convenciones que terrain_vol/layout.py: X = Norte (fila), Y = Este (columna), Z = arriba;
el trozo (col, fila) tiene su centro en (fila * CELL_M, col * CELL_M). El volumen son 3 x 3 trozos de
100 m (300 m de lado, de MAP_MIN_M a MAP_MAX_M en X e Y). El plano dibujado (LAYOUT_SIZE_M = 200 m)
ocupa el centro, de LAYOUT_ORIGIN_M a LAYOUT_ORIGIN_M + 200, y lo rodean FRAME_M = 50 m de rio: la
malla no se cierra en el borde del volumen y asi ninguna plataforma queda cortada ahi.

Las cotas del plano (+20, +25, +30 m; rios a 0 m) se miden sobre el agua: WATER_M es la cota
del agua en Unreal (-4 m), asi que la cima de una plataforma +30 queda a WATER_M + 30 m.
"""

from __future__ import annotations

from pathlib import Path

import numpy as np
from PIL import Image

from terrain_vol.layout import (CELL_M, CELL_SAMPLES, STEP_XY_M, STEP_Z_M, UU_PER_M, WATER_M,  # noqa: F401
                                Z_MAX_M, Z_MIN_M)

GRID = 3
MAP_MIN_M = -CELL_M / 2.0
VOLUME_M = GRID * CELL_M
MAP_MAX_M = MAP_MIN_M + VOLUME_M
FRAME_M = 50.0
LAYOUT_ORIGIN_M = MAP_MIN_M + FRAME_M
LAYOUT_SIZE_M = VOLUME_M - 2.0 * FRAME_M         # 200 m: el plano dibujado

LAYOUT_PX = 400                                  # pixeles del mapa de clases (0,5 m cada uno)
PX_M = LAYOUT_SIZE_M / LAYOUT_PX
FRAME_PX = int(FRAME_M / PX_M)

CLASS_WATER, CLASS_PLAT20, CLASS_PLAT25, CLASS_PLAT30, CLASS_BRIDGE = range(5)
# Colores del PNG del layout (los del plano original).
PALETTE = ((0, 170, 204), (230, 230, 230), (174, 179, 179), (120, 120, 120), (255, 181, 71))

# Altura de la cima sobre el agua (m) de cada clase de plataforma.
PLATFORM_ABOVE_WATER_M = {CLASS_PLAT20: 20.0, CLASS_PLAT25: 25.0, CLASS_PLAT30: 30.0}
RIVER_DEPTH_M = 1.5                              # el lecho queda esto por debajo del agua

LAYOUT_FILE = Path(__file__).with_name("P01_layout.png")


def load_layout(path: Path = LAYOUT_FILE) -> np.ndarray:
    """Mapa de clases (LAYOUT_PX x LAYOUT_PX) indexado [Norte, Este]. El PNG se guarda con el
    Norte arriba, asi que la fila 0 de la imagen es la ultima del array."""
    with Image.open(path) as image:
        return np.asarray(image, dtype=np.uint8)[::-1].copy()


def pad_layout(layout: np.ndarray) -> np.ndarray:
    """El layout rodeado de rio (FRAME_M por lado): un pixel por cada 0,5 m de todo el volumen."""
    return np.pad(layout, FRAME_PX, mode="constant", constant_values=CLASS_WATER)


def save_layout(layout: np.ndarray, path: Path = LAYOUT_FILE) -> None:
    image = Image.fromarray(np.ascontiguousarray(layout[::-1]), mode="P")
    image.putpalette([c for color in PALETTE for c in color] + [0] * (768 - 3 * len(PALETTE)))
    path.parent.mkdir(parents=True, exist_ok=True)
    image.save(path, optimize=True)
