---
name: tortu-que-hacer
description: Use when someone on the Tortunabo team asks what to work on, what is pending, what is free, or the state of the board ("¿qué hago?", "¿qué hay pendiente?", "¿qué podemos trabajar?", "¿cómo va el tablero?").
---

# Qué hacer ahora en Tortunabo

El tablero lo mantienes tú, sin esperar a que te lo pidan. Ciclo y estados: «Tablero» en `CLAUDE.md`.

1. Ejecuta `uv run python Scripts/tablero/tablero.py pendiente` (detecta al usuario por su login de `gh`). No leas todas las issues; sí las relacionadas: las que `pendiente` enseña arriba (`colision`, `revisar-organizacion`), las abiertas en Revisiones y, antes de proponer una, los resúmenes de su objeto (`tablero.py resumenes <n>`).
2. Responde en este orden y en pocas líneas:
   - **primero** las issues `colision` (dos PR tocan los mismos ficheros: hay que mezclarlas) y `revisar-organizacion` (su comentario dice qué falta);
   - lo que ya tiene en curso o con PR abierta: terminar antes de empezar otra cosa;
   - «Puedes probar en el editor»: sus tareas en In progress o In review sin probar (si funciona en In progress, no pasará por QA editor);
   - lo que le toca revisar y las issues en Revisiones que son suyas;
   - si es aprobador: decisiones pendientes, lotes abiertos (`gh issue list --label lote` y `tablero.py lote estado <lote>`) y `tablero.py auditar` si hace días que nadie lo pasa;
   - issues en QA editor que puede probar ahora mismo (probar es trabajo útil y rápido);
   - de 3 a 5 issues libres en Ready, por prioridad, con una frase de qué supone cada una. `pendiente` no ofrece las Bloqueadas ni las que tienen bloqueantes abiertas.
3. Recomienda una con el motivo (prioridad, tamaño, que no choque con lo que otro tiene en curso en la misma área o ficheros). Para Ruby, tamaños XS/S y `buena-primera`.
4. Si el usuario elige, sigue con `tortu-coger`.

Las tareas cuelgan de objetos (sistemas del juego); `pendiente` no los lista porque no se cogen. Para verlos, la vista «Objetos» del proyecto o `gh issue view <objeto>`.

Si no hay nada en Ready, dilo y propone concretar una del Backlog (criterios verificables) para que un aprobador la pase a Ready. No inventes tareas: si el usuario menciona un trabajo que no existe, créalo con `tablero.py nueva`.
