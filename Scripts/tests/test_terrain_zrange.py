"""Rango vertical configurable del voxelizado (terrain_vol.layout.ZRange): por defecto 128 niveles y sin
cambiar la malla de los mapas que ya existian (P01, E01, Mapa01).

    uv run --with pytest --with numpy --with scipy --with pillow --with scikit-image \
        python -m pytest Scripts/tests/test_terrain_zrange.py
"""

from __future__ import annotations

import sys
from pathlib import Path

import numpy as np
import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from terrain_vol.density import MapModel  # noqa: E402
from terrain_vol.export import write_chunk  # noqa: E402
from terrain_vol.layout import DEFAULT_Z_RANGE, LEGACY_Z_RANGE, STEP_Z_M, Z_MAX_M, Z_MIN_M, Z_SAMPLES, ZRange  # noqa: E402
from terrain_vol.mesh import build_chunk, model_z_range  # noqa: E402

VARIANTS = Path(__file__).resolve().parent.parent / "terrain_volumes" / "Variants"


def test_default_range_is_128_levels_from_the_same_floor():
    assert DEFAULT_Z_RANGE.levels == Z_SAMPLES == 128
    assert DEFAULT_Z_RANGE.z_min_m == Z_MIN_M and DEFAULT_Z_RANGE.step_m == STEP_Z_M
    assert DEFAULT_Z_RANGE.z_max_m == pytest.approx(53.5)
    assert LEGACY_Z_RANGE.levels == 89 and LEGACY_Z_RANGE.z_max_m == pytest.approx(Z_MAX_M)
    assert len(DEFAULT_Z_RANGE.z_values()) == 128


def test_covering_range_grows_only_when_the_terrain_needs_it():
    assert ZRange.covering(20.0) == DEFAULT_Z_RANGE
    tall = ZRange.covering(80.0, headroom_m=3.0)
    assert tall.z_max_m >= 83.0 and tall.levels > 128
    with pytest.raises(ValueError):
        ZRange(levels=1)


def test_a_model_can_fix_its_own_range():
    class Tall:
        z_range = ZRange(levels=200)

    assert model_z_range(Tall()).levels == 200
    assert model_z_range(object()) == DEFAULT_Z_RANGE


def _same_mesh(model, col: int, row: int) -> None:
    old = build_chunk(model, col, row, LEGACY_Z_RANGE)
    new = build_chunk(model, col, row)
    assert np.array_equal(old.vertices, new.vertices)
    assert np.array_equal(old.triangles, new.triangles)
    assert np.array_equal(old.colors, new.colors)
    assert np.allclose(old.top, new.top)
    assert new.standable.shape[2] == 128 and old.standable.shape[2] == 89
    assert np.array_equal(old.standable, new.standable[:, :, :89])


def _same_as_stored(model, col: int, row: int, stored: Path, tmp_path: Path) -> None:
    out = tmp_path / stored.name
    write_chunk(out, build_chunk(model, col, row))
    assert out.read_bytes() == stored.read_bytes(), f"{stored} ha cambiado"


def test_spain_mesh_does_not_change(tmp_path):
    from terrain_geo.model import SpainModel
    model = SpainModel()
    _same_mesh(model, 2, 3)
    _same_as_stored(model, 2, 3, VARIANTS / "E01_espana" / "Chunks" / "r3c2.bin", tmp_path)


def test_platforms_mesh_does_not_change(tmp_path):
    from terrain_platforms.layout import LAYOUT_FILE, load_layout
    from terrain_platforms.model import PlatformModel
    model = PlatformModel(20260929, load_layout(LAYOUT_FILE))
    _same_mesh(model, 1, 1)                          # la meseta +30 m llega a 26 m
    _same_as_stored(model, 1, 1, VARIANTS / "P01_plataformas" / "Chunks" / "r1c1.bin", tmp_path)


def test_volume_map_mesh_does_not_change():
    model = MapModel(20260925)                       # Mapa01: el acantilado toca su techo de 32 m
    _same_mesh(model, 0, 0)
