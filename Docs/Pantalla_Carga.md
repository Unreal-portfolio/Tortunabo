# Pantalla de carga del huevo

En Tortunavy la pantalla de carga es un huevo que ocupa la pantalla entera: sus dos mitades de cáscara entran desde
arriba y desde abajo, se estampan con un «¡clac!» y lo tapan todo. Mientras se carga, encima de la cáscara van el nombre
del juego, el estado con puntos animados, un consejo y cuatro tortugas andando. Cuando el mapa está listo, el huevo
tiembla, se agrieta desde la línea de unión y revienta con un «¡PUM!»: las mitades salen despedidas y dejan ver la
partida. En el mapa procedural espera además a que empiece la ronda y revienta con un «¡ADELANTE!» enorme y una frase
de ánimo; en las rondas siguientes (sin huevo) sale el mismo rótulo solo. Todo es código: arte pintado en ejecución,
Slate y sonido sintetizado.

## Piezas

| Pieza | Archivo | Qué hace |
|---|---|---|
| `UTN_LoadingScreenSubsystem` | `UI/Loading/TN_LoadingScreenSubsystem.*` | `UGameInstanceSubsystem` + `FTickableGameObject`: decide cuándo se cierra, cuándo espera, cuándo se rompe y cuándo se abre sin romperse. |
| `STN_EggLoadingScreen` | `Private/UI/Loading/STN_EggLoadingScreen.*` | El huevo a pantalla completa, los rótulos, las tortugas, las grietas y el «¡PUM!» o el «¡ADELANTE!» (un `SLeafWidget` que lo pinta todo en `OnPaint`). |
| `FTNGoBannerPainter` | `Private/UI/Loading/STN_EggLoadingScreen.*` | Pinta el rótulo «¡ADELANTE!» con su frase; lo usan el huevo y `STN_GoBanner`. |
| `STN_GoBanner` | `Private/UI/Loading/STN_EggLoadingScreen.*` | «¡ADELANTE!» solo, sin huevo, encima del juego (rondas siguientes del mapa procedural); no recibe clics y se quita solo. |
| `UTN_EggSynthComponent` | `UI/Loading/TN_LoadingScreenSubsystem.*` | «¡Clac!» del cierre, crujidos, «¡pum!» y «fiuu» de las mitades, sintetizados en 2D. |
| `UMP_GameInstance` | `Multiplayer/MP_GameInstance.cpp` | `ShowLoadingScreen` cierra el huevo; `HideLoadingScreen` lo abre si el viaje no llegó a empezar; Host y Join esperan a que esté cerrado para viajar. |

## Cómo se ve

- **La cáscara es la pantalla.** Cada mitad es una malla propia (`FSlateDrawElement::MakeCustomVerts`) que va del borde
  de fuera (más allá de la pantalla) hasta la línea de unión siguiendo sus dientes. La superficie (crema con un moteado
  suave, motas turquesa, coral, doradas y lila y pintitas) es una textura de 2,4:1 ajustada «cubriendo» con un 20 % de
  zoom: las motas nunca se deforman y la cáscara llega a los bordes en 16:9, 16:10, 21:9, 4:3 o 32:9.
- **Volumen**: sombreado por vértice (la luz viene de arriba a la izquierda; hacia los bordes se oscurece y se calienta),
  una sombra suave a los dos lados de la unión, una media luna de brillo arriba a la izquierda y el filo de la cáscara
  (su grosor, crema claro con línea de tinta) que se ve al entrar y al salir las mitades.
- **Línea de unión**: zigzag irregular (dientes de 2,6-5 % del alto y de altura distinta) sobre una onda suave. Sale
  de la semilla del huevo, así que cada carga es un poco distinta. La tinta es un trazo propio con esquinas a inglete y
  un píxel de borde suave (los vértices de Slate no tienen antialias).
- **Encima de la cáscara**: «Tortunavy» dorado con contorno y sombra (mitad de arriba); el estado, el consejo y las
  cuatro tortugas andando (mitad de abajo). Van con su mitad: al romperse salen volando con ella. Los rótulos se miden
  con la pantalla (alto de 1080 de referencia) y un consejo largo se encoge para caber en 4:3.
- **Cierre**: las mitades aceleran hasta estamparse (0,5 s), sacudida que se apaga, polvo a lo largo de la unión y
  «¡clac!». Cada 4,3 s da un temblorcito, como si algo se moviera dentro.
- **Rotura** (1,9 s; con «¡ADELANTE!», 4,5 s): tiembla cada vez más; diez grietas (con alguna rama) salen de la unión
  hacia arriba o hacia abajo en orden barajado, cada una con su crujido; al final se abre una rendija por la que sale
  luz; a 1,05 s, «¡PUM!» con fogonazo, estrella y 24 trozos de cáscara, y las mitades salen despedidas girando hacia
  fuera de la pantalla.
- **Abrir sin romperse**: las mitades se retiran por donde vinieron con un «fiuu» suave (cuenta atrás cancelada,
  sesión fallida...).
- **«¡ADELANTE!»** (salida de la ronda del mapa procedural): en lugar de la estrella y el «¡PUM!» sale la palabra
  enorme (el 86 % del ancho de la pantalla; en pantallas muy anchas, como mucho el 36 % del alto) con una frase de
  ánimo debajo. Ver [Salida de la ronda](#salida-de-la-ronda-en-el-mapa-procedural-adelante).

## Cuándo se cierra y cuándo se abre

| Momento | Qué pasa | Señal |
|---|---|---|
| **Host** | Se cierra en el acto al pulsar; `ServerTravel` sale cuando está cerrado del todo (con el subsistema NULL la sesión se crea en el mismo fotograma). Se rompe cuando el lobby está listo. | `HostSession` → `ShowLoadingScreen` → `CloseForTravel`; `OnCreateSessionComplete` → `RunWhenClosed` |
| **Buscar y unirse** | Igual: se cierra al buscar, se conecta cuando está cerrado y se rompe en el lobby. Si no hay partidas o falla, se abre. | `FindAndJoinSession`, `OnJoinSessionComplete` → `RunWhenClosed`; fallo → `HideLoadingScreen` → `CancelPendingClose` |
| **Todos listos en el lobby** | Se cierra en todas las pantallas («¡Todos listos! Salimos en 3», «Preparando la expedición») y no se abre hasta que el mapa de la partida esté cargado y con su terreno. | `ATN_CoopGameState::CountdownValue > 0` o `MatchFlowState` en `Countdown`/`Cinematic`, en un mundo del lobby (`GameModeClass` hijo de `ATN_HQGameMode`, o mapa con «HQ» o «Lobby») |
| **Cuenta atrás cancelada** | Si alguien sale de su huevo (0,25 s sin cuenta atrás) se abre sin romperse. | `CountdownValue == 0` y `MatchFlowState` en `WaitingForPlayers` |
| **Resultados → cuartel** | En el último segundo de los resultados se cierra y viaja cerrado. | `MatchFlowState == Results` y `CountdownValue == 1` en un mapa de partida |
| **Cualquier LoadMap** | Cerrado en seco (sin animación) antes de que el LoadMap congele nada. | `FCoreUObjectDelegates::PreLoadMapWithContext` |
| **Viaje sin cortes** | Se cierra si no lo estaba ya. | `FWorldDelegates::OnSeamlessTravelStart` |

Mientras está cerrado esperando un viaje que no ha empezado no se rompe nunca; si el viaje no llega se abre solo
(45 s para Host y Join, 60 s en el lobby, 20 s en los resultados) y lo avisa en el registro (`[Carga]`).

### Mapa listo (cuándo se rompe)

- La partida ha empezado (`HasBegunPlay`), han pasado 0,6 s desde la carga (el primer fotograma del mapa ya pintado),
  no quedan paquetes cargando en segundo plano (como mucho 6 s) y hay tortuga propia (como mucho 6 s).
- Mapa procedural: todos los `ATN_ProcMapGenerator` tienen construida en esta máquina la generación que ha pedido el
  servidor (`IsMapReady()` y `GetBuiltGeneration() == GetRequestedGeneration() > 0`). Sin generador todavía (réplica
  en camino) espera hasta 12 s.
- Tope: 45 s después de cargar el mapa se da por listo igual. Nunca se rompe a medio cerrar.
- Con el mapa listo, en los mapas que no son de rondas (lobby, menú, `LVL_Run`, `LVL_ProcMap_Terrain`...) se rompe en
  el acto con el «¡PUM!» de siempre. En el mapa procedural espera a la ronda (sección siguiente).

## Salida de la ronda en el mapa procedural («¡ADELANTE!»)

El servidor no arranca la ronda hasta que todos los clientes tienen el mapa construido (mínimo 2 s y máximo 30 s tras
generarlo, `ATN_ProcMapGameMode::PollMapReady`); si el huevo se rompiera con el mapa listo, se vería la salida antes de
tiempo. Por eso, en un mapa de rondas el huevo no se rompe hasta que la ronda está en juego.

- **Mapa de rondas**: el GameState es `ATN_ProcMapGameState` (o uno `ATN_CoopGameState` cuyo `GameModeClass` hereda de
  `ATN_ProcMapGameMode`). Si el GameState aún no ha llegado y el mapa se llama `*ProcMap*`, también espera. El mapa de
  solo terreno (`ATN_TerrainViewGameMode`) no es de rondas.
- **Ronda en juego**: `MatchFlowState == ETNMatchFlowState::InProgress` o `bRoundInProgress` (basta con que haya
  llegado cualquiera de las dos réplicas). Si la partida ya está en `Results` (alguien que llega al final), se rompe
  con el «¡PUM!» y se ven los resultados.
- **Mientras espera**, el estado dice «Preparando la salida» o, con más de un jugador conectado (`ConnectedPlayers`),
  «Esperando a las demás tortugas».
- **Tope**: 40 s desde que el mapa quedó listo en esta máquina (`TNLoadingTimes::MaxRoundWaitSeconds`); entonces se
  rompe con el «¡PUM!» y avisa en el registro (`[Carga]`). Si la ronda empieza después, sale el rótulo solo.
- **Rondas siguientes**: el mapa se regenera sin viajar y no hay huevo. `UpdateRoundWatch` mira en cada fotograma si la
  ronda pasa a estar en juego y, si no hay un huevo cerrado esperándola (que ya se rompería con su «¡ADELANTE!»), pone
  `STN_GoBanner` en el viewport (orden 19990, justo debajo del huevo; no recibe clics). La ronda que ya estaba en juego
  al llegar a un mundo nuevo no cuenta como salida: de esa se encarga el huevo. Así la ronda 1 nunca sale dos veces.

### Línea de tiempo

Tiempos desde que el servidor pone la ronda en juego (`BeginRoundPlay`); en los clientes, desde que llega la réplica.

| Momento | Con huevo (primera ronda, o quien llega a una ronda en juego) | Sin huevo (rondas siguientes) |
|---|---|---|
| 0 s | El huevo empieza a romperse: tiembla y se agrieta. Las letras del rótulo se rasterizan casi transparentes (alfa 1/255) debajo de la cáscara. | Entra `STN_GoBanner`, aún sin nada a la vista (también rasteriza sus letras). |
| 1,05 s | «¡pum!», fogonazo y trozos de cáscara; las mitades salen despedidas y entra «¡ADELANTE!» con rebote (0,32 s, ≈ 20 % de más). La frase entra 0,12 s después. | Entra «¡ADELANTE!» con el mismo rebote, con «¡pum!» y «fiuu». |
| 1,2 s | El servidor abre la puerta doble o los huevos de la salida del mapa (`ATN_ProcMapGameMode`, trabajo de otro ingeniero). | Igual. |
| 1,9 s | Las mitades ya han salido: solo queda el rótulo y el huevo deja pasar los clics (`HitTestInvisible`). | — |
| 4,1 s | El rótulo empieza a desvanecerse (0,4 s, creciendo un 10 %). | 3,65 s: empieza a desvanecerse. |
| 4,5 s | Acaba la rotura (`IsBreakFinished`) y `Hide` quita el huevo. | 4,05 s: se quita solo. |

Constantes: `FTNEggTimeline::PopAt` (1,05 s), `BreakEnd` (1,9 s), `GoLingerSeconds` (2,2 s) y `GoBreakEnd` (4,5 s);
`FTNGoBannerPainter::PopSeconds` (0,32 s), `FadeSeconds` (0,4 s) y `OverlayHoldSeconds` (2,6 s). Sin huevo, el
rótulo espera `PopAt` para salir a la vez que con huevo, justo antes de que se abra la salida.

### Cómo se pinta el rótulo

- **Palabra**: Roboto Bold de 220 puntos a 1080 de alto, con una escala de maquetación que solo depende del tamaño de
  la pantalla (86 % del ancho, como mucho 36 % del alto). El rebote, la respiración (±1,2 %) y el crecimiento final van
  en la transformación de render: las letras se rasterizan una sola vez a su tamaño final y el atlas de fuentes no
  recibe un tamaño nuevo en cada fotograma.
- **Capas**: sombra suave (azul marino al 30 %), relieve naranja (cinco copias escalonadas hacia abajo con el contorno
  de tinta de 12 unidades: todos los contornos en una capa y todos los rellenos en la de encima, así el contorno rodea
  el conjunto), una línea de sombra tostada bajo la cara y la cara en cuatro franjas recortadas con `PushClip`
  (naranja dorado abajo, dos dorados y amarillo pálido arriba), que dan el degradado. Todas usan el mismo contorno y
  solo cambian su color, que no cuenta para la caché de letras.
- **Frase**: una de ocho, al azar y nunca la misma dos veces seguidas, en crema con contorno de tinta y sombra; a 1080
  de alto mide 50 puntos y se encoge si no cabe en el 90 % del ancho.
- **Desvanecido**: Slate no aplica el tinte del texto al contorno, así que su alfa se baja a mano en cada capa (también
  en el «¡PUM!»).
- **Rasterizado previo**: letras de este tamaño pueden abrir una página nueva del atlas de fuentes y provocar un vaciado
  de la caché; `FTNGoBannerPainter::Warm` las pinta antes, casi transparentes, para que ese tirón caiga debajo de la
  cáscara (o antes de que salga el rótulo) y no en el momento del «¡ADELANTE!».

### Que no se vea nada durante el viaje

- **LoadMap fuera del editor**: MoviePlayer pinta en su hilo una copia del mismo huevo cerrado (misma semilla, mismo
  origen de tiempos: la misma línea de unión y las tortugas donde iban). Al acabar, el del viewport sigue igual.
- **LoadMap en PIE** (sin MoviePlayer): en `PreLoadMap` el huevo se cierra en seco y se pinta un fotograma de Slate a
  mano (`FSlateApplication::Tick`, como la pantalla de carga de Lyra): el viewport se congela con el huevo cerrado.
- **Viaje sin cortes**: el hilo de juego sigue vivo y el huevo se anima todo el rato; en el lobby ya estaba cerrado
  desde la cuenta atrás.
- El huevo va en el viewport con orden 20000 (por encima del HUD y de los menús) y bloquea los clics a lo de debajo,
  salvo cuando ya solo queda el «¡ADELANTE!» a la vista.

## Textos

`UMP_GameInstance` pasa sus mensajes («Creando lobby...», «Conectando a la partida...») y el huevo quita los puntos
suspensivos (los anima él). `FriendlyStatusForMap` da el texto de cada mapa («Rumbo al cuartel», «Incubando la
partida», «Volviendo al menú»); en la primera carga del juego pone «Cargando».

## Arte

Se genera una vez por ejecución al crearse el subsistema (unas décimas de segundo) y queda en la raíz del recolector
(`TNHUDArt::Cached`, claves `EggShell_*`): superficie de 2048 × 854 (pasa a sRGB con tabla), blanco liso, brillo, punto
blando, tres trozos de cáscara, la estrella y las ocho tortugas (cuatro colores, dos fotogramas). Con Live Coding las
cachés no se regeneran: si cambia el arte, cambiar la clave o reiniciar el editor.

## Pruebas por consola

- `TN.Loading.Test`: cierra el huevo y lo rompe a los 3 s.
- `TN.Loading.Test.Close`: lo cierra como al pulsar Host y lo deja cerrado (`TN.Loading.Test.Hold` hace lo mismo).
- `TN.Loading.Test.Break`: rompe el que esté a la vista.
- `TN.Loading.Test.Open`: lo abre sin romperlo (como al cancelarse la cuenta atrás).
- `TN.Loading.Test.Go`: cierra el huevo y a los 2 s lo rompe con «¡ADELANTE!» y una frase (la salida de la ronda del
  mapa procedural entera, sin mirar el mapa ni la ronda).
- `TN.Loading.Test.GoOnly`: enseña en el acto «¡ADELANTE!» solo, sin huevo, como en las rondas siguientes (en el juego
  sale 1,05 s después de que empiece la ronda).

Necesita el módulo `MoviePlayer` en `Tortunabo.Build.cs`.
