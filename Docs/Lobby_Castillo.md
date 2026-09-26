# Lobby: el castillo de arena y el general

El lobby de Tortunavy (`LVL_Lobby` / `LVL_HQ`) es un castillo de arena: murallas con almenas alrededor de todo el
recinto, una puerta enorme que da a la sala de espera con los huevos de los que se sale al empezar la partida, la
tienda a la izquierda, el general y los probadores a la derecha y un mini parkour para practicar mientras llegan los
compañeros. Todo se construye en código en cada máquina; solo se replica el estado de la puerta y de los huevos.

Coordenadas en cm. El jugador aparece en el origen mirando a +Y (hacia la sala de espera): **su izquierda es +X**.

## `ATN_SandCastleLobby` (`Lobby/TN_SandCastleLobby.*`)

Lo coloca `ATN_HQGameMode::SpawnLobbyShops` en el origen, a ras del suelo, si el nivel tiene la maqueta
(`ATN_LobbyReadyZone` o vallas y torres `BP_Fence*` / `BP_Tower*`) y todavía no hay un castillo. `TN.Lobby.Castle 0`
lo desactiva (hay que volver a cargar el lobby).

- **Patio**: suelo de arena con manchas de arena mojada (x ±2500, y de -2500 a 3300, cota 2), murallas de 5,6 m con
  almenas (x ±2400, y de -2400 a 2150), marcas de cubo y conchas incrustadas, diez torres de cubo con bandera y
  barreras invisibles por fuera de los adarves.
- **Puerta grande** (y = 2150, 10 m de ancho y 4,6 m de alto): dos hojas de madera con bisagra. Se abre cuando alguien
  está a menos de 10 m y durante la cuenta atrás, y se queda abierta 2,5 s después. Bloquea solo cerrada.
- **Sala de espera** (x ±1600, y de 2150 a 3150): ocho huevos en (±525 y ±175; 2450 y 2800). Meterse en uno (a menos
  de 95 cm de su centro) baja su tapa y marca al jugador como listo (`ATN_HQGameMode::SetPlayerReadyState`); con todos
  dentro empieza la cuenta atrás. Salir del huevo lo quita de listo. La zona de listos vieja (`ATN_LobbyReadyZone`)
  se apaga. Los selectores de modo siguen en (-950; 2400 y 2840).
- **Puerta del mar** (y = 3150): se abre con la cuenta atrás y deja ver el mar; su barrera bloquea siempre (fuera no
  hay suelo).
- **Mini parkour**:
  1. Escalera de arena de seis bloques (90 cm por peldaño) en la esquina suroeste, hasta el adarve sur.
  2. Circuito de saltos al sur (y = -1650): escalón, plataforma A (-900), B (-400; salto de 1,8 m) y C (250; salto de
     3,3 m con panzazo), y una rampa de concha de colores que baja de C a la arena.
  3. Seis pilares de cubo de playa en x = 2080 (de 90 a 490 cm, 80 cm más cada uno), una pasarela de palos de polo
     que sube hasta un torreón de 5 m en (2080, 450) y de ahí al adarve este.
- **Adornos**: conchas y estrellas de mar por la arena, cubo y pala junto a la puerta, el mar al fondo y carteles
  «SALA DE ESPERA», «¡A LA PLAYA!» y «TORTUNAVY».
- **Maqueta escondida** (en cada máquina): las paredes «Extrude» (solo si son altas, para no esconder nunca un suelo),
  las vallas y torres de la zona de salida con sus puertas y los huevos «Capsule*» del centro.
- La malla con colisión se cocina en el acto (`bUseAsyncCooking = false`) para que nadie atraviese el suelo al
  aparecer.

## El general del cuartel (`ATN_GeneralBriefing`, `Lobby/TN_GeneralBriefing.*`)

El General Galápago (gorra de capitán, caparazón de musgo y cuerpo verde bosque, a escala 3,4) detrás de una mesa de
madera con una maqueta del castillo hecha en código (murallas, torres, puerta abierta, huevos, tienda, botellas,
parkour y banderitas), puntero, taza, cartel «CUARTEL GENERAL» y mástil con bandera. Se gira hacia el jugador local y
le hace el saludo militar.

- Colocación: `SpawnLobbyShops` lo pone sobre el general de la maqueta (la tortuga cerca de (-892, 1479)) o sobre un
  actor con la etiqueta `TN_GeneralAnchor`, mirando al `PlayerStart`, y esconde esa tortuga y su mesa («Boolean»).
- Al hablar con él, `AMP_GamePlayerController::ClientOpenBriefing` abre `UTN_BriefingWidget` (`UI/Briefing/`), con
  el estilo de la tienda: el general habla letra a letra y hay cuatro pestañas, «CÓMO SE JUEGA», «MODOS DE JUEGO»,
  «REGLAS» y «CONTROLES» (esta con las teclas reales de Enhanced Input de las acciones
  `/Game/Blueprints/Gameplay/Controls/IA_*`, de teclado y de mando).
- Teclas: Q/E, flechas izquierda/derecha o 1-4 cambian de pestaña; arriba/abajo desplazan; Escape, Intro o
  «¡Entendido!» cierran.
