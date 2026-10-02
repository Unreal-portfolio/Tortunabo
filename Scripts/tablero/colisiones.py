"""Colisiones entre PR abiertas: pares cuyos cambios chocan al mezclarlos.

Dos PR que tocan el mismo fichero en sitios distintos se fusionan solas; solo cuenta un par si
`git merge-tree` de sus dos cabezas da conflicto. Si git no puede comprobarlo (sin red, sin las
cabezas), se vuelve a lo prudente: los ficheros en común.

Cada par se convierte en una issue «Mezclar PR #a y #b: ficheros en común» con las instrucciones
para que un Claude las mezcle, y se cierra sola cuando el par deja de chocar o una de las dos PR
se fusiona o se cierra. Los binarios de Unreal no se mezclan: esa issue lleva `decision` para que
un aprobador decida qué versión gana. La localización generada no se mezcla a mano: se regenera.
"""

from __future__ import annotations

import json
import re
import subprocess
from collections.abc import Callable, Iterable
from itertools import combinations

Gh = Callable[..., str]
Conflicto = Callable[[int, int], "list[str] | None"]

ETIQUETA = "colision"
COLOR = "B60205"
DESCRIPCION_ETIQUETA = "Dos PR abiertas chocan al mezclarse: hay que mezclarlas (tablero.py colisiones)"
EXTENSIONES_BINARIAS = (".uasset", ".umap")
LOCALIZACION = "Content/Localization/"
REF_PR = "refs/remotes/tablero-pr/{}"
TITULO = re.compile(r"^Mezclar PR #(\d+) y #(\d+):")


def pares(ficheros_por_pr: dict[int, set[str]],
          conflicto: Conflicto | None = None) -> list[tuple[int, int, list[str]]]:
    """Pares (a, b) con a < b que chocan, con la lista ordenada de los ficheros en conflicto.

    Sin `conflicto`, cuenta todo fichero en común. Con él, solo los pares en los que devuelve
    ficheros; si devuelve None (no se pudo comprobar), cuenta los ficheros en común.
    """
    resultado = []
    for a, b in combinations(sorted(ficheros_por_pr), 2):
        comunes = ficheros_por_pr[a] & ficheros_por_pr[b]
        if not comunes:
            continue
        en_conflicto = conflicto(a, b) if conflicto else None
        ficheros = comunes if en_conflicto is None else set(en_conflicto)
        if ficheros:
            resultado.append((a, b, sorted(ficheros)))
    return resultado


def titulo(a: int, b: int) -> str:
    primera, segunda = sorted((a, b))
    return f"Mezclar PR #{primera} y #{segunda}: ficheros en común"


def par_de_titulo(texto: str) -> tuple[int, int] | None:
    encontrado = TITULO.match(texto.strip())
    return tuple(sorted((int(encontrado[1]), int(encontrado[2])))) if encontrado else None


def hay_binarios(ficheros: list[str]) -> bool:
    return any(f.lower().endswith(EXTENSIONES_BINARIAS) for f in ficheros)


def solo_localizacion(ficheros: list[str]) -> bool:
    return bool(ficheros) and all(f.startswith(LOCALIZACION) for f in ficheros)


def etiquetas(ficheros: list[str]) -> list[str]:
    return [ETIQUETA, "decision"] if hay_binarios(ficheros) else [ETIQUETA]


def cuerpo(antigua: dict, reciente: dict, ficheros: list[str], integracion: str) -> str:
    """Cuerpo de la issue: ficheros en conflicto e instrucciones para mezclar (o para decidir si hay binarios)."""
    lista = "\n".join(f"- `{f}`" for f in ficheros)
    cabecera = (f"Las PR #{antigua['number']} (`{antigua['headRefName']}`) y #{reciente['number']} "
                f"(`{reciente['headRefName']}`) están abiertas contra `{integracion}` y chocan al mezclarse en:"
                f"\n\n{lista}\n\n")
    if hay_binarios(ficheros):
        return cabecera + ("**No mezclar.** Hay `.uasset` o `.umap` en conflicto: son binarios y no se fusionan. "
                           "SkiTemplar o Mokius deciden qué versión gana (etiqueta `decision`); la otra PR rehace "
                           "su cambio sobre la versión elegida.")
    if solo_localizacion(ficheros):
        return cabecera + (
            "Solo chocan los ficheros generados de la localización: **no se mezclan a mano**.\n"
            f"1. Cuando #{antigua['number']} entre en `{integracion}`, rebasa #{reciente['number']} sobre "
            f"`{integracion}` quedándote con su versión de esos ficheros.\n"
            "2. Regenera: `Scripts\\localization_gather_export.bat`, traduce las entradas que falten en los "
            "`Game.po` y `Scripts\\localization_import_compile.bat`.\n"
            "3. Commitea `Game.manifest`, `.archive`, `.po` y `.locres` y actualiza la PR. Esta issue se cierra "
            "sola cuando el par deja de chocar.")
    return cabecera + (
        "Instrucciones para Claude:\n"
        f"1. Rebasa la PR más reciente, #{reciente['number']}, sobre la rama de #{antigua['number']} "
        f"(`{antigua['headRefName']}`), o sobre `{integracion}` si #{antigua['number']} ya está fusionada.\n"
        "2. Mezcla las dos versiones de cada fichero conservando el comportamiento de ambas PR; "
        "no descartes ninguno de los dos cambios.\n"
        "3. Compila (`TortunaboEditor` DebugGame si hay C++) y ejecuta los tests que afecten.\n"
        "4. Actualiza la PR rebasada y comenta aquí el resultado. Esta issue se cierra sola cuando las dos "
        "PR dejan de chocar.")


def ficheros_de_pr(gh: Gh, repo: str, numero: int) -> set[str]:
    salida = gh("pr", "view", str(numero), "--repo", repo, "--json", "files")
    return {f["path"] for f in json.loads(salida).get("files") or []}


def colisiones_abiertas(gh: Gh, repo: str) -> list[dict]:
    """Issues `colision` abiertas (número y título), para no duplicar un par y cerrar las resueltas."""
    salida = gh("issue", "list", "--repo", repo, "--state", "open", "--label", ETIQUETA,
                "--limit", "200", "--json", "number,title")
    return json.loads(salida)


def resueltas(abiertas: list[dict], prs: set[int], vigentes: set[tuple[int, int]]) -> list[tuple[int, str]]:
    """Issues `colision` que ya se pueden cerrar, con el motivo: alguna PR cerrada o el par ya no choca."""
    resultado = []
    for issue in abiertas:
        par = par_de_titulo(issue["title"])
        if par is None:
            continue
        fuera = [n for n in par if n not in prs]
        if fuera:
            resultado.append((issue["number"], f"la PR #{fuera[0]} ya no está abierta"))
        elif par not in vigentes:
            resultado.append((issue["number"], f"las PR #{par[0]} y #{par[1]} ya se mezclan sin conflicto"))
    return resultado


# --- git ------------------------------------------------------------------------------------------

def _git(*args: str) -> subprocess.CompletedProcess:
    return subprocess.run(["git", *args], capture_output=True, text=True, encoding="utf-8")


def traer_cabezas(numeros: Iterable[int]) -> bool:
    """Descarga la cabeza de cada PR en REF_PR. Sin blobs: merge-tree solo pide los de los ficheros que chocan.

    En un clon superficial (el puente) hace falta la historia para encontrar la base común.
    """
    refspecs = [f"+refs/pull/{n}/head:{REF_PR.format(n)}" for n in numeros]
    if not refspecs:
        return True
    extra = ["--unshallow"] if _git("rev-parse", "--is-shallow-repository").stdout.strip() == "true" else []
    filtro = ["--filter=blob:none"] if _git("config", "remote.origin.promisor").stdout.strip() == "true" else []
    return _git("fetch", "--quiet", "--no-tags", *filtro, *extra, "origin", *refspecs).returncode == 0


def conflicto_git(a: int, b: int) -> list[str] | None:
    """Ficheros en conflicto al mezclar las cabezas de dos PR ([] si se mezclan solas, None si no se sabe)."""
    proc = _git("merge-tree", "--write-tree", "--name-only", "--no-messages", REF_PR.format(a), REF_PR.format(b))
    if proc.returncode not in (0, 1):
        return None
    return [linea for linea in proc.stdout.splitlines()[1:] if linea.strip()] if proc.returncode == 1 else []
