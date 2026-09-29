"""Tests de las funciones puras de Scripts/tablero/tablero.py (sin red ni gh)."""

import importlib.util
from pathlib import Path

RUTA = Path(__file__).resolve().parents[1] / "tablero" / "tablero.py"
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
