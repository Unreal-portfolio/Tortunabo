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
