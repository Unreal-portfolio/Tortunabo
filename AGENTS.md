# AGENTS Guide for Tortunabo

## Snapshot del proyecto (UE 5.6)
- Módulo runtime único: `Tortunabo` (`Tortunabo.uproject`).
- Entry point de módulo: `Source/Tortunabo/Tortunabo.cpp` con `IMPLEMENT_PRIMARY_GAME_MODULE`.
- Targets estándar: `Source/Tortunabo.Target.cs` y `Source/TortunaboEditor.Target.cs`.
- Flujo cooperativo actual en C++: `Menu -> HQ Lobby -> Countdown -> Cinematic -> Run -> Finish/Spectate -> Results -> Return HQ` (ver `Source/Tortunabo/Public/Core/TN_MatchFlowTypes.h` y `Source/Tortunabo/Private/Lobby/TN_HQGameMode.cpp`).
- **Sistema de perspectiva**: Tercera persona al estilo Fall Guys (camera boom + follow camera en TortugaCharacter). Momentos puntuales en primera persona solo en cinemáticas.

## Arquitectura por dominios (Source/Tortunabo)
- `Public/Core`, `Private/Core`: estado replicado y tipos de flujo (`TN_CoopGameState`, `TN_CoopPlayerState`, `TN_MatchFlowTypes`).
- `Public/Lobby`, `Private/Lobby`: cuartel general y zona ready (`TN_HQGameMode`, `TN_LobbyReadyZone` con trigger volume).
- `Public/Game`, `Private/Game`: reglas de la carrera (`TN_RunGameMode`).
- `Public/World`, `Private/World`: volúmenes de mundo (`TN_FinishLineVolume`).
- `Public/Player`, `Private/Player`: personaje/control de jugador (`TortugaCharacter` con Enhanced Input, `MP_GamePlayerController`).
- `Public/Multiplayer`, `Private/Multiplayer`: sesiones Steam y lifecycle de sesión (`MP_GameInstance`).
- `Public/Menu`, `Private/Menu` y `Public/UI/*`, `Private/UI/*`: menú principal e HUDs (`MP_MainMenuWidget`, `TN_CoopFlowHUDWidget` con countdown display).
- `Public/Voice`, `Private/Voice`: VOIP por proximidad (`ProximityVoiceComponent`).

## Configuración crítica (fuente de verdad)
- `Config/DefaultEngine.ini`:
  - `GameInstanceClass=/Script/Tortunabo.MP_GameInstance`
  - `GlobalDefaultGameMode=/Script/Tortunabo.TN_HQGameMode`
  - NetDriver SteamSockets y `OnlineSubsystemSteam` con `SteamDevAppId=480` (ID de prueba pública; cambiar antes de release).
- `Config/DefaultInput.ini`: backend Enhanced Input (`EnhancedPlayerInput`, `EnhancedInputComponent`).
- `Config/DefaultGame.ini`: cook de `/Game/Maps`, `/Game/UI`, `/Game/Input`.

## Sistema de Countdown (IMPLEMENTADO)
- **Estado central**: `ATN_CoopGameState::CountdownValue` (replicado, se actualiza cada 1 segundo).
- **Lógica**: `ATN_HQGameMode::StartCountdown()` → `TickCountdown()` (valida que todos están ready) → decrementa valor.
- **Reset automático**: Si alguien sale de `TN_LobbyReadyZone` o disconnecta, `RefreshLobbyState()` llama `ResetCountdown()`.
- **UI**: `UTN_CoopFlowHUDWidget` muestra "La partida empieza en: X" durante estado `Countdown`. Bindings automáticos a GameState.
- **Transición**: Cuando `CountdownValue <= 0`, viaja a Run map con delay de `CinematicDelaySeconds` (editable en HQGameMode).

## Sistema de Input (Enhanced Input Completo)
- **IMC**: `/Game/Input/IMC_Player` (InputMappingContext) - carga automáticamente en `TortugaCharacter::BeginPlay()`.
- **InputActions requeridas**: Todas en `/Game/Input/`:
  - `IA_Move` (Axis2D): WASD + Gamepad_LeftThumbstick
  - `IA_Look` (Axis2D): Mouse + Gamepad_RightThumbstick
  - `IA_Jump` (Digital): Space + Gamepad_FaceButton_Bottom
  - `IA_Interact` (Digital): E + Gamepad_FaceButton_Top
  - `IA_RotateInventory` (Digital): Q/R + Gamepad_FaceButton_Left
- **Bind**: En `SetupPlayerInputComponent()`, cada acción se bindea a método del pawn (Move, Look, TryInteract, etc.).
- **Scope**: Funciona en Lobby Y Run (no restricción de GameMode).
- **Si assets faltan**: Logs ERROR en Output Log; juego funciona pero sin inputs. Ver `GUIA_MONTAJE_EDITOR.md`.

## Bugs Arreglados (Marzo 2026)
### 1. ProximityVoiceComponent EndPlay Crash
- **Problema**: Destrucción de FAudioCaptureSynth, UAudioComponent, USoundWaveProcedural sin orden correcto + posible double-cleanup.
- **Solución**: Consolidar `EndPlay()`, `OnUnregister()`, `BeginDestroy()` con guard `bIsShuttingDown`. `CleanupRuntimeResources()` ahora:
  1. Flag shutdown temprano
  2. Stop capture
  3. Lock + reset buffer
  4. Stop audio component antes unregister
  5. Reset sound wave
  6. Remove widget
  - Previene dangling pointers y race conditions de threading de audio.

### 2. NewObject sin Factory en MP_GamePlayerController::OnPossess()
- **Problema**: `NewObject<UProximityVoiceComponent>(InPawn, TEXT("ProximityVoice"))` viola requirement de UE 5.6 (NewObject requiere nombre válido + factory context).
- **Solución**: Usar `NewObject<T>(Outer, StaticClass(), FName(...))` con FObjectInitializer implícito. Luego `SetupAttachment()`, `RegisterComponent()`, `BeginPlay()`.

### 3. Input No Funciona en Lobby
- **Problema**: TortugaCharacter solo se configuraba si estaba en Run; logs oscuros sobre qué fallaba.
- **Solución**: `ApplyInputMappingIfLocal()` sin restricción GameMode. Logging mejorado: si IMC es null, error claro "Missing /Game/Input/IMC_Player".

## Workflows prácticos
- **Compilar editor**:
  - `Build.bat TortunaboEditor Win64 Development Tortunabo.uproject -WaitMutex -NoHotReload`
- **Si cambias clases/módulos C++**, regenera project files:
  - Clic derecho en `Tortunabo.uproject` → Generate Visual Studio project files
  - Rebuild en VS, abre Unreal Editor de nuevo
- **Montaje inicial del editor**: Ver `GUIA_MONTAJE_EDITOR.md` (nueva)
  - Crear InputActions en `/Game/Input/`
  - Colocar TN_LobbyReadyZone en mapa Lobby
  - Configurar GameMode override en World Settings
  - Validar collision del trigger volume
- **Smoke tests recomendados**:
  1. PIE Local (Num Players: 4) → Todos a ReadyZone → Countdown → Travel
  2. Standalone Build → Host en PC1, Join en PC2 → Flujo completo
  3. Audio: Standalone (NO PIE), 2 máquinas, habla en micrófono → escucha en otra
  4. Desconexión mid-countdown: Alguien se desconecta → Countdown debe resetear
- **Validación de replication**: Debug overlay muestra `MatchFlowState`, `CountdownValue`, `ReadyPlayers` en cada cliente. Todos deben coincidir.

## Convenciones para cambios
- Mantener clases nuevas dentro de su dominio (`Core`, `Lobby`, `Game`, `Player`, `UI`, `Voice`, `Multiplayer`, `World`).
- En `.cpp`, incluir headers con ruta de módulo (`#include "Voice/ProximityVoiceComponent.h"`, `#include "UI/HUD/TN_CoopFlowHUDWidget.h"`).
- Evitar lógica persistente en carpetas generadas; son regenerables: `Binaries/`, `DerivedDataCache/`, `Intermediate/`, `Saved/`.
- Integraciones activas en `Source/Tortunabo/Tortunabo.Build.cs`: `OnlineSubsystemSteam` (dinámico), `EnhancedInput` y stack VOIP/audio (`AudioCapture`, `AudioCaptureCore`, `AudioMixer`, `SignalProcessing`); si tocas sesión/voz/input, verifica que sigan declaradas.
- Si agregas dependencias nuevas (UI avanzada, networking extra, online), actualiza primero `Source/Tortunabo/Tortunabo.Build.cs`.
- Logging: Prefijo `[Dominio]` para categorizar (ej: `[Input]`, `[Lobby]`, `[Voice]`, `[Audio]`).

## Estructura de Content recomendada
- Base creada para escalar: `Content/Maps/{Lobby,Run}`, `Content/UI/{Menu,HUD,Results}`, `Content/Input`, `Content/Blueprints/{Characters,Gameplay}`, `Content/Characters/Turtles`, `Content/Audio/Voice`.
- Mantener assets del flujo coop en esas carpetas para evitar acoplamiento y facilitar cook/package.
- **Nuevos ficheros generados**: 
  - `GUIA_MONTAJE_EDITOR.md`: Paso a paso completo para configurar editor (inputs, lobby, countdown UI, voice).
  - `Public/Player/TN_InputSetupSubsystem.h/cpp`: Subsistema para verificación/creación de InputActions en runtime (USE CASE: si los assets no existen en desarrollo temprano).

## Notas Importantes
- **Steam AppId=480**: Es el ID público de prueba (Half-Life 2 App). Permite testing sin Steam account. CAMBIAR a AppId real antes de publicar.
- **Countdown lógica**: Funciona con cualquier número de jugadores (configurable `LobbyExpectedPlayers` en HQGameMode editor). Para 1 jugador, countdown inicia inmediatamente. Para 4, espera a que todos estén ready.
- **Perspectiva**: TortugaCharacter siempre crea una CameraBoom (tercera persona). NO hay opción de FPS en Lobby; la FPS es solo para cinemáticas (diferente sistema, no implementado aún en código).
- **Audio en tiempo real**: ProximityVoiceComponent usa threading (FAudioCaptureSynth). Destrucción requiere ser cuidadosa (ya fixed). Distancia max configurable: `OuterRadius` (default 3000 units).
- **Ready Zone**: Volumen trigger (ATriggerBox). Debe estar en mapa Lobby. Automáticamente notifica al HQGameMode cuando player entra/sale.

