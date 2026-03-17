# Tortunabo - Setup completo (UE 5.6)

Guia unica para dejar el proyecto funcional de extremo a extremo:
`Menu -> Host/Join -> HQ Lobby -> Countdown -> Cinematic -> Run -> Finish/Spectate -> Results -> Return HQ`.

---

## 0) Objetivo y estado actual

Este repo ya tiene base C++ de cooperativo y sesiones. El objetivo de este README es que puedas:

1. Abrir, compilar y regenerar el proyecto sin errores.
2. Configurar mapas/GameModes/Blueprints para que el jugador **spawnee** y **reciba input** correctamente.
3. Evitar los problemas reportados en logs:
   - jugador no aparece o no se mueve al entrar a Lobby desde Menu,
   - incompatibilidad `AGameMode` vs `AGameStateBase`,
   - widget HUD faltante,
   - crash de audio al cerrar el Editor.
4. Ejecutar una validacion funcional minima en Standalone.

---

## 1) Estructura del modulo `Tortunabo`

- `Source/Tortunabo/Public/Core`: `TN_CoopGameState`, `TN_CoopPlayerState`, `TN_MatchFlowTypes`
- `Source/Tortunabo/Public/Lobby`: `TN_HQGameMode`, `TN_LobbyReadyZone`
- `Source/Tortunabo/Public/Game`: `TN_RunGameMode`
- `Source/Tortunabo/Public/Menu`: `MP_MenuGameMode`, `MP_MenuPlayerController`
- `Source/Tortunabo/Public/Multiplayer`: `MP_GameInstance`
- `Source/Tortunabo/Public/Player`: `TortugaCharacter`, `MP_GamePlayerController`
- `Source/Tortunabo/Public/UI/*`: `MP_MainMenuWidget`, `VoiceIndicatorWidget`
- `Source/Tortunabo/Public/Voice`: `ProximityVoiceComponent`
- `Source/Tortunabo/Public/World`: `TN_FinishLineVolume`

---

## 2) Pre-requisitos

1. Unreal Engine `5.6` instalado.
2. Proyecto en: `C:\Users\Rodrigo\CARRERA\Workspace\Unreal Engine\Proyectos\Tortunabo`.
3. Steam abierto (para pruebas reales de online).
4. Visual Studio con toolchain C++ para UE (Desktop development with C++).

---

## 3) Build y regeneracion de project files

> Haz esto siempre que agregues/renombres clases C++ o toques `Build.cs`.

1. Cierra Unreal Editor.
2. Regenera project files desde `Tortunabo.uproject` (clic derecho -> Generate Visual Studio project files).
3. Compila el editor target:

```powershell
& "C:\Program Files\Epic Games\UE_5.6\Engine\Build\BatchFiles\Build.bat" TortunaboEditor Win64 Development "C:\Users\Rodrigo\CARRERA\Workspace\Unreal Engine\Proyectos\Tortunabo\Tortunabo.uproject" -WaitMutex -NoHotReload
```

4. Abre `Tortunabo.uproject` (no la `.sln`) para validar carga de clases en UE.

---

## 4) Config global obligatoria (source of truth)

Verifica en `Config/DefaultEngine.ini`:

- `GameInstanceClass=/Script/Tortunabo.MP_GameInstance`
- `GlobalDefaultGameMode=/Script/Tortunabo.TN_HQGameMode`
- NetDriver para SteamSockets y `OnlineSubsystemSteam` (con `SteamDevAppId=480` para dev).

Verifica en `Config/DefaultInput.ini`:

- Enhanced Input activo (`EnhancedPlayerInput`, `EnhancedInputComponent`).

Verifica en `Config/DefaultGame.ini`:

- Cook de carpetas: `/Game/Maps`, `/Game/UI`, `/Game/Input`.

---

## 5) Setup de contenido en Editor (paso a paso, sin saltos)

### 5.1 Crear/validar mapas

1. Crea o valida:
   - `/Game/Maps/Lobby/LVL_HQ`
   - `/Game/Maps/Run/LVL_Run`
   - `/Game/Maps/Menu/LVL_Menu` (recomendado para flujo limpio de menu)
2. Guarda todos los niveles.

### 5.2 Configurar `LVL_HQ` (Lobby)

1. Abre `LVL_HQ`.
2. `World Settings -> GameMode Override`: asigna `TN_HQGameMode` (o `BP_HQGameMode` si existe).
3. Coloca al menos **1 `PlayerStart`** valido en una zona navegable.
4. Coloca `TN_LobbyReadyZone` en la zona de ready.
5. Si usas Blueprint hijo (`BP_HQGameMode`), revisa:
   - `Default Pawn Class` apunta al pawn jugable (idealmente BP hijo de `TortugaCharacter`).
   - `Player Controller Class` apunta al controller de gameplay (`MP_GamePlayerController` o hijo BP).
   - `Game State Class` es compatible con `AGameMode` (ver seccion de troubleshooting).

### 5.3 Configurar `LVL_Run` (partida)

1. Abre `LVL_Run`.
2. `World Settings -> GameMode Override`: `TN_RunGameMode` (o su BP).
3. Coloca varios `PlayerStart`.
4. Coloca `TN_FinishLineVolume` en la meta.
5. En el GameMode de Run, ajusta `LobbyMapPath` a `/Game/Maps/Lobby/LVL_HQ`.

### 5.4 Configurar `LVL_Menu` (menu)

1. `World Settings -> GameMode Override`: `MP_MenuGameMode` (o BP hijo).
2. `Player Controller Class`: `MP_MenuPlayerController`.
3. El flujo de Host/Join debe viajar a `LVL_HQ`.
4. Al terminar el viaje, el mapa destino (`LVL_HQ`) debe imponer su GameMode de lobby.

### 5.5 Paths de travel y flujo

1. En `TN_HQGameMode`, ajusta `MatchMapPath` a `/Game/Maps/Run/LVL_Run`.
2. En `TN_RunGameMode`, ajusta `LobbyMapPath` a `/Game/Maps/Lobby/LVL_HQ`.
3. Ajusta `CinematicDelaySeconds` en `TN_HQGameMode` para tu timing de countdown/cinematica.

---

## 6) Solucion al problema principal: no spawn / sin input al abrir Lobby desde Menu

Checklist exacta:

1. En `LVL_HQ` hay `PlayerStart` y no esta bloqueado.
2. El `GameMode Override` real de `LVL_HQ` es `TN_HQGameMode`/`BP_HQGameMode` correcto.
3. `Default Pawn Class` del GameMode de lobby apunta a tu pawn real (BP de tortuga o `TortugaCharacter`).
4. `Player Controller Class` del lobby es controller de gameplay, no el del menu.
5. En travel desde menu, no dejes `InputMode UI Only` persistente. Al entrar al lobby, fuerza `Game Only` o `Game and UI` segun HUD.
6. Verifica que el spawn sea server-authoritative y no dependa de logica solo cliente.
7. Prueba primero en Standalone (PIE puede usar OSS NULL y ocultar comportamientos de Steam).

---

## 7) Troubleshooting de errores observados en log

### 7.1 Error: `Mixing AGameStateBase with AGameMode is not compatible`

Log detectado:
- `Change AGameStateBase subclass (TN_CoopGameState) to derive from AGameState, or make both derive from Base`.

Accion:
1. Si tu GameMode deriva de `AGameMode`, su GameState debe derivar de `AGameState`.
2. Si quieres mantener `AGameStateBase`, entonces el modo debe ser `AGameModeBase`.
3. En Blueprints de GameMode, revisa que `Game State Class` no haya quedado apuntando a una clase incompatible.

### 7.2 Warning: falta `WBP_CoopFlowHUD`

Logs:
- `LoadPackage: SkipPackage: /Game/UI/HUD/WBP_CoopFlowHUD ... package does not exist`
- `Failed to find object ... WBP_CoopFlowHUD_C`

Accion:
1. Crea `WBP_CoopFlowHUD` en `/Game/UI/HUD/` o corrige la referencia donde se carga.
2. Si se crea nuevo asset, compila/guarda el widget y vuelve a abrir el mapa.
3. Asegura que `/Game/UI` esta incluido para cook (ya documentado en `DefaultGame.ini`).

### 7.3 Steam no disponible en PIE

Log esperado en PIE:
- `Online Subsystem: NULL`
- `Steam not available in PIE. Use Standalone Game to test multiplayer.`

Accion:
1. Para validar sesiones reales, usa `Standalone Game` o build empaquetada.
2. Usa PIE solo para iteracion rapida local de gameplay basico.

### 7.4 Crash de audio al cerrar editor

Sintoma reportado:
- Crash al cerrar Editor con stack de audio/voice activo.

Accion recomendada de depuracion:
1. Revisa ciclo de vida de `ProximityVoiceComponent` (init/deinit en BeginPlay/EndPlay).
2. Verifica destruccion segura de capturas/streams al cerrar mundo (`EndPlay`, `OnUnregister`, `BeginDestroy`).
3. Evita callbacks activos apuntando a objetos ya destruidos durante teardown.
4. Mantener dependencias en `Source/Tortunabo/Tortunabo.Build.cs`:
   - `AudioCapture`, `AudioCaptureCore`, `AudioMixer`, `SignalProcessing`, `OnlineSubsystemSteam`.
5. Reproducir con logs de cierre y, si persiste, aislar temporalmente voz para confirmar origen.

---

## 8) Prueba funcional minima (smoke test)

Haz este orden exacto:

1. Abre `LVL_Menu`.
2. Ejecuta en `Standalone Game`.
3. Crea lobby desde menu (Host).
4. Confirma spawn en `LVL_HQ` y movimiento del jugador.
5. Entra/sale de `TN_LobbyReadyZone` y verifica countdown cancelable.
6. Espera transicion `Countdown -> Cinematic -> Travel`.
7. En `LVL_Run`, confirma spawn en `PlayerStart` y llegada a meta.
8. Verifica estado de spectate/resultados y retorno a HQ.

Si algo falla, vuelve a secciones `6` y `7`.

---

## 9) Backlog inmediato (pendiente funcional)

1. Verificar GUI y funcionalidad de contador al inicio segun numero de jugadores en sala.
2. Verificar GUI de fin de partida.
3. Crear variante de jugador en primera persona (look/camara FP).
4. Implementar sistema modular de interaccion:
   - base padre de interaccion,
   - hija para interaccion directa (palanca/canon),
   - hija para objetos de recogida.
5. Inventario basico 2 slots (equipado/no equipado) + rotacion con Enhanced Input.
6. Mantener optimizacion multiplayer: authority clara, replicacion minima, evitar trafico innecesario.

---

## 10) Objetivo Demo (memoria de proyecto)

Target funcional de demo:

- Menu principal.
- Lobby para iniciar partida.
- Nivel para 2 tortugas con:
  - 2 interactuables directos,
  - 2 objetos de recogida,
  - zonas de muerte (p. ej. gaviotas),
  - amenaza ambiental progresiva (niebla/kill volume movil).
- Pantalla final de victoria.
- Sistema primitivo de cosmeticos:
  - boton en menu,
  - guardado local de skins disponibles,
  - GUI con array de skins,
  - equipar/desequipar para usar en partida.
- Menu de pausa y opcion al finalizar para volver a menu o lobby.
- Acceso a GUI de skins desde lobby por interactuable directo.

Prioridad: funcionamiento primero, modularidad alta, evitar codigo acoplado.

---

## 11) Coordinacion entre dos desarrolladores/agents

1. Usar carpeta `Docs/AgentSync/` como punto de sincronizacion tecnica.
2. Registrar cambios importantes en `Docs/AgentSync/SESSION_LOG.md`.
3. Usar `Docs/AgentSync/MEMORY_DUMP_TEMPLATE.md` para volcados tecnicos detallados.
4. Cada cambio relevante debe dejar contexto suficiente para que el segundo agente continue sin bloqueo.

