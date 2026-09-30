"""Circuitos de Rally con lazos y tuneles (terrain_shapes/lots_loops.py): N09-N12, N19 y N20.

Los tests de diseño construyen el modelo (sin voxelizar) y miden el eje, los huecos y los tableros; los del
manifest leen lo que ha escrito gen_terrain_inventados.py y se saltan si el mapa aun no se ha generado.

    uv run --with numpy --with scipy --with pillow --with scikit-image --with matplotlib --with pytest \
        python -m pytest Scripts/tests/test_terrain_loops.py -q
"""

from __future__ import annotations

import json
import math
import sys
from pathlib import Path

import numpy as np
import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from terrain_shapes import lots_loops as L  # noqa: E402
from terrain_shapes.kit import Axis, Deck, Tunnel, arc_length, min_radius  # noqa: E402
from terrain_shapes.rally_circuit import DECK_THICKNESS_M, UNDERPASS_CLEAR_M, find_crossings  # noqa: E402
from terrain_vol.layout import WATER_M  # noqa: E402

VARIANTS = Path(__file__).resolve().parents[1] / "terrain_volumes" / "Variants"
TUNNEL_GAUGE_M = 6.0                                     # galibo de los tuneles de build_rally
_BUILT: dict = {}


def built(key: str):
    """Modelo de cada circuito, construido una sola vez por sesion de tests."""
    if key not in _BUILT:
        _BUILT[key] = L.MAPS[key][1](None)
    return _BUILT[key]


def names(items) -> list[str]:
    return sorted(i.name for i in items)


def ground(shape, e: float, n: float) -> float:
    """Cota del campo de alturas sobre el agua."""
    X, Y = shape.canvas.to_world(e, n)
    return float(shape.model.ground_height(np.array([X]), np.array([Y]))[0]) - WATER_M


def lap_m(extras) -> float:
    pts = extras.road.pts
    return float(arc_length(pts)[-1] + np.hypot(*(pts[0] - pts[-1])))


def crossing_pairs(extras) -> list[tuple[int, int]]:
    pts = extras.road.pts
    arc = arc_length(pts)
    return find_crossings(pts, arc, lap_m(extras))


# ── Reglas de calzada (sobre el eje, sin terreno) ────────────────────────────────
@pytest.mark.parametrize("make", [L.canyon_spec, L.fjord_spec, L.hollow_spec, L.delta_spec, L.corkscrew_spec,
                                  L.trefoil_spec])
def test_every_axis_keeps_radius_and_fits_the_map(make):
    spec = make()
    pts, _, total = L.lap_axis(spec.control)
    assert min_radius(pts, True)[0] >= 25.0
    assert np.abs(pts).max() <= 260.0 and spec.grid == 6 and spec.mode == "rally"
    assert 900.0 <= total <= 2100.0 and spec.auto_tunnel_m in (0.0, 14.0)


def test_seed_changes_the_relief_but_not_the_road():
    a, b = L.canyon_spec(), L.canyon_spec(7)
    assert a.control == b.control and a.seed != b.seed
    shape_a, _ = built("N09")
    shape_b, _ = L.build_canyon(7)
    assert not np.allclose(shape_a.model.height, shape_b.model.height)


# ── N09 Cañon serpiente ──────────────────────────────────────────────────────────
def test_canyon_has_two_tunnels_cutting_the_meanders():
    shape, extras = built("N09")
    tunnels = [v for v in shape.model.voids if isinstance(v, Tunnel)]
    assert len(tunnels) == 2 and not shape.model.solids
    assert 900.0 <= lap_m(extras) <= 1100.0
    for tunnel in tunnels:
        mid = tunnel.axis.array[len(tunnel.axis.array) // 2]
        peak = min(L.CANYON_MEANDERS, key=lambda p: abs(float(L._angle_off(math.degrees(math.atan2(mid[1], mid[0])), p))))
        assert abs(float(L._angle_off(math.degrees(math.atan2(mid[1], mid[0])), peak))) < 10.0
        assert tunnel.axis.length >= 70.0
        tip = np.hypot(*L.canyon_meander(peak).T).max()
        assert tip - np.hypot(*mid) >= 80.0                          # el meandro da un rodeo de 80 m o mas
        assert ground(shape, *mid) - (tunnel.axis.zs.max() - WATER_M) >= 14.0    # 8 m de roca sobre el galibo


# ── N10 Fiordo de los puentes ────────────────────────────────────────────────────
def test_fjord_is_jumped_four_times_on_viaducts_over_water():
    shape, extras = built("N10")
    decks = [s for s in shape.model.solids if s.name == "viaducto"]
    assert len(decks) == 4 and len(shape.model.voids) == 1
    for deck in decks:
        mid = deck.axis.array[len(deck.axis.array) // 2]
        assert deck.axis.length >= 60.0 and ground(shape, *mid) < 0.0
        assert deck.axis.zs.min() - WATER_M >= 9.5                   # viaducto alto sobre el fiordo
    assert 1900.0 <= lap_m(extras) <= 2100.0


# ── N11 Montaña hueca ────────────────────────────────────────────────────────────
def test_hollow_mountain_has_two_tunnels_and_a_skylit_hall():
    shape, extras = built("N11")
    tunnels = [v for v in shape.model.voids if isinstance(v, Tunnel)]
    hall = next(v for v in shape.model.voids if isinstance(v, L.Hall))
    assert len(tunnels) == 2 and [c.name for c in extras.clearances] == ["tunel", "tunel", "sala"]
    assert hall.floor_z - WATER_M == pytest.approx(L.HOLLOW_FLOOR_M, abs=0.1)
    X, Y = shape.canvas.to_world(np.array([0.0, 30.0]), np.array([0.0, 0.0]))
    Z = np.arange(hall.floor_z + 0.5, WATER_M + 55.0, 0.5)
    D = shape.model.density(np.asarray(X)[None, :], np.asarray(Y)[None, :], Z)[0]
    assert (D[0] < 0.0).all()                                        # lucernario: aire hasta el cielo
    roof = Z > hall.floor_z + L.HALL_H
    assert (D[1][roof] > 0.0).sum() * 0.5 >= 8.0                     # a 30 m del centro, roca sobre la boveda


# ── N12 Delta de los islotes ─────────────────────────────────────────────────────
def test_delta_links_nine_islets_with_nine_bridges_and_one_rock_tunnel():
    shape, _ = built("N12")
    decks = [s for s in shape.model.solids if s.name == "viaducto"]
    assert len(decks) == L.DELTA_ISLETS and len(shape.model.voids) == 1
    for deck in decks:
        mid = deck.axis.array[len(deck.axis.array) // 2]
        assert ground(shape, *mid) < 0.0 and deck.axis.length >= 45.0
    assert len(shape.unreachable_ok) == len(L.DELTA_BARS)
    assert all(ground(shape, *bar) > 0.5 for bar in L.DELTA_BARS)
    tunnel = shape.model.voids[0].axis.array
    assert np.hypot(*tunnel[len(tunnel) // 2]) > 200.0 and tunnel[len(tunnel) // 2][1] > 200.0   # islote del vertice


# ── N19 Sima del sacacorchos ─────────────────────────────────────────────────────
def test_corkscrew_turns_one_and_a_half_times_and_leaves_under_its_own_ramp():
    shape, extras = built("N19")
    road = extras.road
    end = int(np.argmin(np.hypot(*(road.pts - np.asarray(L.corkscrew_spec().control[L.CORK_STEPS])).T)))
    rel = road.pts[:end + 1] - np.asarray(L.CORK_CENTRE)
    turn = np.unwrap(np.arctan2(rel[:, 1], rel[:, 0]))
    assert math.degrees(turn[-1] - turn[0]) >= 530.0
    (a, b), = crossing_pairs(extras)
    hi, lo = (a, b) if road.z[a] > road.z[b] else (b, a)
    assert road.z[hi] - road.z[lo] >= TUNNEL_GAUGE_M + 8.0           # galibo y 8 m de roca bajo la rampa
    assert road.covered[lo] and lo > end                              # el tramo bajo es la salida, en tunel
    assert names(shape.model.solids) == ["paso_elevado"] and len(shape.model.voids) == 1
    under = next(c for c in extras.clearances if c.name == "paso_inferior")
    assert under.min_cover_m == 8.0 and under.width_m == 12.0


def test_corkscrew_overpass_rests_on_cleared_ground():
    shape, _ = built("N19")
    deck = shape.model.solids[0]
    pts, zs = deck.axis.array, deck.axis.zs
    for k in range(5, len(pts) - 5, 5):
        tangent = pts[k + 1] - pts[k - 1]
        side = np.array([-tangent[1], tangent[0]]) / np.hypot(*tangent)
        for off in (-6.5, 0.0, 6.5):
            assert ground(shape, *(pts[k] + off * side)) <= zs[k] - WATER_M - 0.25


# ── N20 Nudo de trebol ───────────────────────────────────────────────────────────
def test_trefoil_crosses_itself_three_times_alternating_over_and_under():
    shape, extras = built("N20")
    road = extras.road
    pairs = crossing_pairs(extras)
    assert len(pairs) == 3 and shape.params["crossings"] == 3
    passes = sorted((k, road.z[k] > road.z[j]) for a, b in pairs for k, j in ((a, b), (b, a)))
    assert [over for _, over in passes] in ([True, False] * 3, [False, True] * 3)
    for a, b in pairs:
        assert abs(road.z[a] - road.z[b]) >= UNDERPASS_CLEAR_M + DECK_THICKNESS_M + 0.5
    assert names(shape.model.solids) == ["paso_elevado"] * 3 + ["viaducto"]
    assert names(shape.model.voids) == ["tunel", "tunel"]
    assert [c.name for c in extras.clearances].count("paso_inferior") == 3


# ── Tableros a escuadra ──────────────────────────────────────────────────────────
def test_flush_deck_ends_one_metre_past_its_last_sample():
    xs = np.arange(0.0, 30.0)
    axis = Axis.of(np.column_stack([xs, np.zeros_like(xs)]), np.full(len(xs), 5.0))
    e, n, Z = np.array([[31.5, 29.5]]), np.array([[0.0, 0.0]]), np.array([4.5])
    round_end = Deck(axis, 14.0, 1.6).density(e, n, Z)[0, :, 0]
    flush_end = L.FlushDeck(axis, 14.0, 1.6).density(e, n, Z)[0, :, 0]
    assert round_end[0] > 0.0 and flush_end[0] < 0.0                  # sin el casquete que dejaba escalon
    assert flush_end[1] > 0.0


# ── Lo generado (manifest) ───────────────────────────────────────────────────────
GENERATED = {"N09_canon_serpiente": (2, 0, 0), "N10_fiordo_puentes": (1, 4, 0), "N11_montana_hueca": (2, 0, 0),
             "N12_delta_islotes": (1, 9, 0), "N19_sima_sacacorchos": (1, 1, 1), "N20_nudo_trebol": (2, 4, 3)}


@pytest.mark.parametrize("name", sorted(GENERATED))
def test_generated_circuit_is_valid(name):
    path = VARIANTS / name / "manifest.json"
    if not path.exists():
        pytest.skip(f"{name} sin generar")
    data = json.loads(path.read_text(encoding="utf-8"))
    tunnels, decks, crossings = GENERATED[name]
    checks = data["checks"]
    assert data["recorrible"] and data["mode"] == "rally"
    assert len(data["tunnels"]) == tunnels and len(data["decks"]) == decks
    assert data["generator"]["crossings"] == crossings
    road = checks["road"]
    assert road["min_radius_m"] >= 25.0 and road["max_grade_deg"] <= 12.0
    assert road["narrow_samples"] == 0 and road["too_close_pairs"] == 0
    assert all(c["ok"] for c in checks["clearances"]) and checks["taludes"]["ok"]
    assert sum(c["triangles"] for c in data["cells"]) <= 1_200_000
    assert len(data["checkpoints_uu"]) >= road["length_m"] // 200.0                  # un punto de control cada 200 m
    under = [c for c in checks["clearances"] if c["name"] == "paso_inferior"]
    assert len(under) == crossings and all(c["ok"] for c in under)
    if name == "N11_montana_hueca":
        assert [c["name"] for c in checks["clearances"]] == ["tunel", "tunel", "sala"]
