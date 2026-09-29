"""Tests del mapa a partir de una foto (Scripts/gen_terrain_portrait.py y terrain_geo/portrait.py), con una imagen
sintetica: dos manchas (una clara y una oscura) sobre gris.

    uv run --with pytest --with numpy --with scipy --with pillow --with scikit-image \
        pytest Scripts/tests/test_terrain_portrait.py
"""

from __future__ import annotations

import sys
from pathlib import Path

import numpy as np
import pytest
from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from terrain_geo.layout import MAP_MAX_M, MAP_MIN_M, RASTER_PX_M, WATER_M  # noqa: E402
from terrain_geo.portrait import PortraitModel  # noqa: E402

WIDTH, HEIGHT = 300, 400
FOCUS = (0.5, 0.5, 0.45, 0.45)
SIZE_M = 500.0


@pytest.fixture(scope="module")
def image_path(tmp_path_factory) -> Path:
    v, u = np.mgrid[0:HEIGHT, 0:WIDTH]
    gray = np.full((HEIGHT, WIDTH), 0.5)
    gray += 0.45 * np.exp(-(((u - 0.5 * WIDTH) / 30.0) ** 2 + ((v - 0.35 * HEIGHT) / 30.0) ** 2))      # mancha clara arriba
    gray -= 0.45 * np.exp(-(((u - 0.5 * WIDTH) / 30.0) ** 2 + ((v - 0.70 * HEIGHT) / 30.0) ** 2))      # mancha oscura abajo
    path = tmp_path_factory.mktemp("foto") / "sintetica.png"
    Image.fromarray((np.clip(gray, 0.0, 1.0) * 255).astype(np.uint8)).save(path)
    return path


@pytest.fixture(scope="module")
def model(image_path) -> PortraitModel:
    return PortraitModel(image_path, SIZE_M, 20.0, FOCUS)


def height_at(model: PortraitModel, point: tuple[float, float]) -> float:
    return float(model.ground_height(np.array([point[0]]), np.array([point[1]]))[0])


def test_the_island_is_an_oval_of_the_requested_size(model):
    a_m = FOCUS[2] * WIDTH * SIZE_M / (2.0 * FOCUS[3] * HEIGHT)
    expected_m2 = np.pi * a_m * SIZE_M / 2.0
    assert model.land.sum() * RASTER_PX_M ** 2 == pytest.approx(expected_m2, rel=0.03)


def test_the_mesh_is_closed_by_sea_at_the_volume_edge(model):
    h = model.height
    for edge in (h[0], h[-1], h[:, 0], h[:, -1]):
        assert edge.max() < WATER_M


def test_bright_is_high_and_dark_is_low(model):
    bright = height_at(model, model.to_game(0.5, 0.35))
    dark = height_at(model, model.to_game(0.5, 0.70))
    assert bright > dark + 3.0
    assert bright > WATER_M + 5.0


def test_outside_the_oval_is_shallow_sea(model):
    assert height_at(model, (MAP_MIN_M + 10.0, MAP_MIN_M + 10.0)) == pytest.approx(WATER_M - 1.5, abs=0.01)
    assert height_at(model, model.to_game(0.02, 0.5)) == pytest.approx(WATER_M - 1.5, abs=0.5)


def test_image_points_map_to_the_volume(model):
    mid = (MAP_MIN_M + MAP_MAX_M) / 2.0
    assert model.to_game(*FOCUS[:2]) == pytest.approx((mid, mid))
    north, _ = model.to_game(0.5, 0.2)                    # mas arriba en la foto = mas al Norte
    south, _ = model.to_game(0.5, 0.8)
    assert north > south
    _, east = model.to_game(0.8, 0.5)
    _, west = model.to_game(0.2, 0.5)
    assert east > west
