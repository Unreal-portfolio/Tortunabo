"""Banco del mapa de Supervivencia (Scripts/terrain_survival).

    uv run pytest Scripts/tests/test_terrain_survival.py
"""

from __future__ import annotations

import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from terrain_survival import spec  # noqa: E402
from terrain_survival.adapters import referencia  # noqa: E402
from terrain_survival.bench import collect, markdown, summarize  # noqa: E402
from terrain_survival.mapa import SurvivalMap, expected_shape  # noqa: E402
from terrain_survival.metrics import difficulty_rank, evaluate, route, variety  # noqa: E402


def flat(height: float = 3.0) -> SurvivalMap:
    h, w = expected_shape()
    mid = h // 2
    return SurvivalMap(np.full((h, w), height), (mid, spec.START_MARGIN_M), (mid, w - 1 - spec.START_MARGIN_M), 1, 1,
                       "plano")


def test_la_especificacion_es_alargada():
    assert spec.LENGTH_M / spec.WIDTH_M >= spec.MIN_ASPECT
    assert spec.MAX_TRIANGLES == 180_000


def test_un_mapa_llano_es_valido_y_el_camino_es_la_recta():
    r = evaluate(flat())
    assert r["valid"] and r["reached"] and r["wide_path"]
    assert abs(r["route_ratio"] - 1.0) < 0.02


def test_fuera_del_camino_distingue_lineal_de_laberinto():
    corridor = flat(-10.0)
    mid = corridor.top.shape[0] // 2
    corridor.top[mid - 3:mid + 4, :] = 3.0                  # pasillo de 7 m: solo sobra lo que hay tras inicio y meta
    assert evaluate(corridor)["off_route_share"] < 0.05
    open_field = evaluate(flat())                            # campo abierto: casi todo queda lejos del camino
    assert open_field["off_route_share"] > 0.7


def test_un_muro_de_agua_corta_el_mapa():
    m = flat()
    m.top[:, 200:204] = -10.0
    r = evaluate(m)
    assert not r["reached"] and not r["valid"]
    assert route(m.top, m.top > -3.9, m.start, m.goal) is None


def test_un_paso_estrecho_no_vale_como_camino():
    m = flat()
    m.top[:, 200:204] = -10.0
    mid = m.top.shape[0] // 2
    m.top[mid, 200:204] = 3.0                      # pasarela de 1 m: se anda, pero no cabe el ancho mínimo
    r = evaluate(m)
    assert r["reached"] and not r["wide_path"] and not r["valid"]


def gap_map(leap_m: float) -> SurvivalMap:
    """Llano cortado por una zanja de 3 m (columnas 200-202) que se cruza con un salto de leap_m."""
    m = flat()
    m.top[:, 200:203] = -20.0
    mid = m.top.shape[0] // 2
    m.jumps = np.array([[mid, 198, mid, 204, leap_m]])
    return m


def test_un_hueco_de_salto_se_cruza_si_cabe_en_el_dive():
    r = evaluate(gap_map(3.0))
    assert r["reached"] and r["wide_path"] and r["valid"]
    assert r["jumps_on_route"] == 1
    sin_salto = evaluate(flat())
    assert r["challenge"] > sin_salto["challenge"]                 # cada salto suma reto


def test_un_hueco_mas_largo_que_el_dive_corta_el_mapa():
    r = evaluate(gap_map(spec.MAX_JUMP_M + 0.5))
    assert not r["reached"] and not r["valid"]


def test_formato_npz_guarda_los_huecos(tmp_path):
    m = gap_map(3.0)
    m.save(tmp_path / "1_1.npz")
    back = SurvivalMap.load(tmp_path / "1_1.npz")
    assert np.array_equal(back.jumps, m.jumps)


def test_forma_inicio_y_triangulos_incorrectos_se_detectan():
    m = flat()
    m.top = m.top[:, :-50]
    assert not evaluate(m)["shape_ok"]
    m = flat()
    m.start = (m.start[0], 100)
    assert not evaluate(m)["ends_ok"]
    m = flat()
    m.triangles = spec.MAX_TRIANGLES + 1
    assert not evaluate(m)["triangles_ok"]
    m = flat()
    m.gen_seconds = spec.MAX_GEN_SECONDS + 0.1
    assert not evaluate(m)["time_ok"]


def test_formato_npz_ida_y_vuelta(tmp_path):
    m = referencia(7, 3)
    m.save(tmp_path / "7_3.npz")
    back = SurvivalMap.load(tmp_path / "7_3.npz")
    assert back.start == m.start and back.goal == m.goal and (back.seed, back.difficulty) == (7, 3)
    assert np.allclose(back.top, m.top, atol=1e-4) and back.triangles is None


def test_variedad_y_rango_de_dificultad():
    a, b = referencia(1, 2).top, referencia(2, 2).top
    assert variety([a, a]) == 0.0 and variety([a, b]) > spec.MIN_VARIETY
    reports = [{"difficulty": d, "challenge": c} for d, c in ((1, 0.1), (2, 0.2), (3, 0.4), (4, 0.5))]
    assert difficulty_rank(reports) > 0.9
    assert difficulty_rank([{"difficulty": 1, "challenge": 0.2}]) is None


def test_la_referencia_pasa_el_banco_y_notas_la_dificultad(tmp_path):
    maps = collect("referencia", None, [1, 2, 3], [1, 3, 5])
    result = summarize(maps)
    assert result["summary"]["valid_share"] == 1.0
    assert result["summary"]["difficulty_ok"] and result["summary"]["variety_ok"]
    assert "## referencia" in markdown("referencia", result)
