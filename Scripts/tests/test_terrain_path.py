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
    assert C01_STYLE.loops == 7 and C01_STYLE.nested_loops >= 1 and C01_STYLE.crossings == (1, 4)
    assert C01_STYLE.width_m[1] == 6.5 and C01_STYLE.canyon == "deadly"


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
        assert z_lo >= WATER_M + 0.6          # sobre el rio, el de abajo va a la cota de la orilla
        tunnel = plan.profiles[lo.id].tunnel          # solo los cruces de tipo tunel van cubiertos
        assert tunnel[int(np.searchsorted(lo.arc, c.s_lower))] == (c.kind == "tunnel")


def test_cruces_de_uno_a_cuatro_con_tipo(plan):
    assert 1 <= len(plan.crossings) <= 4
    assert all(c.kind in ("bridge", "tunnel") for c in plan.crossings)


def test_biomas_en_orden_y_final_en_la_playa(plan):
    b = plan.profiles[0].biome
    assert list(np.unique(b)) == [0, 1, 2, 3] and np.all(np.diff(b) >= 0)
    assert plan.profiles[0].z[-1] <= WATER_M + 0.6


def test_hay_tuneles_de_cerro_en_el_acantilado(plan):
    assert len(plan.hill_tunnels) >= 1
    for line_id, s0, s1 in plan.hill_tunnels:
        assert line_id == 0 and 25.0 <= s1 - s0 <= 45.0      # los del plan: en el principal


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
            if prof.tunnel[max(k - 15, 0):k + 15].any() or model.near_junction(line.point_at(s), 20.0)                     or _on_deck(model, line.id, s, 15.0):
                continue
            if any(np.hypot(*(line.point_at(s) - c.center)) < 14.0 for c in model.castles):
                continue                                   # el castillo esta dentro del camino a proposito
            w = prof.half_width[k]
            h = _cross_section(model, line.id, s, np.linspace(-0.8 * w, 0.8 * w, 9))
            assert np.ptp(h) <= 0.3, f"camino {line.id} s={s:.0f}: cuenco o escalon ({np.ptp(h):.2f} m)"
            checked += 1
    assert checked >= 12


def _on_deck(model, line_id, s, margin):
    """(line_id, s) esta en un puente (o a menos de margin m): alli el suelo es el tablero."""
    return any(lid == line_id and a - margin <= s <= b + margin for lid, a, b in model.deck_cuts)


def _other_path_near(model, line_id, p, radius):
    S = model.samples()
    near = np.hypot(S["p"][:, 0] - p[0], S["p"][:, 1] - p[1]) - S["w"] < radius
    return bool(np.any(near & (S["line"] != line_id)))


def test_el_borde_cierra_el_paso(model):
    """A 1,5 x semiancho del eje, fuera del camino, la pared ya esta 3 m o mas por encima del suelo."""
    plan = model.plan
    for line in plan.graph.lines:
        prof = plan.profiles[line.id]
        for s in np.arange(30.0, line.length - 60.0, 23.0):
            k = int(np.searchsorted(line.arc, s))
            if prof.biome[k] == 3 or prof.tunnel[max(k - 15, 0):k + 15].any() \
                    or model.near_junction(line.point_at(s), 25.0) or _on_deck(model, line.id, s, 15.0):
                continue
            w = prof.half_width[k]
            if _other_path_near(model, line.id, line.point_at(s), w + 8.0):
                continue                           # al otro lado hay otro camino, no las vistas
            # La pared mas alta entre el borde del camino y 6 m mas alla (en una curva cerrada, mas
            # alla de la pared vuelve a estar el mismo camino).
            h0 = _cross_section(model, line.id, s, np.array([0.0]))[0]
            side = [_cross_section(model, line.id, s, sign * np.arange(w, w + 6.5, 0.5)).max() for sign in (-1.0, 1.0)]
            assert min(side) - h0 >= 3.0, f"camino {line.id} s={s:.0f}: borde bajo"


def test_la_union_de_un_lazo_no_tiene_escalon(model):
    """Donde un lazo se une a su padre, el suelo pasa de uno a otro sin salto."""
    for loop in model.plan.graph.loops():
        for s in (1.0, loop.length - 1.0):
            k = int(np.searchsorted(loop.arc, s))
            if model.plan.profiles[loop.id].biome[k] == 1:
                continue                           # en el rio el eje es cauce, no suelo
            h = _cross_section(model, loop.id, s, np.array([0.0]))[0]
            z = np.interp(s, loop.arc, model.plan.profiles[loop.id].z)
            assert abs(h - z) <= 0.8


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


def test_los_cruces_de_tunel_tienen_techo(model):
    """Los puentes: test_hay_puentes_con_losa_gruesa_y_hueco_al_aire."""
    for c in [c for c in model.plan.crossings if c.kind == "tunnel"]:
        levels = _column(model, c.point)
        lo = np.interp(c.s_lower, model.plan.graph.lines[c.lower].arc, model.plan.profiles[c.lower].z)
        hi = np.interp(c.s_upper, model.plan.graph.lines[c.upper].arc, model.plan.profiles[c.upper].z)
        assert np.any(np.abs(levels - lo) < 0.8), f"sin suelo de tunel en {c.point}: {levels}"
        assert np.any(np.abs(levels - hi) < 0.8), f"sin tablero en {c.point}: {levels}"


def test_los_tuneles_de_cerro_tienen_techo_y_suelo(model):
    assert model.plan.hill_tunnels
    for line_id, s0, s1 in model.plan.hill_tunnels + model.arch_ranges:
        line = model.plan.graph.lines[line_id]
        s = 0.5 * (s0 + s1)
        levels = _column(model, line.point_at(s))
        z = np.interp(s, line.arc, model.plan.profiles[line_id].z)
        assert np.any(np.abs(levels - z) < 0.8) and levels.max() > z + 5.5


def _solid_column(model, p):
    """Cotas solidas (densidad > 0) de la columna en p."""
    i, j = int(round(p[0] - model.axis[0])), int(round(p[1] - model.axis[0]))
    f = model.grid.window(i, i + 1, j, j + 1)
    X, Y = np.meshgrid(model.axis[i:i + 1], model.axis[j:j + 1], indexing="ij")
    return z_levels()[model.density(X, Y, z_levels(), f)[0, 0] > 0.0]


def test_hay_puentes_con_losa_gruesa_y_hueco_al_aire(model):
    """Sobre el camino de abajo de cada puente: hueco libre de 5 m o mas, losa de 1,4 m o mas y
    tablero pisable a la cota del de arriba. A un lado del tablero, sobre el camino de abajo,
    cielo abierto (es un puente, no un tunel)."""
    bridges = [c for c in model.plan.crossings if c.kind == "bridge"]
    assert bridges
    for c in bridges:
        lo, up = model.plan.graph.lines[c.lower], model.plan.graph.lines[c.upper]
        z_lo = np.interp(c.s_lower, lo.arc, model.plan.profiles[c.lower].z)
        z_up = np.interp(c.s_upper, up.arc, model.plan.profiles[c.upper].z)
        solid = _solid_column(model, c.point)
        above = solid[solid > z_lo + 0.5]
        assert above.size and above.min() >= z_lo + 5.0, f"hueco bajo el puente en {c.point}: {above[:3]}"
        slab = above[above <= z_up + 0.5]
        assert slab.max() - slab.min() + 0.5 >= 1.4, f"losa fina en {c.point}"   # + un paso de Z
        assert np.any(np.abs(_column(model, c.point) - z_up) < 0.8), f"tablero no pisable en {c.point}"
        side = c.point + 6.0 * lo.tangent_at(c.s_lower)
        open_sky = _solid_column(model, side)
        assert not np.any(open_sky > z_lo + 0.5), f"el camino de abajo no esta al aire junto al puente {c.point}"


def test_los_arcos_cruzan_de_pared_a_pared(model):
    assert model.arch_ranges
    for line_id, s0, s1 in model.arch_ranges:
        line = model.plan.graph.lines[line_id]
        s = 0.5 * (s0 + s1)
        z = np.interp(s, line.arc, model.plan.profiles[line_id].z)
        for off in (-2.0, 0.0, 2.0):
            solid = _solid_column(model, line.point_at(s) + off * line.normal_at(s))
            above = solid[solid > z + 0.5]
            assert above.size and above.min() >= z + 4.5 and above.max() - above.min() + 0.5 >= 1.4


def test_castillos_segun_el_estilo(model):
    """C01 ya no lleva castillos (los ponen los disenadores); si un estilo los pide, aparecen."""
    assert len(model.castles) <= C01_STYLE.castles


def test_los_castillos_dejan_paso(model):
    """Del castillo hacia el centro del camino quedan 3 m o mas de suelo libre."""
    for c in model.castles:
        h = []
        for dist in np.arange(4.5, 12.0, 0.5):
            p = c.center + c.facing * dist
            i, j = int(round(p[0] - model.axis[0])), int(round(p[1] - model.axis[0]))
            h.append(model.grid.height[i, j] - c.base)
        free = np.array(h) < 0.3
        assert free[:6].all(), f"castillo en {c.center} tapona el camino"


from gen_terrain_volume import build_all, ground_level, walk, world_index  # noqa: E402


@pytest.fixture(scope="module")
def chunks(model):
    return build_all(model, grid=4)


@pytest.fixture(scope="module")
def reached(model, chunks):
    from terrain_path.model import walkable
    standable = walkable(global_standable(chunks, grid=4), model.grid.height[1:-1, 1:-1], z_levels())
    standable = model.remove_deadly(standable, z_levels())
    start = world_index(model.start)
    return standable, walk(standable, (*start, ground_level(standable, *start)))


def test_se_llega_a_pie_del_inicio_al_final(model, reached):
    _, seen = reached
    i, j = world_index(model.end)
    assert seen[i, j].any(), "no se llega al final andando o saltando"


def test_todos_los_lazos_se_recorren(model, reached):
    _, seen = reached
    for loop in model.plan.graph.loops():
        i, j = world_index(loop.point_at(loop.length / 2.0))
        assert seen[i - 2:i + 3, j - 2:j + 3].any(), f"lazo {loop.id} inalcanzable"


def test_las_vistas_no_se_pisan(model, reached):
    _, seen = reached
    region = model.region[1:-1, 1:-1]
    assert int((seen.any(axis=2) & (region == 2)).sum()) == 0


def _world_vertices(chunks):
    from terrain_vol.layout import UU_PER_M, cell_center
    out = []
    for (col, row), ch in chunks.items():
        cx, cy = cell_center(col, row)
        out.append(ch.vertices / UU_PER_M + np.array([cx, cy, 0.0]))
    return np.vstack(out)


def test_el_tablero_sobrevive_a_la_malla(model, chunks):
    """El suavizado de la malla no perfora los tableros: hay cara de arriba y de abajo sobre el
    camino de abajo de cada puente (antes, con losas de 0,8 m, desaparecian)."""
    v = _world_vertices(chunks)
    for c in [c for c in model.plan.crossings if c.kind == "bridge"]:
        up, lo = model.plan.graph.lines[c.upper], model.plan.graph.lines[c.lower]
        z_up = np.interp(c.s_upper, up.arc, model.plan.profiles[c.upper].z)
        z_lo = np.interp(c.s_lower, lo.arc, model.plan.profiles[c.lower].z)
        near = np.hypot(v[:, 0] - c.point[0], v[:, 1] - c.point[1]) < 1.5
        top = near & (np.abs(v[:, 2] - z_up) < 0.6)
        bottom = near & (v[:, 2] > z_lo + 4.0) & (v[:, 2] < z_up - 1.0)
        assert top.sum() >= 4 and bottom.sum() >= 4, f"tablero perforado en {c.point}: {top.sum()} / {bottom.sum()}"


def test_los_trozos_vecinos_coinciden(chunks):
    left, right = chunks[(1, 1)], chunks[(2, 1)]
    a = left.vertices[np.isclose(left.vertices[:, 1], 5000.0, atol=1e-2)]
    b = right.vertices[np.isclose(right.vertices[:, 1], -5000.0, atol=1e-2)]
    assert len(a) > 0 and len(a) == len(b)
    a = a[np.lexsort((a[:, 2], a[:, 0]))][:, [0, 2]]
    b = b[np.lexsort((b[:, 2], b[:, 0]))][:, [0, 2]]
    assert np.allclose(a, b, atol=0.1)


def _wall_profiles(model):
    """A cada lado de cada camino, cada 10 m: (altura maxima de la pared sobre el suelo, distancia del borde del
    camino a la que ya ha bajado 3 m de su maximo, anchura de la zona casi llana en lo alto). El
    perfil se corta donde el punto pasa a estar mas cerca de otro camino (pared compartida). Lejos
    de uniones, cruces, tuneles, agua y playa."""
    tops, ends, flats = [], [], []
    for line in model.plan.graph.lines:
        prof = model.plan.profiles[line.id]
        for s in np.arange(40.0, line.length - 60.0, 10.0):
            k = int(np.searchsorted(line.arc, s))
            p = line.point_at(s)
            if prof.biome[k] in (1, 3) or prof.tunnel[max(k - 20, 0):k + 20].any() or model.near_junction(p, 30.0):
                continue
            w, n = prof.half_width[k], line.normal_at(s)
            for side in (-1.0, 1.0):
                off = side * (w + np.arange(0.0, 32.0, 0.5))
                pts = p[None, :] + off[:, None] * n[None, :]
                _, idx = model._nearest(pts[:, 0], pts[:, 1])
                mine = model.S["line"][idx] == line.id
                stop = int(np.argmin(mine)) if not mine.all() else len(off)
                if stop < 12:
                    continue
                h = _cross_section(model, line.id, s, off[:stop])
                top = int(np.argmax(h))
                tops.append(h[top] - prof.z[k])
                flats.append(0.5 * int((np.abs(h - h[top]) < 0.3).sum()))
                below = np.nonzero(h[top:] < h[top] - 3.0)[0]
                if len(below):
                    ends.append(0.5 * (top + below[0]))
    return np.array(tops), np.array(ends), np.array(flats)


def test_la_cresta_no_es_meseta(model):
    tops, _, flats = _wall_profiles(model)
    assert len(tops) >= 30
    assert np.std(tops) >= 1.5, f"cresta demasiado uniforme (desviacion {np.std(tops):.2f} m)"
    assert np.median(flats) <= 5.0, f"cimas llanas: mediana de {np.median(flats):.1f} m casi a la misma cota"


def test_la_pared_acaba_a_distancias_distintas(model):
    _, ends, _ = _wall_profiles(model)
    assert len(ends) >= 15 and np.std(ends) >= 7.0, f"la pared acaba casi siempre a la misma distancia ({np.std(ends):.2f} m)"


def test_sin_picos_de_una_celda(model):
    """Ninguna celda sobresale (o se hunde) mas de 0,5 m respecto de todas sus vecinas: eso es un
    pico o una aleta en la malla. Las esquinas de pared (casi vertical) no cuentan: no son picos."""
    from scipy import ndimage
    h = model.grid.height
    ring = np.ones((3, 3), dtype=bool)
    ring[1, 1] = False
    peak = h - ndimage.maximum_filter(h, footprint=ring)
    pit = ndimage.minimum_filter(h, footprint=ring) - h
    assert int((peak > 0.5).sum()) == 0 and int((pit > 0.5).sum()) == 0


from terrain_path import canyon as canyon_mod  # noqa: E402


def test_hay_barranco_mortal_con_puente_del_principal(model):
    c = model.canyon
    assert c is not None and c.mode == "deadly"
    lo, hi = C01_STYLE.canyon_width_m
    assert 0.5 * lo * 0.8 <= c.half.min() and c.half.max() <= 0.5 * hi * 1.2 + 1e-9
    assert any(lid == 0 and a <= c.s_main <= b for lid, a, b in model.deck_cuts)
    main = model.plan.graph.main
    z = np.interp(c.s_main, main.arc, model.plan.profiles[0].z)
    depth = z - canyon_mod.FLOOR_M
    assert C01_STYLE.canyon_depth_m[0] - 0.1 <= depth <= C01_STYLE.canyon_depth_m[1] + 0.1


def test_el_barranco_solo_toca_caminos_por_sus_puentes(model):
    """Ningun camino abierto pasa por el cauce salvo en el tramo de su puente."""
    c = model.canyon
    field_ = model.canyon_field
    S = model.S
    idx = model.open_idx
    q, half, depth = field_.query(S["p"][idx, 0][:, None], S["p"][idx, 1][:, None])
    inside = ((q.ravel() < half.ravel() + 1.0) & (depth.ravel() > 0.2))
    for j in idx[inside]:
        assert _on_deck(model, int(S["line"][j]), float(S["s"][j]), 6.0), \
            f"el camino {S['line'][j]} cae al barranco en s={S['s'][j]:.0f}"


def test_el_fondo_del_barranco_tiene_agua(model):
    c = model.canyon
    mid = c.pts[len(c.pts) // 2]
    i, j = int(round(mid[0] - model.axis[0])), int(round(mid[1] - model.axis[0]))
    assert model.grid.height[i, j] < WATER_M - 0.5


def test_las_cajas_cubren_el_fondo_y_no_el_puente(model):
    c = model.canyon
    boxes = canyon_mod.kill_boxes_uu(c)
    assert boxes

    def inside_any(p, z):
        for b in boxes:
            cx, cy, cz = (v / 100.0 for v in b["center"])
            ex, ey, ez = (v / 100.0 for v in b["extent"])
            yaw = np.radians(b["yaw"])
            dx, dy = p[0] - cx, p[1] - cy
            lx, ly = dx * np.cos(yaw) + dy * np.sin(yaw), -dx * np.sin(yaw) + dy * np.cos(yaw)
            if abs(lx) <= ex and abs(ly) <= ey and abs(z - cz) <= ez:
                return True
        return False

    for k in range(0, len(c.pts), 7):
        if c.depth[k] < 0.9:
            continue
        t = c.pts[min(k + 1, len(c.pts) - 1)] - c.pts[max(k - 1, 0)]
        n = np.array([-t[1], t[0]]) / max(float(np.linalg.norm(t)), 1e-9)
        for off in (-0.8, 0.0, 0.8):
            p = c.pts[k] + off * c.half[k] * n
            assert inside_any(p, canyon_mod.FLOOR_M + 0.5), f"fondo sin caja en {p}"
    main = model.plan.graph.main
    top = np.interp(c.s_main, main.arc, model.plan.profiles[0].z)
    assert top - canyon_mod.KILL_TOP_M >= 4.0
