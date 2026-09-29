# F — Gaps Steam sobre main 41a0ee8a5 (auditoría de solo lectura, 2026-09-29)

Base de comparación: auditoría 2026-08-18 sobre 2443d4e. Desde entonces: 402 commits (Mokius 179, Rodrigo 206), 434 ficheros de código nuevos. Módulo actual: 597 ficheros, ~242 000 líneas (antes 163 / ~26 500). Nada se ha compilado ni ejecutado en esta auditoría: los estados «cerrado» son de código, no validados en runtime.

## 1. Gaps de la auditoría previa

| # | Gap | Estado | Evidencia | Coste pendiente (h) |
|---|---|---|---|---|
| 1 | SteamDevAppId=480 | **Abierto** | `Config/DefaultEngine.ini:116`, `Config/DefaultGame.ini:10`, `MP_GameInstance.h:435`; además `MP_GameInstance.cpp:1695` filtra salas porque «el 480 lo comparten muchos proyectos» | 4 (código) + trámite Steamworks |
| 2 | Menú de ajustes y audio | **Cerrado** | `TN_SettingsSaveGame.h:17-160` (volúmenes por categoría, sensibilidad ratón/mando, invertir Y, `KeyOverrides` para remapeo, FOV, daltonismo, escala UI, brillo); gráficos vía `UGameUserSettings` (`TN_PauseMenuWidget.cpp`, 12 usos); `TN_GameSettingsSubsystem.cpp:884-967` crea `USoundClass`/`USoundMix` en runtime | 4 (validar) |
| 3 | Localización | **Parcial (avanzado)** | 1175 `LOCTEXT/NSLOCTEXT` frente a 37 `FText::FromString` (15 en `TN_PauseMenuWidget.cpp`, 14 en `TN_TutorialPlayerComponent.cpp:58-62`, nombres de teclas); 13 culturas, `.po` de 1308 entradas completas (ja revisado por muestreo); `Docs/Localizacion.md:259-271` lista lo pendiente; fuentes Noto CJK **no descargadas** (`Content/Slate/Fonts` no existe; `DefaultGame.ini:62-65` las espera) | 8 + LQA nativa (externa) |
| 4 | SaveGame por nick y sin versión | **Parcial** | Nick resuelto: slot fijo `"%s_Local"` (`MP_GameInstance.cpp:1205-1211`). Sin versión: `TN_CosmeticSaveGame.h` y `TN_TutorialSaveGame` sin campo `Version`; solo ajustes lo tienen (`TN_SettingsSaveGame.h:174`). Guardado síncrono (`MP_GameInstance.cpp:1202`); sin configuración de Steam Cloud | 6 |
| 5 | Late join / PreLogin / OnTravelFailure | **Parcial** | PreLogin hecho: `MP_GameInstance.cpp:111` y `:1989-2016` (sala llena, bloqueada, expulsado). Reconexión en carrera: `TN_BeachRaceGameMode.cpp:421-450` (sprint → espectadora; ronda activa → recolocar en salida). **Sin `OnTravelFailure`** (0 coincidencias); solo `OnNetworkFailure` (`:104`, `:1351`) | 4 |
| 6 | Pausa / VOIP sin mute ni PTT | **Cerrado** | `UI/Pause/TN_PauseMenuWidget.cpp`; `TN_SettingsSaveGame.h:44-80` (`MutedPlayers`, volumen por compañero, `bPushToTalk`, `bMicMuted`, dispositivo); `Docs/Menu_Pausa.md` | 0 |
| 7 | Mando / Steam Deck en menús | **Parcial** | Menú principal y ajustes con foco y mando (`MP_MainMenuWidget.cpp:185,325,382`; `Menu_Pausa.md:107`; 82 referencias a gamepad en `Private/UI`). Sin detección de Deck ni teclado virtual (`ShowGamepadTextInput`: 0) para el código de sala; sin glifos por plataforma | 20 |
| 8 | Logros / rich presence | **Abierto** | 0 referencias a `Achievement`, `RichPresence`, `IOnlinePresence`, `SteamUserStats` en `Source/` y `Config/`; solo `bAllowJoinViaPresence` (`MP_GameInstance.cpp:538`) | 20 |
| 9 | Icono / splash | **Abierto** | `Build/` no existe; sin `.ico` ni splash en el repo | 4 + arte |
| 10 | MapsToCook anulado | **Abierto y peor** | `DefaultGame.ini:17-22`: `DirectoriesToAlwaysCook=/Game/Maps` sigue anulando la lista. La lista **no incluye** `LVL_Lobby` (destino de `MP_GameInstance.h:421`, `TN_RunGameMode.h:133`) ni `LVL_BeachRace` (`TN_HQGameMode.h:119`): quitar el directorio sin corregir la lista rompe el juego empaquetado. Se cocinan `LVL_TestMap`, `LVL_LevelMetrics`, `LVL_ProcGenDemo`, `LVL_Demo01` | 3 |
| 11 | build_check.bat | **Abierto** | `build_check.bat:2` apunta a `C:\Users\mokiu\Documents\...` y a Development (no DebugGame) | 1 |
| 12 | Créditos / licencias | **Abierto** | 0 ficheros de créditos/licencias/NOTICE; `Localizacion.md:213-214` exige el texto OFL de Noto en créditos; audio de `Content/Audio` de otros autores sin registro de origen | 8 |
| — | Log versionado | Abierto | `Source/Logs/Tortunabo_2.log` sigue en git | 0,2 |

## 2. Tests, CI y tamaño

- **Automation**: 87 `IMPLEMENT_*_AUTOMATION_TEST` en 22 ficheros de `Private/Tests` (antes 19); 0 specs (`BEGIN_DEFINE_SPEC`). Además 110 `def test_` de Python en `Scripts/tests` (generadores de terreno).
- **CI**: inexistente (sin `.github/`, sin pipeline). Los 87 tests no se ejecutan de forma automática; nadie detecta una regresión de la otra rama.
- **Ficheros > 800 líneas: 88 de 597** (26 por encima de 1500). Los más gruesos: `TN_BeachLayout.h` 4319, `TN_BeachPropMeshes.h` 3712, `TN_PauseMenuWidget.cpp` 3534, `TN_ProcMapGenerator_Build.cpp` 3310, `TN_BeachRaceGameMode.cpp` 3014, `TN_MusicSynthDSP.h` 2703, `TN_AmbientSynthDSP.h` 2485, `TN_ProcFauna.cpp` 2232, `MP_GameInstance.cpp` 2182, `TN_GameSettingsSubsystem.cpp` 2050.
- Olor principal: `MP_GameInstance` concentra sesiones, salas, expulsiones, cosméticos, tutorial y mensajes de estado; `TN_PauseMenuWidget` monta toda la interfaz de ajustes en C++. Son los dos puntos donde chocarán las dos ramas de trabajo.

## 3. Riesgos de netcode en el código integrado (muestra de 10 ficheros)

Muestreados: `TN_BeachGullZone`, `TN_BeachGiantCrab`, `TN_BeachStorm`, `TN_ProcFauna`, `TN_ProcSearchSpot`, `TN_BeachRaceGameMode`, `TN_TutorialCourse`, `TN_SpectatorGhost`, `TN_CarryComponent`, `TN_PauseMenuWidget`, más las cabeceras nuevas con RPC.

Juicio general: **el patrón es sólido**. Los enemigos separan `ServerTick`/`VisualTick` con estado replicado (`TN_BeachEnemy.h:284` `ReplicatedUsing=OnRep_Mover`); de 34 multicast nuevas, 30 son `Unreliable` y solo efectos; los empujones se aplican en servidor y en el cliente dueño (`TN_BeachClamTrap.cpp:871-886`, `TN_BeachMine.cpp:581-588`, `TN_BeachSpadeRamp.cpp:457-477`), que es lo correcto con `CharacterMovementComponent`. Los timers de 14 usos de `TN_BeachRaceGameMode` viven en el GameMode (solo servidor). La espera de ronda tiene tope (`TN_BeachRaceGameMode.cpp:561-583`).

| Id | Riesgo | Evidencia | Severidad |
|---|---|---|---|
| N-A | RPC de depuración de cliente a servidor activa en builds Development: cualquier invitado ejecuta comandos de depuración en el anfitrión | `TN_SpectatorGhost.cpp:485-493` (`#if !UE_BUILD_SHIPPING`) | Media si se distribuye Development (demo, playtest abierto); nula en Shipping |
| N-B | `ServerRegen` sin comprobar quién llama | `TN_TerrainViewGameMode.cpp:24-30` | Baja (solo mapa de visor); desaparece si ese mapa no se cocina |
| N-C | Estado persistente enviado por multicast: la mancha en el caparazón no llega a quien entra o reconecta | `TN_BeachGullZone.cpp:1487-1507` (Reliable), `TN_RaceGullStrike.cpp:569-577` | Baja (cosmético) |
| N-D | 12 Server RPC nuevas sin `WithValidation`; la validación está dentro del `_Implementation` en las muestreadas (`TN_CarryComponent.cpp:160-167` valida distancia), pero `ServerGoToStation` pasa el índice sin comprobar aquí (`TN_TutorialPlayerComponent.cpp:235-240`) | Cabeceras de `TN_CarryComponent`, `TN_TutorialPlayerComponent`, `TN_SpectatorGhost` | Baja (verificar `ATN_TutorialCourse::GoToStation`) |
| N-E | Sin `OnTravelFailure`: un `ServerTravel` fallido (mapa no cocinado, ver gap 10) deja al cliente colgado sin mensaje | búsqueda sin coincidencias; combinado con gap 10 | **Alta** en la build empaquetada |
| N-F | Estado de sala (miembros, bloqueo, expulsados) solo en la `GameInstance` del anfitrión | `MP_GameInstance.cpp:1897`, `:1989` | Baja (aceptable en listen server) |
| N-G | Tres modos de juego vivos (Run cooperativo, ProcMap, BeachRace) con sus GameModes y mapas; la superficie de red a probar se multiplica y el playtest multijugador real sigue sin hacerse | `TN_HQGameMode.h:111-119` | **Alta** (alcance) |

## 4. Top-10 priorizado para acabar el juego

1. **Playtest multijugador real con Steam y build empaquetada (Shipping)**, 4 jugadores, con checklist. Todo el código integrado (~215 000 líneas nuevas) está sin validar en runtime. 16 h por iteración.
2. **AppID propio y Steamworks** (cuota, depósitos, página de tienda, `steam_appid.txt`), y quitar el filtro de salas por 480. La revisión de Valve tiene plazo de semanas: empezar ya. 4 h de código.
3. **Cocinado explícito**: `MapsToCook` con `LVL_Lobby` y `LVL_BeachRace`, fuera `DirectoriesToAlwaysCook=/Game/Maps`, excluir mapas de test y del visor de terreno (resuelve también N-B). 3 h más una build de humo.
4. **`OnTravelFailure`** con vuelta al menú y mensaje localizado. 4 h.
5. **CI mínima**: script `RunUAT BuildCookRun` + `-ExecCmds="Automation RunTests Tortunabo"` en headless, disparado en cada merge a main; arreglar `build_check.bat` con ruta relativa. 12 h. Con dos autores en paralelo es la única red contra regresiones.
6. **Congelar alcance**: decidir qué modos salen en 1.0 (BeachRace, Run, ProcMap) y apartar el resto. Decisión, no horas; condiciona el resto de la lista.
7. **Versionado de guardados + Steam Cloud**: campo `Version` y migración en cosmético y tutorial; guardado asíncrono. 6 h.
8. **Steam Deck y mando**: teclado virtual para el código de sala, glifos, prueba en Deck. 20 h.
9. **Logros y rich presence** (estado «En la carrera, ronda N» y unirse desde la lista de amigos). 20 h.
10. **Cierre legal y de presentación**: pantalla de créditos con licencias (Noto OFL, audio de terceros), descargar las fuentes CJK, icono y splash en `Build/Windows`, quitar `Source/Logs` del repo. 14 h.

Deuda sin prioridad de lanzamiento: partir `MP_GameInstance` (sesiones / salas / perfil) y `TN_PauseMenuWidget` (una página por fichero) antes de que otro merge grande los toque.

Total estimado de lo pendiente (sin arte ni LQA externa): ~140 h de código y configuración, más las iteraciones de playtest.
