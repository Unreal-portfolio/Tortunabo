"""Ruido de valor 2D y 3D en coordenadas de MUNDO (metros), con la reticula sorteada una
sola vez. Se puede evaluar en cualquier trozo del mapa por separado y dos trozos vecinos ven
exactamente los mismos valores en su borde comun."""

from __future__ import annotations

import math

import numpy as np

from .layout import MAP_MAX_M, MAP_MIN_M, Z_MAX_M, Z_MIN_M


def _smooth(t):
    return t * t * (3.0 - 2.0 * t)


class ValueNoise2D:
    """Ruido de valor en [-1, 1] sobre el mapa entero, longitud de onda wavelength (m)."""

    def __init__(self, rng: np.random.Generator, wavelength: float, margin: float = 200.0):
        self.wavelength = wavelength
        self.origin = MAP_MIN_M - margin
        cells = int(math.ceil((MAP_MAX_M - MAP_MIN_M + 2.0 * margin) / wavelength)) + 3
        self.lattice = rng.uniform(-1.0, 1.0, (cells, cells))

    def __call__(self, x, y):
        u = (np.asarray(x, dtype=np.float64) - self.origin) / self.wavelength
        v = (np.asarray(y, dtype=np.float64) - self.origin) / self.wavelength
        last = self.lattice.shape[0] - 2
        i0 = np.clip(np.floor(u).astype(np.int64), 0, last)
        j0 = np.clip(np.floor(v).astype(np.int64), 0, last)
        fu = _smooth(np.clip(u - i0, 0.0, 1.0))
        fv = _smooth(np.clip(v - j0, 0.0, 1.0))
        a = self.lattice[i0, j0]
        b = self.lattice[i0 + 1, j0]
        c = self.lattice[i0, j0 + 1]
        d = self.lattice[i0 + 1, j0 + 1]
        return (a * (1 - fu) + b * fu) * (1 - fv) + (c * (1 - fu) + d * fu) * fv


class ValueNoise3D:
    """Ruido de valor 3D en [-1, 1] (paredes con voladizos, rugosidad de roca)."""

    def __init__(self, rng: np.random.Generator, wavelength: float, margin: float = 50.0):
        self.wavelength = wavelength
        self.origin = (MAP_MIN_M - margin, MAP_MIN_M - margin, Z_MIN_M - margin)
        cells_xy = int(math.ceil((MAP_MAX_M - MAP_MIN_M + 2.0 * margin) / wavelength)) + 3
        cells_z = int(math.ceil((Z_MAX_M - Z_MIN_M + 2.0 * margin) / wavelength)) + 3
        self.lattice = rng.uniform(-1.0, 1.0, (cells_xy, cells_xy, cells_z))

    def __call__(self, x, y, z):
        coords = [(np.asarray(c, dtype=np.float64) - o) / self.wavelength for c, o in zip((x, y, z), self.origin)]
        idx, frac = [], []
        for axis, c in enumerate(coords):
            i0 = np.clip(np.floor(c).astype(np.int64), 0, self.lattice.shape[axis] - 2)
            idx.append(i0)
            frac.append(_smooth(np.clip(c - i0, 0.0, 1.0)))
        (i, j, k), (fx, fy, fz) = idx, frac
        L = self.lattice

        def lerp(a, b, t):
            return a + (b - a) * t

        x00 = lerp(L[i, j, k], L[i + 1, j, k], fx)
        x10 = lerp(L[i, j + 1, k], L[i + 1, j + 1, k], fx)
        x01 = lerp(L[i, j, k + 1], L[i + 1, j, k + 1], fx)
        x11 = lerp(L[i, j + 1, k + 1], L[i + 1, j + 1, k + 1], fx)
        return lerp(lerp(x00, x10, fy), lerp(x01, x11, fy), fz)


class Fbm2D:
    """Suma de octavas de ValueNoise2D, normalizada a [-1, 1]."""

    def __init__(self, rng: np.random.Generator, wavelength: float, octaves: int = 3, gain: float = 0.5):
        self.layers = [(gain ** o, ValueNoise2D(rng, wavelength / (2 ** o))) for o in range(octaves)]
        self.norm = sum(a for a, _ in self.layers)

    def __call__(self, x, y):
        return sum(a * n(x, y) for a, n in self.layers) / self.norm

    def unit(self, x, y):
        """El mismo ruido llevado a [0, 1]."""
        return self(x, y) * 0.5 + 0.5
