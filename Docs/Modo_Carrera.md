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
  desnivel hacia el mar (siempre se ve la meta) y dunas suaves.
- **Meta**: un acantilado de rocas al final de la playa, de unos 55 cm reales (`TNBeach::CliffHeight` = 15,5 m en el
  juego, 5-6 veces la tortuga): se salta desde el borde y se cae al agua, donde flotan las banderas de meta; se gana al
  tocar el agua. El último salto, desde la zona del borde, se hace de cabeza (zambullida).
- **Bordes**: a los lados y detrás, selva de palmeras y árboles enormes (a escala: una palmera de 10 m mide 280 m).
- **Pasarelas**: la pasarela de madera vieja y el caminito de palos con cuerda son elementos del reparto procedural
  (`Boardwalk`, `WoodenPostPath`), repartidos por la playa y a veces como guía visual hacia el mar; no marcan la
  salida ni la meta.
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

## Terreno, nivel y reparto (`ATN_BeachRaceGenerator`, `TN_BeachLayout.h`)

Archivos: `Public/World/Beach/TN_BeachLayout.h` (lógica pura: terreno fijo, salida, meta y reparto, con sus tests en
`Private/Tests/TN_BeachLayoutTest.cpp`), `Public/World/Beach/TN_BeachRaceGenerator.h` y, en `Private/World/Beach/`,
`TN_BeachRaceGenerator.cpp` (rondas, consultas y meta), `_Build.cpp` (arena, roca, mar, muros y agua), `_Scenery.cpp`
(salida, meta, selva, huellas y chapuzón) y `TN_BeachRaceKit.h`; el nivel, `Scripts/build_beach_race.py`.

### Medidas (espacio del generador en cm: X hacia el mar, Y a lo ancho, el agua en Z = 0)

- **Recorrido**: línea de salida en X = 0 y filo del acantilado en X = 1200 m (ondula ±2,5 m a lo ancho). Playa jugable
  `|Y| <= 140 m`. Muros invisibles a 148 m a cada lado (también en el agua), detrás de la salida (X = -32 m) y mar
  adentro (X = 1500 m), de -200 a +1200 m de alto.
- **Arena**: 51,5 m sobre el agua en la salida y 15,5 m en el filo: cae 36 m como `(1 - t)^1,5` (4,5 % al principio,
  casi llana al final), así siempre se ve el mar por encima del filo (y las banderas de meta) desde la salida. Dunas:
  crestas a lo ancho cada ~52 m, deformadas, de 1,7 m en el centro a 3,5 m junto a la selva; nada en los primeros 30 m
  y se allanan antes de la roca (pendiente máxima ~15°). Rejilla de 3 m en la playa (hasta 18 m lejos) en 55 teselas
  de 40 x 40 casillas; con colisión, las que quedan a tiro. `M_ProcTerrain` con relieve y, en la arena (alfa 1), grano,
  guijarros y marcas del viento; tierra y hojarasca en los bancos.
- **Bancos de la selva**: suben 38 m en los 110 m de fuera de la playa y 45 m más hasta 460 m, con colinas; detrás de la
  salida, 42 m y luego 30 m más.
- **Acantilado de roca**: repisa de caras planas en los últimos 24 m (enterrada al empezar, asoma 50 cm sobre la arena
  desde ~16 m antes del filo), filo limpio a 15,5-16 m del agua y pared casi vertical, socavada hasta 2,8 m (nunca
  sobresale del filo: se cae al agua); banda mojada oscura y algas bajo el agua; peñascos al pie de los cabos (fuera de
  donde se cae). En los bancos el acantilado sigue su altura (cabos de 40-80 m).
- **Agua de meta**: 11 m de hondo al pie (18 m a 250 m, 35 m lejos). Nadable (`ATN_ProcWaterVolume`, local en cada
  máquina) de debajo de la repisa a 300 m mar adentro y 250 m a cada lado; superficie con `MI_ProcSeaAnim` (hondura y
  espuma a escala: `DepthRange` 1500, `FoamWidth` 160) hasta el horizonte (6 km).
- **Salida**: en el linde de la selva, entre las raíces de una ceiba colosal (~300 m; tronco de 26 m de radio en la
  base, 52 m detrás de la línea). Sus dos raíces tabulares enmarcan la salida (30 m de alto junto al tronco, 2-3 m en la
  línea de salida, a ±34 m); seis plantas de hojas enormes (hojas de 42-60 m) la techan. Cartel «¡A LA META!» (por
  detrás, «TORTUNAVY») entre dos palos de madera a la deriva, a 8,5-14 m sobre la arena en la línea. Cuatro sitios en
  X = -8 m, Y = -15, -5, 5 y 15 m (más filas cada 9 m por detrás), 110 cm sobre el suelo y mirando al mar.
- **Meta**: boyas con banderas a cuadros de 12 m en mástiles de 28 m, cada 40 m y a 26 m del filo, unidas por un cabo
  con boyas pequeñas (se mecen); el arco de neumático de la meta del mapa procedural cinco veces más grande (125 m de
  luz, 63 m sobre el agua) a 40 m del filo, con TORTUNAVY hacia la playa y TORTUNABO hacia el mar, banderines hasta dos
  mástiles en los cabos y banderolas por la ladera de la selva en los últimos 250 m. Zambullida
  (`IsCliffJumpZone`): los últimos 7,5 m antes del filo y 40 m sobre el vacío.
- **Selva**: vegetación del mapa procedural a escala: palmeras de 230-300 m (x26-34), árboles de copa, ceibas de
  250-340 m, casuarinas, pándanos y peñascos en rejilla de 48 m (espesa en los primeros 160 m, más clara hasta 460 m);
  una primera fila de palmeras cada 28-42 m inclinadas sobre la arena; sotobosque de plataneras, palmitos, helechos
  arbóreos y uvas de playa de 50-100 m en los primeros 100 m. ~400 árboles y ~500 plantas en ~30 HISM, sin colisión
  (detrás de los muros) y con viento solo a menos de 200 m de la cámara.

### Reparto por ronda (`TNBeachLayout::GenerateRound`, determinista con la semilla)

1. **Castillo con salas** (uno) entre el 42 y el 58 % del recorrido, a ±39 m del centro, con dos alas en embudo hacia
   su entrada (barren 18 cm hacia la salida por metro) de decorado grande (7-26 m de huella, los mayores más a menudo) y
   alambre de espino entre pieza y pieza hasta la selva: o se atraviesa o se rodea por un único hueco de 18 m (con
   algas) junto a la selva de un lado, al azar.
2. **Pasos de quads**: 2 o 3 franjas que cruzan la playa entera (`Extent` = 28000, Yaw 90°), entre el 15 y el 92 % y a
   170 m como poco entre ellas; nada se pone encima.
3. **Zonas de gaviotas**: 2-4 (radio 27-39 m) entre el 22 y el 97 %, a ±77 m del centro; van por encima (no ocupan
   suelo, solo no se pisan entre ellas).
4. **Pasarela guía** (el 60 % de las rondas): una hilera de pasarelas y caminitos de palos de 150-350 m hacia el mar
   (tramos de 30-70 m, ±22°) que rodea lo que haya.
5. **Bandas de 50 m** desde 50 m por delante de la salida hasta 30 m antes del filo: huellas hasta cubrir del 5 %
   (salida) al 19 % (mar) de cada banda, 16 como mucho. Categoría: decorado 62→40 %, trampas 22→32 %, enemigos
   16→28 %. Dentro de cada una se recorren todos los valores de `ETNBeachElement` (`CategoryOf`, `FootprintRadius`: lo
   nuevo del contrato entra solo; el decorado pesa por su tamaño) con ajustes: cocos y lagartos junto a la selva;
   conchas, almejas, estrellas, medusas, madera, algas, cangrejos (desde el 30 %) y erizos (desde el 45 %) hacia el mar;
   vasos, botellas, sombrillas y sillas hacia el centro; un sujetador y una vela por ronda como mucho; lo pequeño, a
   veces en corrillos de 2-3.
6. **Paso libre**: lo que cierra el paso (decorado —salvo pasarelas y caminitos—, alambre y castillo; no las trampas que
   se pisan o se atraviesan ni los enemigos) se infla 6 m en una rejilla de 2 m, y tras cada paso se quita lo último
   puesto hasta que haya un camino de casillas libres de la salida al filo: en cada corte a lo ancho queda un hueco de
   12 m como poco.

Sin solapes (1,5 m entre huellas; las alas del castillo se tocan), nada a menos de 40 m de la salida (las huellas
empiezan 58 m por delante de las tortugas), a menos de 30 m del filo ni a menos de 5 m de la selva (salvo los quads).
**Orientación**: Yaw 0 = X local hacia el mar; los alargados, a lo largo de su X local (alambre y quads a ~90°,
pasarelas a ~0°); castillo, cubo, pala, puerta y plataforma a ~0° (se entra por -X). Con 40 semillas: 200-260
elementos por ronda (~150 de decorado, casi todo pequeño; ~55 trampas; ~20 enemigos, con 2-3 quads, 2-4 gaviotas y 4-6
plataformas), el doble de huella en el último tercio que en el primero; cangrejos y erizos, de media al 70 % del
recorrido. La densidad se ajusta en `TNBeachLayout::BandCoverage` y el tope de 16 por banda (`FillBands`).

**Asiento en la arena** (el «sello» de la ronda): el terreno es fijo y no se cava (los hoyos los traen los elementos:
la plataforma, su cráter). Cada ronda deja liso el suelo bajo la huella de cada elemento del suelo: a nivel, a la cota
natural de su centro, en los redondos; en los alargados, la cuesta de la playa sin dunas pasando por esa cota (los pasos
de quads, algo más oscuros: rodadas). Borde de 2,5-16 m hasta la arena natural (menos de 40°). Se rehacen solo las
teselas tocadas, en el servidor y en cada cliente con la semilla replicada; `GetGroundHeightAt` lo da sin trazas.

### Interfaz (`ATN_BeachRaceGenerator`)

- Servidor: `GenerateRound(int32 Seed)` (destruye la ronda anterior, reparte, asienta y crea cada elemento con
  `SpawnElement`, con `bAlwaysRelevant`: la playa mide 1,2 km; replica la semilla), `ClearRound()`,
  `ResetFinishWater()`, `OnTurtleReachedWater(ACharacter*)` / `OnTurtleReachedWaterNative` (una vez por tortuga y
  ronda, con los pies en el agua de meta).
- Cualquier máquina: `IsRoundReady()`, `GetRoundNumber()`, `GetRoundSeed()`, `GetStartTransform(int32)`,
  `GetNumStartSpots()`, `IsFinishWater(P)` (más allá del filo y a 30 cm del agua o menos: sirven los pies o el centro
  de la cápsula), `IsCliffJumpZone(P)`, `GetCliffEdgeDistance(P)` (negativa antes del filo), `GetCourseProgress(P)`
  (0-1), `GetGroundHeightAt(P)`, `GetSeaDirection()`, `static Find(WorldContext)`, `GetRoundLayout()`.
- Chapuzón en cada máquina al entrar una tortuga en el agua de meta (gotas, espuma y ondas).
- Si nadie reparte, el servidor reparte una ronda al azar a los 3 s (sin el GameMode de la carrera) o a los 20 s (con
  él). Consola: `TN.Beach.ShowFootprints 1` enseña las huellas del reparto en juego. Registro: `[Playa] terreno fijo:
  ...` al construir y `[Playa] ronda N: ...` (resumen y tiempo) en cada ronda.

### Nivel y capturas

- `Scripts/build_beach_race.py` (en el editor, con el C++ compilado) crea o abre `/Game/Maps/Run/LVL_BeachRace`: sol a
  la espalda de la salida, cielo, luz del cielo, niebla suave (desde 300 m), el generador «PlayaCarrera» en el origen,
  cuatro `PlayerStart` en las salidas y `TN_BeachRaceGameMode` en World Settings; lo guarda.
- Para ver el reparto sin jugar: en el generador, Details > Beach|Editor > **Preview Round** (con `Editor Seed` o al
  azar) y **Clear Preview**; no se guarda con el nivel. Las huellas: amarillo decorado, naranja trampas, rojo enemigos,
  morado quads, celeste gaviotas, marrón pasarela guía y rosa el castillo y sus alas, con una flecha hacia su X local.
- Qué capturar: con `vista(nombre)` del script (`BEACH_SKIP_MAIN = True` antes del `exec`): `salida` (a la altura de
  una tortuga: la playa, el filo y el mar con las banderas), `planta` (con Preview Round: el reparto entero), `castillo`,
  `acantilado`, `meta` (desde el agua) y `selva` (la orilla con las palmeras). En juego: un salto desde el filo
  (zambullida, chapuzón, «tocó el agua» en el registro) y el tiempo de `[Playa] terreno fijo` y `[Playa] ronda N`.
- Pruebas: `Automation RunTests Tortunabo.Beach` (terreno, salida, meta, zambullida, asientos, determinismo y reglas del
  reparto con 40 semillas).

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

## Flujo de la carrera: menú, lobby, rondas y aturdimiento

### Menú principal y lobby

- **Menú** (`UMP_MainMenuWidget`): los mismos tres botones del Blueprint (mismo estilo; solo cambian los textos) en dos
  pasos. «Crear partida» → «Cooperativo» / «Carrera» / «Volver». «Unirse» no pregunta nada: el modo lo decide el
  anfitrión. El modo va a `UMP_GameInstance::SelectedProcMode` (`HostSessionWithMode`) y sobrevive a los viajes.
- **Lobby** (`ATN_HQGameMode::BeginMatchTravel`): Carrera → `BeachRaceMapPath` (`/Game/Maps/Run/LVL_BeachRace`; si el
  nivel aún no existe, error en el log y se juega la carrera del mapa procedural). Cooperativo y 2vs2 →
  `LVL_ProcMap`; Clásico → `LVL_Run`. El castillo no tiene selector: el modo se elige en el menú o con «Cambiar de
  modo» al acabar la carrera (para probar, `TN.Mode Coop|Race` en el anfitrión). En el lobby viejo (`LVL_HQ`),
  «MODO: CARRERA» del selector también va ya a la playa.

### GameMode y fases

`ATN_BeachRaceGameMode` hereda de `ATN_RunGameMode`, no de `ATN_ProcMapGameMode`: el bucle de rondas de este es privado
y va atado a `ATN_ProcMapGenerator` (lo crearía si el nivel no lo tiene). De la base se reutiliza la espera a los
jugadores tras el viaje, la meta (puesto, puntos y espectador), la vuelta al lobby (`LobbyReturnMapPath`) y el flujo
replicado del que tiran el huevo de la pantalla de carga («¡ADELANTE!») y la música de fin de partida. El bucle de
rondas es el de la carrera del mapa procedural, adaptado. GameState: `ATN_BeachRaceGameState` (`ProcMode = Race`,
`RoundTarget` = `WinsToWinMatch` = 3).

| Fase (`RacePhase`) | `MatchFlowState` | Qué pasa | Tiempo |
|---|---|---|---|
| `Waiting` | `WaitingForPlayers` | `GenerateRound(semilla)`; tortugas nuevas en la salida (escalonada; los sitios rotan cada ronda), quietas | ≥ 2 s (`MinPreRoundSeconds`; como mucho 20 s esperando al generador) + cuenta atrás de 3 s (`CountdownValue` y `PhaseSecondsLeft`), salvo en la primera ronda tras el viaje, cuya cuenta es el huevo |
| `Racing` | `InProgress` | gana la ronda la primera que toca el agua de meta; tormenta en marcha | límite 9 min (`RoundTimeLimitSeconds`): gana la más cerca del mar |
| `RoundResults` | `Countdown` | recuento: `RoundWinner` y `RoundWins`; todas quietas | 7 s (`RoundResultsSeconds`) |
| `Champion` | `Results` | `Champion` y `Podium` (por conchas; a igualdad, quien ganó una ronda más tarde); se espera al anfitrión | sin límite |

- El recuento sale siempre, también el de la tercera concha; el campeón va después.
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
- **Desconexiones**: si alguien se va en plena carrera, la ronda sigue con las demás (gana la primera que toque el agua).
  Quien entra con la ronda en marcha sale desde la salida; entre rondas, se queda quieta. El podio conserva los
  PlayerState mientras existan (después llegan como null). Si se va el anfitrión, se acaba la partida (lo de siempre).

### Lo que el GameMode usa del generador y de la tormenta

- `ATN_BeachRaceGenerator`: `void GenerateRound(int32 Seed)`, `bool IsRoundReady() const`,
  `FTransform GetStartTransform(int32 PlayerIndex) const` (mirando hacia el mar; la cápsula se apoya en el suelo que
  haya debajo) y `bool IsFinishWater(const FVector& WorldLocation) const`. No hace falta aviso de meta: el GameMode
  mira `IsFinishWater` de cada tortuga diez veces por segundo. Si otra pieza quiere dar la meta, que llame a
  `GetAuthGameMode<ATN_RunGameMode>()->MarkPlayerFinished(PC)`.
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

### Pruebas

- Sin lobby: `open LVL_BeachRace?BeachSeed=42?BeachWins=1` (semilla fija y conchas para ganar).
- Consola en la ventana del anfitrión: `TN.Race.WinRound [jugador]`, `TN.Race.Champion [jugador]`,
  `TN.Race.Stun [segundos] [jugador]`, `TN.Race.Kill [jugador]` (ruta de muerte), `TN.Race.Void [jugador]` (al vacío),
  `TN.Race.PlayAgain`, `TN.Race.ChangeMode`, `TN.Race.Menu` y `TN.Mode [Coop|Race]`. `jugador` es el índice en
  `PlayerArray` (0 por defecto, normalmente el anfitrión).

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
| `TN_BeachEnemy.*` | `ATN_BeachEnemy` (abstracta, hija de `ATN_BeachElement`): base de los cinco enemigos (red, tortugas, suelo, voz) |
| `TN_BeachGiantCrab.*` | `ATN_BeachGiantCrab`: cangrejo gigante con una pinza enorme |
| `TN_BeachSeaUrchin.*` | `ATN_BeachSeaUrchin`: erizo de mar que rueda hacia ti |
| `TN_BeachLizard.*` | `ATN_BeachLizard`: lagarto que toma el sol, asusta y se esconde |
| `TN_BeachQuadLane.*` | `ATN_BeachQuadLane`: paso de quads que cruza la playa |
| `TN_BeachGullZone.*` | `ATN_BeachGullZone`: gaviotas y pelícanos que cagan, bajan en picado y se llevan tortugas |
| `TN_BeachStorm.*` | `ATN_BeachStorm`: la tormenta de bañistas (no es un elemento; la crea el GameMode) |
| `TN_BeachCameraShake.*` | `UTN_BeachCameraShake`: temblor de cámara (no había ninguno en el proyecto) |
| `TN_BeachEnemySynth.*` | `UTN_BeachEnemySynthComponent`: sonidos sintetizados (golpes, graznidos, motor, viento) |
| `TN_BeachEnemyKit.h`, `TN_BeachEnemyMeshes.h` | Mallas low-poly por piezas, caché de mallas, sombras y partículas propias |
| `TN_BeachEnemyDebug.cpp` | Consola de pruebas |

### Comunes

- **Escala**: todo a `TNBeach::Scale` (28). Se reutiliza la fauna low-poly (`TN_ProcMapFaunaMeshes.h`: gaviota, pelícano,
  cuadrúpedo del lagarto y sus ayudas de cuerpo y ojos), las partículas de `TNAmbientFX` (sin su registro global: cada
  actor lleva sus emisores), el velo de la tormenta del camino (`TNStormFX::BuildVeil`) y la tos
  (`UTN_StormCoughComponent`). Las mallas se construyen en ejecución y se comparten por paleta (referencia débil: el
  recolector las suelta cuando no queda ninguno). Colisión: solo el cuerpo del cangrejo (caja tipo Pawn).
- **Red** (servidor escucha): el servidor decide objetivos, golpes y aturdimientos (`TNBeach::StunTurtle`); los que
  andan replican `FTNBeachMoverRep` (suelo bajo el cuerpo cuantizado a 1 cm, giro en 16 bits, estado, hora del estado
  y un punto de interés) a 8-12 Hz y solo lo que cambia; los clientes interpolan con una pizca de extrapolación. El
  paso de quads, la zona de gaviotas y la tormenta replican solo horas y sentidos: cada máquina calcula la posición con
  el reloj del servidor. Los golpes van por multicast no fiable. Siempre relevantes (con la distancia por defecto un
  cliente destruiría y rehará sus mallas cada vez que se aleja 150 m por la playa).
- **Carrera en marcha**: no atacan durante el recuento ni el podio (`RacePhase` = `RoundResults` o `Champion`); en
  `Waiting` sí (por si la fase no cambiara).
- **Temblor de cámara**: `UTN_BeachCameraShake::Kick` (golpe que se apaga) y `Rumble` (sostenido mientras se refresque),
  con la fuerza entera hasta un radio y apagándose hasta otro, para los jugadores locales.
- **Rendimiento**: sin pantalla (servidor dedicado) no hay mallas ni efectos; lejos de la cámara (300 m; 400-600 m el
  quad y las gaviotas) no se anima. Trazas de suelo a 10 Hz como mucho por enemigo; nada asigna memoria por fotograma.

### Cangrejo gigante (`ATN_BeachGiantCrab`)

- Caparazón de 5 m de ancho (una cría de 18 cm) sobre ocho patas, ojos en pedúnculos y una pinza de ~6 m a la derecha.
  Cuatro paletas (rojo con pinza amarilla, violinista azul, violeta, fantasma de arena). `SizeScale` 0,8-1,2.
- Patrulla el 52 % de su huella (13 m) andando de lado a 2,6 m/s, con ratos quieto agitando la pinza.
- Ve a una tortuga a 22 m y la persigue de lado a 5,6 m/s (se escapa esprintando: 8 m/s) mientras no se aleje de su
  sitio más de 38 m (o 1,4 veces la huella); si la pierde, vuelve a casa a 4 m/s. Decide con
  `TNCrabLogic::DecideChaseTransition`, la del cangrejo de siempre.
- **Mazazo**: con la tortuga a su alcance (~7 m) más 3 m, se para, levanta la pinza y tiembla 0,6 s; la sombra de dónde
  cae aparece donde estará la tortuga (0,3 s de adelanto) y crece hasta 2,8 m de radio. Cae en 0,14 s: quien esté
  dentro (2,8 m + 0,4) queda en bola aturdida 3,5 s con un empujoncito hacia fuera. Luego 1,1 s con la pinza clavada,
  1,4 s sin poder repetir y a la golpeada la ignora 6 s. Arena, temblor fuerte a menos de 15 m.
- Los objetos del jugador lo aturden (quieto con los ojos dando vueltas) o lo ciegan (vuelve a casa)
  (`ITN_EnemyTargetInterface`).

### Erizo de mar (`ATN_BeachSeaUrchin`)

- Bola violeta, negra, roja u oliva de 72 púas: rueda sobre un radio de 1,26 m (3 m de diámetro). `SizeScale` 0,75-1,35.
- Ve a 20 m y rueda girando hacia la tortuga más cercana a 1,9 m/s (lento: solo pilla a quien se despista), sin salirse
  de 16 m de su sitio (o 1,35 veces la huella). Sin nadie, respira y da paseos cortos a 0,9 m/s.
- Tocarlo (a ~1,65 m del centro) pincha en cualquier estado: aturdida 2,4 s y lanzada a 7 m/s hacia fuera y 4,5 hacia
  arriba; el erizo retrocede 0,8 s y la ignora 3 s.

### Lagarto (`ATN_BeachLizard`)

- Vida del ambiente: lagarto de 15 m (55 cm reales) sobre el cuadrúpedo de la fauna: iguana verde con cresta,
  turquesa de cabeza amarilla, ocelado con manchas azules o naranja de collar. Toma el sol: flexiones cada ~7 s,
  cabeceos, lengua y cola que se mece.
- A 30 m se pone alerta y mira a la tortuga. A 17 m: el 55 % de las veces da un susto (amago de 0,35 s hacia ella, se
  hincha, tiembla, saca la lengua y bufa; a quien esté a menos de 8 m de la cabeza lo empuja 6,5 m/s, sin aturdir) y
  luego huye; si no, huye sin más. A 9 m huye directamente.
- Huye a 15 m/s a la roca, grupo de rocas, tronco, madera, tablones, restos de vela o castillo pequeño más cercano (hasta
  45 m, nunca hacia la tortuga) y se mete debajo; si no hay, da un arreón y se entierra sacudiéndose (1,3 s). Escondido
  7-12 s; no sale con una tortuga a menos de 18 m. Sale y vuelve andando a su sitio.

### Paso de quads (`ATN_BeachQuadLane`)

- Eje X local del actor (el generador lo gira 90° y lo cruza de lado a lado), `Extent` de largo (0 = 280 m). En la arena,
  dos rodadas que avisan por dónde pasa (se trazan contra el suelo; si aún no está, se reintenta).
- Quad a escala con piloto: 56 m de largo, ruedas de 16,8 m de alto y 6,7 m de ancho, centros a ±8,7 m (las ruedas
  llegan a ±12 m: la huella), hueco de 10,6 m entre ruedas y 7,3 m de altura libre bajo el chasis. `SizeScale` 0,7-1,4
  escala todo.
- Primera pasada a los 5-15 s; después, cada 15-24 s. Aviso de 3,5 s: temblor creciente (0,12 → 0,57) a menos de 15 m
  del paso (se nota hasta 90 m), motor que se acerca y humo y hojas entre las palmeras del lado de salida. Cruza a
  42 m/s (unos 9 s de palmera a palmera, sale de 15 m dentro de la selva), revienta las palmeras al salir y al entrar.
- Aplastar: una rueda que pasa por encima (±3,8 m a lo ancho, ±5 m a lo largo) aturde 3,2 s y lanza a 17 m/s en su
  sentido, 3,5 de lado y 11 hacia arriba; 1,2 s sin repetir con la misma. Temblor 0,85 a menos de 25 m del quad.

### Gaviotas y pelícanos (`ATN_BeachGullZone`)

- 3-4 gaviotas (25 m de envergadura) y, el 60 % de las veces, un pelícano (40 m), dando vueltas a 55-80 m de altura en
  un círculo de 35 m o más, con sus sombras en la arena (más grandes y cercanas cuanto más bajan). Graznan de vez en
  cuando.
- Ataca cada 3,5-7 s a una tortuga al azar de las que están a menos de su huella + 8 m del centro (sin aturdir y sin
  sombrilla). Las gaviotas cagan o bajan en picado a partes iguales; el pelícano siempre baja.
- **Cagada**: 1,4 s volando hasta encima; la suelta desde 45 m y cae en 1,6 s mientras su sombra se encoge de 6 a
  2,3 m. Quien esté dentro (2,3 m + 0,45, y a menos de 3 m en altura: a cubierto la mancha cae encima) queda aturdido
  2,6 s con un pegote blanco en el caparazón; en la arena queda la mancha 9 s.
- **Picado**: 0,9 s colocándose a 50 m y 48 m de altura; se lanza acelerando y su sombra crece y se acerca. Corrige
  hasta 0,5 s antes de llegar (1,8 s, con 0,25 s de adelanto). A los 2,3 s coge a la tortuga que esté bajo el pico
  (3 m + 0,45) si no está en pleno panzazo (`IsBellyPoseActive`) ni a cubierto: la sube 32 m en 2 s llevándola 18 m
  hacia la salida (el servidor guía la caja física del caparazón) y la suelta: ~2,6 s de caída y 3 s aturdida al
  llegar. Si falla, remonta graznando.

### Tormenta de bañistas (`ATN_BeachStorm`)

- **Interfaz**: el GameMode la crea 30 m detrás de la salida, en el centro de la playa y mirando al mar, y llama por
  nombre a `StartStorm()` y `StopStorm()` (UFUNCTION sin parámetros). Desde C++: `StartStormAt(Desplazamiento,
  Velocidad, Gracia)`, `GetFrontDistance()` (distancia del frente al actor), `GetFrontLocation()`,
  `IsLocationInside(Punto)`, `IsStormActive()` y `ATN_BeachStorm::FindStorm(Contexto)`. Parada, se queda quieta y a la
  vista (recuento); al destruirla desaparece.
- El frente es la recta X local = distancia del frente, 220 m a cada lado (la playa y la selva). Sale a 3,3 m/s tras
  6 s de gracia y gana 0,3 m/s por minuto (a los 5 min va a 4,8 m/s: más que andando). Editables: `DefaultSpeed`,
  `SpeedRampPerMinute`, `DefaultGrace`, `HalfWidth`, `InsideMargin` (4 m), `StunSeconds` (2,2), `HitInterval` (3 s) y
  el lanzamiento (`PushForward` 11 m/s, `PushUp` 7, `PushSide` 3).
- Dentro: revolcón cada 3 s (bola aturdida lanzada hacia el mar), niebla y tinte de arena, viñeta, tos y viento. Por
  fuera: velo de arena de 65 m, 18 trastos volando alrededor de la cámara (sombrillas de 40 m, sillas, toallas,
  flotadores, cubos, palas, chanclas, pelotas), ocho bañistas cuyas piernas pisan dentro del polvo (pisotón, arena y
  temblor del más cercano) y un temblor suave si el frente está a menos de 60 m por detrás.

### Probar

- `TN.Beach.Spawn GiantCrab` (y `SeaUrchin`, `Lizard`, `QuadLane`, `GullZone`), del generador, en cualquier mapa.
- **Cangrejo**: acercarse andando (persigue de lado) y esprintar (se escapa); quedarse quieto en la sombra de la pinza
  (bola aturdida); salir de la sombra durante el aviso (falla); alejarse más de 38 m (vuelve a casa). Chocar con él (no
  se atraviesa). Con 2 jugadores: al golpeado lo deja y va a por el otro.
- **Erizo**: quedarse quieto delante (rueda hasta pinchar), tocarlo andando por detrás (pincha igual).
- **Lagarto**: acercarse despacio (alerta a 30 m), luego a 17 m (susto o huida), hasta que se meta bajo una roca o se
  entierre; esperar lejos a que salga.
- **Quad**: `TN.Beach.Quad.Now`; notar el temblor y el humo del lado de salida; ponerse en una rodada (aplastado y
  lanzado), fuera del paso y en medio de las dos rodadas (sobrevive). Probar los dos sentidos.
- **Gaviotas**: `TN.Beach.Gull.Attack 1` (cagada: apartarse de la sombra; quedarse: mancha) y `TN.Beach.Gull.Attack 2`
  (picado: panzazo en el último momento para esquivar; quedarse: la sube, la lleva hacia la salida y la suelta).
- **Tormenta**: en la carrera arranca sola; en otro mapa, `TN.Beach.Storm.Start [metros por detrás] [cm/s]` y
  `TN.Beach.Storm.Stop`. Dejarse alcanzar (revolcones cada 3 s, niebla, tos), adelantarla esprintando.
- En PIE con 2-3 jugadores: que los clientes vean lo mismo (posiciones suaves, golpes a la vez) y que no haya
  correcciones raras al empujar el lagarto o al aplastar.
- Consola: `TN.Beach.Enemy.Debug 1` (radios de visión, correa, patrulla, golpe y cajas de las ruedas en el servidor).

## Recuento, campeón y podio (`UI/Race/`)

| Archivo | Qué es |
|---|---|
| `UI/Race/TN_RaceScreens.*` | `UTN_RaceScreensSubsystem`: subsistema de mundo en cada máquina con jugador; mira el estado replicado y pone o quita las pantallas (sin RPC ni cambios en el PlayerController); consola de vista previa |
| `UI/Race/TN_RaceTallyWidget.*` | `UTN_RaceTallyWidget`: recuento de conchas tras cada ronda |
| `UI/Race/TN_RaceChampionWidget.*` | `UTN_RaceChampionWidget`: pantalla del campeón (botones a la izquierda, podio a la derecha) |
| `UI/Race/TN_RacePodiumStage.*` | `ATN_RacePodiumStage`: el podio en 3D, capturado a una textura |
| `Private/UI/Race/TN_RaceArt.h`, `TN_RaceUIKit.h` | arte en código (fondos, huecos, corona, cielo, iconos, caras con el color de piel) y piezas de UMG |
| `Player/TN_TurtleAnimInstance.*` | poses Trofeo, Decepcionada y Pataleta (`SetCelebration`) y la zambullida del acantilado |

Todo en el estilo del HUD Tortunavy (`TN_HUDArt.h`, `TN_HUDFaces.h`, `TN_HUDStyle.h`) y por encima de él (ZOrder 20 y
21: por encima del HUD, 4-10, y por debajo de las ruedas, 30). Sonidos: el «pom» y el «¡plin!» sintetizados de las
conchas de puntos (`UTN_ScoreShellSynthComponent`, en 2D en el PlayerController).

### Recuento (`RoundResults`)

- A pantalla completa: el mar azul marino con rayos de luz y burbujas que suben y, abajo, la orilla de arena. Arriba,
  la cinta «RONDA N» y un cartel que empieza en «Recuento de conchas» y pasa a «¡Concha para X!», «¡X gana la
  partida!» o «¡Nadie ha llegado al agua! Esta vez no hay concha.». Abajo, «Siguiente ronda en N» (`PhaseSecondsLeft`)
  o, si la concha corona, «¡Al podio en N!».
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
- La concha nueva va al hueco siguiente a las que el ganador tenía al empezar la ronda: el subsistema las apunta
  mientras se prepara y en los 4 primeros segundos de carrera, así que da igual que `RoundWins` llegue por red antes que
  la fase. Si `RoundWinner` llega tarde, la columna se corrige mientras la concha no haya salido volando.

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
  entra así en el agua. Se acaba al aterrizar o al nadar (y con el panzazo, el caparazón, el derribo o si la llevan).
  Cosmética y local en cada máquina a partir del movimiento replicado, sin RPC; fuera de la playa no hay generador y
  nunca pasa. `IsCliffDiving()` lo dice.

### Probar sin jugar

En cualquier mapa y solo en la máquina que lo escribe:

- `TN.Race.Tally [ganador] [jugadores] [final]`: recuento con tu tortuga (columna 0) y otras de mentira (Coral, Bruma,
  Perla…, con pieles, caparazones, cascos y ojos del catálogo). `ganador` es la columna a la que vuela la concha (-1 =
  nadie llega al agua; 0 por defecto), `jugadores` de 1 a 6 (4) y `final 1` hace que la concha corone (baja la corona) y
  que después salga la pantalla del campeón. Sin `final` se cierra sola a los 8 s.
- `TN.Race.Podium [jugadores]` (1-3): la pantalla del campeón con el podio y la música; cualquier botón la cierra.
- `TN.Race.PreviewOff`: cierra la vista previa.
- En partida: `TN.Race.WinRound` (recuento) y `TN.Race.Champion` (recuento con la concha que corona y luego el podio).
- Zambullida: en `LVL_BeachRace`, saltar desde la repisa del acantilado de la meta.
