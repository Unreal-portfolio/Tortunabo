"""Generador de islas (Scripts/gen_terrain_island.py): archipielagos ficticios con volcan, lagunas y puentes
naturales (terrain_shapes/archipelago.py) e islas reales por GeoRegion (preset I07_taal, rasters guardados).

    uv run --with pytest --with numpy --with scipy --with pillow --with scikit-image --with pyproj \
        python -m pytest Scripts/tests/test_terrain_island.py
"""

from __future__ import annotations

import dataclasses
import math
import sys
from pathlib import Path

import numpy as np
import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from terrain_shapes.archipelago import (Coast, build_archipelago, build_height, philippines_rare,  # noqa: E402
                                        random_archipelago)
from terrain_shapes.bridges import NaturalBridge  # noqa: E402
from terrain_shapes.canvas import Canvas  # noqa: E402
from terrain_shapes.writer import write_shape_map  # noqa: E402
from terrain_vol.layout import WATER_M  # noqa: E402


def height_at(shape, e: float, n: float) -> float:
    X, Y = shape.canvas.to_world(e, n)
    return float(shape.model.ground_height(np.array([X]), np.array([Y]))[0])


def test_natural_bridge_is_an_arch_with_air_below_the_middle():
    bridge = NaturalBridge((0.0, 0.0), (30.0, 0.0), 0.0, 0.0, width_m=4.0, rise_m=0.5)
    Z = np.arange(-6.0, 2.0, 0.25)
    e, n = np.array([[15.0, 0.5, 15.0]]), np.array([[0.0, 0.0, 3.5]])
    d = bridge.density(e, n, Z)
    mid, foot, beside = d[0, 0], d[0, 1], d[0, 2]
    assert (mid[Z < -2.5] < 0).all() and (mid[(Z > -0.8) & (Z < 0.4)] > 0).any()      # aire debajo, tablero arriba
    assert (foot[(Z > -5.0) & (Z < -0.5)] > 0).all()                                   # estribo hasta el fondo
    assert (beside < 0).all()                                                          # fuera del ancho
    assert bridge.slope_deg() == pytest.approx(math.degrees(math.atan(4 * 0.5 / 30.0)))


def test_philippines_spec_follows_the_catalogue():
    spec = philippines_rare()
    sizes = sorted((i.radii, i.height_m) for i in spec.islands if not i.turtle)
    assert [s for s in sizes if s[0] == (18.0, 18.0)] == [((18.0, 18.0), 3.0)] * 3
    assert [s for s in sizes if s[0] == (10.0, 10.0)] == [((10.0, 10.0), 2.0)] * 3
    turtle = next(i for i in spec.islands if i.turtle)
    assert turtle.radii == (30.0, 45.0) and turtle.height_m == 4.0
    assert spec.volcano.slope_deg(4.0) == pytest.approx(19.65, abs=0.05)
    assert spec.grid == 3 and spec.bridge_count == 9 and spec.bridge_width_m == 4.0


@pytest.fixture(scope="module")
def philippines():
    return build_archipelago(philippines_rare())


def test_philippines_has_nine_gentle_bridges_joining_every_island(philippines):
    bridges = philippines.model.bridges
    assert len(bridges) == 9
    assert all(b.slope_deg() <= 20.0 and b.width_m == 4.0 for b in bridges)
    assert set(philippines.required) == {"cima", "tortuga", "mediana_1", "mediana_2", "mediana_3", "pequena_1",
                                          "pequena_2", "pequena_3"}
    assert height_at(philippines, 0.0, 0.0) - WATER_M == pytest.approx(14.0, abs=0.3)          # cima plana a +14
    assert height_at(philippines, 85.0 * math.cos(math.radians(30)), 85.0 * math.sin(math.radians(30))) - WATER_M \
        == pytest.approx(3.0, abs=0.2)


def test_philippines_map_is_valid_on_the_mesh(philippines, tmp_path):
    r = write_shape_map(philippines, register=False, variants=tmp_path)
    assert r["ok"], {k: r[k] for k in ("required_names_missed", "corridors_failed", "unreachable_islands")}
    assert r["budget"]["triangles"] <= 350_000 and r["seams"]["ok"]
    assert (tmp_path / "I01_filipinas_rara" / "CREDITS.txt").exists()


def test_same_seed_same_map_and_another_seed_moves_the_coast():
    spec = philippines_rare()
    canvas = Canvas(spec.grid)
    a = build_height(spec, canvas, Coast(spec))
    assert np.array_equal(a, build_height(spec, canvas, Coast(spec)))
    other = dataclasses.replace(spec, seed=spec.seed + 1)
    assert not np.array_equal(a, build_height(other, canvas, Coast(other)))


def test_random_archipelago_has_volcano_crater_lagoon_and_is_valid(tmp_path):
    spec = random_archipelago(seed=7, islands=5, lagoons=1)
    assert len(spec.islands) == 5 and len(spec.lagoons) == 1 and spec.volcano.crater_radius_m > 0
    shape = build_archipelago(spec)
    lagoon = spec.lagoons[0]
    assert height_at(shape, *lagoon.center) < WATER_M - 0.5                          # la laguna es agua
    ce, cn = spec.islands[0].center
    rim = height_at(shape, ce + 0.5 * (spec.volcano.crater_radius_m + spec.volcano.top_radius_m), cn)
    assert height_at(shape, ce, cn) < rim - 1.0                                     # crater hundido
    r = write_shape_map(shape, register=False, variants=tmp_path)
    assert r["ok"], {k: r[k] for k in ("required_names_missed", "corridors_failed", "unreachable_islands")}


def test_an_island_without_bridge_must_be_marked(tmp_path):
    spec = philippines_rare()
    lonely = dataclasses.replace(spec, unreachable_ok=("pequena_1",), name="I_marcada")
    shape = build_archipelago(lonely)
    near = [b for b in shape.model.bridges if min(math.dist(b.a, (0.0, 85.0)), math.dist(b.b, (0.0, 85.0))) < 16.0]
    assert near == [] and "pequena_1" not in shape.required
    assert write_shape_map(shape, register=False, variants=tmp_path)["ok"]
    shape.unreachable_ok.clear()                                                      # sin marcar: falla
    r = write_shape_map(shape, register=False, variants=tmp_path)
    assert not r["ok"] and len(r["unreachable_islands"]) == 1


def test_real_island_from_georegion_is_valid(tmp_path):
    pytest.importorskip("pyproj")
    from gen_terrain_geo import build_geo
    r = build_geo("I07_taal", register=False, variants=tmp_path)
    assert r["ok"] and r["budget"]["mode"] == "tct" and r["budget"]["triangles"] <= 350_000
    assert 0.5 <= r["exaggeration"] <= 6.0 and r["projection"] == "laea"
    assert "Terrain Tiles" in (tmp_path / "I07_taal" / "CREDITS.txt").read_text(encoding="utf-8")
