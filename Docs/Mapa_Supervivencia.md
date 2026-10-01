# Mapa de Supervivencia

Supervivencia (#143) deja de usar los chunks del Clásico y pasa a **un mapa nuevo por nivel**, generado entero de
una vez. Este documento fija cómo debe ser ese mapa y cómo se comparan los dos generadores candidatos: el de
SkiTemplar (Python, «camino primero», `Scripts/terrain_path/`: el camino principal vuelve sobre sí mismo en lazos
que se cruzan por arriba o por abajo) y el de Mokius (C++, el del modo cooperativo: `TNProcMap::GenerateLayout` y
`TN_ProcMapTerrain`: el principal avanza de módulo en módulo, con ramas que se separan y vuelven a unirse y pasadas
de cruce). El equipo ve el de Mokius más lineal y el de SkiTemplar más laberíntico; el banco debe confirmarlo
con números. Se usará el que cumpla mejor. Las constantes viven en
`Scripts/terrain_survival/spec.py`.

## Especificación

| | Valor |
|---|---|
| Tamaño | 400 × 150 m (largo × ancho, proporción mínima 2:1), una muestra de cota por metro |
| Orientación | El largo va de oeste a este: inicio al oeste, meta al este, a 12 m del borde corto |
| Generación | Una sola pasada, sin chunks ni módulos; determinista dada la semilla (igual en todos los jugadores) |
| Dificultad | Entrada 1–5. El nivel N de la partida pide `min(N, 5)`; más dificultad = más reto medido |
| Camino | El inicio y la meta se unen a pie por suelo seco, pendiente ≤ 45° y **al menos 3 m de ancho** |
| Malla | ≤ 180 000 triángulos (3 M/km², la densidad de C01) |
| Tiempo | Generar un nivel tarda ≤ 3 s |
| Variedad | Dos semillas distintas dan mapas distintos (diferencia RMS normalizada ≥ 0,05) |
| Dificultad medible | Correlación de Spearman ≥ 0,5 entre la dificultad pedida y el reto medido |

El «reto» es informativo: `(camino / línea recta − 1) + 2 × (proporción del camino con pendiente > 25°)`.
También lo es «fuera del camino», que mide lo laberíntico que es un mapa: la parte del suelo alcanzable que queda
a más de 10 m del camino más corto (0 = lineal; cuanto más alto, más ramas y lazos). No tiene umbral: qué es mejor
para Supervivencia se decide al comparar. **Limitación conocida:** en un campo abierto también sale alta, así que
solo separa lineal de laberinto cuando el borde del camino está cerrado. Medida sobre C01 (2,5D) da 0,95 porque
desde el camino se llega a casi todo el suelo seco (ver «Medición de C01»).

## Medición de C01 (2026-10-01)

Rasterizando a 1 m la malla ya generada de `C01_camino` (cota máxima de cada celda, como `terrain_vol.validate`):

- Meta alcanzable, pero sin camino de 3 m de ancho entre el inicio y la meta: el principal tiene estrechamientos.
- El camino más corto a pie (8 vecinos, pendiente ≤ 45°) mide 381 m frente a 335 m en línea recta (1,14). No
  sigue el camino diseñado: sale del cauce cerca del inicio y cruza la meseta, con escalones de 1,3 m como mucho.
- Desde el inicio se alcanza el 99 % del suelo seco, incluido el fondo de vistas que el diseño (§4.3, banda de
  bloqueo) quiere inalcanzable.

El generador comprueba ese bloqueo con su recorrido 3D (`gen_terrain_path.check`, `vista == 0`) y C01 lo pasa, así
que o el análisis 2,5D ve pasos que en 3D no existen o hay una fuga real. Hay que comprobarlo en el editor antes de
fiarse de las métricas de C01.

## Formato de intercambio

Un `.npz` por mapa (`SurvivalMap` en `mapa.py`) con `top` (cota en metros, `[Norte, Este]`, 151 × 401),
`start` y `goal` (índices de `top`), `seed`, `difficulty`, `algorithm`, `gen_seconds` y `triangles` (−1 si no
se conoce). Un generador Python lo escribe con `SurvivalMap.save`; el de C++ debe escribir el mismo formato y
nombrar los ficheros `<semilla>_<dificultad>.npz`.

## Cómo comparar

Desde `Scripts/`, con las mismas semillas y dificultades para todos:

```bash
uv run --with matplotlib python -m terrain_survival.bench --algoritmo referencia --semillas 5 --hojas --salida ../Docs/Mapas/Supervivencia
uv run --with matplotlib python -m terrain_survival.bench --carpeta <dir con los .npz> --nombre mokius --semillas 5 --hojas
```

Escribe `<nombre>.json`, `<nombre>.md` (resumen y tabla por mapa) y, con `--hojas`, un PNG por mapa con vista
cenital y en perspectiva. `referencia` es una línea base mínima (camino sinuoso entre muros de ruido): no es
candidata, sirve para validar el banco. Los candidatos se añaden en `adapters.py`.

Los tests están en `Scripts/tests/test_terrain_survival.py` (`uv run pytest`).

## Siguiente

1. Prototipo con el generador de SkiTemplar (#272) y con el de Mokius (#273).
2. Informe comparativo con una recomendación, que validan SkiTemplar y Mokius.
3. Integración del ganador en `ATN_SurvivalGameMode` (#274).
