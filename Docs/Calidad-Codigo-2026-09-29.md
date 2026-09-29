# Calidad de código de cara a Steam — auditoría 2026-09-29

Alcance: `Source/Tortunabo` (597 ficheros, 242 840 líneas), HEAD `ad417eaf0`. Solo código **vivo**; el código muerto y la
basura van en `Docs/Limpieza-2026-09-29.md`. Revisión estática (grep, lectura dirigida y escaneo de cuerpos de `Tick`),
sin perfilar ni cocinar: donde el impacto depende de cuántas instancias hay en una ronda, se indica «medir».

Severidad: CRITICAL (vulnerabilidad o pérdida de datos), HIGH (bug serio o rotura probable en la build de Steam),
MEDIUM (fragilidad o coste real), LOW (higiene). Esfuerzo: S (< 2 h), M (medio día a 2 días), L (> 2 días).

## Resumen

| Severidad | Nº |
|-----------|----|
| CRITICAL  | 0  |
| HIGH      | 4  |
| MEDIUM    | 13 |
| LOW       | 8  |

Lo que ya está bien y no hay que tocar: la búsqueda de actores en caliente casi siempre está cacheada o limitada
(`TN_TurtleAnimInstance.cpp:825`, `TN_ShellImpactFXComponent.cpp:601`, `TN_AmbientSoundscape.cpp:385`,
`TN_BeachEnemy.cpp:197` con un escaneo por frame compartido); los elementos de la playa aplican dormancy y distancia de
relevancia por ronda (`TN_BeachElement.cpp:77-94`); `ScorePickup` y `PickupGlow` bajan su intervalo de tick lejos del
jugador; la voz viaja por RPC `Unreliable`; los RPC de depuración del fantasma están protegidos con `!UE_BUILD_SHIPPING`
(`TN_SpectatorGhost.cpp:485-493`). El texto de UI está localizado en su gran mayoría (1192 `LOCTEXT`/`NSLOCTEXT`).

---

## HIGH

### H1 · RPC de pruebas sin protección en la build de Steam — esfuerzo S
- **Hecho** (`88399d1da`): cuerpo vacío en Shipping y, en desarrollo, solo el controlador local del servidor escucha (`TN_DebugRpcDecisions.h`, test `Tortunabo.DebugRpc.HostOnly`). Rechazo suave con aviso en log, sin expulsar.
- `Public/Player/MP_GamePlayerController.h:449-450` y `Private/Player/MP_GamePlayerController.cpp:1340-1450`
  (`ServerStormTest`), `.cpp:1555` (`ServerTestBooth`).
- Escenario: en una sala pública de Steam, cualquier invitado con un cliente modificado (o con consola habilitada) llama
  a `ServerStormTest("off", 0)` y para la tormenta para todos, o la recoloca y se teletransporta por biomas. No hay
  `#if !UE_BUILD_SHIPPING` ni comprobación de anfitrión. `ServerShellsTest` sí lo tiene (`.cpp:1457`), lo que confirma
  que es un olvido.
- Arreglo: el mismo guard que `ServerShellsTest` y `TN_SpectatorGhost::ServerRunDebug`, más `WithValidation` que
  rechace si no es el anfitrión.

### H2 · Guardado de cosméticos y tutorial: sin versión, sin comprobar errores y con sobrescritura del corrupto — esfuerzo S-M
- **Hecho** (`0d393c6f6`): `SaveVersion` + marca de fin `bWriteComplete` y migración v0→v1; el fichero ilegible o truncado se aparta a `<ranura>_corrupto_<fecha>` y no se pisa; `SaveGameToSlot` comprobado con un reintento (también en ajustes). Tests `Tortunabo.SaveGame.*`. Sin `AsyncSaveGameToSlot`.
- `Public/Multiplayer/TN_CosmeticSaveGame.h:20-48` (sin campo de versión), `Private/Multiplayer/MP_GameInstance.cpp:1163-1203`
  (`LoadCosmeticProfile`/`SaveCosmeticProfile`) y `.cpp:1273-1330` (tutorial).
- Escenario: el `.sav` queda truncado (cierre durante la escritura, disco lleno, conflicto de Steam Cloud).
  `LoadGameFromSlot` devuelve `nullptr`, se crea un perfil vacío en silencio y el siguiente `SaveCosmeticProfile`
  sobrescribe el fichero: el jugador pierde todo lo desbloqueado y `AccumulatedRaceScore`. Además, el `bool` de
  `SaveGameToSlot` se ignora en los tres sitios (`.cpp:1202`, `.cpp:1328`, `TN_GameSettingsSubsystem.cpp:622`), y
  cualquier cambio de formato futuro (renombrar un `FName` de casco, añadir moneda) no tiene ruta de migración.
- Arreglo: `int32 SaveVersion` en los dos `USaveGame`, migración al cargar; si la carga falla con el fichero presente,
  renombrarlo a `.bak` antes de crear uno nuevo; registrar el fallo de `SaveGameToSlot` y reintentar. Valorar
  `AsyncSaveGameToSlot`.

### H3 · `BP_ScorePickup` cargado por ruta literal y de forma síncrona en 8 sitios — esfuerzo M
- **Hecho** (`e1e6fa0e5`): `UTN_GameplayAssetSettings::ScorePickupClass` (única ruta, en `DefaultGame.ini`), precarga asíncrona en `UMP_GameInstance::Init` y `Error` en log si falta. Test `Tortunabo.Assets.ScorePickupClass`. El catálogo de `DropPrize` (`:562`) sigue síncrono.
- Ruta `/Game/Blueprints/Gameplay/Items/BP_ScorePickup.BP_ScorePickup_C` repetida en `MP_GamePlayerController.cpp:1506`,
  `TN_BeachChest.cpp:950`, `TN_BeachFortress.cpp:162`, `TN_BeachLizard.cpp:577`, `TN_BeachLootShells.cpp:841`,
  `TN_ProcMapGenerator_Spawn.cpp:414` y `:632`, `TN_ProcMapTypes.cpp:222`.
- Escenario: Álvaro mueve o renombra el BP en el editor (la redirección no cubre cadenas en C++). Todas las fuentes de
  conchas (cofres, fortalezas, lagartos, botín de ronda) dejan de soltar puntos en silencio: `LoadClass` devuelve `nullptr`
  y la ronda se juega sin puntuación. En `TN_BeachLizard::DropPrize` (`:562` y `:577`) la carga síncrona del catálogo y
  de la clase ocurre en el momento de la muerte del lagarto: tirón en frío la primera vez.
- Arreglo: un único `TSoftClassPtr<ATN_ScorePickup>` en unos `UDeveloperSettings` del proyecto (o en el `DA_ProcMapSettings`
  existente), resuelto y retenido al empezar la partida.

### H4 · Assets referenciados solo por cadena fuera de `DirectoriesToAlwaysCook` — esfuerzo S (verificar con un cook)
- **Hecho** (`5c1a471ad`): `/Game/Cosmetics`, `/Game/Animations` y `/Game/Meshses` en `DirectoriesToAlwaysCook` (cargan clases nativas sin asset: pasarlas a UPROPERTY no las cocinaría). Test `Tortunabo.Cook.StringPathsAreCooked` sobre todas las rutas literales de `Source/`. Pendiente: un cook real.
- `Config/DefaultGame.ini:22-27` cocina siempre `/Game/Maps`, `/Game/UI`, `/Game/Input`, `/Game/Blueprints`, `/Game/ProcMap`.
  No cubre `/Game/Cosmetics` (14 literales), `/Game/Animations` (10) ni `/Game/Meshses` (4).
- Escenario: en la build empaquetada, un asset que solo C++ nombra por cadena no se cocina y `LoadObject` devuelve
  `nullptr`. Casos de riesgo: `TN_TurtleAnimInstance.cpp:1029-1031` (`Old_Man_Idle`, `Walking`, `Yelling`: la tortuga
  sin idle ni andar), `TN_TurtleFaceComponent.cpp:187` (`M_TurtleFaceParts`), `TN_CosmeticLook.cpp:14-15`. Los que sí
  cuelgan de un mapa o de `DT_Skins` se salvan; los demás, no. No verificado: requiere un cook y revisar el log de
  `LoadObject` fallidos.
- Arreglo: añadir esas carpetas a `DirectoriesToAlwaysCook` o, mejor, convertirlas en referencias duras
  (`TSoftObjectPtr` en un DataAsset que cuelgue de un mapa cocinado).

---

## MEDIUM

### M1 · Algas: malla procedural reconstruida en CPU cada frame — esfuerzo M
- **Hecho** (`66c8a93ad`): reconstrucción limitada por distancia a la cámara (cada frame < 25 m o enrollando; 20 Hz < 50 m; 8 Hz < 90 m; quieta más lejos), buffers reutilizados. Test `Tortunabo.Seaweed.RebuildRate`. Sin WPO; falta medir con `stat game`.
- `World/Beach/TN_BeachSeaweed.cpp:162-167` (tick siempre activo, sin intervalo), `:789-809` (`Tick`),
  `:724-786` (`RebuildLiveMesh`: `TArray` nuevos por fronda y `UpdateMeshSection` por frame), más tres
  `TActorIterator<ACharacter>` por frame (`:355`, `:445`, `:679`).
- Escenario: con varias algas en pantalla, cada una reconstruye su malla entera cada frame solo para el vaivén en
  reposo (`WasRecentlyRendered(0.3f)` es cierto para todo lo visible). Coste de CPU y de subida a GPU proporcional al
  número de algas visibles; medir con `stat game` en la zona del encharcado.
- Arreglo: vaivén en reposo por World Position Offset en el material; reconstruir la malla solo mientras
  `bWrapping`. Intervalo de tick lejos del jugador, como `ScorePickup`.

### M2 · Tormenta de la carrera resuelta por nombre de clase y llamada por `FName` — esfuerzo S
- `Game/TN_BeachRaceGameMode.cpp:2758` (`FindObject<UClass>("/Script/Tortunabo." + StormClassName)`), `:2782` y `:2794`
  (`CallNoParamFunction(Storm, "StartStorm"/"StopStorm")`), `:2801-2810` (`FindFunction` + `ProcessEvent`);
  mismo patrón en `World/Beach/TN_BeachRaceGenerator.cpp:352` (`OnRep_Spec`).
- Escenario: renombrar `ATN_BeachStorm` o su `StartStorm` compila sin error y la carrera se juega sin tormenta; solo
  queda un `Warning` en el log.
- Arreglo: `TSubclassOf<ATN_BeachStorm>` y llamada directa (o una interfaz `ITN_RoundStorm`).

### M3 · `ButtonGroupManager`: estado solo por multicast y efectos por `BlueprintImplementableEvent` — esfuerzo S
- `Public/World/TN_ButtonGroupManager.h:117-150`, `Private/World/TN_ButtonGroupManager.cpp:180-188`.
- Escenario: un jugador que entra tarde (o para el que el actor no era relevante cuando se resolvió el puzzle) nunca
  recibe `MulticastNotifyActivated`: ve el puzzle sin resolver. Además, los efectos van por BIE, en contra de la
  convención del proyecto (audio y VFX por `UPROPERTY EditDefaultsOnly` en C++).
- Arreglo: `UPROPERTY(ReplicatedUsing=OnRep_bGroupActive)` como fuente de verdad; el multicast, solo para el efecto
  puntual.

### M4 · Doble camino OnRep + `NetMulticast, Reliable` para estado persistente — esfuerzo M
- `Public/Player/TortugaCharacter.h:885` (`MulticastApplyKnockdownVisual`), `:914` (`MulticastSetDeadVisual`),
  `:1325` (`Multicast_OnDiveVisual`), frente a `bIsKnockedDown`, `bIsDead` y `bIsDiving` replicados con OnRep
  (`:1018`, `:1113`, `:1233`); igual en `Public/Core/TN_CoopPlayerState.h:128` y `:140` (`MulticastForceApplyHelmet/Skin`)
  frente a `EquippedHelmetId`/`EquippedSkinId`.
- Escenario: cada derribo aplica el visual dos veces en cada cliente (RPC y OnRep, en orden no garantizado). Hoy
  funciona porque las funciones son idempotentes; cualquier efecto no idempotente que se añada (sonido, partículas,
  contador) saldrá doble. Con 8 jugadores, cada derribo son 7 RPC fiables más la propiedad.
- Arreglo: documentar la idempotencia como contrato o dejar un único camino (OnRep para estado; multicast
  `Unreliable` solo para el efecto de entrada, con la posición como parámetro, como ya hace `MulticastSetDeadVisual`).

### M5 · `TortugaCharacter.h`, cabecera dios incluida en 92 unidades — esfuerzo L
- `Public/Player/TortugaCharacter.h`: 1668 líneas, 157 `UPROPERTY`, 46 `UFUNCTION`, incluida por 92 ficheros.
- Escenario: tocar un flotante de ajuste del buceo o del emote recompila unas 92 unidades y vuelve a pasar UHT. Es
  el mayor coste de iteración del proyecto.
- Arreglo: ver el split 3 más abajo.

### M6 · Implementación entera en cabeceras compartidas — esfuerzo M
- `Public/World/Beach/TN_BeachLayout.h` (4319 líneas, 7 unidades), `Public/World/ProcMap/TN_ProcMapLayout.h` (1071, 20),
  `Public/World/ProcMap/TN_ProcMapTerrain.h` (1498, 9), `Private/World/Beach/TN_BeachEnemyMeshes.h` (1014, 10),
  `Public/World/ProcMap/TN_ProcMapPath.h` (2181, 4).
- Escenario: cada unidad que las incluye compila el generador entero; un retoque de la playa recompila 7 unidades de
  4000 líneas cada una. Con unity build el efecto se amortigua, pero en los builds adaptativos del editor no.
- Arreglo: declaraciones en el `.h` y cuerpos en `.cpp` (splits 4 y 7).

### M7 · Números de equilibrado dispersos como `constexpr` locales — esfuerzo M
- `KnockSeconds` en 6 sitios con valores de 1,9 a 3,0 (`TN_RaceFrisbee.cpp:55`, `TN_RaceGullStrike.cpp:71`,
  `TN_RaceHomingCrab.cpp:52`, `TN_BeachQuadLane.cpp:30`, `TN_BeachHermitCrab.cpp:71`, `TN_BeachSeaUrchin.cpp:37`);
  `IgnoreSeconds` en 5, `EnemyStunSeconds` en 4 (4-5 s), `Gravity` en 5 (980 y 1250 en `TN_BeachHermitCrab.cpp:42`,
  ignorando la gravedad del mundo).
- Escenario: el equilibrado de acceso anticipado exige recompilar y buscar a mano; dos objetos que deberían aturdir
  lo mismo divergen sin que nadie lo decida.
- Arreglo: un `UTN_CombatTuning` (DataAsset o `UDeveloperSettings`) con los aturdimientos, inmunidades y gravedades;
  los `constexpr` pasan a leer de ahí.

### M8 · Emotes como `switch` por índice en una función de 520 líneas — esfuerzo M
- `Player/TortugaCharacter_Emote.cpp:372-892` (`TickEmote`, `case 0:` en `:484` … `case 9:` en `:796`), acoplado al
  `EmoteID` de `UI/HUD/TN_EmoteWheelDataAsset.cpp`.
- Escenario: reordenar o añadir una entrada en el DataAsset de la rueda reproduce otro emote sin error. Cada emote
  nuevo exige C++.
- Arreglo: una función por emote en una tabla indexada por un `enum class` compartido con el DataAsset; validar en
  `IsDataValid` que cada `EmoteID` tiene coreografía.

### M9 · Ajustes: `Version` declarada y nunca leída; fallo de escritura olvidado — esfuerzo S
- `Public/Settings/TN_SettingsSaveGame.h:174` (`Version = 3`), `Private/Settings/TN_GameSettingsSubsystem.cpp:601-612`
  (no la mira), `:622-624` (`SaveGameToSlot` ignorado y `bSettingsDirty = false` igualmente).
- Escenario: si la escritura falla, el juego cree que ha guardado y no reintenta; el próximo arranque carga ajustes
  viejos. Un ajuste de la versión 2 cargado en la 3 no se migra (se confía en los valores por defecto de la `struct`).

### M10 · Mensajes de estado del menú sin localizar — esfuerzo S
- `Multiplayer/MP_GameInstance.cpp`: 42 llamadas a `UpdateStatus(TEXT(...))`, mezcla de inglés y español
  (p. ej. `:83`, `:87`, `:100` «ERROR: No Online Subsystem. Is Steam running?», `:821`, `:825`, `:840`); se enseñan en
  `UI/Menu/MP_MainMenuWidget.cpp:385-395` con `FText::AsCultureInvariant` (el comentario de `:394` dice que llegan
  traducidos, y no es cierto).
- Escenario: un jugador en otro idioma ve «Opening Steam invite overlay...» o «WARNING: SteamDevAppId invalido».
- Arreglo: `UpdateStatus(const FText&)` con `NSLOCTEXT`; separar el log técnico (`UE_LOG`) del texto de pantalla.

### M11 · Rutas `/Game/...` duplicadas (126 literales en 56 ficheros) — esfuerzo M
- Los más repetidos: `M_CosmeticVertexColor` ×11, `M_ProcFoliage` ×11, `BP_ScorePickup` ×8 (H3), `M_ProcGlow` ×7,
  `TotugaDemo_Rig` ×4 bajo la carpeta con errata `/Game/Meshses/` (`TN_CosmeticPreview.cpp:110`,
  `TN_GeneralBriefing.cpp:153`, `TN_ShopKeeper.cpp:172`, `TN_RacePodiumStage.cpp:619`), animaciones de marcador de
  posición `Old_Man_Idle`/`Yelling` ×4.
- Escenario: corregir la errata `Meshses` o reorganizar `/Game/ProcMap/Materials` rompe entre 4 y 11 ficheros a la vez,
  y los que usan `ConstructorHelpers` fallan en el arranque del editor.
- Arreglo: un único `TNAssetPaths.h` con las constantes como paso inmediato (S); a medio plazo, `TSoftObjectPtr` en
  DataAssets.

### M12 · Cargas síncronas en momentos de juego — esfuerzo S
- `World/Beach/TN_BeachLizard.cpp:562` (catálogo al morir), `TN_BeachChest.cpp:955`, `TN_BeachLoot.cpp:451` y `:604`,
  `ProcMap/TN_ProcSearchSpot.cpp:1031`, `TN_RaceItems.cpp:299`, `UI/Shop/TN_ShopWidgets.cpp:288` y `:417` (por tarjeta).
- Escenario: la primera búsqueda o el primer cofre de la ronda bloquea el hilo de juego mientras se lee el
  `DataTable` del disco; en HDD, tirón visible. Con 106 cargas síncronas en total, la mayoría en construcción o
  `BeginPlay` (aceptable), estas son las que caen en pleno juego.
- Arreglo: precargar y retener los catálogos en `UTN_BeachLootSubsystem::Initialize` o en el `BeginPlay` del GameMode.

### M13 · `BuildStructures`: una función de 1559 líneas — esfuerzo L
- `World/ProcMap/TN_ProcMapGenerator_Build.cpp:1752-3310`, con 48 lambdas locales y 9 búferes de malla compartidos.
- Escenario: cualquier cambio en una estructura (torres, puentes, cuevas) exige leer 1500 líneas; imposible de revisar
  en un PR ni de probar por partes. El resto de funciones gigantes vivas: `TickEmote` 520 (M8), `BuildJungle` 379,
  `BuildCastle` 364, `BuildFauna` 354, `PoseBones` 342, `ApplyKnockdownVisual` 305, `GullZone::VisualTick` 280,
  `GiantCrab::ServerTick` 277, `Lizard::ServerTick` 268, `NativeUpdateAnimation` 267 (14 funciones por encima de
  250 líneas).
- Arreglo: split 2.

---

## LOW

| ID | Hallazgo | Fichero:línea | Esfuerzo |
|----|----------|---------------|----------|
| L1 | `static` de diagnóstico compartidos entre instancias en caliente: `LogAccumulator` de la cámara (todas las tortugas suman al mismo), `bLoggedKirk` con `Warning`, contadores de pruebas | `TortugaCharacter.cpp:732`, `TortugaCharacter_Emote.cpp:842`, `MP_GamePlayerController.cpp:1402-1403`, `:1488` | S |
| L2 | `Saved/ResetTutorial.txt` sigue activo en Shipping: un fichero en el disco del jugador reinicia su progreso | `MP_GameInstance.cpp:1289-1319` | S |
| L3 | Un único perfil cosmético por máquina (`_Local`, usuario 0): dos cuentas de Steam en el mismo PC comparten desbloqueos | `MP_GameInstance.cpp:1205-1210` | S |
| L4 | Desbloqueos autoritativos del cliente: el servidor acepta la lista que manda el cliente (solo tope de 256) | `MP_GamePlayerController.cpp:1010-1015`, `:1103-1108` | M (solo si hay cosméticos de pago) |
| L5 | `FindComponentByClass` por frame en widgets del jugador local (4 en el HUD, 1 en la pausa) | `UI/HUD/TN_PlayerHUDWidget.cpp:61-99`, `UI/Pause/TN_PauseMenuWidget.cpp:1134` | S |
| L6 | Texto en pantalla sin localizar: etiquetas de teclas de reserva («Mayús izq.», «Cruz»), nombre del mapa y `Id` de cosmético en crudo | `Lobby/TN_TutorialPlayerComponent.cpp:58-69`, `TN_PauseMenuWidget.cpp:1628`, `:2978` | S |
| L7 | ~17 elementos de playa ponen `bAlwaysRelevant = true` en el constructor y `ApplyRoundNetProfile` lo pisa: engañoso, y el que aparezca fuera del generador (tutorial, depuración) queda siempre relevante | p. ej. `TN_BeachSeaweed.cpp:166`, `TN_BeachMine.cpp:277`; `TN_BeachElement.cpp:84` | S |
| L8 | `TActorIterator<ACharacter>` por frame sin caché en `Tick` (barato con 8 tortugas, pero multiplicado por instancias) | `TN_BeachShellGate.cpp:532`, `ProcMap/TN_ProcMapGenerator.cpp:82`, `Lobby/TN_TutorialFauna.cpp:300` | S |

---

## Top 10 ficheros a partir (sin cambiar lógica)

| # | Fichero (líneas) | División propuesta | Ganancia |
|---|------------------|--------------------|----------|
| 1 | `UI/Pause/TN_PauseMenuWidget.cpp` (3534) | Ya tiene secciones: `TN_PauseRows.cpp` (filas y controles, 327-1050), `TN_PauseOverlays.cpp` (FPS y quién habla, 1051-1168), `TN_PauseMenuWidget.cpp` (montaje, cabecera y páginas, 1169-1800), `_Settings.cpp` (pestañas de ajustes, 1801-2570), `_Room.cpp` (sala, 2571-2776), `_Actions.cpp` (volver, salir, confirmar, 2777-2963), `_Keys.cpp` (reasignación, 2964-3078), `_Input.cpp` (foco, entrada, sonido y contexto, 3079-3534) | Legibilidad máxima: el fichero más editado de la UI pasa a 7 de < 800; tocar la sala no recompila las filas |
| 2 | `ProcMap/TN_ProcMapGenerator_Build.cpp` (3310) | Helpers anónimos (1-1448) a `TN_ProcMapBuildKit.h/.cpp`; `BuildStructures` partida por familia (`_Towers.cpp`, `_Bridges.cpp`, `_Caves.cpp`, `_Formations.cpp`) con las lambdas convertidas en funciones libres que reciben `FTNProcMeshBuffers&` | Revisable y testeable por estructura; compilación paralela |
| 3 | `Public/Player/TortugaCharacter.h` (1668, 92 unidades) | Tipos y `enum` a `TortugaCharacterTypes.h`; declaraciones adelantadas en lugar de includes; estado de emote, buceo y derribo a `UTN_EmoteComponent`/`UTN_DiveComponent`/`UTN_KnockdownComponent` (los `.cpp` ya están separados: `_Emote`, `_Dive`, `_Knockdown`) | La mayor ganancia de compilación del proyecto: un ajuste de emote deja de recompilar 92 unidades |
| 4 | `Public/World/Beach/TN_BeachLayout.h` (4319, 7 unidades) | Declaraciones en el `.h` (~400 líneas); `TN_BeachLayout_Terrain.cpp` (relieve, dunas, trincheras, pozas, asientos: 191-1221), `_Start.cpp` (1222-1706), `_Round.cpp` (reparto: 1707-4319) | 7 unidades dejan de compilar 4000 líneas; los tests no cambian |
| 5 | `Game/TN_BeachRaceGameMode.cpp` (3014) | `_Round.cpp` (482-1353), `_Sprint.cpp` (1354-1736), `_Spawn.cpp` (2010-2258), `_Rescue.cpp` (2259-2743), `_Debug.cpp` (2832-3014, envuelto en `!UE_BUILD_SHIPPING`) | Ciclo de ronda legible por fase; la depuración sale de Shipping |
| 6 | `Multiplayer/MP_GameInstance.cpp` (2182) | `_Sessions.cpp` (465-1160), `_Cosmetics.cpp` (254-464 y 1163-1230), `_Tutorial.cpp` (1233-1330), `_NetErrors.cpp` (1332-1518), `_Rooms.cpp` (1519-2182) | Aísla Steam y el guardado (H2) para revisarlos solos |
| 7 | `World/Beach/TN_BeachPropMeshes.h` (3712) | Un `.cpp` por familia de props, con el `.h` reducido a firmas | Menos compilación y ficheros < 800 |
| 8 | `Settings/TN_GameSettingsSubsystem.cpp` (2050) | `_Audio.cpp` (884-1190), `_Video.cpp` (707-880 y 1191-1287), `_Keys.cpp` (1317-1890), núcleo (carga, guardado, tick) | Reasignación de teclas y audio evolucionan por separado |
| 9 | `World/Beach/TN_BeachGullZone.cpp` (2070) | `_Server.cpp` (938-1415), `_Pose.cpp` (593-850 y 1657-1790), `_Visual.cpp` (1487-1656 y 1791-2070) | Separa autoridad y cliente: menos riesgo de lógica de servidor en el visual |
| 10 | `World/ProcMap/TN_ProcFauna.cpp` (2232) | `_Build.cpp` (234-615), `_Sim.cpp` (762-1612), `_Pose.cpp` (1822-2211) | Legibilidad; la simulación se puede perfilar aislada |

Fuera del top: `TN_MusicSynthDSP.h` (2703) y `TN_AmbientSynthDSP.h` (2485) son grandes pero los incluye 1-3 unidades;
partirlos solo mejora la lectura.

## Orden recomendado

1. H1 y L2 (S, protegen la build pública).
2. H2 y M9 (guardado con versión y sin sobrescribir el corrupto) antes de la primera beta con jugadores externos.
3. H4: un cook de prueba y revisar `LoadObject` fallidos; con el resultado, H3 y M11.
4. M10 y L6 con la pasada de localización.
5. Splits 3 y 4 (compilación), luego 1 y 2 (legibilidad).
