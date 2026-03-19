# SESSION LOG

## 2026-03-17 - Sprint 1 Kickoff

### Contexto
- Inicio de implementacion de base modular para interaccion + inventario 1+1.

### Cambios clave
- Base de interactuables C++:
  - `Source/Tortunabo/Public/World/TN_InteractableBase.h`
  - `Source/Tortunabo/Private/World/TN_InteractableBase.cpp`
- Hijas de interactuables:
  - `Source/Tortunabo/Public/World/TN_DirectInteractableBase.h`
  - `Source/Tortunabo/Private/World/TN_DirectInteractableBase.cpp`
  - `Source/Tortunabo/Public/World/TN_PickupInteractableBase.h`
  - `Source/Tortunabo/Private/World/TN_PickupInteractableBase.cpp`
- Inventario 1 equipado + 1 almacenado:
  - `Source/Tortunabo/Public/Core/TN_InventoryTypes.h`
  - `Source/Tortunabo/Public/Player/TN_InventoryComponent.h`
  - `Source/Tortunabo/Private/Player/TN_InventoryComponent.cpp`
- Integracion de input de interaccion y rotacion en personaje:
  - `Source/Tortunabo/Public/Player/TortugaCharacter.h`
  - `Source/Tortunabo/Private/Player/TortugaCharacter.cpp`
- Nuevo personaje base primera persona:
  - `Source/Tortunabo/Public/Player/TortugaFirstPersonCharacter.h`
  - `Source/Tortunabo/Private/Player/TortugaFirstPersonCharacter.cpp`
- Prompt 3D reusable:
  - `Source/Tortunabo/Public/UI/HUD/TN_InteractPromptWidget.h`
  - `Source/Tortunabo/Private/UI/HUD/TN_InteractPromptWidget.cpp`

### Networking y rendimiento
- Interaccion server-authoritative (`ServerTryInteract`) para evitar desincronizacion.
- Pickups replican estado compacto (`bTaken`) en vez de fisica replicada continua.
- Escaneo de interactuable en timer local (0.1s), no tick por frame.

### Pendientes inmediatos
1. Crear assets Enhanced Input: `IA_Interact`, `IA_RotateInventory`.
2. Crear BP widgets para prompt 3D y validar UX en mapa.
3. Probar end-to-end con 2 clientes (host + join).

## 2026-03-18 - Fix VOIP WASAPI crash during level transitions

### Contexto
- Crash `EXCEPTION_ACCESS_VIOLATION writing address 0x0000000000000024` en `AudioCaptureWasapi` al hacer ServerTravel.
- El workaround anterior (leak intencional via `Release()` en EndPlay) no era suficiente: el `FAudioCaptureSynth` seguía con callbacks WASAPI registrados durante teardown.

### Cambios clave
- `ProximityVoiceComponent`:
  - `Source/Tortunabo/Public/Voice/ProximityVoiceComponent.h`: añadidos `PrepareForLevelTransition()` y `static ShutdownAllCapture(const UWorld*)`.
  - `Source/Tortunabo/Private/Voice/ProximityVoiceComponent.cpp`: implementación que para captura limpiamente (StopCapturing + Reset) mientras WASAPI sigue vivo.
- `TN_HQGameMode`:
  - `Source/Tortunabo/Private/Lobby/TN_HQGameMode.cpp`: `BeginMatchTravel()` llama `ShutdownAllCapture(World)` antes de `ServerTravel`.
- `TN_RunGameMode`:
  - `Source/Tortunabo/Private/Game/TN_RunGameMode.cpp`: `FinishRoundAndReturnToLobby()` llama `ShutdownAllCapture(World)` antes de `ServerTravel`.
- `MP_GameInstance`:
  - `Source/Tortunabo/Private/Multiplayer/MP_GameInstance.cpp`: `HandleReturnToMenu()` llama `ShutdownAllCapture` antes de `ClientTravel`. `HandlePreLoadMap()` actúa como safety net para cualquier transición no cubierta.

### Networking y rendimiento
- Sin impacto en replicación ni RPCs existentes.
- `ShutdownAllCapture` usa `TObjectIterator` (one-shot, no tick); impacto despreciable.
- La limpieza proactiva elimina el memory leak intencional del patrón anterior para transiciones de nivel.

### Pendientes inmediatos
1. Validar con 2+ clientes en Standalone (host + join, viajar ida y vuelta lobby ↔ run).
2. Verificar que VOIP se reinicializa correctamente en BeginPlay del nuevo nivel.

## 2026-03-18 - Fix VOIP WASAPI crash v2 (always-leak)

### Contexto
- El fix anterior (StopCapturing proactivo) seguía crasheando: `ACCESS_VIOLATION reading 0xFFFFFFFFFFFFFFFF` en `StopCapturing()`.
- Los handles WASAPI internos de `FAudioCaptureSynth` pueden ser `INVALID_HANDLE_VALUE` incluso cuando el audio engine sigue activo — bug de UE 5.6.
- Tanto `StopCapturing()` como el destructor (`Reset()`) son fatalmente inseguros.

### Cambios clave
- `ProximityVoiceComponent.cpp`: `CleanupRuntimeResources` ahora **siempre** usa `Release()` (orphan) para el capture synth. Eliminado completamente el path de `StopCapturing()`/`Reset()`.
- El parámetro `bForceLeakAudio` solo controla limpieza de playback/UI, no del capture synth.
- Se mantiene el patrón proactivo `ShutdownAllCapture` para limpiar playback y UI mientras el mundo es válido.

### Riesgos conocidos
- Memory leak de ~KB por cada FAudioCaptureSynth orphaned (1 por transición de nivel o muerte de jugador). Aceptable para sesiones de juego normales.
- El synth orphaned sigue con callbacks WASAPI activos hasta cierre de proceso. No afecta gameplay.

## 2026-03-19 - Review sistema interacción + fix Network error lobby→game + optimización replicación ThrowableItem

### Contexto
- Revisión del sistema de interacción por proximidad.
- Error de red observado al pasar del lobby al mapa de carrera: `OnNetworkFailure` destruía la sesión Steam al recibir `NetDriverListenFailure`/`NetDriverAlreadyExists` incluso durante un `ServerTravel` normal (colisión transitoria del socket Steam).
- Replicación redundante en `TN_ThrowableItemActor`: 3 variables con el mismo `OnRep_ThrowData`, causando hasta 3 llamadas innecesarias por envío.

### Cambios clave

#### `TN_ThrowableItemActor` — optimización de replicación
- **`Public/World/TN_ThrowableItemActor.h`**: Añadido `USTRUCT FTN_ThrowLaunchData` con campos `SpawnLocation`, `LaunchVelocity` y `bReady`. Las 3 propiedades `UPROPERTY(ReplicatedUsing)` separadas se reemplazan por un único `ThrowData` de tipo `FTN_ThrowLaunchData`.
- **`Private/World/TN_ThrowableItemActor.cpp`**: `DOREPLIFETIME` actualizado para un solo struct. `InitializeThrow()` y `ApplyLaunchDataIfReady()` actualizados para usar `ThrowData.*`.
- **Resultado**: Un único dirty bit → un único `OnRep_ThrowData` por replicación (antes: hasta 3 llamadas por frame).

#### `TortugaCharacter` — eliminación de código muerto
- **`Public/Player/TortugaCharacter.h`** y **`Private/Player/TortugaCharacter.cpp`**: Eliminada `ResolveInteractionViewPoint()`. Esta función era residuo del antiguo sistema de raycast; el sistema actual usa proximity scan (`OverlapMultiByObjectType`) y nunca la invocaba.
- Añadido `#include "Engine/OverlapResult.h"` explícito para resolver el tipo `FOverlapResult`.

#### `MP_GameInstance` — fix error de red durante transición lobby→game
- **`Public/Multiplayer/MP_GameInstance.h`**: Añadido flag privado `bool bIsPendingTravel = false`.
- **`Private/Multiplayer/MP_GameInstance.cpp`**:
  - `HandlePreLoadMap` setea `bIsPendingTravel = true`.
  - `HandlePostLoadMap` lo resetea a `false`.
  - `OnNetworkFailure`: cuando `FailureType` es `NetDriverListenFailure/CreateFailure/AlreadyExists` **y** `bIsPendingTravel == true`, el error se trata como transitorio (socket Steam no liberado a tiempo durante ServerTravel). La sesión NO se destruye. Solo se oculta la loading screen y se loguea warning. Cuando `bIsPendingTravel == false` (inicio de conexión), el comportamiento anterior se mantiene (destruir sesión zombi).

### Networking y rendimiento
- `ThrowableItemActor`: -2 OnRep calls por lanzamiento (de 3 a 1). Misma semántica.
- Interacción: sin cambios en el flujo. Sistema ya correcto: scan local 0.1s → `ServerTryInteract(ATN_InteractableBase*)` → validación de distancia en servidor → `Interact()`.
- Network error fix: no hay new allocations; solo un bool y lógica condicional en el handler.

### Pendientes
1. Validar el fix de red con 2+ clientes: probar lobby→run→lobby con Steam real.
2. Si `NetDriverListenFailure` sigue ocurriendo durante ServerTravel, investigar si `Sessions->UpdateSession` tras el travel ayuda a re-registrar el socket Steam.

