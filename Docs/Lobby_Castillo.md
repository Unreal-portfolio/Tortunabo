# Lobby: el castillo de arena redondo

El lobby de Tortunavy (`LVL_Lobby`) es un castillo de arena redondo al fondo de un valle, con un bioma en cada hora
del reloj alrededor (ver «Valle»).
- Al norte está la puerta doble: dos puertas con una sala en medio, que es uno de los dos sitios para ponerse listo.
- En la plaza están la pila de huevos, que es el otro sitio para ponerse listo, y los puestos alrededor: tienda, cuartel
  y probadores.
- Un muro interior con adarve cierra el patio de pruebas, con un mini parkour. En su centro está la torre del homenaje,
  con balcón y escalera de caracol. En su azotea hay un cofre del tesoro que se rebusca (siempre da un objeto).

Todo se construye en código y se ve directamente en el editor. Del castillo solo se replica el estado de la puerta y de
los huevos; el cofre del tesoro es un actor replicado aparte que el castillo crea al empezar la partida.

Coordenadas en cm, locales al castillo (colocado en el origen). Se usa un reloj visto desde arriba: **las 12 son +Y**
(la puerta), las 3 son -X, las 6 son -Y y las 9 son +X. `ATN_SandCastleLobby::LayoutSpot(Hora, Distancia, Yaw)` da el
punto a esa hora y distancia del centro, mirando al centro.

## `ATN_SandCastleLobby` (`Lobby/TN_SandCastleLobby.*`)

Está colocado en `LVL_Lobby` (carpeta `Lobby_Castillo`). Se construye:

- en `OnConstruction`, al moverlo o cambiarlo;
- en `PostRegisterAllComponents`, al cargar el nivel en el editor o al copiarlo para jugar;
- en `BeginPlay`.

Las mallas generadas no se guardan en el nivel:
- los componentes procedurales llevan `RF_Transient`;
- las mallas estáticas en ejecución, `RF_Transient | RF_DuplicateTransient`.

El segundo parámetro de `CreateDefaultSubobject` no marca nada como transitorio: hay que llamar a `SetFlags`.

Con `TN.Lobby.Castle 0` se esconde en ejecución; hay que volver a cargar el lobby. Mallas:

- `CastleMesh`, con colisión: suelo, muralla, torres, puerta doble, muro interior, torre del homenaje, escaleras,
  toboganes y montículo.
- `DecorMesh`, sin colisión: conchas, estrellas, banderas, guirnaldas, antorchas, catalejo, bases de los huevos, playa
  y mar.
- `BarrierMesh`: barreras invisibles.

Las piezas que también salen en el mapa procedural (puerta doble, montículo y huevos, hojas de puerta, torres,
antorchas) están en `Lobby/TN_CastleKit.h` (`TNCastleKit`).

### Muralla, torres y suelo

- **Suelo y playa**: suelo redondo de arena (radio 2620, cota 2) con rayos de colores. Por fuera, un anillo de playa
  hasta 39 m y, con `bDrawSea` (por defecto), el mar a -34. En `LVL_Lobby` el mar está apagado: su sitio lo ocupa el
  valle (ver «Valle»).
- **Muralla**: radio interior `Radius` = 2400, 190 de grosor y unos 6 m de alto, con una ondulación suave.
  - Lleva almenas, marcas de cubo (rebordes horizontales) y conchas incrustadas.
  - Encima del borde de fuera tiene barreras invisibles: nadie se cae del castillo.
  - El adarve de la muralla se puede pisar, aunque ya no hay medusa que suba a él (todas están en la plaza).
- **Torres**: nueve torres de cubo en la muralla, sin simetría, de 7,6 a 12,5 m de alto y de 200 a 300 de radio.
  Unas acaban en cono y otras en azotea con mástil. Cada una lleva su bandera y una barrera cilíndrica.

### Puerta doble (las 12)

La geometría es `TNCastleKit::BuildGatehouse`, la misma que la de la salida del mapa procedural. El origen es el
umbral de la puerta 1, en y = `GateY` = 2470, y la sala sale hacia fuera de la muralla (+Y).
- **Estructura**:
  - puerta 1 en la muralla y puerta 2 a 7 m, hacia fuera;
  - cada hueco mide 8,6 × 5,4 m, con pilares que tapan los cantos de las hojas (sin rendijas);
  - fachadas de 7,6 m con almenas, marcas de cubo y arco de dovelas;
  - sala de 9,2 × 7 m, abierta al cielo, con paredes de 6,4 m con almenas, conchas y dos antorchas (`RoomLight`);
  - ocho estrellas en el suelo, en dos filas de cuatro (a 2,3 m unas de otras), donde se aparece en el mapa procedural;
  - dos torres grandes (13,5 m) a los lados de la puerta 1 y dos torreones (9,8 m) a los de la puerta 2;
  - barreras invisibles encima de paredes y fachadas.
- **Hojas**: son de tablones sobre un tablero de fondo, con la cara de arriba recta, así que no se ve a través.
  - La puerta 1 se abre hacia la plaza (unos 100° en 1,25 s).
  - La puerta 2 siempre está cerrada en el lobby.
  - `GateBlock` y `Gate2Block` solo bloquean con la hoja cerrada del todo.
- **Cartel**: el rótulo `GateName` («TORTUNAVY») va en la fachada de la puerta 1, por la cara de la plaza, y se encoge
  para caber en 5,4 m.
- **Estar en la sala cuenta como listo**, igual que un huevo: `ATN_HQGameMode::SetPlayerReadyState`.
  - La puerta 1 se abre si alguien se acerca por la plaza (a menos de 7,5 m) o está junto a ella por dentro.
  - También sigue abierta mientras haya gente dentro sin estar todos.
  - Con todos dentro se cierra y se ve la puerta 2 cerrada delante. Con todos listos empieza la cuenta atrás.
- **Cómo se empezará** (`GetStartStyle()`, en el servidor): si hay más jugadores listos en la sala que en los huevos,
  la partida del mapa procedural empieza en la puerta doble; si no, en los huevos. En empate, la puerta.

### Muro interior y torre del homenaje

- **Muro interior** (y = `CutY` = -800, 2,6 m de grueso y 6 m de alto): va de la torre del homenaje a la muralla por
  los dos lados. Separa la plaza (norte) del patio de pruebas (sur).
  - Arriba hay un adarve de 1,7 m entre almenas, que están juntas para que no se cuele una tortuga.
  - Lleva rebordes y conchas, y guirnaldas de banderines entre mástiles.
  - **Adarve izquierdo** (-X): se baja a él desde el rellano de la escalera de caracol por una escalera recta de once
    peldaños. Tiene un tobogán rojo que baja a la plaza (x = -1400).
  - **Adarve derecho** (+X): igual que el izquierdo, se baja a él desde la azotea por otra escalera recta de once
    peldaños (sale del rellano del este). También se sube botando en la medusa pequeña de su pie. Tiene un tobogán
    turquesa que baja al patio de pruebas (x = 1500) y un mirador junto a la muralla, con catalejo, cubo con pala y
    banderón.
  - **Toboganes**: canal de 1,9 m que empieza a ~60° (se resbala) y acaba plano en la arena, con bordes, barandillas,
    panza por debajo y dos pilares con la cabeza inclinada como la panza (pegados a ella, sin atravesarla).
- **Torre del homenaje** (las 6, en (0, -800)): cilindro de radio 400, con azotea a 9 m.
  - La azotea tiene almenas y hace de balcón que mira a la plaza. Encima hay un torreón con cono y bandera.
  - Se atraviesa por un túnel de 2,6 × 3 m, con arcos en las dos bocas, dos antorchas y luz (`TunnelLight`).
  - **Escalera de caracol** por fuera: 36 peldaños macizos de 30 cm de grueso (de 290° a 430°, 2,1 m de ancho),
    cerrados por las seis caras, con barandilla invisible y bolardos de arena. Pasa por encima de la boca norte del
    túnel y de su arco sin tocarlos (por eso el túnel mide 3 m de alto y los peldaños no son más gruesos).
  - Arranca a 290°, pegada al muro interior: entre el primer peldaño y el muro no cabe la tortuga, así que los tres
    primeros salen 1,05 / 0,7 / 0,35 m más hacia la plaza y no tienen barandilla (se pisan desde fuera).
  - **Rellanos** a la altura de la azotea, con barandilla: el del oeste (de 70° a 100°, donde acaba el caracol) baja
    al adarve izquierdo y el del este (de 255° a 285°, cruzando la azotea) baja al adarve derecho.
  - **Tarima del cofre**: en la azotea, delante del torreón, centrada en (0, -600) (`TreasureSpot`). Mide 1,25 m de
    radio (`TreasureDaisR`) y 24 cm de alto (`TreasureDaisH`); es de arena, con un reborde oscuro y seis conchas
    alrededor, y forma parte de `CastleMesh` (tiene colisión y se sube andando).
    - Entre la tarima y el torreón quedan unos 90 cm de paso de un rellano al otro; también se puede cruzar por
      encima de la tarima.
    - Entre la tarima y las almenas del norte queda una franja de unos 60 cm.
    - Encima va el cofre del tesoro (ver más abajo).

### Pila de huevos (plaza, centro en (0, 700))

Montículo de dos alturas (`TNCastleKit::BuildEggMound`), con un huevo por cada jugador que cabe en la sesión (ocho,
`ATN_SandCastleLobby::NumEggs`, atado con un `static_assert` a `TNCastleKit::EggMound::NumEggs`).
- **Montículo**:
  - el piso bajo mide 480 de radio y 60 de alto; el alto, 175 de radio y 200 de alto;
  - siete huevos van en el piso bajo, a 335 del centro, en un arco de 282° (47° entre vecinos) centrado detrás, de 129°
    a 51° (`TNCastleKit::EggMoundRingAngle`); el octavo, en lo alto;
  - el arco deja 78° libres delante, en el lado de la puerta, por donde se llega al escalón;
  - el escalón para subir es un tocón de arena con una vieira encima;
  - colores de los huevos (`EggAccent`): turquesa, coral, amarillo, lila, verde lima, rosa, azul y naranja.
- **Estar listo**: meterse en un huevo marca al jugador como listo y baja su tapa. Cuenta si está a menos de 95 cm de
  su eje y entre su base y 3,2 m por encima. Salir del huevo lo quita de listo.
- Las tapas libres flotan encima dando vueltas. La zona de listos vieja (`ATN_LobbyReadyZone`) se apaga.

### Adornos

Hasta sesenta conchas y estrellas de mar sueltas por la plaza y el patio, y otras ocho alrededor del montículo.

### Ocho jugadores

La sesión admite ocho (`MaxPlayers`); el lobby, la salida del cooperativo y el clásico están hechos para todos.
- **Listos**: ocho huevos en la pila (ver arriba) y la sala de la puerta doble, que no tiene tope. El marcador «Sala: X/Y»
  enseña las plazas de la sesión (`ATN_HQGameMode::LobbyExpectedPlayers`, 0 = `UMP_GameInstance::GetMaxPlayers`).
- **Dónde aparecen**: `LVL_Lobby`, `LVL_HQ` y `LVL_Run` siguen teniendo cuatro `PlayerStart` (no se ha tocado ningún mapa).
  Los cuatro primeros aparecen donde siempre. Cuando no queda ninguno libre, `ChoosePlayerStart` de `ATN_HQGameMode` y de
  `ATN_RunGameMode` llama a `TN_PickSpreadPlayerStart` (`Core/TN_GameModeSpawnUtils.*`), que crea un `PlayerStart` nuevo
  junto a uno del mapa:
  - prueba desplazamientos de 2,2 m a los lados (primero), hacia atrás y hacia delante del `PlayerStart` de origen, del
    más corto al más largo; con el mismo desplazamiento, el sitio más lejos de los demás peones;
  - cada sitio tiene que tener suelo firme a menos de 45 cm de altura del de origen, caber la cápsula del peón, no
    chocar con nada en un barrido desde el origen y quedar a 2 m de cualquier peón ajeno; si no cabe ninguno así, repite
    con pasos de ~1,1 m y 90 cm de margen;
  - el `PlayerStart` nuevo copia la orientación y la etiqueta del de origen (lleva además el `Tag` `TNExtraStart`), se
    queda en el mundo y se reutiliza cuando su jugador se va; el mapa se recarga al viajar, así que no se guarda nada;
  - en el tutorial de la primera partida (todos aparecen en el `PlayerStart` con la etiqueta de tutorial) pasa lo mismo.
- `GetSpawnSpots()` da ahora ocho sitios (los cuatro de siempre y una segunda fila 3 m detrás, hacia la puerta). El script
  `Scripts/place_lobby_castle.py` sigue poniendo los cuatro `Salida_1` … `Salida_4` y no hace falta cambiarlo (con ocho
  `PlayerStart` en el nivel no se crearía ninguno de más).
- **Al viajar**: con huevos, cada jugador aparece en el suyo (siete abajo y uno arriba); con la puerta doble, en una de las
  ocho estrellas de la sala (`Gatehouse::SpawnSpot`).

## Valle (`ATN_LobbyValley`, `Lobby/TN_LobbyValley*`)

El castillo está abajo del todo de un valle cerrado por montañas. Alrededor hay un bioma por hora del reloj, y desde la
azotea de la torre del homenaje se ven todos de un vistazo. Detrás de la sierra, una cordillera lejana tapa todo el
horizonte: no se ve el borde del nivel.

- **Código**:
  - `Lobby/TN_LobbyValley.cpp`: terreno, agua, lava, cascada, formaciones, casitas, vegetación y efectos.
  - `Lobby/TN_LobbyValley_Fauna.cpp`: animales, pájaros del castillo y bandadas.
  - `Lobby/TN_LobbyValleyTerrain.h`: lógica pura (sectores, alturas, colores, vegetación por estilo y la rejilla).
- **Construcción**: como el castillo (`OnConstruction`, `PostRegisterAllComponents` y `BeginPlay`).
  - Es igual en el editor y en cada máquina, con la semilla `Seed`. No se replica ninguna malla.
  - El actor se replica sin propiedades: así llega a los clientes si lo coloca el servidor.
  - Solo se rehace si cambian `Seed`, `FloraDensity` o `MaxAnimals`. Moverlo no lo rehace: todo cuelga de su raíz.
  - En un servidor dedicado no se construye nada.
- **No se guarda en el nivel**: `TerrainMesh` y `WaterMesh` llevan `RF_Transient` y punteros `Transient`. Las HISM/ISM y
  las mallas estáticas llevan `RF_Transient | RF_DuplicateTransient`: la copia del PIE se construye sola.
- **Consola**: `TN.Lobby.Valley 0` lo esconde en ejecución y vuelve a pintar el mar del castillo (hay que volver a cargar
  el lobby).

### Forma

Coordenadas locales del valle (centrado en el castillo), en cm.

- **Suelo**: empieza a 38,7 m, 2 cm por debajo del borde de la playa del castillo y con su misma arena. Sube despacio
  (unos 56 cm a 90 m). El agua de la laguna y del manglar está a -25.
- **Sectores**: el relieve propio de cada bioma va de 52 a 150 m del centro. Entre dos sectores hay una transición de
  6° (0,2 horas) a cada lado, y las fronteras ondulan.
- **Sierra**: sube desde 120 m hasta la cresta, a 205 ± 14 m, y baja por detrás.
  - Picos crestados de 0,5 a 1,25 veces el alto de su sector: de 39 m en la playa a 90 m en las cumbres nevadas.
- **Cordillera lejana**: a 350 m, picos de 65 a 125 m, azulados por la distancia y con nieve en las cumbres. Asoma por
  encima de la sierra y cierra el horizonte. El terreno acaba a 420 m, detrás de ella.
- **Malla**: rejilla polar de 240 radios por 72 anillos, cada vez más separados (1,3 m junto al castillo, 15 m al
  fondo).
  - Son 34 080 triángulos de caras planas con color de vértice, sin colisión (nadie sale del castillo).
  - Material `M_ProcTerrain`. Sin él, el de color de vértice del castillo.
- **Volcanes y montículos**: van por encima de los sectores, así que no se cortan en las fronteras.
  - Volcán grande: en la sierra, a las 4.
  - Volcán pequeño: en el valle, entre el cañón y el volcán.
  - Montículos: cinco isletas, el cabo del faro y las colinas del pueblo y las granjas.

### Un bioma por hora

| Hora | Sector | Bioma | Qué hay (y sus animales) |
|---|---|---|---|
| 12 | Laguna | Agua con isletas | laguna turquesa con cinco isletas y palafito; al fondo, cascada de unos 30 m con bruma (flamencos, pelícano y peces que saltan) |
| 1 | Playa | Playa | arena, palmeras, cabo con faro, caracola gigante y barco varado (tortuguitas, cangrejos y una gaviota) |
| 2 | Dunas | Desierto | dunas, saguaros, la tortuga colosal de piedra, un obelisco y un cráneo fósil (suricatos que vigilan y un correcaminos) |
| 3 | Cañón | Desierto | mesas en terrazas con estratos rojos, chimeneas de hadas, roca en equilibrio y una mesa grande al fondo (lagartijas y buitre) |
| 4 | Volcán | Volcánico | cono de unos 57 m con lava en el cráter, tres ríos de lava que brillan, humo y brasas; obsidiana, basalto, fumarola y domo de lava (escarabajos y salamandras de fuego) |
| 5 | Acantilados | Rocas | terrazas grises, agujas y castillo en ruinas (cabras montesas y un águila en la aguja más alta) |
| 6 | Cumbres nevadas | Rocas | la sierra más alta, nevada desde 43 m, detrás de la torre del homenaje; abetos, peñascos y una cabaña (marmotas y una cabra) |
| 7 | Bosque | Rocas | colinas de pinos, abetos y abedules, círculo de piedras en un claro y una cabaña (conejos) |
| 8 | Pueblo | Zona humana | tres colinas con diez casitas de colores (dos echan humo), molino y depósito de agua (gallinas y un gato) |
| 9 | Granjas | Zona humana | parcelas de colores con setos, pacas, granero rojo y molino (palomas y una gallina) |
| 10 | Selva | Selva | árboles de copa, ceibas, bambú y plataneras, pirámide escalonada, cabeza de piedra y pilares kársticos (monos, tucán y capibaras) |
| 11 | Manglar | Manglar | llanura de fango a ras de agua con mangles, cipreses y juncos, y un palafito (garzas, cangrejos violinistas y ranas) |

La puerta doble da a la laguna. Por la puerta 1 no se ve fuera, pero desde la plaza asoman por encima de la muralla la
cascada, la sierra y la cordillera. Desde la azotea se ve la laguna entera.

### Vida

- **Vegetación**: las especies de cada bioma del mapa procedural, con sus mallas y su material con viento.
  - Hay una HISM por bioma, especie y variante (unas 80). En el log sale el total de instancias.
  - En la sierra solo hay árboles y peñascos, menos y más grandes. Más allá de 245 m no hay nada: lo lejano es más
    simple.
  - Nada se corta por distancia (todo el valle está a la vista). El viento solo se evalúa a menos de 120 m.
- **Animales**: 53 de 25 especies (tope `MaxAnimals` = 56), entre 69 y 126 m del centro. Nunca entran en el castillo
  ni en el valle cercano.
  - Son cuerpos rígidos de la fauna del mapa procedural: una ISM por especie, más otra para lo que brilla. Van
    aumentados de 1,3 a 2,3 veces para que se vean desde el castillo.
  - Pasean cerca de su sitio, van a saltitos (conejos, monos, cabras, ranas, tucán), picotean, pastan, vigilan de pie
    (suricatos, marmotas) o flotan (pelícano). Los peces saltan del agua con salpicadura.
  - Son locales y cosméticos, sin huida: nadie llega hasta ellos.
- **Pájaros del castillo** (`CastleBirds` = 6): gaviotas y palomas que van de almena en almena.
  - Las almenas se buscan con trazas de visibilidad sobre la muralla y la azotea de la torre (las barreras invisibles
    no cuentan).
  - A veces dan un rodeo por encima de la plaza. Si se acerca una tortuga, salen volando.
  - En el castillo solo hay pájaros: nada de fauna de suelo.
- **Bandadas en círculo** (`TNAmbientFX`, 48 pájaros en 10 bandadas):
  - sobre el castillo, gaviotas altas y golondrinas rápidas;
  - en la laguna y la playa, gaviotas; en el desierto, buitres; en las cumbres, águilas;
  - en el bosque, pájaros oscuros; en el pueblo, palomas; en la selva, guacamayos; en el manglar, garzas.
- **Efectos**: humo y brasas en el volcán grande, humo fino en el pequeño, bruma al pie de la cascada y humo en dos
  chimeneas.
- **Sonido**: sin cambios. El paisaje sonoro del lobby sigue igual.

### Con el castillo y colocación

- El castillo pinta el mar y la orilla solo con `bDrawSea`. El valle lo apaga (`SetDrawSea(false)`) en el castillo
  más cercano (a menos de 80 m). La playa de fuera del castillo sigue: el valle empieza debajo de su borde.
- `Scripts/place_lobby_castle.py` lo pone en el origen (`Valle_Biomas`, carpeta `Lobby_Valle`) y deja el mar del
  castillo apagado. Si el nivel trae castillo pero no valle, lo pone `ATN_HQGameMode::SpawnLobbyShops`.
- **Coste**:
  - unas 130 piezas de dibujo (terreno y formaciones, agua, unas 80 HISM de vegetación, unas 27 ISM de animales y
    las de los pájaros);
  - sin colisión ni navegación;
  - un Tick ligero: 53 animales, 6 pájaros, 10 bandadas y 7 emisores;
  - construcción de unos cientos de milisegundos al cargar el nivel (se ve en el log, `[Valle]`).

## Colocación en `LVL_Lobby`

Todo está en la carpeta `Lobby_Castillo` del nivel. `Scripts/place_lobby_castle.py` lo vuelve a colocar, dentro del
editor y sin guardar el nivel.

| Actor | Sitio | Notas |
|---|---|---|
| `Castillo_Arena` (`ATN_SandCastleLobby`) | origen | `draw_sea` apagado (el valle ocupa el sitio del mar) |
| `Valle_Biomas` (`ATN_LobbyValley`) | origen | carpeta `Lobby_Valle`; ver «Valle» |
| `Tienda_LaConchaDorada` (`ATN_ShopKeeper`) | las 10:53, a 2225 | pegada a la muralla, entre la torre izquierda de la puerta y la siguiente |
| `Cuartel_General` (`ATN_GeneralBriefing`) | la 1:07, a 2160 | pegado a la muralla, entre la torre derecha de la puerta y la siguiente |
| `Probador_1` … `Probador_4` (`ATN_ChangingBooth`) | 2:04, 2:32, 3:00 y 3:26 | a 2150 y 2040 alternos, puerta al centro |
| `Medusa_1` (`ATN_JellyfishTrampoline`) | (900, -470) | 0,8, celeste: sube al adarve derecho |
| `Medusa_2`, `Medusa_3` | 9:10 a 1900 y 9:36 a 1650 | 1,1 rosa y 1,45 lila |
| `Medusa_4` … `Medusa_7` | (2050, -420), (1280, -180), (1700, -250) y (1150, 280) | 0,7 celeste, 0,9 lila, 1,0 rosa y 0,75 celeste |
| `Prueba_01` … `Prueba_08` (`ATN_PlaygroundPiece`) | patio de pruebas | escalones de polo, galleta, postes de cubo, pala giratoria, túnel (se pasa de pie), tobogán de concha |
| `Puente_Bamboleante` (`ATN_WobblyBridge`) | (350, -1950), yaw 180 | 7 m de vano, tablero a 2,3 m |
| `Salida_1` … `Salida_4` (`PlayerStart`) | x = ±150, ±450; y = 1700 - 0,25·\|x\| | z 97, mirando a los huevos (del quinto al octavo jugador, ver «Ocho jugadores») |
| Cofre del tesoro (`ATN_TreasureChest`) | (0, -600, 924), yaw 90 (mirando a la plaza) | **no está en el nivel**: lo crea el castillo en el servidor, en `BeginPlay` |

La maqueta original (paredes «Extrude»/SandWall, vallas, torres, botellas, carpas, huevos «Capsule»…) está 50 m más
abajo, en la carpeta `Referencia_Blockout_Original`. Se recupera subiéndola 50 m. En ejecución, `HideMaquette` sigue
escondiendo esas piezas si quedaran a la vista.

`ATN_HQGameMode::SpawnLobbyShops` solo coloca lo que falte (castillo, tienda, probadores y general) en niveles que no
los traen puestos. Si hay castillo y no valle, pone también el valle sobre el castillo (nunca dos).

El lobby del castillo no tiene selector de modo: la partida es el cooperativo del mapa procedural (`LVL_ProcMap`),
que es el valor por defecto de `UMP_GameInstance::SelectedProcMode`. El clásico (`LVL_Run`) solo sale si un
`ATN_ProcModeSelector` lo elige.

## El cofre del tesoro (`ATN_TreasureChest`, `Lobby/TN_TreasureChest.*`)

Cofre en lo alto de la torre del homenaje, en el centro de la azotea todo lo que deja el torreón. Se rebusca como
los decorados del mapa procedural: mantener E, el aro del HUD, «¡puf!» y el objeto que sale de un saltito. Pero
cuesta más (**5 s**), **siempre da un objeto al azar** de `DT_Items` y **se puede repetir** tras 2,5 s de respiro.
Las reglas y la red están en `Docs/Botin_Decorados.md`.

- **Quién lo crea**: `ATN_SandCastleLobby::SpawnTreasureChest`, en el servidor, desde `BeginPlay` y solo si el
  castillo está activo (`TN.Lobby.Castle 1`).
  - Es un actor replicado aparte (su dueño es el castillo) y no se guarda en el nivel: no hay que tocar `LVL_Lobby`.
  - Va sobre la tarima, en (0, -600, 924) locales del castillo, con su +X hacia el +Y del castillo (la plaza).
  - Si se destruye el castillo, el cofre se va con él.
- **Malla** (low-poly, colores de vértice como el resto del lobby; `TNProcRuntimeMesh::MakeStaticMesh`, con
  `RF_Transient | RF_DuplicateTransient`):
  - La caja mide 1,56 × 1,00 m y 64 cm de alto. Tiene tablones en tres hileras, zócalo oscuro, borde dorado,
    cantoneras doradas con remaches, flejes de hierro con remaches, una cerradura dorada con el ojo de la llave y dos
    asas de hierro.
  - Dentro lleva una cama de monedas con tres montones, monedas sueltas, cuatro gemas y una copa.
  - La tapa es de medio cañón, con tablones por fuera y madera oscura por dentro. Tiene los testeros con filo dorado,
    flejes y remaches, un labio dorado, el pasador sobre la cerradura, bisagras y una vieira dorada de emblema.
  - Con la tapa cerrada mide 1,17 m de alto. Con la tarima, desde la plaza se ve entre las almenas; la tapa abierta,
    la luz y los destellos asoman por encima.
  - Las mallas se hacen en `OnConstruction` y, si la copia del PIE llega sin ellas, en `BeginPlay`. No se hacen en
    servidor dedicado.
- **Colisión**: una caja (`ChestBlock`, `BlockAll` sin cámara) del tamaño del cofre cerrado. Las mallas no tienen
  colisión.
- **Tapa y brillo**: la tapa gira sobre su bisagra según el estado replicado, igual en todas las máquinas.
  - Mientras se rebusca, se entreabre a tirones: sube de 18° a 55° con el progreso, temblando y crujiendo.
  - Al salir el objeto, salta a unos 110° y aguanta 1,2 s.
  - Luego se cierra con un «¡clonc!». Si se suelta E antes, se cierra igual.
  - La luz dorada de dentro (`GlowLight`, sin sombras) sube con la tapa, y de las monedas suben destellos.
- **El objeto** sale de dentro del cofre y cae delante de él, en la tarima (entre 30 y 58 cm por delante del frente,
  hasta 45 cm a cada lado): nunca se cae de la azotea. Quedan como mucho seis objetos sin recoger.

## La tienda (`ATN_ShopKeeper`)

Va a escala 1 (`StallScale`): por encima, el mostrador tapa al tendero. Mostrador de 5,2 m con toldo de rayas, cartel
y guirnalda.
- **Estantería del fondo**, pegada a la muralla: botes de pintura de los colores, caparazones de muestra, tarros de
  ojos, cascos y etiquetas de precio.
- **A los lados**: perchero con sombreros, barril con pala, cajas con conchas, el cofre y un farol.

Ver `Docs/Tienda_Probador.md`.

## El general del cuartel (`ATN_GeneralBriefing`, `Lobby/TN_GeneralBriefing.*`)

El General Galápago lleva gorra de capitán, caparazón de musgo y cuerpo verde bosque, a escala 3,4. Está detrás de una
mesa de madera con una maqueta del castillo, puntero y taza.

La mesa está dentro de una tienda militar de lona verde oliva de dos aguas.
- **La tienda**: parches de camuflaje, una solapa enrollada, mástiles, vientos (cortos por detrás, porque va pegada a
  la muralla), sacos terreros y cajas.
- **Luz**: un farol colgado del caballete (`TentLight`) ilumina al general y la mesa.
- **Cartel y bandera**: el cartel «CUARTEL GENERAL» va en el frontón (se encoge para caber) y el mástil con la bandera
  sale del caballete.
- El general se gira hacia el jugador local y le hace el saludo militar.

- Colocación: en `LVL_Lobby` está puesto. Si falta, `SpawnLobbyShops` lo pone sobre un actor con la etiqueta
  `TN_GeneralAnchor` o sobre el general de la maqueta.
- Al hablar con él, `AMP_GamePlayerController::ClientOpenBriefing` abre `UTN_BriefingWidget` (`UI/Briefing/`), con
  el estilo de la tienda. El general habla letra a letra y hay cuatro pestañas:
  - «CÓMO SE JUEGA»;
  - «MODOS DE JUEGO»;
  - «REGLAS»;
  - «CONTROLES», con las teclas reales de Enhanced Input de las acciones `/Game/Blueprints/Gameplay/Controls/IA_*`,
    de teclado y de mando.
- Teclas:
  - Q/E, flechas izquierda/derecha o 1-4 cambian de pestaña.
  - Arriba/abajo desplazan.
  - Escape, Intro o «¡Entendido!» cierran.

## Interacción

Los puestos (tienda, cuartel y probadores) miden la distancia de interacción desde `GetInteractionPoint()`, delante
del mostrador, la mesa o la puerta. La usan tanto el escaneo del cliente como `ServerTryInteract`, así que el aviso
sale justo donde se puede pulsar. Las zonas de interacción (cilindros) no se ven ni en el editor ni en el juego.
