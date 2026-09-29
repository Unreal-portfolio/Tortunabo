---
name: tortu-revisar
description: Use in Tortunabo when someone reviews work assigned to them, or when SkiTemplar or Mokius merge PRs, take pending decisions or break an object (sistema del juego) into sub-issues ("revisa lo que me toca", "revisa la PR", "¿qué hay que decidir?", "desglosa el Rally", "ordena el tablero").
---

# Revisar (revisión IA cruzada) y, si eres aprobador, fusionar

`tablero.py pendiente` enseña en «Te toca revisar» las issues en In review cuyo Revisor eres tú.

## Revisar una issue asignada

1. `gh issue view <n> --comments`, la PR enlazada (`gh pr view <pr>` y `gh pr diff <pr>`) y los criterios de aceptación.
2. Revisa como revisor sénior: que cumple los criterios, corrección, replicación (autoridad del servidor, RPC validadas, `DOREPLIFETIME`), punteros y ciclo de vida de UObject, que no toca binarios de otra issue en curso, y tests.
3. Registra el veredicto en la issue:
   - bien: `tablero.py ia <n> aprobada --revisor "<tu login> (Claude)" --nota "<qué has comprobado>"`;
   - algo no funciona: `tablero.py ia <n> cambios --revisor "<tu login> (Claude)" --nota "<fallo, fichero:línea y cómo reproducirlo>"`. La issue pasa a Revisiones con el fallo comentado en ella.
4. Si decides corregirlo tú en lugar de devolverlo, primero déjala en Revisiones con el fallo comentado (paso 3) y luego cógela con `tablero.py coger <n> --forzar`: así pasa a In progress a tu nombre y se sabe quién está con ella. Al terminar, `tortu-entregar`.

## Fusionar (solo SkiTemplar o Mokius)

Comprueba el login con `gh api user --jq .login`.

- Requisitos: Revisión IA = Aprobada; compila en DebugGame si toca `Source/`, `Config/`, `Plugins/` o `.uproject` (las PR con `necesita-unreal` no se fusionan sin esto); sin conflictos con `dev`.
- `gh pr merge <pr> --merge` y después `tablero.py sync --aplicar`: las issues enlazadas pasan a QA editor con Editor = Sin probar.

## Decisiones y objetos (solo aprobadores)

- Issues con `decision`: resume qué hay que decidir y las opciones. Cuando se decida, comenta la decisión en la issue, añade una línea a `Docs/Equipo/Decisiones.md`, quita la etiqueta y, si ya está concretada, pásala a Ready.
- Objetos (etiqueta `objeto`, vista «Objetos»): el trabajo se desglosa por objeto, no en épicas por fase. Para desglosar uno, crea sus sub-issues desde `Docs/ROADMAP-macro-update.md` y el plan maestro con `tablero.py nueva --objeto "<objeto>"` (unidad cerrable en 1-2 días, criterios verificables, Área, Fase, Prioridad y Tamaño; `buena-primera` para lo XS/S aislado). La fase va en el campo Fase de cada sub-issue. `nueva` no duplica: si el objeto ya tiene una sub-issue abierta con ese título, lo avisa y no crea otra.
- Si aparece un sistema nuevo, créalo con `tablero.py objeto "<nombre>" --area <Área> --descripcion "..."`; las issues sueltas se cuelgan con `tablero.py colgar <n> <objeto>`. Un objeto no se coge ni se mueve de columna, y las PR enlazan sus sub-issues, no el objeto.
- `tablero.py sync` da el parte de avisos; aplica con `--aplicar`.
