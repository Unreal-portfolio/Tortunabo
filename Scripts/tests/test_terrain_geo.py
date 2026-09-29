"""GeoRegion (Scripts/terrain_geo/region.py), calibracion de la exageracion (geomodel.py) y regresion de E01
tras pasar gen_terrain_spain.py a GeoRegion. Sin red: los rasters son sinteticos o los guardados de E01.

    uv run --with pytest --with numpy --with scipy --with pillow --with scikit-image --with pyproj \
        python -m pytest Scripts/tests/test_terrain_geo.py
"""

from __future__ import annotations

import math
import sys
from pathlib import Path

import numpy as np
import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from terrain_geo.geomodel import (GeoModel, ReliefParams, calibrate, calibrate_exaggeration,  # noqa: E402
                                  real_slopes)
from terrain_geo.layout import SPAIN_REGION, game_to_lonlat, lonlat_to_game  # noqa: E402
from terrain_geo.region import GeoRegion, choose_projection, raster_axis  # noqa: E402
from terrain_vol.layout import MAP_MIN_M, WATER_M  # noqa: E402

VARIANTS = Path(__file__).resolve().parent.parent / "terrain_volumes" / "Variants"
R = 6378137.0


def old_spain_to_game(lon, lat):
    """La proyeccion de E01 tal como estaba escrita antes de GeoRegion (terrain_geo/layout.py, 2026-09-29)."""
    def merc(lo, la):
        return R * np.radians(lo), R * np.log(np.tan(np.pi / 4.0 + np.radians(la) / 2.0))
    cx, cy = merc(-2.6, 39.9)
    mx, my = merc(np.asarray(lon, float), np.asarray(lat, float))
    return 250.0 + (my - cy) / 2800.0, 250.0 + (mx - cx) / 2800.0


# ── GeoRegion ───────────────────────────────────────────────────────────────────
def test_region_needs_a_box_or_a_country_and_a_valid_edge():
    with pytest.raises(ValueError):
        GeoRegion("x")
    with pytest.raises(ValueError):
        GeoRegion("x", bbox=(1.0, 0.0, 0.0, 1.0))
    with pytest.raises(ValueError):
        GeoRegion("x", bbox=(0.0, 0.0, 1.0, 1.0), edge="border")          # frontera sin pais
    with pytest.raises(ValueError):
        GeoRegion("x", bbox=(0.0, 0.0, 1.0, 1.0), edge="shore")
    GeoRegion("x", country="JPN", edge="both")


def test_projection_is_laea_in_europe_equirectangular_when_large_and_local_laea_elsewhere():
    assert choose_projection((-9.5, 36.0, 3.3, 43.8)) == "laea_europe"
    assert choose_projection((-25.0, 34.0, 45.0, 72.0)) == "laea_europe"
    assert choose_projection((-180.0, -72.0, 180.0, 72.0)) == "eqc"
    assert choose_projection((138.3, 34.6, 139.6, 35.7)) == "laea"


def test_spain_region_reproduces_the_old_e01_projection():
    for lon, lat in ((-3.7038, 40.4168), (-8.545, 42.881), (2.65, 39.57), (-2.6, 39.9)):
        assert lonlat_to_game(lon, lat) == pytest.approx(old_spain_to_game(lon, lat), abs=1e-9)
        X, Y = lonlat_to_game(lon, lat)
        assert game_to_lonlat(X, Y) == pytest.approx((lon, lat), abs=1e-9)
    assert SPAIN_REGION.game_projection().ground_m_per_game_m() == pytest.approx(2800.0 * math.cos(math.radians(39.9)))


@pytest.mark.parametrize("bbox,grid", [((138.30, 34.62, 139.62, 35.72), 6), ((120.945, 13.965, 121.045, 14.05), 3),
                                        ((5.0, 45.0, 15.0, 50.0), 6)])
def test_auto_zoom_fills_the_volume_inside_the_frame(bbox, grid):
    pytest.importorskip("pyproj")
    region = GeoRegion("z", bbox=bbox, grid=grid, frame_m=20.0)
    proj = region.game_projection()
    lon_min, lat_min, lon_max, lat_max = bbox
    t = np.linspace(0.0, 1.0, 50)
    lon = np.concatenate([lon_min + (lon_max - lon_min) * t, lon_min + (lon_max - lon_min) * t, np.full(50, lon_min),
                          np.full(50, lon_max)])
    lat = np.concatenate([np.full(50, lat_min), np.full(50, lat_max), lat_min + (lat_max - lat_min) * t,
                          lat_min + (lat_max - lat_min) * t])
    X, Y = proj.to_game(lon, lat)
    lo, hi = MAP_MIN_M + 20.0 - 1e-6, MAP_MIN_M + grid * 100.0 - 20.0 + 1e-6
    assert X.min() >= lo and X.max() <= hi and Y.min() >= lo and Y.max() <= hi
    span = max(X.max() - X.min(), Y.max() - Y.min())
    assert span == pytest.approx(grid * 100.0 - 40.0, rel=1e-6)            # el lado largo llena el volumen
    Xc, Yc = proj.to_game(0.5 * (lon_min + lon_max), 0.5 * (lat_min + lat_max))
    back = proj.to_lonlat(Xc, Yc)
    assert back == pytest.approx((0.5 * (lon_min + lon_max), 0.5 * (lat_min + lat_max)), abs=1e-9)
    assert 1.0 < proj.ground_m_per_game_m() < 5000.0


def synthetic_island(grid: int = 3, radius_m: float = 90.0, peak_real_m: float = 800.0):
    """Un cono de peak_real_m (m reales) en el centro del volumen y mar alrededor; con 20 m de suelo por m de juego."""
    region = GeoRegion("syn", bbox=(0.0, 0.0, 0.05, 0.05), projection="merc", center=(0.025, 0.025), scale=20.0,
                       grid=grid, frame_m=15.0)
    axis = raster_axis(grid)
    mid = MAP_MIN_M + grid * 50.0
    r = np.hypot(axis[:, None] - mid, axis[None, :] - mid)
    dem = peak_real_m * (1.0 - r / radius_m) - 40.0
    return region, dem


def test_coverage_follows_the_coast_drops_small_islands_and_keeps_the_frame():
    region, dem = synthetic_island()
    dem = dem.copy()
    axis = raster_axis(3)
    dem[np.ix_(np.abs(axis - 0.0) < 3.0, np.abs(axis - 0.0) < 3.0)] = 50.0          # islote de 6 x 6 m en una esquina
    coverage = region.coverage(dem)
    mid_i = len(axis) // 2
    assert coverage[mid_i, mid_i] > 0.99
    assert coverage[0, 0] == 0.0 and coverage[:, :12].max() == 0.0                    # el marco es mar
    assert coverage[np.ix_(np.abs(axis) < 2.0, np.abs(axis) < 2.0)].max() > 0.5       # islote conservado...
    small = GeoRegion("syn", bbox=region.bbox, projection="merc", center=region.center, scale=20.0, grid=3,
                      frame_m=15.0, min_island_m2=100.0)
    assert small.coverage(dem)[np.ix_(np.abs(axis) < 2.0, np.abs(axis) < 2.0)].max() < 0.2    # ...o descartado


def test_calibration_reaches_the_target_slope_inside_the_bounds():
    slopes = np.full(1000, 0.05)                                   # 2,9 grados reales
    assert calibrate_exaggeration(slopes, 14.0) == pytest.approx(math.tan(math.radians(14.0)) / 0.05)
    assert calibrate_exaggeration(slopes, 40.0) == 6.0             # pediria 16x: se queda en 6
    assert calibrate_exaggeration(np.full(10, 0.5), 10.0) == 3.0   # pediria 0,35x: se queda en 3
    assert calibrate_exaggeration(np.full(10, 0.5), 10.0, bounds=(0.2, 6.0)) == pytest.approx(0.3527, abs=1e-3)


def test_calibrated_model_has_the_target_slope_and_fits_the_volume():
    region, dem = synthetic_island(peak_real_m=300.0)
    coverage = region.coverage(dem)
    cal = calibrate(region, dem, coverage, target_slope_deg=20.0, bounds=(0.5, 6.0))
    assert not cal.capped and 0.5 <= cal.exaggeration <= 6.0
    game_slopes = real_slopes(dem, coverage > 0.5, cal.ground_m_per_game_m * 0.5, 3.0) * cal.exaggeration
    assert math.degrees(math.atan(np.percentile(game_slopes, 90))) == pytest.approx(20.0, abs=0.5)
    model = GeoModel(region, dem, coverage, cal.k, 7, ReliefParams(), cal.z_range)
    assert model.height.max() < cal.z_range.z_max_m - 2.0
    assert model.height[len(model.height) // 2, len(model.height) // 2] > WATER_M + 5.0


def test_a_peak_that_does_not_fit_lowers_the_exaggeration_then_grows_the_range():
    region, dem = synthetic_island(peak_real_m=4000.0)            # 200 m de juego a 1x: no cabe
    coverage = region.coverage(dem)
    cal = calibrate(region, dem, coverage, target_slope_deg=40.0, bounds=(3.0, 6.0))
    assert cal.capped and cal.exaggeration == 3.0
    assert cal.z_range.z_max_m > dem.max() * cal.k + WATER_M
    fits = calibrate(region, dem, coverage, target_slope_deg=40.0, bounds=(0.1, 6.0))
    assert fits.capped and fits.exaggeration < 3.0 and fits.z_range.levels == 128


def test_credits_name_the_sources_and_the_projection():
    text = SPAIN_REGION.credits()
    assert "Terrain Tiles" in text and "Natural Earth" in text and "E01_espana" in text
    assert "Web Mercator" in text


# ── Regresion de E01 ────────────────────────────────────────────────────────────
def test_e01_chunks_are_unchanged_after_the_georegion_refactor(tmp_path):
    from gen_terrain_volume import build_all
    from terrain_geo.model import SpainModel
    from terrain_vol.export import chunk_name, write_chunk
    chunks = build_all(SpainModel())
    stored = VARIANTS / "E01_espana" / "Chunks"
    changed = []
    for (col, row), chunk in chunks.items():
        out = tmp_path / f"{chunk_name(col, row)}.bin"
        write_chunk(out, chunk)
        if out.read_bytes() != (stored / out.name).read_bytes():
            changed.append(out.name)
    assert changed == [], f"trozos de E01 distintos: {changed}"
