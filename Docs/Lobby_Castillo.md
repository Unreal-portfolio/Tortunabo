# Lobby: el castillo de arena redondo

El lobby de Tortunavy (`LVL_Lobby`) es un castillo de arena redondo en una isla de playa. Tiene la puerta grande al
norte, una plaza con la pila de huevos donde se espera a los compañeros y los puestos alrededor (tienda, cuartel y
probadores). Un muro interior cierra el patio de pruebas, con un mini parkour, y en su centro está la torre del
homenaje, con balcón y escalera de caracol. Todo se construye en código y se ve directamente en el editor. Solo se
replica el estado de la puerta y de los huevos.

Coordenadas en cm, locales al castillo (colocado en el origen). Se usa un reloj visto desde arriba: **las 12 son +Y**
(la puerta), las 3 son -X, las 6 son -Y y las 9 son +X. `ATN_SandCastleLobby::LayoutSpot(Hora, Distancia, Yaw)` da el
punto a esa hora y distancia del centro, mirando al centro.

## `ATN_SandCastleLobby` (`Lobby/TN_SandCastleLobby.*`)

Está colocado en `LVL_Lobby` (carpeta `Lobby_Castillo`). Se construye:

- en `OnConstruction`, al moverlo o cambiarlo;
- en `PostRegisterAllComponents`, al cargar el nivel en el editor o al copiarlo para jugar;
- en `BeginPlay`.

Las mallas generadas son transitorias: no se guardan en el nivel y nunca se duplican (`RF_DuplicateTransient`). Con
`TN.Lobby.Castle 0` se esconde en ejecución; hay que volver a cargar el lobby. Mallas:

- `CastleMesh`, con colisión: suelo, muralla, torres, muro interior, torre del homenaje, escalera y montículo.
- `DecorMesh`, sin colisión: conchas, estrellas, banderas, bases de los huevos, playa y mar.
- `BarrierMesh`: barreras invisibles.

- **Suelo y playa**: suelo redondo de arena (radio 2620, cota 2) con rayos de colores. Por fuera, un anillo de playa
  hasta 39 m y el mar a -34.
- **Muralla**: radio interior `Radius` = 2400, 190 de grosor y unos 6 m de alto, con una ondulación suave.
  - Lleva almenas, marcas de cubo (rebordes horizontales) y conchas incrustadas.
  - Encima del borde de fuera tiene barreras invisibles: nadie se cae del castillo.
- **Torres**: once torres de cubo sin simetría, de 7,6 a 13,5 m de alto y de 200 a 300 de radio.
  - Unas acaban en cono y otras en azotea con mástil.
  - Cada una lleva su bandera de color y una barrera cilíndrica.
  - Las dos más altas flanquean la puerta (11:26 y 0:34).
- **Puerta grande** (las 12, y = 2470): 8,6 m de ancho y 5,4 de alto.
  - Tiene dintel con almenas y dos hojas de madera con herrajes y aldabas.
  - Por dentro lleva un cartel de madera con marco dorado y cuerdas. El rótulo (`GateName`, «TORTUNAVY») se encoge
    para caber en 5,4 m.
  - Se abre hacia fuera cuando alguien está a menos de 9 m o durante la cuenta atrás, y sigue abierta 2,5 s después.
  - `GateBlock` solo bloquea con la puerta cerrada del todo.
- **Muro interior** (y = `CutY` = -800): va de la torre del homenaje a la muralla por los dos lados. Lleva rebordes,
  almenas por las dos caras y conchas.
  - Separa la plaza (norte) del patio de pruebas (sur).
- **Torre del homenaje** (las 6, en (0, -800)): cilindro de radio 400, con azotea a 9 m.
  - La azotea tiene almenas y hace de balcón que mira a la plaza.
  - Se atraviesa por un túnel de 2,6 × 3,4 m, con arcos en las dos bocas, hacia el patio de pruebas.
  - Por fuera sube una escalera de caracol de 36 peldaños (de 290° a 430°, 2,1 m de ancho). Lleva una barandilla
    invisible por fuera y bolardos de arena.
  - En la azotea hay un torreón con cono y bandera.
- **Pila de huevos** (plaza, centro en (0, 700)): montículo de dos alturas.
  - El piso bajo mide 430 de radio y 60 de alto; el alto, 175 de radio y 200 de alto.
  - Tres huevos van en el piso bajo, a 265 del centro, y el cuarto en lo alto.
  - Para subir al piso alto hay un escalón: un tocón de arena con una vieira encima.
  - Meterse en un huevo marca al jugador como listo (`ATN_HQGameMode::SetPlayerReadyState`) y baja su tapa. Cuenta
    si está a menos de 95 cm de su eje y entre su base y 3,2 m por encima.
  - Salir del huevo lo quita de listo. Con todos dentro empieza la cuenta atrás.
  - Las tapas libres flotan encima dando vueltas. La zona de listos vieja (`ATN_LobbyReadyZone`) se apaga.
- **Adornos**: hasta sesenta conchas y estrellas de mar sueltas por la plaza y el patio, y otras ocho alrededor del
  montículo.

## Colocación en `LVL_Lobby`

Todo está en la carpeta `Lobby_Castillo` del nivel. `Scripts/place_lobby_castle.py` lo vuelve a colocar, dentro del
editor y sin guardar el nivel.

| Actor | Sitio | Notas |
|---|---|---|
| `Castillo_Arena` (`ATN_SandCastleLobby`) | origen | |
| `Tienda_LaConchaDorada` (`ATN_ShopKeeper`) | las 10:30, a 1850 | `StallScale` 1,5 |
| `Cuartel_General` (`ATN_GeneralBriefing`) | la 1:30, a 1800 | tienda militar |
| `Probador_1` … `Probador_4` (`ATN_ChangingBooth`) | 2:04, 2:32, 3:00 y 3:26 | a 2150 y 2040 alternos, puerta al centro |
| `Medusa_1` … `Medusa_3` (`ATN_JellyfishTrampoline`) | 8:44, 9:10 y 9:36 | tamaño 0,8 / 1,1 / 1,45; celeste, rosa y lila |
| `Prueba_01` … `Prueba_08` (`ATN_PlaygroundPiece`) | patio de pruebas | escalones de polo, galleta, postes de cubo, pala giratoria, túnel, tobogán de concha |
| `Puente_Bamboleante` (`ATN_WobblyBridge`) | (350, -1950), yaw 180 | 7 m de vano, tablero a 2,3 m |
| `Salida_1` … `Salida_4` (`PlayerStart`) | x = ±150, ±450; y = 1700 - 0,25·\|x\| | z 97, mirando a los huevos |

La maqueta original (paredes «Extrude»/SandWall, vallas, torres, botellas, carpas, huevos «Capsule»…) está 50 m más
abajo, en la carpeta `Referencia_Blockout_Original`. Se recupera subiéndola 50 m. En ejecución, `HideMaquette` sigue
escondiendo esas piezas si quedaran a la vista.

`ATN_HQGameMode::SpawnLobbyShops` solo coloca lo que falte (castillo, tienda, probadores y general) en niveles que no
los traen puestos.

## El general del cuartel (`ATN_GeneralBriefing`, `Lobby/TN_GeneralBriefing.*`)

El General Galápago lleva gorra de capitán, caparazón de musgo y cuerpo verde bosque, a escala 3,4. Está detrás de una
mesa de madera con una maqueta del castillo, puntero y taza.

La mesa está dentro de una tienda militar de lona verde oliva de dos aguas, con parches de camuflaje, una solapa
enrollada, mástiles, vientos, sacos terreros y cajas. El cartel «CUARTEL GENERAL» va en el frontón (se encoge para
caber) y el mástil con la bandera sale del caballete. El general se gira hacia el jugador local y le hace el saludo
militar.

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
