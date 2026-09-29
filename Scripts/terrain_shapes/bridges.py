"""Puentes naturales tallados: un arco de roca arenosa entre dos puntos, con el tablero plano de lado a lado,
un leve lomo en el centro y estribos que bajan al fondo cerca de los extremos (debajo del centro queda aire:
es volumen, no campo de alturas). Coordenadas de diseño (e, n) y cotas absolutas en m.
"""

from __future__ import annotations

from dataclasses import dataclass

import numpy as np

from .canvas import SEABED_M, smoothstep


@dataclass(frozen=True)
class NaturalBridge:
    a: tuple[float, float]
    b: tuple[float, float]
    z_a: float                        # cota del tablero en a (la de la meseta)
    z_b: float
    width_m: float = 4.0
    thickness_m: float = 1.4
    rise_m: float = 0.5               # lomo del tablero en el centro
    pier_m: float = 4.0               # largo de los estribos que bajan al fondo
    overlap_m: float = 2.5            # el tablero se mete esto en cada meseta (sin rendija con la orilla)

    @property
    def length(self) -> float:
        return float(np.hypot(self.b[0] - self.a[0], self.b[1] - self.a[1]))

    def slope_deg(self) -> float:
        """Pendiente maxima del tablero (la del tramo recto mas la del lomo en los extremos)."""
        grade = abs(self.z_b - self.z_a) / max(self.length, 1e-6) + 4.0 * self.rise_m / max(self.length, 1e-6)
        return float(np.degrees(np.arctan(grade)))

    def _frame(self, e, n):
        """(s en [0, 1] a lo largo, distancia lateral, distancia al extremo mas cercano en m)."""
        ae, an = self.a
        de, dn = self.b[0] - ae, self.b[1] - an
        length = max(self.length, 1e-6)
        ue, un = de / length, dn / length
        along = (e - ae) * ue + (n - an) * un
        lateral = np.abs(-(e - ae) * un + (n - an) * ue)
        s = np.clip(along / length, 0.0, 1.0)
        return s, along, lateral, length

    def deck_top(self, s):
        return self.z_a + (self.z_b - self.z_a) * s + self.rise_m * 4.0 * s * (1.0 - s)

    def density(self, e: np.ndarray, n: np.ndarray, Z: np.ndarray) -> np.ndarray:
        """Densidad (m, positiva dentro) del puente en la rejilla (e, n) 2D por los niveles Z 1D."""
        s, along, lateral, length = self._frame(e, n)
        inside_len = np.minimum(along + self.overlap_m, length + self.overlap_m - along)
        side = np.minimum(0.5 * self.width_m - lateral, inside_len)
        top = self.deck_top(s)
        end_dist = np.minimum(along, length - along)
        pier = 1.0 - smoothstep(0.0, self.pier_m, end_dist)
        bottom = top - self.thickness_m - (top - SEABED_M + 1.0) * pier ** 1.5
        Z3 = Z[None, None, :]
        return np.minimum(np.minimum(side[..., None], top[..., None] - Z3), Z3 - bottom[..., None])

    def bbox(self, margin: float = 1.0) -> tuple[float, float, float, float]:
        r = 0.5 * self.width_m + self.overlap_m + margin
        return (min(self.a[0], self.b[0]) - r, max(self.a[0], self.b[0]) + r, min(self.a[1], self.b[1]) - r,
                max(self.a[1], self.b[1]) + r)

    def inner_points(self, inset_m: float = 1.5) -> tuple[tuple[float, float], tuple[float, float]]:
        """Dos puntos en las mesetas, inset_m mas alla de los extremos (para validar la pasarela)."""
        de, dn = self.b[0] - self.a[0], self.b[1] - self.a[1]
        length = max(self.length, 1e-6)
        ue, un = de / length, dn / length
        return (self.a[0] - ue * inset_m, self.a[1] - un * inset_m), (self.b[0] + ue * inset_m, self.b[1] + un * inset_m)
