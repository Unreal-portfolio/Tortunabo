"""Comando `avisos`: pushes directos a dev sin revisión y resumen del tablero por persona.

La lógica pura vive en avisos.py; aquí solo se habla con GitHub.
"""

from __future__ import annotations

import argparse
import json
import os
import subprocess
from datetime import datetime, timedelta, timezone

import avisos
import flujo
import lotes
import objetos
from base import CONFIG, INTEGRACION, REPO, ErrorTablero, cargar_proyecto, comentar, elegir_revisor, gh, poner_campo

# El comentario con la mención lo tiene que escribir otra cuenta: GitHub no avisa a nadie de lo que hace él mismo.
VARIABLE_TOKEN_AVISOS = "AVISOS_TOKEN"


def leer_pushes(desde: datetime) -> list[dict]:
    salida = gh("api", f"repos/{REPO}/activity?ref={INTEGRACION}&time_period=week&per_page=100")
    return avisos.pushes_directos(json.loads(salida), desde)


def leer_commits(push: dict) -> list[dict]:
    salida = gh("api", f"repos/{REPO}/compare/{push['antes']}...{push['despues']}")
    return [{"sha": c["sha"], "titulo": c["commit"]["message"].splitlines()[0], "padres": len(c["parents"])}
            for c in json.loads(salida).get("commits", [])][-avisos.MAX_COMMITS:]


def leer_prs(commits: list[dict]) -> dict[str, list[dict]]:
    return {c["sha"]: json.loads(gh("api", f"repos/{REPO}/commits/{c['sha']}/pulls")) for c in commits}


def issues_sin_revision() -> dict[str, int]:
    """Título → número de las issues `sin-revision`, abiertas o cerradas, para no duplicar un push."""
    salida = gh("issue", "list", "--repo", REPO, "--state", "all", "--label", avisos.ETIQUETA,
                "--limit", "200", "--json", "title,number")
    return {i["title"].strip(): i["number"] for i in json.loads(salida)}


def crear_issue(proyecto: dict, push: dict, commits: list[dict]) -> int:
    objetos.crear_etiqueta_si_falta(gh, REPO, avisos.ETIQUETA, avisos.COLOR, avisos.DESCRIPCION_ETIQUETA)
    crear = ["issue", "create", "--repo", REPO, "--title", avisos.titulo_issue(push["actor"], push["despues"]),
             "--label", avisos.ETIQUETA, "--body", avisos.cuerpo_issue(push, commits, REPO, INTEGRACION)]
    if push["actor"] in CONFIG["miembros"]:
        crear += ["--assignee", push["actor"]]
    numero = int(gh(*crear).strip().splitlines()[-1].rstrip("/").rsplit("/", 1)[-1])
    campos = {"Status": "In review", "Prioridad": "P0", "Tamaño": "S", "Fase": "Sin fase",
              "Revisión IA": "Pendiente", "Editor": "Sin probar"}
    if push["actor"] in CONFIG["miembros"]:
        campos["Revisor"] = elegir_revisor(proyecto, push["actor"])
    for campo, valor in campos.items():
        poner_campo(proyecto, numero, campo, valor)
    return numero


def incidencias(proyecto: dict, desde: datetime, aplicar: bool) -> list[tuple[str, str]]:
    """(actor, línea) por cada push directo desde `desde`; con `aplicar`, abre la issue de los que no son de un aprobador."""
    existentes, lista = None, []
    for push in leer_pushes(desde):
        try:
            commits = leer_commits(push)
        except ErrorTablero as exc:  # p. ej. un push forzado cuyo commit anterior ya no existe
            lista.append((push["actor"], f"**{push['actor']}** subió a dev por push directo "
                                         f"({push['cuando']:%d-%m %H:%M} UTC) y no se puede leer su diff: {exc}"))
            continue
        prs_por_commit = leer_prs(commits)
        sin_pr = avisos.commits_sin_pr(commits, prs_por_commit, push["cuando"])
        prs = avisos.prs_del_push(commits, prs_por_commit, push["cuando"])
        if not sin_pr and not prs:
            continue
        aprobador = push["actor"] in CONFIG["aprobadores"]
        numero = None
        if sin_pr and not aprobador:
            existentes = issues_sin_revision() if existentes is None else existentes
            titulo = avisos.titulo_issue(push["actor"], push["despues"])
            numero = existentes.get(titulo)
            if numero is None and aplicar:
                numero = existentes[titulo] = crear_issue(proyecto, push, sin_pr)
        lista.append((push["actor"], avisos.linea_push(push, sin_pr, prs, numero, aprobador)))
    return lista


def reconciliar(proyecto: dict, aplicar: bool) -> list[str]:
    """Issues `sin-revision` abiertas: su código ya está en dev, así que avanzan solo con las dos validaciones."""
    cambios = []
    for numero, issue in sorted(proyecto["items"].items()):
        if issue["state"] != "OPEN" or avisos.ETIQUETA not in objetos.nombres_etiquetas(issue):
            continue
        actual = issue["valores"].get("Status")
        destino, cerrar = flujo.estado_objetivo(actual, issue["valores"], fusionada=True, en_lote=False)
        if not destino or destino == actual:
            continue
        cambios.append(f"#{numero} → {destino}" + (" (revisada y probada: se cierra)" if cerrar else ""))
        if not aplicar:
            continue
        poner_campo(proyecto, numero, "Status", destino)
        if cerrar:
            comentar(numero, f"Fusionada en `{INTEGRACION}` por push directo, revisada y probada: Done.")
            gh("issue", "close", str(numero), "--repo", REPO, "--reason", "completed")
    return cambios


def leer_parte(numero: int | None, ahora: datetime) -> str | None:
    if numero is None:
        return None
    comentarios = json.loads(gh("api", f"repos/{REPO}/issues/{numero}/comments?per_page=100"))
    return avisos.parte_reciente(comentarios, ahora)


def publicar(numero: int, texto: str) -> None:
    """Comenta con el token de AVISOS_TOKEN si está definido (en el workflow, el de Actions), no con el del tablero."""
    entorno = dict(os.environ)
    if entorno.get(VARIABLE_TOKEN_AVISOS):
        entorno["GH_TOKEN"] = entorno[VARIABLE_TOKEN_AVISOS]
    proc = subprocess.run(["gh", "issue", "comment", str(numero), "--repo", REPO, "--body-file", "-"],
                          capture_output=True, text=True, encoding="utf-8", input=texto, env=entorno)
    if proc.returncode != 0:
        raise ErrorTablero(f"gh issue comment {numero}: {proc.stderr.strip()}")


def cmd_avisos(args: argparse.Namespace) -> None:
    ahora = datetime.now(timezone.utc)
    proyecto = cargar_proyecto()
    lista = incidencias(proyecto, ahora - timedelta(hours=args.horas), args.aplicar)
    cambios = reconciliar(proyecto, args.aplicar)
    if args.aplicar and (lista or cambios):
        proyecto = cargar_proyecto()  # con las issues recién creadas y los estados nuevos
    trabajo = [i for i in proyecto["items"].values()
               if i["state"] == "OPEN" and not objetos.es_objeto(i) and not lotes.es_lote(i)]
    parte = leer_parte(args.parte, ahora)
    print(f"## Avisos · {ahora:%Y-%m-%d %H:%M} UTC" + ("" if args.aplicar else " (simulación: usa --aplicar)") + "\n")
    print("\n".join(f"- {c}" for c in cambios) or "- sin cambios en las issues `sin-revision`")
    for login in CONFIG["miembros"]:
        aprobador = login in CONFIG["aprobadores"]
        propias = [linea for actor, linea in lista if aprobador or actor == login]
        texto = avisos.render(login, avisos.secciones(trabajo, login, aprobador, ahora, CONFIG["dias_sin_movimiento"]),
                              propias, ahora, parte if aprobador else None)
        if texto is None:
            print(f"\n{login}: nada que avisar")
            continue
        print(f"\n{texto}")
        if args.publicar is not None:
            publicar(args.publicar, texto)
    if args.publicar is not None:
        print(f"Avisos publicados en #{args.publicar}")


def anadir_comandos(sub: argparse._SubParsersAction) -> None:
    p = sub.add_parser("avisos", help="pushes directos a dev sin revisión y lo que espera por cada persona")
    p.add_argument("--aplicar", action="store_true", help="abrir las issues `sin-revision` y avanzar las validadas")
    p.add_argument("--publicar", type=int, metavar="ISSUE", help="comentar en esa issue el aviso de cada persona")
    p.add_argument("--parte", type=int, metavar="ISSUE", help="issue del parte de la rutina, para adjuntarlo a los aprobadores")
    p.add_argument("--horas", type=int, default=26, help="ventana de los pushes directos (por defecto, 26 h)")
    p.set_defaults(fn=cmd_avisos)
