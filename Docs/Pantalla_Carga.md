# Pantalla de carga del huevo

En Tortunavy la pantalla de carga es un huevo que ocupa la pantalla entera: sus dos mitades de cáscara entran desde
arriba y desde abajo, se estampan con un «¡clac!» y lo tapan todo. Mientras se carga, encima de la cáscara van el nombre
del juego, el estado con puntos animados, un consejo y cuatro tortugas andando. Cuando el mapa está listo, el huevo
tiembla, se agrieta desde la línea de unión y revienta con un «¡PUM!»: las mitades salen despedidas y dejan ver la
partida. Todo es código: arte pintado en ejecución, Slate y sonido sintetizado.

## Piezas

| Pieza | Archivo | Qué hace |
|---|---|---|
| `UTN_LoadingScreenSubsystem` | `UI/Loading/TN_LoadingScreenSubsystem.*` | `UGameInstanceSubsystem` + `FTickableGameObject`: decide cuándo se cierra, cuándo espera, cuándo se rompe y cuándo se abre sin romperse. |
| `STN_EggLoadingScreen` | `Private/UI/Loading/STN_EggLoadingScreen.*` | El huevo a pantalla completa, los rótulos, las tortugas, las grietas y el «¡PUM!» (un `SLeafWidget` que lo pinta todo en `OnPaint`). |
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
- **Rotura** (1,9 s): tiembla cada vez más; diez grietas (con alguna rama) salen de la unión hacia arriba o hacia abajo
  en orden barajado, cada una con su crujido; al final se abre una rendija por la que sale luz; a 1,05 s, «¡PUM!» con
  fogonazo, estrella y 24 trozos de cáscara, y las mitades salen despedidas girando hacia fuera de la pantalla.
- **Abrir sin romperse**: las mitades se retiran por donde vinieron con un «fiuu» suave (cuenta atrás cancelada,
  sesión fallida...).

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
- Tope: 45 s después de cargar se rompe igual. Nunca se rompe a medio cerrar.

### Que no se vea nada durante el viaje

- **LoadMap fuera del editor**: MoviePlayer pinta en su hilo una copia del mismo huevo cerrado (misma semilla, mismo
  origen de tiempos: la misma línea de unión y las tortugas donde iban). Al acabar, el del viewport sigue igual.
- **LoadMap en PIE** (sin MoviePlayer): en `PreLoadMap` el huevo se cierra en seco y se pinta un fotograma de Slate a
  mano (`FSlateApplication::Tick`, como la pantalla de carga de Lyra): el viewport se congela con el huevo cerrado.
- **Viaje sin cortes**: el hilo de juego sigue vivo y el huevo se anima todo el rato; en el lobby ya estaba cerrado
  desde la cuenta atrás.
- El huevo va en el viewport con orden 20000 (por encima del HUD y de los menús) y bloquea los clics a lo de debajo.

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

Necesita el módulo `MoviePlayer` en `Tortunabo.Build.cs`.
