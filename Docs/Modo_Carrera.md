# Modo carrera en la playa

Rama `claude/modo-carrera` (sale de `claude/elegant-fermi-6n1hxy` en `5153406b`): se puede descartar sin tocar el
cooperativo. El cooperativo (lobby del castillo y mapa procedural) sigue igual.

## Bucle de juego

1. **Menú principal**: en vez de solo «Crear partida», se elige el modo: **Cooperativo** (lo de siempre: lobby del
   castillo y mapa procedural) o **Carrera**. El modo va en `UMP_GameInstance::SelectedProcMode` (`Coop` o `Race`).
2. **Lobby**: el mismo (`LVL_Lobby`, pantalla de carga, huevos y puerta doble). El anfitrión puede cambiar ahí el modo y
   la dificultad hablando con el General Galápago (pestaña «Misión»). Al ponerse todos listos, el lobby viaja según el
   modo: Cooperativo → `LVL_ProcMap`; Carrera → `LVL_BeachRace`.
3. **Carrera** (`ATN_BeachRaceGameMode`, `ATN_BeachRaceGameState`): todos contra todos en la playa. Gana la ronda quien
   primero toca el agua tras saltar el acantilado de la orilla y se lleva una **concha** entera. Desde ese momento,
   **cuenta atrás de 10 s** para todas: quien llegue dentro se lleva **media concha**; al acabar, «¡TIEMPO!» y a cada
   una que no ha llegado le sale de la arena un **gusano gigantesco que se la come** (si ya han llegado todas, «¡TODAS AL
   AGUA!» sin gusanos). Las conchas van en medias (`ATN_CoopPlayerState::RaceShellHalves`). En carrera **no se muere**:
   lo que en el cooperativo mata, aquí aturde (bola de caparazón temblando unos segundos).
4. **Recuento** tras cada ronda (`ETNBeachRacePhase::RoundResults`): las caras de los jugadores, cada una con tres
   huecos de concha en zigzag (como en los juegos de preguntas; cada hueco, dos medias); vuela la concha entera de la
   ganadora y saltan las medias de la cuenta atrás. Luego, otra ronda con los elementos recolocados (el terreno es
   siempre el mismo).
5. **Sprint final** (`ETNBeachRacePhase::SprintIntro`) si al cerrar una ronda hay empate en lo más alto con tres conchas
   o más: título «¡SPRINT FINAL!» y una ronda corta solo para las empatadas, que salen como en cada ronda, del nido de
   huevos, pero llevado a mitad de la playa; la primera en el agua es campeona. Las demás lo miran de fantasma.
6. **Campeón** (`ETNBeachRacePhase::Champion`) al llegar una sola a tres conchas (o al ganar el sprint): menú distinto con **Volver a jugar**,
   **Cambiar de modo** y **Salir** a la izquierda y, a la derecha, un fondo animado: las tortugas en un podio hecho de
   basura de la playa. La primera levanta la concha como un trofeo con las dos manos; la segunda, decepcionada; la
   tercera, sentada en el podio, enfadadísima y pataleando. Movimientos cortos en bucle, como un GIF.

## El mapa (`LVL_BeachRace`, `ATN_BeachRaceGenerator`)

- **Escala**: la tortuga es una cría de ~5 cm; todo va a `TNBeach::Scale` = 28 veces su tamaño real.
- **Terreno fijo** (siempre el mismo): un único tramo recto de playa de `TNBeach::CourseLength` = 1200 m por
  `TNBeach::CourseWidth` = 280 m jugables, calculado para ~5 min: andando a 4,5 m/s y esprintando a 8 m/s (13 s por
  barra de energía), con un 40 % de esprint y los obstáculos, sale una media de ~4 m/s. Toda de arena, bajando hacia el
  mar (siempre se ve la meta) con un relieve irregular: dunas con cresta (algunas con una cornisa que se salta o un
  collado para pasar), corredores más bajos que se separan y se juntan, charcas y pozas de marea que se nadan y dos
  líneas de trincheras en zigzag.
- **Salida**: una fila de cuatro huevos (como la salida del cooperativo); cada tortuga espera dentro del suyo y, al dar
  la salida, las tapas saltan y sale lanzada hacia el mar, ya corriendo. Nada del reparto en los primeros 60 m.
- **Meta**: un acantilado de rocas al final de la playa, de unos 55 cm reales (`TNBeach::CliffHeight` = 15,5 m en el
  juego, 5-6 veces la tortuga): se salta desde el borde y se cae al agua, donde flotan las banderas de meta; se gana al
  tocar el agua. El último salto, desde la zona del borde, se hace de cabeza (zambullida).
- **Bordes**: a los lados y detrás, selva de palmeras y árboles enormes (a escala: una palmera de 10 m mide 280 m).
- **Pasarelas**: la pasarela de madera vieja y el caminito de palos con cuerda son elementos del reparto procedural
  (`Boardwalk`, `WoodenPostPath`), repartidos por la playa y a veces como guía visual hacia el mar; no marcan la
  salida ni la meta.
- **Reparto procedural** (cada ronda, con semilla): decorado, trampas y enemigos de `ETNBeachElement`
  (`Public/World/Beach/TN_BeachTypes.h`), ~1000 por ronda, cada uno con su huella, sin solaparse, dejando siempre paso
  (aunque sea sinuoso) y sin ninguna línea recta libre hacia el mar: hay que cambiar de rumbo sin parar.
- **Decorado gigante** (`ATN_BeachDecor`): cocos, medusas varadas, anillas de latas cortadas, un sujetador rojo,
  almejas, conchas y estrellas de adorno, rocas, restos de una vela de barco, troncos con musgo, tablones viejos, redes
  de pesca, vasos, botellas, chupachups, cortezas de sandía roídas, pajitas, sombrillas clavadas y sillas de playa del
  día anterior, castillos de arena pequeños y enormes, madera a la deriva. Y lo de la tropa de Tortunavy: parapetos de
  sacos terreros, cajas de munición, erizos antitanque, cascos tirados, redes de camuflaje sobre palos, bidones y
  soldaditos de juguete verdes.
- **Trampas e interacciones**: alambre de espino, algas que enredan, plataformas sobre hoyos que se tambalean y se
  rompen con más de una tortuga encima, cubos rotos por los que se pasa, palas que hacen de trampolín o de puente con un
  compañero, un castillo de arena enorme con salas por dentro que hay que atravesar, puertas de conchas, minas de
  juguete medio enterradas que explotan al pisarlas y te lanzan en bola unos metros hacia atrás.
- **Enemigos y amenazas**: cangrejos gigantes que patrullan sin parar, te ven de frente y te oyen alrededor, te
  persiguen y dan un mazazo con la pinza (te dejan despachurrada en bola, aturdida); erizos de mar grandes que ruedan
  despacio hacia ti (pinchan: derribo con ragdoll y mareo); lagartos que pasean, asustan y se esconden; quads enormes
  que cruzan la playa de lado a lado entre palmeras (temblor de pantalla, nube de humo, ruedas anchísimas: o fuera de su
  paso o en el hueco entre las ruedas; si te pillan, ragdoll lanzado); gaviotas y pelícanos que rondan por arriba, cada
  uno en su círculo (te cagan encima: ragdoll y mancha; bajan en picado con su sombra creciendo: esquiva con el panzazo;
  o te cogen con el pico, cuelgas pataleando, te suben y te sueltan: en bola al caer); la tormenta de bañistas por
  detrás, a ras de arena y más lenta que la carrera, con sombrillas, cubos y sillas de playa volando.

## Terreno, nivel y reparto (`ATN_BeachRaceGenerator`, `TN_BeachLayout.h`)

Archivos: `Public/World/Beach/TN_BeachLayout.h` (lógica pura: terreno fijo con su relieve, salida, sprint, meta y
reparto, con sus tests en `Private/Tests/TN_BeachLayoutTest.cpp`), `Public/World/Beach/TN_BeachRaceGenerator.h` y, en
`Private/World/Beach/`, `TN_BeachRaceGenerator.cpp` (rondas, consultas y meta), `_Build.cpp` (arena, roca, mar, muros y
agua), `_Features.cpp` (trincheras, cornisas, rocas de las pozas de marea y agua de las pozas), `_Start.cpp` (tapas de
los huevos de la salida y lanzamiento), `_Scenery.cpp` (salida, meta, selva y sus huecos, huellas y chapuzón) y
`TN_BeachRaceKit.h`; el nivel, `Scripts/build_beach_race.py`. Fuera: un cambio mínimo de sombras en
`Lobby/TN_LobbyValley.cpp` (ver «Sombras») y la llamada a `OpenStartEggs` en `ATN_BeachRaceGameMode::BeginRace`.

### Medidas (espacio del generador en cm: X hacia el mar, Y a lo ancho, el agua en Z = 0)

- **Recorrido**: línea de salida en X = 0 y filo del acantilado en X = 1200 m (ondula ±2,5 m a lo ancho). Playa jugable
  `|Y| <= 140 m`. Muros invisibles a 148 m a cada lado (también en el agua), detrás de la salida (X = -32 m) y mar
  adentro (X = 1500 m), de -200 a +1200 m de alto.
- **Perfil**: 51,5 m sobre el agua en la salida y 15,5 m en el filo: cae 36 m como `(1 - t)^1,5` (4,5 % al principio,
  casi llana al final). Con el relieve encima, desde la salida se sigue viendo el mar por encima del filo (4,9 m de
  margen como poco). Rejilla de 3 m en la playa (hasta 18 m lejos) en 55 teselas de 40 x 40 casillas; con colisión, las
  que quedan a tiro. `M_ProcTerrain` con relieve y, en la arena (alfa 1), grano, guijarros y marcas del viento; tierra y
  hojarasca en los bancos; arena mojada alrededor de las pozas y húmeda en el fondo de las trincheras.
- **Relieve** (`ReliefZ`, fijo, sobre el perfil): entra entre 35 y 170 m y se apaga en los últimos 90 m antes de la
  roca. Dunas de 2,8 m de amplitud en el centro a 4,8 m junto a la selva (lomos, bultos y nudos) y ondulación a tres
  escalas (1,7 m cada ~130 m, 70 cm cada ~43 m y 25 cm cada ~16 m), más los corredores, las crestas, las pozas y las
  trincheras. Medido (con el puerto a Python del terreno): desviación típica 1,23 m (antes 0,53), de -5,6 a +4,3 m sobre
  el perfil, pendiente máxima 35° en rejilla de 8 m (~39,5° junto a las crestas) y el 23 % de la playa con más de 10°.
  Todo se anda salvo las cornisas; lo empinado son las caras de las crestas que miran a la salida.
- **Corredores** (`CorridorAt`, 3): dos caminos más bajos que se separan (hasta 54 m entre ejes; 61 m con el tercero)
  y se vuelven a juntar, y un tercero en medio por tramos. 1,7 ± 0,7 m más hondos que lo de alrededor, con las dunas
  apagadas dentro: el camino natural (y el de algunos campos de minas).
- **Crestas** (`Ridges()`, 10): dunas de 2,6-4,2 m de alto y 55-125 m de largo. Cuatro cruzan la playa sobre un corredor
  (X ≈ 150, 438, 768 y 1002 m), con un collado de 7-11 m donde pasa el corredor y, a veces, otro; tres separan
  corredores a lo largo (X ≈ 246, 846 y 924 m); tres son medias lunas entre el corredor de fuera y la selva (X ≈ 282,
  858 y 1074 m). Cara empinada (~30°, con el pie suavizado) hacia la salida y bajada suave (4 veces su alto) hacia el
  mar. Seis llevan **cornisa** (`LipHeight`): un labio de arena de 95 cm en lo alto, donde la cresta pasa del 72 % de su
  alto (fuera de los collados y las puntas): se salta (la tortuga salta 1,2 m) o se pasa por el collado. Malla del
  generador (`FeatureMesh`, con colisión), asentada en la malla del suelo (`MeshGroundZ`).
- **Pozas** (`Pools()`, 10, con agua de verdad): seis charcas entre las dunas (X ≈ 186, 228, 402, 480, 726 y 816 m;
  alguna corta un corredor) y cuatro pozas de marea con rocas alrededor en el último quinto (X ≈ 894, 960, 1034 y
  1080 m). El agua queda 30 cm por debajo de la arena más baja de su orilla (nunca rebosa: la orilla, 15-29 cm por
  encima), de 1,33 a 2,4 m de hondo (`min(2,4 m, 20 %` del radio menor) y orillas de 28° como mucho: se nada y se sale
  andando. Nadable (`ATN_ProcWaterVolume::AddWaterBox`: cajas que siguen su forma, ~150 entre todas), superficie con
  `MI_ProcSeaAnim` (instancia con `DepthRange` 300 y `FoamWidth` 60) y chapuzón al entrar.
- **Trincheras** (`Trenches()`, 2): dos líneas en zigzag de lado a lado de la playa al 28-31 % (X ≈ 331-341 m y
  361-372 m). Canal de 3,2 m con el fondo 60 cm por debajo de la arena entre dos caballones de 45 cm (~1,05 m desde
  dentro: se sale de un salto), tablones por dentro, sacos terreros del lado del mar en grupos de 7 (dos capas, huecos de
  1,6 m), postes y tarimas en el fondo y dos puentes de tablones por línea. El terreno se cava 60 cm hasta 2,1 m del eje
  y vuelve a la arena natural a 6,6 m.
- **Bancos de la selva**: suben 38 m en los 110 m de fuera de la playa y 45 m más hasta 460 m, con colinas; detrás de la
  salida, 42 m y luego 30 m más.
- **Acantilado de roca**: repisa de caras planas en los últimos 24 m (enterrada al empezar, asoma 50 cm sobre la arena
  desde ~16 m antes del filo), filo limpio a 15,5-16 m del agua y pared casi vertical, socavada hasta 2,8 m (nunca
  sobresale del filo: se cae al agua); banda mojada oscura y algas bajo el agua; peñascos al pie de los cabos (fuera de
  donde se cae). En los bancos el acantilado sigue su altura (cabos de 40-80 m). El relieve se apaga antes: el borde se
  lee limpio.
- **Agua de meta**: 11 m de hondo al pie (18 m a 250 m, 35 m lejos). Nadable (`ATN_ProcWaterVolume`, local en cada
  máquina) de debajo de la repisa a 300 m mar adentro y 250 m a cada lado; superficie con `MI_ProcSeaAnim` (hondura y
  espuma a escala: `DepthRange` 1500, `FoamWidth` 160) hasta el horizonte (6 km).
- **Salida con huevos**: en el linde de la selva, entre las raíces de una ceiba colosal (~300 m; tronco de 26 m de radio
  en la base, 52 m detrás de la línea); sus dos raíces tabulares la enmarcan y seis plantas de hojas enormes la techan.
  Cartel «¡A LA META!» (por detrás, «TORTUNAVY») entre dos palos de madera a la deriva. Una fila de **cuatro huevos**
  (los del lobby y la salida del cooperativo: base y tapa de `TNCastleKit`, cada uno con su color) en X = -8 m,
  Y = -15, -5, 5 y 15 m, en un nido de arena (anillo de 1,3 a 2,8 m, con colisión); más filas cada 9 m por detrás si
  hay más de cuatro. Cada tortuga espera dentro del suyo (110 cm sobre el suelo, mirando al mar). Nada del reparto a
  menos de 60 m de la línea (68 m de los huevos) y el relieve empieza a los 35 m.
- **Línea del sprint** (`SprintLineX()`, siempre la misma): lo más cerca de la mitad del recorrido donde los 12 sitios
  (4 en fila y dos filas detrás, como en la salida) caen en arena seca y casi llana (menos de 12°, fuera de pozas y
  trincheras y a 6 m de las cornisas): X ≈ 615 m.
- **Meta**: boyas con banderas a cuadros de 12 m en mástiles de 28 m, cada 40 m y a 26 m del filo, unidas por un cabo
  con boyas pequeñas (se mecen); el arco de neumático de la meta del mapa procedural cinco veces más grande (125 m de
  luz, 63 m sobre el agua) a 40 m del filo, con TORTUNAVY hacia la playa y TORTUNABO hacia el mar, banderines hasta dos
  mástiles en los cabos y banderolas por la ladera de la selva en los últimos 250 m. Zambullida
  (`IsCliffJumpZone`): los últimos 7,5 m antes del filo y 40 m sobre el vacío.
- **Selva**: vegetación del mapa procedural a escala: palmeras de 230-300 m (x26-34), árboles de copa, ceibas de
  250-340 m, casuarinas, pándanos y peñascos en rejilla de 48 m (espesa en los primeros 160 m, más clara hasta 460 m);
  una primera fila de palmeras algo más bajas (x22-29) cada 28-42 m; sotobosque de plataneras, palmitos, helechos
  arbóreos y uvas de playa de 50-100 m en los primeros 100 m. Las palmeras de la orilla se inclinan en diagonal sobre la
  arena y hacia el mar (30-60° de través): sus copas cubren los lados de la playa y dejan el centro a cielo abierto (de
  frente, las copas de los dos lados casi se juntaban sobre la arena vista desde arriba). Sin colisión (detrás de los
  muros), sin sombra y con viento solo a menos de 200 m de la cámara.
- **Huecos entre copas**: rejilla de 15 m por la selva; donde el tronco más cercano queda a más de 22 m y la mata más
  cercana a más de 15 m, enredaderas por el suelo (60 %), helechos o helechos arbóreos (50 %), matas de hojas enormes
  (65 %: tallo de 10 m y hojas de 12-18 m), lianas colgadas de tronco a tronco por encima del hueco (troncos a 3,5-11 m)
  y cortinas de lianas colgando de ellas. Mallas propias instanciadas con viento por vértice (se mecen), sin sombra,
  hasta 300-350 m de la cámara. Registro: `[Playa] selva: ... huecos entre copas con ... lianas y matas`.

### Sombras (sombras virtuales)

La selva no proyecta sombra dinámica. Las copas de la orilla, inclinadas sobre la arena, dejaban casi toda la playa a la
sombra aun con el sol alto (no se veía dónde iba a caer la gaviota ni dónde subirse), y la selva gigante, sin Nanite,
cubre muchísimo mapa de sombras virtuales (de ahí el aviso `[VSM] Non-Nanite Marking Job Queue overflow`). La sombra de
la playa la dan sus elementos, el relieve y las rocas:

- **Con sombra**: las teselas de la playa y del pie de los bancos (hasta 60 m fuera de la playa y desde 40 m detrás del
  muro de la salida: las dunas se leen por su sombra), el acantilado y sus rocas, la salida (ceiba colosal, raíces,
  plantas, nido y huevos), el arco de la meta, las mallas del relieve (trincheras con sus sacos y puentes, cornisas y
  rocas de las pozas de marea) y los elementos de la ronda (lo que diga cada clase).
- **Sin sombra**: toda la vegetación de la selva (árboles, sotobosque y las lianas, cortinas y matas de los huecos:
  `ShadowZone` en `BuildJungle` devuelve siempre false), el resto de las teselas de los bancos, las banderolas de la
  ladera, las boyas y sus banderas, el mar, el fondo, el agua de las pozas, los postes y tarimas de las trincheras y las
  huellas.
- **Valle del lobby** (`ATN_LobbyValley`): la vegetación de las montañas lejanas (a más de 155 m del centro) sin sombra;
  el resto, igual (misma semilla y mismas mallas).

### Reparto por ronda (`TNBeachLayout::GenerateRound`, determinista con la semilla)

Por pasadas, de lo grande y lo que tiene que verse a lo que rellena:

1. **Castillos con salas** (1-3; dos o más en 22 de cada 24 rondas): el principal entre el 42 y el 58 %, a ±39 m del
   centro, con dos alas en embudo hacia su entrada (barren 18 cm hacia la salida por metro) de decorado grande y alambre
   de espino hasta la selva (a 6 m de los muros): o se atraviesa o se rodea por un único hueco de 14 m con algas junto a
   la selva de un lado. Otro sin alas entre el 12 y el 36 % (el 60 % de las rondas) y otro entre el 62 y el 90 %
   (siempre si no hubo el primero; si no, el 60 %).
2. **Pasos de quads**: 2 o 3 franjas que cruzan la playa entera (`Extent` = 28000, Yaw 90°), entre el 15 y el 92 % y a
   170 m como poco entre ellas; nada se pone encima (solo oscurecen la arena: rodadas). Van antes que los castillos
   enormes, que si no les quitan la franja.
3. **Castillos de arena enormes** (5-8, a ±98 m del centro): uno por tramo con sus propios intentos (un tramo de dunas
   no se come los de los demás) y los que falten, donde quepan; la mitad con un trampolín delante para subirse.
4. **Zonas de gaviotas y pelícanos**: 4-6, una por tramo del 10 al 97 %, en lados alternos (a ±70 m o en el centro), a
   150 m como poco entre centros y cada una con su tamaño de círculo, distinto de las demás. Van por encima.
5. **La tropa de las trincheras**: sacos en las puntas de cada línea, una fila de erizos antitanque 18-26 m por delante
   de la del mar, a veces (60 %) un campo de minas más allá y un puesto por detrás de la de la salida.
6. **Filas que obligan a zigzaguear**: hasta 7 (al 10, 18, 37, 64, 72, 80 y 88,5 %; cada una, el 90 % de las veces) que
   cierran la playa de selva a selva con un hueco de 14-22 m que cambia de sitio de una a otra: junto a la selva de un
   lado, del otro o **embudo** al centro. Cada fila, de un tema: militar (sacos, erizos y cajas, con alambre), restos de
   la marea (madera, troncos, rocas, tablones, redes y boyas) o trastos de playa (sillas, sombrillas, castillos, boyas,
   pelotas y cubos); algo inclinadas y combadas, con alguna rendija estrecha para apurar, algas en el hueco y, el 60 %
   de las veces, una catapulta o un trampolín delante (lejos del hueco) para saltarla. Van antes que las trampas
   destacadas, y la pieza que no cabe (una duna, una poza, un paso de quads) se prueba más pequeña en su sitio
   (`BuildWall`): las filas salen enteras.
7. **Trampas destacadas**, repartidas a lo largo antes del relleno (grandes como son, luego no cabrían): catapultas 6-9
   (primero: su arco libre es el más largo), trampolines 6-9, plataformas móviles 9-13, conchas que atrapan 7-10,
   plataformas sobre hoyos 5-7, palas 4-6, cubos rotos 4-6 y puertas de conchas 3-5. Un tramo por pieza con 8 intentos
   cada uno y, lo que no quepa en el suyo, luego por toda la playa (12 intentos por pieza del cupo).
8. **Puestos militares**: 3-4 puestos (red de camuflaje con un parapeto de 2-3 sacos hacia el mar y 3-6 cajas de
   munición, bidones, cascos y soldaditos alrededor), 1-2 filas de erizos de 60-110 m (con huecos de 2,5-5 m) y 2-3
   campos de 5-9 minas en ~25 x 40 m, la mitad en un corredor.
9. **Rincones escondidos** (3-5): herraduras de decorado grande (piezas de 6,5 m de huella como mucho) junto a la
   selva, con el hueco (de 6-9 m de radio, vacío: para el botín) abierto hacia el centro de la playa o hacia el mar.
10. **Lanzadores** delante de lo alto: 1-2 trampolines delante de cada castillo con salas, 1-2 al pie de la cara
    empinada de las crestas con cornisa y, en el 60 % de las pozas que cortan un corredor, un trampolín o una catapulta
    para saltarla en vez de nadar.
11. **Pasarela guía** (el 60 % de las rondas): una hilera de pasarelas y caminitos de palos de 150-350 m hacia el mar
    (tramos de 30-70 m, ±22°) que rodea lo que haya.
12. **Relleno por bandas de 50 m** (desde 60 m hasta 30 m antes del filo): un cupo de enemigos por banda
    (`floor(1,2 + 2,2 t + azar)`: 1-2 junto a la salida, 3-4 junto al mar; los cangrejos, a veces en grupos de 2-3) y
    decorado y trampas hasta cubrir del 26 % (salida) al 46 % (mar) de la banda (`BandCoverage`; 110 piezas por banda
    como mucho), con más trampas hacia el mar (decorado/trampas de 58/42 a 46/54). Algas a menudo en corrillos de hasta 5;
    plataformas y castillos enormes del relleno, a veces con un trampolín delante.
13. **Catapultas garantizadas** (`EnsureCatapults`, `MinCatapults` = 6): si con todo lo anterior hay menos, los
    trampolines de delante de un obstáculo pasan a ser catapultas (en su sitio, algo más atrás).
14. **Tapones**: ninguna línea recta libre hacia el mar de más de 70 m (`MaxStraightRun`). Cada 4 m a lo ancho se mira
    una fila de casillas de 2 m (cuentan lo que ocupa cada elemento, los pasos de quads, las pozas, las trincheras y lo
    alto de las crestas) y donde queda un tramo libre más largo se pone en él algo pequeño (7 m de huella como mucho:
    basura, algas, una mina, un trampolín...), hasta 16 intentos a lo largo del tramo. Lo que cierra el paso se queda
    solo si sigue habiendo paso en toda la playa (las líneas largas van junto a la selva, por los huecos de las filas, y
    la ventana de ±60 m no basta); si no, se prueba con algo que no lo cierra (algas, una mina, una trampa pequeña).

**Reglas de todas las pasadas**:

- **Sitio**: nada a menos de 60 m de la salida ni de 30 m del filo. El decorado y las trampas llegan hasta 4 m de los
  muros (también sobre el pie de los bancos: junto a la selva tampoco queda un pasillo libre); los enemigos, con su
  zona de patrulla entera, a 5 m de la selva como poco (salvo los quads). 1,5 m entre huellas (las piezas de las filas,
  las alas y los rincones se tocan). Cada elemento ocupa su **núcleo** (`Core`): la huella entera en el decorado y las
  trampas, el 35-45 % en los enemigos, que patrullan entre el decorado.
- **Terreno**: nada dentro de las pozas (ni en su orilla), sobre las trincheras (ni sus caballones) ni sobre las
  cornisas; lo redondo grande (14 m de radio o más) tampoco en lo alto de una cresta. Los pasos de quads y lo que va
  por encima, en cualquier sitio.
- **Arcos de salto**: cada catapulta y cada trampolín suelto se pone solo con su arco libre (30 m y 22 m por delante de
  su borde, 10 m de ancho) y lo reserva: nada cae después en él. Los de delante de un obstáculo (castillo, cresta, poza o
  fila) saltan encima o por encima de él a propósito. Todos miran al mar (±20° las catapultas y ±15° los trampolines;
  ±8° los de delante de algo).
- **Paso libre**: lo que cierra el paso (decorado —salvo pasarelas y caminitos—, alambre, castillos y las piezas de las
  filas; no las trampas que se pisan o se atraviesan ni los enemigos) se infla 4 m en una rejilla de 2 m. Cada pieza que
  cierra se comprueba al ponerla (una ventana de ±60 m, de lado a lado; los tapones, en toda la playa) y, en toda la
  playa, tras cada fila, cada banda y cada pasada que cierra, se quita lo último puesto hasta que haya camino: siempre
  queda un paso de 8 m, aunque sea sinuoso.

**Cifras** (las 24 semillas de `Layout.Rules`, con el puerto a Python del reparto, que da lo mismo que el C++; en juego,
el resumen `[Playa] ronda N: ...`):

| | Por ronda |
|---|---|
| Elementos | 903-1191 (media ~1030: ~790 decorado, ~180 trampas, ~59 enemigos); antes de esta tanda, 200-260 |
| Enemigos | 50-65: cangrejos 12-30, erizos 14-22, lagartos 8-14, pasos de quads 2-3, zonas de gaviotas 4-6 |
| Castillos | con salas 1-3 (media 2), enormes 6-10 |
| Trampas destacadas | catapultas 6-12, trampolines 14-26, plataformas móviles 9-16, conchas 8-16, plataformas sobre hoyos 6-10, algas 14-33, minas 18-45 |
| Lo demás | piezas militares 10-31, filas 4-7, rincones 3-5, tapones 45-99 |
| Ocupación (núcleos / banda) | media 31 % (29-33), primer tercio 27 % (22-31), último tercio 32 % (25-37); antes, del 5 al 19 % |
| Líneas rectas | la más larga, 70-106 m según la ronda (ninguna de más de 112 m); paso libre en todas |

Ejemplo, semilla 1000, ocupación de cada banda de 50 m (%): `34 26 29 29 18 10 21 29 37 29 36 66 28 39 11 31 48 36 31
39 29 36 3`. Hacia el mar no llega al objetivo: se acaba el sitio (paso, arcos, pozas, crestas y trincheras). La
densidad se ajusta en `BandCoverage`, los cupos de `PlaceFeaturedTraps` y `FillBands`, `MinCatapults` y los pesos de
`RuleOf`.

**Asiento en la arena** (el «sello» de la ronda): los hoyos los traen los elementos (la plataforma, su cráter). Cada
elemento del suelo (no los enemigos ni lo que va por encima) deja liso el suelo bajo su huella a la cota de la arena en
su centro (con el relieve), con un borde de 2,5-16 m hasta la arena natural. En las dunas, `SeatIsGentle` (dentro de
`TryAdd`) descarta el sitio si la arena de alrededor se aparta del nivel más del 40 % del borde: nada abre paredes. Los
pasos de quads no allanan: solo oscurecen la arena (rodadas). Se rehacen solo las teselas tocadas (en paralelo), en el
servidor y en cada cliente con la semilla replicada; `GetGroundHeightAt` lo da sin trazas.

**Puntos interesantes** (`FRoundLayout::Interest`, para el botín; local, con la Z de la arena): `JumpArc` (arco de cada
lanzador, de `Pos` a `To`: libre), `Summit` (lo alto de las crestas y de los castillos, con `Height`), `Shortcut`
(lanzadores delante de un obstáculo y pozas que cortan un corredor, de `Pos` a `To`), `Nook` (hueco de cada rincón),
`Trench` (tramos de las trincheras) y `Detour` (fondo de cada corredor donde se separa). `Source`, el elemento que lo
crea (`Count` si es del terreno).

### Interfaz (`ATN_BeachRaceGenerator`)

- Servidor: `GenerateRound(int32 Seed)` (destruye la ronda anterior, reparte, asienta, cierra los huevos, crea cada
  elemento con `SpawnElement`, con `bAlwaysRelevant`: la playa mide 1,2 km; reparte el botín con
  `TNBeachLoot::SpawnRoundLoot(*this)` y replica la semilla), `ClearRound()`, `ResetFinishWater()`,
  `OnTurtleReachedWater(ACharacter*)` / `OnTurtleReachedWaterNative` (una vez por tortuga y ronda, con los pies en el
  agua de meta).
- Servidor: `OpenStartEggs()` (BlueprintAuthorityOnly; lo llama `ATN_BeachRaceGameMode::BeginRace` al soltar a las
  tortugas, también en el sprint): se rompen los huevos de la salida o del sprint (ver abajo).
- Servidor: `SetStartEggsAtSprint(bool bAtSprint)` (BlueprintAuthorityOnly) y `AreStartEggsAtSprint()`: lleva el nido de
  huevos a la línea del sprint final o lo devuelve a la salida, con los huevos cerrados. `GenerateRound` y `ClearRound`
  los devuelven a la salida; el GameMode lo llama justo después de `GenerateRound` (`true` solo en el sprint).
- Servidor: `int32 ClearElementsAround(const FVector& WorldCenter, float Radius)` (BlueprintAuthorityOnly): destruye los
  elementos de la ronda cuya huella (un disco o, en los alargados, una cápsula a lo largo de su X) toca el círculo y
  devuelve cuántos. Lo usa el GameMode para despejar el nido del sprint.
- Cualquier máquina: `IsRoundReady()`, `GetRoundNumber()`, `GetRoundSeed()`, `GetStartTransform(int32)`,
  `GetNumStartSpots()`, `AreStartEggsOpen()`, `GetSprintStartTransform(int32 Index)` (BlueprintPure: como
  `GetStartTransform`, pero en la línea del sprint: 4 en fila y filas detrás, 110 cm sobre la arena de la ronda,
  mirando al mar, en arena seca fuera de pozas y trincheras), `IsFinishWater(P)` (más allá del filo y a 30 cm del agua o
  menos: sirven los pies o el centro de la cápsula), `IsCliffJumpZone(P)`, `GetCliffEdgeDistance(P)` (negativa antes del
  filo), `GetCourseProgress(P)` (0-1), `GetGroundHeightAt(P)`, `GetSeaDirection()`, `static Find(WorldContext)` y
  `GetRoundLayout()` (con los puntos interesantes).
- `OnRoundLayoutReady` (delegado C++, `FOnBeachRoundLayoutReady`): al tener el reparto de una ronda, en el servidor
  (tras crear los elementos), en cada cliente (al llegar la semilla nueva) y con Preview Round en el editor.
- **Salida con huevos**: `OpenStartEggs` replica que están rotos y desde cuándo (`RoundNet.bStartOpen` y
  `StartOpenTime`, en tiempo del servidor). Las tapas saltan dando vueltas hacia fuera de la fila y algo hacia atrás, de
  las puntas al centro (0,08 s entre una y otra), y al posarse encogen en 0,3 s (cada máquina las anima). A la vez, cada
  tortuga que esté en la salida (entre el muro de detrás y 15 m por delante) sale lanzada hacia el mar a 10 m/s y
  6,5 m/s hacia arriba (~15 m por el aire, ya corriendo): el servidor a todas y cada cliente a la suya al recibirlo, como
  en la salida de huevos del cooperativo. Quien llega más de 1,5 s tarde los ve ya rotos, sin salto. Cada ronda nueva
  los vuelve a cerrar (y a llevar a la salida).
- **Nido del sprint** (`TN_BeachRaceGenerator_Start.cpp`): con `SetStartEggsAtSprint(true)` se replica
  `RoundNet.bSprintEggs` y cada máquina hace en la línea del sprint (`TNBeachLayout::SprintSpot`, sobre la arena con los
  asientos de la ronda, como `GetSprintStartTransform`) las cuatro bases y el anillo de arena que se pisa, igual que el
  de la salida (`SprintNestMesh`, con colisión en el anillo), y muda allí las tapas, cerradas. `OpenStartEggs` las rompe
  igual y lanza a quien esté en la franja de esa línea (la de la salida, movida hasta `SprintLineX`). En la salida se
  quedan las bases abiertas. La ronda siguiente (o volver a jugar) vacía el nido del sprint y devuelve las tapas.
- Chapuzón en cada máquina al entrar una tortuga en el agua de meta o en una poza (gotas, espuma y ondas).
- Si nadie reparte, el servidor reparte una ronda al azar a los 3 s (sin el GameMode de la carrera) o a los 20 s (con
  él). Consola: `TN.Beach.ShowFootprints 1` enseña las huellas del reparto en juego. Registro: `[Playa] terreno fijo:
  ...` y `[Playa] selva: ...` al construir, `[Playa] agua nadable: ...` (pozas y cajas), `[Playa] ronda N: ...` en cada
  ronda (resumen con la ocupación por banda y los tiempos del reparto, los asientos y el total) y `[Playa] ronda N: se
  rompen los huevos de la salida.` (o `del sprint final`) y `[Playa] ronda N: los huevos de la salida van a la línea
  del sprint final (M m).`

### Nivel y capturas

- `Scripts/build_beach_race.py` (en el editor, con el C++ compilado) crea o abre `/Game/Maps/Run/LVL_BeachRace`: sol a
  la espalda de la salida, cielo, luz del cielo, niebla suave (desde 300 m), el generador «PlayaCarrera» en el origen,
  cuatro `PlayerStart` en las salidas y `TN_BeachRaceGameMode` en World Settings; lo guarda. El sol, si ya existe, no se
  toca.
- Para ver el reparto sin jugar: en el generador, Details > Beach|Editor > **Preview Round** (con `Editor Seed` o al
  azar) y **Clear Preview**; no se guarda con el nivel. Las huellas: amarillo decorado, naranja trampas, rojo enemigos,
  morado quads, celeste gaviotas, marrón pasarela guía, rosa los castillos (con salas y sus alas, y los enormes), verde
  azulado las filas, oliva lo militar, blanco azulado los lanzadores, marrón oscuro los rincones y gris los tapones, con
  una flecha hacia su X local. Los puntos interesantes: arcos de salto en blanco y atajos en fucsia (tiras) y rincones,
  cimas, trincheras y caminos alternativos en rombos (marrón, amarillo, oliva y azul).
- Qué capturar, con `vista(nombre)` del script (`BEACH_SKIP_MAIN = True` antes del `exec`): `salida`, `planta` (con
  Preview Round: el reparto entero y sus huellas), `castillo`, `acantilado`, `meta`, `selva` y las nuevas `huevos` (la
  fila en su nido), `dunas` (a ras de arena: relieve, corredores y crestas), `trinchera`, `poza`, `cresta` (la cornisa
  desde la cara empinada y un collado), `huecos` (lianas y hojas entre las copas) y `sprint`. Las cotas de las vistas
  salen del terreno fijo medido en Python: si alguna queda enterrada o alta, se retoca su Z. En juego: la salida (tapas
  y salto de las cuatro), un trampolín delante de un castillo, cruzar a nado una poza, pasar una trinchera y una
  cornisa, y el registro `[Playa] ronda N` (ocupación por banda y tiempos).
- Pruebas: `Automation RunTests Tortunabo.Beach`: `Terrain` (salida en la zona del salto de los huevos, nada a menos de
  60 m, línea del sprint a 30 m de la mitad con sus 12 sitios secos y llanos, meta, zambullida y asientos), `Relief`
  (pendiente máxima < 42°, desviación > 90 cm y más del 10 % por encima de 10°; corredores más hondos, crestas con
  cornisa, pozas sin rebosar, hondas, de orilla suave y nadables; trincheras cavadas junto al eje y con la arena natural
  lejos de todos los canales), `Layout.Determinism` (más de 500 elementos, iguales dos veces, puntos interesantes
  incluidos) y `Layout.Rules` (24 semillas: límites, sin solapes, asientos suaves, arcos libres, castillos, quads,
  gaviotas separadas y distintas, cupos de enemigos, cangrejos, 4 catapultas o más, trampolines, plataformas, filas,
  militar, ocupación, puntos interesantes, como mucho 8 filas con líneas rectas de más de 112 m y más huella hacia el
  mar).

## Reparto del trabajo (agentes)

| Parte | Archivos |
|---|---|
| Contrato común | `Public/World/Beach/TN_BeachTypes.h`, `TN_BeachElement.*`, `Game/TN_BeachRaceGameState.*`, este documento |
| Menú, flujo y reglas | `UI/Menu/MP_MainMenuWidget.*`, `Lobby/TN_HQGameMode.*` (viaje), `Game/TN_BeachRaceGameMode.*`, aturdir en vez de morir; cuenta atrás tras la primera, medias conchas, enganche del gusano de arena y sprint final (con el nido de huevos en su línea, `World/Beach/TN_BeachRaceGenerator_Start.cpp`, y su interfaz en `UI/Race/`); salto final y chapuzón (`World/Beach/TN_BeachFinishSplash.*`, `TN_BeachSplashSynthComponent.*`); misión con el general (`Lobby/TN_LobbyMission.*`, pestaña «Misión» de `UI/Briefing/TN_BriefingWidget.*`, pizarra de `Lobby/TN_GeneralBriefing.*`, `Lobby/TN_ProcModeSelector.*`) |
| Terreno, nivel y reparto | `World/Beach/TN_BeachRaceGenerator.*`, `TN_BeachLayout.h`, `Scripts/build_beach_race.py` |
| Decorado gigante | `World/Beach/TN_BeachDecor.*`, `TN_BeachPropMeshes.h` |
| Trampas e interacciones | `World/Beach/TN_BeachBarbedWire.*`, `Seaweed`, `WobblyPlatform`, `BrokenBucket`, `SpadeRamp`, `SandDungeon`, `ShellGate`, `ClamTrap`, `MovingPlatform`, `Catapult`, `Trampoline`, `TN_BeachRideKit.h` |
| Enemigos y tormenta | `World/Beach/TN_BeachGiantCrab.*`, `SeaUrchin`, `Lizard`, `QuadLane`, `GullZone`, `TN_BeachStorm.*` |
| Recuento, campeón y podio | `UI/Race/*`, poses de celebración en `Player/TN_TurtleAnimInstance.*`, escena del podio |
| Botín en la playa y brillo de lo que se coge | `World/Beach/TN_BeachLoot.*`, `TN_BeachLootShells.cpp`, `World/TN_PickupGlowComponent.*`, `Private/World/TN_LootGlowKit.h`; ganchos en `World/ProcMap/TN_ProcSearchSpot.*` y `World/TN_PickupInteractableBase.*` |
| Decorado militar y minas | `Private/World/Beach/TN_BeachMilitaryMeshes.h` (lo engancha `TN_BeachPropMeshes.h`), `World/Beach/TN_BeachMine.*`, `TN_BeachMineSynth.*` |

## Flujo de la carrera: menú, lobby, rondas y aturdimiento

### Menú principal y lobby

- **Menú** (`UMP_MainMenuWidget`): los mismos tres botones del Blueprint (mismo estilo; solo cambian los textos) en dos
  pasos. «Crear partida» → «Cooperativo» / «Carrera» / «Volver». «Unirse» no pregunta nada: el modo lo decide el
  anfitrión. El modo va a `UMP_GameInstance::SelectedProcMode` (`HostSessionWithMode`) y sobrevive a los viajes.
- **Lobby** (`ATN_HQGameMode::BeginMatchTravel`): Carrera → `BeachRaceMapPath` (`/Game/Maps/Run/LVL_BeachRace`; si el
  nivel aún no existe, error en el log y se juega la carrera del mapa procedural). Cooperativo y 2vs2 →
  `LVL_ProcMap`; Clásico → `LVL_Run`. En el lobby, el modo y la dificultad se cambian hablando con el General
  Galápago (pestaña «Misión», solo el anfitrión: ver «Misión con el general del lobby»); también en el menú, con
  «Cambiar de modo» al acabar la carrera y con `TN.Mode Coop|Race` en el anfitrión. Los selectores del lobby
  (`ATN_ProcModeSelector`), si el nivel los tiene, usan la misma lógica: «MODO: CARRERA» también va a la playa.

### Misión con el general del lobby (modo y dificultad)

Para no tener que subir a los selectores: al hablar con el general (`ATN_GeneralBriefing`) la sesión informativa
(`UTN_BriefingWidget`) se abre en la pestaña **«Misión»**, la primera de cinco («Misión», «Cómo se juega», «Modos de
juego», «Reglas» y «Controles»).

- **Qué se elige**: el **modo**, Cooperativo o Carrera (las pastillas; debajo, una línea de cada uno, las mismas que da
  el menú al crear partida), y la **dificultad**, Fácil, Normal o Difícil (con lo que cambia en el cooperativo; la playa
  de la carrera es siempre la misma). La elegida va en coral; abajo, la etiqueta «Orden del día: CARRERA · NORMAL» y,
  al cambiar, el general contesta en su bocadillo («¡Carrera! Todas contra todas hasta el agua…»).
- **Quién**: solo el anfitrión (`TNLobbyMission::CanLocalPlayerChoose`: servidor escucha o partida sola). Los demás ven
  lo mismo con las pastillas apagadas y el aviso «El modo y la dificultad los elige el anfitrión…»; si el anfitrión lo
  cambia mientras lo miran, se repinta y el general anuncia la nueva orden del día. Sin RPC: la interfaz del anfitrión
  corre en el servidor (como la pantalla del campeón) y un cliente no puede cambiarla.
- **Dónde vive**: donde siempre, en `UMP_GameInstance::SelectedProcMode`/`SelectedProcDifficulty` del anfitrión, que lee
  `ATN_HQGameMode` al viajar. Para que todos lo vean al momento se replica en el general (`MissionMode`,
  `MissionDifficulty`, con `ForceNetUpdate`), que lo escribe con tiza en una **pizarra** en su caballete junto a la mesa
  («ORDEN DEL DÍA / MISIÓN: CARRERA / DIFICULTAD: NORMAL»), y en las etiquetas de los selectores si los hay.
- **Lógica común** (`Lobby/TN_LobbyMission.h`, `namespace TNLobbyMission`): `MenuModes` (Coop y Carrera) y
  `Difficulties`, `ModeName`, `DifficultyName`, `ModeBlurb`, `DifficultyBlurb`, `NextSelectorMode` (el ciclo del
  selector: Clásico → Coop → Carrera → 2vs2 solo con cuatro), `CanLocalPlayerChoose`, `GetHostMode`/`GetHostDifficulty`
  y `SetMode`/`SetDifficulty` (anfitrión: cambian la GameInstance y llaman a `SyncLobby`, que copia la misión en el
  general y en los selectores; 2vs2 solo con cuatro jugadores). La usan el menú principal (textos de los modos), el
  general, los selectores (`ATN_ProcModeSelector::SyncFromGameInstance`) y `TN.Mode`.
- **Controles**: ratón, clic en la opción. Teclado y mando (anfitrión, en «Misión»): ↑/↓, W/S o cruceta arriba/abajo
  eligen la fila (modo o dificultad, marcada con «»»); ←/→, A/D o cruceta izquierda/derecha cambian la opción; las
  pestañas siguen con Q/E, Tab o LB/RB (y 1-5); Esc, Intro, A o B cierran. En el resto de pestañas, lo de siempre.

### GameMode y fases

`ATN_BeachRaceGameMode` hereda de `ATN_RunGameMode`, no de `ATN_ProcMapGameMode`: el bucle de rondas de este es privado
y va atado a `ATN_ProcMapGenerator` (lo crearía si el nivel no lo tiene). De la base se reutiliza la espera a los
jugadores tras el viaje, la meta (puesto, puntos y espectador), la vuelta al lobby (`LobbyReturnMapPath`) y el flujo
replicado del que tiran el huevo de la pantalla de carga («¡ADELANTE!») y la música de fin de partida. El bucle de
rondas es el de la carrera del mapa procedural, adaptado. GameState: `ATN_BeachRaceGameState` (`ProcMode = Race`,
`RoundTarget` = `WinsToWinMatch` = 3).

| Fase (`RacePhase`) | `MatchFlowState` | Qué pasa | Tiempo |
|---|---|---|---|
| `Waiting` | `WaitingForPlayers` | `GenerateRound(semilla)`; tortugas nuevas en la salida, cada una dentro de su huevo (escalonada; los sitios rotan cada ronda), quietas | ≥ 2 s (`MinPreRoundSeconds`; como mucho 20 s esperando al generador) + cuenta atrás de 3 s (`CountdownValue` y `PhaseSecondsLeft`), salvo en la primera ronda tras el viaje, cuya cuenta es el huevo |
| `Racing` | `InProgress` | al empezar se rompen los huevos y las tortugas salen lanzadas hacia el mar (`OpenStartEggs`); la primera que toca el agua de meta gana la ronda (`RoundWinner`) y arranca la cuenta atrás (`FinishCountdown` = `Counting`): quien llega dentro, media concha (`RoundHalfShells`). Cada una se queda a la vista en el agua con su chapuzón 0,8 s (`FinishSplashHoldSeconds`) y luego pasa a espectadora. Al acabar la cuenta, `TimeUp`: a cada una que no ha llegado se la come un gusano de arena (`ATN_BeachSandWorm::EatTurtle`) y todas quietas `EatSeconds` + 0,6 s (`SandWormMarginSeconds`); sin nadie a quien comer, o con todas dentro (`AllIn`), 1,6 s (`TimeUpHoldSeconds`). La tormenta sigue hasta entonces | cuenta de 10 s (`FinishCountdownSeconds`); límite 9 min (`RoundTimeLimitSeconds`) sin nadie en el agua: gana la más cerca del mar (con su «¡TIEMPO!») |
| `RoundResults` | `Countdown` | recuento: `RoundWinner` (entera), `RoundHalfShells` (medias) y `RaceShellHalves`; todas quietas | 7 s (`RoundResultsSeconds`) |
| `SprintIntro` | `Countdown` | empate en lo más alto con `WinsToWinMatch` conchas o más: `bSprintFinal` y `SprintFinalists`; título «¡SPRINT FINAL!» | 5 s (`SprintIntroSeconds`) |
| `Waiting` → `Racing` (sprint) | `WaitingForPlayers` → `InProgress` | las demás, a espectadoras; reparto nuevo con el nido de huevos en la línea del sprint (`SetStartEggsAtSprint`); el nido despejado (`ClearElementsAround`) y cada finalista, tortuga nueva dentro de su huevo (`RestartPlayerAtTransform` en `GetSprintStartTransform(i)`), quietas; 3, 2, 1 y los huevos se rompen (`OpenStartEggs`); la primera en el agua es campeona (sin cuenta de 10 s ni gusanos) | límite 5 min (`SprintTimeLimitSeconds`): gana la más cerca del mar |
| `Champion` | `Results` | `Champion` y `Podium` (por conchas en medias; a igualdad, quien ganó una ronda más tarde); se espera al anfitrión | sin límite |

- El recuento sale siempre, también el de la tercera concha; el campeón (o el sprint) va después. Tras el sprint, el podio
  sale directamente.
- En `Champion`, `CountdownValue` se queda en 99 (no hay cuenta): así la música de fin de partida no se funde ni se
  cierra el huevo. El HUD de siempre (`UTN_CoopFlowHUDWidget`) enseña su panel de resultados en `Results`: la pantalla
  del campeón tiene que taparlo u ocultarlo en la playa.
- **Pantalla del campeón** (para la interfaz): `ATN_BeachRaceGameMode::RequestChampionChoice(this, Choice)` con
  `ETNBeachChampionChoice::PlayAgain`, `ChangeMode` o `Quit`, y `CanLocalPlayerChoose(this)` (true en el anfitrión con la
  partida acabada). Sin RPC: en servidor escucha la interfaz del anfitrión corre en el servidor. En un cliente solo
  `Quit` hace algo (sale él solo al menú).
  - **Volver a jugar**: conchas a cero y ronda 1 en la misma playa, sin viajar (con cuenta atrás).
  - **Cambiar de modo**: `SelectedProcMode = Coop` y vuelta al lobby; al ponerse listos, cooperativo.
  - **Salir**: el anfitrión vuelve al menú y la partida se cierra (los clientes vuelven al menú con «El host abandonó la
    partida»).
  - Al elegir, `CountdownValue` = 1 durante 1,2 s: el huevo se cierra en todas las pantallas, la música se funde y
    luego se viaja.
- **Desconexiones**: si alguien se va en plena carrera, la ronda sigue con las demás (gana la primera que toque el agua);
  si se va la última que corría durante la cuenta atrás, «¡TIEMPO!» sin esperar. Quien llegó conserva su concha mientras
  exista su PlayerState. Quien entra con la ronda en marcha sale desde la salida (en el sprint, mira); entre rondas, se
  queda quieta. En el sprint, si solo queda una finalista, es campeona (sin ninguna, la de más conchas). El podio
  conserva los PlayerState mientras existan (después llegan como null). Si se va el anfitrión, se acaba la partida.

### Cuenta atrás tras la primera y medias conchas

Lo que pidió el usuario: que la ronda no se cierre al llegar la primera, sino que salga una cuenta atrás grande de 10 s
y quien llegue dentro se lleve media concha.

- **Llegadas** (`ATN_BeachRaceGameMode::MarkPlayerFinished`, `Arrivals`): el puesto se decide en el instante del
  contacto. La primera: `RoundWinner`, concha entera (2 medias) y arranca la cuenta (`FinishCountdown` = `Counting`,
  `FinishCountdownEndTime` en hora del servidor y `FinishCountdownSeconds`, replicados; la interfaz calcula lo que queda
  con `GetFinishCountdownLeft`). Las siguientes, dentro de la cuenta: media (`RoundHalfShells`, en orden). Nada se
  cierra: la tormenta, los enemigos y el resto siguen, y quien aún corre puede llegar.
- **A la vista y luego espectadora**: cada una sale del caparazón, del aturdimiento y de quien la llevara y se queda en
  el agua `FinishSplashHoldSeconds` (0,8 s) con su chapuzón; pasado ese margen (`SettleArrivals`, en `WatchRacers`)
  pasa por la meta de la base (`ATN_RunGameMode::MarkPlayerFinished`: puesto, puntos, la oculta y
  `MovePlayerToSpectator`, la vía normal del espectador sobre la que va el fantasma).
- **Fin** (`FinishTimeUp`): al acabar la cuenta, o si ya no queda nadie corriendo (han llegado todas, se han ido o
  miran), `TimeUp` o `AllIn`: «¡TIEMPO!» (o «¡TODAS AL AGUA!»), todas quietas, la tormenta se para y, a los 1,6 s
  (`TimeUpHoldSeconds`), el recuento (`EndRound`): reparte `RaceShellHalves` (+2 la primera, que suma también
  `RoundWins`; +1 cada media) y pasa detrás del recuento por la meta de la base a quien aún estaba en el agua.
- **Gusano de arena** (lo pidió el usuario): solo cuando la cuenta llega a 0 (`OnFinishCountdownEnd` →
  `FinishTimeUp(false, true)`; ni con «¡TODAS AL AGUA!», ni en el límite de 9 min, ni en el sprint), en el servidor,
  `FeedSandWorms` saca a cada tortuga que aún corría de quien la llevara, del mareo, del derribo y del caparazón y llama a
  `ATN_BeachSandWorm::EatTurtle(Tortuga)` (la clase es de otro agente: sale de la arena, se la come y la deja quieta,
  sin control y oculta). Si ha salido algún gusano, el recuento espera `ATN_BeachSandWorm::EatSeconds` (3,2 s) +
  `SandWormMarginSeconds` (0,6 s); si no, los 1,6 s de siempre. Mientras, nada la toca: `WatchRacers` se la salta,
  `MarkPlayerDead` y el rescate no actúan con la ronda cerrada y `TNBeach::StunTurtle` / `KnockDownTurtle` no hacen nada
  con una tortuga en la boca de un gusano (`IsBeingEaten`). Nadie muere: la ronda siguiente las vuelve a crear en la
  salida (`CleanupRoundActors` y `PlacePlayersAtStart`). En pantalla, «¡TIEMPO!» se encoge y sube tras el golpe y quien
  corría se queda sin etiqueta, para ver el bocado.
- **Medias conchas**: `ATN_CoopPlayerState::RaceShellHalves` (replicado; 2 = una entera). `RoundWins` sigue contando
  las rondas ganadas enteras (desempate del podio). La partida es a `WinsToWinMatch` = 3 conchas (6 medias). La música
  (`UTN_MatchMusicSubsystem`) le pasa al director las medias (una media suena a ronda ganada) y, con campeón, solo él
  gana.

### Sprint final de desempate

- **Cuándo** (`AfterRoundResults`): la más alta en medias con `WinsToWinMatch` × 2 o más es campeona si está sola; si
  empatan varias ahí arriba (p. ej. 3 y 3), `EnterSprintIntro(finalistas)`: `bSprintFinal`, `SprintFinalists` y la fase
  `SprintIntro` (5 s, `SprintIntroSeconds`): título «¡SPRINT FINAL!» con las caras, «VS», fanfarria y confeti.
- **Preparación** (`StartSprint`, ronda nueva con `bSprint`): en la carrera no se muere ni se vuelve a la vida (volver
  desde un huevo con `TNGhost::ReviveIntoEgg` es, de momento, solo del cooperativo), así que el sprint sale como cada
  ronda, del nido de huevos, pero en su línea. Las que no corren se quedan sin tortuga y pasan a espectadoras por la vía
  normal (`SendOutOfSprint`: `bHasFinishedRun` y `MovePlayerToSpectator`; el fantasma es del agente del espectador) y las
  tortugas de las finalistas se cambian por otras nuevas al ponerlas en su huevo (sin limpiar todos los peones como
  entre rondas, para no quitar su vista a las espectadoras). Reparto nuevo (`GenerateRound`) y el nido a la línea del
  sprint (`SetStartEggsAtSprint(true)`); al estar listo (`PlaceSprintFinalists`), se despeja el nido
  (`ATN_BeachRaceGenerator::ClearElementsAround`, con `SprintClearMargin` = 15 m alrededor de sus sitios) y cada
  finalista, por su orden, recibe una tortuga nueva dentro de su huevo (`RestartPlayerAtTransform` en
  `GetSprintStartTransform(i)`, apoyada en el suelo), quieta durante el 3, 2, 1 (`PreRaceCountdownSeconds`). Al dar la
  salida, `OpenStartEggs`: las tapas saltan y las finalistas salen lanzadas hacia el mar. La tormenta sale por detrás
  del nido.
- **Carrera**: solo las finalistas; la primera en el agua es campeona (sin cuenta de 10 s ni gusanos: su chapuzón y el
  podio, `CompleteSprintWin`). Límite 5 min (`SprintTimeLimitSeconds`): la finalista más cerca del mar. Si una se queda
  sin tortuga al dar la salida, vuelve a su huevo.
- **Después**: la ronda siguiente o volver a jugar devuelven el nido a la salida (`GenerateRound` y
  `SetStartEggsAtSprint(false)`).

### Lo que el GameMode usa del generador y de la tormenta

- `ATN_BeachRaceGenerator`: `void GenerateRound(int32 Seed)`, `bool IsRoundReady() const`,
  `FTransform GetStartTransform(int32 PlayerIndex) const` (mirando hacia el mar; la cápsula se apoya en el suelo que
  haya debajo) y `bool IsFinishWater(const FVector& WorldLocation) const`. No hace falta aviso de meta: el GameMode
  mira `IsFinishWater` de cada tortuga diez veces por segundo. Si otra pieza quiere dar la meta, que llame a
  `GetAuthGameMode<ATN_RunGameMode>()->MarkPlayerFinished(PC)`.
- También del generador: `OpenStartEggs()` al dar la salida (también en el sprint), y para el sprint de desempate
  `SetStartEggsAtSprint(bool)` (el nido a su línea), `GetSprintStartTransform(int32)` (sitios a mitad de la playa, en
  arena seca y llana) y `ClearElementsAround(WorldCenter, Radius)` (quita los elementos de la ronda que tocan el círculo;
  devuelve cuántos).
- Del gusano de arena (`World/Beach/TN_BeachSandWorm.h`, de otro agente): `ATN_BeachSandWorm::EatTurtle`, `EatSeconds` e
  `IsBeingEaten`.
- `ATN_BeachStorm` (por nombre): se crea al dar la salida, `StormSpawnBehind` (30 m) detrás de ella y mirando hacia el
  mar. Si tiene `UFUNCTION() void StartStorm()` y `UFUNCTION() void StopStorm()` sin parámetros, se llaman al salir y al
  acabar la ronda; si no, arranca sola y se destruye al acabar. Al preparar la ronda siguiente se destruye siempre.

### No se muere: aturdimiento

- `TNBeach::StunTurtle(Tortuga, Segundos, Lanzamiento)` (`Private/World/Beach/TN_BeachStun.cpp`): suelta lo que lleve,
  sale del derribo y se mete en el caparazón como bola (`ForceEnterShell` sin cuerpo + `StartBody(Lanzamiento,
  lanzada)`; sin lanzamiento, cae donde está), con la salida bloqueada. Si ya estaba aturdida, se alarga hasta el mayor
  de los dos finales (y, con lanzamiento, la bola sale disparada otra vez). Al acabar se desbloquea y sale sola en
  cuanto la bola se para. El estado va en `UTN_BeachStunComponent` (`bStunned` y el final, replicados), que el servidor
  añade en ejecución la primera vez: el cooperativo no lo lleva. En todas las máquinas: la bola tiembla (la malla se
  agita sobre la caja física) y dan vueltas los pájaros del mareo.
- `TNBeach::IsNoDeathWorld`: el GameState es `ATN_BeachRaceGameState`.
- Todas las rutas de muerte acaban en `MarkPlayerDead` (zonas de muerte, `ATN_StormVolume`, caídas de más de
  `FatalFallHeight` y enemigos vía `ATortugaCharacter::RequestKill`); el de la playa nunca llama a la base:
  - en el agua de meta → llega (una caída larga al agua es llegar);
  - dentro de una zona de muerte o de tormenta, o por debajo del vacío → vuelve a su último sitio seguro (se apunta cada
    0,5 s si pisa suelo, fuera de zonas de muerte y sin aturdir; se usa el más nuevo con al menos 1 s) y queda aturdida
    2,5 s (`RescueStunSeconds`);
  - si no → aturdida 3 s donde está (`DeathStunSeconds`).
- Vacío: el suelo más bajo pisado en la ronda (o la salida) menos 150 m (`VoidDepth`), siempre por encima del `KillZ`
  del nivel. Si el motor destruye la tortuga igualmente, reaparece en su sitio seguro, aturdida.
- El salto del acantilado de meta no aturde ni hace bola: ver «Salto final al agua».

### Salto final al agua

Lo que pidió el usuario: que el salto final no se haga bolita por los metros de caída, que caiga de cabeza al agua y que
se quede medio segundo largo dentro del agua para que se vea la salpicadura.

- **De dónde salía la bola**: no de la muerte por caída. `ATortugaCharacter::TickFallRules` (servidor) mete a la tortuga
  sola en el caparazón como bola con física a los `AutoShellFallHeight` = 5 m de caída libre; el acantilado mide 15,5 m.
  El golpe de `Landed` (`FatalFallHeight` = 35 m → `RequestKill` → `MarkPlayerDead` → aturdir) no llega a saltar: la
  caída es más corta y al entrar al agua el movimiento pasa a nadar, que deja de contar la caída.
- **Arreglo** (`ATN_BeachRaceGameMode::GuardCliffJump`, en `WatchRacers`, diez veces por segundo, también mientras se ve
  el chapuzón de la ganadora): si una tortuga cae (`MOVE_Falling`, fuera del caparazón y sin aturdir) dentro de
  `ATN_BeachRaceGenerator::IsCliffJumpZone` (los últimos 7,5 m de la repisa y el vacío sobre el agua), su caída pasa a
  inmune (`SetFallImmuneUntilLanded`, lo mismo que géiseres y palas): ni bola a los 5 m ni golpe al aterrizar hasta tocar
  suelo o agua. Vale si la caída empieza o pasa por la zona; el resto de caídas de la playa, igual que siempre. Sin
  tocar `TortugaCharacter`.
- **De cabeza**: la zambullida de `UTN_TurtleAnimInstance` (ver «Poses nuevas y zambullida») ya no se corta por la bola;
  y, si la caída entra en la zona ya lanzada (más de 0,35 s cayendo), empieza igual en cuanto cae a más de 10 m/s.
- **A la vista en el agua** (`MarkPlayerFinished`): el puesto se decide en el instante del contacto (la primera que el
  `Watch` ve en el agua de meta gana la ronda; ver «Cuenta atrás tras la primera y medias conchas») y la tortuga sale
  del caparazón, del aturdimiento y de quien la llevara. Se queda en el agua, visible en todas las máquinas,
  `FinishSplashHoldSeconds` = 0,8 s, sin aturdirla ni rescatarla; después pasa por la meta de la base (puesto, puntos,
  la oculta y la pasa a espectadora). La cámara no corta antes de ese margen.
- **Chapuzón** (en cada máquina, a partir del movimiento replicado, sin RPC): el generador ya pinta la corona de gotas,
  la espuma y las ondas al entrar los pies en el agua de meta (`ATN_BeachRaceGenerator::Splash`). Lo que faltaba lo
  añade `UTN_BeachFinishSplashSubsystem` (`World/Beach/TN_BeachFinishSplash.*`), que mira lo mismo: el **chorro** de
  agua que sube un instante después (gotas casi verticales y una columna de espuma blanca, `TNAmbientFX` en un actor
  local) y el **«¡chof!» sintetizado** (`UTN_BeachSplashSynthComponent`: chasquido del golpe, lámina de agua que baja de
  tono, «plom» grave de la cavidad, burbujas que suben de tono y la lluvia de gotas del chorro; sin archivos de audio,
  como el foley de las trampas). El tamaño sale de la velocidad de caída al tocar el agua (el acantilado es el 1).

### Pruebas

- Sin lobby: `open LVL_BeachRace?BeachSeed=42?BeachWins=1` (semilla fija y conchas para ganar).
- Consola en la ventana del anfitrión: `TN.Race.WinRound [jugador]` (toca el agua: la primera arranca la cuenta atrás de
  10 s; las siguientes, media concha), `TN.Race.Champion [jugador]`, `TN.Race.Sprint [jugador] [jugador]…` (empate
  forzado a tres conchas y sprint final; por defecto 0 y 1; con uno solo también, para probarlo),
  `TN.Race.Stun [segundos] [jugador]`, `TN.Race.Kill [jugador]` (ruta de muerte), `TN.Race.Void [jugador]` (al vacío),
  `TN.Race.PlayAgain`, `TN.Race.ChangeMode`, `TN.Race.Menu` y `TN.Mode [Coop|Race]`. `jugador` es el índice en
  `PlayerArray` (0 por defecto, normalmente el anfitrión).
- En cualquier máquina y mapa: `TN.Race.Splash [tamaño]` (chorro y «¡chof!» delante de tu tortuga, solo en esa máquina;
  la corona de gotas del generador sale solo al entrar de verdad en el agua de meta).
- Salto final (PIE de 2 jugadores): saltar desde la repisa del acantilado → cae de cabeza, sin bola, y entra al agua con
  corona, chorro y «¡chof!»; se ve 0,8 s dentro del agua (también desde la otra ventana) y luego pasa a espectadora.
  Caerse por el resto de la playa sigue haciendo bola a los 5 m.
- Cuenta atrás (PIE de 2): la primera llega → en las dos ventanas, la cinta «¡La primera ya está en el agua!» y el
  número de 10 a 1 con «¡toc!» que se acelera; la segunda llega dentro → «¡Media concha para ti!» y, como ya no queda
  nadie, «¡TODAS AL AGUA!» con silbato y el recuento, sin gusanos (la entera vuela y la media salta después). Sin llegar
  la segunda: «¡TIEMPO!» a los 10 s, le sale el gusano de arena, se la come y el recuento espera a que acabe (~3,8 s);
  en ese rato no la aturde, rescata ni mueve nada. En la ronda siguiente sale de su huevo como todas. Con tres, que la
  tercera fuera de tiempo no se lleve nada.
- Sprint (PIE de 2 o 3): `TN.Race.Sprint 0 1` en el anfitrión → título con fanfarria y «VS»; el nido de huevos aparece
  a mitad de la playa (en la salida quedan las bases abiertas) con las finalistas dentro, quietas; 3, 2, 1, las tapas
  saltan y salen lanzadas hacia el mar (en las dos ventanas); la primera en el agua sale en el podio como campeona
  («¡Gana el sprint final…!»), sin cuenta ni gusanos; una tercera jugadora mira de fantasma. Después, «Volver a jugar»:
  el nido vuelve a la salida. Probar también que un empate de verdad (dos a tres conchas en la misma ronda) lo lanza
  solo.
- Misión (PIE de 2 jugadores, en el lobby): el anfitrión habla con el general, cambia modo y dificultad con el ratón y
  con teclado o mando; la otra ventana lo ve al momento en el diálogo abierto y en la pizarra, y no puede cambiarlo.
  Al ponerse listos, se viaja al modo elegido.

## Decorado gigante (`ATN_BeachDecor`)

| Archivo | Qué es |
|---|---|
| `Public/World/Beach/TN_BeachDecor.h`, `Private/World/Beach/TN_BeachDecor.cpp` | `ATN_BeachDecor`: todos los elementos de la categoría Decor; caché de mallas, colocación, colisión, tramos instanciados y animación |
| `Private/World/Beach/TN_BeachPropMeshes.h` | Recetas low-poly (`TNBeachProp`): primitivas, colisión simple, una receta por pieza y los kits de la pasarela y del caminito |

**Cómo funciona.** En `ApplySpec` la variante sale de `Spec.Seed` (4 por pieza; 8 rocas, 3 grupos de rocas) y la malla de
cada (elemento, variante) se construye **una vez por partida** (`static`, fuera del recolector, `RF_Transient |
RF_DuplicateTransient`, `M_CosmeticVertexColor` con el alfa del vértice como brillo) y la comparten todos los ejemplares:
el motor junta en una llamada de dibujo las mallas iguales con el mismo material. Cada ejemplar se coloca con la semilla
(giro libre salvo la silla, que mira a +X del actor con ±15°; inclinación de 0-8°; hundimiento en la arena según la
pieza, para que no flote en las dunas) y se escala a `Spec.SizeScale` (0,5-1,6) en su componente: **el actor no se escala**. La colisión va
en el `BodySetup` de la malla compartida: solo cajas, esferas y cápsulas (simple como compleja, nada que cocinar), con el
perfil `BlockAll`; la cámara solo choca con lo grande y macizo (rocas, grupos de rocas, troncos y castillos). Detalles
finos sin colisión (anillas, chapas, cáscaras, palitos, cuerdas, plumas, pajitas tumbadas). Lo pequeño deja de dibujarse
a 60 veces su huella (de 120 m a 600 m); lo de más de 10 m de huella, nunca.

**Subir y saltar.** Los escalones son de 80 cm (la tortuga salta 1,2 m; con `SizeScale` 1,4 siguen por debajo) y las
rampas de menos de 34° (lona de la vela 12-33°, toalla de la silla 33°, rampa de tablones 19°, losa 20°, pliegues de la
toalla 21-30°, lona de la sombrilla 13-31°, casquetes de la medusa y de la red 38°, montón de arena de la sombrilla 27°).

**Movimiento** (solo en las máquinas con cámara, nunca en el servidor dedicado ni en la ronda de prueba del editor): la
campana de la medusa respira y tiembla a ratos (escala), la valva de arriba de la almeja se abre y se cierra cada 7-12 s
por la bisagra (no si hay una tortuga encima al empezar el ciclo), el jirón de la vela ondea, el paño de red se mece y
la banderita de los castillos ondea. La parte que se mueve es un segundo componente sin colisión colgado de la malla
fija en su pivote. Un temporizador (cada 0,5 s) enciende el tick solo con una cámara local a menos de 60-200 m (según
el tamaño) y si se ha dibujado hace poco; si no, el actor no tiene tick.

**Tramos** (`Boardwalk`, `WoodenPostPath`): `Spec.Extent` es el largo (0 → 60 m; entre 15 m y el recorrido) por el eje X
local, centrado en el origen. Se montan con piezas instanciadas (`UInstancedStaticMeshComponent`, una por malla, con la
colisión de cada instancia); a lo largo y a lo ancho se escalan con `SizeScale`, el alto no.

| Pieza (`ETNBeachElement`) | Real | En el juego | Variantes | Colisión | Triángulos |
|---|---|---|---|---|---|
| Coco (`Coconut`) | 14-16 cm | 4-4,5 m de largo, 3,1-3,4 m de alto | pardo peludo, verde, partido en dos (una mitad boca arriba), germinando con brote | cápsula; las mitades, prisma y casquete | 100-290 |
| Medusa varada (`StrandedJellyfish`) | campana de 37 cm | 10,4 m de campana, 1,8 m de alto, brazos hasta 13 m | aurelia, aguamala, acalefo, clavel | casquete de 38° (se sube andando) | 770-900 |
| Anillas de latas (`SixPackRings`) | 21 × 14 cm | 6 × 4 m, 10 cm de grueso | casi todas cortadas, pocas cortadas, retorcidas | — | 700 |
| Sujetador rojo (`RedBra`) | copas de 14 cm | 11 m de ancho, copas de 3,9 m y 1,5 m de alto | liso, lunares, encaje con lazo y tirante en arco, una copa boca arriba | casquetes; la copa boca arriba, anillo de cajas | 530-600 |
| Almeja (`Clam`) | 5 cm | 1,4 m, 0,65 m de alto | crema, lila con rayos, naranja, gris con perla | caja | 390-440 |
| Concha de adorno (`DecorShell`) | 5-9 cm | 1,4-2,5 m | caracola, berberecho, porcelana moteada, vieira pálida | caja | 100-190 |
| Estrella de mar (`Starfish`) | 15 cm | 4 m, 0,55 m en el centro | naranja, roja con un brazo levantado, morada con un brazo corto, azul | casquete y cajas bajas en los brazos | 240 |
| Roca (`Rock`) | 35-45 cm | hasta 13 m, 3,2 m de alto | de estratos (4 escalones), losa inclinada, canto rodado, mesa con charco; arenisca, granito o pizarra | prismas por capa, caja inclinada o casquete | 70-240 |
| Grupo de rocas (`RockCluster`) | 1 m | 26 m, 4-4,8 m | 3 repartos: una grande de estratos, dos medianas de escalón, cantos y guijarros | prismas y casquetes | 1160-1220 |
| Restos de vela (`ShipSailWreck`) | mástil de 1,7 m | 48 m de mástil, lona hasta 8 m | 4 lonas con franja y remiendos | cápsula del mástil, cajón y 16 cajas por la lona (hueca por debajo) | 1290 |
| Tronco con musgo (`MossyLog`) | 85 × 12 cm | 23,5 × 3,4 m | en rampa sobre una piedra (de 0,9 a 6,1 m), medio enterrado (1,1 m), hueco para cruzarlo, con repisas de hongo (0,9 / 1,8 / 2,7 m) | cápsula, 8 cajas en tubo o cajas de las repisas | 200-510 |
| Tablones viejos (`OldPlanks`) | 50 × 9 × 2 cm | 14 × 2,5 × 0,56 m | pelados, azules con borde blanco, verde agua, rojos | una caja por tablón y el cajón (3,4 m) | 310-400 |
| Red de pesca (`FishingNet`) | 90 cm | 23 m, montón de 2,2 m, palo de 6 m | verde, azul, naranja, turquesa; bolas o corchos | casquete de 38° y el palo | 1780-2040 |
| Vaso de plástico (`PlasticCup`) | 9,5 cm | 2,7 m | rojo o transparente tumbado (se entra), de pie medio enterrado, aplastado | tubo de 8 cajas con suelo plano, anillo o caja | 170 |
| Botella (`Bottle`) | 25 cm | 7 m, 1,7 m de diámetro | verde tumbada, marrón clavada boca abajo, clara con un mensaje, verde clavada de culo | cápsulas | 310-380 |
| Chupachups (`Lollipop`) | 12-16 cm | 3,4-4,5 m | de bola con el envoltorio abierto, chupado con arena, espiral tumbada, espiral clavada (4 m) | esfera, caja o palo y disco | 140-220 |
| Corteza de sandía (`WatermelonRind`) | arco de 24 cm | 6,5 m de arco, 1 m de grueso | con carne, comida hasta lo blanco, de pie como un barquito, dos trozos | 4 cajas por el arco | 120-300 |
| Pajita (`Straw`) | 20 cm | 5,6 m | recta, doblada, de papel, clavada (4,5 m) | solo la clavada | 130-290 |
| Sombrilla clavada (`PlantedUmbrella`) | lona de 92 cm a 1,25 m | lona de 26 m a 35 m de alto | roja, azul, amarilla y naranja, turquesa (torcida 3-7°) | mástil, 16 cajas en la lona y el montón de arena del pie | 330 |
| Silla de playa (`BeachChair`) | 50 × 55 cm, respaldo de 68 cm | 14 × 15 m, asiento a 5,6 m, respaldo a 19 m | 4 lonas y toallas, bastidor de aluminio o blanco | patas, travesaño de atrás, asiento, respaldo, reposabrazos y la toalla-rampa | 720 |
| Castillo pequeño (`SandCastleSmall`) | 55 cm | 15 m, 3,2 m (bandera a 5,2 m) | 2 o 3 torrecillas, medio deshecho (bandera caída) | prismas y cajas | 790-1440 |
| Castillo enorme (`SandCastleHuge`) | 1,75 m | 49 m, 8,8 m (bandera a 12 m) | 4 colores de bandera | 100 formas: plataforma, rampa de 16°, murallas con puerta, torres, torreón, escalinatas | 3180 |
| Madera a la deriva (`Driftwood`) | 55 cm | 15 m | con horquilla, raíz, dos palos cruzados, rama en S | cápsulas por tramos | 140-420 |
| Pasarela (`Boardwalk`) | tablas de 50 × 9 cm | tramos de 12,8 m, 13 m de ancho, tablas a 1,1 m | tramo entero, sin tabla, rota, suelta levantada, movida; bajadas en los extremos | una caja por tabla y los pilotes | 160-190 por tramo |
| Caminito de palos (`WoodenPostPath`) | palos de 15 cm cada 20 cm | palos de 4,2 m cada 5,6 m, 5,4 m de ancho | palo recto, roto o con vueltas; cuerda, cabo azul o cinta de balizar | cápsula por palo (la cuerda se pasa por debajo) | 50-210 por palo, 72 por cuerda |
| Lata (`SodaCan`) | 12,2 cm | 3,4 m | roja, azul, aplastada, chafada de pie | cápsula, caja o prisma | 220 |
| Chapas (`BottleCaps`) | 3,2 cm | 0,9 m | 2-4, alguna boca arriba | — | 220-450 |
| Chanclas (`FlipFlop`) | 26 cm | 7,3 m | una, el par, del revés, con la tira rota | caja de la suela y cápsulas de la tira | 170-490 |
| Brick de zumo (`JuiceBox`) | 10,5 cm | 2,9 m | con pajita, de pie, aplastado, con la pajita envuelta | caja | 60-90 |
| Boya (`Buoy`) | 24 cm | 6,7 m | bola naranja con cabo, baliza de rayas, defensa blanca, bola amarilla con algas | esfera o cápsula | 290-460 |
| Toalla (`BeachTowel`) | 100 × 60 cm | 28 × 17 m | 1-3 pliegues, un extremo enrollado | caja plana, dos cajas por pliegue, cápsula del rollo | 780-870 |
| Crema solar (`SunscreenBottle`) | 17 cm | 4,8 m | tumbada con pegotes de crema, clavada del revés | caja | 190-290 |
| Palitos de helado (`PopsicleSticks`) | 11,4 cm | 3,2 m | 1-3, con restos de helado | — | 60-120 |
| Cáscaras (`SnackShells`) | 1-1,5 cm | 0,3-0,4 m | 8-13 de pipas, pistachos y cacahuetes | — | 170-270 |
| Trozo de cuerda (`RopePiece`) | 40 cm | 11 m | en S, en lazada, con nudo, cabo corto | — | 160-350 |
| Gafas de sol (`Sunglasses`) | 14 cm | 3,9 m de ancho, 1,4 m de alto | negras, rojas de espejo plegadas, de carey sin un cristal, de corazón | caja del frente y cápsulas de las patillas | 400-420 |
| Cubito de juguete (`ToyBucket`) | 9 cm | 2,5 m | con flan de estrella, flan de tortuga, molde de pez o rastrillo | prisma del cubo y el flan | 290-410 |
| Pelota hinchable (`BeachBall`) | 30 cm | 8,4 m | clásica, medio deshinchada, pastel, azul y blanca | esfera o casquete | 180 |
| Disco volador (`Frisbee`) | 27 cm | 7,6 m, 1 m de alto | boca abajo (se sube), boca arriba como un cuenco, de canto medio enterrado | prisma, cuenco o caja | 220 |
| Hueso de sepia (`Cuttlebone`) | 15 cm | 4,2 m | 4 (uno partido) | caja | 140 |
| Patito de goma (`RubberDuck`) | 9 cm | 2,5 m | amarillo, descolorido y tumbado, rosa, azul | cápsula y esfera | 330 |
| Pluma de gaviota (`GullFeather`) | 15 cm | 4,2 m | blanca con la punta negra, gris | — | 130 |

Las 17 últimas filas (de `SodaCan` a `GullFeather`) amplían el contrato: van en `ETNBeachElement` antes de `BarbedWire`
(cuentan como decorado) con su huella en `TNBeach::FootprintRadius`; el reparto las coge solas.

**Probar.** Una ronda con semilla fija (`open LVL_BeachRace?BeachSeed=42`) y recorrerla: subir a la medusa, la red, la
toalla de la silla (y colarse debajo del asiento), la losa y las rocas de estratos, el castillo enorme hasta la cima y la
vela hasta el mástil; entrar en el vaso tumbado y en el tronco hueco; pasar por la pasarela (y caer por la tabla que
falta) y chocar con los palos del caminito. Mirar que la almeja se abre, la medusa respira y la vela, la red y las
banderas se mueven solo de cerca, y que nada del decorado se sale de su huella ni flota en las dunas (hundimiento).

### Decorado militar (la tropa de Tortunavy)

Recetas en `Private/World/Beach/TN_BeachMilitaryMeshes.h`, que `TN_BeachPropMeshes.h` incluye al final: `BuildDecor` le
pasa los siete elementos militares (`BuildMilitaryDecor`) y `NumVariants` le pregunta sus variantes
(`NumMilitaryVariants`). Todo lo demás es lo del decorado: malla compartida por variante, colisión simple en el
`BodySetup`, hundimiento e inclinación por ejemplar y `SizeScale` en el componente. Paleta de la tienda del General
Galápago del cuartel (verde oliva, caqui, azul marino y oro). La **escarapela de Tortunavy** es una estrella dorada de
cinco puntas sobre un disco azul marino con aro dorado (cajas, bidones, cascos); la **bandera de Tortunavy**, azul marino
con la estrella dorada por las dos caras. Alturas para la tortuga (1,4 m; salta 1,2 m): escalones de 80 cm como mucho y
1,1-1,2 m para cubrirse.

| Pieza (`ETNBeachElement`) | Real | En el juego | Variantes | Colisión | Triángulos |
|---|---|---|---|---|---|
| Parapeto de sacos terreros (`Sandbags`) | sacos de 4,3 × 2,4 × 1,2 cm | sacos de 1,2 × 0,66 × 0,34 m; 4 hileras (1,2 m) con banqueta de 2 (63 cm); recto de 14,4 m, media luna de 5,4 m de radio | 6: recto; media luna; recto a medio desmoronar; esquina en L con la bandera; nido redondo de 3 hileras (91 cm, 8 m) con bandera, caja y un soldadito vigía; media luna a medio desmoronar | una caja por tramo de igual alto (extremos en escalera de 28 cm; en los arcos, tramos de 20° como mucho) y una por saco caído | 2300-3750 |
| Caja de munición (`AmmoCrate`) | 5,4 × 3,1 × 2,7 cm | 1,52 × 0,88 × 0,75 m; cartuchos de 76 cm; lata de 0,9 × 0,42 × 0,57 m | cerrada con una lata y cartuchos sueltos; tres pilas en escalera (0,75, 1,5 y 2,25 m); abierta y llena de cartuchos con la tapa de rampa (28°) hasta el canto; volcada con los cartuchos desparramados y otra caja con la lata encima | caja por caja (la abierta: paredes, fondo y el lecho de cartuchos a 44 cm) y la tapa | 700-2500 |
| Erizo antitanque (`TankTrap`) | barras de 28 cm (raíles de un tren de juguete) | barras de 7,8 m a 35°: cruce a 2,25 m (se pasa por debajo entre dos patas), puntas a 4,5 m | raíles oxidados de doble T; raíles con algas colgando; madera a la deriva atada con cuerda; pareja pequeña (raíl y madera) | una cápsula por barra | 280-800 |
| Casco militar (`MilitaryHelmet`) | 23,6 × 20 cm, 9,5 de hondo | 6,6 × 5,7 m, 2,65 m de hondo | boca abajo y medio enterrado con la escarapela (asoma 1,7 m; la banda de abajo es empinada, pero desde ~50 cm la cúpula se anda: se sube de un salto); boca arriba como un cuenco (borde a 90 cm, arena y un charco dentro a 18 cm); apoyado en un palo, con redecilla y una lata escondida debajo (la boca, a 2 m: se mete una debajo); de lado como una cueva (boca de 4,3 × 7 m, suelo de arena) | 8 cajas finas tangentes a la cúpula por banda de latitud (hueco por dentro), el suelo de arena y el palo | 520-720 |
| Red de camuflaje (`CamoNet`) | 71 × 56 cm sobre palos de 13-16 cm | 20 × 15,6 m, palos de 3,7-4,5 m | plana sobre cuatro palos y uno en medio (lados a 1,7 m) con un faldón que se mece; a dos aguas como un túnel (faldones de 26° que se suben hasta la cumbrera, a 4,3 m); caída por un lado (rampa de 16° hasta un techo a 3,8 m del que se salta); la plana sobre un puesto de vigía (anillo de sacos, caja y soldadito) | los palos; en el túnel, los faldones; en la caída, la rampa y el techo | 1200-3600 |
| Bidón (`Jerrycan`) | 7,7 × 2,9 × 10,7 cm | 2,15 × 0,8 × 3 m | de pie, verde oliva con la escarapela (3 m: para cubrirse); tumbado, caqui (80 cm: un escalón); dos tumbados en cruz, verde y rojo (80 y 160 cm); rojo de gasolina tumbado con el tapón abierto y un charco tornasolado delante | caja | 350-700 |
| Soldaditos de juguete (`ToySoldiers`) | 5 cm | 1,4 m (como la tortuga), peana de 64 × 48 cm | 8: fusil; prismáticos; bazuca de rodillas; tumbado; pareja; trío con uno volcado; uno volcado y otro mirando; patrulla de tres | una cápsula por soldadito (el tumbado, una caja de 40 cm que se sube) | 500-1600 |

- **Orientación**: los parapetos siguen la de los alargados del reparto: el largo por la X local, la cara alta hacia -Y
  y la banqueta y la bandera hacia +Y (lo que se defiende). No giran al azar (solo ±6°): así el reparto los pone en arco
  delante de un puesto. El resto gira libre.
- **Movimiento** (solo de cerca, como el resto del decorado): la bandera de Tortunavy ondea (esquina y nido) y el faldón
  suelto de la red se mece. La cámara atraviesa todo lo militar (es bajo o hueco).

**Probar.** `TN.Beach.Place Sandbags 1 0 <semilla>` (y `AmmoCrate`, `TankTrap`, `MilitaryHelmet`, `CamoNet`,
`Jerrycan`, `ToySoldiers`; la variante sale de la semilla, así que conviene probar varias). Subir al parapeto por la
banqueta y por los extremos en escalera y agacharse detrás; subir la escalera de cajas hasta 2,25 m y la tapa de la caja
abierta y meterse dentro; pasar por debajo del erizo grande entre dos patas; subir de un salto al casco enterrado,
meterse en el cuenco, debajo del casco apoyado en el palo y dentro del casco de lado; cruzar el túnel de red y subir por
sus faldones hasta la cumbrera, subir por la red caída y saltar desde el techo; subir a los bidones tumbados y cruzados.
Que nada se salga de su huella (`TN.Beach.ShowFootprints 1`) ni flote.

## Trampas e interacciones (`World/Beach/`)

| Archivo | Qué es |
|---|---|
| `TN_BeachBarbedWire.*` | `ATN_BeachBarbedWire`: alambre de espino enrollado; tocarlo aturde y empuja |
| `TN_BeachSeaweed.*` | `ATN_BeachSeaweed`: montón de algas que enredan (y algas que se enrollan a la tortuga) |
| `TN_BeachWobblyPlatform.*` | `ATN_BeachWobblyPlatform`: tabla o tapa de nevera sobre un hoyo; se tambalea y con dos se parte |
| `TN_BeachBrokenBucket.*` | `ATN_BeachBrokenBucket`: cubo tumbado y roto que se cruza como un túnel |
| `TN_BeachSpadeRamp.*` | `ATN_BeachSpadeRamp`: pala gigante, balancín con trampolín o puente-trampolín entre dos alturas |
| `TN_BeachShellGate.*` | `ATN_BeachShellGate`: puerta de dos hojas de conchas (en pared, entre rocas o desnuda) con interruptor |
| `TN_BeachSandDungeon.*` | `ATN_BeachSandDungeon`: castillo de arena con salas que se atraviesa |
| `TN_BeachMine.*` | `ATN_BeachMine`: mina de juguete medio enterrada; al pisarla explota y lanza en bola hacia atrás |
| `TN_BeachMineSynth.*` | `UTN_BeachMineSynthComponent`: sonidos sintetizados de la mina (clic, pitido, explosión, lluvia de arena y rearme) |
| `TN_BeachClamTrap.*` | `ATN_BeachClamTrap`: almeja gigante que se cierra, atrapa unos segundos, tiembla echando humo y escupe mareada |
| `TN_BeachMovingPlatform.*` | `ATN_BeachMovingPlatform`: balsa que va y viene sobre un charco de verdad, o ascensor a una torre con catapulta |
| `TN_BeachCatapult.*` | `ATN_BeachCatapult`: cuchara sobre un tapón con cubito de contrapeso; lanza en bola hacia el mar |
| `TN_BeachTrampoline.*` | `ATN_BeachTrampoline`: medusa gorda, colchoneta, donut o sombrero de paja que rebotan y se deforman |
| `Private/World/Beach/TN_BeachRideKit.h` | Lo común de esas cuatro: hacia dónde queda el mar, quién puede montar, lanzar en bola, mareo y vaivén con el reloj del servidor |
| `TN_BeachTrapSynthComponent.*` | `UTN_BeachTrapSynthComponent`: sonidos sintetizados de las trampas (chispazo, «¡ay!», chof, crujido, chasquido, muelle, golpe sordo, conchas, roce de arena y clic) |
| `TN_BeachTrapCommon.*` | Reloj del servidor suavizado, estallidos de partículas low-poly y texto emergente («¡AY!», «¡CRAC!») |
| `Private/World/Beach/TN_BeachTrapKit.h` | Reglas comunes (quién simula a quién, quién puede caer en una trampa) y piezas de arena |
| `Private/World/Beach/TN_BeachDebugCommands.cpp` | Consola `TN.Beach.Place` |

**Convenciones** (las de `TN_BeachLayout.h`): origen en la arena, en el centro; X local = sentido de la carrera (se
entra por -X y se sale por +X); los alargados (el alambre) tienen el largo por X y la huella es su semigrosor en Y. Todo
cabe dentro de la huella del contrato con su `SizeScale` (lo que tiene medidas de tortuga, como puertas, pasillos y
peldaños de 40 cm, no se escala: se ajusta la planta). Mallas en ejecución con color de vértice
(`M_CosmeticVertexColor`, kit del parque del lobby), `RF_Transient | RF_DuplicateTransient`; colisión convexa por
piezas. Todas son `bAlwaysRelevant` con pocas actualizaciones por segundo y `ForceNetUpdate` en cada cambio. Solo
cuentan las tortugas vivas, fuera del caparazón y sin aturdir (`TNBeach::IsTurtleStunned`): los enemigos no caen en
trampas y una tortuga aturdida no se engancha ni se pincha. Variantes con la semilla del `Spec`.

**Qué significa `Spec.Extent`** en cada una: alambre = largo (0 → 24 m); algas = ancho máximo en Y (0 → libre; para
meterlas en un pasillo); puerta de conchas = ancho del hueco de una puerta «desnuda» (0 → con su pared o sus rocas); el
resto no lo usa.

### Alambre de espino (`ATN_BeachBarbedWire`)

- Concertina de rollos de 66·`SizeScale` cm de radio (52-84: ~1,3 m de alto) a lo largo de X, ±56 cm en Y, con pinchos
  en cruz, un hilo tenso por encima, estacas de madera cada ~7 m y trozos sueltos en los extremos. Bloquea (cajas por
  tramos de ~4 m; la cámara las atraviesa).
- **Servidor**: si la cápsula queda a menos de 14 cm de la colisión (a los lados o encima), `TNBeach::StunTurtle` 1,2 s
  (`StunSeconds`) con un empujón de 750 cm/s hacia el lado del que venía (si ya está encima, al contrario de su marcha)
  y 420 hacia arriba. Una vez por tortuga cada 2,7 s (aturdimiento + `HitCooldown` 1,5 s).
- Efectos en todas las máquinas (multicast no fiable): chispas, esquirlas, chispazo y «¡ay!» sintetizados y un «¡AY!»
  rojo que sale flotando sobre la tortuga.

### Algas que enredan (`ATN_BeachSeaweed`)

- Elipse de 0,9·huella (630 cm con `SizeScale` 1) en X y 78-100 % de eso en Y (o `Extent`/2), pila de 25-65 cm de
  gotas verdes y marrones, cintas onduladas con vesículas, siete tallos de pie que se mecen y mancha de arena mojada.
  Sin colisión: se vadea con las patas dentro.
- **Enganche** (servidor): quien pisa el 85 % central con los pies en el suelo se queda enganchada 3,2 s
  (`CatchSeconds`): anda a 55 cm/s (`HeldSpeed`, también en el aire y en el panzazo) y el salto no la levanta
  (`JumpZVelocity` = 0: cada intento es un tirón y la animación de salto). Cada salto adelanta la suelta 0,4 s
  (`JumpTug`) y cada meneo (cambiar de golpe la dirección del movimiento, como mucho uno cada 0,12 s) 0,2 s
  (`WiggleTug`): machacando el salto sale en ~1,3 s. Si la arrastran fuera de la elipse (×1,2), se suelta. Tras
  soltarse, 2,5 s de gracia (`ImmuneSeconds`); mientras siga dentro vadea a 320 cm/s (`WadeSpeed`). Hasta 4 a la vez.
- **Red**: `Catches` replicado (tortuga, hora de la suelta y tirones). El tope de velocidad (`UTN_StaminaComponent`) y
  el salto se tocan en el servidor y en el cliente dueño, como `TN_SlowZoneVolume`; el dueño predice el enganche al
  pisarla (si el servidor no lo confirma en 0,6 s, lo deshace) y siente sus tirones al momento. Las hebras que se
  enrollan (5 por tortuga: suben en espiral al engancharse, se sacuden con cada tirón y caen al soltarse) y los chofs
  salen del estado replicado en cada máquina. Si la aturden, se suelta sin tocar el tope del caparazón.

### Plataforma sobre un hoyo (`ATN_BeachWobblyPlatform`)

- **El hoyo va en la malla** (el terreno no se cava, `TN_BeachLayout.h`): cráter de arena amontonada con el fondo a ras
  del suelo. Con `SizeScale` 1: borde de fuera a 870 cm, cresta redondeada de 90 cm de ancho a 240 de alto (0,27·huella,
  180-240), ladera de fuera a 30° (se sube andando), pared de dentro empinada y hoyo de 364 de radio arriba; en el lado
  +Y, una brecha de ~2,4 m por la que se sale andando del fondo. Fondo mojado, guijarros, una concha y un banderín en
  la cresta.
- **Encima**, a lo largo de X y apoyada 70 cm en la cresta por cada lado: tabla vieja de tres tablones (868 x 220 x 24,
  con travesaños, clavos y una grieta pintada en el centro) o tapa de nevera (plástico blanco con reborde de color,
  bisagras y pegatina; hasta 300 de ancho y 28 de grueso), según la semilla.
- **Tambaleo** (cada máquina con las tortugas que ve encima; la tabla es una base móvil): se ladea 4,5° por tortuga
  según dónde pise (hasta 7°, `MaxRollDeg`), cabecea hasta 2°, se mece al andar y los aterrizajes (caída > 250 cm/s) la
  sacuden; muelle poco amortiguado (~1,2 Hz). Crujidos al pisar y al andar.
- **Rotura** (servidor): con 2 o más tortugas a la vez (`BreakRiders`) la grieta sube y en 1,1 s (`CrackSeconds`) se
  parte; si se bajan, baja a 0,45/s. Mientras, tiembla, se hunde unos centímetros y cruje cada vez más agudo y seguido,
  soltando astillas. Al partirse: chasquido, astillas, «¡CRAC!», las mitades resbalan 70 cm hacia dentro y caen (0,5 s,
  con un botecito) hasta el fondo y a los 0,8 s son rampas de ~34° de la cresta al fondo. Quien estuviera encima cae al
  hoyo y sale por la brecha o subiendo por una mitad. No se recompone en la ronda. `BrokenAt` (hora del servidor)
  replicado: quien llega tarde las ve ya caídas.

### Cubo roto (`ATN_BeachBrokenBucket`)

- Cubo de juguete de 18 cm a escala: 504 de largo, 238 de radio en la boca y 182 en el culo, pared de 26 (x0,7-1,15
  para caber en la huella). Tumbado a lo largo de X, apoyado en la arena: boca en -X con rampa de arena (170) al suelo
  de dentro (plano, a 69 cm), culo en +X con un agujero dentado del 74 % del radio y una lengua de arena (150) que baja.
  ~2,5 m libres en la salida: se cruza de pie. Asa caída sobre el lomo, nervios, reborde, pegatina de estrella, grietas y
  colores de juguete desteñidos. Estático (sin Tick).

### Pala (`ATN_BeachSpadeRamp`)

- Pala de 1120 cm con `SizeScale` 1 (entre 700 y 1150 según la huella): hoja del 36 % (336 de ancho) y mango de 84 de
  ancho y 34 de grueso por el que se anda. Dos variantes según la paridad de `Spec.Seed`:
- **Balancín** (par): sobre una piedra redonda (fulcro a 150 cm), la hoja en la arena (-X) y el mango en alto (+X,
  ~3,3 m; 17° en reposo). **Trampolín**: saltar en el último 20 % del mango lanza 1250 cm/s arriba y 700 hacia +X
  (conserva la mitad de la velocidad de lado). **Vuelta**: si una tortuga cae de un salto sobre la mitad del mango (a más
  de 60 cm del fulcro, con más de 380 cm/s de caída), la pala gira: el mango golpea la arena en 0,16 s (golpe sordo,
  muelle, polvo), se queda hasta 0,9 s y vuelve sola a los 2,2 s. A quien esté en la hoja lo lanza 1500 arriba y 1050
  hacia +X (más lejos que el trampolín). Inmunes a la caída hasta aterrizar.
- **Puente-trampolín** (impar): montículo de arena (mango, 70-100 cm de alto, laderas < 32°) y roca de 2,2-2,8 m (con un
  charco en medio); la pala sube de uno a otra (~20-30°) y el 70 % de la hoja asoma por fuera de la roca como un trampolín de
  piscina: saltar en su 30 % de la punta lanza igual que arriba.
- **Red**: el trampolín lo aplican el servidor y el cliente dueño dentro del mismo movimiento (el salto cambia el modo de
  movimiento y el lanzamiento entra en ese paso), como la medusa del lobby. La vuelta la decide el servidor (`FlipAt`
  replicado; cada máquina gira la pala desde esa hora) y lanza a las víctimas; el cliente de cada víctima aplica el mismo
  lanzamiento al recibir el aviso (fiable), así la corrección queda pequeña.

### Puerta de conchas (`ATN_BeachShellGate`)

- Hueco de 300 x 330 con dos hojas de 147 de ancho: mosaico de conchas de vieira en filas escalonadas por las dos caras,
  marco de cuerda y una perla de tirador; bastidor de madera de deriva con estrella. Según la semilla, en una pared de
  arena de ±(0,95·huella − 10) (±560 cm), 4,4 m de alto y 2 m de grueso con almenas y conchas, o entre dos rocas de
  ~4,2 m. Interruptor: concha grande en una peana de 85 cm de radio a 3,8 m por delante (-X) y a un lado, con un
  caminito de conchitas hasta la puerta.
- **Reglas** (servidor): se abre empujando 0,6 s (`PushSeconds`: andar contra una hoja cerrada, pegada a ella, desde
  cualquier lado; se abre hacia el otro) o pisando el interruptor (hacia +X). Se abre en 0,45 s hasta 95° (`OpenDeg`,
  con un pasito de más), sigue abierta mientras haya alguien en el hueco (±2,1 m) o en el interruptor y 3 s más
  (`OpenHold`), y se cierra en 0,7 s; si alguien entra mientras se cierra, se vuelve a abrir. Las hojas solo chocan
  cerradas. Castañeteo de conchas y roce de arena al moverse; clic al pisar el interruptor.
- **Desnuda** (`Extent` > 0, la usa el castillo): solo bastidor (con dintel hasta `BareDoorHeight` = 420), hojas e
  interruptor, siempre a -Y.

### Castillo de arena con salas (`ATN_BeachSandDungeon`)

- Con `SizeScale` 1: 57,6 x 44 m (el recorrido por dentro, ~70 m; rodearlo por el hueco del embudo de sus alas, más).
  Murallas de 2,4 m de grueso y 8,2 m de alto con almenas, cuatro torres de 11,5 m con banderas, zócalo de 50 cm (el
  suelo de dentro). Todo con colisión que también para la cámara.
- **Recorrido**: arco de entrada (4 x 4,6 m, a -Y) con rampita → **sala de las columnas** (planta baja, tres columnas de
  cubos) → **puerta de conchas** desnuda de 3,2 m con su interruptor en la sala → **pasillo de las algas** (3,6 m de
  ancho, techo a 4,3 m con lucernarios, algas que enredan) → **escalera** de 8 peldaños de 40 x 45 cm → **sala de las ventanas** (piso de arriba,
  +3,2 m, con la terraza sobre el pasillo): dos muretes de 70 cm que se saltan o se rodean por el hueco de cada uno y
  ventanas de 2 x 1,9 m (a 80 cm del suelo) al mar y a la playa; se puede saltar por ellas (~4 m) → **salida** por la
  puerta alta de la muralla +X (3,6 x 4 m) a una rampa de arena de 6,4 m (30°).
- Las piezas de dentro las crea el servidor con `ATN_BeachElement::SpawnElement` (puerta de conchas `Extent` 320;
  algas de `SizeScale` 0,3-0,6 y `Extent` 320) y las destruye con el castillo. Con `bSpawnEnemiesInside` (apagado)
  crearía además un erizo (0,35) junto a la escalera y un cangrejo (0,4) en la sala de las columnas: está apagado porque
  los enemigos de ahora no bajan de 0,75-0,8 de tamaño, se alejan 16-38 m de su sitio sin chocar con paredes y buscan
  el suelo desde arriba (se subirían al techo del pasillo).

### Mina (`ATN_BeachMine`)

- **Aspecto** (con `SizeScale` 1; las mallas son de ese tamaño y las comparten todas las minas del mismo aspecto, que
  escalan sus componentes: en una ronda hay decenas): montoncito de arena removida de 1,28 m de radio con una mina de
  juguete de ~5 cm (1,4 m) medio enterrada: plato verde oliva con franja amarilla, bote gris de tres pinchos, plato
  oxidado con percebes o juguete caqui con botón rojo, según la semilla. Asoman la tapa (22 cm; el bote, 16) y el pincho
  de la espoleta (hasta 30 cm por encima); un piloto rojo da un destello cada 1,6 s. El 45 % lleva una banderita roja de
  aviso con franja blanca a 1,9 m (1,85 m de alto). Todo cabe en la huella (3,5 m). Sin colisión: se pisa.
- **Pisada** (servidor): una tortuga viva, fuera del caparazón y sin aturdir con los pies sobre la tapa (a menos de
  70 cm + el 45 % del radio de su cápsula en planta y entre 60 cm por debajo y 45 cm por encima de la tapa: también
  cayendo encima de un salto; saltándola por encima, no). «¡clic!» (doble clic de plástico y el texto), la tapa se hunde
  5 cm y parpadea en rojo pitando cada vez más deprisa (de 5 a 17 veces por segundo) durante `FuseSeconds` (0,4 s).
- **Explosión** (servidor): toda tortuga viva a menos de `BlastRadius` (2,3 m) —y la que la pisó hasta el doble, aunque
  se haya alejado durante la mecha— sale en bola hacia atrás en la carrera (contrario al mar del generador; sin
  generador, hacia -X de la mina): `TNBeach::StunTurtle` 2,4 s (`StunSeconds`) con 420 cm/s hacia atrás (`LaunchBack`),
  950 hacia arriba (`LaunchUp`) y hasta 160 de lado según dónde estaba (si la pisan dos, no caen juntas): ~1,9 s de vuelo
  y ~7-8 m hacia atrás. Las de alrededor, hasta `PushRadius` (7 m), en pie y sin aturdir, reciben un empujón sin
  aturdir hacia fuera y algo hacia atrás (a quien va delante no le da un acelerón): de 750 cm/s y 450 hacia arriba
  (`PushSpeed`, `PushUp`) junto a la explosión a un tercio en el borde; lo aplica también su cliente dueño (multicast,
  como el susto del lagarto). Las medidas escalan con `SizeScale`.
- **Efectos** (cada máquina con pantalla): fogonazo (bola de 2,4 m que crece y se apaga en 0,23 s y una luz naranja de
  0,28 s), bola de fuego, nube de arena, terrones, trozos de la carcasa, humo que sube 2-3 s, «¡BUM!», temblor de cámara
  (0,8 hasta 7 m, apagándose hasta 32 m) y, sintetizados (`UTN_BeachMineSynthComponent`), la explosión (chasquido que
  rasga, golpe grave que cae, retumbo y crepitar) y después la arena y las piedrecitas que caen. Las partículas se crean
  con la primera explosión de cada mina.
- **Cráter y rearme**: queda un cráter de adorno (suelo chamuscado de 1,5 m, reborde de arena de 26 cm hasta 2,6 m,
  rayas de quemado y trozos de la carcasa). **Decisión: se rearma.** A los `RearmSeconds` (9 s) la mina vuelve a asomar
  en medio del cráter con un botecito, un clic-clac y un poco de arena, y el cráter se queda de aviso: todas las
  tortugas se encuentran la misma playa, y mientras tanto quien viene justo detrás pasa sin peligro. Con
  `RearmSeconds` = 0 no se rearma en la ronda.
- **Red**: el servidor replica dos horas suyas, `TriggeredAt` (pisada) y `ExplodedAt` (explosión); cada máquina anima el
  parpadeo, los pitidos, la explosión, el cráter y el rearme con su reloj del servidor suavizado. Quien llega tarde ve el
  estado sin oírlo. Registro: `[Playa] Mina ...: explota (N en bola, M empujadas)` (y `Verbose`: aspecto y pisadas).
- **Probar**: `TN.Beach.Place Mine [Tamaño] 0 [Semilla]` (el aspecto y la bandera salen de la semilla). Pisarla andando,
  esprintando (sale lanzada igual) y cayendo encima; saltarla por encima; quedarse a 3-6 m de la que pisa otra (empujón
  sin bola); ver el cráter y el rearme a los 9 s; una tortuga aturdida o rodando en su caparazón no la pisa. Con un
  cliente: la bola vuela igual en las dos pantallas y el empujón no da tirones de corrección.

### Segunda tanda: concha, plataforma móvil, catapulta y trampolín (comunes)

- **Orientación**: la catapulta, la plataforma móvil y el trampolín siguen su X local si ya mira al mar (±45°: el reparto
  los orienta así y les deja libre el arco de salto por delante); si no, se giran solos hacia el mar
  (`ATN_BeachRaceGenerator::GetSeaDirection`; sin generador, su +X). La concha se orienta hacia el mar ±25°. Todo gira
  dentro de la huella, que es redonda.
- **Quién monta**: tortugas vivas, fuera del caparazón, sin aturdir, sin derribar, sin que las lleve nadie y sin llevar a
  nadie (`TNBeachRideKit::IsFreeRider`). La concha y la catapulta no actúan en el recuento ni en el podio.
- **Red**: nada replica posiciones. Las horas del servidor (cierre de la concha, disparo de la catapulta) se replican y
  cada máquina anima con el reloj del servidor suavizado; la plataforma se mueve con una función de la hora del servidor
  y una fase por la semilla. Los lanzamientos de la catapulta son bolas de caparazón con física (la caja se replica sola:
  sin predicción del movimiento). El rebote del trampolín y el saltito de la concha los aplican a la vez el servidor y el
  cliente dueño.
- **En cadena**: las bolas de la catapulta rebotan en los trampolines y en el charco salen del caparazón y nadan; la que
  para dentro de una concha y sale de la bola se la come; la torre del ascensor lleva una catapulta arriba.

### Concha que atrapa (`ATN_BeachClamTrap`)

- **Aspecto** (con `SizeScale` 1): almeja gigante de 32 cm reales, **9 x 6,5 m**, con la valva de abajo medio enterrada
  (labio en zigzag a 38 cm de la arena, montículo de arena alrededor que se sube andando, conchitas y guijarros), el manto
  de colores dentro (azul eléctrico, verde esmeralda con oro, morado con azul o dorado con ojos azules, según la semilla),
  sifón y una perla de ~50 cm que destella (estrella de cuatro puntas) cuando está lista. La valva de arriba, con cinco
  pliegues y anillos de crecimiento por fuera y nácar por dentro, está abierta **68°** sobre la charnela (a +Y o -Y según
  la semilla) y respira ±2°. Cerrada deja ~1,7 m libres dentro y una rendija de 5 cm entre los labios.
- **Cierre** (servidor): una tortuga libre que pisa el manto (el 72 % central de la elipse, con los pies en el suelo)
  la hace temblar **0,25 s** (`TellSeconds`: la valva sube 7° y castañetea) y cerrarse de golpe en **0,14 s**
  (`CloseSeconds`). Atrapa a la tortuga libre más cercana al centro que siga dentro (el 86 % central; una sola): quien
  corre y sale en el aviso se escapa. A las demás que estén en la valva las despide **700 cm/s** hacia fuera y **450**
  hacia arriba (su cliente aplica el mismo empujón). Sin nadie dentro se queda cerrada **1 s** y se abre.
- **Dentro** (**3,2-4 s** al azar, `HoldMin`/`HoldMax`): la presa queda en el centro, quieta y sin control (`MOVE_None`
  en el servidor y en su cliente a la vez, como el probador del lobby, y sin teclas de mover); su cámara pasa a la de la
  concha (fuera, del lado de la boca, a ~12 m y 4,7 m de alto) con fundido de 0,35 s. La concha vibra a sacudidas (la
  presa pataleando: una cada 0,62 s, +0-0,18 s) con golpes sordos, algún «¡ay!» ahogado y humo y burbujas de arena por la
  rendija; en los últimos 0,7 s tiembla sin parar y echa humo seguido. «¡ÑAM!» al atraparla.
- **Suelta**: se abre en **0,4 s** (con un pasito de más) y a los **0,16 s** (`SpitDelay`) la escupe de un saltito como
  al salir del huevo o del probador: **560 cm/s** hacia el lado de la boca y hacia el mar y **560** hacia arriba
  (~6 m), con «¡PTUI!» y burbujas. Queda **mareada 1 s** desde que aterriza (`DizzySeconds`; tope de 3 s desde el
  saltito): pajaritos del mareo y sin mover las patas; la cámara vuelve a ella en 0,4 s. No la vuelve a atrapar esa
  concha en 3 s.
- **Recarga**: **4,5 s** tras abrirse (`RechargeSeconds`) sin cerrarse, con el manto encogido y la perla apagada; al
  estar lista, el manto se abre, la perla destella y suena un «plin».
- **Se suelta antes** (sin saltito ni mareo) si la presa muere, la aturden, se mete en el caparazón, la derriban, la
  cogen o se desconecta; en el recuento o el podio la deja donde está (el GameMode la tiene congelada).
- **Red**: `State` replicado (`SnapAt`, `OpenAt`, `SpitAt`, `Captive`). El cliente de la presa la sujeta al recibir
  `Captive` y la suelta él solo a la hora `OpenAt + SpitDelay` (no espera a la réplica), igual que el servidor.
- **Probar**: `TN.Beach.Place ClamTrap [Tamaño] 0 [Semilla]`. Entrar andando al manto (se cierra y atrapa; la cámara sale
  fuera; tiembla y humea; escupe y mareo 1 s); cruzarla esprintando por el borde (escapar en el aviso); dos tortugas
  dentro (una atrapada, la otra despedida); pisarla durante la recarga (nada); `TN.Race.Stun` a la atrapada (se abre ya).
  Con un cliente atrapado: sin tirones al entrar ni al salir, y el saltito a la vez en las dos pantallas.

### Plataforma móvil (`ATN_BeachMovingPlatform`)

- **Balsa** (semilla par): charco dentro de un cráter de arena (cresta a **1,7 m**, taludes de 30° que se suben andando
  por fuera y por dentro), con agua de verdad **1,28 m** de honda (`ATN_ProcWaterVolume` local en cada máquina, solo en lo
  hondo: quien cae nada despacio y sale andando por el talud). Encima flota y va y viene a lo largo de X, de orilla a
  orilla (su punta se mete 60 cm en el talud: se sube y se baja andando), una chancla (6,8 x 2,5 m), una tabla de surf
  de juguete (6 x 2 m), un disco volador (5,6 m, gira 14°/s) o la tapa de una fiambrera (5,4 x 3,7 m), según la semilla
  (x0,85-1,15 con el tamaño). Recorrido **8 m** (`Spec.Extent`; se recorta para caber: charco de 12-14 m), **3,3 m/s**
  (`FerrySpeed`), **1,6 s** de espera en cada orilla (`FerryDwell`), meciéndose ±3 cm y ±1,2°. Andar encima en su
  sentido suma las dos velocidades. Embarcaderos de palos de polo en cada orilla; espuma, una hoja, una chapa y una
  concha flotando.
- **Ascensor** (semilla impar): torre cuadrada de arena de molde de **~13 m** de lado (0,54·huella de semilado) y
  **4,5 m** de alto (`Spec.Extent`, 2,5-4,8 m: saltar desde arriba no mete en el caparazón), con marcas de cubo, almenas en
  los lados ±Y, conchas y bandera. Por su cara -X sube y baja una bandeja o un disco volador de **3,9 m** colgado con
  cuatro cuerdas de una grúa de palos de polo con una chapa por polea: **1,7 m/s** (`LiftSpeed`), **2 s** abajo y
  **1,6 s** arriba. Arriba espera una **catapulta** (la crea el servidor, de tamaño 0,95·semilado/900 ≈ 0,69, y la
  destruye con la torre; `bCatapultOnTop`): desde 4,5 m lanza aún más lejos.
- **Base móvil**: la balsa y la bandeja son colisión convexa con nombre estable por red; quien va encima se mueve con ella
  sin resbalar (base de movimiento de UE: el cliente manda su posición relativa a la base, así que el desfase de reloj
  no corrige). Posición = `TNBeachRideKit::ShuttleAlpha(hora del servidor + fase por la semilla)`: arranca y frena
  suave, igual en todas las máquinas. Crujido o roce al salir; golpe o chapoteo al llegar.
- **Probar**: `TN.Beach.Place MovingPlatform 1 0 2` (balsa) y `... 1 0 3` (ascensor); `... 1 1200 2` (balsa con 12 m de
  recorrido). Subir a la balsa en una orilla, cruzar (y andar encima), caer al agua y salir nadando por el talud; subir
  al ascensor, llegar arriba, usar la catapulta y saltar desde la torre. Con un cliente encima: sin resbalar ni tirones.

### Catapulta (`ATN_BeachCatapult`)

- **Aspecto** (con `SizeScale` 1): brazo de **11 m** (el 68 % del lado del cazo) apoyado como un balancín sobre un tapón
  de garrafa (rojo, azul, verde o blanco, con estrías y una cuna) encima de una piedra: eje a **2,5 m**. El brazo es,
  según la semilla, una cuchara de plástico de color, una de madera con vetas o dos palos de polo atados con gomas y un
  vasito de yogur por cazo. Cazo de **~3 x 2,2 m** y 30 cm de hondo (medidas de tortuga: no baja de 2,5 x 2 m), en
  reposo apoyado en la arena (el brazo a ~22°, se entra andando); en el otro extremo, un cubito de arena mojada
  (88 cm de radio, 70 de alto) y un palo de polo de pie que sujeta el mango en alto. Banderín verde (lista) o rojo.
- **Disparo** (servidor): una tortuga libre en el cazo la arma: **1 s** de aviso (`WarnSeconds`: «¡AGÁRRATE!», el palo
  tiembla y cruje cada vez más agudo y seguido, el banderín parpadea); si el cazo se queda vacío 0,35 s, se desarma. Otra
  tortuga que sube por el mango y cae de un salto sobre el cubito (1 m por encima del mango) dispara al momento. Al
  disparar: el palo se parte y sale volando, el cubito cae, la cuchara da la vuelta en 0,16 s hasta -41° y rebota
  contra la arena (golpe, muelle, polvo, temblor de cámara, «¡ZAS!»). Las del cazo salen como **bolas de caparazón** a
  **2300 cm/s** y **44°** (±3°) hacia el mar con **±12°** de desvío y ±5 % de fuerza al azar (cada una la suya: no caen
  juntas); las del mango, al 55 %. La bola vuela ~30 m (más cuesta abajo o desde la torre), rebota, rueda y sale sola al
  pararse (o al caer al agua); no se puede salir en el aire. Ahorra camino, pero se cae donde toque: entre enemigos, en
  algas o en una concha.
- **Recarga**: **4,6 s** (`ReloadSeconds`): rebote hasta 0,6 s, quieta hasta 1,1 s, vuelve a golpes de carraca (14
  pasos, «clic» cada 0,2 s), el palo se pone de pie y al final el cazo se asienta con un botecito. La colisión del brazo
  se apaga 0,55 s al disparar (las bolas nacen dentro del cazo).
- **Red**: `ArmedAt` y `FiredAt` (horas del servidor) replicados; cada máquina anima el brazo, el palo, el banderín y los
  sonidos desde ellas.
- **Probar**: `TN.Beach.Place Catapult [Tamaño] 0 [Semilla]` mirando al mar. Meterse en el cazo (aviso y disparo),
  salir durante el aviso (se desarma), dos o tres en el cazo (salen abiertas), subir por el mango y saltar sobre el
  cubito con otra en el cazo, meterse durante la recarga (nada). Con un cliente lanzado: la bola vuela igual en las dos
  pantallas.

### Trampolín (`ATN_BeachTrampoline`)

- **Variantes** (por la semilla; con `SizeScale` 1, huella de 7 m): **medusa gorda varada** (campana de 10 m y 3,2 m de
  alto, rosa, lila o celeste, con trébol, motas, una cara que mira a la salida y ocho brazos orales tendidos en la arena);
  **colchoneta hinchable** (cinco tubos a rayas de 1,44 m y una almohada de 1,9 m, 9,8 x 6,9 m, con válvula); **flotador de
  donut** (12,3 m, 4,2 m de alto, glaseado rosa, de chocolate, celeste o menta con gotas y virutas; se puede caer en el
  agujero de 3,9 m); **sombrero de paja tenso** (ala de 13 m a 22-38 cm, que se pisa, y copa de 5,3 m y 2,2 m de alto
  con la tapa tensa, cinta y lazo). Como la medusa del lobby, rebota todo el cuerpo: cima, costados y borde, también de
  lado desde la arena (sin ir subiendo), con un «boing» sintetizado más grave en las grandes.
- **Rebote**: hacia arriba **1250 cm/s** (x1 medusa, x0,92 colchoneta, x1,08 donut, x0,96 sombrero; ~8 m de altura) más
  **0,55** por cada cm/s de caída por encima de 300, con tope de **2000** (~20 m): de trampolín en trampolín se sube cada
  vez más. Hacia delante se conserva el **75 %** de la velocidad horizontal y se suman **320 cm/s** hacia el mar (tope
  1100): 16 m andando, ~23 m esprintando. Sin meterse en el caparazón al caer. 0,3 s entre dos rebotes de la misma.
- **Deformación** (cada máquina con pantalla): se aplasta entera y rebota estirándose (10-26 %) y se hunde donde cae
  la tortuga (22-64 cm, en un radio de ~2,7 m) con una abolladura que vibra y se recupera en ~1 s (la malla procedural
  se actualiza solo mientras dura y si se ve). La medusa además respira.
- **Red**: como la medusa del lobby: el rebote lo aplican el servidor y el cliente dueño dentro del mismo movimiento
  (golpe con la colisión o solape con el sensor, 15 cm más grande); el resto lo ve por un multicast no fiable. Las bolas
  de caparazón rebotan también (las lanza el servidor, al 90 %).
- **Probar**: `TN.Beach.Place Trampoline [Tamaño] 0 [Semilla]` (semillas seguidas para ver las cuatro). Caer encima,
  andar contra un costado, encadenar dos trampolines (el segundo rebote más alto), saltar desde uno por encima de un
  alambre; lanzar una bola con la catapulta encima de uno. Con un cliente: sin correcciones al rebotar.

### Probar

- `TN.Beach.Place <Elemento|número> [Tamaño=1] [Extent=0] [Semilla]` en la ventana del anfitrión (desde un cliente del
  PIE se crea en el servidor del mismo proceso): lo pone apoyado en el suelo delante de tu tortuga y mirando hacia donde
  mira (su +X). Sirve para cualquier elemento de `ETNBeachElement`. `TN.Beach.Place clear` borra lo creado así.
  (`TN.Beach.Spawn` de la parte de enemigos hace lo mismo más simple, a 30 m.) Ejemplos:
  `TN.Beach.Place BarbedWire 1 2000`, `TN.Beach.Place Seaweed`, `TN.Beach.Place WobblyPlatform 1 0 2` (tabla) y
  `... 3` (tapa), `TN.Beach.Place SpadeRamp 1 0 2` (balancín) y `... 3` (puente), `TN.Beach.Place ShellGate 1 0 4`
  (pared) y `... 5` (rocas), `TN.Beach.Place BrokenBucket`, `TN.Beach.Place SandDungeon`.
- Con dos jugadores (anfitrión y cliente) y el log `LogTortunabo Verbose`:
  - **Alambre**: tocarlo andando y saltando encima desde los dos lados: bola aturdida 1,2 s lanzada hacia atrás, chispas
    y «¡AY!» en las dos pantallas; no vuelve a pinchar hasta 2,7 s después. Saltarlo si se llega.
  - **Algas**: entrar andando y cayendo; freno inmediato en el cliente sin tirones de corrección; hebras enrolladas en
    las dos pantallas; salir machacando el salto (~1,3 s), meneando o esperando 3,2 s; 2,5 s de gracia; aturdir a una
    enganchada (`TN.Race.Stun`) la suelta.
  - **Plataforma**: una tortuga la ladea y cruje sin romperla; dos a la vez la parten en ~1,1 s, caen al hoyo y salen por
    la brecha o por las mitades; el cliente ve la misma caída.
  - **Cubo**: entrar por la boca y salir por el agujero de pie, en los dos sentidos; la cámara dentro.
  - **Pala**: trampolín en la punta (sin corrección en el cliente); balancín: una en la hoja y otra saltando sobre el
    mango: la de la hoja sale volando, sin meterse en el caparazón al caer; la pala vuelve sola. Puente: subir por el
    mango y saltar desde la hoja.
  - **Puerta**: abrir empujando desde cada lado y con el interruptor; que no se cierre con alguien en medio; que se
    vuelva a abrir si alguien entra mientras se cierra.
  - **Castillo**: el recorrido entero con la puerta y las algas de dentro; saltar por una ventana; las piezas de dentro
    desaparecen con el castillo al cambiar de ronda.

## Enemigos y tormenta (`World/Beach/`)

| Archivo | Qué es |
|---|---|
| `TN_BeachEnemy.*` | `ATN_BeachEnemy` (abstracta, hija de `ATN_BeachElement`): base de los cinco enemigos (red, tortugas, suelo, voz, golpes, apartarse y nivel de detalle) |
| `TN_BeachGiantCrab.*` | `ATN_BeachGiantCrab`: cangrejo gigante con una pinza enorme |
| `TN_BeachSeaUrchin.*` | `ATN_BeachSeaUrchin`: erizo de mar que rueda hacia ti |
| `TN_BeachLizard.*` | `ATN_BeachLizard`: lagarto que toma el sol, asusta y se esconde |
| `TN_BeachQuadLane.*` | `ATN_BeachQuadLane`: paso de quads que cruza la playa |
| `TN_BeachGullZone.*` | `ATN_BeachGullZone`: gaviotas y pelícanos que cagan, bajan en picado y se llevan tortugas en el pico |
| `TN_BeachStorm.*` | `ATN_BeachStorm`: la tormenta de bañistas (no es un elemento; la crea el GameMode) |
| `TN_BeachSandWorm.*`, `TN_BeachSandWormMeshes.h`, `TN_BeachSandWormSynth.*` | `ATN_BeachSandWorm`: el gusano de arena gigante que se come a las rezagadas al acabar la cuenta atrás (no es un elemento; lo crea el GameMode), sus mallas y sus sonidos |
| `TN_BeachCameraShake.*` | `UTN_BeachCameraShake`: temblor de cámara (no había ninguno en el proyecto) |
| `TN_BeachEnemySynth.*` | `UTN_BeachEnemySynthComponent`: sonidos sintetizados (golpes, graznidos, motor, viento) |
| `TN_BeachEnemyKit.h`, `TN_BeachEnemyMeshes.h` | Mallas low-poly por piezas, caché de mallas, sombras, partículas propias, el pico de las aves y el enganche a la espalda de la tortuga |
| `TN_BeachEnemyDebug.cpp` | Consola de pruebas |

### Comunes

- **Escala**: todo a `TNBeach::Scale` (28). Se reutiliza la fauna low-poly (`TN_ProcMapFaunaMeshes.h`: gaviota, pelícano,
  cuadrúpedo del lagarto y sus ayudas de cuerpo y ojos), las partículas de `TNAmbientFX` (sin su registro global: cada
  actor lleva sus emisores), el velo de la tormenta del camino (`TNStormFX::BuildVeil`), la tos
  (`UTN_StormCoughComponent`) y el texto emergente de las trampas (`FTNTrapPopText`). Las mallas se construyen en
  ejecución y se comparten por paleta (referencia débil: el recolector las suelta cuando no queda ninguno). Colisión:
  solo el cuerpo del cangrejo (caja tipo Pawn).
- **Red** (servidor escucha): el servidor decide objetivos y golpes; los que andan replican `FTNBeachMoverRep` (suelo
  bajo el cuerpo cuantizado a 1 cm, giro en 16 bits, estado, hora del estado y un punto de interés) a 8-12 Hz y solo lo
  que cambia; los clientes interpolan con una pizca de extrapolación. El paso de quads, la zona de gaviotas y la
  tormenta replican solo horas y sentidos: cada máquina calcula la posición con el reloj del servidor. Los efectos van
  por multicast no fiable y el empujón del ragdoll, por multicast fiable. Siempre relevantes (con la distancia por
  defecto un cliente destruiría y rehará sus mallas cada vez que se aleja 150 m por la playa).
- **Carrera en marcha**: no atacan durante el recuento ni el podio (`RacePhase` = `RoundResults` o `Champion`); en
  `Waiting` sí (por si la fase no cambiara).
- **A quién se le da** (`ATN_BeachEnemy::CanBeHit`): viva, sin aturdir, sin derribar y sin ir en el pico de una gaviota
  (`IsTurtleHeld`). A quien ya está en el suelo no le da nadie (tampoco el empujón del lagarto ni la tormenta).
- **Golpes variados**: no todo es la bola. El derribo (`ATN_BeachEnemy::KnockDownTurtle`, o `ServerKnockDown` desde la
  tormenta) llama a `TNBeach::KnockDownTurtle` (ragdoll y mareo de la piel de plátano; como poco 2,2 s tumbada y 0,75 s
  levantándose) con solo 0,6 m/s hacia abajo: lo que se le pasa lo aplica la cápsula al levantarse, porque queda
  pendiente en el movimiento mientras dura el ragdoll. El empujón de verdad va a los cuerpos del ragdoll en cada máquina
  (`FTNBeachRagdollPushes`: se guarda hasta que el ragdoll simula, un segundo como mucho), así sale lanzado igual en
  todas.

| Quién | Qué te pasa | Después |
|---|---|---|
| Cangrejo (mazazo) | despachurrada en bola aturdida 3,5 s (`StunTurtle`), empujoncito de 3,2 m/s hacia fuera | te ignora 6 s |
| Erizo (pinchazo) | derribo con ragdoll y mareo 2,4 s, despedida a 6,2 m/s hacia fuera y 3,8 hacia arriba dando una vuelta hacia atrás (260°/s); «¡PINCHAZO!» | te ignora 4,5 s |
| Gaviota (cagada) | derribo con ragdoll y mareo 2,4 s, tumbada de espaldas (2,4 m/s hacia fuera y 1,2 hacia arriba) con la mancha en el caparazón; «¡PLOF!» | la zona te deja 6 s |
| Gaviota o pelícano (picado) | colgada del pico 3,3 s, pataleando; al soltarte, bola aturdida lo que tardas en caer (~2,3 s) + 2 s; «¡ÑAC!» | la zona te deja 12 s |
| Quad (rueda) | derribo con ragdoll y mareo 3 s, lanzada a 9,5 m/s en su sentido, 3,8 de lado y 7,5 hacia arriba dando vueltas de campana (420°/s); «¡ATROPELLO!» | 1,2 s sin repetir |
| Tormenta (revolcón) | derribo con ragdoll y mareo 2,2 s, empujada a 6,5 m/s hacia el mar, 3,5 hacia arriba y ±1,5 de lado dando una vuelta; «¡REVOLCÓN!» | como mucho uno cada 9 s |
| Lagarto (susto) | empujón de 6,5 m/s, sin derribar ni aturdir | — |

- **Muchos a la vez** (el reparto es ~3 veces más denso, con cangrejos en grupos): los que andan se apartan entre sí
  (`GetBodyRadius`: cangrejo 4,2 m, erizo 1,15 veces su radio de rodar, lagarto 3,8 m, todo por el tamaño; cada uno se
  aparta la mitad del solape en cada paso) y el cangrejo y el erizo rodean lo grande del reparto (trampas salvo las
  algas, y el decorado que cierra el paso con 3,8 m de huella o más; cápsulas al 85 % de su huella, las que quedan a
  menos de 1,6 huellas + 60 m de su sitio): se deslizan por el borde. El lagarto no (se mete debajo de las rocas). Sus
  paseos nunca eligen una meta dentro de un obstáculo y se rinden a los 7-12 s. El suelo de los que andan sale del
  generador (`ATN_BeachRaceGenerator::GetGroundHeightAt`, sin trazas y sin subirse encima del decorado); sin generador,
  traza.
- **Nivel de detalle** (cangrejos, erizos y lagartos, `bThrottleWhenFar`): cada 0,5 s miran la tortuga más cercana y la
  cámara. Con una tortuga a su alcance (cangrejo: correa + vista + 15 m, ~75 m; erizo: correa + 22 m + 15 m, ~53 m;
  lagarto: 45 m) o la cámara a menos de 150 m, se mueven en cada fotograma; si no, cada 66 ms a la vista (hasta 300 m) o
  cada 250 ms, y la red baja a 3 Hz (8-12 Hz de cerca). `TN.Beach.Enemy.Stats` cuenta cuántos hay, cuántos van despacio
  y cuántos se apartan.
- **Temblor de cámara**: `UTN_BeachCameraShake::Kick` (golpe que se apaga) y `Rumble` (sostenido mientras se refresque),
  con la fuerza entera hasta un radio y apagándose hasta otro, para los jugadores locales.
- **Textos emergentes** (`ATN_BeachEnemy::ShowPop`): solo con la cámara a menos de 60 m.
- **Rendimiento**: sin pantalla (servidor dedicado) no hay mallas ni efectos; lejos de la cámara (300 m; 400-600 m el
  quad y las gaviotas) no se anima. Nada asigna memoria por fotograma.

### Cangrejo gigante (`ATN_BeachGiantCrab`)

- Caparazón de 5 m de ancho (una cría de 18 cm) sobre ocho patas, ojos en pedúnculos y una pinza de ~6 m a la derecha.
  Cuatro paletas (rojo con pinza amarilla, violinista azul, violeta, fantasma de arena). `SizeScale` 0,8-1,2.
- **Patrulla sin parar** su propio recorrido, de lado a 3 m/s (por el tamaño), empezando en un punto al azar para que
  los de un grupo no vayan a la par: entre las dos rocas, grupos de rocas, troncos, maderas, tablones, castillos
  pequeños, sacos terreros o erizos antitanque más separados en ángulo que tenga a menos de 34 m (el 35 % de las veces,
  si los hay: se para junto a cada uno
  y pasa por su sitio), un óvalo de 23 m por 11-16 m (dos paradas por vuelta) o una ida y vuelta de 23 m por una
  recta que pasa por su sitio (se para en los extremos). Las paradas duran 0,5-0,95 s, con la pinza en alto
  chasqueando deprisa. Ningún punto dentro de lo grande del reparto; si no llega a uno en 12 s, pasa al siguiente.
- **Vista y oído**: ve de frente (±70° hacia donde mira) a 22 m y oye alrededor a 10 m (por el tamaño): 13 m si la
  tortuga corre a más de 6 m/s y 6 m si va agachada, en bola, en panzazo o casi quieta (menos de 0,6 m/s). Tiene que
  estar dentro de su correa (38 m o 1,4 veces la huella desde su sitio). Entonces se da la vuelta (chasquido) y la
  persigue de lado a 5,6 m/s (se escapa esprintando: 8 m/s). Si la pierde, vuelve a 4,2 m/s al punto más cercano de su
  recorrido y sigue patrullando. Decide con `TNCrabLogic::DecideChaseTransition`, la del cangrejo de siempre.
- **Mazazo**: con la tortuga a su alcance (~7 m) más 3 m, se para, levanta la pinza y tiembla 0,6 s; la sombra de dónde
  cae aparece donde estará la tortuga (0,3 s de adelanto) y crece hasta 2,8 m de radio. Cae en 0,14 s: quien esté
  dentro (2,8 m + 0,4) queda despachurrada en bola aturdida 3,5 s con un empujoncito hacia fuera. Luego 1,1 s con la
  pinza clavada, 1,4 s sin poder repetir y a la golpeada la ignora 6 s. Arena, temblor fuerte a menos de 15 m.
- Los objetos del jugador lo aturden (quieto con los ojos dando vueltas) o lo ciegan (vuelve a su recorrido)
  (`ITN_EnemyTargetInterface`).

### Erizo de mar (`ATN_BeachSeaUrchin`)

- Bola violeta, negra, roja u oliva de 72 púas: rueda sobre un radio de 1,26 m (3 m de diámetro). `SizeScale` 0,75-1,35.
- Nota las vibraciones alrededor a 22 m (por la raíz del tamaño) y rueda girando hacia la tortuga más cercana a
  2,1 m/s (lento: solo pilla a quien se despista), sin salirse de 16 m de su sitio (o 1,35 veces la huella). Sin nadie,
  pasea casi sin parar (respiros de 0,6-1,6 s) a 1,2 m/s por el 80 % de su huella, rodeando lo grande del reparto.
- Tocarlo (a ~1,65 m del centro) pincha en cualquier estado: derribo con ragdoll y mareo (tabla de arriba); el erizo
  retrocede 0,8 s y la ignora 4,5 s.

### Lagarto (`ATN_BeachLizard`)

- Vida del ambiente: lagarto de 15 m (55 cm reales) sobre el cuadrúpedo de la fauna: iguana verde con cresta,
  turquesa de cabeza amarilla, ocelado con manchas azules o naranja de collar. Toma el sol 4-8 s (flexiones cada ~7 s,
  cabeceos, lengua y cola que se mece) y se va andando a 3,8 m/s a otro rincón de su zona (hasta el 60 % de su huella,
  nunca dentro de una roca); así siempre se mueve.
- A 30 m se pone alerta y mira a la tortuga (también andando). A 17 m: el 70 % de las veces da un susto (amago de
  0,35 s hacia ella, se hincha, tiembla, saca la lengua y bufa; a quien esté a menos de 8 m de la cabeza lo empuja
  6,5 m/s, sin aturdir) y luego huye; si no, huye sin más. A 9 m huye directamente.
- Huye a 15 m/s a la roca, grupo de rocas, tronco, madera, tablones, restos de vela, castillo pequeño, sacos terreros,
  caja de munición o red de camuflaje más cercano (hasta 45 m, nunca hacia la tortuga) y se mete debajo (anda sobre la arena del generador: no se sube encima); si no hay, da
  un arreón y se entierra sacudiéndose (1,3 s). Escondido 7-12 s; no sale con una tortuga a menos de 18 m. Sale y
  vuelve andando a su sitio.

### Paso de quads (`ATN_BeachQuadLane`)

- Eje X local del actor (el generador lo gira 90° y lo cruza de lado a lado), `Extent` de largo (0 = 280 m). En la arena,
  dos rodadas que avisan por dónde pasa (se trazan contra el suelo; si aún no está, se reintenta).
- Quad a escala con piloto: 56 m de largo, ruedas de 16,8 m de alto y 6,7 m de ancho, centros a ±8,7 m (las ruedas
  llegan a ±12 m: la huella), hueco de 10,6 m entre ruedas y 7,3 m de altura libre bajo el chasis. `SizeScale` 0,7-1,4
  escala todo.
- Primera pasada a los 5-14 s; después, cada 12-20 s. Aviso de 3,5 s: temblor creciente (0,12 → 0,57) a menos de 15 m
  del paso (se nota hasta 90 m), motor que se acerca y humo y hojas entre las palmeras del lado de salida. Cruza a
  42 m/s (unos 9 s de palmera a palmera, sale de 15 m dentro de la selva), revienta las palmeras al salir y al entrar.
- Atropello: una rueda que pasa por encima (±3,8 m a lo ancho, ±5 m a lo largo) derriba con ragdoll y lanza (tabla de
  arriba; lanzamiento moderado para que el ragdoll no atraviese la arena al caer); 1,2 s sin repetir con la misma.
  Temblor 0,85 a menos de 25 m del quad.

### Gaviotas y pelícanos (`ATN_BeachGullZone`)

- 3-4 gaviotas (25 m de envergadura) y, el 60 % de las veces, un pelícano (40 m), **cada una en su círculo y a su
  altura**: óvalo con el centro desplazado del de la zona (12-60 % del radio de la zona, 35 m o más, repartidos con el
  ángulo áureo y derivando ±7 m), radio del 45-90 % (18 m como poco), achatado 0,65-1 y girado al azar, a 9-13 m/s (el
  pelícano, 7-9); el 30 % gira al revés. Alturas en capas de 8 m barajadas (32, 40, 48, 56 y 64 m, ±2 m): nunca dos a
  la misma. Sombras en la arena (más grandes y cercanas cuanto más bajan); graznan de vez en cuando abriendo el pico.
- Ataca cada 3-6 s a una tortuga al azar de las que están a menos de su huella + 8 m del centro (atacable y sin
  sombrilla); va el pájaro más cercano. La mitad de las veces caga una gaviota; si no, picado (el pelícano solo pica).
- **Cagada**: 1,5 s volando hasta encima; la suelta desde 30 m y cae acelerando en 1,35 s: un pegote de 1,6 m con su
  estela de gotitas, un silbido y su sombra que se encoge de 5,2 m a 2,8 m. Quien esté dentro (2,8 m por el tamaño +
  0,45, y a menos de 3 m en altura: a cubierto la mancha cae encima) cae derribada (tabla de arriba) con la mancha en el
  caparazón (pegada al hueso de la espalda, va con el ragdoll; 6,4 s); gotas, «¡PLOF!» y la mancha en la arena 12 s.
- **Picado**: 1 s colocándose a 48 m y 42 m de altura; se lanza acelerando con las alas recogidas y su sombra crece y se
  acerca. Fija el blanco a los 1,9 s (0,25 s de adelanto). A 0,45 s de llegar abre el pico, abre las alas y adelanta las
  patas para frenar con el morro levantado. A los 2,45 s coge a la tortuga que esté bajo el pico (3 m por el tamaño +
  0,45) si está de pie: ni en pleno panzazo (`IsBellyPoseActive`), ni en bola, ni en brazos de otra, ni a cubierto.
- **Agarre**: el pico se cierra (con chasquido, plumas, arena y «¡ÑAC!») en la espalda de su caparazón (el punto al 72 %
  del pico; en el pelícano, al 62 %, dentro de la bolsa). La tortuga queda colgando pataleando (pose de pataleta de
  `UTN_TurtleAnimInstance`, sin meterse en bola): 0,35 s de tirón, sube 26 m aleteando fuerte hasta los 2,2 s, vuela
  meciéndola y sacudiendo la cabeza y a los 3,3 s la suelta abriendo el pico, 15 m más hacia la salida: cae en bola
  aturdida (empujada 3,5 m/s hacia la salida). Si se mete en el caparazón mientras cuelga, se escurre y cae en bola. Si
  falla, remonta graznando.
- **Red del agarre**: cada máquina coloca a la tortuga con el mismo camino (`FTNBeachGullAttack::Hold` y el reloj del
  servidor), con su movimiento apagado (`MOVE_None`); con malla, por su hueso de la espalda (`Spine2`, 35 cm por delante
  del pico), y en un servidor dedicado, por la cápsula. El pájaro se coloca para que su pico quede justo ahí (su
  cuerpo, cabeza y pico: `TNBeachMeshes::BirdGeom`). Mientras cuelga, el servidor no corrige al dueño
  (`bIgnoreClientMovementErrorChecksAndCorrection`) y los demás clientes la ven sin suavizado de red. Al soltarla vuelve
  a caer (`MOVE_Falling`; con la ronda parada, la deja congelada el GameMode) y el servidor la mete en bola.

### Tormenta de bañistas (`ATN_BeachStorm`)

- **Interfaz**: el GameMode la crea 30 m detrás de la salida, en el centro de la playa y mirando al mar, y llama por
  nombre a `StartStorm()` y `StopStorm()` (UFUNCTION sin parámetros). Desde C++: `StartStormAt(Desplazamiento,
  Velocidad, Gracia)`, `GetFrontDistance()` (distancia del frente al actor), `GetFrontSpeed()`, `GetFrontLocation()`,
  `IsLocationInside(Punto)`, `IsStormActive()` y `ATN_BeachStorm::FindStorm(Contexto)`. Parada, se queda quieta y a la
  vista (recuento); al destruirla desaparece.
- **A ras de arena** (antes salía «arriba del todo»): el suelo del frente se buscaba con trazas contra lo estático desde
  60 m por encima del último suelo encontrado; el muro invisible de detrás de la salida (de -200 a +1200 m de alto)
  devolvía el punto de partida y la cota subía 60 m en cada traza, así que velo, trastos y bañistas acababan a cientos
  de metros. Ahora la arena sale de `ATN_BeachRaceGenerator::GetGroundHeightAt` (sin trazas) para el frente, cada trasto
  y cada bañista; sin generador (otro mapa), traza por el canal de visibilidad, que los muros invisibles no bloquean.
- **Marcha justa**: 15 s de gracia (`DefaultGrace`), de 0 a 3 m/s en 7,5 s (`StartAccel` 40 cm/s²) y 3 m/s
  (`DefaultSpeed`: la tortuga anda a 4,5 y la media de la carrera es ~4 m/s, así que quien avanza con normalidad le
  saca ~1 m/s). Solo acelera (a 25 cm/s², `SpeedChangeAccel`) al final: pasados 4 min de marcha (`LateStartSeconds`),
  +0,5 m/s por minuto hasta 5,2 m/s (`SpeedRampPerMinute`, `MaxSpeed`); con la primera tortuga pasado el 80 % del
  recorrido, al menos 4,2 m/s (`EndRushProgress`, `EndRushSpeed`); y si la última le saca más de 180 m, a 4,7 m/s hasta
  quedarse a 120 m (`CatchUpGap`, `CatchUpSpeed`, `CatchUpRelease`), para que siempre se note. Cada cambio empieza un
  tramo nuevo replicado desde donde está (desplazamiento, velocidad, aceleración con signo, velocidad a la que va, hora).
- **Aviso**: con el frente a menos de 25 m por detrás (`WarnDistance`), temblor creciente, viento, arena alrededor de la
  cámara y «¡QUE VIENE LA TORMENTA!» (una vez por acercamiento); al entrar, «¡CORRE!».
- **Revolcón**: dentro (6 m por detrás del frente, `InsideMargin`) durante 1,2 s (`FirstHitDelay`), derribo con
  ragdoll y mareo de 2,2 s (`KnockSeconds`) empujada hacia el mar (`PushForward` 6,5 m/s, `PushUp` 3,5, `PushSide` 1,5)
  dando una vuelta hacia delante; como mucho uno cada 9 s por tortuga (`HitInterval`). Dentro también: niebla y tinte
  de arena, viñeta, tos y viento.
- **Por fuera**: velo de arena de 55 m apoyado en la arena del frente (a la altura de la cámara a lo ancho); 24 trastos
  que nacen en el polvo y salen volando hasta 35 m por delante del frente, a su velocidad + 3-9 m/s: los pequeños
  (cubos, palas, chanclas, pelotas) a 1,5-8 m, rebotando y rodando por la arena; los grandes (sombrillas de 50 m,
  sillas, toallas, flotadores) planeando a 6-26 m, sostenidos por el viento. Ocho bañistas con los pies en la arena
  pisando dentro del polvo (pisotón, arena y temblor del más cercano).

### Gusano de arena (`ATN_BeachSandWorm`)

Lo que pidió el usuario: si en los 10 s que se dan tras la primera no llegan las demás, que salga de debajo de la arena,
como una animación, un gusano de arena gigantesco que se las coma. Es un remate cómico: nadie muere; la tortuga se queda
dentro, oculta, hasta la ronda siguiente, que vuelve a crear a todas en la salida.

- **Contrato** (`Public/World/Beach/TN_BeachSandWorm.h`): `ATN_BeachSandWorm::EatTurtle(Tortuga)` (servidor; nullptr sin
  autoridad, con la tortuga nula o ya comida), `IsBeingEaten(Tortuga)` (en cualquier máquina, desde que sale su gusano
  hasta que la tortuga reaparece) y `EatSeconds` = 3,2 s. El GameMode llama a `EatTurtle` con cada rezagada al acabar la
  cuenta (`FeedSandWorms`, ver «Cuenta atrás tras la primera y medias conchas») y espera `EatSeconds` antes del recuento.
  No hace falta pasarla a espectadora: si otra cosa se lleva su cámara (el fantasma, el podio), el gusano no se la pelea.
- **Escena** (segundos de escena, iguales en todas las máquinas con el reloj del servidor):

| Tiempo | Qué pasa |
|---|---|
| 0-0,7 | Aviso: bajo la tortuga la arena se hunde en un remolino de espiral que gira cada vez más deprisa (hasta 4,3 m de radio), con polvo, piedrecitas que saltan y retumbar grave; temblor de cámara entero a 15 m y hasta 60 m. La tortuga tiembla y se hunde 35 cm en la arena. |
| 0,7 | Revienta la arena: nube, terrones y piedras hacia arriba, arena y rugido, y un golpe de temblor fuerte (hasta 90 m). |
| 0,7-1,3 | Sale en vertical con los cuatro labios abriéndose como una flor (dientes a la vista) y la tortuga dentro, pataleando (pose de pataleta) y dando vueltas; sube 25 m (se pasa un poco y vuelve) soltando arena del cuerpo. |
| 1,2-1,55 | Bocado: la tortuga sube un poco dentro de la boca, los labios se cierran en cúpula (1,43) y desaparece (1,55): «¡ÑAM!» grande sobre la boca, bocado y golpe de temblor. |
| 1,55-2,25 | Traga: mastica, se dobla en arco (72°) y un bulto baja por el cuerpo hasta la arena, con dos «glup». |
| 2,35 | Eructa: entreabre la boca y suelta una nube de arena hacia donde mira. |
| 2,6-3,0 | Se hunde acelerando por su agujero (terrones, polvo, arena y retumbar). |
| 3,0-3,2 | El cráter (5,2 m de radio, con reborde y agujero oscuro) se cierra. |

- **Varias rezagadas**: cada una tiene su gusano; cada uno sale 0,11 s después del anterior de la misma tanda (como
  mucho 0,3 s) y su escena va algo más deprisa para acabar igual, a `EatSeconds` de la llamada. No chocan con nada.
- **Mallas** (`TN_BeachSandWormMeshes.h`, caras planas con color de vértice, en caché por paleta, `RF_Transient |
  RF_DuplicateTransient`): boca de 6 m de diámetro (la tortuga mide 1,4 m) con collar, encía, garganta oscura y tres
  coronas de dientes (16, 12 y 9) que apuntan hacia dentro; cuatro labios-pétalo con la piel fuera, la carne rosa dentro
  y dientes en los bordes; 13 anillos del cuerpo (5,4 m de grosor, más finos hacia abajo) con surcos oscuros y
  verrugas; cráter y remolino, a ras de arena y apoyados en su cuesta (el terreno no se agujerea: el hoyo es el centro
  oscuro, y lo que se hunde de verdad, dentro de la arena, son la tortuga y el gusano). Tres paletas: arena tostada, rosa
  de lombriz y gris de duna. Sin esqueleto: cada fotograma los anillos se colocan a lo largo de una columna (recta
  desde la arena y en arco arriba, con una ondulación de lado), los labios giran sobre su bisagra y cabeza y anillos se
  hinchan (bocado, eructo, bulto del trago).
- **La tortuga comida**: antes de empezar, el servidor le quita aturdimiento, bola, derribo y carga (y lo vuelve a quitar
  si algo se lo pone mientras está en la boca), le para el movimiento y le quita la colisión; queda marcada como sujeta
  (`ATN_BeachEnemy::SetTurtleHeld`: ningún enemigo le da). Cada máquina la coloca con las mismas cuentas (de pie en el
  remolino, luego dentro de la boca) sin suavizado de red y sin correcciones al dueño; en el bocado se oculta
  (`SetActorHiddenInGame`, replicado) y se queda de pie, oculta, donde estaba. Su jugador pierde el control (sin input en
  su tortuga) y su cámara funde en 0,6 s a un lado del gusano (15 m durante el aviso y 32 m después, nunca dentro de
  una roca ni de una duna), mirando a media altura para que quepa entero con la boca.
- **Hasta cuándo**: el gusano sigue vivo, sin nada que ver, hasta que la tortuga reaparece (alguien la vuelve a mostrar)
  o deja de existir (la ronda nueva la quita); entonces el servidor lo destruye y cada máquina devuelve lo que tocó
  (input, cámara si aún miraba al gusano, colisión y la marca de sujeta).
- **Red**: un actor replicado y siempre relevante con la tortuga, la hora de inicio, el desfase, el suelo, el sentido del
  arco y la semilla, todo fijado al crearlo; ningún RPC. Cada máquina anima el gusano y los efectos con esas cuentas.
- **Sonido** (`UTN_BeachSandWormSynthComponent`, sintetizado como el de los enemigos): retumbar continuo (aviso y
  hundirse), rugido grave, arena que revienta, bocado («¡ÑAM!»: dos mordiscos húmedos con chasquido de dientes), trago y
  eructo.
- **Consola**: `TN.Beach.Worm [jugador]` (en el anfitrión; `jugador` es el índice en `PlayerArray`, 0 por defecto): se
  come ya a esa tortuga, en cualquier fase. En plena carrera se queda comida hasta la ronda siguiente.

### Probar

- `TN.Beach.Spawn GiantCrab` (y `SeaUrchin`, `Lizard`, `QuadLane`, `GullZone`), del generador, en cualquier mapa.
- **Gusano** (PIE de 2): `TN.Beach.Worm 1` en el anfitrión con la otra tortuga en la arena → en las dos ventanas,
  remolino, salida con la tortuga pataleando en la boca, «¡ÑAM!», bulto, eructo y cráter que se cierra, a la vez; la
  comida no se mueve ni usa nada y su cámara mira desde un lado (la otra ventana deja de verla en el bocado). Después, la
  cuenta de verdad: llegar con una y dejar a la otra en la arena 10 s; que el recuento espere al gusano y que la ronda
  siguiente la traiga con control y cámara normales. Probar también con la rezagada en bola, derribada, en brazos de
  otra o saltando al acabar la cuenta, y con 3 o más (que no salgan todos a la vez).
- **Cangrejo**: mirar que nunca se queda quieto más de un segundo (recorrido y chasquidos); acercarse por detrás andando
  (oye a 10 m: se da la vuelta), corriendo (13 m) y en panzazo o agachada (6 m); de frente, a 22 m. Esprintar (se
  escapa); quedarse quieta en la sombra de la pinza (bola aturdida); salir de la sombra durante el aviso (falla);
  alejarse más de 38 m (vuelve a su recorrido). Con un grupo de 2-3: que no se monten unos encima de otros y que rodeen
  las rocas en vez de atravesarlas.
- **Erizo**: quedarse quieta delante (rueda hasta pinchar: ragdoll lanzado y mareo), tocarlo andando por detrás.
- **Lagarto**: que pasee entre ratos al sol; acercarse despacio (alerta a 30 m), luego a 17 m (susto o huida), hasta
  que se meta bajo una roca (debajo, no encima) o se entierre; esperar lejos a que salga.
- **Quad**: `TN.Beach.Quad.Now`; notar el temblor y el humo del lado de salida; ponerse en una rodada (ragdoll lanzado
  dando vueltas), fuera del paso y en medio de las dos rodadas (sobrevive). Probar los dos sentidos.
- **Gaviotas**: mirar que cada una va por su círculo y a su altura. `TN.Beach.Gull.Attack 1` (cagada: se ve caer con su
  estela y su sombra; apartarse; quedarse: ragdoll con la mancha en el caparazón) y `TN.Beach.Gull.Attack 2` (picado:
  panzazo en el último momento para esquivar; quedarse: abre el pico, te coge por el caparazón, cuelgas pataleando,
  sube aleteando, vuela y te suelta en bola). Meterse en el caparazón colgando (se escurre).
- **Tormenta**: en la carrera arranca sola; `TN.Beach.Storm.Info` dice dónde va y a qué velocidad. En otro mapa,
  `TN.Beach.Storm.Start [metros por detrás] [cm/s]` y `TN.Beach.Storm.Stop`. Que el velo, los trastos y los bañistas
  estén sobre la arena; andar sin prisa (no te alcanza); dejarse alcanzar (aviso, «¡CORRE!», un revolcón y ninguno más
  en 9 s) y salir corriendo.
- En PIE con 2-3 jugadores: que los clientes vean lo mismo (posiciones suaves, golpes a la vez, el ragdoll lanzado igual
  en todas las pantallas, la tortuga en el pico sin tirones) y que no haya correcciones raras al empujar el lagarto, al
  atropellar o al colgar del pico (sobre todo en la pantalla de la que cuelga).
- Consola: `TN.Beach.Enemy.Debug 1` (radios de vista y de oído, correa, recorridos y golpe del cangrejo, cajas de las
  ruedas en el servidor) y `TN.Beach.Enemy.Stats`.

## Botín en la playa (`TN_BeachLoot`)

Como en el cooperativo, en la playa se rebusca en el decorado, hay objetos y power-ups por el suelo (para ti o para
fastidiar a las demás) y conchas de puntos; todo lo que se coge lleva la marca común (anillo dorado que gira en el
suelo, columna de luz tenue, chispitas que suben y luz suave cerca: `Docs/Botin_Decorados.md`, «Brillo de lo que se
coge»). A petición del usuario, a rebosar: casi todo el decorado se rebusca y hay objetos por toda la playa.

| Archivo | Qué es |
|---|---|
| `Public/World/Beach/TN_BeachLoot.h`, `Private/World/Beach/TN_BeachLoot.cpp` | `TNBeachLoot` (reglas, pesos y `SpawnRoundLoot`), `ATN_BeachSearchSpot` (el rebuscable con las reglas de la carrera) y `UTN_BeachLootSubsystem` (reparte y quita el botín de cada ronda) |
| `Private/World/Beach/TN_BeachLootShells.cpp` | Las conchas de puntos de cada ronda (`UTN_BeachLootSubsystem::SpawnRoundShells`) |

### Cuándo se reparte

- En el servidor, en cada ronda y con su semilla (misma semilla, mismo reparto; `TN.Beach.Loot.Reroll` cambia la
  tirada). Al cambiar de ronda, o al quitarla, se va todo lo de la anterior: los rebuscables se llevan lo que salió de
  ellos y nadie cogió, y se quitan las conchas y los objetos sueltos que queden. Lo que ya está en un inventario se queda.
- `UTN_BeachLootSubsystem` (subsistema de mundo con tick; solo hace algo en el servidor y donde hay un
  `ATN_BeachRaceGenerator`) mira el generador sin tocarlo y reparte en cuanto ve una ronda nueva lista (el mismo
  fotograma o el siguiente). Para que salga exactamente con los elementos, el generador puede llamarlo él: en
  `ATN_BeachRaceGenerator::GenerateRound` (`TN_BeachRaceGenerator.cpp`), justo después de
  `const FString Missing = SpawnRoundElements();`, la línea `TNBeachLoot::SpawnRoundLoot(*this);` (con
  `#include "World/Beach/TN_BeachLoot.h"`). Con o sin ella, nunca se reparte dos veces la misma ronda.

### Rebuscables (`ATN_BeachSearchSpot`)

- El rebuscable del mapa procedural (mantener E 1,3 s, aro, «¡puf!» u «¡pof!», saltito del objeto, una vez para todas:
  la primera que llega) con las reglas de la carrera: **70 % de suerte** (55 % en el cooperativo: pararse en una carrera
  cuesta), los pesos de la carrera (abajo), polvo de arena y las pistas a la escala de la playa: chispitas desde 35 m y
  el anillo dorado de dónde rebuscar desde 18 m (105 cm de radio).
- **Huella real**: la caja de la malla del decorado (su cuerpo), girada, inclinada y escalada como ese ejemplar; cápsula
  a lo largo del lado largo (tablones, troncos, toallas, botellas...) y redonda en lo demás. La sombrilla se rebusca en el
  montón de arena de su pie (la lona está a 35 m).
- **Qué decorado** (`TNBeachLoot::SearchChance`, probabilidad por ronda):

| Probabilidad | Decorado |
|---|---|
| 100 % | castillos (pequeño y enorme), restos de vela, silla, sombrilla, red de pesca, grupo de rocas, tablones, tronco con musgo, cubito de juguete, toalla, sacos terreros, caja de munición, red de camuflaje |
| 85 % | roca, madera a la deriva, botella, vaso, boya, coco, corteza de sandía, sujetador, chanclas, pelota, disco volador, crema solar, bidón, erizo antitanque, soldaditos, casco, almeja |
| 60 % | lata, brick, gafas de sol, patito, chupachups, concha de adorno, estrella de mar, anillas de latas, trozo de cuerda, hueso de sepia |
| nunca | chapas, cáscaras, palitos de helado, pluma, pajita (diminutos o finos), medusa (pica), pasarela y caminito de palos (son camino) |

- **Uno por corrillo**: 9 m como poco entre centros y 3 m entre bordes; hasta 90 por ronda y 20 por sexto del recorrido.
  Con ~150 piezas de decorado por ronda salen del orden de 50-80 (el registro da el número exacto).

### Objetos sueltos

- **30-38 sueltos**, uno por tramo igual del recorrido desde los 90 m (nada en la salida) hasta 30 m del filo, más hacia
  el centro que hacia la selva.
- **4 filas de lado a lado de la playa** (hacia el 12, 37, 63 y 88 % del recorrido, ±6 %; un objeto cada 32 m, de selva a
  selva), como las cajas de objetos de las carreras de karts: todas pasan por una. Si su sitio está ocupado, un poco más
  adelante o atrás.
- En arena libre: a 3 m de lo que ocupa cada elemento del reparto (también de los pasos de quads), fuera del agua de las
  pozas, a 2,5 m del borde de los rebuscables y a 6 m de otro objeto; apoyados en el suelo de la ronda
  (`GetGroundHeightAt`). En total, unos 55-65 por ronda.
- Del catálogo con los pesos de la carrera, con su pickup de siempre (`PickupActorClass` + `InitializeFromInventoryItem`).

### Pesos de la carrera (`TNBeachLoot::RaceWeight`, por el uso del objeto)

| Uso (`ETN_ItemUseType`) | Fila de `DT_Items` | Peso | Por qué |
|---|---|---|---|
| `SelfStaminaBoost` | StaminaBoost | 1,5 | energía sin fin unos segundos: correr y dejar atrás la tormenta |
| `SelfStaminaFull` | (ninguna todavía) | 1,2 | la barra llena de golpe |
| `Throwable` | ThrowableBall | 1,3 | derriba a la que alcanza |
| `InkThrower` | Tinta | 1,3 | ciega a la que alcanza |
| `Conch` | Conch | 1 | concha trampa para las de detrás |
| `BigHead` | BigHead | 0,3 | en la playa no protege de nada (las gaviotas de la carrera no la miran): solo la cabezota y el mareo al acabar |
| `Totem` | Totem | 0 (fuera) | en la carrera no se muere: no revive a nadie |

Las filas sin `PickupActorClass` o sin uso (`None`) no salen nunca, como en el cooperativo; un uso nuevo sale con peso 1.

### Conchas de puntos (`ATN_ScorePickup`, las del cooperativo)

Las mismas conchas (`TNScoreShells`: 1, 25, 50 y 100), que suman `RaceScore` a quien las coge, como en el cooperativo.
Muchos sitios salen de los puntos interesantes del reparto (`TNBeachLayout::FRoundLayout::Interest`: arcos de salto,
cimas, atajos, rincones, trincheras y caminos alternativos). Se planean en este orden (lo difícil primero, para que cada
sitio tenga la suya), con topes por ronda de 300 de 1, 40 de 25, 12 de 50 y 2 de 100, y 1 m entre conchitas y 3 m
alrededor de las demás:

- **Reinas de 100**: en la sala de arriba del castillo con salas, entre sus dos muretes (tras la puerta de conchas, las
  algas del pasillo y la escalera), y en lo alto del castillo enorme (si ya hay dos reinas, grande de 50).
- **Grandes de 50**: en los rincones escondidos (hasta 3), en las dos primeras trincheras, tras el alambre de espino
  (hasta 3, a 2,6 m hacia el mar), tras las dos primeras minas, en lo alto de dos castillos pequeños y encima de las
  plataformas móviles (hasta 2, sobre lo más alto del elemento al crearse: se coge montada en ella). Si se acaban, las
  de los rincones y las trincheras pasan a normales.
- **Normales de 25 junto a los peligros**: en medio de las algas (5), dentro de la concha que atrapa (4), en el fondo del
  hoyo de la plataforma que se rompe (3), dentro del cubo roto (3), dos por paso de quads en sus rodadas, en casa de
  cangrejos y erizos (5), bajo las gaviotas (3), tras los sacos terreros (4), tras las demás minas (60 %) y en las
  demás trincheras (hasta 5 en total); además, en la sala de las columnas del castillo, en lo alto de tres castillos
  pequeños más y en las cimas de las crestas de arena (6).
- **Conchitas de 1**: arcos de 7 que dibujan el vuelo de las palas (desde el mango en alto o la punta de la hoja, con su
  `TipUp`/`TipForward`) y de los trampolines y las catapultas (su arco del reparto, de lo alto del lanzador a donde cae;
  los que no lo tengan, con su impulso: `LaunchSpeed` y `LaunchPitch` de la catapulta, o `LaunchUp`/`LaunchForward` en
  cm/s si la clase los tiene, o 1600/650 y 1300/1500); rachas
  por los atajos (el hueco estrecho de las filas y, flotando sobre el agua, el de nadar las pozas que cortan un
  corredor), a la entrada de los caminos alternativos (7 hacia el mar), por encima de las pasarelas y por los caminitos
  de palos (hasta 14 por tramo y 110 en total), por el hueco con algas que rodea el castillo con salas (11) y
  serpenteando por los lados de la playa (6-9, la mitad de los tramos de 65 m).
- Nada a menos de 50 m de la salida. Lo del suelo va en arena libre, fuera del agua de las pozas (a 1-1,2 m de lo que
  ocupa cada elemento, salvo lo que va a propósito dentro de un peligro); lo que va encima o dentro de algo busca su suelo
  con trazas contra los elementos.
- La planta de las salas del castillo repite la de `TNBeachDungeonDetail::MakeLayout` (`TN_BeachSandDungeon.cpp`): si
  cambia allí, hay que cambiarla en `DungeonRooms` (`TN_BeachLootShells.cpp`).

### Consola, registro y pruebas

- `TN.Beach.Loot 0` (sin botín ni conchas desde la ronda siguiente; `1`, lo normal) y `TN.Beach.Loot.Reroll` (quita el
  botín de la ronda y lo reparte otra vez, en el anfitrión o desde un cliente del PIE). Los de siempre de los
  rebuscables: `tn.Search.Luck 1` (siempre sale algo), `tn.Search.Seconds`, `tn.Search.Show 1` (baliza y huella de
  cada rebuscable) y `TN.Debug.Interaction 1`.
- Registro del servidor en cada ronda: `[Playa] botín de la ronda N: X decorados para rebuscar (de Y candidatos) y Z
  objetos sueltos` y `[Playa] conchas de la ronda N: ... de 1, ... de 25, ... de 50 y ... de 100 (dónde)`.
- Probar (`open LVL_BeachRace?BeachSeed=42`, con 1 y con 2 jugadores):
  1. Desde la salida se ven las columnas doradas de los objetos sueltos y, más cerca, sus anillos; las filas de lado a
     lado. Cogerlos (el anillo se apaga con ellos) y usarlos: bola y tinta contra la otra tortuga, concha trampa detrás.
  2. Rebuscar en una silla, un castillo, unos tablones (por el lado largo y por la punta) y una sombrilla (en su pie):
     el anillo de dónde rebuscar sale a 18 m del borde y sigue a la tortuga; con dos, a la otra no le sale mientras una
     rebusca y el objeto cae en el mismo sitio para las dos.
  3. Conchas: los arcos de una pala (saltar desde el mango) y de un trampolín, la reina de la sala de arriba del castillo
     con salas, la de lo alto del castillo enorme, las de tras el alambre y las rodadas de un paso de quads. Que sumen al
     contador y al recuento.
  4. Siguiente ronda (`TN.Race.WinRound`): desaparece lo que nadie cogió y sale el botín nuevo. `TN.Beach.Loot.Reroll`
     lo vuelve a repartir sin cambiar de ronda.
- Límites: lo que está en el inventario pasa a la ronda siguiente; si `RaceScore` se reinicia o no entre rondas lo decide
  el GameMode. Los arcos de trampolines y catapultas son una estimación mientras sus clases no den `LaunchUp` y
  `LaunchForward`; la concha de las plataformas móviles va sobre lo más alto del elemento al crearse.

## Recuento, campeón y podio (`UI/Race/`)

| Archivo | Qué es |
|---|---|
| `UI/Race/TN_RaceScreens.*` | `UTN_RaceScreensSubsystem`: subsistema de mundo en cada máquina con jugador; mira el estado replicado y pone o quita las pantallas (sin RPC ni cambios en el PlayerController); consola de vista previa |
| `UI/Race/TN_RaceFinishCountdownWidget.*` | `UTN_RaceFinishCountdownWidget`: cuenta atrás de 10 s tras la primera en el agua y «¡TIEMPO!» |
| `UI/Race/TN_RaceTallyWidget.*` | `UTN_RaceTallyWidget`: recuento de conchas tras cada ronda (con medias conchas) |
| `UI/Race/TN_RaceSprintWidget.*` | `UTN_RaceSprintWidget`: título «¡SPRINT FINAL!» con las finalistas |
| `UI/Race/TN_RaceCueSynthComponent.*` | `UTN_RaceCueSynthComponent`: «¡toc!» de la cuenta, silbato, fanfarria y «¡pum!», sintetizados en 2D |
| `UI/Race/TN_RaceChampionWidget.*` | `UTN_RaceChampionWidget`: pantalla del campeón (botones a la izquierda, podio a la derecha) |
| `UI/Race/TN_RacePodiumStage.*` | `ATN_RacePodiumStage`: el podio en 3D, capturado a una textura |
| `Private/UI/Race/TN_RaceArt.h`, `TN_RaceUIKit.h` | arte en código (fondos, huecos, corona, cielo, iconos, caras con el color de piel) y piezas de UMG |
| `Player/TN_TurtleAnimInstance.*` | poses Trofeo, Decepcionada y Pataleta (`SetCelebration`) y la zambullida del acantilado |

Todo en el estilo del HUD Tortunavy (`TN_HUDArt.h`, `TN_HUDFaces.h`, `TN_HUDStyle.h`) y por encima de él (cuenta atrás
15, recuento 20, campeón 21 y sprint 22: por encima del HUD, 4-10, y por debajo de las ruedas, 30). Sonidos: el «pom» y el «¡plin!» sintetizados de las
conchas de puntos (`UTN_ScoreShellSynthComponent`, en 2D en el PlayerController).

### Cuenta atrás (`Racing` con `FinishCountdown`)

- Sin tapar la carrera (no coge ratón ni teclado): arriba, la cinta «¡La primera ya está en el agua!» con su cara y la
  etiqueta «Concha para X · media concha para quien llegue antes del final»; en medio, el número (10… 1) en un medallón
  azul marino que late con cada número, se pone dorado a los 3 s y coral en el último y tiembla al final; debajo, lo que
  te toca: «¡Corre! Media concha si llegas», «¡Concha entera para ti!» o «¡Media concha para ti!» (si solo miras, nada).
- Sonido (`UTN_RaceCueSynthComponent`, en 2D): «¡toc!» de caja china cada segundo, cada medio desde los 5 s y cada cuarto
  desde los 2,5 s (más agudo en los 3 últimos). Al acabar, «¡TIEMPO!» (o «¡TODAS AL AGUA!») con el silbato del árbitro.
  Se va con un fundido cuando sale el recuento.

### Recuento (`RoundResults`)

- A pantalla completa: el mar azul marino con rayos de luz y burbujas que suben y, abajo, la orilla de arena. Arriba,
  la cinta «RONDA N» y un cartel que empieza en «Recuento de conchas» y pasa a «¡Concha para X!» (con medias: «¡Concha
  para X! Media para Y.» o «Medias para Y y Z.»), luego «¡X gana la partida!» o «¡Empate en lo más alto entre X y Y!
  ¡Sprint final!»; si nadie llegó, «¡Nadie ha llegado al agua! Esta vez no hay concha.». Abajo, «Siguiente ronda en N»
  (`PhaseSecondsLeft`), «¡Al podio en N!» o «¡Sprint final en N!».
- **Medias conchas**: cada hueco guarda dos medias; la media es la concha reina partida en diagonal con el corte en
  zigzag (`TNRaceArt::HalfShell`; las dos mitades encajan). Las que ya tenía salen con su «pom» (la media, más bajito);
  tras caer la entera de la ganadora, las medias de la cuenta atrás saltan una a una (cada 0,35 s) a su hueco con un
  «plin» pequeño y destellos, y la cara de quien la gana celebra con un salto más corto. Si completa una concha, la otra
  mitad llega desde abajo a la derecha y encaja, y queda entera con un rebote. Si la entera cae sobre una media, la
  completa y la otra mitad salta al hueco siguiente.
- Una columna por jugador, siempre en el orden de entrada a la partida: los huecos de concha (`RoundTarget`, 3) en
  zigzag de abajo arriba unidos por una cuerda, con la corona apagada en lo alto; debajo, su cara del HUD con su color
  de piel (`TNRaceArt::TurtleFaceFor` gira el verde de la piel y conserva luces, pecas y el filo) en un aro del color de
  su caparazón (o de su piel) y su nombre en una etiqueta de arena (dorada y con «TÚ» la tuya).
- Guion (segundos desde que sale): 0,1-0,5 entran las columnas con rebote; desde 0,55 aparecen una a una las conchas
  que ya tenía cada uno, con un «pom» que sube por la escala; hacia 1,3 nace en el centro la concha de la ronda (la
  reina de las de puntos: rosa y violeta con estrella) con destellos y un «plin» pequeño; 0,55 s después vuela en arco
  con estela hasta su hueco (0,65 s) y cae con «¡plin!» (el de la grande; el de la reina si corona), rebote y destellos.
  La cara del ganador pasa a ojos de estrella y salta con un brillo dorado detrás; si la concha completa el objetivo, la
  corona baja de lo alto a su cabeza. Si nadie ganó: «pom… pom» que bajan y todas las caras mareadas.
- Las conchas de la ronda llegan desde las que cada uno tenía al empezarla: el subsistema apunta `RaceShellHalves`
  mientras se prepara y en los 4 primeros segundos de carrera, así que da igual que llegue por red antes que la fase. Si
  `RoundWinner` llega tarde, la columna se corrige mientras la concha no haya salido volando. La campeona o el empate se
  deciden igual que en el servidor (`DecideVerdict`): la corona baja a la única en lo más alto con tres conchas o más; si
  hay empate ahí, las empatadas brillan en coral y el cartel anuncia el sprint.

### Sprint final (`SprintIntro`)

- A pantalla completa: fondo azul marino con rayos dorados y corales que giran, «¡SPRINT FINAL!» que entra de golpe y se
  mece, la cinta «¡Empate a 3 conchas! Solo corren las finalistas, desde la mitad de la playa: la primera en el agua se
  lleva la partida.», las caras de las finalistas (su piel, su aro y su nombre) que entran una a una de un «¡pum!» con un
  «VS» entre ellas, y confeti y arena cayendo. Suena la fanfarria sintetizada (metales «¡ta-ta-ta-taaan!», redoble,
  timbal y platillo). Abajo, «¡Tú corres!…» o «Tú lo miras de fantasma…» y «El sprint empieza en N».
- Tras el sprint, la pantalla del campeón sale directamente (sin el recuento de la concha que corona) y su cartel dice
  «¡Gana el sprint final y se lleva la partida!».

### Campeón (`Champion`)

- Tras el recuento de la tercera concha (7 s), o tras un recuento con la concha que corona si se salta directamente al
  campeón (`TN.Race.Champion`), entra con un fundido mientras el recuento se va por debajo. Tapa toda la pantalla,
  también el panel de resultados del HUD de siempre («Volviendo al lobby en: 99» de `CountdownValue`).
- Izquierda, sobre un panel azul marino que se funde con el fondo: la cinta «¡CAMPEONA DE LA PLAYA!», el cartel con su
  cara (su piel, corona, saltando), su nombre y sus conchas (aparecen con «pom») y los botones **Volver a jugar**,
  **Cambiar de modo** y **Salir** (etiquetas de arena con icono; se inflan bajo el ratón con un «pom» suave y suenan con
  «plin» al pulsar). Solo el anfitrión (`ATN_BeachRaceGameMode::CanLocalPlayerChoose`) tiene los dos primeros
  encendidos; los demás los ven apagados con el candado «Volver a jugar o cambiar de modo lo decide el anfitrión.» y
  pueden salir. Los botones solo piden: `ATN_BeachRaceGameMode::RequestChampionChoice(this, PlayAgain | ChangeMode |
  Quit)`; tras elegir, «¡Allá vamos!» o «¡Hasta la próxima!» y los botones se apagan. Mientras se ve, ratón a la vista
  (`FInputModeGameAndUI`); al quitarse, `FInputModeGameOnly`.
- Derecha, de fondo animado: el podio en 3D sobre un cielo pintado (degradado, sol que late y nubes que pasan), con
  «1.º X», «2.º Y» y «3.º Z» (colores de medalla) encima de cada tortuga y confeti que cae solo sobre el podio.
- Música: a los 2,2 s de la fase suena para todos la de victoria (`UTN_MatchMusicSubsystem::DebugPlayTrack(Victory)`).
  Antes, en `Results`, el director de la música ya ha puesto victoria a la campeona y derrota a las demás (lo fija a los
  1,6 s y no lo vuelve a tocar con `CountdownValue` = 99); al elegir, el director la funde como siempre.

### El podio (`ATN_RacePodiumStage`)

- **Decisión: escena capturada, no la cámara del jugador.** Un escenario 3D de verdad a 1,5 km sobre el mapa, capturado
  con `SceneCapture2D` (1600 × 900, `SCS_SceneColorHDR`, solo sus componentes) a un render target que la pantalla pinta
  con `M_UI_Preview` (el del escaparate de la tienda: el alfa es la cobertura, así que el cielo es el de la interfaz). No
  toca el `PlayerCameraManager` ni los peones (el GameMode los tiene congelados), se ve igual en todas las máquinas y en
  cualquier mapa (vista previa en el lobby) y deja el podio a la derecha de los botones sin mover la vista del juego.
- Local (sin réplica), visible solo en su captura y con el canal de luz 2 (ni el sol del nivel ni el escaparate de la
  tienda, que usa el 1): sol direccional cálido sin sombras, principal con sombras, relleno frío, contraluz y una luz
  rosa que late junto a la concha. Solo se captura y se anima mientras se ve la pantalla (`SetLive`).
- Todo a `TNBeach::Scale` (28 ×): 1.º **vaso de plástico** rojo boca abajo de 6 cm (1,68 m; aros en relieve y el borde
  blanco enrollado); 2.º **caja de zumo** de 10,5 × 6,3 cm tumbada y aplastada a 3,4 cm (0,95 m; abollada, una esquina
  chafada, naranja con franja blanca, una naranja dibujada y la **pajita** doblada a rayas); 3.º **chancla** de
  26 × 9,5 cm con 1,8 cm de suela (50 cm; el talón hacia la cámara y la tira rosa en Y detrás de la tortuga sentada).
  Alrededor, un **tapón** azul de 3 cm con estrías, una **lata** roja tumbada y medio enterrada, conchas, una estrella de
  mar y piedrecitas; la orilla a 15 m con espuma que va y viene, el mar hasta el horizonte (turquesa → azul hondo) y dos
  islitas a 2,5-3 km con selva y palmeras de 280 m (10 m reales). Mallas estáticas construidas en ejecución
  (`TNProcRuntimeMesh`, `M_CosmeticVertexColor`: el alfa del vértice es el brillo metálico).
- Tortugas: `TotugaDemo_Rig` a escala 2,5 con su aspecto (`UTN_CosmeticLook::ApplyLook`: casco, caparazón, piel y ojos
  de sus cosméticos, de `Podium`) y `UTN_TurtleAnimInstance` sin personaje: **Trofeo** la primera (la concha trofeo va
  entre sus manos, huesos `LeftHand` y `RightHand`, mirando a la cámara y meciéndose), **Decepcionada** la segunda y
  **Pataleta** la tercera, sentada en la chancla. La cara de `M_TurtleBody` acompaña: la primera, sonrisa enorme,
  colorete y un «>_<» de gusto en cada vuelta; la segunda, párpados caídos y boca pequeña que se abre en el suspiro; la
  tercera, ojos apretados, colorete rojo y boca gritando. Parpadean de vez en cuando. Con menos de tres jugadores, los
  puestos que faltan se quedan vacíos.
- Encuadre: cámara a 21,5 m (`TN.Race.PodiumDistance`, en cm), FOV 32° (`TN.Race.PodiumFOV`), girada para que el podio
  quede al 62 % del ancho. Exposición de la captura en pantalla: `TN.Race.PodiumExposure` (1,5).

### Poses nuevas y zambullida (`UTN_TurtleAnimInstance`)

- `SetCelebration(ETNTurtleCelebration)` (`None`, `Trophy`, `Disappointed`, `Tantrum`; también desde Blueprint),
  `GetCelebration` y `GetCelebrationTime`: una capa encima de todo, en espacio de malla sobre la postura en T, con un
  peso que entra y sale suave (al cambiar de una a otra, la anterior sale antes de que entre la nueva). Vale con
  personaje o sin él. Bucles exactos, como un GIF: Trofeo 1,6 s, Decepcionada 3,2 s y Pataleta 1,2 s (detalle en
  `Docs/Animacion_Tortuga.md`).
- **Zambullida de la meta**: si la tortuga despega o empieza a caer (en los primeros 0,35 s de la caída) dentro de
  `ATN_BeachRaceGenerator::IsCliffJumpZone` (la repisa final y el vacío sobre el agua), se pone de cabeza: cuerpo
  estirado, brazos por encima de la cabeza con las manos juntas y piernas juntas con las puntas de los pies; el cuerpo
  gira sobre la cadera siguiendo la trayectoria (tumbada en lo alto del salto, casi vertical, 165°, al caer deprisa) y
  entra así en el agua. Si la caída entra en la zona más tarde (lanzada desde más atrás), empieza igual en cuanto cae a
  más de 10 m/s (`CliffDiveLateFallSpeed`; un salto que vuelve a la repisa no llega a tanto). Se acaba al aterrizar o al
  nadar (y con el panzazo, el caparazón, el derribo o si la llevan). Cosmética y local en cada máquina a partir del
  movimiento replicado, sin RPC; fuera de la playa no hay generador y nunca pasa. `IsCliffDiving()` lo dice. Ya no la
  corta la bola de las caídas largas: el GameMode hace inmune esa caída (ver «Salto final al agua»).

### Probar sin jugar

En cualquier mapa y solo en la máquina que lo escribe:

- `TN.Race.Tally [ganador] [jugadores] [final] [medias]`: recuento con tu tortuga (columna 0) y otras de mentira (Coral,
  Bruma, Perla…, con pieles, caparazones, cascos y ojos del catálogo) y medias conchas de mentira. `ganador` es la
  columna a la que vuela la concha entera (-1 = nadie llega al agua; 0 por defecto), `jugadores` de 1 a 6 (4), `final 1`
  hace que la concha corone (baja la corona) y que después salga la pantalla del campeón, y `medias` cuántas columnas
  siguientes ganan media concha (1). Sin `final` se cierra sola a los 8 s.
- `TN.Race.CountdownPreview`: la cuenta atrás de 10 s con sus «¡toc!» y el «¡TIEMPO!» con silbato.
- `TN.Race.SprintPreview [finalistas]` (2-6): el título del sprint final con la fanfarria; se cierra solo a los 7 s.
- `TN.Race.Podium [jugadores]` (1-3): la pantalla del campeón con el podio y la música; cualquier botón la cierra.
- `TN.Race.PreviewOff`: cierra la vista previa.
- En partida: `TN.Race.WinRound` (llegada: cuenta atrás y luego recuento), `TN.Race.Sprint` (sprint final) y
  `TN.Race.Champion` (recuento con la concha que corona y luego el podio).
- Zambullida: en `LVL_BeachRace`, saltar desde la repisa del acantilado de la meta (sin bola: ver «Salto final al agua»).
