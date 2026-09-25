"""Prototipo P01 (2026-09-25): camino principal trenzado, puentes naturales sobre un canon
transversal, rios como rapidos que llegan al mar, salida y meta reconocibles."""

from __future__ import annotations

import sys
from pathlib import Path

import numpy as np
import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from gen_terrain_volume import build_all, global_standable, ground_level, walk, world_index  # noqa: E402
from terrain_vol.density import MapModel  # noqa: E402
from terrain_vol.layout import MAP_MAX_M, WATER_M  # noqa: E402
from terrain_vol.style import MAPA01_STYLE  # noqa: E402
from terrain_vol.variants import PROTOTYPES  # noqa: E402


@pytest.fixture(scope="module")
def proto():
    spec = PROTOTYPES[0]
    model = MapModel(spec.seed, style=spec.style, erode=False)
    standable = global_standable(build_all(model))
    return model, standable


def test_el_estilo_por_defecto_no_activa_nada_del_prototipo():
    style = MAPA01_STYLE
    assert not style.trails and not style.river_rapids and not style.landmarks
    assert style.land_bridges == (0, 0) and style.human_marks == 0 and style.ground_flatten == 0.0


def test_el_camino_principal_se_abre_en_ramales_que_vuelven_al_tronco(proto):
    model, _ = proto
    trunk, branches = model.trails[0], model.trails[1:]
    assert len(branches) >= 2
    tree_d = lambda p: np.min(np.linalg.norm(trunk - p, axis=1))     # noqa: E731
    for line in branches:
        assert tree_d(line[0]) < 3.0 and tree_d(line[-1]) < 3.0, "un ramal no sale ni vuelve al tronco"
        assert max(tree_d(p) for p in line[::10]) > 10.0, "un ramal no se separa del tronco"


def test_los_puentes_naturales_se_cruzan_por_arriba_y_por_abajo(proto):
    model, standable = proto
    assert len(model.bridges) >= 2
    start_ij = world_index(model.route.points[0])
    reached = walk(standable, (*start_ij, ground_level(standable, *start_ij)))
    for b in model.bridges:
        i, j = world_index(b["c"])
        levels = np.nonzero(standable[i, j])[0]
        assert len(levels) >= 2, f"puente sin paso por debajo en {b['c']}"
        assert reached[i - 1:i + 2, j - 1:j + 2, levels.max()].any(), f"no se llega al tablero en {b['c']}"


def test_los_rios_llegan_al_mar(proto):
    model, _ = proto
    h = model.grid.height
    for pts, _ in model.rivers:
        near_sea = pts[(pts[:, 0] > MAP_MAX_M - 60.0) & (pts[:, 0] < MAP_MAX_M - 5.0)]
        assert len(near_sea), "el rio no llega al borde norte"
        i = np.clip(np.rint(near_sea[:, 0] - model.axis[0]).astype(int), 0, len(model.axis) - 1)
        j = np.clip(np.rint(near_sea[:, 1] - model.axis[0]).astype(int), 0, len(model.axis) - 1)
        assert np.mean(h[i, j] < WATER_M) > 0.8, "la playa tapa el cauce antes del mar"


def test_el_prototipo_se_recorre_de_inicio_a_fin(proto):
    model, standable = proto
    start_ij, end_ij = world_index(model.route.points[0]), world_index(model.route.points[-1])
    start = (*start_ij, ground_level(standable, *start_ij))
    end = (*end_ij, ground_level(standable, *end_ij))
    assert start[2] >= 0 and end[2] >= 0
    assert walk(standable, start)[end]
