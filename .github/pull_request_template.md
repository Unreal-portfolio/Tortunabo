Closes #

<!-- PR pequeñas y frecuentes. Un lote de varios bugs puede ir en una sola PR: una línea «Closes #n» por issue y su lote (`tablero.py lote crear`, que añade «Refs #<lote>»). -->

## Qué cambia

<!-- Una o dos frases: qué hace esta rama y por qué. -->

## Cómo probarlo en el editor

<!-- Mapa, número de jugadores, pasos y comandos de consola (Docs/Comandos_Prueba.md). Qué debería verse. -->

## Comprobaciones

- [ ] Compila en el editor (UE 5.6) sin errores ni avisos nuevos.
- [ ] Probado en PIE; si toca red, también con 2 jugadores (anfitrión y cliente).
- [ ] Sin archivos de `Binaries/`, `Intermediate/`, `Saved/` ni `DerivedDataCache/`.
- [ ] Si toca mapas o assets binarios (`.umap`, `.uasset`), anotado en la issue para que nadie más los toque a la vez.
- [ ] Textos visibles con `NSLOCTEXT` (Docs/Localizacion.md) y documentación al día en `Docs/`.
- [ ] Revisión cruzada pedida con `tablero.py revision <n>` (la hace el Claude del revisor).

<!-- La PR va a dev desde la rama de la issue (nunca se trabaja sobre dev). La fusiona cualquiera de los tres con la revisión IA aprobada y, en un lote, con `lote estado` en verde (CLAUDE.md). -->
