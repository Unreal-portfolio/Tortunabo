---
name: tortu-que-hacer
description: Use when someone on the Tortunabo team asks what to work on, what is pending, what is free, or the state of the board ("¿qué hago?", "¿qué hay pendiente?", "¿qué podemos trabajar?", "¿cómo va el tablero?").
---

# Qué hacer ahora en Tortunabo

1. Ejecuta `uv run python Scripts/tablero/tablero.py pendiente`. Detecta quién es el usuario por su login de `gh`. Lee también `Docs/Equipo/Decisiones.md` si vas a proponer algo que cambie el comportamiento del juego.
2. Responde en este orden y en pocas líneas:
   - lo que ya tiene en curso o con PR abierta (terminar antes de empezar otra cosa);
   - lo que le toca revisar (revisión IA cruzada) y las issues en Revisiones que son suyas;
   - si es aprobador: decisiones pendientes;
   - issues en QA editor que puede probar en el editor ahora mismo (probar es trabajo útil y rápido);
   - de 3 a 5 issues libres en `Ready`, ordenadas por prioridad, con una frase de qué supone cada una.
3. Recomienda una con el motivo (prioridad, tamaño, que no choque con lo que otro tiene en curso en la misma área o en los mismos ficheros). Para Ruby, prioriza tamaños XS/S y la etiqueta `buena-primera`.
4. Si el usuario elige, sigue con la skill `tortu-coger`.

Las tareas cuelgan de objetos (sistemas del juego: Rally Tortuga, HUD y menús…). `pendiente` no lista los objetos porque no se cogen; si el usuario quiere ver el trabajo por sistema o elegir por objeto, remítelo a la vista «Objetos» del proyecto (https://github.com/orgs/Unreal-portfolio/projects/2), que muestra cada objeto con el progreso de sus sub-issues, o lista sus sub-issues con `gh issue view <objeto>`.

Si no hay nada en `Ready`, dilo y propone concretar una del Backlog: redactar criterios de aceptación verificables y pedir a un aprobador que la pase a `Ready`.

No inventes tareas que no estén en el tablero. Si el usuario menciona un trabajo que no existe como issue, créalo primero con `tablero.py nueva`.
