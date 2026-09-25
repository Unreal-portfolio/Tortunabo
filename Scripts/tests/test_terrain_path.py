"""Terreno «camino primero» (Docs/Diseno_Terreno_CaminoPrimero.md)."""

from __future__ import annotations

import sys
from pathlib import Path
from types import SimpleNamespace

import numpy as np
import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from gen_terrain_volume import global_standable  # noqa: E402
from terrain_vol.export import global_top  # noqa: E402
from terrain_vol.layout import CELL_SAMPLES, Z_SAMPLES  # noqa: E402


def test_las_utilidades_aceptan_un_mapa_de_4x4():
    chunks = {(c, r): SimpleNamespace(standable=np.ones((CELL_SAMPLES, CELL_SAMPLES, Z_SAMPLES), bool),
                                      top=np.full((CELL_SAMPLES, CELL_SAMPLES), float(c + r)))
              for r in range(4) for c in range(4)}
    size = 4 * (CELL_SAMPLES - 1) + 1
    assert global_standable(chunks, grid=4).shape == (size, size, Z_SAMPLES)
    top = global_top(chunks, grid=4)
    assert top.shape == (size, size) and top[-1, -1] == 6.0


from terrain_path.curves import knot_noise, longest_straight, resample  # noqa: E402
from terrain_path.layout import GRID, MAP_MAX_M, MAP_MIN_M  # noqa: E402
from terrain_path.style import C01_STYLE  # noqa: E402


def test_el_mapa_mide_400_metros():
    assert GRID == 4 and MAP_MAX_M - MAP_MIN_M == 400.0


def test_remuestreo_a_paso_fijo():
    pts, arc = resample(np.array([[0.0, 0.0], [10.0, 0.0], [10.0, 5.0]]), 1.0)
    assert np.isclose(arc[-1], 15.0) and np.allclose(np.diff(arc), 1.0)


def test_detecta_rectas_largas():
    s = np.arange(0.0, 60.0, 1.0)
    recta = np.stack([s, np.zeros_like(s)], axis=1)
    curva = np.stack([30.0 * np.cos(s / 30.0), 30.0 * np.sin(s / 30.0)], axis=1)
    assert longest_straight(recta) >= 50.0
    assert longest_straight(curva) == 0.0


def test_ruido_por_nudos_en_rango():
    rng = np.random.default_rng(1)
    arc = np.arange(0.0, 300.0, 1.0)
    v = knot_noise(rng, arc, (20.0, 60.0), 2.5, 8.0, mode=4.0)
    assert v.min() >= 2.5 - 1e-9 and v.max() <= 8.0 + 1e-9 and np.ptp(v) > 1.0


def test_estilo_c01():
    assert C01_STYLE.loops == 7 and C01_STYLE.nested_loops >= 1 and C01_STYLE.crossings >= 1
