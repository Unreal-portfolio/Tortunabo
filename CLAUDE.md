# Tortunabo — guía para Claude

Juego cooperativo de 1 a 4 tortugas en Unreal Engine 5.6 con C++ (módulo `Source/Tortunabo`), listen server con Steam. Tres desarrolladores trabajan a la vez, cada uno con su Claude. Esta guía, las skills de `.claude/skills/` y `Docs/Equipo/Decisiones.md` son iguales para los tres: así las tres sesiones trabajan igual y saben lo mismo.

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

GitHub Project «Tortunabo · Desarrollo» en vista Kanban: https://github.com/orgs/Unreal-portfolio/projects/2. Toda tarea es una issue del repo y vive en el tablero.

| Estado | Significa |
|---|---|
| Backlog | Por concretar |
| Ready | Lista para coger |
| In progress | Alguien la hace o la corrige (el asignado es quien está con ella) |
| In review | Terminada; la IA de otro miembro del equipo la revisa (campo Revisor) |
| Revisiones | Algo no funciona. El fallo va comentado en la propia issue, no en una issue nueva |
| QA editor | Fusionada en dev; falta probarla en el editor |
| Done | Probada en el editor y cerrada |

Ciclo: Ready → In progress → In review → (Revisiones → In progress → In review)* → QA editor → Done. Si en QA editor algo falla, vuelve a Revisiones con el fallo comentado. Lo normal es que el propio revisor arregle lo que encuentra: la deja en Revisiones con el fallo comentado y la coge (`tablero.py coger <n> --forzar`), así pasa a In progress a su nombre y se sabe quién está con ella.

Cada issue lleva dos validaciones independientes:

- **Revisión IA** (`Pendiente` / `Aprobada` / `Cambios pedidos`): la hace el Claude del revisor asignado, nunca la sesión que escribió el código.
- **Editor** (`Sin probar` / `Funciona` / `Falla`): prueba real en el editor de Unreal. Si algo que funcionaba vuelve a fallar, la issue se reabre con la etiqueta `regresion`.

### Objetos y sub-issues

Las tareas y los fallos se agrupan por **objeto**: un sistema o una pieza del juego (el Rally, el puente y la plataforma tambaleantes, el HUD, las catapultas…). Un objeto es una issue padre con la etiqueta `objeto`; sus tareas y fallos cuelgan de él como sub-issues nativas de GitHub. El objeto no se mueve por el Kanban (no lleva Status): se ve en la vista «Objetos» del proyecto, con su barra de progreso de sub-issues, y las vistas de trabajo lo excluyen con `-label:objeto`.

- **El mismo fallo vuelve**: comentario en su misma sub-issue y la issue pasa a Revisiones (`tablero.py editor <n> falla`). No se abre otra.
- **Un fallo distinto del mismo objeto**: sub-issue nueva de ese objeto (`tablero.py nueva --tipo bug --objeto "<objeto>"`).
- **Un fallo de algo que aún no tiene objeto**: se crea el objeto (`tablero.py objeto "<nombre>" --area <Área>`) y se cuelga de él.
- **Sistemas grandes** como el Rally son objetos con todas sus tareas dentro; no se desglosan en épicas por fase. La fase de cada tarea va en su campo Fase.
- Las PR enlazan la sub-issue concreta, nunca el objeto.

El script `Scripts/tablero/tablero.py` hace todos los cambios de estado. No muevas tarjetas a mano:

```bash
uv run python Scripts/tablero/tablero.py pendiente             # qué hay para mí
uv run python Scripts/tablero/tablero.py coger <n>             # asignarme, In progress y rama
uv run python Scripts/tablero/tablero.py revision <n>          # terminada: In review con revisor cruzado
uv run python Scripts/tablero/tablero.py ia <n> aprobada|cambios --revisor "<quién>" --nota "..."
uv run python Scripts/tablero/tablero.py editor <n> funciona|falla --como "PIE 4P" --nota "..."
uv run python Scripts/tablero/tablero.py nueva --titulo "..." --tipo bug|tarea --cuerpo f.md --objeto "<objeto>" [--prioridad P1 --tamano S --area Red --estado Ready]
uv run python Scripts/tablero/tablero.py objeto "<nombre>" [--area X --descripcion "..."]   # busca o crea el objeto; imprime su número
uv run python Scripts/tablero/tablero.py colgar <hijo> <padre>  # colgar una issue existente de su objeto
uv run python Scripts/tablero/tablero.py estado <n> <estado> | campo <n> <campo> <valor>
uv run python Scripts/tablero/tablero.py sync [--aplicar]      # reconciliar PR, estados y avisos
```

Requiere `gh` autenticado con el scope de proyectos: `gh auth refresh -s project`.

## Decisiones y contexto compartido

`Docs/Equipo/Decisiones.md`: registro de decisiones del equipo, una línea por decisión con fecha, quién la tomó y enlace. Léelo antes de proponer algo que cambie un comportamiento del juego y añade una línea cuando SkiTemplar o Mokius decidan algo nuevo. Las decisiones de diseño de fondo están en el plan maestro (§1 y §7).

## Skills del proyecto

- `tortu-que-hacer`: «¿qué hago?», «¿qué hay pendiente?». Lee el tablero y propone.
- `tortu-coger`: empezar o retomar una issue.
- `tortu-entregar`: commit, PR hacia dev y paso a revisión cruzada.
- `tortu-revisar`: revisar lo que te han asignado; los aprobadores, además, fusionan y desglosan objetos en sub-issues.
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
