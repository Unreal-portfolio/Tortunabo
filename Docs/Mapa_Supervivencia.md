# Mapa de Supervivencia

Supervivencia (#143) deja de usar los chunks del Clásico y pasa a **un mapa nuevo por nivel**, generado entero de
una vez. Decisión en #143: se genera con el **generador del modo cooperativo** de Mokius (`TNProcMap::GenerateLayout`,
`TN_ProcMapRoute.h`, `TN_ProcMapPath.h`, `TN_ProcMapTerrain.h`, actor `ATN_ProcMapGenerator`), adaptado a lo que
sigue. El de SkiTemplar (`Scripts/terrain_path/`) queda descartado para este modo. Las constantes que mide el banco
viven en `Scripts/terrain_survival/spec.py`.

## Especificación

| | Valor |
|---|---|
| Tamaño | Unos 400 × 150 m (largo × ancho, proporción mínima 2:1) |
| Orientación | La salida en un extremo corto y la meta en el otro, a unos 12 m del borde |
| Generación | Todo el mapa de una pasada al empezar el nivel (los módulos internos del generador sí valen; los chunks que se encadenan durante la partida no); determinista dada la semilla, igual en todos los jugadores |
| Trazado | **Más o menos lineal**: el principal avanza siempre hacia la meta, sin pasadas de cruce ni lazos que vuelvan atrás |
| Bifurcaciones | **Todas terminan en el área de meta o vuelven al principal**: ningún callejón sin salida |
| Dificultad | Entrada 1–5. El nivel N de la partida pide `min(N, 5)`; más dificultad = más reto medido |
| Camino | La salida y la meta se unen a pie por suelo seco, pendiente ≤ 45° y **al menos 3 m de ancho** |
| Malla | ≤ 180 000 triángulos (3 M/km², la densidad de C01) |
| Tiempo | Generar un nivel tarda ≤ 3 s |
| Variedad | Dos semillas distintas dan mapas distintos (diferencia RMS normalizada ≥ 0,05) |
| Dificultad medible | Correlación de Spearman ≥ 0,5 entre la dificultad pedida y el reto medido |

### Qué cambia respecto al Coop

- **Forma**: la rejilla de módulos del Coop es cuadrada (`GridSize`×`GridSize` de `ModuleSize` = 400 m). Supervivencia
  necesita una rejilla rectangular (p. ej. 8×3 módulos de 50 m) o un perfil propio.
- **Ruta de módulos**: `NumCrossings = 0` y la ruta avanza de la salida a la meta.
- **Ramas**: las de dentro de un módulo ya se separan y vuelven a unirse al principal; la red de sendas, que une zonas
  del principal, debe acabar en el principal o en el área de meta. Un test de Automation sobre el layout comprueba que
  ninguna rama termina sin salida.
- **El Coop no cambia**: todo va en un perfil de Supervivencia.

## Banco de métricas

Mide los mapas ya generados con los mismos criterios de transitabilidad que los mapas de Coop
(`terrain_vol.validate`). La regla de las bifurcaciones se comprueba en C++ sobre el layout, no aquí.

- **Reto** (informativo): `(camino / línea recta − 1) + 2 × (proporción del camino con pendiente > 25°)`.
- **Fuera del camino** (informativo): la parte del suelo alcanzable a más de 10 m del camino más corto. En un mapa
  lineal con el borde cerrado es baja. Un campo abierto también la sube, así que solo vale si el borde es infranqueable.

### Formato de intercambio

Un `.npz` por mapa (`SurvivalMap` en `mapa.py`) con `top` (cota en metros de lo alto de cada columna, una muestra
por metro, `[Norte, Este]`, 151 × 401 con la salida al oeste), `start` y `goal` (índices de `top`), `seed`,
`difficulty`, `algorithm`, `gen_seconds` y `triangles` (−1 si no se conoce). El export de C++ escribe ficheros
`<semilla>_<dificultad>.npz`.

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
