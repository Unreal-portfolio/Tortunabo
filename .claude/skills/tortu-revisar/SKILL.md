---
name: tortu-revisar
description: Use in Tortunabo when someone reviews work assigned to them, or when SkiTemplar or Mokius merge PRs, take pending decisions, audit the board or break an object (sistema del juego) into sub-issues ("revisa lo que me toca", "revisa la PR", "¿qué hay que decidir?", "desglosa el Rally", "ordena el tablero", "audita el tablero").
---

# Revisar (revisión IA cruzada) y, si eres aprobador, fusionar

El tablero lo mantienes tú, sin esperar a que te lo pidan. Ciclo y estados: «Tablero» en `CLAUDE.md`. `tablero.py pendiente` enseña en «Te toca revisar» las issues en In review cuyo Revisor eres tú.

## Revisar una issue asignada

1. `gh issue view <n> --comments`, la PR (`gh pr view <pr>`, `gh pr diff <pr>`), los criterios y los resúmenes de su objeto (`tablero.py resumenes <n>`).
2. Revisa como sénior: criterios, corrección, replicación (autoridad del servidor, RPC validadas, `DOREPLIFETIME`), ciclo de vida de UObject, binarios de otra issue en curso y tests.
3. Veredicto:
   - bien: `tablero.py ia <n> aprobada --revisor "<tu login> (Claude)" --nota "<qué has comprobado>"`;
   - fallo: `tablero.py ia <n> cambios --revisor "<tu login> (Claude)" --nota "<fallo, fichero:línea y cómo reproducirlo>"` → Revisiones con el fallo comentado (Editor vuelve a Sin probar si constaba Funciona).
   - `--revisor` empieza por tu login: `ia` deja en el campo Revisor a quien ha revisado de verdad, aunque el asignado al entregar fuera otro. Nunca revises lo que ha escrito tu usuario.
4. Lo normal es que arregles tú lo que encuentras: tras el paso 3, `tablero.py coger <n> --forzar` (In progress a tu nombre) y, al terminar, `tortu-entregar`; la nueva revisión la hace otro. Si el arreglo es grande o no te toca, déjala en Revisiones para el autor.

## Fusionar (solo SkiTemplar o Mokius; `gh api user --jq .login`)

- Requisitos: Revisión IA = Aprobada; compila en DebugGame si toca `Source/`, `Config/`, `Plugins/` o `.uproject` (`necesita-unreal`); sin conflictos con `dev`; sin issue `colision` abierta sobre la PR (`tablero.py colisiones`).
- **PR de un lote** (lleva «Refs #<lote>»): `tablero.py lote estado <lote>`. Si falla (algún miembro no está en Validada o tiene una `decision` pendiente), **no la fusiones** y di qué falta.
- `gh pr merge <pr> --merge` y, sin que te lo pidan, `tablero.py sync --aplicar`: Editor = Funciona → Done y se cierra; si no, QA editor. En un lote, todos pasan a Done a la vez y el lote se cierra.
- Cada issue que pase a Done lleva su resumen: `tablero.py resumen <n> --que "<qué fallaba>" [--por-que "<causa>"] --como "<arreglo>" --pr <pr>`. El lote, uno del conjunto.

## Decisiones, organización y objetos (solo aprobadores)

- Issues con `decision`: resume qué hay que decidir y las opciones. Decidido, `tablero.py decidir <n> --texto "<decisión>"` en la issue u objeto afectado, quita la etiqueta (`gh issue edit <n> --remove-label decision`) y, si está concretada, pásala a Ready. No hay otro registro de decisiones.
- El puente ejecuta cada mañana `sync`, `auditar` y `colisiones` con `--aplicar` y la rutina de Claude deja el parte en la #127 («Organización diaria» en `CLAUDE.md`); a mano solo hacen falta tras un cambio grande. `tablero.py auditar` y `tablero.py colisiones` informan; con `--aplicar`, la auditoría corrige lo trivial y lo anota (columna, campo evidente, Ready con bloqueantes abiertas → Bloqueada), pasa a Revisiones con P0 lo que afecta al trabajo y etiqueta el resto `revisar-organizacion` (sin objeto, sin Prioridad, Área, Tamaño o Fase, forma de la issue, P0 en Backlog, asignado fuera de curso), y `colisiones` crea una issue `colision` por par de PR. Ninguno toca código.
- Desglose de objetos: sub-issues con `tablero.py nueva --objeto "<objeto>"` desde `Docs/ROADMAP-macro-update.md` y el plan maestro (1-2 días, criterios verificables, Área, Fase, Prioridad y Tamaño; `buena-primera` para lo XS/S aislado). Si una no puede empezar hasta que se cierren otras, `tablero.py bloquear <n> --por <m>`: en Backlog registra la dependencia y la deja en Backlog; aprobada, la pasa a Bloqueada y `sync` la devuelve a Ready. Aprobar es `tablero.py estado <n> Ready` (va a Bloqueada si sus bloqueantes siguen abiertas). `nueva` exige Prioridad, Tamaño y Área, rechaza títulos de más de 80 caracteres y cuerpos sin casillas, y no duplica títulos dentro de un objeto. Un P0 no se deja en Backlog.
- Sistema nuevo: `tablero.py objeto "<nombre>" --area <Área> --descripcion "..."`; issues sueltas, `tablero.py colgar <n> <objeto>`. Un objeto no se coge ni se mueve de columna, y las PR enlazan sus sub-issues, no el objeto.
