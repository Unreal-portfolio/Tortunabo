---
name: tortu-entregar
description: Use when work on a Tortunabo issue is finished and needs to go up ("ya está", "súbelo", "haz la PR", "listo para revisar").
---

# Entregar una issue

1. Verifica lo que se pueda en esta máquina: compila `TortunaboEditor` DebugGame si has tocado C++, y ejecuta los tests que afecten (`Automation RunTests Tortunabo...` o `uv run pytest`). Si no has podido compilar, dilo en la PR; no lo des por bueno.
2. `git fetch origin && git rebase origin/macro-update`. Si hay conflictos en C++ o texto, resuélvelos con el usuario; si el conflicto es en un `.uasset` o `.umap`, para y avisa: no se fusionan, hay que decidir qué versión gana.
3. Push de la rama y PR hacia `macro-update`:
   `gh pr create --base macro-update --title "<tipo>: <resumen>" --body "Closes #<n>` + resumen + cómo probarlo en el editor`".
4. `uv run python Scripts/tablero/tablero.py estado <n> "In review"`.
5. Revisión por una segunda IA, nunca esta misma conversación, porque la sesión que escribió el código no ve sus propios fallos:
   - con el plugin de Codex: `/codex:review` sobre la rama;
   - si no, un subagente revisor con contexto limpio (tipo `code-reviewer` si existe; si no, `general-purpose`) al que le pasas solo el número de PR y el diff (`gh pr diff <pr>`).
   Registra el veredicto: `tablero.py ia <n> aprobada|cambios --revisor "<quién>" --nota "<hallazgos>"`. Si pide cambios, arréglalos, vuelve a subir y repite la revisión.
6. Di al usuario qué debe probar en el editor cuando se fusione. La fusión la hace un aprobador (skill `tortu-revisar`).
