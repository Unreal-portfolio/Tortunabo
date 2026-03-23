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

## 2026-03-19 - Diagnóstico NetChecksumMismatch al unirse a partida + fix OnNetworkFailure

### Contexto
- Error al intentar acceder a la partida de un amigo desde Standalone Game.
- Log analizado: `Source/Logs/Tortunabo_2.log` (sesión 12:24:30).

### Diagnóstico (causa raíz)
1. **Live Coding** recompiló `TN_ThrowableItemActor` en el cliente (11:24:38) antes de buscar lobbies.
2. Esto cambió los checksums de red de `TN_InventoryComponent` y `BP_GenericPickup` en el cliente.
3. El servidor (amigo) ya estaba corriendo con el build anterior → checksums distintos.
4. Al completar el join y cargar `LVL_HQ`, el engine detectó el mismatch y lanzó `NetChecksumMismatch`:
   - `BP_TortugaCharacter_C.[14]InventoryComponent` → 2196210924 vs 687168063
   - `BP_GenericPickup_C` → 2294192916 vs 364860978
5. El engine desconectó al cliente y lo devolvió al menú. La sesión quedó en estado zombie.

### Solución operativa (requerida)
- Ambos jugadores deben usar **el mismo binario compilado** sin Live Coding activo.
- En desarrollo: compilar con `Build.bat TortunaboEditor Win64 Development` antes de probar.
- No usar el botón "Hot Reload" / Live Coding durante sesiones multijugador.

### Cambios de código (`MP_GameInstance.cpp`)
- **`OnNetworkFailure`**: Refactor completo con lógica separada por categoría de error:
  - **Servidor (socket/driver)**: sin cambios funcionales, solo se añade `UpdateStatus` antes de return.
  - **Cliente - `NetChecksumMismatch`**: nuevo case en el switch; muestra mensaje explicativo sobre incompatibilidad de builds y destruye la sesión zombie.
  - **Cliente - `ConnectionLost/Timeout/FailureReceived/PendingConnectionFailure`**: nuevo bloque que oculta loading screen, muestra error y destruye sesión zombie para permitir reintentar sin estado corrupto.
  - **Resto** (OutdatedClient/Server, Unknown): solo loguea, sin acción agresiva.

### Networking
- Sin cambios en replicación ni flujo de sesión nominal.
- La destrucción de sesión en errores de cliente evita el warning `Player is not part of session` al volver al menú.

### Pendientes
1. Validar que al recibir `NetChecksumMismatch`, el jugador vuelve al menú y puede buscar/unirse de nuevo sin reiniciar.
2. Investigar si `BP_GenericPickup` hereda de alguna clase C++ que se modificó con el Live Coding rebuild — si es así, sería un buen candidato para añadir a un `UPROPERTY` de Blueprint estable.

## 2026-03-20 - Fix clientes no unen a Run, knockdown multicast, throwable rework, optimización red/audio

### Contexto
- Bugs en partida 4 jugadores: clientes no viajan a Run, knockdown visual no replica bien, emotes (cabeza) intermitentes, lag general.
- Bola lanzable necesita parábola, rebote con jugadores, knockdown condicional por velocidad.

### Cambios clave

#### 1. `TN_RunGameMode` — Fix clientes no se unen al mapa Run
- **`Public/Game/TN_RunGameMode.h`**: Añadidos overrides `PostLogin` y `Logout`.
- **`Private/Game/TN_RunGameMode.cpp`**:
  - `PostLogin`: llama `EnsurePlayerSpawned`, inicializa `TN_CoopPlayerState` (bIsAlive, FinishRank, etc.), actualiza `ConnectedPlayers`/`ExpectedPlayers` en GameState. **Bug raíz**: sin PostLogin, los clientes reconectando tras non-seamless travel no tenían su PlayerState inicializado ni se contaban en el GameState.
  - `Logout`: actualiza conteo de jugadores y llama `UpdateRoundProgressAndMaybeFinish` para cerrar ronda si todos los restantes terminaron.
  - `HandleStartingNewPlayer_Implementation`: añadidos logs de diagnóstico.

#### 2. `TortugaCharacter` — Knockdown multicast RPC
- **`Public/Player/TortugaCharacter.h`**: Añadido `UFUNCTION(NetMulticast, Reliable) MulticastApplyKnockdownVisual(bool bKnocked)`.
- **`Private/Player/TortugaCharacter.cpp`**:
  - `ApplyKnockdown`: tras aplicar visual en servidor, llama `MulticastApplyKnockdownVisual(true)`.
  - `RecoverFromKnockdown`: ídem con `false`.
  - `MulticastApplyKnockdownVisual_Implementation`: en clientes, aplica visual + bloquea/restaura movimiento del owner. Servidor es skip (ya aplicó).
  - `OnRep_IsKnockedDown` se mantiene como fallback para late-joiners.
  - **Belt-and-suspenders**: OnRep para estado, Multicast para inmediatez.

#### 3. `TortugaCharacter` — Mejora replicación emotes (Cabeza)
- **`Private/Player/TortugaCharacter.cpp`**:
  - `ServerSetEmote_Implementation`: guard antes de CancelEmote — solo cancela si hay emote activo o blend. Logs verbose con estado de Cabeza.
  - `OnRep_ReplicatedEmoteIndex`: mismo guard + logs verbose.
  - **Nota**: la animación de Cabeza se computa localmente via `TickEmote` en cada máquina usando el index replicado. Si Cabeza no anima, verificar que el componente `Cabeza` existe en BP_TortugaCharacter con nombre exacto.

#### 4. `TN_ThrowableItemActor` — Parábola + knockdown por velocidad
- **`Public/World/TN_ThrowableItemActor.h`**:
  - Nuevo: `float MinKnockdownSpeed = 600.0f` — velocidad mínima para noquear.
  - `Bounciness` de 0.35 → 0.55 para rebotes más visibles.
- **`Private/World/TN_ThrowableItemActor.cpp`**:
  - `OnMeshHit`: check `ProjectileMovement->Velocity.Size() >= MinKnockdownSpeed`. Si la bola va lenta, rebota sin noquear.
  - `BounceVelocityStopSimulatingThreshold` de 150 → 50 para que ruede más antes de spawn pickup.
- **`Public/Player/TortugaCharacter.h`**: `ThrowUpAngleDeg` de 15° → 35° para parábola pronunciada. Rango extendido a 60°.

#### 5. `ProximityVoiceComponent` — Optimización de audio
- **`Public/Voice/ProximityVoiceComponent.h`**:
  - `SendInterval` de 0.05s → 0.08s (12.5Hz vs 20Hz, -37.5% paquetes).
  - Nuevo: `int32 VoiceDownsampleFactor = 3` (48kHz → 16kHz, -66% datos por paquete).
  - Nuevo: `float SilenceHoldOffSeconds = 0.3f` (debounce para `bIsSpeaking`).
  - Nuevo: `float SilenceHoldOffTimer` (estado interno).
- **`Private/Voice/ProximityVoiceComponent.cpp`**:
  - `TickComponent`: decimation de MonoData por `VoiceDownsampleFactor` antes de buffer.
  - Speaking detection con hold-off: `bIsSpeaking` no vuelve a false hasta 0.3s de silencio continuo.
  - `Server_SendVoiceData` envía sample rate efectivo (VoiceSampleRate / DownsampleFactor).

#### 6. Optimización de red general
- **`TortugaCharacter`**: `SetNetUpdateFrequency` 45→30, `SetMinNetUpdateFrequency` 20→15.
- **`TN_StaminaComponent`**: `bIsSprinting` y `bSprintRequested` de `COND_None` → `COND_SkipOwner`.
- **`Config/DefaultEngine.ini`**:
  - `NetServerMaxTickRate` 60→45, `MaxNetTickRate` 60→45.
  - `InitialConnectTimeout` 15→30s (más margen para Steam Sockets en travel).
  - `ConnectionTimeout` 30→45s.

### Networking y rendimiento (estimado)
- **Paquetes de voz**: ~77% reducción por jugador (downsample 3x + sendInterval +60%).
- **Character updates**: ~33% menos frecuentes (45Hz→30Hz net, 60→45 server tick).
- **Stamina**: 2 propiedades menos replicadas al owner.
- **Knockdown**: Multicast Reliable garantiza que el visual llega a todos los clientes inmediatamente.

### Pendientes
1. Smoke test 4 jugadores: validar que clientes llegan a Run tras countdown en HQ.
2. Verificar en BP_TortugaCharacter que el SceneComponent `Cabeza` existe con nombre exacto.
3. Ajustar `MinKnockdownSpeed` (600) y `ThrowUpAngleDeg` (35°) según gameplay feel.
4. Probar calidad de voz con downsampling 3x — si es muy baja, reducir a 2x.
5. Monitorear lag con `stat net` durante partida 4 jugadores.

## 2026-03-20 (sesión 2) - Fix LVL_Run Listen failure, knockdown visual en remotos, optimización dormancy

### Contexto
- `ServerTravel` de HQ→Run falla con `LogNet: Error: LoadMap: failed to Listen(...)` → el socket Steam del mapa anterior no se ha liberado a tiempo. El host entra pero los clientes pierden conexión.
- Knockdown visual (`bIsKnockedDown`) no se ve en clientes remotos: `CharacterMovementComponent::NetworkSmoothingMode::Exponential` sobreescribe `SetRelativeRotation` del mesh cada frame.
- Necesidad de optimizar tráfico de red con muchos actores interactuables.

### Cambios clave

#### 1. `TN_RunGameMode` — Staging "WaitingForPlayers" antes de InProgress
- **`Public/Game/TN_RunGameMode.h`**: Nuevos miembros: `WaitingTimeoutTimerHandle`, `ExpectedPlayersFromLobby`, `bMatchStarted`, `WaitingForPlayersTimeoutSeconds` (default 15s). Nuevas funciones: `TryStartMatch()`, `OnWaitingTimeout()`.
- **`Private/Game/TN_RunGameMode.cpp`**:
  - `BeginPlay`: arranca en `ETNMatchFlowState::WaitingForPlayers` (no InProgress). Lee `PendingTravelPlayerCount` de `MP_GameInstance`. Inicia timeout de seguridad.
  - `PostLogin`: actualiza conteo y llama `TryStartMatch()`. Si todos los esperados llegaron → `OnWaitingTimeout()` arranca la carrera inmediatamente.
  - `Logout` (pre-match): reduce `ExpectedPlayersFromLobby` si alguien se desconecta durante staging.
  - `OnWaitingTimeout`: transiciona a `InProgress`, marca `MatchStartServerTime`.

#### 2. `MP_GameInstance` — PendingTravelPlayerCount + fix OnNetworkFailure
- **`Public/Multiplayer/MP_GameInstance.h`**: Nuevo `UPROPERTY int32 PendingTravelPlayerCount` — persiste a través del non-seamless travel.
- **`Private/Multiplayer/MP_GameInstance.cpp`**: `OnNetworkFailure` durante `bIsPendingTravel`: ya NO oculta loading screen ni resetea `bIsPendingTravel` — deja que `PostLoadMap` lo gestione. Esto permite que el mapa Run cargue correctamente aunque el Listen falle transitoriamente.

#### 3. `TN_HQGameMode` — Guarda player count antes de travel
- **`Private/Lobby/TN_HQGameMode.cpp`**: `BeginMatchTravel()` cuenta `ConnectedPlayers` y lo guarda en `GI->PendingTravelPlayerCount` antes de `ServerTravel`.

#### 4. `TortugaCharacter` — Fix knockdown visual en remotos
- **`Private/Player/TortugaCharacter.cpp`**: `ApplyKnockdownVisual(true)` ahora pone `CMC->NetworkSmoothingMode = ENetworkSmoothingMode::Disabled` para que el smoothing exponencial NO sobreescriba la rotación del mesh. `ApplyKnockdownVisual(false)` la restaura a `Exponential`.
- **Causa raíz**: en clientes remotos, el CMC con smoothing `Exponential` interpola la rotación del mesh hacia la posición replicada cada frame, borrando nuestro tilt de -100° pitch. Desactivarlo solo durante knockdown es seguro: el personaje está inmóvil.

#### 5. Optimización de red — Dormancy y condiciones de replicación
- **`TN_InteractableBase.cpp`**: `NetDormancy = DORM_DormantAll`, `NetUpdateFrequency = 4`, `MinNetUpdateFrequency = 2`. `SetInteractionEnabled()` llama `FlushNetDormancy()`.
- **`TN_PickupInteractableBase.cpp`**: `Interact()` y `InitializeFromInventoryItem()` llaman `FlushNetDormancy()` para que cambios se repliquen al despertar.
- **`TN_ThrowableItemActor.cpp`**: `NetUpdateFrequency = 30`, `MinNetUpdateFrequency = 15`.
- **`TN_CoopGameState.cpp`**: `ServerMatchElapsedTime` → `COND_SkipOwner`.
- **`ProximityVoiceComponent.cpp`**: `VoiceSampleRate` → `COND_InitialOnly`.

### Networking y rendimiento
- **Interactuables**: ~95% reducción de tráfico para pickups estáticos (DORM_DormantAll).
- **GameState**: `ServerMatchElapsedTime` ya no se envía al listen-server (él lo calcula).
- **Voice**: `VoiceSampleRate` se envía solo en initial replication (1 vez vs cada dirty check).
- **Travel**: Los clientes reconectan vía sesión Steam aunque el Listen falle inicialmente. El staging state espera hasta 15s.

### Pendientes
1. Smoke test 4 jugadores: validar staging → InProgress transition completa.
2. Si el Listen sigue fallando sin recuperación tras 15s, investigar forzar `FURL::Listen()` retry en PostLoadMap.
3. Ajustar `WaitingForPlayersTimeoutSeconds` según experiencia real.

## 2026-03-20 - Fix: Steam P2P Socket Race Condition en ServerTravel

### Diagnóstico
- **Bug**: Al hacer ServerTravel de LVL_HQ → LVL_Run, el listen server no puede crearse porque el socket Steam P2P del mapa anterior aún está ocupado.
- **Error en log**: `SteamSockets API: Error Cannot create listen socket. Already have a listen socket on P2P vport 7777` → `NetDriverListenFailure` → UE devuelve al menú.
- **Causa raíz**: Steam P2P sockets tardan ~133ms en liberarse tras destruir el NetDriver, pero UE intenta crear el nuevo listen server 108ms después de la destrucción (dentro del mismo `LoadMap` frame). Race condition de 25ms.

### Fix aplicado
- **Patrón**: Destruir el NetDriver **antes** de `ServerTravel`, esperar 500ms para que Steam libere el socket, y luego viajar.
- **Flujo**: `ClientTravel` a clientes remotos → `GEngine->DestroyNamedNetDriver()` → Timer 500ms → `ServerTravel`

### Archivos modificados
- `Public/Lobby/TN_HQGameMode.h`: Añadidos `DeferredTravelTimerHandle`, `PendingTravelURL`, `ExecuteDeferredTravel()`.
- `Private/Lobby/TN_HQGameMode.cpp`: `BeginMatchTravel()` ahora pre-cierra el NetDriver y usa timer de 500ms antes de `ServerTravel`. Incluidos `Engine/Engine.h` y `Engine/NetDriver.h`.
- `Public/Game/TN_RunGameMode.h`: Añadidos `DeferredTravelTimerHandle`, `PendingTravelURL`, `ExecuteDeferredTravel()`.
- `Private/Game/TN_RunGameMode.cpp`: `FinishRoundAndReturnToLobby()` mismo patrón de pre-close + deferred travel. Incluidos `Engine/Engine.h`, `Engine/NetDriver.h`, `GameFramework/PlayerController.h`.

### Otros warnings menores del log (no bloqueantes)
- `GetSocketByName(None): No SkeletalMesh for Component(CharacterMesh0)` → El BP no tiene skeletal mesh asignado aún.
- `'Cabeza' not found as exact name — matched 'Cabeza1' (fuzzy)` → Renombrar componente en BP a `Cabeza` exacto.
- `Player is not part of session (GameSession)` → Warning cosmético durante travel, no afecta funcionalidad.

## 2026-03-20 - Megasprint: Bugs críticos + Nuevos sistemas + Optimización MP

### Fase 1: Fix SteamSocket Race Condition (Listen Retry)
- **Problema**: El deferred travel de 0.5s no era suficiente. El socket Steam P2P seguía ocupado al crear el nuevo listen server.
- **Fix**: 
  - Delay aumentado a 1.0s en `TN_HQGameMode` y `TN_RunGameMode`.
  - Nuevo **retry de listen server** en `MP_GameInstance::HandlePostLoadMap`: si `OnNetworkFailure` detecta `NetDriverListenFailure` durante travel, marca `bNeedsListenRetry=true`. Tras cargar el mapa, un timer cada 400ms intenta `World->Listen()` hasta 5 veces. Si agota reintentos → destruye sesión y vuelve al menú.
- **Archivos**: `MP_GameInstance.h`, `MP_GameInstance.cpp`, `TN_HQGameMode.cpp`, `TN_RunGameMode.cpp`

### Fase 2: Fix Knockdown Visual en Otros Clientes
- **Problema**: `ApplyKnockdownVisual` usaba `GetMesh()` (SkeletalMeshComponent) que devolvía un componente sin mesh asignado en personajes blockout con StaticMesh.
- **Fix**: Nuevo campo `KnockdownVisualComp` (`TWeakObjectPtr<USceneComponent>`) cacheado en `BeginPlay`. Busca primero StaticMeshComponent hijo con mesh, fallback a SkeletalMesh. Logs de diagnóstico si el componente es null.
- **Archivos**: `TortugaCharacter.h`, `TortugaCharacter.cpp`

### Fase 3: Fix Jugador Fantasma Tras Terminar + "Eliminado" Erróneo
- **Problema**: `MarkPlayerFinished` no detenía el pawn → el CMC mantenía velocidad residual → otros jugadores veían al personaje avanzar infinitamente.
- **Fix**: Antes de `MovePlayerToSpectator`, llamar `StopMovementImmediately()` + `DisableMovement()` + `DisableInput`. Mismo patrón en `MarkPlayerDead`.
- **Archivos**: `TN_RunGameMode.cpp` (+ include `Character.h`, `CharacterMovementComponent.h`)

### Fase 4: Fix Bola Recta + Pickup Fantasma + Clipping Suelo
- **Bola recta**: La dirección de cámara incluía pitch → mirar abajo anulaba el arco. Fix: aplanar a pitch=0 antes de tiltar por `ThrowUpAngleDeg`.
- **Pickup fantasma**: `DORM_DormantAll` impedía replicación inicial de pickups dinámicos. Fix: `SetNetDormancy(DORM_Awake)` al inicializar, timer de 3s para volver a `DormantAll`.
- **Clipping suelo**: `FindGroundBelow` offset subido de 5cm a 15cm. `SpawnPickupAtLocation` ahora hace floor trace propio.
- **Archivos**: `TortugaCharacter.cpp`, `TN_PickupInteractableBase.cpp`, `TN_ThrowableItemActor.cpp`

#### 5A. `TN_ItemSpawnZone` (Nuevo)
- Actor con `UBoxComponent` como zona de spawn. Configurable: DataTable + RowNames + SpawnCount + MinSpacing + MaxRetries.
- BeginPlay (server): genera N posiciones random, valida suelo (line trace) y obstáculos (sweep), spawna pickups.
- **Archivos**: `Public/World/TN_ItemSpawnZone.h`, `Private/World/TN_ItemSpawnZone.cpp`

#### 5B. `TN_ButtonInteractable` (Nuevo)
- Hijo de `TN_DirectInteractableBase`. Al interactuar, avanza un `MoveTarget` al siguiente waypoint (array de FTransform, cíclico).
- Interpolación fluida en Tick (server, `FMath::VInterpConstantTo`). `MoveTarget` usa `SetReplicateMovement(true)`.
- Multicast cosmético para feedback BP.
- **Archivos**: `Public/World/TN_ButtonInteractable.h`, `Private/World/TN_ButtonInteractable.cpp`

#### 5C. `TN_PufferFishActor` — Pez Globo (Nuevo)
- Hijo de `ATN_ThrowableItemActor`. Tras delay aleatorio (1-3s), se infla ×5, empuja pawns en radio (400cm) con `LaunchCharacter`, knockdown si fuerza > umbral. Tras 1.5s, desinfla y spawna pickup.
- Estado replicado: `ETN_PufferState {Flying, Inflating, Deflated}` — un solo byte + OnRep.
- Nuevo `ETN_ItemUseType::PufferFish` en `TN_InventoryTypes.h`.
- `SpawnPickupAtLocation` y `bPickupSpawned` movidos de `private` a `protected` en `TN_ThrowableItemActor.h`.
- **Archivos**: `Public/World/TN_PufferFishActor.h`, `Private/World/TN_PufferFishActor.cpp`, `TN_ThrowableItemActor.h`, `TN_InventoryTypes.h`, `TortugaCharacter.cpp`

### Fase 6: Sistema DBNO (Down But Not Out) con revive por emote
- Implementado sistema DBNO para que los jugadores no mueran instantáneamente al caer en una death zone.
- Objetivo: dar a los compañeros una ventana de tiempo para revivir al jugador caído.

#### 1. `TN_CoopPlayerState` — Estado DBNO replicado
- **`Public/Core/TN_CoopPlayerState.h`**: Nuevas propiedades `bIsDBNO` (Replicated, all) y `DBNOBleedoutTimeRemaining` (Replicated, COND_OwnerOnly).
- **`Private/Core/TN_CoopPlayerState.cpp`**: Registradas en `GetLifetimeReplicatedProps`.

#### 2. `TN_RunGameMode` — Gestión del ciclo DBNO
- **`Public/Game/TN_RunGameMode.h`**: Nuevas funciones `EnterDBNO()`, `RevivePlayer()`. Nuevos settings `DBNOBleedoutSeconds=15`, `ReviveImmunitySeconds=2`. Miembros privados `DBNOPlayers` (TMap), `ReviveImmunePlayers` (TSet), `DBNOBleedoutTimerHandle`.
- **`Private/Game/TN_RunGameMode.cpp`**:
  - `EnterDBNO`: valida estado, aplica knockdown al pawn, registra en `DBNOPlayers`, inicia timer compartido `TickDBNOBleedout` (0.1s). Respeta inmunidad post-revive.
  - `RevivePlayer`: limpia DBNO, recupera de knockdown, otorga inmunidad temporal via lambda timer.
  - `TickDBNOBleedout`: decrementa bleedout, sincroniza `DBNOBleedoutTimeRemaining` al PlayerState, ejecuta `MarkPlayerDead` al expirar.
  - `CheckAllAliveDBNO`: si todos los vivos están en DBNO (nadie puede revivir), mata a todos inmediatamente.
  - `MarkPlayerDead` actualizado: limpia DBNO state del jugador al morir.
  - `BeginPlay` y `PostLogin`: inicializan `bIsDBNO=false`, `DBNOBleedoutTimeRemaining=-1`.

#### 3. `TN_DeathZoneVolume` — DBNO en vez de muerte directa
- **`Private/World/TN_DeathZoneVolume.cpp`**: `HandlePlayerDeath` ahora llama `RunGameMode->EnterDBNO(PC)` en vez de `MarkPlayerDead(PC)`.

#### 4. `TortugaCharacter` — Revive por emote canalizado
- **`Public/Player/TortugaCharacter.h`**:
  - `RecoverFromKnockdown` movido a `public` (necesario desde RunGameMode).
  - Nuevos settings: `ReviveRadiusCm=300`, `ReviveDurationSeconds=3`.
  - Nuevas propiedades replicadas: `bIsReviving` (all), `ReviveProgress` (COND_OwnerOnly).
  - Nuevas funciones privadas: `TryStartReviveChannel()`, `CancelReviveChannel()`, `TickReviveChannel()`.
  - Miembros privados: `ReviveTargetPC`, `ReviveChannelElapsed`, `ReviveChannelTimerHandle`.
- **`Private/Player/TortugaCharacter.cpp`**:
  - Nuevos includes: `Core/TN_CoopPlayerState.h`, `Game/TN_RunGameMode.h`.
  - `GetLifetimeReplicatedProps`: registra `bIsReviving` y `ReviveProgress`.
  - `TryStartReviveChannel`: busca el DBNO más cercano dentro del radio, inicia timer de 0.1s.
  - `TickReviveChannel`: valida emote activo, no knockeado, target DBNO en rango. Avanza progreso. Al completar llama `RunGameMode->RevivePlayer()` y cancela emote.
  - `CancelReviveChannel`: limpia todo el estado de canal.
  - `ServerSetEmote_Implementation`: hook de revive — `TryStartReviveChannel()` al empezar emote, `CancelReviveChannel()` al terminar.

#### 5. `TN_PlayerHUDWidget` — Hooks BP para DBNO/Revive
- **`Public/UI/HUD/TN_PlayerHUDWidget.h`**: Nuevos BlueprintImplementableEvent `OnDBNOStateChanged(bIsDBNO, BleedoutRemaining)` y `OnReviveProgressUpdated(Progress01, bIsReviving)`. Nuevo tracking state: `bLastDBNO`, `bLastReviving`, `LastReviveProgress`.
- **`Private/UI/HUD/TN_PlayerHUDWidget.cpp`**: NativeTick pollea `TN_CoopPlayerState::bIsDBNO` y `TortugaCharacter::bIsReviving/ReviveProgress` para disparar hooks BP.

#### 6. `AGENTS.md` — Documentación DBNO
- Nueva sección "## DBNO system (Down But Not Out)" con flujo, estado replicado, config, mecánica de revive y hooks de HUD.

### Networking y rendimiento
- **Nuevas propiedades replicadas**: 2 en PlayerState (`bIsDBNO`, `DBNOBleedoutTimeRemaining`), 2 en Character (`bIsReviving`, `ReviveProgress`). `DBNOBleedoutTimeRemaining` y `ReviveProgress` son COND_OwnerOnly.
- **Timers**: `TickDBNOBleedout` solo corre si hay jugadores en DBNO. `TickReviveChannel` solo corre si alguien está canalizando.
- **CheckAllAliveDBNO**: O(N) sobre PlayerArray, solo se llama al entrar en DBNO (no cada tick).

### Pendientes
1. Crear widgets BP en WBP_PlayerHUD para mostrar indicador de DBNO (contador de bleedout) y barra de revive.
2. Smoke test: validar flujo DeathZone → DBNO → Emote → Revive con 2+ jugadores.
3. Smoke test: validar que si todos caen en DBNO, todos mueren y se muestran resultados.
4. Ajustar `DBNOBleedoutSeconds` (15s) y `ReviveDurationSeconds` (3s) según gameplay feel.
5. Considerar feedback visual/audio adicional para el canal de revive (partículas, sonido).

## 2026-03-20 (sesión 4) - Fase 6: Sistema DBNO (Down But Not Out) con revive por emote

### Contexto
- Pendiente desde la sesión anterior: implementar sistema DBNO para que los jugadores no mueran instantáneamente al caer en una death zone.
- Objetivo: dar a los compañeros una ventana de tiempo para revivir al jugador caído.

### Cambios clave

#### 1. `TN_CoopPlayerState` — Estado DBNO replicado
- **`Public/Core/TN_CoopPlayerState.h`**: Nuevas propiedades `bIsDBNO` (Replicated, all) y `DBNOBleedoutTimeRemaining` (Replicated, COND_OwnerOnly).
- **`Private/Core/TN_CoopPlayerState.cpp`**: Registradas en `GetLifetimeReplicatedProps`.

#### 2. `TN_RunGameMode` — Gestión del ciclo DBNO
- **`Public/Game/TN_RunGameMode.h`**: Nuevas funciones `EnterDBNO()`, `RevivePlayer()`. Nuevos settings `DBNOBleedoutSeconds=15`, `ReviveImmunitySeconds=2`. Miembros privados `DBNOPlayers` (TMap), `ReviveImmunePlayers` (TSet), `DBNOBleedoutTimerHandle`.
- **`Private/Game/TN_RunGameMode.cpp`**:
  - `EnterDBNO`: valida estado, aplica knockdown al pawn, registra en `DBNOPlayers`, inicia timer compartido `TickDBNOBleedout` (0.1s). Respeta inmunidad post-revive.
  - `RevivePlayer`: limpia DBNO, recupera de knockdown, otorga inmunidad temporal via lambda timer.
  - `TickDBNOBleedout`: decrementa bleedout, sincroniza `DBNOBleedoutTimeRemaining` al PlayerState, ejecuta `MarkPlayerDead` al expirar.
  - `CheckAllAliveDBNO`: si todos los vivos están en DBNO (nadie puede revivir), mata a todos inmediatamente.
  - `MarkPlayerDead` actualizado: limpia DBNO state del jugador al morir.
  - `BeginPlay` y `PostLogin`: inicializan `bIsDBNO=false`, `DBNOBleedoutTimeRemaining=-1`.

#### 3. `TN_DeathZoneVolume` — DBNO en vez de muerte directa
- **`Private/World/TN_DeathZoneVolume.cpp`**: `HandlePlayerDeath` ahora llama `RunGameMode->EnterDBNO(PC)` en vez de `MarkPlayerDead(PC)`.

#### 4. `TortugaCharacter` — Revive por emote canalizado
- **`Public/Player/TortugaCharacter.h`**:
  - `RecoverFromKnockdown` movido a `public` (necesario desde RunGameMode).
  - Nuevos settings: `ReviveRadiusCm=300`, `ReviveDurationSeconds=3`.
  - Nuevas propiedades replicadas: `bIsReviving` (all), `ReviveProgress` (COND_OwnerOnly).
  - Nuevas funciones privadas: `TryStartReviveChannel()`, `CancelReviveChannel()`, `TickReviveChannel()`.
  - Miembros privados: `ReviveTargetPC`, `ReviveChannelElapsed`, `ReviveChannelTimerHandle`.
- **`Private/Player/TortugaCharacter.cpp`**:
  - Nuevos includes: `Core/TN_CoopPlayerState.h`, `Game/TN_RunGameMode.h`.
  - `GetLifetimeReplicatedProps`: registra `bIsReviving` y `ReviveProgress`.
  - `TryStartReviveChannel`: busca el DBNO más cercano dentro del radio, inicia timer de 0.1s.
  - `TickReviveChannel`: valida emote activo, no knockeado, target DBNO en rango. Avanza progreso. Al completar llama `RunGameMode->RevivePlayer()` y cancela emote.
  - `CancelReviveChannel`: limpia todo el estado de canal.
  - `ServerSetEmote_Implementation`: hook de revive — `TryStartReviveChannel()` al empezar emote, `CancelReviveChannel()` al terminar.

#### 5. `TN_PlayerHUDWidget` — Hooks BP para DBNO/Revive
- **`Public/UI/HUD/TN_PlayerHUDWidget.h`**: Nuevos BlueprintImplementableEvent `OnDBNOStateChanged(bIsDBNO, BleedoutRemaining)` y `OnReviveProgressUpdated(Progress01, bIsReviving)`. Nuevo tracking state: `bLastDBNO`, `bLastReviving`, `LastReviveProgress`.
- **`Private/UI/HUD/TN_PlayerHUDWidget.cpp`**: NativeTick pollea `TN_CoopPlayerState::bIsDBNO` y `TortugaCharacter::bIsReviving/ReviveProgress` para disparar hooks BP.

#### 6. `AGENTS.md` — Documentación DBNO
- Nueva sección "## DBNO system (Down But Not Out)" con flujo, estado replicado, config, mecánica de revive y hooks de HUD.

### Networking y rendimiento
- **Nuevas propiedades replicadas**: 2 en PlayerState (`bIsDBNO`, `DBNOBleedoutTimeRemaining`), 2 en Character (`bIsReviving`, `ReviveProgress`). `DBNOBleedoutTimeRemaining` y `ReviveProgress` son COND_OwnerOnly.
- **Timers**: `TickDBNOBleedout` solo corre si hay jugadores en DBNO. `TickReviveChannel` solo corre si alguien está canalizando.
- **CheckAllAliveDBNO**: O(N) sobre PlayerArray, solo se llama al entrar en DBNO (no cada tick).

### Pendientes
1. Crear widgets BP en WBP_PlayerHUD para mostrar indicador de DBNO (contador de bleedout) y barra de revive.
2. Smoke test: validar flujo DeathZone → DBNO → Emote → Revive con 2+ jugadores.
3. Smoke test: validar que si todos caen en DBNO, todos mueren y se muestran resultados.
4. Ajustar `DBNOBleedoutSeconds` (15s) y `ReviveDurationSeconds` (3s) según gameplay feel.
5. Considerar feedback visual/audio adicional para el canal de revive (partículas, sonido).

## 2026-03-20 (sesión 6) - Fix Clipping Pickups + Sistema Cosméticos Completo + Guía Editor

### Contexto
- Pickups colocados en nivel clipean en el suelo (pivote del mesh en centro).
- Sistema de cosméticos requería: socket Sombrero en personaje, DataTable de cascos, replicación de mesh por HelmetId, widget C++ de menú.

### Cambios clave

#### 1. Fix Clipping — `TN_InteractableBase` + `TN_PickupInteractableBase`
- **`Public/World/TN_InteractableBase.h`**: Nueva UPROPERTY `float MeshFloorOffset = 0.f` (`EditAnywhere, BlueprintReadWrite`). Permite ajuste per-instancia desde el editor.
- **`Private/World/TN_InteractableBase.cpp`**: `BeginPlay` aplica `Mesh->SetRelativeLocation(FVector(0,0,MeshFloorOffset))` si el offset no es cero.
- **`Private/World/TN_PickupInteractableBase.cpp`**: `InitializeFromInventoryItem` y `OnRep_PickupItem` usan `Mesh->CalcLocalBounds()` para auto-calcular `MeshFloorOffset = BoxExtent.Z * ScaleZ`. Pickups dinámicos (spawneados en runtime) se auto-ajustan siempre.
- **Para pickups ya colocados en nivel**: ajustar `MeshFloorOffset` por instancia en Details, o usar End (snap to floor) en el editor.

#### 2. Sistema de Cosméticos — Nuevo tipo `FTN_HelmetData`
- **`Public/Core/TN_CosmeticsTypes.h`** (NUEVO): `USTRUCT FTN_HelmetData : FTableRowBase` con campos: `HelmetId`, `DisplayName`, `DisplayMesh` (UStaticMesh*), `Icon` (UTexture2D*), `MeshScale`, `MeshOffset`, `MeshRotation`. DataTable `DT_Helmets` debe crearse en `Content/Blueprints/Gameplay/Cosmetics/`.

#### 3. Sistema de Cosméticos — DataTable en GameInstance
- **`Public/Multiplayer/MP_GameInstance.h`**: Nueva UPROPERTY `TObjectPtr<UDataTable> HelmetDataTable` + getter público `GetHelmetDataTable()`. Asignar `DT_Helmets` en `BP_GameInstance → Class Defaults → Cosmetics`.

#### 4. Sistema de Cosméticos — Replicación `OnRep_EquippedHelmetId`
- **`Public/Core/TN_CoopPlayerState.h`**: `EquippedHelmetId` cambia de `Replicated` a `ReplicatedUsing = OnRep_EquippedHelmetId`.
- **`Private/Core/TN_CoopPlayerState.cpp`**: `OnRep_EquippedHelmetId` → `GetPawn()` → cast a `ATortugaCharacter` → llama `UpdateHelmetMesh(EquippedHelmetId)`. Include de `TortugaCharacter.h`.

#### 5. Sistema de Cosméticos — Socket Sombrero + Mesh en Personaje
- **`Public/Player/TortugaCharacter.h`**:
  - Include `Core/TN_CosmeticsTypes.h` y `UStaticMeshComponent` forward-declared.
  - Nueva UPROPERTY `VisibleAnywhere UStaticMeshComponent* HelmetMeshComp`.
  - Nueva función pública `BlueprintCallable UpdateHelmetMesh(FName HelmetId)`.
  - Nuevo campo privado `TWeakObjectPtr<USceneComponent> SombreroSocket`.
- **`Private/Player/TortugaCharacter.cpp`**:
  - Includes añadidos: `StaticMeshComponent.h`, `Engine/DataTable.h`, `Core/TN_CosmeticsTypes.h`, `Multiplayer/MP_GameInstance.h`.
  - Constructor: `HelmetMeshComp` creado adjunto a root, `NoCollision`, `bIsReplicated=false`, `HiddenInGame=true`.
  - `BeginPlay`: busca `Sombrero` via `FindChildByName`, re-adjunta `HelmetMeshComp` al socket. Timer-for-next-tick aplica `UpdateHelmetMesh` desde `CoopPlayerState::EquippedHelmetId` (el PlayerState puede no estar disponible en el mismo frame de BeginPlay).
  - `UpdateHelmetMesh`: obtiene `HelmetDataTable` desde `UMP_GameInstance`, lookup por `HelmetId`, aplica mesh/escala/offset/rotación/visibilidad.

#### 6. Sistema de Cosméticos — Fix Desequipar + Apply en Servidor
- **`Private/Player/MP_GamePlayerController.cpp`**:
  - `ServerSetEquippedHelmet_Implementation`: permite `NAME_None` (desequipar). Tras setear `EquippedHelmetId` en PlayerState, llama `TurtleChar->UpdateHelmetMesh(HelmetId)` directamente (OnRep no dispara en authority).
  - Nueva función `RequestUnequipHelmet()`: llama `GI->EquipHelmet(NAME_None)` + `ServerSetEquippedHelmet(NAME_None)`.
  - Include de `Player/TortugaCharacter.h`.
- **`Public/Player/MP_GamePlayerController.h`**: Declaración de `RequestUnequipHelmet()`.

#### 7. Sistema de Cosméticos — Widget `UTN_CosmeticsMenuWidget` (NUEVO)
- **`Public/UI/HUD/TN_CosmeticsMenuWidget.h`** y **`Private/UI/HUD/TN_CosmeticsMenuWidget.cpp`**: Base C++ del menú de cosméticos.
  - Widgets `BindWidgetOptional`: `UnequipButton`, `CloseButton`, `HelmetGrid`.
  - `NativeConstruct`: vincula botones + llama `RefreshHelmetGrid()`.
  - `RefreshHelmetGrid()`: obtiene `UnlockedHelmetIds` de `GI` → dispara `OnHelmetGridRefreshed(Ids, CurrentEquipped)` (BlueprintImplementableEvent).
  - `RequestEquip(FName)` / `RequestUnequip()`: llaman a `PC::RequestEquipHelmet` / `PC::RequestUnequipHelmet` + disparan `OnHelmetEquipped` localmente para feedback inmediato.
  - `CloseMenu()`: oculta widget + restaura `FInputModeGameOnly` + foco al viewport.
  - `GetHelmetData(FName, OutData)`: lookup en DT_Helmets para que el BP construya cada entrada del grid.
  - `GetEquippedHelmetId()`: consulta `GI->GetEquippedHelmetId()`.

### Pasos Editor tras compilar
1. **DT_Helmets**: DataTable → Row Struct = `FTN_HelmetData`. Una fila por casco: asignar `HelmetId` (= RowName), `DisplayMesh`, `Icon`, `DisplayName`.
2. **BP_GameInstance → Class Defaults → Cosmetics → HelmetDataTable**: asignar `DT_Helmets`.
3. **BP_TortugaCharacter**: añadir SceneComponent hijo con nombre exacto `Sombrero`, posicionarlo sobre la cabeza de la tortuga.
4. **WBP_CosmeticsMenu** (hijo de `UTN_CosmeticsMenuWidget`): implementar `OnHelmetGridRefreshed` y `OnHelmetEquipped`. Opcional: añadir `UnequipButton` y `CloseButton` con esos nombres exactos.
5. **BP_GamePlayerController → Class Defaults → CosmeticsWidgetClass**: asignar `WBP_CosmeticsMenu`.
6. **BP_CosmeticsStation** (hijo de `ATN_CosmeticsStationInteractable`) en el lobby: al interactuar abre el menú automáticamente.
7. **Pickups en nivel**: seleccionar todos los pickup actors y ajustar `MeshFloorOffset` (Details) o pulsar `End` para snap to floor.

### Networking
- `EquippedHelmetId` ya replicado (DOREPLIFETIME sin cambio de condición). OnRep añadido.
- `HelmetMeshComp`: `bIsReplicated=false` — solo cosmético local, driven por el `EquippedHelmetId` replicado.
- Sin nuevas RPCs. Flujo: widget → `RequestEquipHelmet` → `ServerSetEquippedHelmet` (Server RPC ya existente) → `EquippedHelmetId` replicado → `OnRep_EquippedHelmetId` en clientes → `UpdateHelmetMesh`.

### Pendientes
1. Crear `DT_Helmets` con filas de prueba en el Editor.
2. Añadir SceneComponent `Sombrero` en `BP_TortugaCharacter`.
3. Crear `WBP_CosmeticsMenu` y diseñar el grid (evento `OnHelmetGridRefreshed`).
4. Asignar `WBP_CosmeticsMenu` en `BP_GamePlayerController`.
5. Colocar `BP_CosmeticsStation` en el lobby.

## 2026-03-22 - Sistema radial de Emotes + Quick Chat estilo GTA

### Contexto
- Se pidió reemplazar la selección por múltiples inputs discretos con dos ruedas radiales hold-to-open: una para emotes y otra para quick chat.
- Requisito clave: lógica completa en C++, soporte mouse+gamepad, multiplayer server-authoritative, sin spam de RPCs ni replicación por frame.

### Cambios clave

#### 1. UI radial base reutilizable
- Nuevos archivos:
  - `Source/Tortunabo/Public/UI/HUD/TN_RadialWheelTypes.h`
  - `Source/Tortunabo/Public/UI/HUD/TN_RadialWheelWidgetBase.h`
  - `Source/Tortunabo/Private/UI/HUD/TN_RadialWheelWidgetBase.cpp`
- `UTN_RadialWheelWidgetBase` calcula selección angular en C++ (`atan2`, deadzone, confirmación) y expone solo eventos/hooks BP para renderizar highlight, iconos y animaciones.
- La actualización del vector de selección no usa tick global: se alimenta desde un timer del `PlayerController` solo mientras la rueda está abierta.

#### 2. Catálogos editables por DataAsset
- Nuevos archivos:
  - `Source/Tortunabo/Public/UI/HUD/TN_EmoteWheelDataAsset.h`
  - `Source/Tortunabo/Private/UI/HUD/TN_EmoteWheelDataAsset.cpp`
  - `Source/Tortunabo/Public/UI/HUD/TN_QuickChatWheelDataAsset.h`
  - `Source/Tortunabo/Private/UI/HUD/TN_QuickChatWheelDataAsset.cpp`
- Ambos catálogos validan IDs únicos en editor (`IsDataValid`) y generan una vista compacta para la rueda (`FTN_RadialWheelEntryView`).
- El orden visual queda desacoplado del networking: la rueda pinta según el array del DataAsset, pero la red solo usa IDs compactos (`uint8`).

#### 3. `MP_GamePlayerController` — apertura/cierre de ruedas e input mínimo
- `Source/Tortunabo/Public/Player/MP_GamePlayerController.h`
- `Source/Tortunabo/Private/Player/MP_GamePlayerController.cpp`
- Nuevas soft references por defecto:
  - `IA_OpenEmoteWheel`
  - `IA_OpenChatWheel`
  - `IA_RadialNavigate`
- El controller ahora:
  - crea `WBP` de rueda radial (`EmoteWheelWidgetClass`, `QuickChatWheelWidgetClass`)
  - abre la rueda en `Started`
  - confirma al soltar (`Completed/Canceled`)
  - usa mouse (cursor vs centro viewport) o stick (`Axis2D`) sin crear 10 acciones de emote/chat
  - bloquea solo el look mientras la rueda está abierta, restaurando `GameOnly` al cerrar
- El update de selección usa `SetTimer(..., 1/60s, true)` únicamente mientras la rueda está visible.

#### 4. Quick Chat — red compacta + JIP-friendly
- `Source/Tortunabo/Public/Core/TN_MatchFlowTypes.h`
- `Source/Tortunabo/Public/Core/TN_CoopGameState.h`
- `Source/Tortunabo/Private/Core/TN_CoopGameState.cpp`
- `FTN_QuickChatEntry` deja de replicar `SenderName` y ahora usa:
  - `Sequence` (`uint16`)
  - `SenderPlayerId` (`uint16`)
  - `MessageID` (`uint8`)
  - `ServerTime` (`float`)
- `ATN_CoopGameState` mantiene historial rolling de 32 mensajes y en `OnRep_QuickChatHistory` hace broadcast incremental por secuencia para que la UI pueda appendear mensajes nuevos y para que join-in-progress reciba el buffer actual.
- `AMP_GamePlayerController::ResolveQuickChatDisplayData()` resuelve localmente nombre+texto+icono usando `GameState + QuickChatWheelDataAsset`.

#### 5. Rate limiting server-side por jugador
- `Source/Tortunabo/Public/Core/TN_CoopPlayerState.h`
- `Source/Tortunabo/Private/Core/TN_CoopPlayerState.cpp`
- Nuevos cooldown helpers server-only:
  - `CanServerSendQuickChat/MarkServerQuickChatSent`
  - `CanServerPlayEmote/MarkServerEmotePlayed`
- El estado anti-spam vive en `PlayerState`, no en UI ni en GameMode, así funciona igual en listen y dedicated.

#### 6. `TortugaCharacter` — integración segura con rueda de emotes
- `Source/Tortunabo/Public/Player/TortugaCharacter.h`
- `Source/Tortunabo/Private/Player/TortugaCharacter.cpp`
- Nuevo `EmoteWheelDataAsset` para validar IDs/cooldowns desde el mismo personaje.
- `RequestWheelEmote(uint8)` reutiliza el backend existente (`TriggerEmote` / `ServerSetEmote`) para no romper revive DBNO ni la replicación actual de `ReplicatedEmoteIndex`.
- `ServerSetEmote_Implementation` ahora valida en servidor:
  - ID válido en catálogo
  - jugador vivo / no DBNO / no knockdown
  - cooldown por emote en `TN_CoopPlayerState`
- Se añadió `ClientRejectEmote` para corregir la predicción local si el servidor rechaza el emote (evita que el owner se quede viendo un emote inválido).

### Networking y rendimiento
- Sin replicación por frame de hover/selección radial: solo se replica el resultado confirmado.
- Quick chat usa `Reliable Server RPC + historial replicado`, apropiado porque debe llegar y soportar join-in-progress.
- Emotes reutilizan el estado replicado existente del personaje; no se añadió multicast extra para cada hover ni selección intermedia.
- El timer 60 Hz existe solo con la rueda abierta; coste cero en gameplay normal.

### Pendientes
1. Crear assets Enhanced Input en `/Game/Blueprints/Gameplay/Controls/`: `IA_OpenEmoteWheel`, `IA_OpenChatWheel`, `IA_RadialNavigate`.
2. Crear `WBP_EmoteWheel` y `WBP_QuickChatWheel` heredando de `UTN_RadialWheelWidgetBase`.
3. Asignar `EmoteWheelDataAsset`, `QuickChatWheelDataAsset` y widget classes en `BP_GamePlayerController`.
4. Smoke test con 2-4 jugadores: host/listen + dedicated PIE, deadzone cancel, gamepad stick, join-in-progress quick chat history.
5. Si más adelante los emotes migran de blockout a `AnimMontage`, reutilizar `FTN_EmoteWheelEntry::Montage` sin tocar la rueda ni el networking.

## 2026-03-23 - Ruedas radiales: hook C++ de quick chat al HUD + soporte de AnimMontage en emotes

### Contexto
- El backend radial ya existía en C++, pero quedaban dos huecos de integración para dejar la solución alineada con la arquitectura pedida:
  1. El `HUD` aún no consumía en C++ el historial replicado de quick chat para alimentar un feed visual sin lógica en Blueprint.
  2. El catálogo de emotes ya exponía `UAnimMontage* Montage`, pero el personaje todavía no lo usaba al arrancar/cancelar emotes desde la rueda.

### Cambios clave
- `Source/Tortunabo/Public/UI/HUD/TN_CoopFlowHUDWidget.h`
- `Source/Tortunabo/Private/UI/HUD/TN_CoopFlowHUDWidget.cpp`
  - Nuevo binding/unbinding automático a `ATN_CoopGameState::OnQuickChatReceived`.
  - Replay del historial `QuickChatHistory` al crear/recrear el HUD (importante tras non-seamless travel y join-in-progress).
  - Nuevo evento visual `OnQuickChatEntryReceived(...)` para que el WBP solo pinte líneas/iconos, sin lógica de red ni parsing.
- `Source/Tortunabo/Public/Player/TortugaCharacter.h`
- `Source/Tortunabo/Private/Player/TortugaCharacter.cpp`
  - Nuevos helpers `ResolveWheelEmoteEntry`, `PlayWheelEmoteMontage` y `StopWheelEmoteMontage`.
  - `StartEmoteLocally()` ahora reinicia el montage configurado en el `DataAsset` si existe.
  - `CancelEmoteLocalOnly()` hace stop con blend-out del montage del emote activo, manteniendo el blockout actual como fallback visual.

### Networking y rendimiento
- Quick chat sigue sin RPC spam: solo `ServerSendQuickChat` al confirmar y luego historial replicado compacto en `GameState`.
- El feed del HUD se reconstruye desde el buffer replicado; no hay polling adicional por frame aparte del refresh ya existente del widget.
- Emotes no replican montages ni assets: solo `EmoteID`/estado existente; cada cliente resuelve localmente el montage desde su `DataAsset`.

### Pendientes
1. Crear en UMG el feed visual que consuma `OnQuickChatEntryReceived`.
2. Asignar montages reales en `DA_EmoteWheelCatalog` si se quiere reemplazar el blockout por animación de personaje.
3. Smoke test de listen/dedicated con host + cliente: abrir rueda, confirmar chat, verificar replay del historial tras travel y que el montage se vea en proxies remotos.

## 2026-03-23 - Bugfix Multiplayer: Travel, PufferFish, Death System

### Contexto
- Tres bugs críticos detectados durante smoke tests multiplayer.

### Cambios clave

#### FIX 1 — Cliente/Host no viajan a LVL_Run (P1 CRÍTICO)
- **Causa raíz (host)**: Sin `DestroyNamedNetDriver`, el socket Steam P2P no se libera antes de que `ServerTravel` intente crear un listen server nuevo → `NetDriverListenFailure` → host no entra al mapa.
- **Causa raíz (cliente)**: `ClientTravel` manual con URL de mapa local (path, no connect string Steam) → el cliente cargaba el mapa en standalone → no reconectaba al host.
- **Fix — flujo de 4 pasos**:
  1. `Pawn->Destroy()` para todos los PCs (WASAPI cleanup).
  2. `ClientNotifyServerTravel` (nuevo Client RPC en `AMP_GamePlayerController`) → marca `bIsPendingTravel` en el GameInstance del cliente + muestra loading screen.
  3. `FlushNet` + `DestroyNamedNetDriver` (libera socket Steam; clientes reciben ConnectionLost → auto-rejoin porque `bIsPendingTravel=true`).
  4. Timer 1s + `ServerTravel(URL)` (host carga nuevo mapa, crea listen server nuevo).
- **Red de seguridad**: `OnNetworkFailure` ahora también intenta auto-rejoin si hay sesión Steam activa (fallback si el RPC no llegó).
- Archivos nuevos/modificados:
  - `Source/Tortunabo/Public/Player/MP_GamePlayerController.h` — Nuevo `ClientNotifyServerTravel` (Client, Reliable).
  - `Source/Tortunabo/Private/Player/MP_GamePlayerController.cpp` — Implementación del RPC.
  - `Source/Tortunabo/Public/Multiplayer/MP_GameInstance.h` — Nuevo `NotifyClientPendingTravel()`.
  - `Source/Tortunabo/Private/Multiplayer/MP_GameInstance.cpp` — Implementación + fallback en `OnNetworkFailure`.
  - `Source/Tortunabo/Private/Lobby/TN_HQGameMode.cpp` — BeginMatchTravel con flujo de 4 pasos.
  - `Source/Tortunabo/Private/Game/TN_RunGameMode.cpp` — FinishRoundAndReturnToLobby con mismo patrón.

#### FIX 2 — PufferFish: knockdown visual no se muestra + knockdown por impacto directo
- **Causa 1**: `OnMeshHit` heredado del padre (`TN_ThrowableItemActor`) aplicaba knockdown al impactar directamente. El PufferFish solo debe knockear durante la inflación.
- **Fix**: En `ATN_PufferFishActor::BeginPlay`, se remueve `Mesh->OnComponentHit` del padre. Solo la explosión de inflación aplica knockdown.
- **Causa 2**: `KnockdownVisualComp` podía ser null en clientes remotos si el multicast llegaba antes de que BeginPlay encontrara el componente.
- **Fix**: `ApplyKnockdownVisual` ahora intenta resolver `KnockdownVisualComp` en el momento si es null, usando la misma lógica de búsqueda que BeginPlay.
- Archivos modificados:
  - `Source/Tortunabo/Private/World/TN_PufferFishActor.cpp` — Removido OnComponentHit binding.
  - `Source/Tortunabo/Private/Player/TortugaCharacter.cpp` — Fallback de resolución de KnockdownVisualComp en ApplyKnockdownVisual.

#### FIX 3 — Rework sistema de muerte: ocultar pawn + spawn pickup de rescate
- **Antes**: `MarkPlayerDead` ocultaba extremidades y dejaba el pawn como cadáver interactuable.
- **Ahora**: `MarkPlayerDead` oculta el pawn completamente (`SetActorHiddenInGame + disable collision`) y spawnea un `TN_RescuePickup` en la posición de muerte. El compañero interactúa con el pickup para revivir al muerto en esa ubicación.
- Nuevos archivos:
  - `Source/Tortunabo/Public/World/TN_RescuePickup.h`
  - `Source/Tortunabo/Private/World/TN_RescuePickup.cpp`
- Archivos modificados:
  - `Source/Tortunabo/Public/Game/TN_RunGameMode.h` — Añadido `RescuePickupClass` (EditDefaultsOnly), `RescuePickups` tracking map.
  - `Source/Tortunabo/Private/Game/TN_RunGameMode.cpp` — `MarkPlayerDead` oculta pawn y spawnea pickup. `RevivePlayer` restaura visibilidad y destruye pickup.
  - `Source/Tortunabo/Private/Player/TortugaCharacter.cpp` — `TryInteract` y `ServerTryReviveNearby` solo buscan DBNO, no muertos. Muertos se rescatan vía pickup.

### Setup requerido en Editor
1. **`BP_RunGameMode → Class Defaults → Run|Death → RescuePickupClass`**: Crear un BP hijo de `TN_RescuePickup`, asignar mesh de caparazón/alma, y configurar aquí. Sin esto, los muertos no tendrán pickup de rescate.

### Pendientes
1. Crear `BP_RescuePickup` en el Editor (hijo de `TN_RescuePickup`), asignar mesh visual.
2. Smoke test completo: Host + Cliente viajar HQ→Run, muerte → pickup → rescate → volver a lobby.
3. Verificar que el knockdown visual del PufferFish se ve en clientes remotos.

## 2026-03-23 - Sprint: Bugfix Multiplayer (8 fixes)

### Contexto
- Testing multiplayer real (Steam, 2 jugadores) reveló múltiples bugs de replicación, dormancy, espectador y timing.

### Cambios clave

#### FIX 1 — [P0 CRÍTICO] Rescue Pickup no revive al jugador muerto
- **Causa raíz**: `MovePlayerToSpectator()` → `EnterSpectateMode()` → UE llama `UnPossess()` internamente. Después, `RevivePlayer()` y `TN_RescuePickup::Interact()` llaman `PC->GetPawn()` que devuelve nullptr. Todo el bloque de restauración se saltaba.
- **Fix**: Nuevo `TMap<int32, TWeakObjectPtr<APawn>> DeadPlayerPawns` en `TN_RunGameMode`. `MarkPlayerDead` guarda la referencia al pawn ANTES de `MovePlayerToSpectator`. `RevivePlayer` busca el pawn en `DeadPlayerPawns` si `GetPawn()` es null. `TN_RescuePickup::Interact` usa `RunGM->GetDeadPlayerPawn()` para teletransportar. Tras re-`Possess()`, se limpia la entrada del mapa.
- Archivos modificados:
  - `Source/Tortunabo/Public/Game/TN_RunGameMode.h` — `DeadPlayerPawns`, `GetDeadPlayerPawn()`.
  - `Source/Tortunabo/Private/Game/TN_RunGameMode.cpp` — `MarkPlayerDead`, `RevivePlayer`, nuevo `GetDeadPlayerPawn`.
  - `Source/Tortunabo/Private/World/TN_RescuePickup.cpp` — `Interact` usa `GetDeadPlayerPawn`.

#### FIX 2 — [P1] Pickup recogido sigue visible en cliente
- **Causa raíz**: `TN_InteractableBase` usa `DORM_DormantAll`. `FlushNetDormancy()` en `Interact()` no despierta el canal de replicación sin primero cambiar a `DORM_Awake`.
- **Fix**: Antes de asignar `bTaken=true`, se llama `SetNetDormancy(DORM_Awake)`. Después de `FlushNetDormancy()`, se añade `ForceNetUpdate()`. Timer 2s para volver a `DORM_DormantAll`.
- Archivos modificados:
  - `Source/Tortunabo/Private/World/TN_PickupInteractableBase.cpp` — `Interact()`.

#### FIX 3 — [P1] PufferFish ahora empuja a TODOS (incluido lanzador)
- **Antes**: `Inflate()` añadía `GetInstigator()` a actors ignorados del overlap. El lanzador era inmune.
- **Fix**: Eliminada la línea `Params.AddIgnoredActor(GetInstigator())`. Ahora la explosión afecta a todos.
- Aumentados defaults para efecto exagerado: `InflateRadius 400→600`, `InflatePushForce 1500→2500`, `MinKnockdownForce 800→600`, componente vertical Z `0.3→0.5`.
- Archivos modificados:
  - `Source/Tortunabo/Private/World/TN_PufferFishActor.cpp` — `Inflate()`.
  - `Source/Tortunabo/Public/World/TN_PufferFishActor.h` — defaults de fuerza/radio.

#### FIX 4 — [P0] Knockdown visual no aparece (bolas y pufferfish)
- **Causa raíz**: `KnockdownVisualComp` se resolvía al SkeletalMesh vacío de ACharacter o a una pieza pequeña. `TickLegAnimation` sobreescribía la rotación de patas cada frame.
- **Fix**: Nueva `UPROPERTY KnockdownComponentName = "Cuerpo"` en `TortugaCharacter.h`. `BeginPlay` y `ApplyKnockdownVisual` buscan primero por nombre configurable. `TickLegAnimation` ahora hace early-out si `bIsKnockedDown`.
- Archivos modificados:
  - `Source/Tortunabo/Public/Player/TortugaCharacter.h` — `KnockdownComponentName`.
  - `Source/Tortunabo/Private/Player/TortugaCharacter.cpp` — `BeginPlay`, `ApplyKnockdownVisual`, `TickLegAnimation`.

#### FIX 5 — [P1] Muerte tarda ~10-18s en multijugador
- **Causa raíz**: `DBNOBleedoutSeconds=15.f` demasiado largo. `CheckAllAliveDBNO()` solo se llamaba en `EnterDBNO`, no en `TickDBNOBleedout`.
- **Fix**: Reducido `DBNOBleedoutSeconds` de 15→8. `TickDBNOBleedout` ahora llama `CheckAllAliveDBNO()` al final del tick.
- Archivos modificados:
  - `Source/Tortunabo/Public/Game/TN_RunGameMode.h` — default.
  - `Source/Tortunabo/Private/Game/TN_RunGameMode.cpp` — `TickDBNOBleedout`.

#### FIX 6 — [P2] Optimización de red (lag del cliente)
- **Config de red** (`DefaultEngine.ini`): `NetServerMaxTickRate 45→60`, `MaxNetTickRate 45→60`, `MaxInternetClientRate 100000→200000`, `MaxClientRate 100000→200000`, `ConfiguredInternetSpeed 100000→200000`.
- **Character**: `NetUpdateFrequency 30→60 Hz`, `MinNetUpdateFrequency 15→30 Hz`.
- Archivos modificados:
  - `Config/DefaultEngine.ini`.
  - `Source/Tortunabo/Private/Player/TortugaCharacter.cpp` — constructor.

#### FIX 7 — [P2] Helmet con lag visual
- **Causa raíz**: `APlayerState` replica a ~1-2 Hz por defecto. Cambios de `EquippedHelmetId` tardaban en llegar.
- **Fix**: `ATN_CoopPlayerState` ahora usa `SetNetUpdateFrequency(10.f)`. `ServerSetEquippedHelmet` añade `TNPS->ForceNetUpdate()` tras asignar el helmet.
- Archivos modificados:
  - `Source/Tortunabo/Private/Core/TN_CoopPlayerState.cpp` — constructor.
  - `Source/Tortunabo/Private/Player/MP_GamePlayerController.cpp` — `ServerSetEquippedHelmet`.

#### FIX 8 — [P2] Loading screen no persiste durante auto-rejoin
- **Causa raíz**: Widget se destruía junto al PC viejo durante map transition. `bIsLoadingScreenVisible` quedaba true pero widget null.
- **Fix**: `ShowLoadingScreen` ahora valida si el widget sigue vivo (`IsInViewport()`); si no, resetea y recrea. `HandlePostLoadMap` llama `ShowLoadingScreen` cuando `bPendingAutoRejoin` para recrear el widget.
- Archivos modificados:
  - `Source/Tortunabo/Private/Multiplayer/MP_GameInstance.cpp` — `ShowLoadingScreen`, `HandlePostLoadMap`.

### Setup requerido en Editor
1. **`BP_TortugaCharacter → Class Defaults → Knockdown → KnockdownComponentName`**: Verificar que el valor es `"Cuerpo"` o el nombre exacto del SceneComponent principal del cuerpo en el BP. Si el componente se llama diferente, actualizar aquí.
2. **`BP_RunGameMode → Class Defaults → Run|Death → RescuePickupClass`**: Seguir asignado (sin cambios).
3. Sonidos: revisar compresión de audio en assets (Issue de Editor, no código).

### Pendientes
1. Smoke test completo 2+ jugadores: death → pickup → revive en la posición del pickup.
2. Verificar knockdown visual: lanzar bola → confirmar rotación 100° visible en ambos clientes.
3. Verificar pickup: cliente recoge → desaparece inmediatamente del suelo.
4. Probar explosión PufferFish: lanzador también sale volando.
5. Medir latencia del cliente con nuevo tick rate 60Hz.
6. Ruedas radiales de emotes/quick chat (documento `Implementacion_Radial.md` — pendiente de implementación en Editor).

---

## Sesión 2026-03-23 — Helmet/Cosmetics Update Rate + Diagnóstico Travel

### Cambios realizados

#### FIX 9 — [P2] Helmet/cosméticos con lag visual residual
- **Causa raíz**: `ATN_CoopPlayerState` usaba `SetNetUpdateFrequency(10.f)` sin `MinNetUpdateFrequency`. UE podía reducir la frecuencia real por debajo de 10Hz con adaptive net frequency.
- **Fix**: `SetNetUpdateFrequency(30.f)` + `SetMinNetUpdateFrequency(15.f)`. Ahora helmet y estados (alive, DBNO, eliminated, etc.) se replican a 30Hz max,  15Hz mínimo garantizado.
- Archivos modificados:
  - `Source/Tortunabo/Private/Core/TN_CoopPlayerState.cpp` — constructor.

#### FIX 10 — [P1] Clientes expulsados al viajar HQ → Run (timing insuficiente)
- **Causa raíz**: Race condition temporal — el delay de 1s entre destruir el NetDriver y hacer ServerTravel no siempre era suficiente para que Steam libere el socket P2P. Además, el auto-rejoin de clientes empezaba solo 2s después de ConnectionLost, a veces antes de que el host terminara de cargar el mapa y crear el listen server.
- **Fix (timing adjustments)**:
  - `TN_HQGameMode::BeginMatchTravel` y `TN_RunGameMode::FinishRoundAndReturnToLobby`: delay de deferred travel **1.0s → 1.5s** (más margen para liberar socket Steam).
  - `MP_GameInstance::OnNetworkFailure`: delay inicial del primer auto-rejoin **2.0s → 3.0s** (dar más tiempo al host).
  - `MP_GameInstance::HandlePostLoadMap`: delay del deferred auto-rejoin **2.0s → 3.0s**.
  - `MP_GameInstance::AttemptAutoRejoin`: delay entre reintentos **2.0s → 2.5s**.
  - `MaxAutoRejoinRetries` **5 → 8** (total: 3s + 7×2.5s = 20.5s de ventana vs 2s + 4×2s = 10s anterior).
- Archivos modificados:
  - `Source/Tortunabo/Private/Lobby/TN_HQGameMode.cpp` — timer 1.5s.
  - `Source/Tortunabo/Private/Game/TN_RunGameMode.cpp` — timer 1.5s.
  - `Source/Tortunabo/Public/Multiplayer/MP_GameInstance.h` — MaxAutoRejoinRetries 8.
  - `Source/Tortunabo/Private/Multiplayer/MP_GameInstance.cpp` — delays 3.0s/2.5s.

### Diagnóstico: ¿Por qué los clientes se "expulsan"?
- **Es comportamiento esperado del patrón Deferred ServerTravel**: el NetDriver se destruye explícitamente antes del travel para liberar el socket Steam P2P. Los clientes SIEMPRE reciben ConnectionLost y deben reconectar vía sesión Steam.
- El flujo diseñado es: Host destroy NetDriver → wait 1.5s → ServerTravel → nuevo listen server. Clientes: ConnectionLost → loading screen → auto-rejoin vía sesión Steam.
- Los fallos de reconexión ocurrían por timing insuficiente, no por bug lógico.
- Con los nuevos timings, los clientes tienen una ventana de reconexión de ~20.5s.
