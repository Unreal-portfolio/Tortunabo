"""Colisiones entre PR abiertas: pares que tocan los mismos ficheros.

Cada par se convierte en una issue «Mezclar PR #a y #b: ficheros en común» con las
instrucciones para que un Claude las mezcle. Los binarios de Unreal no se mezclan:
esa issue lleva `decision` para que un aprobador decida qué versión gana.
"""

from __future__ import annotations

import json
from collections.abc import Callable
from itertools import combinations

Gh = Callable[..., str]

ETIQUETA = "colision"
COLOR = "B60205"
DESCRIPCION_ETIQUETA = "Dos PR abiertas tocan los mismos ficheros: hay que mezclarlas (tablero.py colisiones)"
EXTENSIONES_BINARIAS = (".uasset", ".umap")


def pares(ficheros_por_pr: dict[int, set[str]]) -> list[tuple[int, int, list[str]]]:
    """Pares (a, b) con a < b que comparten ficheros, con la lista ordenada de los comunes."""
    resultado = []
    for a, b in combinations(sorted(ficheros_por_pr), 2):
        comunes = ficheros_por_pr[a] & ficheros_por_pr[b]
        if comunes:
            resultado.append((a, b, sorted(comunes)))
    return resultado


def titulo(a: int, b: int) -> str:
    primera, segunda = sorted((a, b))
    return f"Mezclar PR #{primera} y #{segunda}: ficheros en común"


def hay_binarios(ficheros: list[str]) -> bool:
    return any(f.lower().endswith(EXTENSIONES_BINARIAS) for f in ficheros)


def etiquetas(ficheros: list[str]) -> list[str]:
    return [ETIQUETA, "decision"] if hay_binarios(ficheros) else [ETIQUETA]


def cuerpo(antigua: dict, reciente: dict, ficheros: list[str], integracion: str) -> str:
    """Cuerpo de la issue: ficheros en común e instrucciones para mezclar (o para decidir si hay binarios)."""
    lista = "\n".join(f"- `{f}`" for f in ficheros)
    cabecera = (f"Las PR #{antigua['number']} (`{antigua['headRefName']}`) y #{reciente['number']} "
                f"(`{reciente['headRefName']}`) están abiertas contra `{integracion}` y tocan los mismos ficheros:"
                f"\n\n{lista}\n\n")
    if hay_binarios(ficheros):
        return cabecera + ("**No mezclar.** Hay `.uasset` o `.umap` en común: son binarios y no se fusionan. "
                           "SkiTemplar o Mokius deciden qué versión gana (etiqueta `decision`); la otra PR rehace "
                           "su cambio sobre la versión elegida.")
    return cabecera + (
        "Instrucciones para Claude:\n"
        f"1. Rebasa la PR más reciente, #{reciente['number']}, sobre la rama de #{antigua['number']} "
        f"(`{antigua['headRefName']}`), o sobre `{integracion}` si #{antigua['number']} ya está fusionada.\n"
        "2. Mezcla las dos versiones de cada fichero conservando el comportamiento de ambas PR; "
        "no descartes ninguno de los dos cambios.\n"
        "3. Compila (`TortunaboEditor` DebugGame si hay C++) y ejecuta los tests que afecten.\n"
        "4. Actualiza la PR rebasada y comenta aquí el resultado; cierra esta issue cuando las dos PR "
        "puedan fusionarse sin conflicto.")


def ficheros_de_pr(gh: Gh, repo: str, numero: int) -> set[str]:
    salida = gh("pr", "view", str(numero), "--repo", repo, "--json", "files")
    return {f["path"] for f in json.loads(salida).get("files") or []}


def colisiones_abiertas(gh: Gh, repo: str) -> set[str]:
    """Títulos de las issues `colision` abiertas, para no duplicar un par."""
    salida = gh("issue", "list", "--repo", repo, "--state", "open", "--label", ETIQUETA,
                "--limit", "200", "--json", "title")
    return {i["title"].strip() for i in json.loads(salida)}
