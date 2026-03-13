# AGENTS Guide for Tortunabo

## Snapshot del proyecto (UE 5.6)
- Módulo runtime único: `Tortunabo` (`Tortunabo.uproject`).
- Entry point de módulo: `Source/Tortunabo/Tortunabo.cpp` con `IMPLEMENT_PRIMARY_GAME_MODULE`.
- Targets estándar: `Source/Tortunabo.Target.cs` y `Source/TortunaboEditor.Target.cs`.
- Flujo cooperativo actual en C++: `Menu -> HQ Lobby -> Countdown -> Cinematic -> Run -> Finish/Spectate -> Results -> Return HQ` (ver `Source/Tortunabo/Public/Core/TN_MatchFlowTypes.h` y `Source/Tortunabo/Private/Lobby/TN_HQGameMode.cpp`).

## Arquitectura por dominios (Source/Tortunabo)
- `Public/Core`, `Private/Core`: estado replicado y tipos de flujo (`TN_CoopGameState`, `TN_CoopPlayerState`, `TN_MatchFlowTypes`).
- `Public/Lobby`, `Private/Lobby`: cuartel general y zona ready (`TN_HQGameMode`, `TN_LobbyReadyZone`).
- `Public/Game`, `Private/Game`: reglas de la carrera (`TN_RunGameMode`).
- `Public/World`, `Private/World`: volúmenes de mundo (`TN_FinishLineVolume`).
- `Public/Player`, `Private/Player`: personaje/control de jugador (`TortugaCharacter`, `MP_GamePlayerController`).
- `Public/Multiplayer`, `Private/Multiplayer`: sesiones Steam y lifecycle de sesión (`MP_GameInstance`).
- `Public/Menu`, `Private/Menu` y `Public/UI/*`, `Private/UI/*`: menú principal e HUDs (`MP_MainMenuWidget`, `VoiceIndicatorWidget`).
- `Public/Voice`, `Private/Voice`: VOIP por proximidad (`ProximityVoiceComponent`).

## Configuración crítica (fuente de verdad)
- `Config/DefaultEngine.ini`:
  - `GameInstanceClass=/Script/Tortunabo.MP_GameInstance`
  - `GlobalDefaultGameMode=/Script/Tortunabo.TN_HQGameMode`
  - NetDriver SteamSockets y `OnlineSubsystemSteam` con `SteamDevAppId=480`.
- `Config/DefaultInput.ini`: backend Enhanced Input (`EnhancedPlayerInput`, `EnhancedInputComponent`).
- `Config/DefaultGame.ini`: cook de `/Game/Maps`, `/Game/UI`, `/Game/Input`.

## Workflows prácticos
- Compilar editor (ruta del README):
  - `Build.bat TortunaboEditor Win64 Development <Tortunabo.uproject> -WaitMutex -NoHotReload`
- Si cambias clases/módulos C++, regenera project files desde `Tortunabo.uproject` antes de usar `Tortunabo.sln`.
- El paso `Countdown -> Cinematic -> Travel` depende de `CinematicDelaySeconds` en `TN_HQGameMode`; valida ese delay en smoke tests de flujo.
- Validación funcional principal: probar en Editor/Standalone (PIE no siempre refleja Steam real).
- No hay suite de tests automatizados en repo; validar con smoke tests de flujo completo.

## Convenciones para cambios
- Mantener clases nuevas dentro de su dominio (`Core`, `Lobby`, `Game`, `Player`, `UI`, `Voice`, `Multiplayer`, `World`).
- En `.cpp`, incluir headers con ruta de módulo (`#include "Voice/ProximityVoiceComponent.h"`, `#include "UI/Menu/MP_MainMenuWidget.h"`).
- Evitar lógica persistente en carpetas generadas; son regenerables: `Binaries/`, `DerivedDataCache/`, `Intermediate/`, `Saved/`.
- Integraciones activas en `Source/Tortunabo/Tortunabo.Build.cs`: `OnlineSubsystemSteam` (dinámico) y stack VOIP/audio (`AudioCapture`, `AudioCaptureCore`, `AudioMixer`, `SignalProcessing`); si tocas sesión/voz, verifica que sigan declaradas.
- Si agregas dependencias nuevas (UI avanzada, networking extra, online), actualiza primero `Source/Tortunabo/Tortunabo.Build.cs`.

## Estructura de Content recomendada
- Base creada para escalar: `Content/Maps/{Lobby,Run}`, `Content/UI/{Menu,HUD,Results}`, `Content/Input`, `Content/Blueprints/{Characters,Gameplay}`, `Content/Characters/Turtles`, `Content/Audio/Voice`.
- Mantener assets del flujo coop en esas carpetas para evitar acoplamiento y facilitar cook/package.
