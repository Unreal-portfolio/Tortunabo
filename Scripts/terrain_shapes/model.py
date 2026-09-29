"""Modelo de densidad de los mapas parametricos: un campo de alturas (raster del volumen) mas puentes naturales
en 3D. Misma interfaz que el resto de modelos (chunk_fields, density) para build_chunk y el exportador TNTM2.
"""

from __future__ import annotations

from dataclasses import dataclass, field

import numpy as np

from terrain_geo.heightfield import HeightfieldModel
from terrain_vol.layout import DEFAULT_Z_RANGE, ZRange

from .bridges import NaturalBridge
from .canvas import Canvas


class ShapeModel(HeightfieldModel):
    def __init__(self, canvas: Canvas, height: np.ndarray, bridges: tuple[NaturalBridge, ...] = (),
                 z_range: ZRange = DEFAULT_Z_RANGE):
        self.canvas, self.bridges, self.z_range = canvas, tuple(bridges), z_range
        super().__init__(height)

    def density(self, X: np.ndarray, Y: np.ndarray, Z: np.ndarray, fields=None) -> np.ndarray:
        out = self.ground_height(X, Y)[..., None] - Z[None, None, :]
        if not self.bridges:
            return out
        e, n = self.canvas.to_design(X, Y)
        e_lo, e_hi, n_lo, n_hi = float(e.min()), float(e.max()), float(n.min()), float(n.max())
        for bridge in self.bridges:
            b_e0, b_e1, b_n0, b_n1 = bridge.bbox()
            if b_e1 < e_lo or b_e0 > e_hi or b_n1 < n_lo or b_n0 > n_hi:
                continue
            out = np.maximum(out, bridge.density(e, n, Z))
        return out


@dataclass
class ShapeMap:
    """Resultado de un generador parametrico: el modelo y lo que hace falta para validarlo y escribirlo."""
    name: str
    seed: int
    mode: str
    description: str
    canvas: Canvas
    model: ShapeModel
    start: tuple[float, float]                                 # (e, n)
    end: tuple[float, float] | None = None
    required: dict[str, tuple[float, float]] = field(default_factory=dict)    # puntos que se alcanzan a pie
    corridors: list[tuple[tuple[float, float], tuple[float, float]]] = field(default_factory=list)
    unreachable_ok: list[tuple[float, float]] = field(default_factory=list)
    corridor_width_m: float = 3.0
    corridor_slope_deg: float = 20.0
    params: dict = field(default_factory=dict)                # parametros del generador (van al manifest)
