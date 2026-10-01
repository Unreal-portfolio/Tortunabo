"""Generadores que el banco sabe ejecutar. Cada uno es `generar(seed, difficulty) -> SurvivalMap`.

  - `referencia`: línea base mínima (camino sinuoso entre muros de ruido) que sirve para validar el banco.

El generador de verdad es el del Coop adaptado (C++): su export escribe los .npz y el banco los lee con
`--carpeta` (ver bench.py).
"""

from __future__ import annotations

import time
from typing import Callable

import numpy as np
from scipy import ndimage

from terrain_vol.layout import WATER_M

from . import spec
from .mapa import SurvivalMap, expected_shape

Generator = Callable[[int, int], SurvivalMap]


def referencia(seed: int, difficulty: int) -> SurvivalMap:
    t0 = time.perf_counter()
    rng = np.random.default_rng(seed)
    h, w = expected_shape()
    ground = 3.0
    # Camino: curva sinuosa de oeste a este; la dificultad aumenta la amplitud y la frecuencia.
    amplitude = 8.0 + 6.0 * difficulty
    turns = 1.0 + 0.3 * difficulty
    phase = rng.uniform(0, 2 * np.pi)
    cols = np.arange(w)
    centre = h / 2.0 + amplitude * np.sin(2 * np.pi * turns * cols / w + phase) * np.sin(np.pi * cols / w) ** 0.5
    rows = np.arange(h)[:, None]
    distance = np.abs(rows - centre[None, :])
    half_width = 11.0 - 1.0 * difficulty
    wall = np.clip((distance - half_width) / 6.0, 0.0, 1.0)
    noise = ndimage.gaussian_filter(rng.standard_normal((h, w)), 6.0)
    noise /= max(float(np.abs(noise).max()), 1e-9)
    top = ground + wall * (6.0 + 3.0 * difficulty) + noise * (1.0 + 0.5 * difficulty) * (0.2 + wall)
    top = np.maximum(top, WATER_M + 1.0)
    # Cráteres de agua fuera del camino: no tocan el camino ni el inicio y la meta.
    for _ in range(3 + difficulty * 2):
        ci, cj = int(rng.integers(0, h)), int(rng.integers(30, w - 30))
        ii, jj = np.ogrid[:h, :w]
        pit = (ii - ci) ** 2 + (jj - cj) ** 2 <= 6.0 ** 2
        if distance[ci, cj] > half_width + 12:
            top[pit] = WATER_M - 1.0
    start = (int(round(centre[spec.START_MARGIN_M])), spec.START_MARGIN_M)
    goal = (int(round(centre[w - 1 - spec.START_MARGIN_M])), w - 1 - spec.START_MARGIN_M)
    return SurvivalMap(top=top, start=start, goal=goal, seed=seed, difficulty=difficulty, algorithm="referencia",
                       gen_seconds=time.perf_counter() - t0)


ADAPTERS: dict[str, Generator] = {"referencia": referencia}
