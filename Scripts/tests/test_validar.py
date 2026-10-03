"""Pruebas de la comprobación de textos que no se pueden traducir de .github/scripts/validar.py (#232)."""
import importlib.util
import json
import os

import pytest

RAIZ = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
_spec = importlib.util.spec_from_file_location("validar", os.path.join(RAIZ, ".github", "scripts", "validar.py"))
validar = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(validar)


@pytest.mark.parametrize("linea, esperado", [
    ('Label = FText::FromString(TEXT("Hola"));', ["Hola"]),
    ('return FText::FromName(TEXT("Tortuga"));', ["Tortuga"]),
    ('X = FText::FromString(GIsEditor ? TEXT("Tab") : TEXT("Esc"));', ["Tab", "Esc"]),
    ('T = FText::FromString(FString::Printf(TEXT("Ronda %d"), N));', ["Ronda %d"]),
    ('T = FText::FromString("Mayús izq.");', ["Mayús izq."]),
    ('T = FText::FromString(FString::Printf(TEXT("%.1f s"), Secs));', ["%.1f s"]),
])
def test_detecta_literales_con_letras(linea, esperado):
    assert validar.untranslatable_literals(linea) == esperado


@pytest.mark.parametrize("linea", [
    'Label = INVTEXT("Esc");',
    'Label = FText::AsCultureInvariant(TEXT("W A S D"));',
    'Label = FText::FromString(Variable);',
    'Label = FText::FromString(FString::Printf(TEXT("%d/%d"), A, B));',
    'Label = FText::FromString(FString::Join(Parts, TEXT(" ")));',
    'Label = NSLOCTEXT("TNHUD", "Hola", "Hola");',
    '// FText::FromString(TEXT("comentario"))',
    'Url = TEXT("http://x"); // FText::FromString(TEXT("Hola"))',
])
def test_no_detecta_lo_que_no_es_texto_suelto(linea):
    assert validar.untranslatable_literals(linea) == []


def test_comentario_respeta_comillas():
    assert validar.strip_line_comment('A = TEXT("http://x"); // nota') == 'A = TEXT("http://x"); '


def test_lineas_anadidas_del_diff():
    diff = "\n".join([
        "diff --git a/Source/A.cpp b/Source/A.cpp",
        "--- a/Source/A.cpp",
        "+++ b/Source/A.cpp",
        "@@ -10,0 +11,2 @@ void F()",
        '+\tX = FText::FromString(TEXT("Hola"));',
        "+\tY = 1;",
        "diff --git a/Source/B.h b/Source/B.h",
        "--- a/Source/B.h",
        "+++ /dev/null",
        "@@ -1 +0,0 @@",
        "-borrada",
    ])
    assert validar.parse_added_lines(diff) == [
        ("Source/A.cpp", 11, '\tX = FText::FromString(TEXT("Hola"));'),
        ("Source/A.cpp", 12, "\tY = 1;"),
    ]


def test_claves_sin_recoger(tmp_path):
    manifiesto = {
        "FormatVersion": 1, "Namespace": "", "Children": [],
        "Subnamespaces": [{"Namespace": "TNHUD", "Children": [
            {"Source": {"Text": "Hola"}, "Keys": [{"Key": "Hola", "Path": "x"}]},
            {"Source": {"Text": "Di \"sí\""}, "Keys": [{"Key": "Comillas", "Path": "x"}]},
        ]}],
    }
    ruta = tmp_path / "Game.manifest"
    ruta.write_bytes(b"\xff\xfe" + json.dumps(manifiesto).encode("utf-16-le"))
    claves = validar.load_manifest_keys(str(ruta))
    assert claves[("TNHUD", "Hola")] == "Hola"
    lineas = [
        ("Source/A.cpp", 1, 'A = NSLOCTEXT("TNHUD", "Hola", "Hola");'),
        ("Source/A.cpp", 2, 'B = NSLOCTEXT("TNHUD", "Comillas", "Di \\"sí\\"");'),
        ("Source/A.cpp", 3, 'C = NSLOCTEXT("TNHUD", "Nueva", "Nueva");'),
        ("Source/B.cpp", 4, 'D = NSLOCTEXT("TNHUD", "Hola", "Hola otra vez");'),
    ]
    assert validar.ungathered_keys(lineas, claves) == {"Source/A.cpp": ["TNHUD,Nueva"], "Source/B.cpp": ["TNHUD,Hola"]}


def test_el_codigo_actual_no_tiene_textos_sin_traducir():
    malos = [(p, n, lit) for p, n, t in validar.all_source_lines() if not validar.is_test_source(p)
             for lit in validar.untranslatable_literals(t)]
    assert malos == []
