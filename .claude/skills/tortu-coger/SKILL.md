---
name: tortu-coger
description: Use when a Tortunabo team member decides to start or resume an issue, including fixing one that is in Revisiones or resolving a `colision` issue ("cojo la #12", "empiezo con esto", "arreglo lo de la #20", "mezcla las PR").
---

# Empezar o arreglar una issue

El tablero lo mantienes tú, sin esperar a que te lo pidan. Ciclo y estados: «Tablero» en `CLAUDE.md`.

1. Lee la issue con sus comentarios (`gh issue view <n> --comments`) y los resúmenes de las demás sub-issues de su objeto, abiertas y cerradas (`tablero.py resumenes <n>`): si un fallo parecido ya se arregló, dilo y aprovecha cómo se hizo. Mira también si hay issues abiertas en Revisiones o con `colision` sobre los mismos ficheros. Si faltan criterios de aceptación, redáctalos, compártelos y añádelos como comentario antes de tocar código.
2. Con `decision` no la cojas: explica qué falta. Si está Bloqueada o tiene bloqueantes abiertas, `coger` la rechaza y dice de cuáles depende.
3. Choques: `tablero.py pendiente` (lo que está en curso) y `tablero.py colisiones` (PR abiertas con los mismos ficheros). Si otra issue en curso toca la misma área, los mismos ficheros de `Source/` o los mismos `.uasset`/`.umap`, avisa antes de seguir.
4. `uv run python Scripts/tablero/tablero.py coger <n>` (`--forzar` si la tenía otro, p. ej. al arreglar lo que encontraste revisando): asignada, In progress y rama `feat|fix/<n>-<slug>` desde `origin/dev`. Si hay cambios sin guardar, resuélvelo con el usuario (commit o `git stash`); nunca descartes cambios.
5. Si vas a tocar assets o mapas binarios, comenta en la issue la lista de ficheros.
6. Commits pequeños en conventional commits. En cuanto haya algo jugable, que el usuario lo pruebe y regístralo con `tortu-editor`: si funciona, Editor = Funciona y no pasará por QA editor; si falla, sigue en In progress. Al terminar, `tortu-entregar`: PR pequeñas y frecuentes.

## Issues `colision`

Dos PR abiertas tocan los mismos ficheros. Sigue la issue: rebasa la PR más reciente sobre la rama de la otra (o sobre `dev` si ya está fusionada), mezcla las dos versiones conservando el comportamiento de ambas, compila, prueba y comenta el resultado. Con `.uasset`/`.umap` en común lleva `decision`: no mezcles; decide un aprobador qué versión gana.
