---
name: tortu-que-hacer
description: Use when someone on the Tortunabo team asks what to work on, what is pending, what is free, or the state of the board ("¿qué hago?", "¿qué hay pendiente?", "¿qué podemos trabajar?", "¿cómo va el tablero?").
---

# Qué hacer ahora en Tortunabo

1. Ejecuta `uv run python Scripts/tablero/tablero.py pendiente`. Detecta quién es el usuario por su login de `gh`.
2. Responde en este orden y en pocas líneas:
   - lo que ya tiene en curso o con PR abierta (terminar antes de empezar otra cosa);
   - si es aprobador: PR de otros por revisar y decisiones pendientes;
   - issues en QA que puede probar en el editor ahora mismo (probar es trabajo útil y rápido);
   - de 3 a 5 issues libres en `Ready`, ordenadas por prioridad, con una frase de qué supone cada una.
3. Recomienda una con el motivo (prioridad, tamaño, que no choque con lo que otro tiene en curso en la misma área o en los mismos ficheros). Para Ruby, prioriza tamaños XS/S y la etiqueta `buena-primera`.
4. Si el usuario elige, sigue con la skill `tortu-coger`.

Si no hay nada en `Ready`, dilo y propone concretar una del Backlog: redactar criterios de aceptación verificables y pedir a un aprobador que la pase a `Ready`.

No inventes tareas que no estén en el tablero. Si el usuario menciona un trabajo que no existe como issue, créalo primero con `tablero.py nueva`.
