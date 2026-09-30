"""Volcado del tablero a Markdown y puente para lanzar comandos desde GitHub Actions.

La rutina diaria de Claude en la nube solo llega a las rutas REST del repositorio: no
puede leer ni mover el Project, que es de la organización. El workflow
`.github/workflows/tablero-puente.yml` sí puede. Publica este volcado en una issue del
repo, que la rutina lee, y ejecuta por ella los comandos de `PERMITIDOS`.

Funciones puras; hablar con GitHub es cosa de tablero.py.
"""

from __future__ import annotations

import shlex
from datetime import datetime

import lotes
import objetos
from base import ESTADOS, ErrorTablero

MAX_TITULO = 70
# Una issue de GitHub admite 65 536 caracteres de cuerpo.
MAX_CUERPO = 60000
# Subcomandos que el workflow acepta por `workflow_dispatch`: los que mantienen el tablero.
# Quedan fuera los que crean ramas o dependen del usuario que los lanza (`coger`, `revision`).
PERMITIDOS = ("estado", "campo", "sync", "auditar", "colisiones", "bloquear", "colgar", "resumen", "decidir",
              "editor", "ia", "volcado")
CABECERA = "| # | Título | Asignados | Prio. | Revisión IA | Editor | Revisor | PR | Etiquetas | Espera a |"
SEPARADOR = "|---|---|---|---|---|---|---|---|---|---|"


def argumentos_de_puente(texto: str) -> list[str]:
    """Trocea el comando recibido por el workflow; lo rechaza si su subcomando no está permitido."""
    try:
        partes = shlex.split(texto or "")
    except ValueError as exc:
        raise ErrorTablero(f"Comando mal formado: {exc}") from exc
    if not partes:
        raise ErrorTablero("Comando vacío.")
    if partes[0] not in PERMITIDOS:
        raise ErrorTablero(f"«{partes[0]}» no se puede lanzar por el puente. Permitidos: {', '.join(PERMITIDOS)}")
    return partes


def _nombres(issue: dict, clave: str, campo: str) -> list[str]:
    return [n[campo] for n in (issue.get(clave) or {}).get("nodes", [])]


def _celda(texto: str) -> str:
    return str(texto).replace("|", "\\|").replace("\n", " ").strip() or "—"


def fila(issue: dict, prs: list[int]) -> str:
    v = issue.get("valores", {})
    titulo = issue.get("title", "")
    if len(titulo) > MAX_TITULO:
        titulo = titulo[:MAX_TITULO - 1] + "…"
    espera = [f"#{b['number']}" for b in (issue.get("blockedBy") or {}).get("nodes", []) if b.get("state") == "OPEN"]
    celdas = (f"#{issue['number']}", titulo, ", ".join(_nombres(issue, "assignees", "login")), v.get("Prioridad", ""),
              v.get("Revisión IA", ""), v.get("Editor", ""), v.get("Revisor", ""), ", ".join(f"#{n}" for n in prs),
              ", ".join(_nombres(issue, "labels", "name")), ", ".join(espera))
    return "| " + " | ".join(_celda(c) for c in celdas) + " |"


def _tabla(issues: list[dict], prs_por_issue: dict[int, list[int]]) -> list[str]:
    return [CABECERA, SEPARADOR, *(fila(i, prs_por_issue.get(i["number"], [])) for i in issues)]


def render(items: dict[int, dict], prs_por_issue: dict[int, list[int]], ahora: datetime) -> str:
    """Tablero completo en Markdown: las issues abiertas por estado, lo incoherente aparte y los objetos."""
    issues = sorted(items.values(), key=lambda i: i["number"])
    trabajo = [i for i in issues if not objetos.es_objeto(i) and not lotes.es_lote(i)]
    abiertas = [i for i in trabajo if i.get("state") == "OPEN"]
    lineas = [f"Volcado automático del tablero · {ahora:%Y-%m-%d %H:%M} UTC · {len(abiertas)} issues abiertas.",
              "No edites esta issue: el workflow «Puente del tablero» sustituye su contenido en cada ejecución.", ""]
    for estado in (*ESTADOS, None):
        grupo = [i for i in abiertas if i.get("valores", {}).get("Status") == estado]
        if estado == "Done" or not grupo:
            continue
        lineas += [f"## {estado or 'Sin estado'} ({len(grupo)})", "", *_tabla(grupo, prs_por_issue), ""]
    incoherentes = [i for i in trabajo
                    if (i.get("state") == "OPEN") == (i.get("valores", {}).get("Status") == "Done")]
    if incoherentes:
        lineas += [f"## Estado incoherente ({len(incoherentes)})", "",
                   "Abiertas en Done, o cerradas fuera de Done.", "",
                   "| # | Issue | Status | Título |", "|---|---|---|---|"]
        lineas += [f"| #{i['number']} | {'abierta' if i.get('state') == 'OPEN' else 'cerrada'} | "
                   f"{_celda(i.get('valores', {}).get('Status', ''))} | {_celda(i.get('title', '')[:MAX_TITULO])} |"
                   for i in incoherentes]
        lineas.append("")
    especiales = [i for i in issues if i.get("state") == "OPEN" and (objetos.es_objeto(i) or lotes.es_lote(i))]
    if especiales:
        lineas += [f"## Objetos y lotes abiertos ({len(especiales)})", ""]
        lineas += [f"- #{i['number']} {i.get('title', '')}" for i in especiales]
    texto = "\n".join(lineas).rstrip() + "\n"
    if len(texto) > MAX_CUERPO:
        texto = texto[:MAX_CUERPO].rsplit("\n", 1)[0] + "\n\n… (recortado: no cabe en una issue)\n"
    return texto
