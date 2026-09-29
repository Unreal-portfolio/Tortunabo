"""Mapas del catalogo completados en el lote C2 (terrain_shapes/lots_islands.py, lots_rally.py) y el generador de
circuitos de Rally (terrain_shapes/rally_circuit.py).

    uv run --with pytest --with numpy --with scipy --with pillow --with scikit-image \
        python -m pytest Scripts/tests/test_terrain_inventados.py
"""

from __future__ import annotations

import math
import sys
from pathlib import Path

import numpy as np
import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from terrain_shapes.kit_writer import write_kit_map  # noqa: E402
from terrain_shapes.lots_islands import build_caldera, build_galapagos, build_turtle  # noqa: E402
from terrain_shapes.lots_rally import build_faroe, build_turtle_rally, build_volcano, faroe_spec, volcano_spec  # noqa: E402
from terrain_shapes.rally_circuit import RallySpec, build_rally, find_crossings  # noqa: E402
from terrain_vol.layout import WATER_M  # noqa: E402


def above_water(shape, e: float, n: float) -> float:
    X, Y = shape.canvas.to_world(e, n)
    return float(shape.model.ground_height(np.array([X]), np.array([Y]))[0]) - WATER_M


def lemniscate(a: float = 210.0, count: int = 24) -> tuple[tuple[float, float], ...]:
    out = []
    for k in range(count):
        t = 2.0 * math.pi * k / count
        d = 1.0 + math.sin(t) ** 2
        out.append((a * math.cos(t) / d, a * math.sin(t) * math.cos(t) / d))
    return tuple(out)


@pytest.fixture(scope="module")
def figure_eight():
    spec = RallySpec("R_ocho", 5, lemniscate(), ((0.0, 6.0), (0.25, 15.0), (0.5, 6.0), (0.75, 2.0)), grid=5,
                     water_spans=((0.4, 0.47),), tunnels=((0.05, 0.12, 16.0),))
    return spec, build_rally(spec)


def test_figure_eight_has_an_overpass_a_viaduct_and_a_tunnel(figure_eight):
    spec, (shape, extras) = figure_eight
    names = sorted(s.name for s in shape.model.solids)
    assert names == ["paso_elevado", "viaducto"] and [v.name for v in shape.model.voids] == ["tunel"]
    assert {c.name for c in extras.clearances} == {"tunel", "paso_inferior"}
    assert shape.params["crossings"] == 1 and extras.road.covered.any()


def test_a_crossing_without_headroom_is_refused():
    spec = RallySpec("R_bajo", 5, lemniscate(), ((0.0, 6.0), (0.25, 8.0), (0.5, 6.0), (0.75, 4.0)), grid=5)
    with pytest.raises(ValueError):
        build_rally(spec)


def test_figure_eight_is_valid_on_the_mesh(figure_eight, tmp_path):
    _, (shape, extras) = figure_eight
    r = write_kit_map(shape, extras, sheet=False, variants=tmp_path)
    assert r["ok"], r["checks"]
    road = r["checks"]["road"]
    assert road["min_radius_m"] >= 25.0 and road["max_grade_deg"] <= 12.0 and road["narrow_samples"] == 0
    tunnel = next(c for c in r["checks"]["clearances"] if c["name"] == "tunel")
    assert tunnel["min_cover_m"] >= 8.0                                                 # roca encima del tunel
    assert r["checks"]["taludes"]["p99_deg"] <= 35.0 and r["budget"]["triangles"] <= 1_200_000


def test_find_crossings_sees_the_middle_of_the_eight():
    from terrain_shapes.kit import arc_length, resample
    pts = resample(lemniscate(), 1.0, closed=True)
    arc = arc_length(pts)
    (a, b), = find_crossings(pts, arc, arc[-1])
    assert np.hypot(*pts[a]) < 3.0 and np.hypot(*pts[b]) < 3.0


def test_galapagos_has_twin_cones_and_shells():
    shape, extras = build_galapagos()
    assert above_water(shape, *shape.required["cima_0"]) == pytest.approx(11.0, abs=0.3)
    assert len([k for k in shape.required if k.startswith("caparazon_")]) == 5
    assert len(shape.model.bridges) == 5 and len(extras.markers["puente_colgante"]) == 3      # 6 se cruzarian


def test_turtle_scales_step_up_to_the_centre_and_the_mouth_is_a_tunnel():
    shape, extras = build_turtle()
    assert above_water(shape, 0.0, 0.0) == pytest.approx(9.0, abs=0.05)
    assert above_water(shape, *shape.required["escama_int_2"]) == pytest.approx(6.0, abs=0.35)
    assert above_water(shape, *shape.required["escama_ext_0"]) == pytest.approx(3.0, abs=0.35)
    assert [v.name for v in shape.model.voids] == ["boca"] and extras.clearances[0].width_m == 14.0


def test_santorini_has_water_in_the_caldera_and_a_cut_ring(tmp_path):
    shape, extras = build_caldera()
    assert above_water(shape, 0.0, 10.0) < 0.0 and above_water(shape, 36.0, 0.0) > 8.0
    ch = math.radians(202.0)
    assert above_water(shape, 55.0 * math.cos(ch), 55.0 * math.sin(ch)) < 0.0                  # canal abierto
    r = write_kit_map(shape, extras, sheet=False, variants=tmp_path)
    assert r["ok"] and r["budget"]["triangles"] <= 350_000 and r["checks"]["nests"]["found"] == 8


@pytest.mark.parametrize("builder", [build_turtle_rally, build_volcano, build_faroe])
def test_catalogue_rally_circuits_keep_the_road_rules(builder):
    shape, extras = builder()
    from terrain_shapes.kit import max_grade_deg, min_radius, arc_length
    road = extras.road
    assert min_radius(road.pts, True)[0] >= 25.0
    assert max_grade_deg(arc_length(road.pts), road.z, True) <= 12.0
    assert extras.static["taludes"]["ok"]
    assert (road.z - WATER_M).max() <= 22.0                                            # sin alturas exageradas


def test_volcano_tunnels_are_found_under_the_rim_and_the_lava_is_crossed_on_a_viaduct():
    shape, extras = build_volcano()
    assert len(shape.model.voids) == 2 and [s.name for s in shape.model.solids] == ["viaducto"]
    assert volcano_spec().auto_tunnel_m == 14.0 and faroe_spec().auto_water
