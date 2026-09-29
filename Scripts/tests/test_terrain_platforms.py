"""Tests del mapa de plataformas P01 (Scripts/gen_terrain_platforms.py y Scripts/terrain_platforms).

    uv run --with pytest --with numpy --with scipy --with pillow --with scikit-image \
        pytest Scripts/tests/test_terrain_platforms.py
"""

from __future__ import annotations

import sys
from pathlib import Path

import numpy as np
import pytest
from scipy import ndimage

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from gen_terrain_platforms import check, kill_boxes_uu, pick_start_end  # noqa: E402
from gen_terrain_volume import build_all, global_standable, world_index  # noqa: E402
from terrain_platforms.layout import (CLASS_BRIDGE, CLASS_WATER, GRID, PLATFORM_ABOVE_WATER_M, PX_M, RIVER_DEPTH_M,  # noqa: E402
                                      UU_PER_M, VOLUME_M, WATER_M, load_layout, pad_layout)
from terrain_platforms.model import PlatformModel  # noqa: E402
from terrain_vol.export import global_top  # noqa: E402
from terrain_vol.layout import STEP_Z_M, Z_MIN_M  # noqa: E402


@pytest.fixture(scope="module")
def model():
    return PlatformModel()


@pytest.fixture(scope="module")
def chunks(model):
    return build_all(model, grid=GRID)


@pytest.fixture(scope="module")
def top(chunks):
    return global_top(chunks, grid=GRID)


@pytest.fixture(scope="module")
def result(model, chunks):
    start, end = pick_start_end(model.layout)
    return check(model, chunks, start, end)


def sample_grid(padded: np.ndarray) -> np.ndarray:
    """Un raster de todo el volumen (0,5 m por pixel) en las muestras de 1 m de global_top (301 x 301)."""
    idx = np.minimum(2 * np.arange(int(VOLUME_M) + 1), padded.shape[0] - 1)
    return padded[np.ix_(idx, idx)]


def on_grid(layout: np.ndarray) -> np.ndarray:
    return sample_grid(pad_layout(layout))


def expected_top(layout: np.ndarray) -> np.ndarray:
    """Cota esperada de la cima en cada muestra de 1 m (NaN en rio y puentes)."""
    grid = on_grid(layout)
    out = np.full(grid.shape, np.nan)
    for cls, above in PLATFORM_ABOVE_WATER_M.items():
        out[grid == cls] = WATER_M + above
    return out


def test_layout_has_every_class():
    layout = load_layout()
    assert layout.shape == (400, 400)
    counts = {cls: int((layout == cls).sum()) for cls in range(5)}
    assert all(c > 5000 for c in counts.values()), counts
    assert counts[CLASS_WATER] > counts[CLASS_BRIDGE]


def test_nine_bridges_and_the_neck_crossing_is_one(model):
    lengths = sorted(b.length for b in model.bridges)
    assert len(model.bridges) == 9
    assert lengths[-1] > 60.0          # el que cruza el cuello oscuro (dos tramos del plano unidos)
    levels = {WATER_M + a for a in PLATFORM_ABOVE_WATER_M.values()}
    for bridge in model.bridges:
        assert set(bridge.z_ends) <= levels
        assert 3.6 / 2 <= bridge.half_width.min() and bridge.half_width.max() <= 9.0 / 2


def test_deck_hangs_between_its_ends(model):
    for bridge in model.bridges:
        s0, s1 = bridge.span
        ends = bridge.deck_top(np.array([s0, s1]))
        assert ends == pytest.approx(bridge.z_ends)
        middle = bridge.deck_top(np.array([(s0 + s1) / 2.0]))[0]
        assert middle < 0.5 * (bridge.z_ends[0] + bridge.z_ends[1]) - 0.4 * bridge.sag


def test_platform_tops_follow_the_drawing(model, top):
    expected = expected_top(model.layout)
    grid = on_grid(model.layout)
    # A 5 m del contorno de cada plataforma: el acantilado inclinado y el escalon entre dos alturas
    # que se tocan no cuentan.
    inner = np.zeros(grid.shape, dtype=bool)
    for cls in PLATFORM_ABOVE_WATER_M:
        inner |= ndimage.binary_erosion(grid == cls, iterations=5)
    assert inner.sum() > 4000
    assert np.abs(top[inner] - expected[inner]).max() < 1.5


def test_river_is_below_the_water_and_shallow(model, top):
    near_land = ndimage.binary_dilation(pad_layout(model.layout) != CLASS_WATER, iterations=int(20.0 / PX_M))
    far = ~sample_grid(near_land)
    assert far.sum() > 3000
    assert top[far].max() < WATER_M - 0.5
    assert top[far].min() > WATER_M - RIVER_DEPTH_M - 1.0


def test_no_platform_is_cut_by_the_map_border(top):
    """La malla no se cierra en el borde del volumen: ahi solo puede haber lecho de rio."""
    for edge in (top[0], top[-1], top[:, 0], top[:, -1]):
        assert edge.max() < WATER_M


def test_start_reaches_the_end_and_every_linked_platform(result):
    assert result["start_ground"] and result["end_reached"]
    assert result["bridges_walked"] == result["bridges"] == 9
    assert result["missed"] == [] and len(result["reached"]) == 9


def test_the_deck_is_walkable_along_its_whole_length(model, chunks):
    standable = global_standable(chunks, grid=GRID)
    for bridge in model.bridges:
        s0, s1 = bridge.span
        for k in range(len(bridge.arc)):
            if not s0 + 1.0 <= bridge.arc[k] <= s1 - 1.0:
                continue
            i, j = world_index(bridge.points[k])
            z = bridge.deck_top(bridge.arc[k:k + 1])[0]
            level = int(round((z - Z_MIN_M) / STEP_Z_M))
            assert standable[i, j, level - 2:level + 1].any(), (bridge.points[k], z)


def test_kill_box_covers_the_river_and_not_the_platforms():
    (box,) = kill_boxes_uu()
    top_m = (box["center"][2] + box["extent"][2]) / UU_PER_M
    assert WATER_M < top_m < WATER_M + min(PLATFORM_ABOVE_WATER_M.values())
    assert box["extent"][0] == box["extent"][1] == VOLUME_M / 2.0 * UU_PER_M
