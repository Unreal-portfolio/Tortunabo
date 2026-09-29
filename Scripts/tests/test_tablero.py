"""Tests de las funciones puras de Scripts/tablero/tablero.py (sin red ni gh)."""

import importlib.util
import sys
from pathlib import Path

import pytest

CARPETA = Path(__file__).resolve().parents[1] / "tablero"
sys.path.insert(0, str(CARPETA))  # tablero.py importa objetos.py de su misma carpeta
import objetos  # noqa: E402

RUTA = CARPETA / "tablero.py"
spec = importlib.util.spec_from_file_location("tablero", RUTA)
assert spec is not None and spec.loader is not None
tablero = importlib.util.module_from_spec(spec)
spec.loader.exec_module(tablero)


def test_slug_quita_tildes_y_simbolos():
    assert tablero.slug("Puente tambaleante: Excitation en uint8 (10 Hz)") == "puente-tambaleante-excitation-en-uint8-1"
    assert tablero.slug("Validar el salto de la tortuga en PIE") == "validar-el-salto-de-la-tortuga-en-pie"


def test_slug_no_termina_en_guion_al_cortar():
    assert not tablero.slug("a" * 39 + " bbbb").endswith("-")


def test_issues_de_pr_lee_cuerpo_y_rama():
    pr = {"body": "Arregla el puente.\n\nCloses #19\nRefs #33", "headRefName": "fix/19-puente-tambaleante"}
    assert tablero.issues_de_pr(pr) == {19, 33}


def test_issues_de_pr_sin_referencias():
    assert tablero.issues_de_pr({"body": "Mejora de rendimiento #sinissue", "headRefName": "nube/optim-2026-09-29"}) == set()


def test_orden_prioridad_antes_que_tamano():
    alta_grande = {"number": 5, "valores": {"Prioridad": "P0", "Tamaño": "L"}}
    baja_pequena = {"number": 1, "valores": {"Prioridad": "P2", "Tamaño": "XS"}}
    sin_campos = {"number": 2, "valores": {}}
    orden = sorted([baja_pequena, sin_campos, alta_grande], key=tablero.clave_orden)
    assert [i["number"] for i in orden] == [5, 1, 2]


def _proyecto_con_revisiones(*revisores):
    items = {n: {"valores": {"Status": "In review", "Revisor": r}} for n, r in enumerate(revisores)}
    return {"items": items}


def test_revisor_cruzado_nunca_el_autor():
    proyecto = _proyecto_con_revisiones()
    assert tablero.elegir_revisor(proyecto, "SkiTemplar") == "Mokius"
    assert tablero.elegir_revisor(proyecto, "Mokius") == "SkiTemplar"


def test_revisor_de_ruby_reparte_carga():
    assert tablero.elegir_revisor(_proyecto_con_revisiones(), "Ruben-Besteiro") == "Mokius"
    assert tablero.elegir_revisor(_proyecto_con_revisiones("Mokius"), "Ruben-Besteiro") == "SkiTemplar"


def test_es_objeto_con_etiquetas_de_gh_y_del_proyecto():
    assert objetos.es_objeto({"labels": [{"name": "objeto"}]})
    assert objetos.es_objeto({"labels": {"nodes": [{"name": "tarea"}, {"name": "objeto"}]}})
    assert not objetos.es_objeto({"labels": {"nodes": [{"name": "tarea"}]}})
    assert not objetos.es_objeto({})


def test_buscar_por_titulo_exacto_y_solo_abiertas():
    issues = [
        {"number": 3, "title": "Rally Tortuga", "state": "CLOSED"},
        {"number": 7, "title": "Rally Tortuga · Red", "state": "OPEN"},
        {"number": 9, "title": " Rally Tortuga ", "state": "OPEN"},
    ]
    assert objetos.buscar_por_titulo(issues, "Rally Tortuga") == 9
    assert objetos.buscar_por_titulo(issues, "rally tortuga") is None
    assert objetos.buscar_por_titulo([], "Rally Tortuga") is None


def test_comprobar_padre_idempotente_y_sin_robar_hijos():
    assert objetos.comprobar_padre(20, 90, None) is True
    assert objetos.comprobar_padre(20, 90, 90) is False
    with pytest.raises(objetos.ErrorObjeto, match="ya cuelga de #40"):
        objetos.comprobar_padre(20, 90, 40)
    with pytest.raises(objetos.ErrorObjeto):
        objetos.comprobar_padre(90, 90, None)


def test_cuerpo_objeto_usa_la_descripcion_o_el_nombre():
    assert objetos.cuerpo_objeto("HUD y menús", "HUD, tutorial y ajustes.").startswith("HUD, tutorial y ajustes.")
    assert objetos.cuerpo_objeto("HUD y menús", None).startswith("Objeto «HUD y menús».")
    assert "vista «Objetos»" in objetos.cuerpo_objeto("X", "")




def _con_etiquetas(numero, *etiquetas, quien="Ruben-Besteiro"):
    return {"number": numero, "labels": {"nodes": [{"name": e} for e in etiquetas]},
            "assignees": {"nodes": [{"login": quien}]}, "valores": {}}


def test_colisiones_y_organizacion_van_primero():
    issues = [_con_etiquetas(1, "colision", quien="Mokius"), _con_etiquetas(2, "revisar-organizacion"),
              _con_etiquetas(3, "revisar-organizacion", quien="Mokius"), _con_etiquetas(4, "tarea")]
    assert [i["number"] for i in tablero.urgentes_de_organizacion(issues, "Ruben-Besteiro", False)] == [1, 2]
    assert [i["number"] for i in tablero.urgentes_de_organizacion(issues, "SkiTemplar", True)] == [1, 2, 3]
