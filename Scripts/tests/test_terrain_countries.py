"""Regla de validez de un pais de Rally (terrain_geo/validity.py, decision del 2026-09-30), region sin pais (el
mundo, cortado por un meridiano) y lo que documentan los mapas generados. Datos sinteticos pequeños, sin red.

    uv run --with numpy --with scipy --with pillow --with scikit-image --with pyproj --with matplotlib --with pytest \
        python -m pytest Scripts/tests/test_terrain_countries.py -q
"""

from __future__ import annotations

import json
import math
import sys
from pathlib import Path

import numpy as np
import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from terrain_geo.countries import COUNTRIES  # noqa: E402
from terrain_geo.country import CountryPreset, Road, plan_region  # noqa: E402
from terrain_geo.rally import build_tunnels  # noqa: E402
from terrain_geo.sources import wrap_polygons  # noqa: E402
from terrain_geo.validity import (MIN_ROAD_BRIDGES, ROAD_SLOPE_DEG, ROAD_WIDTH_M, Discard, RallyChecks, loop_ok,  # noqa: E402
                                  new_discards, rally_verdict, road_breaks, road_ok, road_reach, sustained_grade_deg)
from terrain_vol.layout import WATER_M  # noqa: E402

SCRIPTS = Path(__file__).resolve().parent.parent
VARIANTS = SCRIPTS / "terrain_volumes" / "Variants"
START, END = (20, 5), (20, 114)
TUNNEL_OK = {"ok": True, "length_m": 20.0}


def road_top(width: int = 16, ramp_deg: float = 0.0, ramp_m: int = 12) -> np.ndarray:
    """Mapa de 40 x 120 m: agua y una calzada recta de `width` m de Oeste a Este a 2 m sobre el agua; con ramp_deg,
    un lomo en el centro que sube y baja ramp_m metros con esa pendiente."""
    top = np.full((40, 120), WATER_M - 2.0)
    j = np.arange(120)
    rise = math.tan(math.radians(ramp_deg)) * np.clip(ramp_m - np.abs(j - 60), 0, None)
    top[20 - width // 2:20 - width // 2 + width, :] = WATER_M + 2.0 + rise[None, :]
    return top


def checks(top: np.ndarray, links=(), tunnels=(), loops=(), bridges: int = MIN_ROAD_BRIDGES, **kwargs) -> RallyChecks:
    is_open, linked = road_reach(top, START, END, list(links))
    return RallyChecks(is_open, linked, tuple(tunnels), tuple(loops), bridges, **kwargs)


def test_flat_road_without_tunnels_is_valid():
    verdict = rally_verdict(True, checks(road_top()))
    assert verdict["ok"] and verdict["motivos"] == []
    assert verdict["calzada"] and not verdict["tuneles_necesarios"] and verdict["tuneles"] == []


def test_road_needs_12_m_of_width():
    assert ROAD_WIDTH_M == 12.0
    assert not road_ok(road_top(width=10), START, END, [])          # valia con la regla vieja (10 m)
    assert road_ok(road_top(width=13), START, END, [])


def test_gentle_ramp_passes_and_steep_ramp_requires_a_tunnel():
    assert rally_verdict(True, checks(road_top(ramp_deg=9.0)))["ok"]
    steep = road_top(ramp_deg=20.0)
    verdict = rally_verdict(True, checks(steep))
    assert not verdict["ok"] and verdict["motivos"] == ["tunel_necesario"] and verdict["tuneles_necesarios"]
    assert road_breaks(steep, np.stack([np.full(110, 20), np.arange(5, 115)], axis=1))       # dice donde se corta
    arc = np.arange(120.0)
    assert sustained_grade_deg(arc, steep[20]).max() > ROAD_SLOPE_DEG
    assert sustained_grade_deg(arc, road_top(ramp_deg=9.0)[20]).max() <= 9.0 + 1e-6


def test_valid_tunnel_under_the_steep_stretch_makes_the_road_valid():
    steep = road_top(ramp_deg=20.0)
    mouths = [((20, 44), (20, 76))]
    verdict = rally_verdict(True, checks(steep, links=mouths, tunnels=[TUNNEL_OK]))
    assert verdict["ok"] and verdict["tuneles_necesarios"] and verdict["calzada"]
    # Un tunel tallado que no pasa check_tunnel no cuenta como enlace e invalida el mapa.
    bad = rally_verdict(True, checks(steep, tunnels=[{"ok": False, "length_m": 20.0}]))
    assert not bad["ok"] and set(bad["motivos"]) == {"tunel_no_valido", "calzada_cortada"}


def test_bridges_and_base_report_are_still_required():
    assert rally_verdict(True, checks(road_top(), bridges=1))["motivos"] == ["puentes"]
    assert rally_verdict(False, checks(road_top()))["motivos"] == ["mapa"]


def test_broken_loop_is_discarded_and_counted_without_invalidating():
    top = road_top()
    branch = np.stack([np.full(40, 20), np.arange(40, 80)], axis=1)
    assert loop_ok(top, branch)
    broken = top.copy()
    broken[:, 58:62] = WATER_M - 2.0                     # la rama se corta; la calzada principal va por el tunel
    assert not loop_ok(broken, branch)
    state = checks(broken, links=[((20, 50), (20, 70))], tunnels=[TUNNEL_OK], loops=[False])
    discard = new_discards(state, loop_ids=[1], tunnel_ids=[1])
    assert discard == Discard(loops=frozenset({1})) and discard
    assert discard.merged(Discard(tunnels=frozenset({0}))) == Discard(frozenset({1}), frozenset({0}))
    # Tras regenerar sin la rama: ningun lazo tallado, uno descartado, y el mapa vale.
    verdict = rally_verdict(True, checks(broken, links=[((20, 50), (20, 70))], tunnels=[TUNNEL_OK], discarded_loops=1))
    assert verdict["ok"] and verdict["lazos"] == [] and verdict["lazos_descartados"] == 1 and verdict["lazos_rotos"] == 0
    assert not new_discards(checks(top), [], [])


def test_failed_tunnel_is_discarded():
    state = RallyChecks(True, True, ({"ok": False}, TUNNEL_OK), (True, True), 2)
    assert new_discards(state, [0, 1], [0, 1]) == Discard(tunnels=frozenset({0}))


def test_discarded_loop_keeps_the_tunnel_and_drops_the_branch():
    # Calzada en U alrededor de un cerro; el atajo recto (puntos 0 a 80) pasa por debajo.
    path = np.concatenate([np.stack([np.arange(10, 41), np.full(31, 10)], 1), np.stack([np.full(19, 40), np.arange(11, 30)], 1),
                           np.stack([np.arange(40, 9, -1), np.full(31, 30)], 1)])
    road = Road(path, np.full(len(path), WATER_M + 2.0))
    ii, jj = np.meshgrid(np.arange(60.0), np.arange(60.0), indexing="ij")
    hill = WATER_M + 2.0 + 20.0 * np.exp(-((ii - 10.0) ** 2 + (jj - 20.0) ** 2) / 40.0)
    main, branches, tunnels, loops = build_tunnels(hill, road, [(0, 80, 12.0)])
    assert len(branches) == 1 and len(tunnels) == 1 and tunnels[0].covered.any()
    main2, branches2, tunnels2, loops2 = build_tunnels(hill, road, [(0, 80, 12.0)], frozenset({0}))
    assert branches2 == [] and len(tunnels2) == 1 and loops2 == loops
    assert np.array_equal(main2.path, main.path)


def test_world_polygons_are_cut_at_a_meridian():
    square = [np.array([[-40.0, 0.0], [-10.0, 0.0], [-10.0, 10.0], [-40.0, 10.0], [-40.0, 0.0]])]
    east = [np.array([[100.0, 0.0], [120.0, 0.0], [120.0, 10.0], [100.0, 10.0], [100.0, 0.0]])]
    out = wrap_polygons([square, east], -26.0)
    spans = sorted((float(p[0][:, 0].min()), float(p[0][:, 0].max())) for p in out)
    # El que cruza el corte queda en dos, un pelo dentro del meridiano de corte.
    expected = [(-26.0, -10.0), (100.0, 120.0), (320.0, 334.0)]
    assert np.allclose(np.array(spans), np.array(expected), atol=1e-5)


def test_region_without_country_needs_a_box():
    with pytest.raises(ValueError):
        plan_region(CountryPreset("X00", None, "mul", "rally", "sin pais ni caja"))
    world = COUNTRIES["W01_mundo"]
    assert world.country is None and world.lon_cut is not None
    assert world.bbox[0] == world.lon_cut and world.bbox[2] == world.lon_cut + 360.0 and world.bbox[1] > -60.0


def test_sheet_profile_is_optional(tmp_path):
    pytest.importorskip("matplotlib")
    import inspect

    from terrain_vol.sheet import render_sheet
    assert inspect.signature(render_sheet).parameters["profile"].default is None
    top = road_top()
    plain = render_sheet(top, tmp_path / "a.png", "a")
    with_profile = render_sheet(top, tmp_path / "b.png", "b", profile=(np.arange(120.0), top[20] - WATER_M))
    assert plain > 0 and with_profile > 0 and (tmp_path / "b.png").exists()


@pytest.mark.parametrize("name", list(COUNTRIES))
def test_generated_country_is_valid_and_documented(name):
    manifest_path = VARIANTS / name / "manifest.json"
    if not manifest_path.exists():
        pytest.skip(f"{name} sin generar")
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    geo = manifest["geo"]
    if "slope_deg" not in geo:
        pytest.skip(f"{name} generado con la regla anterior")
    assert {"exaggeration", "tunnels", "bridges", "discarded_loops"} <= set(geo)
    assert manifest["recorrible"], manifest["validation"]
    if manifest["mode"] == "rally":
        validation = manifest["validation"]
        assert validation["calzada"] and validation["puentes_en_calzada"] >= MIN_ROAD_BRIDGES
        assert all(t["ok"] for t in validation["tuneles"]) and validation["lazos_rotos"] == 0
        assert geo["road_max_slope_deg"] <= ROAD_SLOPE_DEG
