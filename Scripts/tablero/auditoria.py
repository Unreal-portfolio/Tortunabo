"""Auditoría de organización del tablero: problemas de forma, no de código.

Cada problema tiene un tipo que decide qué hace `auditar --aplicar` (nunca toca código):

- `grave`: el fallo de organización afecta al trabajo (PR de un lote fusionada con miembros
  sin validar, estado incoherente con la PR, issue cerrada sin probar). La issue pasa a
  Revisiones con Prioridad P0 (reabierta si estaba cerrada) y un comentario que lo explica.
- `trivial`: basta mover la tarjeta a su columna o rellenar un campo evidente. Se corrige
  sin más y se anota en un comentario.
- `organizacion`: el resto. Etiqueta `revisar-organizacion` y un comentario con la lista.

La detección es pura: `problemas` recibe la issue ya normalizada con el contexto de sus PR
(`con_pr`, `fusionada`, `lote_fusionado`, `prs_sin_lote`, `revisor_sugerido`).
"""

from __future__ import annotations

import json
from collections.abc import Callable
from datetime import datetime, timedelta, timezone

import bloqueos
import flujo
import lotes
import memoria
import objetos

Gh = Callable[..., str]

ETIQUETA = "revisar-organizacion"
# La pone y la quita la rutina diaria de QA para lo que un script no ve; `auditar` no la toca.
ETIQUETA_QA = "revisar-qa"
COLOR = "FBCA04"
DESCRIPCION_ETIQUETA = "La issue tiene problemas de organización en el tablero (tablero.py auditar)"
CABECERA = "**Revisión de organización**"
DIAS_RESUMEN = 14
# Las issues cerradas antes de que existiera este sistema no tienen Resumen ni validaciones: no se auditan.
INICIO_SISTEMA = datetime(2026, 9, 30, tzinfo=timezone.utc)
# Asignado significa «estoy con ella ahora»: en estas columnas nadie está trabajando en la issue.
ESTADOS_SIN_DUENO = ("Backlog", "Ready", bloqueos.ESTADO)
CAMPOS_OBLIGATORIOS = ("Prioridad", "Área", "Tamaño")
TITULOS_EXCLUIDOS = {"Parte diario del tablero", "Estado del tablero"}
# Comentarios que escribe el propio tablero y no explican por qué algo falla.
PREFIJOS_AUTOMATICOS = ("Lista para revisión", "**Editor: funciona**", "**Revisión IA", "Fusionada en",
                        CABECERA, memoria.CABECERA_RESUMEN, memoria.CABECERA_DECISION, flujo.AVISO_SIN_QA)

CONSULTA_ISSUES = """
query($owner: String!, $repo: String!, $cursor: String, $since: DateTime) {
  repository(owner: $owner, name: $repo) {
    issues(first: 50, after: $cursor, states: [ESTADOS], filterBy: {since: $since}) {
      pageInfo { hasNextPage endCursor }
      nodes { number title state stateReason closedAt
        labels(first: 20) { nodes { name } }
        assignees(first: 5) { nodes { login } }
        parent { number }
        blockedBy(first: 10) { nodes { number state } }
        blocking(first: 10) { nodes { number state labels(first: 10) { nodes { name } } } }
        comments(last: 40) { nodes { body } }
      }
    }
  }
}
"""


def problema(texto: str, tipo: str = "organizacion", campo: str | None = None, valor: str | None = None) -> dict:
    return {"texto": texto, "tipo": tipo, "campo": campo, "valor": valor}


def explica_fallo(comentario: str) -> bool:
    """True si el comentario cuenta un fallo: prueba fallida con detalle, cambios pedidos o texto libre."""
    texto = comentario.lstrip()
    if texto.startswith("**Editor: falla**"):
        return "Sin detalle" not in texto
    if texto.startswith("**Revisión IA"):
        return "Cambios pedidos" in texto.splitlines()[0]
    return bool(texto) and not texto.startswith(PREFIJOS_AUTOMATICOS)


def graves_abierta(issue: dict) -> list[dict]:
    """Fallos de organización que afectan al trabajo en una issue abierta."""
    estado = issue["valores"].get("Status")
    if estado == "Revisiones":
        return []  # ya está donde debe; el comentario explica el fallo
    if issue.get("lote_fusionado") and estado not in lotes.LISTOS:
        return [problema(f"la PR #{issue['lote_fusionado']} de su lote se fusionó sin que esta issue estuviera "
                         "validada (revisión IA aprobada y Editor = Funciona)", "grave")]
    if estado == "QA editor" and issue.get("con_pr") and not issue.get("fusionada"):
        # Sin PR es una tarea solo de prueba (se crea directamente en QA editor); con PR, debe estar en dev.
        return [problema("está en QA editor, pero su PR no está fusionada en dev", "grave")]
    return []


def columna_correcta(issue: dict) -> str | None:
    """Columna que le corresponde según la regla, si es otra y el cambio es solo mover la tarjeta."""
    estado = issue["valores"].get("Status")
    fusionada, en_lote = issue.get("fusionada", False), bool(issue.get("lotes"))
    if estado == "Validada" and not en_lote and not fusionada:
        return "In review"  # Validada es solo para lotes: aprobada, a la espera de fusionarse
    if fusionada and not flujo.mueve_por_fusion(estado, con_pr_abierta=False):
        return None
    destino, _ = flujo.estado_objetivo(estado, issue["valores"], fusionada, en_lote)
    return destino if destino and destino != estado and destino != "Done" else None


def triviales_abierta(issue: dict) -> list[dict]:
    valores = issue["valores"]
    estado = valores.get("Status")
    lista = []
    if destino := columna_correcta(issue):
        lista.append(problema(f"está en {estado or 'sin estado'} y le corresponde {destino}", "trivial", "Status", destino))
    if estado == "In review" and not valores.get("Revisor") and issue.get("revisor_sugerido"):
        lista.append(problema("In review sin Revisor", "trivial", "Revisor", issue["revisor_sugerido"]))
    elif estado == "In review" and not valores.get("Revisor"):
        lista.append(problema("In review sin Revisor ni asignado del que deducirlo"))
    if estado == "QA editor" and not valores.get("Editor"):
        lista.append(problema("QA editor sin campo Editor", "trivial", "Editor", "Sin probar"))
    return lista


def organizacion_abierta(issue: dict) -> list[dict]:
    valores = issue["valores"]
    estado = valores.get("Status")
    lista = [] if issue.get("padre") else [problema("no cuelga de ningún objeto (`tablero.py colgar <n> <objeto>`)")]
    lista += [problema(f"sin {campo}") for campo in CAMPOS_OBLIGATORIOS if not valores.get(campo)]
    if estado == "In progress" and not issue["asignados"]:
        lista.append(problema("In progress sin asignado"))
    if estado in ESTADOS_SIN_DUENO and issue["asignados"]:
        lista.append(problema(f"tiene asignado ({', '.join(issue['asignados'])}) y está en {estado}: quien la tenga "
                              "que la coja (`tablero.py coger <n>`) o la suelte (`tablero.py soltar <n>`)"))
    if estado == "In review" and not issue.get("con_pr"):
        lista.append(problema("In review sin PR enlazada (`Closes #n` en la PR o rama `tipo/<n>-slug`)"))
    if estado == "Revisiones" and "colision" not in issue["etiquetas"] \
            and not any(explica_fallo(c) for c in issue["comentarios"]):
        lista.append(problema("en Revisiones sin un comentario que explique el fallo"))
    if estado == bloqueos.ESTADO and not issue.get("bloqueantes"):
        lista.append(problema("en Bloqueada sin dependencias registradas (`tablero.py bloquear <n> --por <m>`)"))
    for pr in issue.get("prs_sin_lote", []):
        lista.append(problema(f"su PR #{pr} cierra varias issues sin lote (`tablero.py lote crear`)"))
    return lista


def problemas_cerrada(issue: dict, ahora: datetime) -> list[dict]:
    """Cerradas en los últimos DIAS_RESUMEN días: resumen y, si se completaron, que estuvieran probadas."""
    cerrada = issue.get("cerrada")
    if cerrada is None or cerrada < INICIO_SISTEMA or ahora - cerrada > timedelta(days=DIAS_RESUMEN):
        return []
    lista = []
    completada = issue.get("motivo_cierre") == "COMPLETED"
    if completada and issue["valores"] and issue["valores"].get("Editor") != "Funciona":
        lista.append(problema("se cerró como completada sin estar probada en el editor (Editor ≠ Funciona)", "grave"))
    if not any(memoria.es_resumen(c) for c in issue["comentarios"]):
        lista.append(problema("cerrada sin comentario **Resumen** (`tablero.py resumen <n> --que ... --como ...`)"))
    return lista


def problemas(issue: dict, ahora: datetime) -> list[dict]:
    """Problemas de organización de una issue de trabajo (lista vacía si está en orden)."""
    if issue["estado"] != "OPEN":
        return problemas_cerrada(issue, ahora)
    graves = graves_abierta(issue)
    triviales = [] if graves else triviales_abierta(issue)
    return graves + triviales + organizacion_abierta(issue)


def texto_comentario(lista: list[dict]) -> str:
    return f"{CABECERA} (`tablero.py auditar`)\n" + "\n".join(f"- {p['texto']}" for p in lista)


def acciones(issue: dict, lista: list[dict]) -> dict:
    """Qué hacer al aplicar. Nunca toca código; no repite comentarios ni etiquetas.

    - graves: Revisiones + P0 (reabrir si está cerrada) y un comentario que lo explica;
    - triviales: los campos a corregir y un comentario que lo anota;
    - organización: etiqueta `revisar-organizacion` y un comentario con la lista, sin repetirlo;
      se quita la etiqueta cuando ya no queda ninguno.
    """
    graves = [p for p in lista if p["tipo"] == "grave"]
    triviales = [p for p in lista if p["tipo"] == "trivial"]
    organizacion = [p for p in lista if p["tipo"] == "organizacion"]
    campos = {p["campo"]: p["valor"] for p in triviales}
    comentarios = []
    if graves:
        campos = {"Status": "Revisiones", "Prioridad": "P0"}
        comentarios.append(f"{CABECERA}: fallo de organización que afecta al trabajo; pasa a Revisiones con P0.\n"
                           + "\n".join(f"- {p['texto']}" for p in graves))
    elif triviales:
        comentarios.append(f"{CABECERA}: corregido sin más.\n" + "\n".join(f"- {p['texto']}" for p in triviales))
    etiquetada = ETIQUETA in issue["etiquetas"]
    texto = texto_comentario(organizacion) if organizacion else None
    if texto and not any(c.strip() == texto for c in issue["comentarios"]):
        comentarios.append(texto)
    return {"campos": campos, "reabrir": bool(graves) and issue["estado"] != "OPEN", "comentarios": comentarios,
            "etiquetar": bool(organizacion) and not etiquetada, "desetiquetar": not organizacion and etiquetada}


def es_de_trabajo(issue: dict) -> bool:
    etiquetas = issue["etiquetas"]
    return (objetos.ETIQUETA not in etiquetas and lotes.ETIQUETA not in etiquetas
            and issue["titulo"] not in TITULOS_EXCLUIDOS)


def normalizar(nodo: dict, valores: dict, contexto: dict | None = None) -> dict:
    """Issue de GraphQL en la forma que usan las funciones puras, más el contexto de sus PR."""
    cerrada = nodo.get("closedAt")
    return {
        "numero": nodo["number"], "titulo": nodo["title"], "estado": nodo["state"],
        "motivo_cierre": nodo.get("stateReason"),
        "cerrada": datetime.fromisoformat(cerrada.replace("Z", "+00:00")) if cerrada else None,
        "etiquetas": objetos.nombres_etiquetas(nodo),
        "asignados": [a["login"] for a in nodo["assignees"]["nodes"]],
        "padre": (nodo.get("parent") or {}).get("number"),
        "bloqueantes": bloqueos.bloqueantes(nodo),
        "lotes": lotes.lotes_de(nodo),
        "comentarios": [c["body"] for c in nodo["comments"]["nodes"]],
        "valores": valores,
        **(contexto or {}),
    }


def leer_issues(gh: Gh, repo: str, estado: str, desde: datetime | None = None) -> list[dict]:
    """Issues del repo en ese estado (OPEN o CLOSED), actualizadas desde `desde` si se indica."""
    owner, nombre = repo.split("/", 1)
    consulta = CONSULTA_ISSUES.replace("ESTADOS", estado)
    nodos, cursor = [], None
    while True:
        args = ["api", "graphql", "-f", f"query={consulta}", "-f", f"owner={owner}", "-f", f"repo={nombre}"]
        if cursor:
            args += ["-f", f"cursor={cursor}"]
        if desde:
            args += ["-f", f"since={desde.isoformat()}"]
        datos = json.loads(gh(*args))["data"]["repository"]["issues"]
        nodos += datos["nodes"]
        if not datos["pageInfo"]["hasNextPage"]:
            return nodos
        cursor = datos["pageInfo"]["endCursor"]
