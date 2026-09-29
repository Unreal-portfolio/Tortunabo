---
name: tortu-revisar
description: Use when SkiTemplar or Mokius (approvers) review or merge Tortunabo PRs, take pending decisions, triage the board, or break a phase epic into issues ("revisa las PR", "¿qué hay que decidir?", "desglosa la F1", "ordena el tablero").
---

# Revisar, fusionar y ordenar (solo aprobadores)

Comprueba el login con `gh api user --jq .login`. Si no es SkiTemplar ni Mokius, esta skill no aplica: dile al usuario que pida la revisión a uno de ellos.

## Revisar y fusionar una PR

1. `gh pr view <pr> --comments` y `gh pr diff <pr>`. Mira la issue enlazada y su campo Revisión IA.
2. Requisitos para fusionar en `macro-update`:
   - Revisión IA = `Aprobada` (si no, lanza la revisión como en `tortu-entregar`, paso 5);
   - compila en DebugGame en tu máquina si toca `Source/`, `Config/`, `Plugins/` o `.uproject` (las PR con `necesita-unreal` no se fusionan sin esto);
   - no tiene conflictos con `macro-update`;
   - si es de Ruby, la aprueba un aprobador explícitamente (`gh pr review <pr> --approve`).
3. `gh pr merge <pr> --merge`, después `uv run python Scripts/tablero/tablero.py sync --aplicar`: las issues enlazadas pasan a `QA` con Editor = `Sin probar`.
4. Si pides cambios: `gh pr review <pr> --request-changes --body "<qué y dónde>"`.

## Decisiones

Issues con la etiqueta `decision`: resume en 2-3 líneas qué hay que decidir y las opciones. Cuando el aprobador decida, escribe la decisión como comentario, quita la etiqueta y, si ya está concretada, pasa la issue a `Ready`.

## Desglosar una épica de fase

Las issues `[F0]`…`[F8]` (etiqueta `fase`) son épicas. Cuando una fase se active, crea sus issues concretas a partir de `Docs/ROADMAP-macro-update.md` y del plan maestro con `tablero.py nueva`: una unidad cerrable en 1-2 días, criterios de aceptación verificables, Área, Fase, Prioridad y Tamaño. Deja en `Ready` solo lo que no depende de nada abierto. Marca `buena-primera` lo XS/S aislado.

## Orden del tablero

`tablero.py sync` (sin `--aplicar`) da el parte: PR sin issue, PR con conflictos, issues estancadas, pendientes de revisión IA y de editor. Corrige lo que proceda y aplica con `--aplicar`.
