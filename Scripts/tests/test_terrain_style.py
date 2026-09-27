"""Tests de terrain_vol.style (MapStyle).

    uv run --with pytest --with numpy --with scipy --with pillow --with scikit-image \
        pytest Scripts/tests/test_terrain_style.py
"""

from __future__ import annotations

import sys
from pathlib import Path

import numpy as np
import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from terrain_vol.density import MapModel  # noqa: E402
from terrain_vol.mesh import build_chunk  # noqa: E402
from terrain_vol.style import MAPA01_STYLE, MapStyle  # noqa: E402

MAPA01_SEED = 20260925


# ── MapStyle ────────────────────────────────────────────────────────────────────────
def test_mapa01_style_reproduce_las_zonas_de_siempre():
    zones = MAPA01_STYLE.zones()
    assert zones == (("cliffs", 0.25, 75.0), ("canyon", 0.16, 40.0), ("marsh", 0.20, 85.0),
                     ("algae", 0.24, 80.0), ("beach", 0.15, 95.0))


@pytest.mark.parametrize("field", ["cliffs_share", "canyon_share", "water_share", "dunes_share"])
def test_una_zona_a_cero_genera_un_mapa_valido(field):
    """Una fraccion a 0 no elimina la zona de ZONES (rompería el reparto de arco y las redes
    que dependen de ella): queda como una banda minima. El mapa debe seguir generandose sin
    NaN/Inf ni excepciones."""
    style = MapStyle(**{field: 0.0})
    zones = style.zones()
    assert abs(sum(s for _, s, _ in zones) - 1.0) < 1e-9
    assert all(s > 0.0 for _, s, _ in zones), "una zona a 0 no debe desaparecer del reparto de arco"

    model = MapModel(90001, style=style, erode=False)
    for col, row in ((0, 0), (2, 2), (5, 5)):
        chunk = build_chunk(model, col, row)
        assert len(chunk.vertices) > 0
        assert np.isfinite(chunk.vertices).all()
        assert np.isfinite(chunk.normals).all()


def test_estilo_con_rio_no_revienta():
    style = MapStyle(river_count=3, island_density=0.9)
    model = MapModel(90002, style=style, erode=False)
    assert len(model.rivers) == 3
    chunk = build_chunk(model, 3, 3)
    assert np.isfinite(chunk.vertices).all()
