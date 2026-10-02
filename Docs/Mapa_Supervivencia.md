# Mapa de Supervivencia

Supervivencia (#143) deja de usar los chunks del Clásico y pasa a **un mapa nuevo por nivel**, generado entero de
una vez. Decisión en #143: se genera con el **generador del modo cooperativo** de Mokius (`TNProcMap::GenerateLayout`,
`TN_ProcMapRoute.h`, `TN_ProcMapPath.h`, `TN_ProcMapTerrain.h`, actor `ATN_ProcMapGenerator`), adaptado a lo que
sigue. El de SkiTemplar (`Scripts/terrain_path/`) queda descartado para este modo. Las constantes que mide el banco
viven en `Scripts/terrain_survival/spec.py`.

## Especificación

| | Valor |
|---|---|
| Tamaño | Unos 200 × 75 m (largo × ancho, proporción mínima 2:1), para que ir de la salida a la meta no lleve más de 5 minutos (Rubi, 02-10; antes 400 × 150 m). El generador hace 200 × 80 m (2 × 5 módulos de 40 m) y el banco mide los 75 m centrales: lo que sobra a los lados es muro del borde |
| Orientación | La salida en un extremo corto y la meta en el otro, a unos 12 m del borde |
| Generación | Todo el mapa de una pasada al empezar el nivel (los módulos internos del generador sí valen; los chunks que se encadenan durante la partida no); determinista dada la semilla, igual en todos los jugadores |
| Trazado | **Más o menos lineal**: el principal avanza siempre hacia la meta, sin pasadas de cruce ni lazos que vuelvan atrás |
| Bifurcaciones | **Todas terminan en el área de meta o vuelven al principal**: ningún callejón sin salida |
| Dificultad | Entrada 1–5. El nivel N de la partida pide `min(N, 5)`; más dificultad = más reto medido |
| Camino | La salida y la meta se unen a pie por suelo seco, pendiente ≤ 45° y **al menos 3 m de ancho**, o **saltando los huecos de salto** del Coop si su salto más largo cabe en el dive (≤ 4 m). Pendiente de registrar en #273 |
| Malla | ≤ 45 000 triángulos (3 M/km², la densidad de C01) |
| Tiempo | Generar un nivel tarda ≤ 3 s |
| Variedad | Dos semillas distintas dan mapas distintos (diferencia RMS normalizada ≥ 0,05) |
| Dificultad medible | Correlación de Spearman ≥ 0,5 entre la dificultad pedida y el reto medido |

### Qué cambia respecto al Coop

- **Forma**: la rejilla de módulos del Coop es cuadrada (`GridSize`×`GridSize` de `ModuleSize` = 400 m). Supervivencia
  necesita una rejilla rectangular (p. ej. 8×3 módulos de 50 m) o un perfil propio.
- **Ruta de módulos**: `NumCrossings = 0` y la ruta avanza de la salida a la meta.
- **Huecos de salto**: se mantienen (zanjas de 1,3-3,9 m en el camino); más y más largos con la dificultad. La trampa
  mortal al fondo va en #440.
- **Ramas**: las de dentro de un módulo ya se separan y vuelven a unirse al principal; la red de sendas, que une zonas
  del principal, debe acabar en el principal o en el área de meta. Un test de Automation sobre el layout comprueba que
  ninguna rama termina sin salida.
- **El Coop no cambia**: todo va en un perfil de Supervivencia (`TNProcMap::MakeSurvivalParams`, `TN_ProcMapSurvival.h`).
  La rejilla admite ser rectangular (`GridSizeX`, `WorldSizeX`) y un test fija la huella de los layouts del Coop.

### Cómo queda el perfil (#273)

- 2 × 5 módulos de 40 m; la ruta de módulos solo avanza (`bMonotonicRoute`), sin cruces, y recorre de 5 a 8 módulos
  según la dificultad, con más sinuosidad, más huecos y más largos, y el camino más estrecho (nunca menos de 3 m).
  El trazador de cada módulo está hecho para módulos de 400 m: `WalkScale` reduce sus pasos, tramos rectos y
  márgenes. Las ramas usan aún medidas de 400 m y en este tamaño casi no caben: los mapas salen sin bifurcaciones.
- Sin biomas de agua (`bWetBiomes`: su suelo son isletas y pasarelas), módulos a alturas parecidas (`LevelSpread`) y
  desniveles en rampa, sin toboganes ni géiseres (`SmoothTransitionMax`), el camino nunca a ras del mar antes de la
  playa (`MinPathZ`) y un margen extra en los bordes largos (`SideMargin`).
- `GenerateSurvivalLayout` descarta el mapa y prueba la siguiente semilla de una secuencia fija (igual en todas las
  máquinas) si el camino se pliega sobre sí mismo, se sale de los 75 m centrales o una rama se une con bordillo.
- En el editor: `ATN_ProcMapGenerator` con EditorMode = Supervivencia y `EditorSurvivalDifficulty` (1–5). En PIE:
  `LVL_ProcMap?ProcMode=Survival?ProcDifficulty=Easy|Normal|Hard?ProcSeed=N` (dificultad 1, 3 o 5), con las reglas de
  ronda genéricas hasta #274.
- Banco con 5 semillas × 5 dificultades: 100 % válidos, Spearman 0,71, variedad 0,14-0,19 y 0,1 s por mapa; el
  camino mide 200-250 m (menos de 2 minutos andando) (`Docs/Mapas/Supervivencia/coop.md`). «Fuera del camino» sale
  entre 0,29 y 0,63: fuera del camino hay algo de campo abierto, no solo muro.

## Banco de métricas

Mide los mapas ya generados con los mismos criterios de transitabilidad que los mapas de Coop
(`terrain_vol.validate`). La regla de las bifurcaciones se comprueba en C++ sobre el layout, no aquí.

- **Reto** (informativo): `(camino / línea recta − 1) + 2 × (proporción del camino con pendiente > 25°) + 0,25 ×
  (huecos saltados por cada 100 m)`.
- **Fuera del camino** (informativo): la parte del suelo alcanzable a más de 10 m del camino más corto. En un mapa
  lineal con el borde cerrado es baja. Un campo abierto también la sube, así que solo vale si el borde es infranqueable.

### Formato de intercambio

Un `.npz` por mapa (`SurvivalMap` en `mapa.py`) con `top` (cota en metros de lo alto de cada columna, una muestra
por metro, `[Norte, Este]`, 76 × 201 con la salida al oeste; el agua a −4 m), `start` y `goal` (índices de `top`),
`seed`, `difficulty`, `algorithm`, `gen_seconds`, `triangles` (−1 si no se conoce) y `jumps` (opcional: una fila
por hueco de salto con fila y columna de cada borde y el salto más largo en metros). El export de C++
(`TN.Survival.Export [carpeta] [semillas]`, por defecto en `Saved/Supervivencia`) escribe ficheros
`<semilla>_<dificultad>.npz`. La meta del export es el último punto seco del camino, en la playa de llegada.

### Uso

Desde `Scripts/`:

```bash
uv run --with matplotlib python -m terrain_survival.bench --carpeta <dir con los .npz> --nombre coop --semillas 5 --hojas
uv run --with matplotlib python -m terrain_survival.bench --algoritmo referencia --semillas 5 --hojas
```

Escribe `<nombre>.json`, `<nombre>.md` (resumen y tabla por mapa) y, con `--hojas`, un PNG por mapa con vista
cenital y en perspectiva, en `Docs/Mapas/Supervivencia/`. `referencia` es una línea base mínima (camino sinuoso entre
muros de ruido) que sirve para validar el banco. Los tests están en `Scripts/tests/test_terrain_survival.py`.

## Siguiente

1. Adaptar el generador del Coop (#273).
2. Usar el mapa generado en cada nivel de Supervivencia (#274).
