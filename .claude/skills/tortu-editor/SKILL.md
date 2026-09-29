---
name: tortu-editor
description: Use in Tortunabo whenever the user reports what they saw while testing in the Unreal editor or a build — something broken ("esto no funciona", "se cae", "falla el salto") or something confirmed working ("ya va", "funciona", "probado"). Also use at the end of a testing session to record implicit validations.
---

# Registrar pruebas en el editor

El tablero guarda, por issue, si algo se ha probado en el editor y cómo salió. Así se sabe qué está validado jugando, qué solo lo ha validado una IA y qué ha vuelto a romperse.

## Cuando el usuario dice que algo falla

1. Busca si ya existe: `gh issue list --state all --search "<palabras clave>"` y mira el tablero.
2. Si existe y estaba en `QA` o `Done` (o con Editor = `Funciona`): `tablero.py editor <n> falla --como "<PIE 4P, Standalone…>" --nota "<qué pasa y cómo reproducirlo>"`. La reabre, la devuelve a `In progress` y, si ya se había dado por buena, la marca `regresion`.
3. Si no existe: créala con `tablero.py nueva --tipo bug --estado Ready` con pasos para reproducir, resultado esperado y obtenido, mapa y número de jugadores. Pregunta la prioridad solo si no se deduce.
4. Si el usuario enumera varios fallos seguidos, registra uno por issue, sin mezclarlos.

## Cuando el usuario confirma que algo funciona

`tablero.py editor <n> funciona --como "<cómo lo probó>"`. Si la issue estaba en `QA`, pasa a `Done` y se cierra.

## Validación implícita

Si en esta sesión el usuario reportó un fallo, se corrigió y después siguió probando en el editor, reportando otras cosas sin volver a mencionar ese fallo, se considera validado. Al cerrar la tanda de pruebas, o cuando el usuario pase a otra cosa, enumera esas issues y regístralas con `--como "validación implícita"` y una nota con lo que probó después. Díselo en una línea para que pueda corregirlo. Si más adelante el fallo reaparece, el paso 2 lo reabre como regresión.

No marques `Funciona` algo que nadie ha probado en el editor: la revisión de código por IA va en su propio campo (`tablero.py ia`).
