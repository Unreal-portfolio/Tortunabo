# Guia de montaje inicial (Editor + Steam)

Esta guia deja el proyecto funcional de punta a punta: menu, jugador tercera persona, flujo coop, controles, VOIP, y build para pruebas por Steam.

## Checklist rapido

- [ ] Confirmar configuracion base del proyecto (GameInstance, OnlineSubsystem, AppID Steam).
- [ ] Crear/configurar mapa de menu y su widget.
- [ ] Crear/configurar mapa de lobby (`LVL_HQ`) y zona ready.
- [ ] Crear/configurar mapa de carrera (`LVL_Run`) y meta.
- [ ] Preparar personaje jugable (mesh/anim/camara 3a persona).
- [ ] Verificar controles de gameplay y espectador.
- [ ] Verificar UI minima (menu, voz, estados de partida).
- [ ] Probar flujo completo en Standalone con Steam abierto.
- [ ] Empaquetar build Development y validar host/join/invite con compis.

---

## 1) Configuracion base del proyecto

Fuente de verdad en config/codigo:
- `Config/DefaultEngine.ini`
- `Config/DefaultGame.ini`
- `Source/Tortunabo/Public/Multiplayer/MP_GameInstance.h`
- `Source/Tortunabo/Private/Multiplayer/MP_GameInstance.cpp`

### 1.1 Ajustes globales

En `Project Settings > Maps & Modes` verifica:
- `Game Instance Class` = `MP_GameInstance`
- `Default GameMode` = `TN_HQGameMode`

### 1.2 Steam para testing

Ya queda preparado en `Config/DefaultGame.ini`:
- `[/Script/Tortunabo.MP_GameInstance]`
- `SteamDevAppId=480`

`MP_GameInstance` genera `steam_appid.txt` automaticamente en builds no-shipping al iniciar (intenta en `Binaries/Win64` y usa raiz como fallback).

> Si en el futuro os dan AppID propio, solo cambia `SteamDevAppId` en `DefaultGame.ini`.

---

## 2) Menu principal (mapa + GameMode + Widget)

Codigo implicado:
- `Source/Tortunabo/Public/Menu/MP_MenuGameMode.h`
- `Source/Tortunabo/Public/Menu/MP_MenuPlayerController.h`
- `Source/Tortunabo/Public/UI/Menu/MP_MainMenuWidget.h`

### 2.1 Crear mapa de menu

- Crea `Content/Maps/Lobby/LVL_Menu` (o similar).
- En `World Settings` de ese mapa, pon `GameMode Override` a una clase basada en `MP_MenuGameMode`.

### 2.2 Crear Blueprint de PlayerController de menu

Como `MainMenuWidgetClass` es `EditDefaultsOnly`, lo normal es:
1. Crear `BP_MenuPlayerController` hijo de `MP_MenuPlayerController`.
2. En Class Defaults, asignar `MainMenuWidgetClass` a tu widget de menu.
3. En `BP_MenuGameMode` (hijo de `MP_MenuGameMode`), asignar `PlayerControllerClass = BP_MenuPlayerController`.
4. Usar `BP_MenuGameMode` como override del mapa menu.

### 2.3 Crear widget de menu (obligatorio para botones)

Crea `WBP_MainMenu` basado en `MP_MainMenuWidget` con estos nombres exactos:
- `HostButton` (`Button`)  [BindWidget]
- `FindButton` (`Button`)  [BindWidget]
- `QuitButton` (`Button`)  [BindWidget]
- `StatusText` (`TextBlock`) [BindWidgetOptional]

Si cambian los nombres, no se enlazan los callbacks C++.

### 2.4 Flujo esperado del menu

- `HostButton` -> `HostSession()`
- `FindButton` -> `FindAndJoinSession()`
- `QuitButton` -> cerrar juego
- `StatusText` muestra log de estado emitido por `MP_GameInstance`.

---

## 3) Mapa de lobby (`LVL_HQ`)

Codigo implicado:
- `Source/Tortunabo/Public/Lobby/TN_HQGameMode.h`
- `Source/Tortunabo/Public/Lobby/TN_LobbyReadyZone.h`

### 3.1 World Settings

En `LVL_HQ`:
- `GameMode Override` = `TN_HQGameMode` (o BP derivado).

### 3.2 Colocar zona ready

- Arrastra un `TN_LobbyReadyZone` al mapa.
- Ajusta escala/colision para cubrir la zona donde los jugadores se "preparan".
- El actor marca ready/unready por overlap server-authoritative.

### 3.3 Ajustes de flujo en GameMode

En defaults de `TN_HQGameMode`:
- `LobbyExpectedPlayers` (ej. 4)
- `CountdownStartValue` (ej. 3)
- `CinematicDelaySeconds` (ventana entre countdown y travel)
- `MatchMapPath` = `/Game/Maps/Run/LVL_Run`

---

## 4) Mapa de carrera (`LVL_Run`)

Codigo implicado:
- `Source/Tortunabo/Public/Game/TN_RunGameMode.h`
- `Source/Tortunabo/Public/World/TN_FinishLineVolume.h`

### 4.1 World Settings

En `LVL_Run`:
- `GameMode Override` = `TN_RunGameMode` (o BP derivado).

### 4.2 Actores obligatorios

- Varios `PlayerStart` (el spawn es aleatorio).
- Un `TN_FinishLineVolume` en la meta.

### 4.3 Ajustes de retorno

En defaults de `TN_RunGameMode`:
- `LobbyMapPath` = `/Game/Maps/Lobby/LVL_HQ`
- `ResultsDurationSeconds` (duracion de resultados antes de volver).

---

## 5) Jugador (tercera persona estilo Fall Guys)

Codigo implicado:
- `Source/Tortunabo/Public/Player/TortugaCharacter.h`
- `Source/Tortunabo/Private/Player/TortugaCharacter.cpp`

### 5.1 Camara y movimiento

Ya viene configurado en C++ como tercera persona:
- spring arm + follow camera
- giro orientado a movimiento
- control por camara del jugador

### 5.2 Que debes montar en Editor

En un BP derivado de `TortugaCharacter` (recomendado):
- Asignar `Skeletal Mesh` de tortuga.
- Asignar `Anim Blueprint`.
- Ajustar capsule/mesh offset si hiciera falta.
- Validar que la camara no clippea con obstaculos (longitud del boom y colision).

> Si no asignas mesh/anim, el gameplay funciona pero se vera placeholder/sin animacion final.

---

## 6) Controles (gameplay y espectador)

### 6.1 Controles de gameplay (definidos en C++)

`TortugaCharacter` crea el mapping en runtime:
- Mover: `W A S D` + `Gamepad Left Stick`
- Mirar: `Mouse` + `Gamepad Right Stick`
- Saltar: `Space`

No depende de `InputAction` assets para esos binds base.

### 6.2 Controles de espectador

En `MP_GamePlayerController`:
- Siguiente jugador: `Mouse Wheel Up` o `PageDown`
- Anterior jugador: `Mouse Wheel Down` o `PageUp`

---

## 7) UI y VOIP (minimo funcional)

Codigo implicado:
- `Source/Tortunabo/Public/Voice/ProximityVoiceComponent.h`
- `Source/Tortunabo/Public/UI/Voice/VoiceIndicatorWidget.h`
- `Source/Tortunabo/Public/Player/MP_GamePlayerController.h`

### 7.1 Indicador de voz

Opcion recomendada:
1. Crear `WBP_VoiceIndicator` basado en `VoiceIndicatorWidget`.
2. Incluir `Image` llamada exactamente `SpeakerIcon`.
3. Guardarlo en `/Game/UI/WBP_VoiceIndicator` (ruta esperada por default en el componente).

### 7.2 VOIP por proximidad

`ProximityVoiceComponent` se anade al pawn en `OnPossess` del player controller si no existe.

Ajustes utiles del componente:
- `InnerRadius`
- `OuterRadius`
- `SpeakingThreshold`
- `VoiceGain`
- `PlaybackVolume`

---

## 8) Datos para UMG de flujo coop

Variables replicadas listas para UI:
- `ATN_CoopGameState::MatchFlowState`
- `ATN_CoopGameState::ReadyPlayers`
- `ATN_CoopGameState::ExpectedPlayers`
- `ATN_CoopGameState::CountdownValue`
- `ATN_CoopGameState::FinishedPlayers`
- `ATN_CoopPlayerState::FinishRank`
- `ATN_CoopPlayerState::FinishTimeSeconds`

Con esto montas HUD de countdown, progreso y resultados.

---

## 9) Pruebas correctas en editor

- Para Steam real, usa `Standalone Game` (PIE puede usar subsistema NULL).
- Steam debe estar abierto en cada cliente.
- Para pruebas reales con compis: cuentas Steam distintas y preferiblemente maquinas distintas.

### Smoke test minimo

1. Host abre menu y pulsa `Host`.
2. Joiner entra por `Find` o invitacion.
3. Todos entran/salen ready zone (countdown cancela/reinicia bien).
4. Pasa por `Cinematic` y hace travel a run.
5. Todos cruzan meta, entran en spectate y vuelve al lobby tras resultados.

---

## 10) Build y packaging para compartir

### 10.1 Compilar editor

```powershell
& "C:\Program Files\Epic Games\UE_5.6\Engine\Build\BatchFiles\Build.bat" TortunaboEditor Win64 Development "C:\Users\mokiu\Documents\Unreal Projects\Tortunabo\Tortunabo.uproject" -WaitMutex -NoHotReload
```

### 10.2 Compilar juego

```powershell
& "C:\Program Files\Epic Games\UE_5.6\Engine\Build\BatchFiles\Build.bat" Tortunabo Win64 Development "C:\Users\mokiu\Documents\Unreal Projects\Tortunabo\Tortunabo.uproject" -WaitMutex -NoHotReload
```

### 10.3 Empaquetar

- `Platforms > Windows > Package Project`.
- Usa `Development` para QA con amigos.
- Comparte carpeta empaquetada completa.

---

## 11) Problemas tipicos y solucion rapida

- **No encuentra sesiones**: comprobar Steam abierto + Standalone + mismo AppID (480 en test).
- **Menu sin botones funcionales**: revisar nombres `HostButton/FindButton/QuitButton` en `WBP_MainMenu`.
- **No aparece icono de voz**: revisar ruta `/Game/UI/WBP_VoiceIndicator` y `SpeakerIcon`.
- **No empieza countdown**: confirmar `LobbyExpectedPlayers` y overlaps del `TN_LobbyReadyZone`.
- **No vuelve al lobby**: confirmar `LobbyMapPath` en `TN_RunGameMode`.

---

## 12) Orden recomendado de montaje (si empiezas de cero)

1. Configuracion base de proyecto + Steam.
2. Mapas (`LVL_Menu`, `LVL_HQ`, `LVL_Run`).
3. GameModes/Controllers por mapa.
4. Widget menu y hooks de host/join.
5. Jugador (mesh/anim/camara).
6. Ready zone y finish volume.
7. UI de voz y HUD de estado.
8. Smoke test completo en Standalone.
9. Packaging Development y test con compis.

