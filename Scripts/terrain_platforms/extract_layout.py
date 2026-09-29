"""Convierte un plano dibujado a mano (plataformas de arena, rios y puentes colgantes sobre una
rejilla de 2 m) en un mapa de clases 400 x 400 (0,5 m por pixel) que lee terrain_platforms.

    uv run --with numpy --with scipy --with pillow python Scripts/terrain_platforms/extract_layout.py \
        <plano.jpeg> [Scripts/terrain_platforms/P01_layout.png]  (Norte arriba en el PNG)

Convenciones del plano (P01): Norte arriba, Este a la derecha; una celda de rejilla = 2 m
(15 px); los trazos negros son el contorno de cada plataforma y la leyenda (arriba a la derecha)
cubre rio. Las clases se reconocen por color; la rejilla, el contorno y el texto se quitan con un
filtro de mediana y un relleno por vecino mas cercano.
"""

from __future__ import annotations

import sys
from pathlib import Path

import numpy as np
from PIL import Image
from scipy import ndimage

from .layout import (CLASS_BRIDGE, CLASS_PLAT20, CLASS_PLAT25, CLASS_PLAT30, CLASS_WATER, LAYOUT_FILE, LAYOUT_PX,
                     LAYOUT_SIZE_M, save_layout)

PX_PER_M = 7.5                    # rejilla del plano: 15 px = 2 m
MEDIAN_PX = 9                     # mayor que el doble de un trazo negro (3-4 px): la mediana lo borra
SPECK_PX = 120                    # componentes de menos de 30 m2 (a 0,5 m por pixel) no cuentan
BORDER_PX = 6                     # marco del lienzo del plano: se descarta
LEGEND_BOX = (1130, 0, 1489, 225)  # (x0, y0, x1, y1) en px del plano: leyenda, se rellena con rio


def classify(rgb: np.ndarray) -> np.ndarray:
    """Clase por pixel solo donde el color es inequivoco; -1 = dudoso (borde de un trazo, mezcla)."""
    r, g, b = (rgb[..., i].astype(np.int32) for i in range(3))
    luma = (r + g + b) // 3
    out = np.full(luma.shape, -1, dtype=np.int8)
    out[luma >= 215] = CLASS_PLAT20
    out[(luma >= 160) & (luma <= 200)] = CLASS_PLAT25
    out[(luma >= 105) & (luma <= 140)] = CLASS_PLAT30
    out[(b - r) > 100] = CLASS_WATER
    out[(r - b) > 100] = CLASS_BRIDGE
    return out


def fill_outline(labels: np.ndarray) -> np.ndarray:
    """Da a cada pixel sin clase la del pixel con clase mas cercano."""
    missing = labels < 0
    if not missing.any():
        return labels
    _, (ii, jj) = ndimage.distance_transform_edt(missing, return_indices=True)
    return labels[ii, jj]


def drop_specks(labels: np.ndarray, min_px: int = SPECK_PX) -> np.ndarray:
    """Quita las motas (borde de un trazo, JPEG): cada componente conexa pequena toma la clase vecina."""
    out = labels.copy()
    for cls in range(5):
        comp, count = ndimage.label(out == cls)
        sizes = ndimage.sum(np.ones_like(comp), comp, index=np.arange(1, count + 1))
        for k in np.nonzero(sizes < min_px)[0] + 1:
            out[comp == k] = -1
    return fill_outline(out)


def extract(image_path: Path) -> np.ndarray:
    rgb = np.asarray(Image.open(image_path).convert("RGB"))
    x0, y0, x1, y1 = LEGEND_BOX
    filtered = np.stack([ndimage.median_filter(rgb[..., c], size=MEDIAN_PX) for c in range(3)], axis=-1)
    labels = fill_outline(classify(filtered))
    labels = np.pad(labels[BORDER_PX:-BORDER_PX, BORDER_PX:-BORDER_PX], BORDER_PX, mode="edge")
    labels[y0:y1, x0:x1] = CLASS_WATER
    # Un pixel de un rio o de un puente aislado (JPEG, texto) no es una plataforma.
    labels = drop_specks(ndimage.median_filter(labels, size=5))
    # Remuestreo al lienzo del mapa: 200 m = 1500 px del plano; el plano se centra en el lienzo
    # (le faltan unos pocos pixeles al borde, que se rellenan con rio).
    height, width = labels.shape
    n = LAYOUT_PX
    m = (np.arange(n) + 0.5) * (LAYOUT_SIZE_M / n)                       # metros del centro de cada pixel
    # out[i, j]: i = Norte (X del mundo), j = Este (Y del mundo); el Norte esta arriba en el plano.
    src_col = np.round(m * PX_PER_M - (LAYOUT_SIZE_M * PX_PER_M - width) / 2.0 - 0.5).astype(int)
    src_row = np.round((LAYOUT_SIZE_M - m) * PX_PER_M - (LAYOUT_SIZE_M * PX_PER_M - height) / 2.0 - 0.5).astype(int)
    inside = ((src_row >= 0) & (src_row < height))[:, None] & ((src_col >= 0) & (src_col < width))[None, :]
    out = labels[np.clip(src_row, 0, height - 1)[:, None], np.clip(src_col, 0, width - 1)[None, :]].astype(np.uint8)
    out[~inside] = CLASS_WATER
    return drop_specks(out.astype(np.int8)).astype(np.uint8)


def main() -> None:
    if len(sys.argv) < 2:
        raise SystemExit(__doc__)
    source = Path(sys.argv[1])
    target = Path(sys.argv[2]) if len(sys.argv) > 2 else LAYOUT_FILE
    layout = extract(source)
    save_layout(layout, target)
    counts = {name: int((layout == cls).sum()) for name, cls in
              (("rio", CLASS_WATER), ("+20", CLASS_PLAT20), ("+25", CLASS_PLAT25), ("+30", CLASS_PLAT30),
               ("puente", CLASS_BRIDGE))}
    print(f"{target}: {layout.shape[1]}x{layout.shape[0]} px, {counts}")


if __name__ == "__main__":
    main()
