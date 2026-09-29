"""Tests del mapa de España (Scripts/gen_terrain_spain.py y Scripts/terrain_geo).

    uv run --with pytest --with numpy --with scipy --with pillow --with scikit-image \
        pytest Scripts/tests/test_terrain_spain.py
"""

from __future__ import annotations

import sys
from pathlib import Path

import numpy as np
import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from gen_terrain_spain import DEFAULT_END, DEFAULT_START, DRY_M, flood, kill_boxes_uu  # noqa: E402
from gen_terrain_volume import world_index  # noqa: E402
from terrain_geo.layout import (CENTER_LAT, CENTER_LON, MAP_MAX_M, MAP_MIN_M, RASTER_PX_M, UU_PER_M, WATER_M,  # noqa: E402
                                game_to_lonlat, lonlat_to_game)
from terrain_geo.model import MAX_SLOPE, SpainModel, limit_slope  # noqa: E402
from terrain_vol.mesh import build_chunk  # noqa: E402

MADRID = (40.4168, -3.7038)
MULHACEN = (37.0535, -3.3113)
PALMA = (39.5696, 2.6502)
LISBOA = (38.7223, -9.1393)
TOULOUSE = (43.6047, 1.4442)
CADIZ_MAR = (36.3, -7.2)


@pytest.fixture(scope="module")
def model():
    return SpainModel()


def raster_at(model: SpainModel, raster: np.ndarray, point: tuple[float, float]) -> float:
    X, Y = lonlat_to_game(point[1], point[0])
    return float(raster[int(round((X - MAP_MIN_M) / RASTER_PX_M - 0.5)), int(round((Y - MAP_MIN_M) / RASTER_PX_M - 0.5))])


def test_projection_round_trip_and_center():
    assert lonlat_to_game(CENTER_LON, CENTER_LAT) == pytest.approx(((MAP_MIN_M + MAP_MAX_M) / 2.0,) * 2)
    for lat, lon in (MADRID, PALMA, LISBOA):
        X, Y = lonlat_to_game(lon, lat)
        assert game_to_lonlat(X, Y) == pytest.approx((lon, lat))


def test_border_mask_follows_geography(model):
    for city in (MADRID, MULHACEN, PALMA):
        assert raster_at(model, model.coverage, city) > 0.99, city
    for outside in (LISBOA, TOULOUSE, CADIZ_MAR):
        assert raster_at(model, model.coverage, outside) < 0.01, outside


def test_heights_follow_the_relief(model):
    def above_water(point):
        return raster_at(model, model.height, point) - WATER_M

    assert above_water(MADRID) == pytest.approx(0.45 + 650.0 * model.k, abs=2.0)      # la Meseta, ~650 m
    assert above_water(MULHACEN) > 24.0                                                # la cima de la Peninsula
    assert above_water(MULHACEN) > above_water(MADRID) + 12.0
    assert 0.0 < above_water(PALMA) < 4.0                                              # costa baja de Mallorca
    assert above_water(LISBOA) == pytest.approx(-1.5, abs=0.3)                         # fuera de España: mar somero
    assert above_water(CADIZ_MAR) < -3.0                                               # mar abierto: la batimetria baja


def test_relief_stays_below_the_volume_ceiling(model):
    assert model.height.max() < 30.0 and model.height.min() > -10.0


def test_slope_limiter_caps_a_spike_and_keeps_the_volume():
    field = np.zeros((60, 60))
    field[30, 30] = 20.0
    limited = limit_slope(field, 0.7, 400)
    assert limited.sum() == pytest.approx(field.sum())
    steps = np.abs(np.diff(limited, axis=0))
    assert steps.max() < 0.7 * 1.25 and limited.max() < 20.0


def test_relief_slopes_are_bounded(model):
    relief = model._relief()
    gy, gx = np.gradient(relief, RASTER_PX_M)
    slope = np.hypot(gx, gy)
    assert np.percentile(slope, 99) < MAX_SLOPE * 1.5


def test_the_camino_can_be_walked_from_roncesvalles_to_santiago(model):
    axis = np.arange(MAP_MIN_M, MAP_MAX_M + 1.0)
    top = model.ground_height(axis[:, None] * np.ones((1, len(axis))), np.ones((len(axis), 1)) * axis[None, :])
    start = world_index(tuple(float(v) for v in lonlat_to_game(DEFAULT_START[1], DEFAULT_START[0])))
    end = world_index(tuple(float(v) for v in lonlat_to_game(DEFAULT_END[1], DEFAULT_END[0])))
    assert top[start] > DRY_M and top[end] > DRY_M
    seen = flood(top, start)
    assert seen[end]
    assert not seen[world_index(tuple(float(v) for v in lonlat_to_game(PALMA[1], PALMA[0])))]        # el mar no se anda


def test_flood_does_not_wrap_around_the_edges():
    top = np.full((6, 6), 5.0)
    top[:, 3] = -20.0                                   # un canal de agua separa las columnas 0-2 de las 4-5
    seen = flood(top, (2, 0))
    assert seen[:, :3].all() and not seen[:, 3:].any()


def test_a_chunk_of_the_meseta_has_land_mesh(model):
    col, row = 2, 3                                     # centro (300, 200): Madrid queda en (277, 206)
    chunk = build_chunk(model, col, row)
    assert len(chunk.vertices) > 10000 and len(chunk.triangles) > 20000
    assert chunk.top.max() > WATER_M + 5.0


def test_kill_box_covers_the_sea_and_not_the_land():
    (box,) = kill_boxes_uu()
    top_m = (box["center"][2] + box["extent"][2]) / UU_PER_M
    assert WATER_M < top_m < WATER_M + 0.45
    assert box["extent"][0] == box["extent"][1] == (MAP_MAX_M - MAP_MIN_M) / 2.0 * UU_PER_M
