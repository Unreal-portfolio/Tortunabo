---
name: tortu-coger
description: Use when a Tortunabo team member decides to start or resume an issue, including fixing one that is in Revisiones ("cojo la #12", "empiezo con esto", "arreglo lo de la #20").
---

# Empezar una issue

1. Lee la issue entera con sus comentarios: `gh issue view <n> --comments`. Si no tiene criterios de aceptación claros, redáctalos, compártelos con el usuario y añádelos como comentario antes de tocar código.
2. Si la issue lleva `decision` o `bloqueado`, no la cojas: explica qué falta y vuelve a `tortu-que-hacer`.
3. Comprueba choques: `tablero.py pendiente` muestra lo que está en curso. Si otra issue en `In progress` toca la misma área, los mismos ficheros de `Source/` o los mismos `.uasset`/`.umap`, avisa al usuario antes de seguir.
4. Ejecuta `uv run python Scripts/tablero/tablero.py coger <n>`. Asigna la issue, la pasa a `In progress` y crea la rama `feat|fix/<n>-<slug>` desde `origin/dev`. Falla si hay cambios sin guardar: resuélvelo con el usuario (commit en su rama actual o `git stash`), nunca descartes cambios.
5. Si vas a tocar assets o mapas binarios, añade un comentario en la issue con la lista de ficheros.
6. Trabaja con commits pequeños en conventional commits. Cuando termines, usa `tortu-entregar`.
