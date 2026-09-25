"""Terreno «camino primero» (Docs/Diseno_Terreno_CaminoPrimero.md)."""

from __future__ import annotations

import sys
from pathlib import Path
from types import SimpleNamespace

import numpy as np
import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from gen_terrain_volume import global_standable  # noqa: E402
from terrain_vol.export import global_top  # noqa: E402
from terrain_vol.layout import CELL_SAMPLES, Z_SAMPLES  # noqa: E402


def test_las_utilidades_aceptan_un_mapa_de_4x4():
    chunks = {(c, r): SimpleNamespace(standable=np.ones((CELL_SAMPLES, CELL_SAMPLES, Z_SAMPLES), bool),
                                      top=np.full((CELL_SAMPLES, CELL_SAMPLES), float(c + r)))
              for r in range(4) for c in range(4)}
    size = 4 * (CELL_SAMPLES - 1) + 1
    assert global_standable(chunks, grid=4).shape == (size, size, Z_SAMPLES)
    top = global_top(chunks, grid=4)
    assert top.shape == (size, size) and top[-1, -1] == 6.0


from terrain_path.curves import knot_noise, longest_straight, resample  # noqa: E402
from terrain_path.layout import GRID, MAP_MAX_M, MAP_MIN_M  # noqa: E402
from terrain_path.style import C01_STYLE  # noqa: E402


def test_el_mapa_mide_400_metros():
    assert GRID == 4 and MAP_MAX_M - MAP_MIN_M == 400.0


def test_remuestreo_a_paso_fijo():
    pts, arc = resample(np.array([[0.0, 0.0], [10.0, 0.0], [10.0, 5.0]]), 1.0)
    assert np.isclose(arc[-1], 15.0) and np.allclose(np.diff(arc), 1.0)


def test_detecta_rectas_largas():
    s = np.arange(0.0, 60.0, 1.0)
    recta = np.stack([s, np.zeros_like(s)], axis=1)
    curva = np.stack([30.0 * np.cos(s / 30.0), 30.0 * np.sin(s / 30.0)], axis=1)
    assert longest_straight(recta) >= 50.0
    assert longest_straight(curva) == 0.0


def test_ruido_por_nudos_en_rango():
    rng = np.random.default_rng(1)
    arc = np.arange(0.0, 300.0, 1.0)
    v = knot_noise(rng, arc, (20.0, 60.0), 2.5, 8.0, mode=4.0)
    assert v.min() >= 2.5 - 1e-9 and v.max() <= 8.0 + 1e-9 and np.ptp(v) > 1.0


def test_estilo_c01():
    assert C01_STYLE.loops == 7 and C01_STYLE.nested_loops >= 1 and C01_STYLE.crossings >= 1


from terrain_path.graph import EDGE_MARGIN_M, trace_main  # noqa: E402


@pytest.fixture(scope="module")
def main_line():
    return trace_main(np.random.default_rng(60001), C01_STYLE)


def test_el_principal_va_del_sur_al_mar(main_line):
    lo, hi = C01_STYLE.main_length_m
    assert lo <= main_line.length <= hi
    assert main_line.points[0][0] < MAP_MIN_M + 40.0 and main_line.points[-1][0] > MAP_MAX_M - 40.0


def test_el_principal_no_tiene_rectas_ni_giros_bruscos(main_line):
    assert longest_straight(main_line.points) <= 25.0
    t = np.gradient(main_line.points, axis=0)
    h = np.unwrap(np.arctan2(t[:, 1], t[:, 0]))
    assert np.max(np.abs(h[10:] - h[:-10])) <= np.radians(55.0)


def test_el_principal_no_se_acerca_a_si_mismo_ni_al_borde(main_line):
    from scipy.spatial import cKDTree
    pts, arc = main_line.points, main_line.arc
    for i, j in cKDTree(pts).query_pairs(C01_STYLE.path_separation_m):
        assert abs(arc[i] - arc[j]) <= 70.0
    inner = pts[:-5]
    assert inner.min() >= MAP_MIN_M + EDGE_MARGIN_M - 1.0 and inner.max() <= MAP_MAX_M - EDGE_MARGIN_M + 1.0


from terrain_path.graph import build_graph  # noqa: E402


@pytest.fixture(scope="module")
def graph():
    return build_graph(np.random.default_rng(60001), C01_STYLE)


def test_hay_lazos_anidados_y_cruces(graph):
    loops = graph.loops()
    assert len(loops) >= C01_STYLE.loops - 1
    assert any(l.parent not in (None, 0) for l in loops), "ningun lazo cuelga de otro lazo"
    assert len(graph.crossings) >= 1


def test_cada_lazo_sale_y_vuelve_a_su_padre(graph):
    for loop in graph.loops():
        parent = graph.lines[loop.parent]
        assert np.hypot(*(loop.points[0] - parent.point_at(loop.s_out))) < 1.5
        assert np.hypot(*(loop.points[-1] - parent.point_at(loop.s_back))) < 1e-6
        assert loop.s_out < loop.s_back


def test_los_lazos_no_se_tocan_salvo_en_sus_uniones_y_cruces(graph):
    from scipy.spatial import cKDTree
    crossing_pts = [c.point for c in graph.crossings]
    for a in graph.lines:
        for b in graph.lines:
            if a.id >= b.id:
                continue
            d, _ = cKDTree(b.points).query(a.points)
            close = a.points[d < 6.0]
            for p in close:
                near_join = any(np.hypot(*(p - q)) < 20.0 for line in (a, b) if line.parent is not None
                                for q in (line.points[0], line.points[-1]))
                near_cross = any(np.hypot(*(p - c)) < 20.0 for c in crossing_pts)
                assert near_join or near_cross, f"caminos {a.id} y {b.id} se tocan en {p}"


def test_los_cruces_son_francos_y_lejos_de_los_extremos(graph):
    for c in graph.crossings:
        up, lo = graph.lines[c.upper], graph.lines[c.lower]
        cos = abs(float(np.dot(up.tangent_at(c.s_upper), lo.tangent_at(c.s_lower))))
        assert cos <= np.cos(np.radians(40.0))
        loop = up if up.parent is not None and up.parent == lo.id else lo
        s = c.s_upper if loop is up else c.s_lower
        assert s >= 75.0 and loop.length - s >= 75.0
