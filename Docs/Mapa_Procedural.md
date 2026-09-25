# Mapa procedural por módulos (World/ProcMap)

Sistema nuevo de generación de mapas: una rejilla de **módulos irregulares de 400 m**
por la que serpentea un camino largo y natural, con biomas por regiones, cruces
colosales (puentes y murallas con puerta que pasan por encima o por debajo de un tramo ya
recorrido), ramas que se vuelven a unir, agua con fauna peligrosa y tres modos de
juego (Coop, Carrera y 2vs2). **Convive** con el sistema de chunks
(`ATN_ChunkManager` + `LVL_Run`), que queda intacto como modo *Clásico*.

---

## 1. Cómo probarlo

1. Compila el proyecto (se añadieron los módulos `ProceduralMeshComponent` y `PCG`
   y ambos plugins en `Tortunabo.uproject`).
2. En el editor, consola Python:
   ```python
   exec(open(r"<repo>/Scripts/build_procmap_assets.py", encoding="utf-8").read())
   ```
   Crea `/Game/ProcMap` (materiales, `DA_ProcMapSettings`, 8 `DA_Biome_*`,
   `BP_ProcMapGameMode`), el nivel `/Game/Maps/Run/LVL_ProcMap` y coloca en
   `LVL_HQ` dos selectores (modo y dificultad) junto a la zona de listos.
   Es idempotente: no pisa assets que ya existan.
3. Desde el lobby: interactúa con los selectores (Clásico → Coop → Carrera → 2vs2;
   Fácil → Normal → Difícil). *Clásico* viaja a `LVL_Run` como siempre; el resto a
   `LVL_ProcMap`. El modo por defecto es Clásico para no cambiar nada hasta que se elija.
4. Sin lobby: abre `LVL_ProcMap` en PIE. Opciones de URL para iterar:
   `open LVL_ProcMap?ProcMode=Race?ProcDifficulty=Hard?ProcSeed=42`
   (`ProcMode` = `Coop` | `Race` | `2v2`). También `FixedSeed` en el GameMode.
5. Previsualizar sin jugar: selecciona `ProcMapGenerator` en el nivel y pulsa
   **GenerateInEditor** (semilla, modo y dificultad en *ProcMap|Editor*). Con
   `bDebugDraw` dibuja camino, ramas y módulos.

Cada generación deja en el Output Log una línea `[ProcMap] Mapa listo · semilla …`
con módulos en ruta, cruces, ramas, **longitud del camino y minutos estimados**
a 5,5 m/s, y los tiempos de cada fase.

---

## 2. Arquitectura

Dos capas, como el resto del proyecto (`TNGridLogic`, `TNChunkLogic`):

**Lógica pura** (`namespace TNProcMap`, solo cabeceras, sin UObjects, determinista):

| Cabecera | Qué hace |
|---|---|
| `TN_ProcMapMath.h` | RNG SplitMix64 con `Fork` por fase, ruido de gradiente, fBm, ridged, utilidades 2D. |
| `TN_ProcMapLayout.h` | Tipos del resultado (`FLayout`): módulos, ruta, portales, cruces, camino muestreado, ramas, features. |
| `TN_ProcMapModules.h` | Módulos irregulares **siempre conexos**: semillas con jitter + Dijkstra multi-fuente sobre coste con ruido, limpieza de conectividad y transformadas de distancia. |
| `TN_ProcMapRoute.h` | Ruta por los módulos: DFS aleatorio con Warnsdorff y poda por alcanzabilidad; reserva los pasos de los cruces colosales (A→B→C sobre un módulo ya visitado). |
| `TN_ProcMapPath.h` | Portales en las fronteras (PCA), "caminante" con meandros senoidales dentro de cada módulo, suavizado Chaikin, anchos por tramos 3,5–60 m, perfil de alturas con límite de pendiente y cortes en géiser/tobogán, ramas y carriles. |
| `TN_ProcMapFeatures.h` | Biomas por regiones (tipo Minecraft), huecos saltables, isletas y pasarelas, pilas de huevos, puzles 2vs2, río opcional, decoración y reparto de peligros. |
| `TN_ProcMapTerrain.h` | Altura por vértice: parámetros mezclados por bioma con *domain warp*, pasillo del camino con arcén y taludes de 55-75°, torres y puertas de los cruces, loma sobre las cuevas, volcanes asentados en el relieve, muros del borde, costa y mar abierto al norte. |
| `TN_ProcMapCaves.h` | Cuevas: tramos del principal que atraviesan una loma por un túnel con pasos estrechos, cámara ancha y, en el volcán, río de lava que se salta. |
| `TN_ProcMapFormations.h` | Formaciones temáticas por bioma: arcos que cruzan el camino, piezas en las explanadas (con carriles libres) e hitos lejanos (naturaleza, entorno y guerra). |
| `TN_ProcMapFlora.h` | Vegetación, rocas y objetos sueltos: especies por bioma y reparto determinista en manchas, también en los taludes. |
| `TN_ProcMapGenerate.h` | `GenerateLayout(params)`: orquesta todo, valida y reintenta. |

Tests de automatización: `Tortunabo.ProcMap.*` (`LayoutInvariants`,
`ModulesConnected`, `Determinism`, `Terrain`) en `Private/Tests/TN_ProcMapDecisionsTest.cpp`.

**Capa UE** (`World/ProcMap`, `Game`, `Lobby`, `Player`):

| Clase | Papel |
|---|---|
| `ATN_ProcMapGenerator` | Traduce el layout a mundo: terreno en tiles de `UProceduralMeshComponent` con colisión y color de vértice, agua (plano + `ATN_ProcWaterVolume` nadable), estructuras colosales, formaciones y techos de cueva (con luces), vegetación procedural (mallas estáticas construidas en ejecución e instanciadas con HISM), capas de props por bioma, grafo PCG opcional por bioma y todos los actores de gameplay. Las mallas low-poly salen de cabeceras privadas sin dependencias del motor (`TN_ProcMapMeshKit.h`, `TN_ProcMapFloraMeshes.h`, `TN_ProcMapFormationMeshes.h`, `TN_ProcMapCaveMeshes.h`, `TN_ProcMapFinishMeshes.h`, `TN_ProcMapPropMeshes.h`, `TN_ProcMapRockMeshes.h`), previsualizables fuera de él. |
| `UTN_ProcMapSettings` / `UTN_ProcBiomeDataAsset` | Slots de datos: perfiles por modo × dificultad, materiales, clases, y por bioma colores, capas de vegetación, peligros y criatura acuática. Sin assets, todo sale en greybox. |
| `ATN_ProcGeyser`, `ATN_ProcSlideZone`, `ATN_ProcKillVolume`, `ATN_ProcFinishVolume` | Conexiones especiales (géiser que sube, cascada-tobogán que baja, un solo sentido y automáticas), caídas mortales y meta. |
| `ATN_ProcWaterVolume`, `ATN_ProcWaterCurrent`, `ATN_ProcWhirlpool`, `ATN_ProcWaterPredator`, `ATN_ProcWaterBouncer` | Agua nadable y sus peligros: corrientes, remolinos, depredador (tiburón/morena) y criaturas con comportamiento de medusa distintas por bioma. |
| `ATN_ProcEggNest` | Pilas de huevos de reaparición en los cruces entre módulos (densidad según dificultad). |
| `ATN_ProcThrowWall`, `ATN_ProcSabotageGate`, `ATN_ProcSwitch` | Puzles del 2vs2: muro que hay que superar lanzando al compañero (o bajando la rampa con el interruptor) y compuertas de sabotaje para la otra pareja. |
| `ATN_PathStorm` | Tormenta del Coop que avanza **por el camino** (progreso en cm), no en línea recta. |
| `ATN_ProcMapGameMode` / `ATN_ProcMapGameState` | Rondas, modos, reaparición en huevos, tormenta, espera a que todos tengan el mapa. |
| `ATN_ProcModeSelector` | Interactuable del lobby para elegir modo y dificultad. |
| `UTN_CarryComponent` | Coger y lanzar tortugas (issue #6, fase 2). |

---

## 3. Qué se genera

- **Rejilla** de `GridSize × GridSize` módulos de 400 m, de forma irregular y siempre
  conexos. El camino recorre `Coverage` de ellos (por defecto ~78 %: 28 de 36 en 6×6).
  Los que quedan fuera se rellenan según `EmptyModuleMode`: *Elevated* (mesetas
  inaccesibles), *BranchesAndScenery* (ramas y paisaje), *Explorable* (terreno
  transitable sin objetivo) o *Mixed*.
- **Camino principal** largo y natural, con anchura por tramos de 40–150 m: desfiladeros
  de 3,5–5 m, pasos cerrados de 6–10 m, tramos normales, anchos de 20–35 m y explanadas
  de 40–60 m, con pocos tramos intermedios (más cañones en desierto y roca, más arenales
  abiertos en la playa). La anchura se recorta para que entre dos partes del camino quede siempre un muro de 18 m.
  El terreno ondula sin tendencia general; los cambios grandes de altura solo
  ocurren al cruzar de módulo, mediante **géiser** (sube) o **cascada-tobogán** (baja).
  Los huecos del camino principal miden 1,3–3,9 m (salto corriendo o con dive).
- **Cruces colosales** tipo Mario Kart: puentes y murallas con puerta altísima que pasan por
  encima o por debajo de un módulo ya recorrido. Se llega a ellos por géiser/tobogán
  y caerse de un puente colosal es mortal. Los puentes dentro de un mismo módulo
  son normales.
- **Ramas** que se separan y vuelven a unirse en 1–3 módulos, de cuatro tipos:
  *tranquila* (larga y holgada), *arriesgada* (cornisa de 3,5–5 m con el doble de
  huecos), *ruta alta* (sube en rampa suave por una loma junto al cauce y baja en
  tobogán al principal) y *rodeo* corto alrededor de un peñasco. Además, hasta un tercio
  de NumBranches en *desvíos* largos por los módulos que el camino no visita: salen del
  principal, pasan por el centro del módulo vacío (que deja de ser macizo; por una meseta
  es un desfiladero) y vuelven al principal en otro módulo o en el mismo; en 2vs2, **carriles**
  paralelos con puzles de lanzamiento y sabotaje. Los huecos de salto no se ponen en
  explanadas y, junto al agua, su zanja es menos honda para no bajar del nivel del mar.
- **Biomas por regiones** de varios módulos contiguos, al azar y con transición
  natural: selva, playa, desierto, volcánico, agua (isletas), acantilados rocosos,
  manglar y zona humana. El último módulo es siempre playa con mar abierto.
- **Playa de la meta**: el camino llega recto a 55-80 m de la costa y sus brazos se abren en
  arco (campana) hasta el agua, así la playa se descubre al avanzar; en la orilla la boca mide
  75-110 m y los brazos son el propio acantilado, que sigue en pie pasada la línea. La **línea
  de meta** cruza toda la boca unos 3,5 m mar adentro (agua por la rodilla): allí empieza el
  volumen de meta. Encima, un **neumático gigante en arco** (al estilo del puente Dunlop) con
  TORTUNABO en los flancos, pasarela a cuadros con el cartel de META, rótulo «¡AL AGUA!» y
  banderas a cuadros; boyas marcan la línea de lado a lado y hay banderines y banderolas en la
  arena.
- **Agua en pozas**: en lagunas y manglar el agua no es un lago abierto sino pozas de
  25-60 m alrededor de cada tramo, con acantilado al borde; entre tramos alejados del
  recorrido (más de 90 m) y junto a la costa queda tierra alta con montañas, así que no se
  puede atajar nadando de un tramo a otro ni hasta la meta.
- **Borde** del mapa con muros altos irregulares que llevan el contenido del bioma.
- **Río** opcional (`bRiver`).
- **Taludes** del camino en rampa de 55-75° (no a plomo): la guarda que impide salir sube en
  rampa desde el borde de cada cauce y solo se empina entre dos cauces próximos (horquillas,
  curvas cerradas), para que la cresta que los separa no sea una rampa andable.
- **Volcanes** asentados en la cota del relieve que los rodea (percentil 75 de un anillo a 3/4
  de su radio) sobre una llanura volcánica: asoman por encima de las montañas.
- **Vegetación procedural** (`bProceduralFlora`, `FloraDensity`, material `M_ProcFoliage`): 27
  formas low-poly × 3 variantes por bioma (ceibas, árboles de copa, palmeras, mangles de
  raíces zancudas, secuoyas jóvenes, cipreses, pinos, abetos, sauces, acacias, árboles secos y
  calcinados, helechos, arbustos, hierba, flores, juncos, saguaros, cactus barril, matojos,
  setos, sombrillas, enredaderas y musgo de pared, peñascos y piedras) en manchas de bosque,
  sotobosque, pradera y pedregal, con tamaños muy variados; en los taludes junto al camino,
  densidad doble y enredaderas pegadas a la pared. En el manglar crecen también dentro de las
  pozas, junto a 16-26 secuoyas gigantes por módulo de tamaños muy distintos.
- **Objetos sueltos junto al camino** (34 tipos × 3 variantes, repartidos como la vegetación en
  rincones de ~25 m): cajas, barriles, vallas de obra, conos, pacas, bancos, farolas, buzones,
  sacos y macetas en la zona humana; conchas, estrellas de mar, cocos, troncos a la deriva, cubos
  y palas, toallas, sombrillas con hamaca, tablas de surf y salvavidas en la playa; setas,
  vasijas, antorchas tiki, postes con calavera y tocones en la selva; calaveras de vaca, huesos,
  ánforas, ruedas de carro, postes indicadores, plantas rodadoras y amatistas en el desierto;
  obsidiana, tocones calcinados, huesos e hitos en el volcán; hitos, cuarzo, cajas, barriles,
  faroles y postes en la roca; nasas, troncos, faroles y tocones en agua y manglar. Los macizos
  (cajas, barriles, pacas, bancos, farolas, buzones, vallas...) llevan colisión de caja. Sustituyen a
  las capas de formas básicas (greybox) cuando la vegetación procedural está activa.
- **Obstáculos de objetos dentro del camino** (18 tipos, con colisión y siempre con carril libre):
  pilas de cajas, barriles, vallas con conos, pacas de paja, castillos de arena, barcas volcadas,
  rincones de playa, tótems, columnas en ruinas, setas gigantes, calaveras gigantes, vasijas,
  cristales gigantes, hitos grandes, vagonetas, nasas, puestos de mercado y filas de conos, según
  el bioma; conviven con peñascos, agujas, mogotes y troncos.
- **Rocas del camino con estilo por bioma**: peñascos redondos, losas inclinadas, partidos,
  apilados, de estratos, columnas de basalto, con musgo, con cristales o de coral; agujas con
  sombrero, inclinadas, gemelas, chimeneas de hadas, pilares kársticos con vegetación y órganos de
  basalto; mogotes, tors de bloques, mesas de estratos y domos de lava.
- **Puentes colosales de cuatro estilos** según el bioma del cruce: colgante de cuerda (selva,
  manglar, agua, playa), viaducto de piedra con arcos rebajados entre los apoyos (roca, desierto,
  zona humana), caballete de madera con vigas hasta el suelo (desierto, playa) y hierro con
  pórticos y cadenas (volcán, zona humana).
- **Formaciones temáticas**: arcos que cruzan el camino (arco de roca, costillar de ballena,
  raíces gigantes, pórtico de templo), piezas en explanadas (barco varado, cabeza colosal,
  basalto, fumarola, chimeneas de hadas, rocas en equilibrio, carreta, cañón y, de guerra,
  sacos terreros, búnker, torre de vigía y carro de combate) e hitos lejanos (pirámide, faro,
  farallones, mesas, castillo en ruinas, molino, palafitos).
- **Cuevas** (2-3 por mapa en volcán, roca, selva y desierto): túneles de 60-150 m bajo una
  loma, con pasos de 4-7 m, una cámara de 16-26 m, estalactitas, cristales o brasas y luz
  tenue; en las del volcán, un río de lava cruza la cámara (se salta; caer mata).
- **Viento** en la vegetación (`M_ProcFoliage`, SimpleGrassWind del motor con rachas): el alfa del
  color de vértice es el peso de balanceo (hierba entera, copas más que troncos, rocas y objetos
  quietos); cada especie deja de evaluarlo a su distancia.
- **Géiseres low-poly**: montículo de sínter en terrazas (anaranjado, crema y blanco) con poza
  turquesa y boca oscura, chorro de agua abultado que pulsa a borbotones, gotas que suben y caen,
  vapor y salpicadura en la boca.
- **Cascadas-tobogán**: lámina de agua con UV de flujo (`M_ProcCascade`, ondas que corren ladera
  abajo), alzada sobre el terreno para no cortarse con él, espuma en los bordes y al pie, y una pocita
  con borde de espuma donde cae, con salpicaduras, espuma que se abre y bruma.
- **Efectos ambientales** (`TN_ProcMapAmbientFX.h`, solo visuales y locales): partículas que son
  instancias de mallas low-poly (gotas, vapor, brasas), dormidas lejos de la cámara; brasas sobre los
  lagos y ríos de lava; bandadas de gaviotas en la costa y la meta, guacamayos en la selva, pájaros
  sobre bosques y roca y buitres en el desierto (`M_ProcBird`, aleteo por el alfa del vértice).
- **Agua animada** (`M_ProcWaterAnim`): ondas en dos capas que se desplazan, color de somera a
  profunda, espuma en las orillas; más clara y rápida en los toboganes.

---

## 4. Modos de juego

| | Coop | Carrera | 2vs2 |
|---|---|---|---|
| Jugadores | 1–4 | 1–4 | exactamente 4 (si no, se juega Carrera) |
| Ronda | todos llegan a la meta; tormenta por el camino | el primero en la meta gana la ronda | gana la pareja cuyos **dos** miembros llegan antes |
| Partida | `CoopRounds` (por defecto 1 = la partida) | primero en `WinsToWinMatch` (3) | victorias por jugador, primero en 3; parejas rotan AB\|CD → AC\|BD → AD\|BC |
| Mapa | largo (3×3 / 6×6 / 8×8) | corto (2×2 / 3×3 / 4×4) | corto con carriles |

- **Reaparición**: morir no elimina mientras haya una pila de huevos alcanzada
  (Coop: la más lejana del equipo; Carrera/2vs2: la del propio jugador) que quede
  por delante de la tormenta. Sin pila válida, muerte normal con rescate de compañero.
- **Rondas**: cada ronda genera un mapa nuevo (`bRegenerateEachRound`) y no arranca
  hasta que todos los clientes avisan de que lo tienen construido (o vence
  `MapReadyTimeoutSeconds`). Entre rondas el flujo pasa a *Countdown* con la cuenta
  atrás en `CountdownValue`; el resultado queda en `ATN_ProcMapGameState::RoundResultText`
  (con el delegate `OnRoundInfoChanged`) para el HUD, que aún no tiene widget propio.
- Carrera y 2vs2 tienen límite por ronda (`CompetitiveRoundTimeLimitSeconds`, 15 min):
  al agotarse gana el más adelantado por el camino.
- La tabla final de Carrera/2vs2 reutiliza el widget de resultados: puesto por
  rondas ganadas, con las victorias en la columna de puntos.

---

## 5. Personaje

- **Nado** básico: `SwimSpeed` 625 cm/s (entre andar y esprintar), flotabilidad 1,08;
  se flota con medio cuerpo fuera. *Saltar* nadando da un impulso para subir a orillas
  e isletas (el movimiento del motor no salta en el agua).
- **Coger y lanzar** (`UTN_CarryComponent`): solo se coge a una tortuga metida en
  su caparazón o aturdida (a cualquiera, también rivales), con *Interactuar* cuando
  no hay otro interactuable delante. *Interactuar* lanza hacia donde mira la cámara;
  *Soltar objeto* la deja delante. Si la llevada intenta moverse 2 s seguidos se
  libera; mientras forcejea al portador le tiembla la cámara y su lanzamiento pierde
  fuerza. En el aire la lanzada no puede salir del caparazón: al tocar suelo rebota
  en vertical, se estira durante el rebote y aterriza de pie.
- **Caídas**: más de 5 m de caída libre → se mete sola en el caparazón; más de 35 m →
  se rompe (muere). Géiseres, toboganes y el agua no cuentan.

---

## 6. Red

Solo se replica `FTNProcMapNetConfig` (semilla, modo, dificultad y número de
generación). Cada máquina genera el mismo mapa en local; los actores que afectan
al movimiento (géiser, tobogán, agua, corrientes, remolinos, zonas de muerte) se
crean en todas las máquinas para que la predicción del cliente cuadre, y los que
tienen estado (enemigos, huevos, puzles, meta, PlayerStarts) solo en el servidor
y se replican. Mientras un cliente no tiene su mapa, su pawn queda congelado; al
terminar avisa con `AMP_GamePlayerController::ServerReportProcMapReady`.

---

## 7. Parámetros por defecto (`TN_MakeDefaultProcProfile`)

Editables en `DA_ProcMapSettings → Profiles` (el script los rellena; *FillDefaultProfiles*
los restaura).

| Modo | Rejilla F/N/D | Cobertura | Cruces F/N/D | Ramas F/N/D | Carriles | Tormenta cm/s F/N/D (gracia s) |
|---|---|---|---|---|---|---|
| Coop | 3 / 6 / 8 | 0,78 | 1 / 2 / 4 | 6 / 12 / 16 | 0 | 300 / 360 / 410 (90 / 60 / 45) |
| Carrera | 2 / 3 / 4 | 0,90 | 0 / 1 / 1 | 4 / 7 / 9 | 0 | — |
| 2vs2 | 2 / 3 / 4 | 0,90 | 0 / 0 / 1 | 2 / 3 / 4 | 1 / 2 / 3 | — |

Comunes por dificultad (F/N/D): densidad de peligros 0,7 / 1 / 1,4; huecos por km
2 / 3 / 4,5; una pila de huevos cada 1 / 2 / 3 cruces de módulo.

> **Duración**: con 400 m por módulo, el Coop 6×6 por defecto sale en torno a
> 35–40 min a 5,5 m/s, por encima de los 10–20 min objetivo. Se dejó así a
> propósito para probar; para acercarse al objetivo basta con bajar `GridSize` a 5,
> `Coverage` o `Sinuosity` en el perfil. El log de cada mapa da los minutos estimados.

---

## 8. Límites conocidos

- **Nanite** no aplica a mallas generadas en runtime (`UProceduralMeshComponent`);
  sí a las mallas de vegetación que se asignen en los biomas.
- El **PCG** es un gancho: si un bioma tiene `PCGGraph`, se ejecuta sobre el mapa
  generado. No hay grafos incluidos.
- Vegetación, formaciones y cuevas son **low-poly procedural** con color de vértice; las
  capas de formas básicas de los biomas quedan para props del borde del camino y de la zona
  humana (o para mallas de arte que se asignen en los DataAssets).
- La **vegetación** no tiene colisión (crece fuera del suelo del camino) y usa culling por
  tamaño; con ~500 mil instancias conviene vigilar el rendimiento en equipos modestos
  (`FloraDensity` la reduce).
- Los materiales `M_ProcFoliage`, `M_ProcWaterAnim`, `M_ProcCascade`, `M_ProcFXSoft` y `M_ProcBird` se
  crean con `Scripts/build_procmap_assets.py` (idempotente; rehace `M_ProcFoliage` si no tiene viento).
