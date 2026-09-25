"""Tests del mapa volumetrico (Scripts/gen_terrain_volume.py y Scripts/terrain_vol).

    uv run --with pytest --with numpy --with scipy --with pillow --with scikit-image \
        pytest Scripts/tests/test_terrain_volume.py
"""

from __future__ import annotations

import sys
from pathlib import Path

import numpy as np
import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from gen_terrain_volume import build_all, global_standable, ground_level, walk, world_index  # noqa: E402
from terrain_vol.density import MapModel  # noqa: E402
from terrain_vol.export import read_chunk, write_chunk  # noqa: E402
from terrain_vol.layout import CELL_SAMPLES, STEP_Z_M, WATER_M, Z_MIN_M  # noqa: E402
from terrain_vol.route import ZONE_NAMES  # noqa: E402

SEED = 20260925


@pytest.fixture(scope="module")
def model():
    return MapModel(SEED)


@pytest.fixture(scope="module")
def chunks(model):
    return build_all(model)


@pytest.fixture(scope="module")
def standable(chunks):
    return global_standable(chunks)


def test_las_zonas_siguen_el_orden_del_recorrido(model):
    s = model.route.arc[::10]
    pts = model.route.points[::10]
    weights = model.zones.weights(pts[:, 0], pts[:, 1])
    dominant = [ZONE_NAMES[int(np.argmax([weights[z][k] for z in ZONE_NAMES]))] for k in range(len(s))]
    runs = [z for k, z in enumerate(dominant) if k == 0 or dominant[k - 1] != z]
    assert tuple(runs) == ZONE_NAMES


def test_se_llega_a_pie_del_inicio_al_final(model, standable):
    start_ij, end_ij = world_index(model.route.points[0]), world_index(model.route.points[-1])
    start = (*start_ij, ground_level(standable, *start_ij))
    end = (*end_ij, ground_level(standable, *end_ij))
    assert start[2] >= 0 and end[2] >= 0
    assert walk(standable, start)[end]


def test_el_tunel_tiene_roca_encima_y_se_cruza_por_dentro(model, chunks):
    assert len(model.tunnels) >= 2
    covered = 0
    for a, b in model.tunnels:
        p = model.route.point_at(0.5 * (a + b))
        i, j = world_index(p)
        col, row = j // (CELL_SAMPLES - 1), i // (CELL_SAMPLES - 1)
        chunk = chunks[(min(col, 5), min(row, 5))]
        li, lj = i - row * (CELL_SAMPLES - 1), j - col * (CELL_SAMPLES - 1)
        levels = np.nonzero(chunk.standable[li, lj])[0]
        # Dos niveles de pie en la misma columna: el suelo del tunel y la cima de la dorsal.
        assert len(levels) >= 2, f"tunel en {p}: {levels}"
        floor_z = Z_MIN_M + STEP_Z_M * levels.min()
        roof_z = Z_MIN_M + STEP_Z_M * levels.max()
        assert roof_z - floor_z >= 5.0
        covered += 1
    assert covered == len(model.tunnels)


def test_los_laberintos_tienen_callejones_y_un_solo_paso(model):
    cliffs = model.nets["cliffs"]
    assert len(cliffs.dead_ends()) >= 6
    assert len(model.nets["algae"].dead_ends()) + len(model.nets["algae"].edges) - len(model.nets["algae"].nodes) >= 6
    entrance, exit_ = cliffs.chain[0], cliffs.chain[-1]
    for a, b in zip(cliffs.chain[:-1], cliffs.chain[1:]):
        assert not cliffs.connected(entrance, exit_, without=(a, b)), "hay un atajo que evita la cadena"


def test_la_orilla_del_lago_no_es_recta(chunks):
    from terrain_vol.export import global_top
    water = global_top(chunks) < WATER_M
    shore = water ^ np.roll(water, 1, axis=0) | water ^ np.roll(water, 1, axis=1)
    shore[0, :] = shore[-1, :] = shore[:, 0] = shore[:, -1] = False
    rows = [np.sum(shore[i]) for i in range(shore.shape[0])]
    cols = [np.sum(shore[:, j]) for j in range(shore.shape[1])]
    total = max(int(shore.sum()), 1)
    # Ninguna fila ni columna concentra una recta larga de orilla.
    assert max(rows + cols) / total < 0.2


def test_la_calzada_del_lago_es_seca(model, chunks):
    from terrain_vol.export import global_top
    top = global_top(chunks)
    s0, s1 = model.route.zone_range("lake")
    for s in np.linspace(s0 + 20.0, s1 - 20.0, 30):
        i, j = world_index(model.route.point_at(s))
        assert top[i, j] > WATER_M + 0.2, f"calzada bajo el agua en s={s:.0f}"


def test_los_bordes_de_trozos_vecinos_coinciden(chunks):
    left, right = chunks[(2, 2)], chunks[(3, 2)]
    half = 5000.0
    a = left.vertices[np.isclose(left.vertices[:, 1], half, atol=1e-2)]
    b = right.vertices[np.isclose(right.vertices[:, 1], -half, atol=1e-2)]
    assert len(a) > 0 and len(a) == len(b)
    a_sorted = a[np.lexsort((a[:, 2], a[:, 0]))][:, [0, 2]]
    b_sorted = b[np.lexsort((b[:, 2], b[:, 0]))][:, [0, 2]]
    assert np.allclose(a_sorted, b_sorted, atol=0.1)


def test_el_binario_va_y_vuelve(chunks, tmp_path):
    chunk = chunks[(0, 0)]
    path = tmp_path / "c.bin"
    write_chunk(path, chunk)
    data = read_chunk(path)
    assert np.array_equal(data["triangles"], chunk.triangles)
    assert np.allclose(data["vertices"], chunk.vertices)
    path.write_bytes(path.read_bytes()[:-3])
    with pytest.raises(ValueError):
        read_chunk(path)


def test_las_caras_miran_hacia_su_normal(chunks):
    chunk = chunks[(1, 1)]
    v, t = chunk.vertices.astype(np.float64), chunk.triangles
    cross = np.cross(v[t[:, 1]] - v[t[:, 0]], v[t[:, 2]] - v[t[:, 0]])
    n = chunk.normals[t].mean(axis=1)
    # Cara visible de Unreal = -(B-A)x(C-A): debe apuntar como la normal.
    assert np.mean(np.einsum("ij,ij->i", -cross, n) > 0.0) > 0.999
