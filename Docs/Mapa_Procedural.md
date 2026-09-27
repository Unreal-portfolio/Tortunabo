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

6. **Probar la tormenta** en cualquier sitio (PIE, consola): `TNStorm <Selva|Playa|Desierto|Volcan|Agua|Rocas|Manglar|Pueblo>`
   lleva la tortuga al tramo más largo de ese bioma y pone el frente de la tormenta 9 m por detrás
   (inofensiva); `TNStorm Geiser` y `TNStorm Cascada` van cada vez al siguiente géiser o cascada del
   mapa; un segundo número cambia la distancia (negativo = ya dentro); `TNStorm Off` la para y la
   devuelve a la normal.
7. **Solo terreno** (para comparar generadores): `ATN_ProcMapGenerator::bTerrainOnly` genera el
   terreno y lo integrado en el camino (cuevas, puentes, murallas, huecos, troncos, obstáculos,
   géiseres y cascadas) sin vegetación, fauna, formaciones decorativas, hitos, recompensas, huevos,
   peligros ni efectos. `ATN_TerrainViewGameMode` (nivel `LVL_ProcMap_Terrain`) lo usa con una tortuga
   sin HUD; `TNRegen [semilla]` vuelve a generar.

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
| `TN_ProcMapTerrain.h` | Altura por vértice: parámetros mezclados por bioma con *domain warp*, pasillo del camino con arcén y taludes de 55-75°, torres y puertas de los cruces, loma sobre las cuevas, volcanes asentados en el relieve, muros del borde, costa y mar abierto al norte. `HeightAtPoint` evalúa la altura en cualquier punto con el mismo campo del camino que el mallado. |
| `TN_ProcMapTerrainDetail.h` | Detalle adaptativo: los cuadrados de 1,5 m en los que la malla se aparta más de 20 cm de la forma real cerca de los caminos (60 cm lejos) se parten a 0,5 m; sus vecinos cosen con un abanico. Mallas de las teselas estancas y altura dibujada en cualquier punto (`SurfaceAt`). |
| `TN_ProcMapCaves.h` | Cuevas: tramos del principal de 120-260 m que atraviesan una montaña por un túnel con pasos estrechos, una o dos cámaras anchas y, en el volcán, río de lava que se salta; prefieren tramos que cruzan terreno alto y se estrechan por un desfiladero hasta la boca. |
| `TN_ProcMapFormations.h` | Formaciones temáticas por bioma: arcos que cruzan el camino, piezas en las explanadas (con carriles libres) e hitos lejanos (naturaleza, entorno y guerra). |
| `TN_ProcMapFlora.h` | Vegetación, rocas y objetos sueltos: especies por bioma y reparto determinista en manchas, también en los taludes. |
| `TN_ProcMapGenerate.h` | `GenerateLayout(params)`: orquesta todo, valida y reintenta. |

Tests de automatización: `Tortunabo.ProcMap.*` (`LayoutInvariants`,
`ModulesConnected`, `Determinism`, `Terrain`) en `Private/Tests/TN_ProcMapDecisionsTest.cpp`.

**Capa UE** (`World/ProcMap`, `Game`, `Lobby`, `Player`):

| Clase | Papel |
|---|---|
| `ATN_ProcMapGenerator` | Traduce el layout a mundo: terreno en tiles de `UProceduralMeshComponent` con colisión y color de vértice, agua (plano + `ATN_ProcWaterVolume` nadable), estructuras colosales, formaciones y techos de cueva (con luces), vegetación procedural (mallas estáticas construidas en ejecución e instanciadas con HISM), capas de props por bioma, grafo PCG opcional por bioma y todos los actores de gameplay. Las mallas low-poly salen de cabeceras privadas sin dependencias del motor (`TN_ProcMapMeshKit.h`, `TN_ProcMapFloraMeshes.h`, `TN_ProcMapFormationMeshes.h`, `TN_ProcMapCaveMeshes.h`, `TN_ProcMapCaveDecor.h`, `TN_ProcMapFinishMeshes.h`, `TN_ProcMapPropMeshes.h`, `TN_ProcMapRockMeshes.h`), previsualizables fuera de él. |
| `UTN_ProcMapSettings` / `UTN_ProcBiomeDataAsset` | Slots de datos: perfiles por modo × dificultad, materiales, clases, y por bioma colores, capas de vegetación, peligros y criatura acuática. Sin assets, todo sale en greybox. |
| `ATN_ProcGeyser`, `ATN_ProcSlideZone`, `ATN_ProcKillVolume`, `ATN_ProcFinishVolume` | Conexiones especiales (géiser que sube, cascada-tobogán que baja, un solo sentido y automáticas), caídas mortales y meta. |
| `ATN_ProcWaterVolume`, `ATN_ProcWaterCurrent`, `ATN_ProcWhirlpool`, `ATN_ProcWaterPredator`, `ATN_ProcWaterBouncer` | Agua nadable y sus peligros: corrientes, remolinos, depredador (tiburón/morena) y criaturas con comportamiento de medusa distintas por bioma. |
| `ATN_ProcEggNest` | Pilas de huevos de reaparición en los cruces entre módulos (densidad según dificultad). |
| `ATN_ProcStartStructure` | Salida de la ronda: la puerta doble o la pila de huevos del lobby (mismo kit, `TN_CastleKit.h`), con los jugadores dentro hasta que se abre (ver «Salida» en el apartado 4). |
| `ATN_ProcThrowWall`, `ATN_ProcSabotageGate`, `ATN_ProcSwitch` | Puzles del 2vs2: muro que hay que superar lanzando al compañero (o bajando la rampa con el interruptor) y compuertas de sabotaje para la otra pareja. |
| `ATN_PathStorm` | Tormenta del Coop que avanza **por el camino** (progreso en cm), no en línea recta, y nunca más rápido que la tortuga andando. Frente con velo translúcido animado (`M_ProcStormVeil`), nubes que ruedan (`M_ProcFXCloud`) y lo que arrastra cada bioma (arena, hojas, brasas y ceniza, espuma, lluvia, polvo, humo y papeles), mezclado en degradado al cambiar de bioma; dentro, la niebla del nivel se cierra y la imagen se tiñe según el bioma del jugador (`TN_PathStormFX.h`). |
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
  El terreno ondula sin tendencia general, con lomas de 0,6–2,2 m cada 50–110 m por todo el
  recorrido (12–22 m de subida por km; nada en el agua, al salir ni en la llegada); las ramas, junto a
  sus uniones, toman la cota del principal. Los cambios grandes de altura solo ocurren al cruzar de
  módulo, mediante **géiser** (sube) o **cascada-tobogán** (baja).
  Los huecos del camino principal miden 1,3–3,9 m (salto corriendo o con dive), con labios de
  madera, sillería o basalto según el bioma; algunos son más largos (hasta 1,8 veces) con **postes**
  en rejilla que los parten en saltos cortos (troncos, pilotes, basalto o columnas) y otros llevan
  **troncos de equilibrio** de labio a labio.
  Desde Normal, parte de los huecos de labios son **saltos de panzazo** (`EGapStyle::Dive`, de
  `DiveGapMin` = 2,7 m a `DiveGapMax` = 3,7 m, sin pasar de los huecos máximos de la dificultad): más de lo
  que da un salto corriendo (2 m) y menos que con panzazo (4 m). En el camino principal siempre hay al menos
  uno (si no sale ninguno, el hueco de labios con la zanja más larga pasa a serlo). En el labio de llegada
  llevan tres chevrones amarillos y rojos que apuntan al hueco y, fuera del camino, un cartel con «!».
- **Torres de escalada** junto al borde en tramos anchos: bloques del bioma (cajas con aspa,
  tocones, sillares, losas o basalto) de 3–4 m con escalones de 1 m, banderín, recompensa de puntos
  arriba (`BP_ScorePickup`) y una medusa al pie (`BP_JellyfishActor`) para subir de un bote.
- **Conchas de puntos** (`ATN_ScorePickup`): una vieira dorada de ~1 m que gira como una moneda
  de plataformas clásico, sube y baja y brilla (`M_ProcGlow`), con destellos alrededor. Si arte
  pone una malla propia en `PickupMesh`, se ve esa y la concha no (la de ayuda del motor, el
  signo de interrogación, cuenta como vacía).
- **Terreno**: malla de 1,5 m con **detalle de 0,5 m** donde hace falta (pie y borde de los taludes,
  crestas, bocas de cueva: un 3-4 % de los cuadrados, más un 5-6 % de costuras; 1,4-1,5 veces los
  triángulos). Las paredes suben sin repisa: el talud llega al borde con la pendiente con la que arranca
  la subida, la pared del cañón sube desde el borde del cauce y, cerca de los caminos, la distancia al
  cauce es la exacta (no la rejilla de 10 m). El pie, el ancho del talud y la subida varían a lo largo del
  camino (ruido de 17-30 m). `DetailSpacing` y `DetailError` en los ajustes.
- **Color del camino**: un color de sendero propio de cada bioma, de tono y luminosidad
  claramente distintos de sus paredes (tierra anaranjada, barro claro, ceniza rojiza, arena mojada,
  arcilla roja, grava ocre, adoquín pizarra, tablas oscuras), con una línea oscura al pie del talud;
  las paredes, en degradado por pendiente (suelo, roca y roca más oscura en los tajos) con estratos
  suaves. La playa de la meta conserva su arena.
- **Cruces colosales** tipo Mario Kart: puentes y murallas con puerta altísima que pasan por
  encima o por debajo de un módulo ya recorrido. Se llega a ellos por géiser/tobogán
  y caerse de un puente colosal es mortal. Los puentes dentro de un mismo módulo
  son normales. Sus dos torres son de sillería en talud con pretil y almenas: la de
  entrada es **hueca**: el camino llega en embudo a su puerta, a ras de suelo: un túnel
  recto de 4,6 m de ancho que atraviesa todo el grueso del muro, con bóveda de medio punto,
  suelo enlosado, portada plana al pie del talud con impostas, dovelas y clave en relieve,
  rastrillo levantado y dos antorchas. Dentro (sala iluminada por antorchas) el géiser del
  centro lanza en vertical por un hueco del forjado hasta la cima, junto al arranque del
  puente o del adarve, y su columna de agua asoma por ese hueco; la de salida lleva el
  tobogán. Las dos cimas van **enlosadas** (malla, `TowerDims::PaveLift` = 4 cm por encima
  de la cota de la torre), así que el núcleo de terreno y el tablero o el adarve que entran
  en ellas quedan debajo y no parpadean. Malla y terreno abren los mismos lados del pretil
  (`TowerOpenSides`); el núcleo quita su pretil de roca también en los 70 cm de lado cerrado
  junto a uno abierto (`TowerCoreOpenAt`), para que la rampa entre las dos cotas quede dentro
  del pretil de sillería. En la de salida, por los lados abiertos la sillería baja a plomo a
  60 cm del núcleo (`TowerDims::FlushOut`) y el terreno de alrededor no sube de la cima.
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
  TORTUNAVY en el flanco que se ve al llegar (y, de guiño, el nombre en clave TORTUNABO en el
  que mira al mar), pasarela a cuadros con el cartel de META, rótulo «¡AL AGUA!» y
  banderas a cuadros; boyas marcan la línea de lado a lado y hay banderines y banderolas en la
  arena.
- **Agua en pozas**: en lagunas y manglar el agua no es un lago abierto sino pozas de
  25-60 m alrededor de cada tramo, con acantilado al borde; entre tramos alejados del
  recorrido (más de 90 m) y junto a la costa queda tierra alta con montañas, así que no se
  puede atajar nadando de un tramo a otro ni hasta la meta.
- **Borde** del mapa con muros altos irregulares que llevan el contenido del bioma.
- **Río** opcional (`bRiver`). Sus puentes de madera están **rotos** 3 de cada 4 veces cuando miden más de 16 m y
  debajo hay agua de verdad: les falta el centro (7 m, no se salta; bordes astillados y bandas de aviso) y hay que
  rodear por el agua. Se baja por el hueco, se va por las **piedras** (cima a 35 cm sobre el agua, a saltitos de
  menos de medio metro) o nadando hasta una **escalera de madera** pegada al puente por un lado (peldaños de 30 cm y
  42 de huella, puntales hasta el lecho) y se vuelve al tablero por un rellano, con la barandilla abierta ahí. La
  escalera se coloca donde la orilla no la entierre; si no cabe, el puente queda entero.
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
  pórticos y cadenas (volcán, zona humana). Las pilas y caballetes nunca caen sobre otro camino (el
  arco se une al siguiente) y las cuevas no se ponen bajo un tablero colosal. Los de piedra y hierro
  tienen a media altura una **plaza** redonda (sobre la pila central si la hay): pretil o barandilla
  abierta a las entradas del tablero, fuente con la tortuga o farol alto, farolas, bancos mirando al
  paisaje y una atalaya de 3 m con escalones, recompensa arriba y medusa al pie.
  Los de más de 42 m entre torres tienen un **tramo hundido** de 11-15 m sin tablero (`TNProcAddBrokenSpan`), solo
  donde debajo no hay nada en 9 m (ni pilas ni torres), todo el tramo cae sobre cajas de muerte y no hay nada del
  recorrido a menos de 5 m (plaza, huevos, recompensas, medusas). Se cruza de una de tres maneras: **vigas** de 60 cm
  en zigzag de lado a lado con plataformas en los codos, **postes** cuadrados de 1,1 m al tresbolillo (saltos de
  ~1,3 m y cimas alternas a -8 y -26 cm) o dos **cornisas** de 60 cm por los bordes, cada una con un hueco de 1,8 m (a
  un tercio y a dos tercios) y un tablón atravesado en medio para cambiar de lado. Todo lo pisable queda a menos de
  50 cm bajo el tablero (las cajas de muerte empiezan a 60 cm). Bordes astillados (sillares en los de piedra;
  tablones que cuelgan en los demás), barandillas cortadas y bandas de aviso antes de cada borde. El registro dice
  «Cruce N: tramo hundido de X m (tipo K)».
- **Formaciones temáticas**: arcos que cruzan el camino (arco de roca, esqueleto de ballena con
  columna en arco sobre las costillas, cola y cráneo con mandíbulas,
  raíces gigantes, pórtico de templo, tronco colosal caído con raíces y lianas en selva y manglar,
  acueducto en ruinas en desierto, roca y zona humana, aquí la mitad de las veces), piezas en
  explanadas (barco varado, cabeza colosal, basalto, fumarola, chimeneas de hadas, rocas en
  equilibrio, carreta, cañón, caracola gigante y ancla en la playa, círculo de piedras que se cruza
  entre sus piedras, obelisco y cráneo fósil en el desierto, agujas de obsidiana en el volcán, tortuga
  colosal de piedra, depósito de agua y, de guerra, sacos terreros, búnker, torre de vigía y carro de
  combate) e hitos lejanos (pirámide, faro, farallones, mesas, castillo en ruinas, molino,
  palafitos). La pieza de explanada se sortea primero y espera hasta 40 m a un tramo donde quepa, así
  las grandes salen en los anchos.
- **Cuevas** (1-5 por mapa en volcán, roca, selva y desierto): túneles de 120-260 m, donde se
  puede en tramos que cruzan terreno alto (el paisaje a 30 m de los dos bordes, 12 m por encima
  del camino). Encima, una montaña de cima irregular que crece hacia el centro de las largas. El terreno no
  puede tener techo, así que sobre el túnel una **tapa de montaña** une las laderas de los dos lados a su altura
  (a 7 m del borde del paso), con una loma y los colores del bioma, donde la montaña queda por encima del techo de
  roca. Así la montaña sigue por encima de la cueva en vez de quedar cortada a lo largo del camino. A cada
  boca se llega por un desfiladero de 25-40 m de paredes a plomo que pierde altura hasta 7 m y acaba
  en frente empinado (a la montaña no se sube). El camino se estrecha a 11-15 m en la boca; dentro,
  pasos de 4-7 m y una o dos cámaras de 16-26 m; en las del volcán, un río de lava cruza la primera
  (se salta; caer mata). Interior según el estilo (`TN_ProcMapCaveDecor.h`): **caliza** (estalagmitas,
  columnas, estalactitas grandes, poza, lucernario), **cristales** (racimos gigantes que brillan, con
  colisión), **selva** (raíces por la bóveda, lianas, setas luminosas, musgo, cortina de lianas en las
  bocas), **templo** (pilares, pilastras, antorchas con brasas, vasijas y huesos, portada tallada con la
  cabeza de la tortuga) y **tubo de lava** (obsidiana, basalto, grietas incandescentes). En todas, la
  estatua de la tortuga con ofrendas en la cámara más ancha, estelas y símbolos (tortuga, espiral,
  sol, olas, ojo, panal) pintados o luminosos, y luces sin sombras del color del estilo. Nada invade
  el carril central. Las del volcán, donde no pisa otros caminos ni torres, van **dentro de un volcán**
  de 70-150 m de base con su cráter y lago de lava (el túnel atraviesa su base y los desfiladeros se
  abren en su ladera) y llevan una **cámara de magma**: lago de lava a un lado del camino (mata al
  tocarlo) con anillo de basalto, coladas encendidas por la pared, grieta en la clave, brasas y luz
  fuerte; la estatua, al otro lado.
- **Viento** en la vegetación (`M_ProcFoliage`, SimpleGrassWind del motor con rachas): el alfa del
  color de vértice es el peso de balanceo (hierba entera, copas más que troncos, rocas y objetos
  quietos); cada especie deja de evaluarlo a su distancia.
- **Géiseres low-poly**: montículo de sínter en terrazas (anaranjado, crema y blanco) con poza
  turquesa y boca oscura. La columna de agua (material de agua que corre hacia arriba, blanca y
  opaca en lo alto) sube de golpe hasta 10,5 m, se sostiene temblando, baja y queda borboteando a
  2,6 m, en ciclos de 4,2 s; al lanzar a alguien vuelve a arrancar. Corona de espuma que va con la
  cima y anillo de espuma en la boca (bolas de caras planas), gotas que saltan de lo alto y caen
  alrededor, gotas que suben pegadas a la columna, espuma arriba y abajo, salpicaduras y bruma,
  todo al ritmo del chorro. En la torre hueca la columna llega a asomar por el hueco del forjado.
- **Cascadas-tobogán**: lámina de agua con UV de flujo (`M_ProcCascade`, ondas que corren ladera
  abajo), en rejilla de 30 × ~60 cm con cada vértice 25 cm sobre el punto más alto del terreno de sus
  cuadros vecinos (el terreno nunca asoma), espuma en los bordes, en el labio (el primer metro y
  medio, con espuma y gotitas que se asoman) y al pie. Abajo, una poza pegada a la cascada
  (`TNProcMap::SlidePoolOf`, la misma cuenta para el terreno, la malla y los efectos): el agua
  queda a ras del suelo donde llega la lámina (12 cm por debajo) y el fondo baja un metro justo donde
  cae la tortuga y sube suave hasta la orilla (`PoolDims`, influencia `Pool` del terreno). Sus UV
  salen del punto donde cae el agua, así que las ondas del material corren desde el impacto hacia
  fuera, y además salen anillos de onda que se abren desde ahí y se hunden al final; salpicaduras,
  espuma y bruma, todo a la cota del agua.
- **Efectos ambientales** (`TN_ProcMapAmbientFX.h`, solo visuales y locales): partículas que son
  instancias de mallas low-poly (gotas, vapor, brasas), dormidas lejos de la cámara; brasas sobre los
  lagos y ríos de lava; bandadas de gaviotas en la costa y la meta, guacamayos en la selva, pájaros
  sobre bosques y roca y buitres en el desierto (`M_ProcBird`, aleteo por el alfa del vértice).
- **Agua animada** (`M_ProcWaterAnim`): ondas en dos capas que se desplazan, color de somera a
  profunda, espuma en las orillas; más clara y rápida en los toboganes.
- **Fauna** (`ATN_ProcFauna`, `TN_ProcMapFaunaMeshes.h`; solo visual y local): 31 especies low-poly
  por bioma (monos, tucanes y ranas en la selva; cangrejos, gaviotas y tortuguitas en la playa;
  suricatos, lagartijas y correcaminos en el desierto; salamandras y escarabajos de fuego en el
  volcán; peces, pelícanos y flamencos en el agua; cabras y águilas en la roca; garzas y cangrejos
  violinistas en el manglar; gatos, palomas y gallinas en la zona humana), hasta 900 a la vez
  (`Density` 2,6).
  - Van pegadas a los caminos: bastantes en el propio camino y, de las demás, dos de cada tres a menos de 9 m de
    su borde. El resto queda hasta 25 m (60 m las de agua).
  - Dejan acercarse a la mitad de su distancia de alarma de la tabla y huyen al 75 % de su velocidad, así que se
    las ve escapar: trepan paredes, vuelan, se entierran o se meten en el agua.
  - Reaparecen por delante. Consola: `TN.Fauna.Enable`, `TN.Fauna.Stats`.
- **Tos en la tormenta** (`UTN_StormCoughComponent`, sintetizada, sin archivos de audio). Cada tortuga que está
  dentro de la tormenta tose, y cada una tiene su voz.
  - Al entrar, carraspeos sueltos.
  - Con el tiempo dentro, ataques de tos cada vez más seguidos, con jadeos. Al salir, un último carraspeo. Al
    morir, calla.
  - `ATN_PathStorm::TickCough` le pasa cada 0,1 s si está dentro y cuánto le falta para morir.
  - Prueba sin tormenta: `TN.Storm.Cough <0|1|2>` (apagado, carraspeo o tos fuerte).
- **Sonido ambiente sintetizado** (`TN_AmbientSynthComponent`, `UTN_AmbientSoundscapeComponent` en
  el PlayerController; sin archivos de audio): capas por bioma (viento, oleaje, aves, cigarras,
  grillos, ranas...) que cambian en degradado, tormenta, cuevas amortiguadas, y fuentes 3D en los
  géiseres (siguen el chorro), las cascadas y la lava. Para sustituirlo por sonidos de verdad:
  `TN_AmbienceDataAsset`. Consola: `TN.Ambience.Debug`, `TN.Ambience.Volume`.
- **Música de fin de partida** (`UTN_MatchMusicSubsystem`, sintetizada en `TN_MusicSynthDSP.h`). Suena en 2D para el
  jugador local.
  - Tres pistas: victoria (si bemol mayor, 120 BPM), derrota (re menor, 72 BPM) y una cortinilla de eliminado.
  - Las decide `TNMatchMusic::FDirector` (`TN_MatchMusicDirector.h`) a partir de estados que ya se replican: flujo de
    la partida, resultados, llegada, eliminación y rondas ganadas. No hay RPC.
  - Se funde a silencio al final de la cuenta atrás de resultados y se para al cambiar de mapa.
  - Consola: `TN.Music.Play Victoria|Derrota|Eliminado|Tienda|Probador|Silencio` y `TN.Music.MatchVolume`.

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

### Salida: puerta doble o huevos

Cada ronda empieza dentro de la misma pieza en la que los jugadores se pusieron listos en el lobby
(`ATN_ProcStartStructure`). Se construye con `TNCastleKit` (`Private/Lobby/TN_CastleKit.h`), el kit de
`ATN_SandCastleLobby`, así que la geometría es idéntica.

- **Puerta doble** (`ETNMatchStartStyle::Gate`): la sala entre dos puertas, al fondo del claro de salida, con la
  puerta 1 contra el talud (parece que se sale de la pared) y la puerta 2 mirando al camino. Muros, pilares, torres y
  zócalo bajan 4,5 m bajo el suelo por si el terreno no es plano. Hay cuatro sitios dentro. Al abrirse, las hojas de
  la puerta 2 giran 100° hacia fuera en 1,25 s y su bloqueo invisible desaparece en cuanto empiezan a girar; la
  puerta 1 no se abre nunca. Lleva el rótulo «TORTUNAVY» en la cara de fuera de la puerta 2 y una luz cálida en la sala.
- **Huevos** (`ETNMatchStartStyle::Eggs`): el montículo de dos alturas con la pila de cuatro huevos y el escalón hacia
  el camino. Cada jugador aparece dentro de un huevo con la tapa puesta; una pared invisible lo sujeta hasta que se
  rompe. Los huevos se rompen uno tras otro, cada 0,12 s: la tapa salta dando vueltas, se posa y se esfuma, y la
  tortuga sale despedida (`LaunchCharacter`, 3,8 m/s en horizontal, hacia fuera de la pila y hacia el camino, y
  6,2 m/s hacia arriba). El salto lo dan a la vez el servidor (a todas) y el cliente dueño al recibir `bOpen`, como en
  el probador.

**Colocación** (`ATN_ProcMapGenerator::SpawnStartStructure`, servidor, en cada generación). «Hacia el camino» es la
dirección del punto de salida a la primera muestra del camino que queda fuera del claro. La estructura se coloca
detrás, con su +Y local mirando al camino:

- Puerta doble: el umbral de la puerta 1 a `StartClearingRadius − 1,5 m` del centro del claro.
- Montículo: su centro a `min(StartClearingRadius − 6,5 m, 13 m)`.
- Cota: la más alta del terreno bajo el suelo de la sala (el terreno no asoma por él) o la más baja bajo el
  montículo (no queda flotando).

Se crea diferida, con el estilo puesto antes de su `BeginPlay`. Va en `SpawnedActors`, así que se destruye al
regenerar, como las pilas de huevos. No se crea en modo solo terreno ni si ningún GameMode la pide
(`SetStartStructureStyle`).

**Aparición.** Con estructura, `GetStartTransform(0..3)` y los PlayerStart 0–3 (etiqueta `TNProcStart`) quedan
dentro de ella, a 1,1 m del suelo como los del anillo; del quinto jugador en adelante se usa el anillo del claro.

- `ChoosePlayerStart` da a cada jugador el sitio de su slot (índice en `PlayerArray`) o, si está ocupado, el
  siguiente libre de la estructura (durante el viaje sin cortes los slots aún se reordenan).
- Al empezar la ronda, `PlacePlayersAtStart` lo deja de pie en su sitio con la altura de su cápsula
  (`GetSpawnTransform`). Hasta entonces sigue congelado, como siempre.
- En el cliente, el suelo de la estructura cuenta como suelo del mapa para soltar el peón (`MapCollisionUnder`).
- Sin estructura, todo funciona como antes.

**Apertura.** `BeginRoundPlay` programa `ATN_ProcStartStructure::Open` a `StartStructureOpenDelaySeconds`
(1,2 s), a la vez que el «¡ADELANTE!» de la pantalla de carga. Solo se replican el estilo y `bOpen`. Quien recibe
la estructura ya abierta la ve abierta del todo y no salta. Una ronda nueva sin regenerar el mapa la cierra
(`Close`).

**Del lobby al mapa.** `ATN_HQGameMode::BeginMatchTravel` guarda `ATN_SandCastleLobby::GetStartStyle()` en
`UMP_GameInstance::PendingStartStyle` y añade `?ProcStart=Gate|Eggs` a la URL del viaje. `GetStartStyle()` elige
según dónde haya más jugadores listos, en la sala o en los huevos; si empatan, la puerta.
`ATN_ProcMapGameMode::ResolveStartStyle` mira en cada generación la GameInstance y después la URL (sin nada, la
puerta doble). El resultado queda en el log: `[ProcMap] Salida: puerta doble` o `huevos`.

**Para probar.** La variable de consola `TN.Proc.StartStyle` manda sobre todo lo anterior y vale desde la siguiente
generación: −1 = lo del lobby (por defecto), 0 = puerta doble, 1 = huevos. Sin lobby también sirve
`open LVL_ProcMap?ProcStart=Eggs`.

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
| Coop | 3 / 6 / 8 | 0,78 | 1 / 2 / 4 | 6 / 12 / 16 | 0 | 160 / 180 / 200 (90 / 60 / 45), como mucho la velocidad de andar |
| Carrera | 2 / 3 / 4 | 0,90 | 0 / 1 / 1 | 4 / 7 / 9 | 0 | — |
| 2vs2 | 2 / 3 / 4 | 0,90 | 0 / 0 / 1 | 2 / 3 / 4 | 1 / 2 / 3 | — |

Comunes por dificultad (F/N/D): densidad de peligros 1,6 / 2,4 / 3,2; huecos por km
6 / 9 / 12 (a 40 m como mínimo entre sí); una pila de huevos cada 1 / 2 / 3 cruces de módulo. El camino va
muy poblado de saltos, trampas y obstáculos de juego. `DA_ProcMapSettings` guarda sus perfiles: al cambiar estos
valores en código hay que actualizarlos en el asset (`FillDefaultProfiles` o por Python) y guardarlo.

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
- Los materiales `M_ProcFoliage`, `M_ProcWaterAnim`, `M_ProcCascade`, `M_ProcFXSoft`, `M_ProcGlow` y `M_ProcBird` se
  crean con `Scripts/build_procmap_assets.py` (idempotente; rehace `M_ProcFoliage` si no tiene viento).
