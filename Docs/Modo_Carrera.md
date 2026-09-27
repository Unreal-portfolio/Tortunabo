# Modo carrera en la playa

Rama `claude/modo-carrera` (sale de `claude/elegant-fermi-6n1hxy` en `5153406b`): se puede descartar sin tocar el
cooperativo. El cooperativo (lobby del castillo y mapa procedural) sigue igual.

## Bucle de juego

1. **Menú principal**: en vez de solo «Crear partida», se elige el modo: **Cooperativo** (lo de siempre: lobby del
   castillo y mapa procedural) o **Carrera**. El modo va en `UMP_GameInstance::SelectedProcMode` (`Coop` o `Race`).
2. **Lobby**: el mismo (`LVL_Lobby`, pantalla de carga, huevos y puerta doble). Al ponerse todos listos, el lobby viaja
   según el modo: Cooperativo → `LVL_ProcMap`; Carrera → `LVL_BeachRace`.
3. **Carrera** (`ATN_BeachRaceGameMode`, `ATN_BeachRaceGameState`): todos contra todos en la playa. Gana la ronda quien
   primero toca el agua tras saltar el acantilado de la orilla y se lleva una **concha**
   (`ATN_CoopPlayerState::RoundWins`). En carrera **no se muere**: lo que en el cooperativo mata, aquí aturde (bola de
   caparazón temblando unos segundos).
4. **Recuento** tras cada ronda (`ETNBeachRacePhase::RoundResults`): las caras de los jugadores, cada una con tres
   conchas en zigzag (como en los juegos de preguntas), y se le pone la suya al ganador. Luego, otra ronda con los
   elementos recolocados (el terreno es siempre el mismo).
5. **Campeón** (`ETNBeachRacePhase::Champion`) al llegar alguien a tres conchas: menú distinto con **Volver a jugar**,
   **Cambiar de modo** y **Salir** a la izquierda y, a la derecha, un fondo animado: las tortugas en un podio hecho de
   basura de la playa. La primera levanta la concha como un trofeo con las dos manos; la segunda, decepcionada; la
   tercera, sentada en el podio, enfadadísima y pataleando. Movimientos cortos en bucle, como un GIF.

## El mapa (`LVL_BeachRace`, `ATN_BeachRaceGenerator`)

- **Escala**: la tortuga es una cría de ~5 cm; todo va a `TNBeach::Scale` = 28 veces su tamaño real.
- **Terreno fijo** (siempre el mismo): un único tramo recto de playa de `TNBeach::CourseLength` = 1200 m por
  `TNBeach::CourseWidth` = 280 m jugables, calculado para ~5 min: andando a 4,5 m/s y esprintando a 8 m/s (13 s por
  barra de energía), con un 40 % de esprint y los obstáculos, sale una media de ~4 m/s. Toda de arena, con un leve
  desnivel hacia el mar (siempre se ve la meta), dunas suaves y la orilla en acantilado: se salta y se gana al tocar el
  agua, donde flotan las banderas de meta.
- **Bordes**: a los lados y detrás, selva de palmeras y árboles enormes (a escala: una palmera de 10 m mide 280 m).
  Salida por una pasarela de madera vieja que baja a la playa.
- **Reparto procedural** (cada ronda, con semilla): decorado, trampas y enemigos de `ETNBeachElement`
  (`Public/World/Beach/TN_BeachTypes.h`), cada uno con su huella, sin solaparse y dejando siempre paso.
- **Decorado gigante** (`ATN_BeachDecor`): cocos, medusas varadas, anillas de latas cortadas, un sujetador rojo,
  almejas, conchas y estrellas de adorno, rocas, restos de una vela de barco, troncos con musgo, tablones viejos, redes
  de pesca, vasos, botellas, chupachups, cortezas de sandía roídas, pajitas, sombrillas clavadas y sillas de playa del
  día anterior, castillos de arena pequeños y enormes, madera a la deriva.
- **Trampas e interacciones**: alambre de espino, algas que enredan, plataformas sobre hoyos que se tambalean y se
  rompen con más de una tortuga encima, cubos rotos por los que se pasa, palas que hacen de trampolín o de puente con un
  compañero, un castillo de arena enorme con salas por dentro que hay que atravesar, puertas de conchas.
- **Enemigos y amenazas**: cangrejos gigantes que patrullan y persiguen y dan un mazazo con la pinza (te dejan en bola,
  aturdida); erizos de mar grandes que ruedan despacio hacia ti (pinchan: aturden); lagartos que se esconden; quads
  enormes que cruzan la playa de lado a lado entre palmeras (temblor de pantalla, nube de humo, ruedas anchísimas: o
  fuera de su paso o en el hueco entre las ruedas); gaviotas y pelícanos que rondan por arriba (te cagan encima y
  aturden, bajan en picado con su sombra creciendo: esquiva con el panzazo; o te cogen con el pico, te suben y te
  sueltan: aturdida en bola al caer); la tormenta de bañistas por detrás, con sombrillas, cubos y sillas de playa
  volando.

## Reparto del trabajo (agentes)

| Parte | Archivos |
|---|---|
| Contrato común | `Public/World/Beach/TN_BeachTypes.h`, `TN_BeachElement.*`, `Game/TN_BeachRaceGameState.*`, este documento |
| Menú, flujo y reglas | `UI/Menu/MP_MainMenuWidget.*`, `Lobby/TN_HQGameMode.*` (viaje), `Game/TN_BeachRaceGameMode.*`, aturdir en vez de morir |
| Terreno, nivel y reparto | `World/Beach/TN_BeachRaceGenerator.*`, `TN_BeachLayout.h`, `Scripts/build_beach_race.py` |
| Decorado gigante | `World/Beach/TN_BeachDecor.*`, `TN_BeachPropMeshes.h` |
| Trampas e interacciones | `World/Beach/TN_BeachBarbedWire.*`, `Seaweed`, `WobblyPlatform`, `BrokenBucket`, `SpadeRamp`, `SandDungeon`, `ShellGate` |
| Enemigos y tormenta | `World/Beach/TN_BeachGiantCrab.*`, `SeaUrchin`, `Lizard`, `QuadLane`, `GullZone`, `TN_BeachStorm.*` |
| Recuento, campeón y podio | `UI/Race/*`, poses de celebración en `Player/TN_TurtleAnimInstance.*`, escena del podio |
