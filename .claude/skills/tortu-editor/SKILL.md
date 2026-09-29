---
name: tortu-editor
description: Use in Tortunabo whenever the user reports what they saw while testing in the Unreal editor or a build — something broken ("esto no funciona", "se cae", "falla el salto") or something confirmed working ("ya va", "funciona", "probado"). Also use at the end of a testing session to record implicit validations.
---

# Registrar pruebas en el editor

El tablero guarda, por issue, si algo se ha probado en el editor y cómo salió. Así se sabe qué está validado jugando, qué solo lo ha validado una IA y qué ha vuelto a romperse.

El tablero lo mantienes tú, sin esperar a que te lo pidan: en cuanto el usuario cuente lo que ha visto en el editor, regístralo con `tablero.py editor` o `tablero.py nueva` en ese mismo turno.

La prueba en el editor no espera a la revisión: el campo Editor es independiente del estado y se registra en In progress, In review, QA editor o incluso con la issue cerrada.

## Cuando el usuario dice que algo falla

Lo decides y lo haces tú, sin preguntar. Solo preguntas, en una línea, si dudas de verdad a qué objeto pertenece el fallo.

1. Identifica el objeto afectado (el sistema o la pieza del juego: «Catapultas y manta», «Rally Tortuga», «HUD y menús»…). Mira la vista «Objetos» del proyecto o `gh issue list --label objeto`, y busca entre sus sub-issues y con `gh issue list --state all --search "<palabras clave>"`, cerradas incluidas.
2. Decide si es **el mismo fallo** o **un fallo distinto**:
   - mismo fallo (misma causa o mismo síntoma que una issue existente, en cualquier estado, **aunque esté cerrada**): reactívala con `tablero.py editor <n> falla --como "<PIE 4P, Standalone…>" --nota "<qué pasa y cómo reproducirlo>"`. La reabre si estaba cerrada, la pasa a Revisiones, le pone `regresion` si ya funcionaba y comenta el fallo en ella. No abras otra issue;
   - fallo distinto del mismo objeto: sub-issue nueva colgada del objeto con `tablero.py nueva --tipo bug --estado Ready --objeto "<objeto>"`, con pasos para reproducir, resultado esperado y obtenido, mapa y número de jugadores. Deduce la prioridad; pregúntala solo si no se puede deducir.
3. Si el objeto no existe todavía, `nueva --objeto "<nombre>"` lo crea y cuelga de él la sub-issue (o antes `tablero.py objeto "<nombre>" --area <Área>` para fijar su Área). Usa un nombre de sistema, no de síntoma: «Catapultas y manta», no «Las catapultas no hacen nada». Si la issue existía suelta, cuélgala con `tablero.py colgar <n> <objeto>`.
4. Si el usuario enumera varios fallos seguidos, registra uno por issue, sin mezclarlos.
5. Di en una línea qué has hecho (issue reactivada o creada, y de qué objeto cuelga) para que el usuario pueda corregirlo.

## Cuando el usuario confirma que algo funciona

`tablero.py editor <n> funciona --como "<cómo lo probó>"`, en cualquier estado:

- In progress o In review: solo fija Editor = Funciona y lo comenta. Cuando se fusione con la revisión IA aprobada, `sync --aplicar` la pasa directamente a Done y la cierra, sin pasar por QA editor.
- QA editor (ya fusionada): pasa a Done y se cierra.

## Validación implícita

Si en esta sesión el usuario reportó un fallo, se corrigió y después siguió probando en el editor, reportando otras cosas sin volver a mencionar ese fallo, se considera validado. Al cerrar la tanda de pruebas, o cuando el usuario pase a otra cosa, enumera esas issues y regístralas con `--como "validación implícita"` y una nota con lo que probó después. Díselo en una línea para que pueda corregirlo. Si más adelante el fallo reaparece, el paso 2 lo reactiva como regresión.

No marques `Funciona` algo que nadie ha probado en el editor: la revisión de código por IA va en su propio campo (`tablero.py ia`).
