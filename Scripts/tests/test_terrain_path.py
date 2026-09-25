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


from terrain_path.layout import WATER_M  # noqa: E402
from terrain_path.profile import build_plan  # noqa: E402


@pytest.fixture(scope="module")
def plan():
    return build_plan(np.random.default_rng(60001), C01_STYLE)


def test_pendientes_suaves(plan):
    for line in plan.graph.lines:
        z = plan.profiles[line.id].z
        grade = np.abs(np.diff(z)) / np.maximum(np.diff(line.arc), 1e-9)
        assert grade.max() <= C01_STYLE.steep_grade + 0.02, f"camino {line.id}: pendiente {grade.max():.2f}"


def test_los_lazos_empalman_a_la_cota_de_su_padre(plan):
    for loop in plan.graph.loops():
        parent, pp = plan.graph.lines[loop.parent], plan.profiles[loop.parent]
        z = plan.profiles[loop.id].z
        assert abs(z[0] - np.interp(loop.s_out, parent.arc, pp.z)) < 0.05
        assert abs(z[-1] - np.interp(loop.s_back, parent.arc, pp.z)) < 0.05


def test_los_cruces_dejan_hueco_de_tunel(plan):
    for c in plan.crossings:
        up, lo = plan.graph.lines[c.upper], plan.graph.lines[c.lower]
        z_up = np.interp(c.s_upper, up.arc, plan.profiles[up.id].z)
        z_lo = np.interp(c.s_lower, lo.arc, plan.profiles[lo.id].z)
        assert z_up - z_lo >= C01_STYLE.cross_clearance_m - 0.1
        assert z_lo >= WATER_M + 0.8
        tunnel = plan.profiles[lo.id].tunnel
        assert tunnel[int(np.searchsorted(lo.arc, c.s_lower))]


def test_biomas_en_orden_y_final_en_la_playa(plan):
    b = plan.profiles[0].biome
    assert list(np.unique(b)) == [0, 1, 2, 3] and np.all(np.diff(b) >= 0)
    assert plan.profiles[0].z[-1] <= WATER_M + 0.6


def test_hay_tuneles_de_cerro_en_el_acantilado(plan):
    assert len(plan.hill_tunnels) >= 1
    for line_id, s0, s1 in plan.hill_tunnels:
        assert line_id == 0 and 25.0 <= s1 - s0 <= 45.0


from terrain_path.model import PathModel  # noqa: E402


@pytest.fixture(scope="module")
def model():
    return PathModel(60001, C01_STYLE)


def _cross_section(model, line_id, s, offsets):
    from scipy import ndimage
    line = model.plan.graph.lines[line_id]
    p, n = line.point_at(s), line.normal_at(s)
    pts = p[None, :] + offsets[:, None] * n[None, :]
    i = pts[:, 0] - model.axis[0]
    j = pts[:, 1] - model.axis[0]
    return ndimage.map_coordinates(model.grid.height, [i, j], order=1)


def test_el_suelo_del_camino_es_llano_sin_cuenco(model):
    plan = model.plan
    checked = 0
    for line in plan.graph.lines:
        prof = plan.profiles[line.id]
        for s in np.arange(30.0, line.length - 60.0, 9.0):
            k = int(np.searchsorted(line.arc, s))
            if prof.biome[k] != 0 and prof.biome[k] != 2:
                continue                                   # agua y playa tienen su propio suelo
            if prof.tunnel[max(k - 15, 0):k + 15].any() or model.near_junction(line.point_at(s), 20.0):
                continue
            w = prof.half_width[k]
            h = _cross_section(model, line.id, s, np.linspace(-0.8 * w, 0.8 * w, 9))
            assert np.ptp(h) <= 0.3, f"camino {line.id} s={s:.0f}: cuenco o escalon ({np.ptp(h):.2f} m)"
            checked += 1
    assert checked >= 20


def test_el_borde_cierra_el_paso(model):
    """A 1,5 x semiancho del eje, fuera del camino, la pared ya esta 3 m o mas por encima del suelo."""
    plan = model.plan
    for line in plan.graph.lines:
        prof = plan.profiles[line.id]
        for s in np.arange(30.0, line.length - 60.0, 23.0):
            k = int(np.searchsorted(line.arc, s))
            if prof.biome[k] == 3 or prof.tunnel[max(k - 15, 0):k + 15].any() \
                    or model.near_junction(line.point_at(s), 25.0):
                continue
            w = prof.half_width[k]
            h = _cross_section(model, line.id, s, np.array([0.0, -(w + 6.0), w + 6.0]))
            assert min(h[1], h[2]) - h[0] >= 3.0, f"camino {line.id} s={s:.0f}: borde bajo"


def test_la_union_de_un_lazo_no_tiene_escalon(model):
    """Donde un lazo se une a su padre, el suelo pasa de uno a otro sin salto."""
    for loop in model.plan.graph.loops():
        for s in (1.0, loop.length - 1.0):
            k = int(np.searchsorted(loop.arc, s))
            if model.plan.profiles[loop.id].biome[k] == 1:
                continue                           # en el rio el eje es cauce, no suelo
            h = _cross_section(model, loop.id, s, np.array([0.0]))[0]
            z = np.interp(s, loop.arc, model.plan.profiles[loop.id].z)
            assert abs(h - z) <= 0.6


def test_el_final_toca_el_mar(model):
    end = model.plan.graph.main.points[-1]
    i, j = int(round(end[0] - model.axis[0])), int(round(end[1] - model.axis[0]))
    patch = model.grid.height[i - 3:i + 25, j - 10:j + 11]
    assert (patch < WATER_M).any()


def test_las_islas_son_alargadas_y_siguen_el_rio(model):
    islands = model.river.islands
    assert len(islands) >= 6
    for isl in islands:
        assert isl.half_len / isl.half_wid >= 1.8


def test_no_hay_ristra_de_islas_en_el_centro(model):
    by_line = {}
    for isl in model.river.islands:
        by_line.setdefault(isl.line, []).append(isl)
    for line_id, isls in by_line.items():
        line = model.plan.graph.lines[line_id]
        prof = model.plan.profiles[line_id]
        run = best = 0
        for isl in sorted(isls, key=lambda a: a.s):
            w = np.interp(isl.s, line.arc, prof.half_width)
            run = run + 1 if abs(isl.q) < 0.25 * w else 0
            best = max(best, run)
        assert best <= 3


def test_el_rio_tiene_agua_honda_y_orilla_seca(model):
    main = model.plan.graph.main
    prof = model.plan.profiles[0]
    s_water = main.arc[prof.biome == 1]
    s = float(np.median(s_water))
    w = float(np.interp(s, main.arc, prof.half_width))
    h = _cross_section(model, 0, s, np.linspace(-0.9 * w, 0.9 * w, 19))
    assert h.min() < WATER_M - 0.5 and h.max() > WATER_M + 0.3


from terrain_vol.mesh import standable_cells, z_levels  # noqa: E402


def _column(model, p):
    """Cotas pisables de la columna en p, con la densidad 3D del modelo."""
    i, j = int(round(p[0] - model.axis[0])), int(round(p[1] - model.axis[0]))
    f = model.grid.window(i - 1, i + 2, j - 1, j + 2)
    X, Y = np.meshgrid(model.axis[i - 1:i + 2], model.axis[j - 1:j + 2], indexing="ij")
    D = model.density(X, Y, z_levels(), f)
    return z_levels()[np.nonzero(standable_cells(D)[1, 1])[0]]


def test_los_cruces_tienen_techo(model):
    assert model.plan.crossings
    for c in model.plan.crossings:
        levels = _column(model, c.point)
        lo = np.interp(c.s_lower, model.plan.graph.lines[c.lower].arc, model.plan.profiles[c.lower].z)
        hi = np.interp(c.s_upper, model.plan.graph.lines[c.upper].arc, model.plan.profiles[c.upper].z)
        assert np.any(np.abs(levels - lo) < 0.8), f"sin suelo de tunel en {c.point}: {levels}"
        assert np.any(np.abs(levels - hi) < 0.8), f"sin tablero en {c.point}: {levels}"


def test_los_tuneles_de_cerro_tienen_techo_y_suelo(model):
    main = model.plan.graph.main
    assert model.plan.hill_tunnels
    for _, s0, s1 in model.plan.hill_tunnels:
        s = 0.5 * (s0 + s1)
        levels = _column(model, main.point_at(s))
        z = np.interp(s, main.arc, model.plan.profiles[0].z)
        assert np.any(np.abs(levels - z) < 0.8) and levels.max() > z + 5.5
