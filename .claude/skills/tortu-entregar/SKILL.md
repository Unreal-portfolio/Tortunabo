---
name: tortu-entregar
description: Use when work on a Tortunabo issue is finished and needs to go up ("ya está", "súbelo", "haz la PR", "listo para revisar").
---

# Entregar una issue

El tablero lo mantienes tú, sin esperar a que te lo pidan: subir → In review con revisor (paso 4). Si ya se probó en el editor, que conste en el campo Editor: al fusionarse pasará directamente a Done.

1. Verifica lo que se pueda en esta máquina: compila `TortunaboEditor` DebugGame si has tocado C++, y ejecuta los tests que afecten (`Automation RunTests Tortunabo...` o `uv run pytest`). Si no has podido compilar, dilo en la PR; no lo des por bueno.
2. `git fetch origin && git rebase origin/dev`. Si hay conflictos en C++ o texto, resuélvelos con el usuario; si el conflicto es en un `.uasset` o `.umap`, para y avisa: no se fusionan, hay que decidir qué versión gana.
3. Push de la rama y PR hacia `dev` con la plantilla: `Closes #<n>`, qué cambia y cómo probarlo en el editor.
4. `uv run python Scripts/tablero/tablero.py revision <n>`: pasa la issue a In review, asigna el revisor cruzado (lo de Ruby a Mokius o SkiTemplar, lo de SkiTemplar a Mokius, lo de Mokius a SkiTemplar) y le pide revisión en la PR.
5. No te revises a ti mismo: la revisión IA la hace el Claude del revisor con `tortu-revisar`. Si quieres una comprobación previa antes de mandarla, `/codex:review` vale como segunda opinión, pero no sustituye a la revisión cruzada.
6. Di al usuario quién la revisa. Si aún no se ha probado en el editor, propón probarla ya, mientras está en revisión: no hace falta esperar a la fusión, y así al fusionarse va directa a Done en vez de a QA editor.
