"""Tablero de desarrollo de Tortunabo sobre GitHub Projects.

Una sola fuente de verdad para los tres desarrolladores y sus Claude: las issues
del repo y el proyecto «Tortunabo · Desarrollo». Este script hace de forma
determinista lo que no debe depender de que una IA se acuerde: listar lo
pendiente, coger una tarea, mover estados y reconciliar el tablero con las PR.

Uso (desde la raíz del repo):
    uv run python Scripts/tablero/tablero.py pendiente
    uv run python Scripts/tablero/tablero.py coger 42
    uv run python Scripts/tablero/tablero.py estado 42 "In review"
    uv run python Scripts/tablero/tablero.py nueva --titulo "..." --tipo bug --area Red --prioridad P1 --tamano S --cuerpo cuerpo.md
    uv run python Scripts/tablero/tablero.py sync [--aplicar]

Requiere `gh` autenticado con el scope `project` (`gh auth refresh -s project`).
"""

from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
import unicodedata
from datetime import datetime, timedelta, timezone
from pathlib import Path

CONFIG = json.loads((Path(__file__).parent / "equipo.json").read_text(encoding="utf-8"))
REPO = CONFIG["repo"]
OWNER = CONFIG["proyecto"]["owner"]
NUMERO = CONFIG["proyecto"]["numero"]
INTEGRACION = CONFIG["rama_integracion"]
ESTADOS = ["Backlog", "Ready", "In progress", "In review", "QA", "Done"]
ORDEN_PRIORIDAD = {"P0": 0, "P1": 1, "P2": 2, "P3": 3}
ORDEN_TAMANO = {"XS": 0, "S": 1, "M": 2, "L": 3}
REF_ISSUE = re.compile(r"(?:close[sd]?|fix(?:e[sd])?|resolve[sd]?|cierra|resuelve|refs?)\s+#(\d+)", re.I)
REF_RAMA = re.compile(r"/(\d+)-")

CONSULTA_ITEMS = """
query($org: String!, $num: Int!, $cursor: String) {
  organization(login: $org) { projectV2(number: $num) {
    id
    fields(first: 30) { nodes { ... on ProjectV2SingleSelectField { id name options { id name } } } }
    items(first: 100, after: $cursor) {
      pageInfo { hasNextPage endCursor }
      nodes {
        id
        content { __typename
          ... on Issue { number title state url updatedAt
            assignees(first: 5) { nodes { login } } labels(first: 15) { nodes { name } } }
          ... on PullRequest { number title state url }
        }
        fieldValues(first: 20) { nodes {
          ... on ProjectV2ItemFieldSingleSelectValue { name field { ... on ProjectV2SingleSelectField { name } } }
        } }
      }
    }
  } }
}
"""


class ErrorTablero(RuntimeError):
    """Fallo de gh o de git que el usuario debe ver tal cual."""


def gh(*args: str, entrada: str | None = None) -> str:
    """Ejecuta gh y devuelve stdout; si falla, lanza ErrorTablero con su stderr."""
    proc = subprocess.run(["gh", *args], capture_output=True, text=True, encoding="utf-8", input=entrada)
    if proc.returncode != 0:
        raise ErrorTablero(f"gh {' '.join(args[:3])}…: {proc.stderr.strip()}")
    return proc.stdout


def git(*args: str) -> str:
    proc = subprocess.run(["git", *args], capture_output=True, text=True, encoding="utf-8")
    if proc.returncode != 0:
        raise ErrorTablero(f"git {' '.join(args)}: {proc.stderr.strip()}")
    return proc.stdout.strip()


def cargar_proyecto() -> dict:
    """Devuelve id del proyecto, campos {nombre: {id, opciones}} e items de issues por número."""
    items, cursor, datos = {}, None, None
    while True:
        args = ["api", "graphql", "-f", f"query={CONSULTA_ITEMS}", "-f", f"org={OWNER}", "-F", f"num={NUMERO}"]
        if cursor:
            args += ["-f", f"cursor={cursor}"]
        datos = json.loads(gh(*args))["data"]["organization"]["projectV2"]
        for nodo in datos["items"]["nodes"]:
            contenido = nodo.get("content") or {}
            if contenido.get("__typename") != "Issue":
                continue
            valores = {v["field"]["name"]: v["name"] for v in nodo["fieldValues"]["nodes"] if v.get("field")}
            items[contenido["number"]] = {"item": nodo["id"], "valores": valores, **contenido}
        pagina = datos["items"]["pageInfo"]
        if not pagina["hasNextPage"]:
            break
        cursor = pagina["endCursor"]
    campos = {
        f["name"]: {"id": f["id"], "opciones": {o["name"]: o["id"] for o in f["options"]}}
        for f in datos["fields"]["nodes"] if f
    }
    return {"id": datos["id"], "campos": campos, "items": items}


def item_de_issue(proyecto: dict, numero: int) -> str:
    """Id del item del proyecto para la issue; la añade si aún no está."""
    if numero in proyecto["items"]:
        return proyecto["items"][numero]["item"]
    url = f"https://github.com/{REPO}/issues/{numero}"
    salida = gh("project", "item-add", str(NUMERO), "--owner", OWNER, "--url", url, "--format", "json")
    return json.loads(salida)["id"]


def poner_campo(proyecto: dict, numero: int, campo: str, valor: str) -> None:
    info = proyecto["campos"].get(campo)
    if info is None or valor not in info["opciones"]:
        raise ErrorTablero(f"El campo «{campo}» no admite «{valor}». Opciones: {list((info or {}).get('opciones', {}))}")
    gh("project", "item-edit", "--project-id", proyecto["id"], "--id", item_de_issue(proyecto, numero),
       "--field-id", info["id"], "--single-select-option-id", info["opciones"][valor])


def usuario_actual() -> str:
    return gh("api", "user", "--jq", ".login").strip()


def clave_orden(issue: dict) -> tuple:
    v = issue["valores"]
    return (ORDEN_PRIORIDAD.get(v.get("Prioridad"), 9), ORDEN_TAMANO.get(v.get("Tamaño"), 9), issue["number"])


def linea(issue: dict) -> str:
    v = issue["valores"]
    etiquetas = ",".join(n["name"] for n in issue.get("labels", {}).get("nodes", []))
    meta = " ".join(x for x in (v.get("Prioridad"), v.get("Tamaño"), v.get("Área"), v.get("Fase")) if x)
    validacion = [f"{c}: {v[c]}" for c in ("Revisión IA", "Editor") if v.get(c)]
    if validacion:
        meta += " | " + ", ".join(validacion)
    quien = ",".join(a["login"] for a in issue.get("assignees", {}).get("nodes", [])) or "libre"
    return f"  #{issue['number']} [{meta}] {issue['title']}  ({quien}{'; ' + etiquetas if etiquetas else ''})"


def prs_abiertas() -> list[dict]:
    campos = "number,title,headRefName,baseRefName,body,author,mergeable,isDraft,reviewDecision,updatedAt"
    return json.loads(gh("pr", "list", "--repo", REPO, "--state", "open", "--limit", "100", "--json", campos))


def issues_de_pr(pr: dict) -> set[int]:
    refs = {int(n) for n in REF_ISSUE.findall(pr.get("body") or "")}
    refs |= {int(n) for n in REF_RAMA.findall(pr.get("headRefName") or "")}
    return refs


def cmd_pendiente(_args: argparse.Namespace) -> None:
    yo = usuario_actual()
    aprobador = yo in CONFIG["aprobadores"]
    proyecto = cargar_proyecto()
    abiertas = [i for i in proyecto["items"].values() if i["state"] == "OPEN"]
    por_estado = {e: sorted([i for i in abiertas if i["valores"].get("Status") == e], key=clave_orden) for e in ESTADOS}
    mias = [i for i in por_estado["In progress"] if yo in {a["login"] for a in i["assignees"]["nodes"]}]
    prs = prs_abiertas()
    print(f"Tablero para {yo} ({CONFIG['miembros'].get(yo, {}).get('nombre', yo)}) · rama de integración: {INTEGRACION}\n")
    seccion("Tu trabajo en curso", [linea(i) for i in mias])
    seccion("Tus PR abiertas", [f"  PR #{p['number']} {p['title']} ({p['reviewDecision'] or 'sin revisar'}, {p['mergeable']})"
                                for p in prs if p["author"]["login"] == yo])
    if aprobador:
        seccion("PR de otros por revisar", [f"  PR #{p['number']} de {p['author']['login']}: {p['title']}"
                                            for p in prs if p["author"]["login"] != yo and not p["isDraft"]])
        seccion("Decisiones pendientes (etiqueta decision)",
                [linea(i) for i in abiertas if any(n["name"] == "decision" for n in i["labels"]["nodes"])])
    seccion("En QA: fusionado, falta probar en PIE", [linea(i) for i in por_estado["QA"]])
    no_cogibles = {"decision", "bloqueado"}
    libres = [i for i in por_estado["Ready"] if not i["assignees"]["nodes"]
              and not no_cogibles & {n["name"] for n in i["labels"]["nodes"]}]
    if not aprobador:
        libres.sort(key=lambda i: (ORDEN_TAMANO.get(i["valores"].get("Tamaño"), 9) > 1, clave_orden(i)))
    seccion("Libre para coger (Ready)", [linea(i) for i in libres[:10]])
    if not libres:
        seccion("Nada en Ready: backlog por concretar", [linea(i) for i in por_estado["Backlog"][:8]])


def seccion(titulo: str, lineas: list[str]) -> None:
    print(f"{titulo}:")
    print("\n".join(lineas) if lineas else "  (nada)")
    print()


def slug(texto: str) -> str:
    ascii_ = unicodedata.normalize("NFKD", texto).encode("ascii", "ignore").decode()
    return re.sub(r"[^a-z0-9]+", "-", ascii_.lower()).strip("-")[:40].strip("-")


def cmd_coger(args: argparse.Namespace) -> None:
    proyecto = cargar_proyecto()
    issue = proyecto["items"].get(args.numero)
    if issue is None:
        raise ErrorTablero(f"La issue #{args.numero} no está en el tablero. Ejecuta `sync --aplicar` o créala con `nueva`.")
    yo = usuario_actual()
    otros = [a["login"] for a in issue["assignees"]["nodes"] if a["login"] != yo]
    if otros and not args.forzar:
        raise ErrorTablero(f"#{args.numero} ya es de {', '.join(otros)}. Habla con esa persona o usa --forzar.")
    if git("status", "--porcelain", "--untracked-files=no"):
        raise ErrorTablero("Tienes cambios sin guardar en ficheros versionados. Haz commit o stash antes de cambiar de rama.")
    es_bug = any(n["name"] in ("⚠️bug⚠️", "bug") for n in issue["labels"]["nodes"])
    rama = f"{'fix' if es_bug else 'feat'}/{args.numero}-{slug(issue['title'])}"
    gh("issue", "edit", str(args.numero), "--repo", REPO, "--add-assignee", "@me")
    poner_campo(proyecto, args.numero, "Status", "In progress")
    git("fetch", "origin", INTEGRACION)
    git("switch", "-c", rama, f"origin/{INTEGRACION}")
    print(f"#{args.numero} asignada a {yo}, en In progress. Rama nueva: {rama} (desde origin/{INTEGRACION}).")


def cmd_estado(args: argparse.Namespace) -> None:
    proyecto = cargar_proyecto()
    poner_campo(proyecto, args.numero, "Status", args.estado)
    print(f"#{args.numero} → {args.estado}")


def cmd_campo(args: argparse.Namespace) -> None:
    proyecto = cargar_proyecto()
    poner_campo(proyecto, args.numero, args.campo, args.valor)
    print(f"#{args.numero} {args.campo} → {args.valor}")


def cmd_nueva(args: argparse.Namespace) -> None:
    etiqueta = "⚠️bug⚠️" if args.tipo == "bug" else "tarea"
    etiquetas = [etiqueta, *args.etiqueta]
    crear = ["issue", "create", "--repo", REPO, "--title", args.titulo, "--body-file", args.cuerpo]
    for e in etiquetas:
        crear += ["--label", e]
    url = gh(*crear).strip().splitlines()[-1]
    numero = int(url.rstrip("/").rsplit("/", 1)[-1])
    proyecto = cargar_proyecto()
    campos = {"Status": args.estado, "Prioridad": args.prioridad, "Tamaño": args.tamano, "Área": args.area, "Fase": args.fase,
              "Editor": "Sin probar" if args.estado == "QA" else None}
    for campo, valor in campos.items():
        if valor:
            poner_campo(proyecto, numero, campo, valor)
    print(f"#{numero} creada en {args.estado}: {url}")


def cmd_sync(args: argparse.Namespace) -> None:
    proyecto = cargar_proyecto()
    cambios, avisos = [], []
    reconciliar_issues_sueltas(proyecto, cambios)
    reconciliar_prs(proyecto, cambios, avisos)
    reconciliar_estancadas(proyecto, avisos)
    avisos_validacion(proyecto, avisos)
    print(f"## Parte del tablero · {datetime.now(timezone.utc):%Y-%m-%d %H:%M} UTC\n")
    print("### Cambios de estado" + ("" if args.aplicar else " (simulación: usa --aplicar)"))
    print("\n".join(f"- {c[0]}" for c in cambios) or "- ninguno")
    print("\n### Avisos")
    print("\n".join(f"- {a}" for a in avisos) or "- ninguno")
    if args.aplicar:
        for _texto, accion in cambios:
            accion()


def reconciliar_issues_sueltas(proyecto: dict, cambios: list) -> None:
    abiertas = json.loads(gh("issue", "list", "--repo", REPO, "--state", "open", "--limit", "500", "--json", "number,title"))
    for issue in abiertas:
        n = issue["number"]
        if n not in proyecto["items"]:
            cambios.append((f"#{n} entra al tablero en Backlog ({issue['title']})",
                            lambda n=n: poner_campo(proyecto, n, "Status", "Backlog")))
    for n, issue in proyecto["items"].items():
        if issue["state"] == "CLOSED" and issue["valores"].get("Status") != "Done":
            cambios.append((f"#{n} cerrada → Done", lambda n=n: poner_campo(proyecto, n, "Status", "Done")))


def reconciliar_prs(proyecto: dict, cambios: list, avisos: list) -> None:
    for pr in prs_abiertas():
        refs = issues_de_pr(pr)
        if pr["baseRefName"] != INTEGRACION:
            avisos.append(f"PR #{pr['number']} apunta a {pr['baseRefName']}, no a {INTEGRACION}")
        if pr["mergeable"] == "CONFLICTING":
            avisos.append(f"PR #{pr['number']} ({pr['author']['login']}) tiene conflictos con {pr['baseRefName']}: rebase del autor")
        if not refs:
            avisos.append(f"PR #{pr['number']} no enlaza ninguna issue (falta «Closes #n» o rama tipo/<n>-slug)")
        for n in refs:
            estado = proyecto["items"].get(n, {}).get("valores", {}).get("Status")
            if estado in (None, "Backlog", "Ready", "In progress"):
                cambios.append((f"#{n} → In review (PR #{pr['number']})",
                                lambda n=n: poner_campo(proyecto, n, "Status", "In review")))
    campos = "number,headRefName,body,mergedAt"
    fusionadas = json.loads(gh("pr", "list", "--repo", REPO, "--state", "merged", "--base", INTEGRACION,
                               "--limit", "40", "--json", campos))
    for pr in fusionadas:
        for n in issues_de_pr(pr):
            issue = proyecto["items"].get(n)
            if issue and issue["state"] == "OPEN" and issue["valores"].get("Status") not in ("QA", "Done"):
                cambios.append((f"#{n} → QA (PR #{pr['number']} fusionada en {INTEGRACION})",
                                lambda n=n: marcar_qa(proyecto, n)))


def marcar_qa(proyecto: dict, numero: int) -> None:
    poner_campo(proyecto, numero, "Status", "QA")
    poner_campo(proyecto, numero, "Editor", "Sin probar")
    gh("issue", "edit", str(numero), "--repo", REPO, "--add-label", "qa")


def comentar(numero: int, texto: str) -> None:
    gh("issue", "comment", str(numero), "--repo", REPO, "--body", texto)


def cmd_ia(args: argparse.Namespace) -> None:
    """Registra la revisión de una IA distinta de la que escribió el cambio."""
    proyecto = cargar_proyecto()
    valor = {"aprobada": "Aprobada", "cambios": "Cambios pedidos", "pendiente": "Pendiente"}[args.veredicto]
    poner_campo(proyecto, args.numero, "Revisión IA", valor)
    if args.nota:
        comentar(args.numero, f"**Revisión IA ({args.revisor}): {valor}.**\n\n{args.nota}")
    print(f"#{args.numero} Revisión IA → {valor}")


def cmd_editor(args: argparse.Namespace) -> None:
    """Registra la prueba en el editor de Unreal (PIE o Standalone) y mueve la issue en consecuencia."""
    proyecto = cargar_proyecto()
    issue = proyecto["items"].get(args.numero)
    if issue is None:
        raise ErrorTablero(f"La issue #{args.numero} no está en el tablero.")
    previo = issue["valores"].get("Editor")
    if args.resultado == "funciona":
        poner_campo(proyecto, args.numero, "Editor", "Funciona")
        comentar(args.numero, f"**Editor: funciona** ({args.como}).\n\n{args.nota or ''}".strip())
        if issue["valores"].get("Status") == "QA":
            poner_campo(proyecto, args.numero, "Status", "Done")
            gh("issue", "close", str(args.numero), "--repo", REPO, "--reason", "completed")
        print(f"#{args.numero} Editor → Funciona")
        return
    poner_campo(proyecto, args.numero, "Editor", "Falla")
    poner_campo(proyecto, args.numero, "Status", "In progress")
    poner_campo(proyecto, args.numero, "Revisión IA", "Pendiente")
    etiquetas = ["regresion"] if previo == "Funciona" or issue["state"] == "CLOSED" else []
    if issue["state"] == "CLOSED":
        gh("issue", "reopen", str(args.numero), "--repo", REPO)
    for e in etiquetas:
        gh("issue", "edit", str(args.numero), "--repo", REPO, "--add-label", e)
    comentar(args.numero, f"**Editor: falla** ({args.como}).\n\n{args.nota or 'Sin detalle: añade pasos para reproducirlo.'}")
    print(f"#{args.numero} Editor → Falla; vuelve a In progress{' como regresión' if etiquetas else ''}")


def reconciliar_estancadas(proyecto: dict, avisos: list) -> None:
    limite = datetime.now(timezone.utc) - timedelta(days=CONFIG["dias_sin_movimiento"])
    for n, issue in proyecto["items"].items():
        if issue["state"] != "OPEN" or issue["valores"].get("Status") != "In progress":
            continue
        actualizada = datetime.fromisoformat(issue["updatedAt"].replace("Z", "+00:00"))
        if issue["valores"].get("Editor") == "Falla":
            avisos.append(f"#{n} falló en el editor y está en curso: prioridad antes de coger trabajo nuevo")
        quien = ",".join(a["login"] for a in issue["assignees"]["nodes"]) or "sin asignar"
        if not issue["assignees"]["nodes"]:
            avisos.append(f"#{n} está In progress sin asignar")
        elif actualizada < limite:
            avisos.append(f"#{n} ({quien}) lleva más de {CONFIG['dias_sin_movimiento']} días sin movimiento")


def avisos_validacion(proyecto: dict, avisos: list) -> None:
    abiertas = [(n, i) for n, i in proyecto["items"].items() if i["state"] == "OPEN"]
    sin_ia = [f"#{n}" for n, i in abiertas
              if i["valores"].get("Status") == "In review" and i["valores"].get("Revisión IA") != "Aprobada"]
    sin_editor = [f"#{n}" for n, i in abiertas
                  if i["valores"].get("Status") == "QA" and i["valores"].get("Editor") in (None, "Sin probar")]
    if sin_ia:
        avisos.append(f"En review sin revisión de una segunda IA: {', '.join(sin_ia)}")
    if sin_editor:
        avisos.append(f"En QA sin probar en el editor: {', '.join(sin_editor)}")


def main() -> int:
    parser = argparse.ArgumentParser(description="Tablero de desarrollo de Tortunabo")
    sub = parser.add_subparsers(dest="cmd", required=True)
    sub.add_parser("pendiente", help="qué hay para mí ahora").set_defaults(fn=cmd_pendiente)
    p = sub.add_parser("coger", help="asignarme una issue y crear su rama")
    p.add_argument("numero", type=int)
    p.add_argument("--forzar", action="store_true")
    p.set_defaults(fn=cmd_coger)
    p = sub.add_parser("estado", help="mover una issue de columna")
    p.add_argument("numero", type=int)
    p.add_argument("estado", choices=ESTADOS)
    p.set_defaults(fn=cmd_estado)
    p = sub.add_parser("campo", help="fijar Prioridad, Tamaño, Área, Fase, Editor… de una issue")
    p.add_argument("numero", type=int)
    p.add_argument("campo")
    p.add_argument("valor")
    p.set_defaults(fn=cmd_campo)
    p = sub.add_parser("nueva", help="crear issue y colocarla en el tablero")
    p.add_argument("--titulo", required=True)
    p.add_argument("--tipo", choices=["bug", "tarea"], required=True)
    p.add_argument("--cuerpo", required=True, help="fichero markdown con el cuerpo")
    p.add_argument("--estado", default="Backlog", choices=ESTADOS)
    p.add_argument("--prioridad", choices=list(ORDEN_PRIORIDAD))
    p.add_argument("--tamano", choices=list(ORDEN_TAMANO))
    p.add_argument("--area")
    p.add_argument("--fase")
    p.add_argument("--etiqueta", action="append", default=[])
    p.set_defaults(fn=cmd_nueva)
    p = sub.add_parser("ia", help="registrar la revisión de una segunda IA")
    p.add_argument("numero", type=int)
    p.add_argument("veredicto", choices=["aprobada", "cambios", "pendiente"])
    p.add_argument("--revisor", default="IA revisora", help="p. ej. «Codex», «code-reviewer»")
    p.add_argument("--nota")
    p.set_defaults(fn=cmd_ia)
    p = sub.add_parser("editor", help="registrar la prueba en el editor de Unreal")
    p.add_argument("numero", type=int)
    p.add_argument("resultado", choices=["funciona", "falla"])
    p.add_argument("--como", default="PIE", help="PIE 1P, PIE 4P, Standalone… o «validación implícita»")
    p.add_argument("--nota")
    p.set_defaults(fn=cmd_editor)
    p = sub.add_parser("sync", help="reconciliar tablero, PR e issues")
    p.add_argument("--aplicar", action="store_true")
    p.set_defaults(fn=cmd_sync)
    args = parser.parse_args()
    try:
        args.fn(args)
    except ErrorTablero as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
