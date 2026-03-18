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
- `LobbyExpectedPlayers` (ej. 4) — maximo de sala para display, pero el countdown **arranca cuando todos los conectados estan en zona**, no cuando se llega al maximo.
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

## 6) Controles (Input Assets + gameplay + espectador)

### 6.1 Assets de Input obligatorios (Enhanced Input)

`TortugaCharacter` carga los assets por **soft-reference** a rutas especificas en `/Game/Input/`. Si no existen, el personaje NO recibe input y el log muestra warning.

**Debes crear estos assets en el Editor** (Content Browser → Input):

| Asset | Tipo | Ruta | Configuracion |
|---|---|---|---|
| `IMC_Player` | Input Mapping Context | `/Game/Input/IMC_Player` | Contiene los mappings de todas las acciones de abajo |
| `IA_Move` | Input Action | `/Game/Input/IA_Move` | Value Type = `Axis2D (Vector2D)` |
| `IA_Look` | Input Action | `/Game/Input/IA_Look` | Value Type = `Axis2D (Vector2D)` |
| `IA_Jump` | Input Action | `/Game/Input/IA_Jump` | Value Type = `Digital (Bool)` |
| `IA_Interact` | Input Action | `/Game/Input/IA_Interact` | Value Type = `Digital (Bool)` |
| `IA_Sprint` | Input Action | `/Game/Input/IA_Sprint` | Value Type = `Digital (Bool)` |
| `IA_RotateInventory` | Input Action | `/Game/Input/IA_RotateInventory` | Value Type = `Digital (Bool)` |
| `IA_DropItem` | Input Action | `/Game/Input/IA_DropItem` | Value Type = `Digital (Bool)` |

### 6.2 Configurar IMC_Player (mappings)

Abre `IMC_Player` y añade mappings para cada accion:

| Accion | Tecla sugerida | Modificadores |
|---|---|---|
| `IA_Move` | `W/A/S/D` | Añadir modifier `Swizzle Input Axis Values` en A/D para mapear a X; modifier `Negate` en S y A |
| `IA_Move` | `Gamepad Left Stick 2D` | (ninguno) |
| `IA_Look` | `Mouse XY 2D Axis` | Modifier `Negate` en Y si quieres invertir pitch |
| `IA_Look` | `Gamepad Right Stick 2D` | (ninguno) |
| `IA_Jump` | `Space Bar` | (ninguno) |
| `IA_Jump` | `Gamepad Face Button Bottom` | (ninguno) |
| `IA_Interact` | `E` | (ninguno) |
| `IA_Sprint` | `Left Shift` | (ninguno) |
| `IA_RotateInventory` | `Q` | (ninguno) |
| `IA_DropItem` | `G` | (ninguno) |

> **Importante**: Para WASD, cada tecla se mapea a `IA_Move` por separado. W = Y+1, S = Y-1 (Negate), D = X+1 (Swizzle), A = X-1 (Swizzle + Negate).

### 6.3 Controles de espectador (hardcoded en C++)

En `MP_GamePlayerController`, ya estan bindeados:
- Siguiente jugador: `Mouse Wheel Up` o `PageDown`
- Anterior jugador: `Mouse Wheel Down` o `PageUp`

---

## 7) UI completa — todos los widgets que debes crear

**Los widgets ya NO usan rutas hardcodeadas.** Puedes guardarlos donde quieras. Los asignas en el editor directamente en el Class Defaults del BP derivado del PlayerController.

### 7.0 Patron obligatorio: BP_GamePlayerController

Todos los widgets del gameplay los gestiona `MP_GamePlayerController`. Como la clase C++ no tiene defaults de ruta, **debes crear un BP derivado** y asignarle los widgets ahi:

1. Content Browser → click derecho → Blueprint Class → parent `MP_GamePlayerController`
2. Nombralo `BP_GamePlayerController` (donde quieras en Content)
3. Abrelo → Class Defaults y asigna:
   - `CoopFlowWidgetClass` → tu `WBP_CoopFlowHUD`
   - `VoiceIndicatorWidgetClass` → tu `WBP_VoiceIndicator` (opcional)
   - `CosmeticsWidgetClass` → tu `WBP_CosmeticsMenu` (futuro, deja en blanco)
4. Guarda el BP

Luego en `BP_HQGameMode` (o en `TN_HQGameMode` Class Defaults si usas la clase C++ directa):
- `PlayerControllerClass` = `BP_GamePlayerController`

> Si no asignas `CoopFlowWidgetClass`, el HUD funciona igual con la clase C++ de fallback (texto blanco sin diseño), lo cual es suficiente para probar la logica.

---

### 7.1 `WBP_MainMenu` — Menu principal (ya lo tienes)

- Clase padre: `MP_MainMenuWidget`
- Hijos **obligatorios** (nombres exactos): `HostButton` (Button), `FindButton` (Button), `QuitButton` (Button)
- Hijo opcional: `StatusText` (TextBlock)
- Se asigna en: `BP_MenuPlayerController` → `MainMenuWidgetClass`

---

### 7.2 `WBP_CoopFlowHUD` — HUD de flujo (countdown, carrera, resultados)

- Clase padre: `TN_CoopFlowHUDWidget`
- Guarda en: donde quieras (ej. `/Game/UI/HUD/`)
- Hijos **opcionales** (pero recomendados para que se vea):
  - `RootContainer` (VerticalBox) — contenedor raiz
  - `PrimaryText` (TextBlock) — linea principal
  - `SecondaryText` (TextBlock) — linea secundaria
- Se asigna en: `BP_GamePlayerController` → `CoopFlowWidgetClass`

Que muestra en cada estado del flujo:
- **WaitingForPlayers**: `Sala: 2/4 | Zona: 1/2`
- **Countdown**: `Todos listos! Empieza en: 3`
- **Cinematic**: `Preparando...`
- **InProgress**: `Carrera en curso. Meta: 1/2`
- **Results**: `Volviendo al lobby en: 8`

> **No necesitas nada en Event Graph.** La logica la maneja el C++.

---

### 7.3 `WBP_VoiceIndicator` — Icono de microfono (opcional)

- Clase padre: `VoiceIndicatorWidget`
- Hijo **obligatorio**: `SpeakerIcon` (Image)
- Se asigna en: `BP_GamePlayerController` → `VoiceIndicatorWidgetClass`

---

### 7.4 `WBP_LoadingScreen` — Pantalla de carga (opcional)

- Clase padre: `TN_LoadingScreenWidget`
- Hijos opcionales: `RootOverlay` (Overlay), `StatusText` (TextBlock)
- Se asigna en: `BP_GameInstance` (hijo de `MP_GameInstance`) → `LoadingScreenWidgetClass`
- Si no lo asignas, usa la clase C++ base (pantalla de carga minima funcional)

---

### 7.5 Tabla resumen

| Widget | Clase padre | Se asigna en | Obligatorio |
|---|---|---|---|
| `WBP_MainMenu` | `MP_MainMenuWidget` | `BP_MenuPlayerController.MainMenuWidgetClass` | **Si** |
| `WBP_CoopFlowHUD` | `TN_CoopFlowHUDWidget` | `BP_GamePlayerController.CoopFlowWidgetClass` | Recomendado |
| `WBP_VoiceIndicator` | `VoiceIndicatorWidget` | `BP_GamePlayerController.VoiceIndicatorWidgetClass` | Opcional |
| `WBP_LoadingScreen` | `TN_LoadingScreenWidget` | `BP_GameInstance.LoadingScreenWidgetClass` | Opcional |
| `WBP_CosmeticsMenu` | `UUserWidget` | `BP_GamePlayerController.CosmeticsWidgetClass` | No (futuro) |

---

## 7b) Checklist de configuracion del GameMode en LVL_HQ (zona ready)

Si el HUD aparece pero la zona no funciona, verifica en orden:

1. **World Settings de LVL_HQ** → `GameMode Override` = `TN_HQGameMode` o `BP_HQGameMode` (hijo de `TN_HQGameMode`). Si esta en None o en otro GameMode, la zona no encontrara el game mode y el Output Log mostrara el error `[LobbyReadyZone] No se encontro TN_HQGameMode`.

2. **`TN_LobbyReadyZone` en el nivel** → el actor debe estar colocado en `LVL_HQ`. Al compilar verifica en Output Log que NO aparece el error de arriba.

3. **Collision del TriggerBox** → selecciona el `TN_LobbyReadyZone` en el level → Details → Collision:
   - `Collision Preset` = `Trigger` (o `OverlapAll`)
   - `Generate Overlap Events` = **true**

4. **Collision del personaje** → `TortugaCharacter` usa la capsula por defecto de `ACharacter`, que ya tiene `Generate Overlap Events = true` y profile `Pawn`. No debes cambiar nada.

5. **`PlayerControllerClass` en el GameMode** → si usas `BP_HQGameMode`, asegurate de que `PlayerControllerClass` apunta a `BP_GamePlayerController` (no a `AMP_GamePlayerController` directamente si quieres que los widget defaults del BP se apliquen).

6. **`MatchMapPath` en el GameMode** → en Class Defaults de `TN_HQGameMode` o `BP_HQGameMode`, el campo `MatchMapPath` debe ser la ruta EXACTA de tu mapa de carrera, ej: `/Game/Maps/Run/LVL_Run`. Si es incorrecto, el `ServerTravel` fallara (el Output Log mostrara `[HQGameMode] Iniciando ServerTravel hacia: ...`).

7. **El jugador debe entrar ANDANDO a la zona**, no spawnearse dentro. Si el `PlayerStart` esta dentro del TriggerBox, el overlap puede no registrarse correctamente. Coloca el PlayerStart FUERA de la zona.

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

> Ya no necesitas grafo complejo en UMG para el countdown; la logica principal vive en C++.

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
- **No aparece el HUD de flujo (countdown/sala)**: `CoopFlowWidgetClass` no esta asignado en `BP_GamePlayerController`. Abre el BP → Class Defaults → asigna tu `WBP_CoopFlowHUD`. Si no lo tienes, el fallback C++ funciona igualmente (texto sin estilo).
- **No aparece icono de voz**: `VoiceIndicatorWidgetClass` no esta asignado en `BP_GamePlayerController` → Class Defaults.
- **No empieza countdown**: (1) Verifica en Output Log que aparece `[LobbyReadyZone] Jugador X ENTRO en la zona.`. Si no aparece, el overlap no esta disparando — revisa la checklist de la seccion 7b. (2) Verifica que el GameMode en `LVL_HQ` sea `TN_HQGameMode` o derivado. (3) Si el log aparece pero no empieza, el `ResolveHQGameMode()` devuelve null — el GameMode Override esta mal configurado.
- **Countdown arranca pero no viaja**: Revisa en Output Log `[HQGameMode] Iniciando ServerTravel hacia: ...` y verifica que la ruta coincide con tu mapa de carrera real.

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

