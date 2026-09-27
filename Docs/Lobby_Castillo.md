# Lobby: el castillo de arena redondo

El lobby de Tortunavy (`LVL_Lobby`) es un castillo de arena redondo en una isla de playa.
- Al norte está la puerta doble: dos puertas con una sala en medio, que es uno de los dos sitios para ponerse listo.
- En la plaza están la pila de huevos, que es el otro sitio para ponerse listo, y los puestos alrededor: tienda, cuartel
  y probadores.
- Un muro interior con adarve cierra el patio de pruebas, con un mini parkour. En su centro está la torre del homenaje,
  con balcón y escalera de caracol.

Todo se construye en código y se ve directamente en el editor. Solo se replica el estado de la puerta y de los huevos.

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
  hasta 39 m y el mar a -34.
- **Muralla**: radio interior `Radius` = 2400, 190 de grosor y unos 6 m de alto, con una ondulación suave.
  - Lleva almenas, marcas de cubo (rebordes horizontales) y conchas incrustadas.
  - Encima del borde de fuera tiene barreras invisibles: nadie se cae del castillo.
  - El adarve de la muralla se puede pisar: se llega botando en la medusa del fondo del patio de pruebas.
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
  - cuatro estrellas en el suelo, donde se aparece en el mapa procedural;
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
  - **Adarve derecho** (+X): se sube por una escalera de 24 peldaños macizos pegada a la cara de la plaza (arranca
    junto a la muralla, en x = 2160, y llega arriba en x = 1200) o botando en la medusa pequeña. Tiene un tobogán
    turquesa que baja al patio de pruebas (x = 1500) y un mirador junto a la muralla, con catalejo, cubo con pala y
    banderón.
  - **Toboganes**: canal de 1,9 m que empieza a ~60° (se resbala) y acaba plano en la arena, con bordes, barandillas,
    panza por debajo y dos pilares.
- **Torre del homenaje** (las 6, en (0, -800)): cilindro de radio 400, con azotea a 9 m.
  - La azotea tiene almenas y hace de balcón que mira a la plaza. Encima hay un torreón con cono y bandera.
  - Se atraviesa por un túnel de 2,6 × 3,4 m, con arcos en las dos bocas, dos antorchas y luz (`TunnelLight`).
  - **Escalera de caracol** por fuera: 36 peldaños macizos (de 290° a 430°, 2,1 m de ancho), cerrados por las seis
    caras, con barandilla invisible y bolardos de arena.
  - **Rellano** de 70° a 100° a la altura de la azotea, con barandilla. Por el oeste sigue la escalera que baja al
    adarve izquierdo.

### Pila de huevos (plaza, centro en (0, 700))

Montículo de dos alturas (`TNCastleKit::BuildEggMound`).
- **Montículo**:
  - el piso bajo mide 430 de radio y 60 de alto; el alto, 175 de radio y 200 de alto;
  - tres huevos van en el piso bajo, a 265 del centro, y el cuarto en lo alto;
  - el escalón para subir es un tocón de arena con una vieira encima.
- **Estar listo**: meterse en un huevo marca al jugador como listo y baja su tapa. Cuenta si está a menos de 95 cm de
  su eje y entre su base y 3,2 m por encima. Salir del huevo lo quita de listo.
- Las tapas libres flotan encima dando vueltas. La zona de listos vieja (`ATN_LobbyReadyZone`) se apaga.

### Adornos

Hasta sesenta conchas y estrellas de mar sueltas por la plaza y el patio, y otras ocho alrededor del montículo.

## Colocación en `LVL_Lobby`

Todo está en la carpeta `Lobby_Castillo` del nivel. `Scripts/place_lobby_castle.py` lo vuelve a colocar, dentro del
editor y sin guardar el nivel.

| Actor | Sitio | Notas |
|---|---|---|
| `Castillo_Arena` (`ATN_SandCastleLobby`) | origen | |
| `Tienda_LaConchaDorada` (`ATN_ShopKeeper`) | las 10:53, a 2225 | pegada a la muralla, entre la torre izquierda de la puerta y la siguiente |
| `Cuartel_General` (`ATN_GeneralBriefing`) | la 1:07, a 2160 | pegado a la muralla, entre la torre derecha de la puerta y la siguiente |
| `Probador_1` … `Probador_4` (`ATN_ChangingBooth`) | 2:04, 2:32, 3:00 y 3:26 | a 2150 y 2040 alternos, puerta al centro |
| `Medusa_1` (`ATN_JellyfishTrampoline`) | (900, -470) | 0,8, celeste: sube al adarve derecho |
| `Medusa_2`, `Medusa_3` | 9:10 a 1900 y 9:36 a 1650 | 1,1 rosa y 1,45 lila |
| `Medusa_4` | (880, -1900), en el patio | 1,0, celeste: sube al adarve de la muralla |
| `Prueba_01` … `Prueba_08` (`ATN_PlaygroundPiece`) | patio de pruebas | escalones de polo, galleta, postes de cubo, pala giratoria, túnel (se pasa de pie), tobogán de concha |
| `Puente_Bamboleante` (`ATN_WobblyBridge`) | (350, -1950), yaw 180 | 7 m de vano, tablero a 2,3 m |
| `Salida_1` … `Salida_4` (`PlayerStart`) | x = ±150, ±450; y = 1700 - 0,25·\|x\| | z 97, mirando a los huevos |

La maqueta original (paredes «Extrude»/SandWall, vallas, torres, botellas, carpas, huevos «Capsule»…) está 50 m más
abajo, en la carpeta `Referencia_Blockout_Original`. Se recupera subiéndola 50 m. En ejecución, `HideMaquette` sigue
escondiendo esas piezas si quedaran a la vista.

`ATN_HQGameMode::SpawnLobbyShops` solo coloca lo que falte (castillo, tienda, probadores y general) en niveles que no
los traen puestos.

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
