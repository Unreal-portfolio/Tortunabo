> **Obsoleto** (2026-10-02): traspaso de la rama macro-update; los mapas aparcados se retoman desde `Docs/Catalogo-Mapas-2026-09-29.md` y las issues del tablero.

# Relevo: mapas inventados (rama macro-update, sin push)

## Hecho (commits 65bad29aa y c12785e39)

| id | modo | tamaño | tri | MB |
|---|---|---|---|---|
| A02_donut | tct | 200 | 99 640 | 0,39 |
| A03_espiral | tct | 200 | 121 066 | 0,56 |
| A04_reloj | tct | 200 | 97 522 | 0,39 |
| A05_tablero | tct | 200 | 154 408 | 0,75 |
| A06_panal | tct | 200 | 114 690 | 0,52 |
| I02_galapagos | tct | 200 | 118 504 | 0,75 |
| I03T_tortuga_magna | tct | 200 | 103 388 | 0,52 |
| I05_santorini | tct | 200 | 130 308 | 0,82 |
| I03R_tortuga_magna | rally | 600 | 971 356 | 5,15 |
| I04_volcan_hueco | rally | 600 | 1 040 536 | 5,46 |
| I06_feroe | rally | 600 | 969 048 | 5,14 |

Todos con validadores en verde, lámina en Docs/Mapas/<id>.png (revisada), entrada en index.json y fila en
Docs/Catalogo-Mapas-2026-09-29.md (§7 y §8). Tests: test_terrain_kit.py (9) y test_terrain_inventados.py (11).

## Falta

Los 16 mapas NUEVOS (ninguno generado todavía). Plan de lotes:
- Lote 1, TcT (`terrain_shapes/lots_arenas.py`): N01 Coliseo con foso (gradas en anillo, foso de agua, 4 puertas
  en túnel + puente), N02 Anfiteatro en la ladera, N03 Volcán con cráter-arena y río de lava, N04 Atolón.
- Lote 2, TcT (`lots_giants.py`): N05 Isla calavera (cuevas-ojo como túneles), N06 Castillo de arena, N07 Bañera
  gigante, N08 Mesa de picnic.
- Lote 3, Rally (añadir a `lots_rally.py`): N09 Cañón serpiente, N10 Fiordo de los puentes, N11 Montaña hueca
  (túnel + sala interior + lucernario), N12 Delta de islotes (`auto_water`).
- Lote 4, Coop/2 vs 2 (`lots_routes.py`, modo "coop"; tope 1,1 M tri): N13 Barco varado, N14 Isla flotante en
  escalones (`kit.Floating`), N15 Laberinto de dunas (2 vs 2 simétrico), N16 Acueducto roto; marcas de puzzle
  en `Extras.markers`, puntos de bifurcación en `required`.

Pendiente de pulido: I03R cabeza y cola (el cerro del túnel acaba en acantilado al mar; ensanchar la tierra de
la cabeza a r ≈ 80 m).

## Cómo continuar

- Cada módulo `lots_*.py` exporta `MAPS = {"N01": ("1", factory)}`; `gen_terrain_inventados.py` los carga solo.
- Generar: `cd Scripts && uv run --with numpy --with scipy --with pillow --with scikit-image --with matplotlib
  python gen_terrain_inventados.py --lot 1` (escribe Variants y láminas, NO toca el índice).
- Criterio del director: camino primero, relieve natural. En Rally usar `rally_circuit.build_rally` (prisma de
  calzada a 31°, `auto_tunnel_m=14` para ≥ 8 m de roca con gálibo 6 m, validador `taludes`). En TcT, la marcha
  del validador es a 20° (`corridor_slope_deg`): cúpulas y conos con pendiente ≤ 18°. Cotas TcT ≤ +15 m.
- Antes de cada commit: `register_maps([...])` de `terrain_shapes/kit_writer.py` (lee-modifica-escribe solo esas
  entradas); comprobar con `git diff index.json | grep name` que no entra nada ajeno; `git add` solo rutas propias.
- Tests: `uv run --with numpy --with scipy --with pillow --with scikit-image --with matplotlib --with pytest
  python -m pytest Scripts/tests -q`.
