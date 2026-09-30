# Tortunabo — guía para Claude

Juego cooperativo de 1 a 4 tortugas en Unreal Engine 5.6 con C++ (módulo `Source/Tortunabo`), listen server con Steam. Tres desarrolladores trabajan a la vez, cada uno con su Claude. Esta guía y las skills de `.claude/skills/` son iguales para los tres, y la memoria del equipo está en las issues: así las tres sesiones trabajan igual y saben lo mismo.

**El tablero lo mantiene Claude: cada vez que hagas algo, actualiza su estado con `tablero.py`, sin esperar a que te lo pidan** (coger, probar, entregar, revisar, fusionar, cerrar con **Resumen**, registrar fallos). El ciclo completo está en «Tablero».

El tablero es solo de desarrollo: código, pulido, bugs y revisión de assets. El diseño ya está decidido (plan maestro y decisiones); se pueden hacer prototipos, pero no tareas de «diseñar X».

## Equipo

| GitHub | Persona | Rol | Revisa su trabajo |
|---|---|---|---|
| SkiTemplar | Rodrigo | Director y aprobador | Mokius |
| Mokius | Mokius | Código y aprobador | SkiTemplar |
| Ruben-Besteiro | Ruby | Código | Mokius o SkiTemplar |

Las decisiones que no estén escritas las toman SkiTemplar o Mokius. Si te falta una, etiqueta la issue con `decision`, di qué hay que decidir en un comentario y sigue con otra tarea.

## Ramas

- `dev`: la rama de desarrollo hasta el final del juego. Todas las ramas salen de `origin/dev` y todas las PR van hacia `dev`.
- `main`: versión estable. No se toca ni se abren PR hacia ella.
- Rama de trabajo: `feat/<issue>-<slug>` o `fix/<issue>-<slug>`, una por issue. La crea `tablero.py coger`.
- Antes de abrir o actualizar una PR: `git fetch origin && git rebase origin/dev`. Ramas cortas: si una rama vive más de 2-3 días, rebase diario.

## Tablero: la única lista de tareas

GitHub Project «Tortunabo · Desarrollo» en vista Kanban: https://github.com/orgs/Unreal-portfolio/projects/2. Toda tarea es una issue del repo y vive en el tablero. **Lo mantiene Claude, sin esperar a que se lo pidan: cada paso de abajo se registra con `tablero.py` en el mismo turno.** No muevas tarjetas a mano.

| Estado | Significa |
|---|---|
| Backlog | Aún no aprobada para hacerse |
| Bloqueada | Espera a que se cierren las issues de las que depende (dependencias nativas de GitHub) |
| Ready | Aprobada: se puede trabajar |
| In progress | Alguien (o su Claude) la está haciendo o la dejó a medias; el asignado es quien está con ella |
| In review | Terminada: la revisa la IA de otro miembro del equipo (campo Revisor) |
| Revisiones | La revisión o la prueba encontraron un fallo, comentado en la propia issue |
| QA editor | Aprobada y fusionada en dev; falta probarla en el editor |
| Validada | Solo en lotes: aprobada y probada, espera a las demás issues de su lote |
| Done | Fusionada en dev, aprobada y probada en el editor; cerrada |

Dos validaciones por issue: **Revisión IA** (`Pendiente` / `Aprobada` / `Cambios pedidos`), que hace el Claude del revisor cruzado y nunca quien escribió el código, y **Editor** (`Sin probar` / `Funciona` / `Falla`), prueba real en el editor de Unreal.

Ciclo paso a paso:

1. **Coger** (`coger <n>`): In progress, asignada y rama desde `origin/dev`. `coger` rechaza las issues con bloqueantes abiertas.
2. **Probar mientras se trabaja** (`editor <n> funciona|falla`): si funciona, Editor = Funciona y la issue no pasará por QA editor; si falla, se queda en In progress con el fallo comentado: no se manda algo que no funciona.
3. **Entregar** (`revision <n>`): In review con revisor cruzado. Si el autor no la ha probado, va con Editor = Sin probar y el comentario «Sin QA editor». Con Editor = Falla, `revision` la rechaza.
4. **Revisar** (`ia <n> aprobada|cambios`): con cambios pasa a Revisiones con el fallo comentado. Lo normal es que el propio revisor lo arregle: la coge con `coger <n> --forzar` (In progress a su nombre) y la vuelve a entregar; la nueva revisión la hace otro.
5. **Fusionar** (aprobadores; `gh pr merge` y `sync --aplicar`): Editor = Funciona → Done y se cierra; si no, QA editor, la prueba quien sea (`editor <n> funciona`) → Done.
6. **Cerrar** (`resumen <n>`): cada issue que llega a Done lleva su comentario **Resumen**.

Una prueba que falla en In review, QA editor o con la issue cerrada la lleva a Revisiones (la reabre si hace falta, con `regresion` si ya funcionaba). Regla única que aplican `ia`, `editor` y `sync`: Done solo con la PR en dev, Revisión IA = Aprobada y Editor = Funciona.

**PR pequeñas y frecuentes a dev.** Un lote de varios bugs puede ir en una PR con `Closes` de todos, siempre con su lote: `lote crear --titulo "…" <n> <n> …` crea una issue temporal `lote` («Lote: …») que depende de cada miembro y la enlaza a la PR con «Refs». Cada miembro se revisa y se prueba por separado y, con las dos validaciones, espera en Validada. La PR del lote no se fusiona hasta que `lote estado <lote>` confirma que todos están en Validada; al fusionarla pasan a Done a la vez y el lote se cierra con un **Resumen** del conjunto.

**Dependencias**: si una issue no puede empezar hasta que se cierren otras, `bloquear <n> --por <m>` la deja en Bloqueada; `sync --aplicar` la pasa a Ready cuando se cierran todas.

**Colisiones y auditoría**: `colisiones --aplicar` crea una issue `colision` (P1, Revisiones) por cada par de PR abiertas que tocan los mismos ficheros, con las instrucciones para mezclarlas (si hay `.uasset`/`.umap`, `decision` y no se mezclan). `auditar --aplicar` revisa la organización sin tocar código: lo trivial (mover la tarjeta a su columna, rellenar un campo evidente) lo corrige y lo anota; lo que afecta al trabajo (PR de lote fusionada sin validar, estado incoherente con la PR, issue cerrada sin probar) lo pasa a Revisiones con P0 y lo explica; el resto lo etiqueta `revisar-organizacion` con un comentario. Las issues `colision` y `revisar-organizacion` van antes que cualquier otra tarea. Sin `--aplicar`, ambos solo informan.

### Objetos y sub-issues

Las tareas y los fallos se agrupan por **objeto**: un sistema o una pieza del juego (el Rally, el puente tambaleante, el HUD, las catapultas…). Un objeto es una issue padre con la etiqueta `objeto`, sin Status, que se ve en la vista «Objetos»; sus tareas y fallos cuelgan de él como sub-issues. Los sistemas grandes no se desglosan en épicas por fase: la fase va en el campo Fase. Las PR enlazan la sub-issue concreta, nunca el objeto.

Cuando el usuario dice «esto no funciona», Claude decide y registra sin preguntar (solo pregunta si duda de verdad a qué objeto pertenece), después de mirar los resúmenes del objeto (`resumenes <n>`):

- **El mismo fallo vuelve** (aunque su issue esté cerrada): `editor <n> falla` reactiva esa issue. No se abre otra.
- **Otro fallo del mismo objeto**: sub-issue nueva (`nueva --tipo bug --objeto "<objeto>"`) que cita las issues parecidas.
- **Algo que aún no tiene objeto**: `nueva --objeto "<nombre>"` crea el objeto y cuelga de él la sub-issue.

### Comandos

```bash
uv run python Scripts/tablero/tablero.py pendiente             # qué hay para mí (colisiones y organización primero)
uv run python Scripts/tablero/tablero.py coger <n> [--forzar]  # asignarme, In progress y rama
uv run python Scripts/tablero/tablero.py editor <n> funciona|falla --como "PIE 4P" --nota "..."
uv run python Scripts/tablero/tablero.py revision <n>          # terminada: In review con revisor cruzado
uv run python Scripts/tablero/tablero.py ia <n> aprobada|cambios --revisor "<quién> (Claude)" --nota "..."
uv run python Scripts/tablero/tablero.py resumen <n> --que "<qué fallaba>" [--por-que "<causa>"] --como "<arreglo>" [--pr <n>]
uv run python Scripts/tablero/tablero.py resumenes <n>         # resúmenes de las demás sub-issues de su objeto
uv run python Scripts/tablero/tablero.py decidir <n> --texto "<decisión>"
uv run python Scripts/tablero/tablero.py lote crear --titulo "..." <n> <n> ... | lote estado <lote>
uv run python Scripts/tablero/tablero.py bloquear <n> --por <m> [--por <k>]
uv run python Scripts/tablero/tablero.py nueva --titulo "..." --tipo bug|tarea --cuerpo f.md --objeto "<objeto>" [--prioridad P1 --tamano S --area Red --estado Ready]
uv run python Scripts/tablero/tablero.py objeto "<nombre>" [--area X --descripcion "..."] | colgar <hijo> <objeto>
uv run python Scripts/tablero/tablero.py estado <n> <estado> | campo <n> <campo> <valor>
uv run python Scripts/tablero/tablero.py sync|auditar|colisiones [--aplicar]
uv run python Scripts/tablero/tablero.py volcado [--publicar <issue>]   # tablero completo en Markdown
```

Requiere `gh` autenticado con el scope de proyectos: `gh auth refresh -s project`.

### Revisión diaria en la nube

Una rutina de Claude revisa cada mañana issues, PR y código y deja el parte en la issue #127. Su entorno solo llega a las rutas REST del repositorio, así que no puede leer ni mover el tablero. El workflow «Puente del tablero» (`.github/workflows/tablero-puente.yml`) lo hace por ella: a las 7:15 ejecuta `sync`, `auditar` y `colisiones` con `--aplicar` y publica el volcado del tablero en la issue #131; lanzado a mano con un comando (`estado 123 Ready`), lo ejecuta si está en la lista cerrada de `Scripts/tablero/volcado.py`. Las issues #127 y #131 no van al tablero. El workflow tiene que estar también en `main`, porque el cron solo se ejecuta desde la rama por defecto.

## Memoria del equipo: las issues

La memoria del equipo son las issues: su cuerpo y sus comentarios **Resumen** («Qué fallaba / Por qué / Cómo se arregló», con `resumen`) y **Decisión** («**Decisión** (fecha, quién): …», con `decidir`, en la issue u objeto afectado). Siempre resumidos: el comando rechaza más de 400 caracteres por campo. No hay otro registro. No hace falta leer todas las issues, sí las relacionadas: las del mismo objeto (`resumenes <n>`) y las abiertas en Revisiones o con `colision` o `revisar-organizacion`. Las decisiones de diseño de fondo están en el plan maestro (§1 y §7).

## Skills del proyecto

- `tortu-que-hacer`: «¿qué hago?», «¿qué hay pendiente?». Lee el tablero y propone (colisiones y organización primero).
- `tortu-coger`: empezar, retomar o arreglar una issue (también las `colision`).
- `tortu-entregar`: PR hacia dev, lote si hay varias issues y paso a revisión cruzada.
- `tortu-revisar`: revisión IA cruzada; los aprobadores, además, fusionan, deciden, auditan y desglosan objetos.
- `tortu-editor`: registrar lo que se prueba en el editor («esto no funciona», «esto ya va»).

## Reglas

- `Content/`, `.uasset` y `.umap` son binarios y no se pueden fusionar. El repo no usa Git LFS: antes de tocar un asset o un mapa, comprueba que ninguna issue en In progress lo nombra y escribe en tu issue qué assets vas a tocar.
- No toques `Deprecado/` ni `/Game/_Deprecado`.
- Commits en español técnico con ortografía completa y conventional commits (`feat|fix|refactor|docs|test|chore|perf`), sin líneas `Co-Authored-By`.
- La PR enlaza su issue con `Closes #<n>` en el cuerpo.
- Nunca hagas push a `main` ni a `dev`, ni `--force` sobre ramas ajenas.

## Compilar y probar

- Editor: `Build.bat TortunaboEditor Win64 DebugGame "<ruta>\Tortunabo.uproject" -WaitMutex -NoHotReload`. Cierra el editor antes de compilar.
- Tests de C++: `Automation RunTests Tortunabo` (consola del editor o Session Frontend). Lista de comandos en `Docs/Comandos_Prueba.md`.
- Tests de Python (terreno y tablero): `uv run pytest` desde la raíz.
- Plan vigente: `Docs/2026-09-29-Plan-Maestro-Modos-y-Mapas.md`. Desglose de tareas: `Docs/ROADMAP-macro-update.md`. Historial: `Docs/Bitacora.md`.
