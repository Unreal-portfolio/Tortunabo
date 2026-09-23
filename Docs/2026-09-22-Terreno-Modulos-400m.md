# Terreno por módulos de 400 m — diseño e implementación

Fecha: 2026-09-22
Estado: implementado en `feat/procgen-terrain` (fases 1–4); pendiente de playtest visual
Sustituye a: `2026-09-21-Terreno-Stamps-Design.md` (queda vigente solo su fase 4, la
herramienta de autoría en editor) y a `2026-09-21-Terreno-Modular-Design.md`.

## 1. Decisión

Rodrigo fijó el 2026-09-22 el modelo definitivo del terreno del grid:

- La celda mide **400 m** (40 000 uu). El grid sigue siendo configurable (`GridSize`,
  `CellSize` en `ATN_GridMapGenerator`).
- El terreno se compone de **módulos**: heightfields de una celda, generados por script y
  guardados como assets, que los diseñadores usan como plantilla (duplican, retocan,
  bloquean salidas, hacen terraforming).
- Todos los módulos comparten **el mismo perfil de borde**, de modo que cualquier módulo
  casa con cualquier otro, con cualquier rotación, sin costuras ni blending en runtime.
- Librería inicial de **300 módulos**: 50 rectos, 50 curvas a la izquierda, 50 curvas a la
  derecha, 50 cruces, 50 T a un lado y 50 T al otro. Un diseñador convierte una cruz en
  una recta o una curva bloqueando bocas, no esculpiendo el borde.

## 2. Piezas

| Pieza | Fichero | Qué hace |
|---|---|---|
| Generador offline | `Scripts/gen_terrain_modules.py` | 300 PNG de 16 bits (201×201, 2 m por vértice) + `manifest.json` + hojas de contactos. Determinista. |
| Importador | `Scripts/import_terrain_modules.py` | Dentro del editor: crea `DA_M_*` (`UTN_TerrainModuleAsset`) y `BP_M_*` (hijo de `ATN_TerrainModuleTile`) en `/Game/Terrain/Modules/<Topología>/`, y deja `BP_GridMapGenerator` en modo módulos. |
| Asset | `UTN_TerrainModuleAsset` | Topología, semilla, resolución, codificación y alturas (`uint16`). `SetHeightfield` es la única vía de escritura. |
| Actor | `ATN_TerrainModuleTile` | Convierte el asset en `UProceduralMeshComponent` con colisión. Se replica como los chunks; no viaja ningún dato, cada máquina construye desde los Class Defaults. |
| Lógica pura | `TN_TerrainModuleDecisions.h` | Máscaras de salida por topología, rotación, `YawStepsForExits`, muestreo bilineal, `HasCanonicalBorder`, malla. Cubierta por `Tortunabo.TerrainModule.*`. |
| Generador de mapa | `ATN_GridMapGenerator` (modo módulos) | Para cada celda de camino elige un módulo cuya topología, rotada, ofrezca exactamente las salidas que pide `TNGridLogic::ClassifyPath`; las celdas vacías reciben un módulo cualquiera rotado al azar. |

## 3. Contrato del borde

Perfil canónico `p(|t|)`, con `t` la coordenada a lo largo del lado y 0 en su centro:

- `|t| < 18 m`: suelo a cota 0 (boca del pasillo).
- `18..30 m`: talud hasta la cresta de 10 m.
- `60..170 m`: la cresta ondula ±2 m con una función fija (misma en todos los módulos).
- `> 170 m`: cresta plana de 10 m, para que las esquinas de los cuatro lados coincidan.

El perfil es simétrico (`p(t) = p(-t)`), así que también casa bajo rotación y espejo.
El diseño interior se funde con este perfil en una franja de 16 m (`BAND_M`).

Una salida **no usada** se cierra desde dentro: la meseta se fuerza a ≥ 22 m justo tras
la boca, y la franja de 16 m la convierte en una rampa de ~55° que no se puede subir. Dos
módulos vecinos con la boca cerrada forman una pequeña vaguada en la unión: es la única
marca visible del sistema y se lee como un collado.

Regla para diseñadores: **no tocar los 16 m exteriores del heightfield**. Todo lo demás
es libre. `TNTerrainModule::HasCanonicalBorder` lo comprueba en tests; conviene añadir un
validador de assets en el módulo de editor (fase 4 del spec de stamps).

## 4. Interior de un módulo (lo que genera el script)

Alturas en metros; `Z = 0` es el suelo en las salidas.

1. **Pasillo**: polilíneas del centro a cada salida, deformadas por ruido (serpenteo de
   35–65 m) que se apaga a 70 m del borde para que las bocas no se muevan. Semiancho
   18–30 m con variación lenta; en el borde tiende a los 18 m canónicos.
2. **Plazas** (zonas llanas para puzzles): una en el cruce (radio 45–70 m) y, con
   probabilidad 0,7 por salida, otra a mitad de camino (radio 32–58 m). El suelo se
   aplana a la cota de la plaza.
3. **Suelo**: cota que ondula despacio (±4–9 m, longitud de onda 260 m) y vuelve a 0
   cerca de las salidas; arena de ±0,3 m; afloramientos de 1,5–3 m fuera del carril
   central de 8–13 m.
4. **Agujeros**: un pozo por plaza con probabilidad 0,45 (radio 5–8 m, 3,5–6 m de
   fondo) fuera del carril; 0–2 pozos en la meseta (radio 8–16 m).
5. **Paredes**: talud de 6–9 m de ancho hasta la meseta, situada 8–14 m sobre el suelo
   local, con colinas suaves de 12–30 m (3 octavas, longitud de onda 170 m).
6. **Variante calzada** (20 %): el exterior baja a −12 m (bajo el agua, a −4 m) y el
   pasillo queda como una cresta fina. El borde sigue subiendo a la cresta canónica.
7. **Rutas secundarias** (pedidas por Rodrigo el 2026-09-22: "puede haber caminos
   secundarios, sitios que mergean, una curva con un atajo o que sube y luego cruza un
   puente"):
   - *Atajo* (60 % de los módulos): a ras de suelo, semiancho 7–10 m. Entre dos salidas
     perpendiculares es la cuerda que corta la esquina; en las rectas rodea la plaza
     central. Nace y muere en el pasillo principal.
   - *Ruta alta* (55 %, nunca en calzadas): sale del pasillo en perpendicular, sube por
     el talud (rampa de 60 m gobernada por la distancia al núcleo del pasillo), recorre
     la meseta a `wall_h + 2` m y cruza el pasillo por un tramo fijo en el que se
     mantiene alta; vuelve a bajar y entra en el pasillo tras el cruce. Candidatos que
     cruzarían una plaza (tablero > 110 m) se descartan.
   - *Puente*: el hueco que el pasillo abre bajo el tramo de cruce se detecta por
     componentes conexas (dilatadas 4 m, para que una horquilla atajo+pasillo lleve un
     único tablero) y se exporta como `FTNTerrainModuleBridge` (eje por PCA, centro en
     mitad del hueco, 15–110 m de largo, ancho de la ruta + 2 m, 1,2 m de grosor).
     `ATN_TerrainModuleTile` lo coloca como instancia de cubo escalado; el diseñador lo
     puede mover, quitar o sustituir por una malla real en el asset.
   - *Plazas* exportadas como `FTNTerrainModuleFlatArea` (centro, radio, cota) para
     asentar puzzles.
8. Desenfoque final 1-4-6-4-1: sin picos de ruido.

Validación por módulo en el script: borde canónico byte a byte; bajo cada tablero hay
≥ 4 m de hueco y en ambos apoyos el terreno llega al tablero.

Cifras de la tanda actual: 185 atajos, 115 rutas altas, 115 puentes; cota en
[−12,5, 39,3] m; pendiente p99 máxima 75,9° (taludes, por diseño). 15 MB de PNG.

## 5. Convenciones

- Ejes locales del módulo: X = Sur→Norte (avance), Y = Oeste→Este. Coinciden con el grid.
- Topologías sin rotar: recta S–N; curva izquierda S–W; curva derecha S–E; T izquierda
  S–N–W; T derecha S–N–E; cruz N–E–S–W. Yaw +90° lleva Norte a Este (lado `s → s+1`).
- Una curva izquierda girada tres cuartos es una curva derecha: el generador sortea
  entre ambas familias para cada giro del camino.
- Codificación de altura: `Z_uu = (v − 32768) · 0,25` (±82 m, precisión 0,25 uu).

## 6. Muros de basura y cobertura de salidas (2026-09-22, tarde)

- El generador acepta cualquier módulo cuyas salidas rotadas **cubran** las del camino
  (`YawStepsCoveringExits`): una T o una cruz sirven para una recta. Las bocas que no
  conectan con la celda anterior ni con la siguiente (`BlockedExitsLocal`) se tapan;
  los rellenos tapan todas las suyas.
- `ATN_TerrainModuleTile` lleva `BlockedExits` (bitmask de lados locales, editable por
  el diseñador) y `WallSeed`, ambos replicados una vez. Por cada boca tapada levanta un
  montón de 150 piezas de basura (ISM por forma, `BlockAll`, color por instancia) y una
  caja de colisión invisible que garantiza el cierre (`TN_TerrainModuleWallDecisions.h`).
- Librería ampliada a **600 módulos** (100 por topología) con 5 estilos: cañón 209,
  rocoso 123, dunas 84, marisma 90 (charcos bajo el agua), calzada 94. 364 atajos, 245
  rutas altas con puente.

Estado al cerrar la sesión: C++ compila en el target de juego; **falta** compilar el
editor, reimportar los 600, pasar `Tortunabo.TerrainModule.*` (6 tests) y el smoke.

## 7. Pendiente

- Playtest visual en `LVL_ProcGenDemo` (grid 4×4, celda 40 000) y veredicto de Rodrigo
  sobre la lectura del terreno.
- Guía para diseñadores y validador de borde en editor.
- Exponer los centros de las plazas en el asset para asentar puzzles (`ATN_ChunkManager`).
- Coste de carga: 40 401 vértices por módulo, cocinado de colisión síncrono; medir con
  16 celdas antes de subir el grid.

## 8. Fases 3 y 4: rasgos de roca y biomas (2026-09-23)

Decisiones de Rodrigo el 2026-09-23: tres biomas, **arena** (desolada o montañosa),
**agua** ("el típico fuerte o foso que hace un niño con una murallita para caminar") y
**algas** (zonas frondosas, bosque de algas en tierra); cueva = túnel sobre el pasillo;
el agua es poco profunda y se camina (nadar queda para el futuro).

### Rasgos nuevos del generador (F3)

| Rasgo | Dónde | Cómo |
|---|---|---|
| Túnel | Tramo recto del pasillo principal sin plaza lateral (se reserva al decidirlo) | Arco (`kind: "tunnel"`) de 16–32 m a lo largo del pasillo y 4 m de grosor; las paredes deben llegar al techo en los dos extremos. Lo construye `BuildArchMesh`. |
| Monolito | Borde de las plazas o meseta, a ≥ 7 m + radio del eje del pasillo | `FTNTerrainModuleMonolith` (radio 2,5–5 m, 10–26 m de alto, inclinación ≤ 7°), malla de `TNTerrainModule::BuildMonolithMesh`. Pie enterrado 1 m. |
| Plaza hundida | Plazas laterales (la central nunca) | El suelo baja 3,5–6 m con una rampa de 11 m (~28°). Fuera del bioma de agua no baja del agua + 1 m. `flat_areas[].sunken`. |
| Acantilado | Un lado del pasillo de una salida, tramo de 40–70 m | La meseta sube 12–24 m más, el talud se estrecha a 2 m y la roca se escalona cada 3 m. |

### Biomas (F4)

- Estilos por bioma: arena = `desert`, `canyon`, `mountain`; agua = `causeway` (el
  pasillo es la muralla entre un mar de 1–2 m de fondo) y `fort` (foso de agua y
  murallita de 2,2 m alrededor de la plaza central, con puerta donde entra el pasillo);
  algas = `kelp_forest`, `kelp_meadow`.
- Librería: 100 módulos por topología = 85 puros repartidos entre los 3 biomas + 15
  mixtos (5 por pareja). Un mixto toma la forma de su bioma principal y un frente difuso
  lleva hacia el secundario el color, las algas y el mar.
- Máscara por módulo (`<nombre>_mask.png`, 16 bits como el heightfield): byte alto =
  peso del bioma secundario, byte bajo = densidad del bosque de algas. Va al asset
  (`BiomeMask`, `Biome`, `SecondaryBiome`).
- `ATN_TerrainModuleTile`: paleta de color de vértice por bioma
  (`TNTerrainBiome::ColorsFor`), fundida por la máscara en los mixtos; bosque de algas
  sembrado de forma determinista desde la semilla del asset (tallos, hojas y matas en
  tres ISM **sin colisión**, color por instancia con `M_GridJunk`).
- `ATN_GridMapGenerator` (`bUseBiomeRegions`): `PlanPathBiomes` parte el camino en una
  región por cada 4 celdas (máx. 3), con orden de biomas sorteado por la semilla; la
  primera celda de cada región pide un módulo mixto. `BiomeMatchScore` elige el módulo
  que mejor encaja y, si ninguno encaja, cualquiera que ofrezca las salidas. Los desvíos
  heredan el bioma de la celda de la que salen.

Cifras de la librería regenerada (600, 200 m, sustituye a la de 400 m y a la muestra
`ModulesPreview`, ya borrada): 68 túneles, 81 arcos, 139 puentes, 690 monolitos, 101
acantilados, 205 plazas hundidas; cota [−13,2, 57,1] m.

Pendiente: playtest en PIE (lectura de cada bioma, túneles, fuerte), material propio
para las algas (hoy reutilizan el de la basura), y el `ensure` del ISM al construir
muros (previo a este cambio).
