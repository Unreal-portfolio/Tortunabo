# Tortunabo - Coop Foundation

Base C++ de cooperativo para UE 5.6 con flujo completo:
`Menu/Host -> HQ Lobby (ready zone) -> Countdown -> Cinematic -> Run Map -> Finish/Spectate -> Results -> Return HQ`.

## Estructura escalable (módulo `Tortunabo`)
- `Source/Tortunabo/Public/Core`: estado replicado compartido (`TN_CoopGameState`, `TN_CoopPlayerState`, `TN_MatchFlowTypes`)
- `Source/Tortunabo/Public/Lobby`: lógica de cuartel y ready zone (`TN_HQGameMode`, `TN_LobbyReadyZone`)
- `Source/Tortunabo/Public/Game`: lógica de partida (`TN_RunGameMode`)
- `Source/Tortunabo/Public/Menu`: GameMode/Controller del menú (`MP_MenuGameMode`, `MP_MenuPlayerController`)
- `Source/Tortunabo/Public/Multiplayer`: lifecycle de sesiones Steam (`MP_GameInstance`)
- `Source/Tortunabo/Public/Player`: personaje y controller de partida (`TortugaCharacter`, `MP_GamePlayerController`)
- `Source/Tortunabo/Public/UI/*`: widgets de menú y VOIP (`MP_MainMenuWidget`, `VoiceIndicatorWidget`)
- `Source/Tortunabo/Public/Voice`: componente de voz por proximidad (`ProximityVoiceComponent`)
- `Source/Tortunabo/Public/World`: volúmenes de mundo (`TN_FinishLineVolume`)
- `Source/Tortunabo/Private/*`: implementaciones por dominio

## Limpieza aplicada
- Se eliminaron artefactos regenerables y carpetas legacy del workspace: `Binaries/`, `DerivedDataCache/`, `Intermediate/`, `Saved/`, `oldfiles/`, `.vs/`, `.idea/`.
- Se añadió `.gitignore` con exclusiones de Unreal/IDE para mantener el repo limpio.

## Qué está funcional ya en C++
- Sesiones Steam + host/join con `MP_GameInstance`
- Lobby HQ server-authoritative con ready por zona
- Countdown 3..2..1 cancelable si alguien sale de la zona
- Ventana `Cinematic` antes del travel (controlada por `TN_HQGameMode::CinematicDelaySeconds`)
- Travel a mapa de partida con `TN_RunGameMode`
- Spawn aleatorio sobre `PlayerStart`
- Meta por volumen y paso a espectador de quien llega
- Cambio de espectador con rueda de ratón / `PageUp` / `PageDown`
- Al llegar todos: estado de resultados y retorno automático al HQ

## Setup mínimo en Editor (1 vez)
1. Crea mapas:
   - `/Game/Maps/Lobby/LVL_HQ`
   - `/Game/Maps/Run/LVL_Run`
2. En `LVL_HQ`:
   - World Settings -> GameMode Override: `TN_HQGameMode`
   - Coloca un `TN_LobbyReadyZone` en la "tienda militar" (zona ready)
3. En `LVL_Run`:
   - World Settings -> GameMode Override: `TN_RunGameMode`
   - Coloca varios `PlayerStart`
   - Coloca un `TN_FinishLineVolume` en el mar/meta
4. En `TN_HQGameMode` ajusta `MatchMapPath` a `/Game/Maps/Run/LVL_Run`
5. En `TN_RunGameMode` ajusta `LobbyMapPath` a `/Game/Maps/Lobby/LVL_HQ`

## Comandos de compilación
```powershell
& "C:\Program Files\Epic Games\UE_5.6\Engine\Build\BatchFiles\Build.bat" TortunaboEditor Win64 Development "C:\Users\mokiu\Documents\Unreal Projects\Tortunabo\Tortunabo.uproject" -WaitMutex -NoHotReload
```

## Próximo paso recomendado
- Crear widgets UMG para countdown gigante y resultados finales, enlazados a `TN_CoopGameState::MatchFlowState`, `CountdownValue`, `FinishedPlayers` y datos de `TN_CoopPlayerState`.

