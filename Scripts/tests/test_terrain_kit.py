"""Kit de los mapas inventados (terrain_shapes/kit.py, kit_writer.py) y arenas A04 Reloj y A06 Panal
(terrain_shapes/arena_extra.py).

    uv run --with pytest --with numpy --with scipy --with pillow --with scikit-image \
        python -m pytest Scripts/tests/test_terrain_kit.py
"""

from __future__ import annotations

import json
import math
import sys
from pathlib import Path

import numpy as np
import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from terrain_shapes.arena_extra import build_clock, build_honeycomb, clock_a04, hex_centres, honeycomb_a06  # noqa: E402
from terrain_shapes.canvas import Canvas  # noqa: E402
from terrain_shapes.kit import (Axis, Deck, Floating, KitModel, Tunnel, max_grade_deg, min_radius, profile,  # noqa: E402
                                resample)
from terrain_shapes.kit_writer import Extras, pick_nests, write_kit_map  # noqa: E402
from terrain_vol.layout import WATER_M  # noqa: E402


def above_water(shape, e: float, n: float) -> float:
    X, Y = shape.canvas.to_world(e, n)
    return float(shape.model.ground_height(np.array([X]), np.array([Y]))[0]) - WATER_M


def density_column(model, canvas, e: float, n: float, Z: np.ndarray) -> np.ndarray:
    X, Y = canvas.to_world(e, n)
    return model.density(np.array([[X]]), np.array([[Y]]), Z)[0, 0]


def test_resample_and_radius_of_a_circle():
    ctrl = [(40.0 * math.cos(a), 40.0 * math.sin(a)) for a in np.linspace(0, 2 * math.pi, 12, endpoint=False)]
    pts = resample(ctrl, 1.0, closed=True)
    assert len(pts) == pytest.approx(2 * math.pi * 40.0, rel=0.02)
    r, _ = min_radius(pts, closed=True)
    assert r == pytest.approx(40.0, rel=0.03)


def test_profile_keys_and_grade():
    arc = np.arange(0.0, 200.0, 1.0)
    z = profile(arc, [(0.0, 0.0), (0.5, 10.0), (1.0, 0.0)], total=200.0, closed=True)
    assert z[0] == pytest.approx(0.0) and z[100] == pytest.approx(10.0)
    grade = max_grade_deg(arc, z, closed=True)
    assert 5.0 < grade < 12.0                           # smoothstep: pico de 1,5 veces la media (5,7 grados)


def test_tunnel_carves_air_with_solid_floor_and_deck_leaves_air_below():
    canvas = Canvas(2)
    e, n = canvas.design_grid()
    height = np.full(e.shape, WATER_M + 20.0)                       # bloque macizo hasta +20
    axis = Axis.of(np.column_stack([np.linspace(-30, 30, 61), np.zeros(61)]), np.full(61, WATER_M + 2.0))
    deck_axis = Axis.of(np.column_stack([np.zeros(41), np.linspace(40, 80, 41)]), np.full(41, WATER_M + 25.0))
    model = KitModel(canvas, height, solids=(Deck(deck_axis, 10.0),), voids=(Tunnel(axis, 12.0, 6.0),))
    Z = np.arange(-8.0, 30.0, 0.25)
    col = density_column(model, canvas, 0.0, 0.0, Z)
    assert (col[(Z > WATER_M + 2.3) & (Z < WATER_M + 7.5)] < 0).all()              # hueco
    assert (col[Z < WATER_M + 1.5] > 0).all() and (col[(Z > WATER_M + 9.0) & (Z < WATER_M + 19.0)] > 0).all()
    side = density_column(model, canvas, 0.0, 7.5, Z)                                 # fuera del ancho: roca
    assert (side[Z < WATER_M + 19.0] > 0).all()
    mid = density_column(model, canvas, 0.0, 60.0, Z)                                  # centro del tablero
    assert (mid[(Z > WATER_M + 23.8) & (Z < WATER_M + 24.8)] > 0).all() and (mid[(Z > WATER_M + 20.5) & (Z < WATER_M + 22.5)] < 0).all()


def test_floating_island_has_air_under_its_belly():
    island = Floating((0.0, 0.0), (15.0, 10.0), 10.0, depth_m=6.0)
    Z = np.arange(-5.0, 15.0, 0.25)
    d = island.density(np.array([[0.0, 14.0]]), np.array([[0.0, 0.0]]), Z)[0]
    assert (d[0][(Z > 3.5) & (Z < 9.5)] > 0).all() and (d[0][Z < 2.5] < 0).all()
    assert (d[1][Z < 8.0] < 0).all()                                                    # el borde es fino


def test_pick_nests_keeps_separation_and_distance_to_water():
    top = np.full((201, 201), WATER_M - 2.0)
    top[40:161, 40:161] = WATER_M + 3.0
    nests = pick_nests(top, (100, 100), 8)
    assert len(nests) == 8
    for a in nests:
        assert min(a[0] - 40, 160 - a[0], a[1] - 40, 160 - a[1]) >= 4
        assert all(math.dist(a, b) >= 15.0 for b in nests if b != a)


def test_clock_has_twelve_pillars_and_a_flat_disc():
    shape, markers = build_clock(clock_a04())
    for hour, deg in ((3, 0.0), (6, -90.0), (9, 180.0)):
        e, n = 50.0 * math.cos(math.radians(deg)), 50.0 * math.sin(math.radians(deg))
        assert above_water(shape, e, n) == pytest.approx(7.0, abs=0.1), hour
    assert above_water(shape, 0.0, 50.0) == pytest.approx(8.0, abs=0.1)                # las 12, mayor
    assert above_water(shape, 30.0, 10.0) == pytest.approx(4.0, abs=0.05)
    assert above_water(shape, 70.0, 0.0) < -1.0
    assert len(markers["trampolin"]) == 3 and markers["eje_agujas"] == [(0.0, 0.0)]


def test_honeycomb_has_19_hexes_water_gaps_and_a_connected_tree():
    spec = honeycomb_a06()
    shape, markers = build_honeycomb(spec)
    assert len(hex_centres(2, 1.0)) == 19 and len(shape.required) == 19
    assert above_water(shape, 0.0, 0.0) == pytest.approx(6.0, abs=0.05)
    links = shape.params["links"]
    assert len(links) == 18 + spec.extra_links
    assert markers["salto"], "debe quedar algun hueco sin istmo"
    gap = markers["salto"][0]
    assert above_water(shape, *gap) < 0.0                                               # hueco de agua


@pytest.mark.parametrize("builder", [lambda: build_clock(clock_a04()), lambda: build_honeycomb(honeycomb_a06())])
def test_catalogue_arenas_are_valid_on_the_mesh(builder, tmp_path):
    shape, markers = builder()
    r = write_kit_map(shape, Extras(nests=8, markers=markers), sheet=False, variants=tmp_path)
    assert r["ok"], (r["required_names_missed"], r["corridors_failed"], r["unreachable_islands"], r["checks"])
    assert r["budget"]["triangles"] <= 350_000
    data = json.loads((tmp_path / shape.name / "manifest.json").read_text(encoding="utf-8"))
    assert len(data["nests_uu"]) == 8 and data["recorrible"] is True and "trampolin" in data["markers_uu"]
