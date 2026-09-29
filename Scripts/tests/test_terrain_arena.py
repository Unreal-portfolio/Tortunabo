"""Arenas parametricas de Todos contra Todos (Scripts/gen_terrain_arena.py, terrain_shapes/arena.py): diana,
donut, espiral y tablero de mesetas.

    uv run --with pytest --with numpy --with scipy --with pillow --with scikit-image \
        python -m pytest Scripts/tests/test_terrain_arena.py
"""

from __future__ import annotations

import dataclasses
import math
import sys
from pathlib import Path

import numpy as np
import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from terrain_shapes.arena import (BoardSpec, DianaSpec, DonutSpec, SpiralSpec, board_heights, build_board,  # noqa: E402
                                  build_diana, build_donut, build_spiral, diana_a01)
from terrain_shapes.writer import write_shape_map  # noqa: E402
from terrain_vol.layout import WATER_M  # noqa: E402
from terrain_vol.validate import DRY_M  # noqa: E402


def above_water(shape, e: float, n: float) -> float:
    X, Y = shape.canvas.to_world(e, n)
    return float(shape.model.ground_height(np.array([X]), np.array([Y]))[0]) - WATER_M


def assert_valid(result: dict) -> None:
    assert result["ok"], {k: result[k] for k in ("required_names_missed", "corridors_failed", "unreachable_islands",
                                                  "budget", "seams")}


@pytest.fixture(scope="module")
def diana():
    return build_diana(diana_a01())


def test_diana_rings_and_ramps_follow_the_catalogue(diana):
    for r, h in ((0.0, 12.0), (12.0, 9.0), (25.0, 6.0), (39.0, 4.0), (52.0, 2.0)):
        assert above_water(diana, r * math.cos(math.radians(45)), r * math.sin(math.radians(45))) == pytest.approx(h, abs=0.05)
    assert above_water(diana, 64.0, 0.0) < -1.0                                         # fuera: agua
    # Rampa del escalon +6 -> +4 a 0 grados: empieza en r = 32 a +6 y baja 15 grados.
    assert above_water(diana, 33.0, 0.0) == pytest.approx(6.0 - math.tan(math.radians(15)), abs=0.05)
    assert above_water(diana, 33.0, 5.0) == pytest.approx(4.0, abs=0.05)               # fuera de los 6 m de ancho
    assert len(diana.corridors) == 16 and diana.canvas.grid == 2


def test_diana_map_is_valid_on_the_mesh(diana, tmp_path):
    r = write_shape_map(diana, register=False, variants=tmp_path)
    assert_valid(r)
    assert r["budget"]["triangles"] <= 350_000
    top = r["top"]
    row = top[diana.canvas.to_ij(0.0, 0.0)[0]]
    dry = np.nonzero(row > DRY_M)[0]
    assert dry.max() - dry.min() == pytest.approx(120.0, abs=3.0)                       # diametro 120 m +- 3


def test_a_ramp_that_does_not_fit_its_ring_is_refused():
    with pytest.raises(ValueError):
        build_diana(dataclasses.replace(diana_a01(), ramp_slope_deg=5.0))


def test_diana_with_moats_uses_natural_bridges(tmp_path):
    spec = DianaSpec("A_foso", 11, 2, ((0.0, 10.0, 6.0), (10.0, 30.0, 4.5), (30.0, 50.0, 3.0)), moat_m=4.0,
                     stagger_deg=30.0)
    shape = build_diana(spec)
    assert len(shape.model.bridges) == 8 and above_water(shape, 0.0, 30.0) < 0.0 and above_water(shape, 0.0, 10.0) < 0.0
    assert_valid(write_shape_map(shape, register=False, variants=tmp_path))
    steep = build_diana(dataclasses.replace(spec, rings=((0.0, 10.0, 12.0), (10.0, 30.0, 6.0), (30.0, 50.0, 2.0)),
                                            name="A_foso_empinado"))
    r = write_shape_map(steep, register=False, variants=tmp_path)
    assert not r["ok"] and r["corridors_failed"]                                               # puentes de mas de 20 grados


def test_donut_has_water_in_the_hole_and_bridges_across(tmp_path):
    shape = build_donut(DonutSpec("A02_donut", 3002, 2))
    assert above_water(shape, 10.0, 10.0) < 0.0 and above_water(shape, 45.0, 0.0) == pytest.approx(4.0, abs=0.05)
    assert len(shape.model.bridges) == 2
    assert_valid(write_shape_map(shape, register=False, variants=tmp_path))


def test_spiral_climbs_to_the_centre(tmp_path):
    spec = SpiralSpec("A03_espiral", 3003, 2)
    shape = build_spiral(spec)
    tail = above_water(shape, *shape.required["cola"])
    assert above_water(shape, 0.0, 0.0) == pytest.approx(spec.h_top, abs=0.05) and tail < 2.0
    assert above_water(shape, 18.0, 0.0) < 0.0                                              # foso entre vueltas
    assert_valid(write_shape_map(shape, register=False, variants=tmp_path))


def test_board_heights_respect_counts_and_neighbour_steps():
    spec = BoardSpec("A05_tablero", 3005, 2)
    h = board_heights(spec)
    assert h.shape == (5, 5)
    assert [int((h == level).sum()) for level in spec.levels] == list(spec.counts)
    assert np.abs(np.diff(h, axis=0)).max() <= 3.0 and np.abs(np.diff(h, axis=1)).max() <= 3.0
    assert not np.array_equal(h, board_heights(dataclasses.replace(spec, seed=4)))
    with pytest.raises(ValueError):
        board_heights(dataclasses.replace(spec, counts=(20, 0, 5)))                         # +3 y +9 vecinas: imposible


def test_board_is_valid(tmp_path):
    shape = build_board(BoardSpec("A05_tablero", 3005, 2))
    assert all(b.slope_deg() <= 18.5 for b in shape.model.bridges) and len(shape.model.bridges) == 30
    assert_valid(write_shape_map(shape, register=False, variants=tmp_path))
