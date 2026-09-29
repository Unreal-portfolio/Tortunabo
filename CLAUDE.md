# Tortunabo — guía para Claude

Juego cooperativo de 1 a 4 tortugas en Unreal Engine 5.6 con C++ (módulo `Source/Tortunabo`), listen server con Steam. Tres desarrolladores trabajan a la vez, cada uno con su Claude. Esta guía y las skills de `.claude/skills/` son iguales para los tres: así las tres sesiones trabajan igual.

## Equipo

| GitHub | Persona | Rol |
|---|---|---|
| SkiTemplar | Rodrigo | Director y aprobador |
| Mokius | Mokius | Código y aprobador |
| Ruben-Besteiro | Ruby | Código; sus PR las aprueba SkiTemplar o Mokius |

Las decisiones de diseño o alcance que no estén escritas en la issue las toman SkiTemplar o Mokius. Si te falta una, etiqueta la issue con `decision`, di qué hay que decidir en un comentario y sigue con otra tarea.

## Ramas

- `macro-update`: rama de integración. Todo el código más avanzado está aquí. Todas las ramas salen de `origin/macro-update` y todas las PR van hacia `macro-update`.
- `main`: versión estable. No se toca ni se abren PR hacia ella; la fusión de `macro-update` a `main` la deciden los aprobadores.
- Rama de trabajo: `feat/<issue>-<slug>` o `fix/<issue>-<slug>`, una por issue. La crea `tablero.py coger`.
- Antes de abrir o actualizar una PR: `git fetch origin && git rebase origin/macro-update`. Ramas cortas: si una rama vive más de 2-3 días, rebase diario.

## Tablero: la única lista de tareas

GitHub Project «Tortunabo · Desarrollo»: https://github.com/orgs/Unreal-portfolio/projects/2. Toda tarea es una issue del repo y vive en el tablero. No hay otra lista de tareas.

Estados: `Backlog` (por concretar) → `Ready` (lista para coger) → `In progress` (asignada y con rama) → `In review` (PR abierta) → `QA` (fusionada en macro-update, falta probarla en el editor) → `Done` (probada y cerrada).

Cada issue lleva además dos validaciones independientes:

- **Revisión IA** (`Pendiente` / `Aprobada` / `Cambios pedidos`): revisión del código por una IA distinta de la sesión que lo escribió.
- **Editor** (`Sin probar` / `Funciona` / `Falla`): prueba real en el editor de Unreal (PIE o Standalone). Si algo que funcionaba vuelve a fallar, la issue se reabre con la etiqueta `regresion`.

Una issue solo llega a `Done` con Editor = `Funciona`.

El script `Scripts/tablero/tablero.py` hace todos los cambios de estado. No muevas tarjetas a mano ni con llamadas sueltas a la API:

```bash
uv run python Scripts/tablero/tablero.py pendiente            # qué hay para mí
uv run python Scripts/tablero/tablero.py coger <n>            # asignarme, In progress y rama
uv run python Scripts/tablero/tablero.py estado <n> "In review"
uv run python Scripts/tablero/tablero.py ia <n> aprobada|cambios --revisor Codex --nota "..."
uv run python Scripts/tablero/tablero.py editor <n> funciona|falla --como "PIE 4P" --nota "..."
uv run python Scripts/tablero/tablero.py nueva --titulo "..." --tipo bug|tarea --cuerpo f.md [--prioridad P1 --tamano S --area Red --fase F4 --estado Ready]
uv run python Scripts/tablero/tablero.py sync [--aplicar]     # reconciliar PR, estados y avisos
```

Requiere `gh` autenticado con el scope de proyectos: `gh auth refresh -s project`.

## Skills del proyecto

- `tortu-que-hacer`: «¿qué hago?», «¿qué hay pendiente?». Lee el tablero y propone.
- `tortu-coger`: empezar una issue.
- `tortu-entregar`: commit, PR hacia macro-update y revisión de una segunda IA.
- `tortu-editor`: registrar lo que se prueba en el editor («esto no funciona», «esto ya va»).
- `tortu-revisar`: solo aprobadores. Revisar y fusionar PR, decidir y desglosar épicas.

## Reglas

- `Content/`, `.uasset` y `.umap` son binarios y no se pueden fusionar. El repo no usa Git LFS, así que no hay bloqueo de ficheros: antes de tocar un asset o un mapa, comprueba que ninguna issue en `In progress` lo nombra y escribe en tu issue qué assets vas a tocar.
- No toques `Deprecado/` ni `/Game/_Deprecado`.
- Commits en español técnico con ortografía completa y conventional commits (`feat|fix|refactor|docs|test|chore|perf`), sin líneas `Co-Authored-By`.
- La PR enlaza su issue con `Closes #<n>` en el cuerpo.
- Nunca hagas push a `main` ni a `macro-update`, ni `--force` sobre ramas ajenas.
- Si has encontrado un fallo que no es de tu tarea, créalo con `tablero.py nueva --tipo bug` y sigue con lo tuyo.

## Compilar y probar

- Editor: `Build.bat TortunaboEditor Win64 DebugGame "<ruta>\Tortunabo.uproject" -WaitMutex -NoHotReload`. Cierra el editor antes de compilar.
- Tests de C++: `Automation RunTests Tortunabo` (consola del editor o Session Frontend). Lista de comandos en `Docs/Comandos_Prueba.md`.
- Tests de Python (terreno): `uv run pytest` desde la raíz.
- Plan vigente: `Docs/2026-09-29-Plan-Maestro-Modos-y-Mapas.md`. Desglose de tareas: `Docs/ROADMAP-macro-update.md`. Historial: `Docs/Bitacora.md`.
