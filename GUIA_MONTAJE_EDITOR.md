# Guía Completa de Montaje Inicial del Editor - Tortunabo UE 5.6

## Índice
1. [Configuración de Enhanced Input](#1-configuración-de-enhanced-input)
2. [Montaje del Jugador (TortugaCharacter)](#2-montaje-del-jugador)
3. [Montaje de Lobby & Ready Zone](#3-montaje-de-lobby--ready-zone)
4. [Montaje del Countdown UI](#4-montaje-del-countdown-ui)
5. [Montaje del Voice Chat](#5-montaje-del-voice-chat)
6. [Flujo Completo y Pruebas](#6-flujo-completo-y-pruebas)
7. [Troubleshooting](#7-troubleshooting)

---

## 1. Configuración de Enhanced Input

### 1.1 Crear la Carpeta de Inputs
1. En el Content Browser, navega a `/Content/Input/` (crear carpeta si no existe)
2. Haz clic derecho en `/Content/Input/` → New Folder → nombre `Input`

### 1.2 Crear el InputMappingContext (IMC)
1. En `/Content/Input/`, haz clic derecho → Miscellaneous → Input Mapping Context
2. Llámalo `IMC_Player`
3. Abre `IMC_Player`
4. **NO NECESITAS AGREGAR MAPPINGS AQUÍ AHORA** - Se auto-cargarán cuando agregues InputActions

### 1.3 Crear los InputActions
Haz esto para cada acción. Haz clic derecho en `/Content/Input/`:

#### IA_Move (Movimiento)
1. Miscellaneous → Input Action
2. Llámalo `IA_Move`
3. Abre y en Details:
   - Value Type: `Axis2D` (0,0 a 1,1 o -1,-1 a 1,1)
   - Modifiers: Agregar `Input Modifier Swizzle Axis` si quieres invertir ejes
4. Guarda

#### IA_Look (Cámara)
1. Miscellaneous → Input Action
2. Llámalo `IA_Look`
3. Value Type: `Axis2D`
4. Guarda

#### IA_Jump (Saltar)
1. Miscellaneous → Input Action
2. Llámalo `IA_Jump`
3. Value Type: `Digital` (botón on/off)
4. Guarda

#### IA_Interact (Interactuar)
1. Miscellaneous → Input Action
2. Llámalo `IA_Interact`
3. Value Type: `Digital`
4. Guarda

#### IA_RotateInventory (Rotar Inventario)
1. Miscellaneous → Input Action
2. Llámalo `IA_RotateInventory`
3. Value Type: `Digital`
4. Guarda

### 1.4 Mapear teclas en IMC_Player
1. Abre `IMC_Player`
2. En Details (abajo), bajo "Mappings", agrega:

| Input Action | Key | Modifier | Trigger |
|--------------|-----|----------|---------|
| IA_Move | Gamepad_LeftThumbstick | (none) | (default) |
| IA_Move | W,A,S,D | (none) | (default) |
| IA_Look | Gamepad_RightThumbstick | (none) | (default) |
| IA_Look | Mouse X/Y | (none) | (default) |
| IA_Jump | Gamepad_FaceButton_Bottom (A/Cross) | (none) | (default) |
| IA_Jump | Space | (none) | (default) |
| IA_Interact | Gamepad_FaceButton_Top (Y/Triangle) | (none) | (default) |
| IA_Interact | E | (none) | (default) |
| IA_RotateInventory | Q/R o Gamepad_FaceButton_Left | (none) | (default) |

3. Guarda IMC_Player

### 1.5 Verificar en DefaultInput.ini (Opcional)
- Ve a `Config/DefaultInput.ini` y verifica que Enhanced Input está activo
- Deberías ver `EnhancedInputComponent` ya configurado

---

## 2. Montaje del Jugador

### 2.1 Crear/Editar el Mapa de Lobby
1. File → New Level → Basic (Default)
2. Guarda como `/Content/Maps/Lobby/LVL_Lobby`

### 2.2 Agregar PlayerStart
1. En el World Outliner, Add Actor → Gameplay → Player Start
2. Posiciona en (0, 0, 100) aproximadamente
3. Verifica que sea visible en el viewport

### 2.3 Verificar TortugaCharacter
- En Code, el `TortugaCharacter` ya carga:
  - `DefaultMappingContext` desde `/Game/Input/IMC_Player` ✓
  - `MoveAction`, `LookAction`, `JumpAction` ✓
  - `CameraBoom` (SpringArm) ✓
  - `FollowCamera` ✓
- **Todo esto es automático; NO necesitas hacer nada extra en Blueprint**

### 2.4 Test: ¿Funciona el Movimiento?
1. En LVL_Lobby, en Details → World Settings:
   - GameMode Override: `TN_HQGameMode`
   - DefaultPawnClass: `TortugaCharacter` (ya está en código)
2. Play (PIE)
3. Presiona WASD o Gamepad_LeftThumbstick
   - Si se mueve → ✓ Input funciona
   - Si NO se mueve → Ver [Troubleshooting](#71-input-no-funciona)

---

## 3. Montaje de Lobby & Ready Zone

### 3.1 Agregar Ready Zone Volume
1. En LVL_Lobby, en World Outliner: Add Actor → Volumes → Trigger Box
2. Llámalo `ReadyZone`
3. En Details:
   - Mesh → Static Mesh: `Engine_SM_Trigger_Box` (default)
   - Transform → Scale: (3, 3, 1) para que sea visible y ancho
   - Transform → Location: (0, 0, 100) - Centro de la arena
   - Collision → Generate Overlap Events: ✓ ON
   - Collision → Collision Enabled: `Query Only`
   - Collision → Object Type: `WorldStatic` (quedará así; no pasa nada)
4. En Details (arriba), en Class, reemplaza la clase por `TN_LobbyReadyZone`
   - Click derecho en ReadyZone (en Outliner) → Replace with...  `TN_LobbyReadyZone`
   - O en las Class details, asegúrate de que el blueprint apunta a C++ class TN_LobbyReadyZone

### 3.2 Verificar Collision Setup
1. En el ReadyZone, Details → Collision:
   - Object Type: `WorldStatic`
   - Trace Responses: `Ignore`
   - Object Responses: (default)
   - Generate Overlap Events: ✓ ON

### 3.3 Test: ¿Entra en la Zona?
1. Play PIE
2. Mueve el personaje hacia el centro (donde está el trigger)
3. Mira la Output Log:
   - Deberías ver: `[Lobby] Ready zone initialized...`
   - Cuando entres: `[Lobby] Player entered ready zone...`
   - Cuando salgas: `[Lobby] Player left ready zone...`
4. Si no ves logs → Ver [Troubleshooting](#72-ready-zone-no-triggea)

---

## 4. Montaje del Countdown UI

### 4.1 El Widget ya existe
- `WBP_CoopFlowHUD` en `/Game/UI/HUD/` (o lo crea automáticamente el código)
- Muestra:
  - **Esperando jugadores listos: X/4**
  - **La partida empieza en: 3...2...1** (durante Countdown)

### 4.2 Verificar Widget en el Mapa
1. En `MP_GamePlayerController::BeginPlay()`, se llama `CreateCoopFlowHUD()`
   - Carga automáticamente `WBP_CoopFlowHUD`
   - Lo agrega al Viewport con Z-order 5
2. NO NECESITAS AGREGAR NADA EN EL MAPA

### 4.3 Test: ¿Se Ve el Countdown?
1. Play PIE con 2+ Players (Standalone o Net: Open 2)
   - Mira abajo/esquina el HUD: Debería decir "Esperando jugadores listos: 0/4"
2. Mueve ambos al ReadyZone:
   - Debería cambiar a "Esperando jugadores listos: 2/4"
3. Si todos entran (4 en ReadyZone):
   - Countdown inicia: "La partida empieza en: 3"
   - Cada segundo: 2, 1, 0
   - Luego: Travel a Run map
4. Si alguien sale durante countdown:
   - Countdown resetea a "Esperando jugadores listos: 3/4"

---

## 5. Montaje del Voice Chat

### 5.1 ProximityVoiceComponent
- Ya está integrado en `TortugaCharacter` via `MP_GamePlayerController::OnPossess()`
- Cada pawn automáticamente obtiene un componente de voz

### 5.2 Verificar Audio Setup
1. En Project Settings → Audio:
   - Master Volume: 1.0
   - Voice: 1.0
   - Asegúrate de que no hay muting

### 5.3 Test: ¿Funciona el Voice?
1. Play Standalone (NO PIE - necesitas real audio capture)
2. En 2 máquinas diferentes, habla en una
3. En la otra, deberías escuchar la voz (si proximity es correcta)
4. Si no escuchas:
   - Verifica que tu micrófono funciona en Windows
   - Verifica que los players están en "Proximity" (< 3000 units)

---

## 6. Flujo Completo y Pruebas

### 6.1 Smoke Test Local (PIE con Net)
```
1. File → Project Settings → Engine → Net Driver → DefaultNetDriverClassName
   - Asegúrate de que usa `NetworkDriver` o `UDPNetDriver` (default está bien)
2. Play → New Editor Window (PIE)
3. Multiplayer Options:
   - Num Players: 4
   - Window Size: 960x720
4. Observa 4 ventanas de juego
5. Mueve todos a ReadyZone → Countdown inicia → Travel a Run
```

### 6.2 Smoke Test Standalone (Real Networking)
```
1. Compile proyecto
2. Build → Standalone Game (or Launch)
3. En 2+ máquinas:
   - Una abre "Host Game"
   - Otra abre "Join Game" + ingresa IP del host
4. Todos en ReadyZone → Countdown → Run
```

### 6.3 Validar Cada Sistema
| Sistema | Prueba | Esperado |
|---------|--------|----------|
| Input | WASD/Gamepad en Lobby | Personaje se mueve |
| Ready Zone | Entra en trigger | Log: "entered ready zone" + bIsInReadyZone=true |
| Countdown | 4 en zona → espera 3s | UI: 3...2...1... → Travel |
| Audio | 2 players hablan | Escuchas la voz del otro |
| Replication | Cambios de state | Todos ven lo mismo |

---

## 7. Troubleshooting

### 7.1 Input No Funciona
**Síntoma**: Presionas WASD/Gamepad pero no se mueve.

**Checks**:
1. ¿Existen los InputActions?
   - Ve a `/Content/Input/`
   - Deberías ver: `IMC_Player`, `IA_Move`, `IA_Look`, `IA_Jump`, etc.
   - Si no existen → Sigue sección 1 (Crear InputActions)

2. ¿IMC_Player tiene mappings?
   - Abre `IMC_Player`
   - En Details → Mappings, verifica que ves WASD → IA_Move, Mouse → IA_Look, etc.
   - Si está vacío → Agrega mappings (sección 1.4)

3. ¿El GameMode es TN_HQGameMode?
   - En LVL_Lobby, World Settings → GameMode Override → TN_HQGameMode
   - Si está vacío → Asígna GameMode correcto

4. ¿Mira los logs?
   - Output Log debe decir: `[Input] Applied IMC to local player`
   - Si dice `Missing IMC_Player` → Crea los InputActions (sección 1)

### 7.2 Ready Zone No Triggea
**Síntoma**: Entras en la zona pero no se pone "ready".

**Checks**:
1. ¿Existe el ReadyZone?
   - World Outliner → Busca `ReadyZone`
   - Si no existe → Crea uno (sección 3.1)

2. ¿Es de clase TN_LobbyReadyZone?
   - Click en ReadyZone en Outliner
   - Details → Class: debe mostrar `TN_LobbyReadyZone`
   - Si es ATriggerBox → Reemplaza la clase (sección 3.1)

3. ¿Tiene collision y overlap events?
   - Details → Collision → Generate Overlap Events: ✓ ON
   - Details → Collision → Collision Enabled: `Query Only` o `Query and Physics`
   - Si no → Habilitalo

4. ¿Mira los logs?
   - Output Log debe decir: `[Lobby] Ready zone initialized...`
   - Si no sale → Presiona Play de nuevo o revisa que está en LVL_Lobby

### 7.3 Countdown No Inicia
**Síntoma**: Entran todos en la zona pero no inicia countdown.

**Checks**:
1. ¿Hay 4 jugadores?
   - En editor, Net: Open 4 Instances
   - Todos deben estar listos (en ReadyZone)

2. ¿El GameState refleja los ready players?
   - En Runtime, coloca breakpoint en `TN_HQGameMode::RefreshLobbyState()`
   - Verifica que `ReadyPlayers == ExpectedPlayers`
   - Si no, es que alguien no entró en la zona

3. ¿El HUD muestra "3"?
   - En el UI, deberías ver: "La partida empieza en: 3"
   - Si ves "Esperando jugadores listos" aún → Alguien se salió

### 7.4 Audio Crashea
**Síntoma**: Al cerrar juego o cambiar mapa, crash en EndPlay de voz.

**Checks**:
1. ¿Todos los cambios de ProximityVoiceComponent se aplicaron?
   - Recompila (Ctrl+Alt+F11 en VS o Build en editor)
   - Si compilation error → Revisa sintaxis

2. ¿Los logs dicen qué falló?
   - Output Log → Busca `[Voice]` o `ProximityVoice`
   - Si dice `... being destroyed`, es normal en shutdown

---

## 8. Configuración de DefaultEngine.ini (Verificación)

Asegúrate de que estos valores están en `Config/DefaultEngine.ini`:

```ini
[/Script/Engine.Engine]
GlobalDefaultGameMode=/Script/Tortunabo.TN_HQGameMode

[OnlineSubsystemSteam]
bEnabled=true
SteamDevAppId=480

[/Script/Engine.WorldSettings]
DefaultGameMode=/Script/Tortunabo.TN_HQGameMode
```

Si no están, agrégalos.

---

## 9. Compilación y Build

### 9.1 Recompile C++
```bash
# En PowerShell (desde carpeta del proyecto):
Build.bat TortunaboEditor Win64 Development Tortunabo.uproject -WaitMutex
```

### 9.2 Si cambias clases/módulos
```bash
# Regenera .sln desde .uproject:
# 1. Cierra Unreal Editor
# 2. Clic derecho en Tortunabo.uproject → Generate Visual Studio project files
# 3. Abre Tortunabo.sln en VS
# 4. Build → Build Solution (Ctrl+Shift+B)
# 5. Abre de nuevo en Unreal Editor
```

### 9.3 Standalone Build para Testing
```bash
# En Project Launcher (Editor → Tools → Project Launcher):
1. Create Custom Launch Profile
2. Project: Tortunabo
3. Build: Win64 Development
4. Cook: By the Book → Include all maps (Lobby, Run)
5. Package & Deploy to: Standalone
6. Launch cuando termine
```

---

## 10. Resumen de Estado Esperado

Al completar esta guía, deberías tener:

✓ Inputs funcionando (WASD/Gamepad mueve en Lobby)  
✓ Ready Zone visible (trigger box en centro)  
✓ Countdown UI visible (top-left muestra "Esperando..." → "3..." → "1...")  
✓ Audio capturado (sin crashes en shutdown)  
✓ Flujo completo: Lobby → Ready → Countdown (3s) → Cinematic → Run map  
✓ Multi-player sincronizado (todos ven lo mismo)  

Si algo no funciona, sigue el Troubleshooting o abre Output Log (Window → Developer Tools → Output Log) para ver qué falla.

---

**Fecha de creación**: Marzo 2026  
**Motor**: Unreal Engine 5.6  
**Proyecto**: Tortunabo (Coop Racing Game)

