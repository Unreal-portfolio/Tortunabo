# Tortunabo

**Tortunabo** es un juego cooperativo multijugador (1–4 jugadores) en tercera persona desarrollado en **Unreal Engine 5.6** con C++. Los jugadores controlan tortugas antropomórficas que avanzan juntas por niveles generados proceduralmente por chunks, enfrentándose a obstáculos, recogiendo y lanzando objetos, y compitiendo por llegar antes a la meta.

- **Motor**: Unreal Engine 5.6
- **Lenguaje**: C++ (módulo `Tortunabo`) + Blueprints
- **Red**: Steam Sockets (`SteamDevAppId=480` en testing)
- **Equipo**: Rodrigo Fernández y José Antonio (Mokius)
- **Contexto académico**: entrega T-Day, U-tad

## Documentación

| Documento | Contenido |
|---|---|
| [`Docs/Memoria_T-Day.md`](Docs/Memoria_T-Day.md) | Memoria principal del equipo (defensa). |
| [`Docs/Memoria_Individual_Rodrigo_T-Day.md`](Docs/Memoria_Individual_Rodrigo_T-Day.md) | Memoria individual de Rodrigo. |
| [`Docs/Memoria_Individual_JoseAntonio_T-Day.md`](Docs/Memoria_Individual_JoseAntonio_T-Day.md) | Memoria individual de José Antonio. |
| [`Docs/DayT_GDD_Final.pdf`](Docs/DayT_GDD_Final.pdf) | Game Design Document (PDF final). |
| [`Docs/LDD_Tortunabo.md`](Docs/LDD_Tortunabo.md) | Level Design Document. |
| [`Docs/Inventario_Scripts.md`](Docs/Inventario_Scripts.md) | Inventario completo de los 73 archivos `.h` del módulo, por dominio, con descripción y autores. |

## Setup paso a paso

Esta guia contiene el setup paso a paso para dejar el proyecto funcional en UE 5.6.

## 1) Pre-requisitos

1. Unreal Engine `5.6` instalado.
2. Visual Studio con toolchain C++ para Unreal (`Desktop development with C++`).
3. Steam abierto (para pruebas online reales).
4. Proyecto en: `C:\Users\Rodrigo\CARRERA\Workspace\Unreal Engine\Proyectos\Tortunabo`.

## 2) Regenerar project files y compilar

1. Cierra Unreal Editor.
2. Clic derecho en `Tortunabo.uproject` -> `Generate Visual Studio project files`.
3. Ejecuta compilacion del editor target:

```powershell
& "C:\Program Files\Epic Games\UE_5.6\Engine\Build\BatchFiles\Build.bat" TortunaboEditor Win64 Development "C:\Users\Rodrigo\CARRERA\Workspace\Unreal Engine\Proyectos\Tortunabo\Tortunabo.uproject" -WaitMutex -NoHotReload
```

4. Abre `Tortunabo.uproject` para continuar en Editor.

## 3) Verificar configuracion global (`.ini`)

### 3.1 `Config/DefaultEngine.ini`

Verifica estos valores:

- `GameInstanceClass=/Script/Tortunabo.MP_GameInstance`
- `GameDefaultMap=/Game/Maps/Lobby/LVL_Menu.LVL_Menu`
- `GlobalDefaultGameMode=/Game/Blueprints/Gameplay/GameModes/BP_MenuGameMode.BP_MenuGameMode_C`
- `DefaultPlatformService=Steam`
- `SteamDevAppId=480`
- NetDriver con SteamSockets:
  - `DriverClassName="/Script/SteamSockets.SteamSocketsNetDriver"`
  - `DriverClassNameFallback="/Script/OnlineSubsystemUtils.IpNetDriver"`

### 3.2 `Config/DefaultInput.ini`

Verifica Enhanced Input:

- `DefaultPlayerInputClass=/Script/EnhancedInput.EnhancedPlayerInput`
- `DefaultInputComponentClass=/Script/EnhancedInput.EnhancedInputComponent`

### 3.3 `Config/DefaultGame.ini`

Verifica cook de carpetas:

- `/Game/Maps`
- `/Game/UI`
- `/Game/Input`

## 4) Preparar mapas requeridos

En Content Browser, crea o valida:

1. `/Game/Maps/Lobby/LVL_Menu`
2. `/Game/Maps/Lobby/LVL_HQ`
3. `/Game/Maps/Run/LVL_Run`

Guarda todos los mapas.

## 5) Setup de `LVL_Menu`

1. Abre `LVL_Menu`.
2. En `World Settings -> GameMode Override`, asigna `BP_MenuGameMode` (o `MP_MenuGameMode` si no usas BP).
3. En el GameMode de menu, confirma:
   - `Player Controller Class = MP_MenuPlayerController` (o su BP).
   - flujo de Host/Join configurado para viajar a `/Game/Maps/Lobby/LVL_HQ`.
4. Si el menu crea widgets, deja Input Mode para UI en menu (solo dentro de este mapa).

## 6) Setup de `LVL_HQ` (Lobby)

1. Abre `LVL_HQ`.
2. En `World Settings -> GameMode Override`, asigna `TN_HQGameMode` (o `BP_HQGameMode`).
3. Coloca al menos 1 `PlayerStart` valido.
4. Coloca `TN_LobbyReadyZone` en la zona de ready.
5. En el GameMode de lobby, confirma:
   - `Default Pawn Class` apunta al pawn jugable (BP derivado de `TortugaCharacter` o `TortugaCharacter`).
   - `Player Controller Class` apunta a `MP_GamePlayerController` (o su BP).
   - `MatchMapPath=/Game/Maps/Run/LVL_Run`.
6. Al entrar al lobby desde menu, aplicar Input Mode de juego (`Game Only` o `Game and UI` segun HUD).

## 7) Setup de `LVL_Run` (Partida)

1. Abre `LVL_Run`.
2. En `World Settings -> GameMode Override`, asigna `TN_RunGameMode` (o su BP).
3. Coloca varios `PlayerStart`.
4. Coloca `TN_FinishLineVolume` en la meta.
5. En el GameMode de run, confirma:
   - `LobbyMapPath=/Game/Maps/Lobby/LVL_HQ`.
   - `Default Pawn Class` y `Player Controller Class` de gameplay correctos.

## 8) Guardar y setear mapa de arranque

1. Guarda todos los Blueprints y mapas.
2. Ve a `Project Settings -> Maps & Modes`.
3. Verifica:
   - `Editor Startup Map = /Game/Maps/Lobby/LVL_Menu`
   - `Game Default Map = /Game/Maps/Lobby/LVL_Menu`
4. Reinicia el Editor para validar que todo carga correctamente.

## 9) Validacion final del setup (solo verificacion)

1. Ejecuta `Standalone Game` desde `LVL_Menu`.
2. Crea Lobby (Host).
3. Verifica que en `LVL_HQ`:
   - el jugador aparece en `PlayerStart`,
   - el jugador se puede mover,
   - el control ya no queda en modo UI del menu.
4. Verifica transicion de Lobby a Run.
5. Verifica retorno de Run a Lobby.

