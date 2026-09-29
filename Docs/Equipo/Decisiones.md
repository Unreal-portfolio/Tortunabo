# Decisiones del equipo

Registro compartido para los tres desarrolladores y sus Claude. Una línea por decisión, la más reciente arriba: `fecha · quién · decisión · enlace`. Solo decisiones vigentes que cambian cómo se trabaja o cómo se comporta el juego; si una queda anulada, se tacha y se añade la nueva.

Las decisiones de diseño de fondo (modos, mapas, prioridades de la macro update) están en `Docs/2026-09-29-Plan-Maestro-Modos-y-Mapas.md`, §1 «Decisiones cerradas» y §7 «Respuestas del director». No se copian aquí.

## Vigentes

- 2026-09-29 · SkiTemplar · La prueba en el editor no espera a la revisión: el campo Editor es independiente del estado y se registra en In progress o In review. Al fusionar en dev, si Editor = Funciona y Revisión IA = Aprobada, la issue pasa directamente a Done y se cierra; QA editor queda solo para lo fusionado que nadie ha probado. In review y la prueba en el editor van en cualquier orden. · `CLAUDE.md`, `Scripts/tablero/tablero.py`
- 2026-09-29 · SkiTemplar · El tablero lo mantiene Claude de forma automática: en cada paso (coger, probar, subir, revisar, fusionar) actualiza el estado con `tablero.py` sin que se lo pidan. · `CLAUDE.md`
- 2026-09-29 · SkiTemplar · Ante «esto no funciona», Claude decide y registra sin preguntar: si es el mismo fallo de una issue existente, aunque esté cerrada, la reactiva (reabrir, Revisiones, `regresion` si ya funcionaba, fallo comentado); si es un fallo distinto del mismo objeto, sub-issue nueva colgada del objeto (creándolo si no existe). Solo pregunta si duda de verdad a qué objeto pertenece. · `.claude/skills/tortu-editor`
- 2026-09-29 · SkiTemplar · Quien revisa una issue y encuentra un fallo lo arregla él mismo por defecto (Revisiones con el fallo comentado → coger → In progress); la nueva revisión la hace otro. · `CLAUDE.md`

- 2026-09-29 · SkiTemplar · La rama de integración se llama `dev` (antes `macro-update`). · `CLAUDE.md`
- 2026-09-29 · SkiTemplar · Las tareas y fallos se agrupan por objeto con sub-issues. · https://github.com/orgs/Unreal-portfolio/projects/2
- 2026-09-29 · SkiTemplar · El tablero «Tortunabo · Desarrollo» es la única lista de tareas y solo recoge desarrollo: código, pulido, bugs y revisión de assets. Nada de tareas de «diseñar X». · https://github.com/orgs/Unreal-portfolio/projects/2
- ~~2026-09-29 · SkiTemplar · `macro-update` es la rama de desarrollo hasta el final del juego; todo sale de ella y vuelve a ella por PR. · `CLAUDE.md`~~ Sustituida: la rama se llama `dev`.
- 2026-09-29 · SkiTemplar · Revisión IA cruzada: lo de Ruby lo revisa Mokius o SkiTemplar; lo de SkiTemplar, Mokius; lo de Mokius, SkiTemplar. · `Scripts/tablero/equipo.json`
- 2026-09-29 · SkiTemplar · Un fallo encontrado al revisar o probar una issue se comenta en la propia issue y la issue pasa a Revisiones; quien lo corrige la coge y pasa a In progress. · `CLAUDE.md`
- 2026-09-29 · SkiTemplar · La capa de producto de Steam (AppID, trámites) queda fuera del tablero por ahora. · #7, #14
- 2026-09-29 · SkiTemplar · El salto de la tortuga está validado en el editor. · #4
