"""Tablero de desarrollo de Tortunabo sobre GitHub Projects.

Una sola fuente de verdad para los tres desarrolladores y sus Claude: las issues
del repo y el proyecto «Tortunabo · Desarrollo». Este script hace de forma
determinista lo que no debe depender de que una IA se acuerde: listar lo
pendiente, coger una tarea, mover estados y reconciliar el tablero con las PR.

Uso (desde la raíz del repo):
    uv run python Scripts/tablero/tablero.py pendiente
    uv run python Scripts/tablero/tablero.py coger 42
    uv run python Scripts/tablero/tablero.py estado 42 "In review"
    uv run python Scripts/tablero/tablero.py editor 42 funciona --como "PIE 4P"   # en cualquier estado
    uv run python Scripts/tablero/tablero.py nueva --titulo "..." --tipo bug --area Red --prioridad P1 --tamano S --cuerpo cuerpo.md --objeto "Rally Tortuga"
    uv run python Scripts/tablero/tablero.py objeto "Rally Tortuga" --area Modos --descripcion "..."
    uv run python Scripts/tablero/tablero.py colgar 57 90
    uv run python Scripts/tablero/tablero.py sync [--aplicar]

Las tareas y los fallos se agrupan por objeto (issue padre con la etiqueta `objeto`)
como sub-issues nativas de GitHub; la lógica de objetos vive en objetos.py.

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

import objetos

CONFIG = json.loads((Path(__file__).parent / "equipo.json").read_text(encoding="utf-8"))
REPO = CONFIG["repo"]
OWNER = CONFIG["proyecto"]["owner"]
NUMERO = CONFIG["proyecto"]["numero"]
INTEGRACION = CONFIG["rama_integracion"]
ESTADOS = ["Backlog", "Ready", "In progress", "In review", "Revisiones", "QA editor", "Done"]
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
    cache = proyecto.setdefault("items_nuevos", {})
    if numero not in cache:
        url = f"https://github.com/{REPO}/issues/{numero}"
        salida = gh("project", "item-add", str(NUMERO), "--owner", OWNER, "--url", url, "--format", "json")
        cache[numero] = json.loads(salida)["id"]
    return cache[numero]


def poner_campo(proyecto: dict, numero: int, campo: str, valor: str) -> None:
    info = proyecto["campos"].get(campo)
    if info is None or valor not in info["opciones"]:
        raise ErrorTablero(f"El campo «{campo}» no admite «{valor}». Opciones: {list((info or {}).get('opciones', {}))}")
    gh("project", "item-edit", "--project-id", proyecto["id"], "--id", item_de_issue(proyecto, numero),
       "--field-id", info["id"], "--single-select-option-id", info["opciones"][valor])


def vaciar_campo(proyecto: dict, numero: int, campo: str) -> None:
    """Deja el campo sin valor; los objetos, por ejemplo, no llevan Status."""
    gh("project", "item-edit", "--project-id", proyecto["id"], "--id", item_de_issue(proyecto, numero),
       "--field-id", proyecto["campos"][campo]["id"], "--clear")


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


def es_de(issue: dict, login: str) -> bool:
    return login in {a["login"] for a in issue.get("assignees", {}).get("nodes", [])}


def estado_tras_fusion(valores: dict) -> tuple[str, bool]:
    """Estado al fusionar la PR en dev y si hay que cerrar la issue.

    Si ya se probó en el editor (Editor = Funciona) y la revisión IA está aprobada, no queda nada
    que validar: Done y se cierra. Si no, QA editor, a la espera de que alguien la pruebe.
    """
    if valores.get("Editor") == "Funciona" and valores.get("Revisión IA") == "Aprobada":
        return "Done", True
    return "QA editor", False


def editor_tras_fusion(valores: dict) -> str | None:
    """Valor de Editor al pasar a QA editor: Sin probar, salvo que ya constara que funciona."""
    return None if valores.get("Editor") == "Funciona" else "Sin probar"


def estado_tras_editor(estado: str | None, resultado: str) -> tuple[str | None, bool]:
    """Estado tras registrar una prueba en el editor (None = no cambia) y si hay que cerrar la issue.

    La prueba no espera a la revisión: `funciona` en In progress o In review solo fija el campo;
    si la issue ya estaba fusionada (QA editor), pasa a Done. `falla` lleva a Revisiones siempre.
    """
    if resultado == "falla":
        return "Revisiones", False
    if estado == "QA editor":
        return "Done", True
    return None, False


def probables_en_editor(issues: list[dict], login: str) -> list[dict]:
    """Tareas propias en In progress o In review que aún no constan como Funciona en el editor."""
    return [i for i in issues if es_de(i, login) and i["valores"].get("Status") in ("In progress", "In review")
            and i["valores"].get("Editor") != "Funciona"]


def cmd_pendiente(_args: argparse.Namespace) -> None:
    yo = usuario_actual()
    aprobador = yo in CONFIG["aprobadores"]
    proyecto = cargar_proyecto()
    abiertas = [i for i in proyecto["items"].values() if i["state"] == "OPEN" and not objetos.es_objeto(i)]
    por_estado = {e: sorted([i for i in abiertas if i["valores"].get("Status") == e], key=clave_orden) for e in ESTADOS}
    mias = [i for i in por_estado["In progress"] if es_de(i, yo)]
    prs = prs_abiertas()
    print(f"Tablero para {yo} ({CONFIG['miembros'].get(yo, {}).get('nombre', yo)}) · rama de integración: {INTEGRACION}\n")
    seccion("Tu trabajo en curso", [linea(i) for i in mias])
    seccion("Puedes probar en el editor (tus tareas en curso o en revisión; no esperes a la revisión)",
            [linea(i) for i in sorted(probables_en_editor(abiertas, yo), key=clave_orden)])
    seccion("Te toca revisar (revisión IA cruzada)",
            [linea(i) for i in por_estado["In review"] if i["valores"].get("Revisor") == yo])
    seccion("Revisiones: algo no funciona, fallo comentado en la issue", [linea(i) for i in por_estado["Revisiones"]])
    seccion("Tus PR abiertas", [f"  PR #{p['number']} {p['title']} ({p['reviewDecision'] or 'sin revisar'}, {p['mergeable']})"
                                for p in prs if p["author"]["login"] == yo])
    if aprobador:
        seccion("PR de otros por revisar", [f"  PR #{p['number']} de {p['author']['login']}: {p['title']}"
                                            for p in prs if p["author"]["login"] != yo and not p["isDraft"]])
        seccion("Decisiones pendientes (etiqueta decision)",
                [linea(i) for i in abiertas if any(n["name"] == "decision" for n in i["labels"]["nodes"])])
    seccion("En QA editor: fusionado, falta probar en el editor", [linea(i) for i in por_estado["QA editor"]])
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


def elegir_revisor(proyecto: dict, autor: str) -> str:
    """Revisor cruzado según equipo.json; entre varios, el que tiene menos revisiones abiertas."""
    candidatos = CONFIG["revisores"].get(autor) or [a for a in CONFIG["aprobadores"] if a != autor]
    carga = {c: 0 for c in candidatos}
    for issue in proyecto["items"].values():
        r = issue["valores"].get("Revisor")
        if issue["valores"].get("Status") == "In review" and r in carga:
            carga[r] += 1
    return min(candidatos, key=lambda c: (carga[c], candidatos.index(c)))


def cmd_revision(args: argparse.Namespace) -> None:
    """Manda una issue terminada a revisión cruzada: In review, revisor asignado y Revisión IA pendiente."""
    proyecto = cargar_proyecto()
    autor = usuario_actual()
    revisor = args.revisor or elegir_revisor(proyecto, autor)
    poner_campo(proyecto, args.numero, "Status", "In review")
    poner_campo(proyecto, args.numero, "Revisor", revisor)
    poner_campo(proyecto, args.numero, "Revisión IA", "Pendiente")
    for pr in prs_abiertas():
        if args.numero in issues_de_pr(pr):
            gh("pr", "edit", str(pr["number"]), "--repo", REPO, "--add-reviewer", revisor)
    comentar(args.numero, f"Lista para revisión. Revisor: @{revisor} (su Claude hace la revisión IA con `tortu-revisar`).")
    print(f"#{args.numero} → In review; revisa {revisor}")


def cmd_campo(args: argparse.Namespace) -> None:
    proyecto = cargar_proyecto()
    poner_campo(proyecto, args.numero, args.campo, args.valor)
    print(f"#{args.numero} {args.campo} → {args.valor}")


def resolver_padre(args: argparse.Namespace) -> int | None:
    """Número del objeto del que colgará la issue nueva (lo crea si `--objeto` no existe)."""
    if args.padre is not None:
        return args.padre
    if not args.objeto:
        return None
    numero, creado = objetos.buscar_o_crear(gh, REPO, args.objeto)
    if creado:
        item_de_issue(cargar_proyecto(), numero)
        print(f"Objeto nuevo #{numero}: {args.objeto}")
    return numero


def cmd_nueva(args: argparse.Namespace) -> None:
    padre = resolver_padre(args)
    if padre is not None and (repetida := objetos.sub_issue_existente(gh, REPO, padre, args.titulo)):
        print(f"#{repetida} ya existe en el objeto #{padre} con ese título; no se crea otra.")
        return
    etiqueta = "⚠️bug⚠️" if args.tipo == "bug" else "tarea"
    etiquetas = [etiqueta, *args.etiqueta]
    crear = ["issue", "create", "--repo", REPO, "--title", args.titulo, "--body-file", args.cuerpo]
    for e in etiquetas:
        crear += ["--label", e]
    url = gh(*crear).strip().splitlines()[-1]
    numero = int(url.rstrip("/").rsplit("/", 1)[-1])
    if padre is not None:
        objetos.colgar(gh, REPO, numero, padre)
    proyecto = cargar_proyecto()
    campos = {"Status": args.estado, "Prioridad": args.prioridad, "Tamaño": args.tamano, "Área": args.area, "Fase": args.fase,
              "Editor": "Sin probar" if args.estado == "QA editor" else None}
    for campo, valor in campos.items():
        if valor:
            poner_campo(proyecto, numero, campo, valor)
    print(f"#{numero} creada en {args.estado}{f' dentro de #{padre}' if padre else ''}: {url}")


def cmd_objeto(args: argparse.Namespace) -> None:
    """Busca el objeto abierto con ese título o lo crea; lo deja en el proyecto sin Status."""
    numero, creado = objetos.buscar_o_crear(gh, REPO, args.nombre, args.descripcion)
    proyecto = cargar_proyecto()
    item_de_issue(proyecto, numero)
    if args.area:
        poner_campo(proyecto, numero, "Área", args.area)
    print(numero)
    print(f"{'Creado' if creado else 'Ya existía'} el objeto #{numero}: {args.nombre}", file=sys.stderr)


def cmd_colgar(args: argparse.Namespace) -> None:
    if objetos.colgar(gh, REPO, args.hijo, args.padre):
        print(f"#{args.hijo} cuelga ahora de #{args.padre}")
    else:
        print(f"#{args.hijo} ya colgaba de #{args.padre}")


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
    abiertas = json.loads(gh("issue", "list", "--repo", REPO, "--state", "open", "--limit", "500",
                             "--json", "number,title,labels"))
    for issue in abiertas:
        n = issue["number"]
        if n in proyecto["items"] or issue["title"] == "Parte diario del tablero":
            continue
        if objetos.es_objeto(issue):
            cambios.append((f"#{n} (objeto) entra al tablero sin Status ({issue['title']})",
                            lambda n=n: item_de_issue(proyecto, n)))
        else:
            cambios.append((f"#{n} entra al tablero en Backlog ({issue['title']})",
                            lambda n=n: poner_campo(proyecto, n, "Status", "Backlog")))
    for n, issue in proyecto["items"].items():
        if objetos.es_objeto(issue):
            if issue["valores"].get("Status"):
                cambios.append((f"#{n} es un objeto: se le quita el Status «{issue['valores']['Status']}»",
                                lambda n=n: vaciar_campo(proyecto, n, "Status")))
        elif issue["state"] == "CLOSED" and issue["valores"].get("Status") != "Done":
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
            issue = proyecto["items"].get(n, {})
            if issue and objetos.es_objeto(issue):
                avisos.append(f"PR #{pr['number']} enlaza el objeto #{n}: debe enlazar una de sus sub-issues")
                continue
            estado = issue.get("valores", {}).get("Status")
            if estado in (None, "Backlog", "Ready", "In progress"):
                cambios.append((f"#{n} → In review (PR #{pr['number']})",
                                lambda n=n, autor=pr["author"]["login"]: mover_a_review(proyecto, n, autor)))
    campos = "number,headRefName,body,mergedAt"
    fusionadas = json.loads(gh("pr", "list", "--repo", REPO, "--state", "merged", "--base", INTEGRACION,
                               "--limit", "40", "--json", campos))
    ya_vistas: set[int] = set()  # una issue con varias PR fusionadas se mueve una sola vez
    for pr in fusionadas:
        for n in issues_de_pr(pr) - ya_vistas:
            ya_vistas.add(n)
            issue = proyecto["items"].get(n)
            if issue and objetos.es_objeto(issue):
                continue
            if issue and issue["state"] == "OPEN" and issue["valores"].get("Status") not in ("QA editor", "Done"):
                destino, cerrar = estado_tras_fusion(issue["valores"])
                motivo = "; ya probada en el editor y aprobada: se cierra" if cerrar else ""
                cambios.append((f"#{n} → {destino} (PR #{pr['number']} fusionada en {INTEGRACION}{motivo})",
                                lambda n=n: aplicar_fusion(proyecto, n)))


def mover_a_review(proyecto: dict, numero: int, autor: str) -> None:
    poner_campo(proyecto, numero, "Status", "In review")
    valores = proyecto["items"].get(numero, {}).get("valores", {})
    if not valores.get("Revisor"):
        poner_campo(proyecto, numero, "Revisor", elegir_revisor(proyecto, autor))
    if not valores.get("Revisión IA"):
        poner_campo(proyecto, numero, "Revisión IA", "Pendiente")


def aplicar_fusion(proyecto: dict, numero: int) -> None:
    """Issue cuya PR se ha fusionado en dev: Done y cerrada si ya estaba validada; si no, QA editor."""
    valores = proyecto["items"][numero]["valores"]
    estado, cerrar = estado_tras_fusion(valores)
    poner_campo(proyecto, numero, "Status", estado)
    if cerrar:
        comentar(numero, f"Fusionada en `{INTEGRACION}` con la revisión IA aprobada y ya probada en el editor: Done.")
        gh("issue", "close", str(numero), "--repo", REPO, "--reason", "completed")
        return
    editor = editor_tras_fusion(valores)
    if editor:
        poner_campo(proyecto, numero, "Editor", editor)
    gh("issue", "edit", str(numero), "--repo", REPO, "--add-label", "qa")


def comentar(numero: int, texto: str) -> None:
    gh("issue", "comment", str(numero), "--repo", REPO, "--body", texto)


def cmd_ia(args: argparse.Namespace) -> None:
    """Registra la revisión de una IA distinta de la que escribió el cambio."""
    proyecto = cargar_proyecto()
    valor = {"aprobada": "Aprobada", "cambios": "Cambios pedidos", "pendiente": "Pendiente"}[args.veredicto]
    poner_campo(proyecto, args.numero, "Revisión IA", valor)
    if args.veredicto == "cambios":
        poner_campo(proyecto, args.numero, "Status", "Revisiones")
        if proyecto["items"].get(args.numero, {}).get("valores", {}).get("Editor") == "Funciona":
            # El arreglo cambia el código que se probó: la prueba anterior ya no lo valida.
            poner_campo(proyecto, args.numero, "Editor", "Sin probar")
    if args.nota:
        comentar(args.numero, f"**Revisión IA ({args.revisor}): {valor}.**\n\n{args.nota}")
    print(f"#{args.numero} Revisión IA → {valor}")


def issue_para_editor(proyecto: dict, numero: int) -> dict:
    """Issue del tablero; si no está (p. ej. cerrada y fuera del proyecto), la lee de GitHub y la añade."""
    issue = proyecto["items"].get(numero)
    if issue is not None:
        return issue
    datos = json.loads(gh("issue", "view", str(numero), "--repo", REPO, "--json", "state,labels"))
    item_de_issue(proyecto, numero)
    return {"state": datos["state"], "labels": datos["labels"], "valores": {}}


def cmd_editor(args: argparse.Namespace) -> None:
    """Registra la prueba en el editor de Unreal (PIE o Standalone) en cualquier estado de la issue.

    `funciona` solo fija el campo, salvo en QA editor (ya fusionada), donde pasa a Done y se cierra.
    `falla` la lleva a Revisiones y la reabre si estaba cerrada, con `regresion` si ya funcionaba.
    """
    proyecto = cargar_proyecto()
    issue = issue_para_editor(proyecto, args.numero)
    if objetos.es_objeto(issue):
        raise ErrorTablero(f"#{args.numero} es un objeto: registra la prueba en su sub-issue o crea una con `nueva --objeto`.")
    previo = issue["valores"].get("Editor")
    estado, cerrar = estado_tras_editor(issue["valores"].get("Status"), args.resultado)
    if args.resultado == "funciona":
        poner_campo(proyecto, args.numero, "Editor", "Funciona")
        comentar(args.numero, f"**Editor: funciona** ({args.como}).\n\n{args.nota or ''}".strip())
        if estado:
            poner_campo(proyecto, args.numero, "Status", estado)
        if cerrar:
            gh("issue", "close", str(args.numero), "--repo", REPO, "--reason", "completed")
        print(f"#{args.numero} Editor → Funciona{f'; pasa a {estado} y se cierra' if cerrar else ''}")
        return
    poner_campo(proyecto, args.numero, "Editor", "Falla")
    poner_campo(proyecto, args.numero, "Status", estado)
    poner_campo(proyecto, args.numero, "Revisión IA", "Pendiente")
    etiquetas = ["regresion"] if previo == "Funciona" or issue["state"] == "CLOSED" else []
    if issue["state"] == "CLOSED":
        gh("issue", "reopen", str(args.numero), "--repo", REPO)
    for e in etiquetas:
        gh("issue", "edit", str(args.numero), "--repo", REPO, "--add-label", e)
    comentar(args.numero, f"**Editor: falla** ({args.como}).\n\n{args.nota or 'Sin detalle: añade pasos para reproducirlo.'}")
    print(f"#{args.numero} Editor → Falla; pasa a Revisiones{' como regresión' if etiquetas else ''}")


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
                  if i["valores"].get("Status") == "QA editor" and i["valores"].get("Editor") in (None, "Sin probar")]
    if sin_ia:
        avisos.append(f"En review sin revisión de una segunda IA: {', '.join(sin_ia)}")
    if sin_editor:
        avisos.append(f"En QA sin probar en el editor: {', '.join(sin_editor)}")


def anadir_comandos_de_flujo(sub: argparse._SubParsersAction) -> None:
    """Comandos que mueven una issue por el ciclo: coger, estado, revisión, validaciones."""
    sub.add_parser("pendiente", help="qué hay para mí ahora").set_defaults(fn=cmd_pendiente)
    p = sub.add_parser("coger", help="asignarme una issue y crear su rama")
    p.add_argument("numero", type=int)
    p.add_argument("--forzar", action="store_true")
    p.set_defaults(fn=cmd_coger)
    p = sub.add_parser("estado", help="mover una issue de columna")
    p.add_argument("numero", type=int)
    p.add_argument("estado", choices=ESTADOS)
    p.set_defaults(fn=cmd_estado)
    p = sub.add_parser("revision", help="mandar una issue terminada a revisión cruzada")
    p.add_argument("numero", type=int)
    p.add_argument("--revisor", choices=list(CONFIG["miembros"]))
    p.set_defaults(fn=cmd_revision)
    p = sub.add_parser("campo", help="fijar Prioridad, Tamaño, Área, Fase, Editor… de una issue")
    p.add_argument("numero", type=int)
    p.add_argument("campo")
    p.add_argument("valor")
    p.set_defaults(fn=cmd_campo)
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


def anadir_comandos_de_alta(sub: argparse._SubParsersAction) -> None:
    """Comandos que crean u organizan issues: nueva, objeto, colgar y sync."""
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
    padre = p.add_mutually_exclusive_group()
    padre.add_argument("--objeto", help="título del objeto del que cuelga (se crea si no existe)")
    padre.add_argument("--padre", type=int, help="número de la issue padre")
    p.set_defaults(fn=cmd_nueva)
    p = sub.add_parser("objeto", help="buscar o crear un objeto (issue padre) e imprimir su número")
    p.add_argument("nombre")
    p.add_argument("--area")
    p.add_argument("--descripcion")
    p.set_defaults(fn=cmd_objeto)
    p = sub.add_parser("colgar", help="colgar una issue existente como sub-issue de otra")
    p.add_argument("hijo", type=int)
    p.add_argument("padre", type=int)
    p.set_defaults(fn=cmd_colgar)
    p = sub.add_parser("sync", help="reconciliar tablero, PR e issues")
    p.add_argument("--aplicar", action="store_true")
    p.set_defaults(fn=cmd_sync)


def main() -> int:
    parser = argparse.ArgumentParser(description="Tablero de desarrollo de Tortunabo")
    sub = parser.add_subparsers(dest="cmd", required=True)
    anadir_comandos_de_flujo(sub)
    anadir_comandos_de_alta(sub)
    args = parser.parse_args()
    try:
        args.fn(args)
    except (ErrorTablero, objetos.ErrorObjeto) as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
