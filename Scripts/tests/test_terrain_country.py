"""Paises enteros miniaturizados (Scripts/gen_terrain_country.py, terrain_geo/country.py y rally.py): giro y
compresion de la proyeccion, regla de exageracion de E01, perfil de calzada, terrazas, tuneles validados en 3D y
los mapas generados (presupuesto, costuras, CREDITS y lamina).

    uv run --with pytest --with numpy --with scipy --with pillow --with scikit-image --with pyproj \
        python -m pytest Scripts/tests/test_terrain_country.py
"""

from __future__ import annotations

import json
import math
import sys
from pathlib import Path

import numpy as np
import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from terrain_geo.country import E01_EXAGGERATION, E01_K, lipschitz, main_axis_bearing, terraces  # noqa: E402
from terrain_geo.rally import CLEAR_M, Tunnel, check_tunnel  # noqa: E402
from terrain_geo.region import GameProjection  # noqa: E402
from terrain_vol.layout import WATER_M  # noqa: E402
from terrain_vol.validate import check_budget, check_seams  # noqa: E402

SCRIPTS = Path(__file__).resolve().parent.parent
VARIANTS = SCRIPTS / "terrain_volumes" / "Variants"
SHEETS = SCRIPTS.parent / "Docs" / "Mapas"
COUNTRY_MAPS = ["L10_japon_v2", "I01_filipinas_v2", "L02_reino_unido", "L03_francia", "L04_alemania", "L05_italia",
                "L06_brasil", "L07_rusia", "L08_polonia", "L09_turquia", "L11_corea_sur", "L12_china", "L13_taiwan"]


def test_rotated_and_squashed_projection_round_trips():
    proj = GameProjection("merc", (135.0, 36.0), 2500.0, 9, "", (0.0, 0.0), 33.0, (400.0, 300.0), 0.5)
    for lon, lat in ((130.5, 31.6), (141.7, 45.4), (138.7, 35.36)):
        X, Y = proj.to_game(lon, lat)
        assert proj.to_lonlat(X, Y) == pytest.approx((lon, lat), abs=1e-9)
    # El rumbo 33 grados queda como Norte del juego: un punto al Noreste a ese rumbo solo sube en X.
    x0, y0 = (float(v) for v in GameProjection("merc", (135.0, 36.0), 1.0).project(135.0, 36.0))
    plain = GameProjection("merc", (135.0, 36.0), 2500.0, 9, "", (x0, y0), 33.0, (400.0, 300.0))
    d = 100000.0
    lon, lat = plain.unproject(x0 + d * math.sin(math.radians(33)), y0 + d * math.cos(math.radians(33)))
    X, Y = plain.to_game(lon, lat)
    assert Y == pytest.approx(300.0, abs=1e-6) and X > 400.0


def test_main_axis_bearing():
    t = np.linspace(-1.0, 1.0, 200)
    x, y = 1000.0 * t * math.sin(math.radians(33)), 1000.0 * t * math.cos(math.radians(33))
    assert main_axis_bearing(x + np.random.default_rng(1).normal(0, 5, 200), y) == pytest.approx(33.0, abs=1.0)


def test_e01_exaggeration_rule():
    assert E01_K * 2148.0 == pytest.approx(E01_EXAGGERATION, abs=0.1)        # E01: 40 m por 3480 m a 2,15 km por m
    assert min(E01_EXAGGERATION, E01_K * 1000.0) == pytest.approx(11.49, abs=0.01)   # pais pequeño: escala de E01


def test_road_profile_grade_is_limited():
    values = np.concatenate([np.zeros(20), np.full(20, 10.0), np.zeros(20)])
    out = lipschitz(values, np.ones(60), 0.15)
    assert np.abs(np.diff(out)).max() <= 0.15 + 1e-9


def test_terraces_make_flats():
    h = WATER_M + 0.45 + np.linspace(0.0, 10.0, 1001)
    t = terraces(h, 2.5, 0.45)
    flat = np.abs(np.diff(t)) < 1e-9
    assert flat.mean() > 0.6 and t.max() <= h.max() + 1e-9


def test_tunnel_is_checked_in_3d():
    pts = np.stack([np.linspace(0.0, 60.0, 61), np.zeros(61)], axis=1)
    tunnel = Tunnel(pts, np.zeros(61), np.ones(61, dtype=bool))

    def solid(X, Y, Z):
        return 20.0 - Z[None, None, :] + 0.0 * X[..., None]

    def bored(X, Y, Z):
        void = tunnel.void(X, Y, Z)
        return np.minimum(solid(X, Y, Z), -void) if void is not None else solid(X, Y, Z)

    assert not check_tunnel(solid, tunnel)["ok"]
    good = check_tunnel(bored, tunnel)
    assert good["ok"] and good["clearance_m"] >= 5.0 and good["width_m"] >= 12.0 and CLEAR_M >= 5.0


@pytest.mark.parametrize("name", COUNTRY_MAPS)
def test_country_map_is_within_budget_without_cracks_and_documented(name):
    out = VARIANTS / name
    if not (out / "manifest.json").exists():
        pytest.skip(f"{name} sin generar")
    manifest = json.loads((out / "manifest.json").read_text(encoding="utf-8"))
    assert manifest["mode"] in ("rally", "tct") and "validation" in manifest
    budget = check_budget(out)
    assert budget["ok"], budget
    assert check_seams(out)["ok"]
    assert "Terrain Tiles" in (out / "CREDITS.txt").read_text(encoding="utf-8")
    sheet = SHEETS / f"{name}.png"
    assert sheet.exists() and sheet.stat().st_size < 1_000_000
