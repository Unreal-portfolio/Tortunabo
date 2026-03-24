# AGENTS Guide for Tortunabo

## Snapshot del proyecto (UE 5.6)
- Módulo runtime único: `Tortunabo` (`Tortunabo.uproject`).
- Entry point de módulo: `Source/Tortunabo/Tortunabo.cpp` con `IMPLEMENT_PRIMARY_GAME_MODULE`.
- Targets estándar: `Source/Tortunabo.Target.cs` y `Source/TortunaboEditor.Target.cs`.
- Flujo cooperativo actual en C++: `Menu -> HQ Lobby -> Countdown -> Cinematic -> Run -> Finish/Spectate -> Results -> Return HQ` (ver `Source/Tortunabo/Public/Core/TN_MatchFlowTypes.h` y `Source/Tortunabo/Private/Lobby/TN_HQGameMode.cpp`).
- **Seamless Travel**: `TN_HQGameMode` y `TN_RunGameMode` usan `bUseSeamlessTravel = true`. La conexión de red de los clientes se **mantiene viva** durante `ServerTravel` entre HQ y Run. No hay desconexión, no hay rejoin, no hay race condition de sockets Steam.
  - **PlayerControllers persisten** entre mapas. PlayerStates también persisten y se mueven al nuevo GameState.
  - **Pawns NO persisten**: se destruyen explícitamente ANTES de `ServerTravel` para WASAPI cleanup (`ProximityVoiceComponent::EndPlay(Destroyed)` fires safely). En el mapa destino, `HandleSeamlessTravelPlayer` + `PostSeamlessTravel` limpian estado espectador y respawnean pawns.
  - **PostLogin NO se llama** para jugadores que viajan (solo para conexiones nuevas mid-game). `HandleSeamlessTravelPlayer` y `PostSeamlessTravel` manejan el reset de PlayerState y spawn de pawns.
  - `HandleSeamlessTravelPlayer`: limpia `SetIsOnlyASpectator(false)` ANTES de Super para que `PlayerCanRestart()` permita spawn.
  - `PostSeamlessTravel`: resetea todos los PlayerStates, respawnea pawns, actualiza GameState.
  - **ServerTravel SIN `?listen`**: con seamless travel el NetDriver persiste, no se necesita `?listen` en la URL.
  - **ServerTravel SIN `?game=` explícito**: El GameMode lo determina **WorldSettings → GameMode Override** de cada mapa. **Cada mapa DEBE tener su BP GameMode en WorldSettings**: `LVL_HQ → BP_HQGameMode`, `LVL_Run → BP_RunGameMode`.
  - **Staging en TN_RunGameMode**: Tras el seamless travel, el mapa Run arranca en `WaitingForPlayers`. Con seamless travel los jugadores llegan inmediatamente (no necesitan reconectar), por lo que TryStartMatch generalmente arranca enseguida. Timeout de 15s como safety net.
  - **WASAPI cleanup**: Los GameModes destruyen pawns (`Pawn->Destroy()`) ANTES de `ServerTravel`. `EndPlay(Destroyed)` fires while WASAPI is still alive → cleanup seguro.
  - **PendingTravelPlayerCount**: `MP_GameInstance` persiste entre mapas. `TN_HQGameMode` lo escribe; `TN_RunGameMode` lo lee en `BeginPlay` para saber cuántos jugadores esperar.
  - `OnNetworkFailure` en `MP_GameInstance` mantiene lógica de auto-rejoin como red de seguridad para desconexiones reales (pérdida de Internet, crash del host).
  - `NetChecksumMismatch` es detectado con mensaje claro al usuario. La causa principal es builds incompatibles (Live Coding, Hot Reload). **Ambas máquinas deben usar el mismo DLL compilado** — compilar con `-NoHotReload` y nunca usar Live Coding durante tests multiplayer.

## Arquitectura por dominios (Source/Tortunabo)
- `Public/Core`, `Private/Core`: estado replicado, tipos de flujo e inventario (`TN_CoopGameState`, `TN_CoopPlayerState`, `TN_MatchFlowTypes`, `TN_InventoryTypes`).
- `Public/Lobby`, `Private/Lobby`: cuartel general y zona ready (`TN_HQGameMode`, `TN_LobbyReadyZone`).
- `Public/Game`, `Private/Game`: reglas de la carrera (`TN_RunGameMode`).
- `Public/World`, `Private/World`: volúmenes de mundo e interactuables (`TN_FinishLineVolume`, `TN_DeathZoneVolume`, `TN_InteractableBase`, `TN_DirectInteractableBase`, `TN_PickupInteractableBase`, `TN_StaminaBoostPickup`, `TN_ThrowableItemActor`, `TN_CosmeticsStationInteractable`, `TN_ButtonInteractable`, `TN_ItemSpawnZone`, `TN_PufferFishActor`, `TN_RescuePickup`).
  - **`TN_ButtonInteractable`** (hereda `TN_DirectInteractableBase`): mueve un actor `MoveTarget` a través de waypoints al interactuar. `CurrentWaypointIndex` replicado; `MoveTarget` debe tener `SetReplicateMovement(true)`. Multicast unreliable para feedback cosmético (sonido/VFX en BP).
  - **`TN_ItemSpawnZone`**: actor (BeginPlay server-only) que spawnea ítems aleatorios dentro de un `UBoxComponent`. Configurar `ItemDataTable`, `ItemRowNames`, `SpawnCount` y `MinSpacing` por instancia en el nivel.
  - **`TN_PufferFishActor`** (hereda `TN_ThrowableItemActor`): throwable especial que tras un delay aleatorio (`InflateDelayMin`/`Max`) se infla (`InflateScale`, `InflateRadius=800`, `InflatePushForce=4000`) empujando y potencialmente knockeando a **TODOS** los jugadores cercanos **incluido el lanzador**, luego se desinfla. `PufferState` replicado con `OnRep_PufferState` para visual en clientes. **NO knockea por impacto directo** (OnMeshHit del padre está deshabilitado en BeginPlay); el knockdown solo ocurre durante la explosión de inflación. Componente vertical del empuje exagerado (`Z ≥ 0.7`).
  - **Bomb explosion window**: `BombExplosionTick` tickea a ~60fps durante `BombWindowSeconds` (0.15s) usando `TActorIterator<ACharacter>` para búsqueda directa por distancia (no OverlapMulti). `BombHitCharacters` (TSet) evita doble empuje. Colisión del mesh se desactiva antes de escalar para evitar depenetración errática del motor de físicas.
  - **Knockdown + Launch**: Knockdown se aplica PRIMERO (bloquea input + visual), luego `LaunchCharacter` setea `PendingLaunchVelocity` que en el siguiente tick fuerza `MOVE_Falling` vía `HandlePendingLaunch` → personaje vuela. `ATortugaCharacter::Tick` re-desactiva movimiento cuando el personaje knocked aterriza (`IsMovingOnGround()`).
- `Public/Player`, `Private/Player`: personaje/control de jugador y componentes (`TortugaCharacter`, `TortugaFirstPersonCharacter`, `MP_GamePlayerController`, `TN_StaminaComponent`, `TN_InventoryComponent`).
- `Public/Multiplayer`, `Private/Multiplayer`: sesiones Steam, lifecycle de sesión y cosméticos persistentes (`MP_GameInstance`, `TN_CosmeticSaveGame`).
- `Public/Menu`, `Private/Menu` y `Public/UI/*`, `Private/UI/*`: menú principal, HUDs, loading e interacción (`MP_MainMenuWidget`, `MP_MenuGameMode`, `MP_MenuPlayerController`, `TN_CoopFlowHUDWidget`, `TN_LoadingScreenWidget`, `TN_InteractPromptWidget`, `VoiceIndicatorWidget`, `TN_PlayerHUDWidget`).
- `Public/Voice`, `Private/Voice`: VOIP por proximidad (`ProximityVoiceComponent`).

### Documentación auxiliar (Docs/)
- `Docs/GUIA_PANTALLA_RESULTADOS.md`: guía visual para montar la pantalla de resultados en Blueprint (nombres de widgets, jerarquía requerida).
- `Docs/AgentSync/`: protocolo de colaboración entre agents/desarrolladores. Incluye `MEMORY_DUMP_TEMPLATE.md` (plantilla de memory dump técnico) y `SESSION_LOG.md` (historial de sesiones con cambios, networking, pendientes). Registrar cada cambio relevante aquí.
- `GUIA_MONTAJE_INICIAL.md` (raíz): checklist completo para dejar el proyecto funcional de punta a punta en el Editor.

## Configuración crítica (fuente de verdad)
- `Config/DefaultEngine.ini`:
  - `GameInstanceClass=/Script/Tortunabo.MP_GameInstance`
  - `GlobalDefaultGameMode` apunta a BP: `/Game/Blueprints/Gameplay/GameModes/BP_MenuGameMode.BP_MenuGameMode_C` (debe existir como BP hijo de `AMP_MenuGameMode`).
  - `GameDefaultMap=/Game/Maps/Lobby/LVL_Menu`
  - NetDriver SteamSockets y `OnlineSubsystemSteam` con `SteamDevAppId=480`.
  - `[Voice] bEnabled=false`: desactiva el VOIP nativo de UE para que no duplique audio con `ProximityVoiceComponent`.
- `Config/DefaultInput.ini`: backend Enhanced Input (`EnhancedPlayerInput`, `EnhancedInputComponent`).
- `Config/DefaultGame.ini`: cook de `/Game/Maps`, `/Game/UI`, `/Game/Input`. Contiene `SteamDevAppId=480` bajo `[/Script/Tortunabo.MP_GameInstance]` (UPROPERTY Config).

## Input System
- `TortugaCharacter` usa Enhanced Input con soft references a assets en `/Game/Blueprints/Gameplay/Controls/`:
  - `IMC_Player` (Input Mapping Context)
  - `IA_Move`, `IA_Look`, `IA_Jump`, `IA_Interact`, `IA_RotateInventory`, `IA_Sprint`, `IA_DropItem`
  - `IA_Emote` (slot 0), `IA_Emote1`..`IA_Emote9` (slots 1-9)
- Estos assets deben crearse en el Editor en `/Game/Blueprints/Gameplay/Controls/`. Si faltan, el personaje loggea warning y no recibe input.
- El mapping se aplica en `BeginPlay` y `PawnClientRestart` via `UEnhancedInputLocalPlayerSubsystem`.

## Lobby countdown — lógica dinámica
- El countdown arranca cuando **todos los jugadores conectados** están en la ready zone (no cuando se alcanza `LobbyExpectedPlayers`).
- `LobbyExpectedPlayers` define el máximo de sala (para display "Sala: X/4"), pero el countdown compara contra `ConnectedPlayers`.
- Si alguien sale de la zona durante countdown, se resetea.
- `TickCountdown` recuenta connected/ready cada tick para detectar salidas.

## Cosmetics system
- `MP_GameInstance` gestiona un perfil de cosméticos (`TN_CosmeticSaveGame`) con helms desbloqueados y equipados.
- Crate table con pesos configurable en `HelmetCrateTable`.
- `MP_GamePlayerController` sincroniza helms al servidor via `ServerSyncUnlockedHelmets`/`ServerSetEquippedHelmet`.
- `TN_CosmeticsStationInteractable` en el lobby abre el menú de cosméticos via Client RPC.
- **Race condition OnRep vs BeginPlay**: `OnRep_EquippedHelmetId` en PlayerState llama `GetPawn()` que puede ser null si el PlayerState llega antes de que se posea el pawn. Fix: `ATortugaCharacter::OnRep_PlayerState()` (override) re-aplica el casco al recibir el PlayerState tardío, cubriendo el caso donde el timer `SetTimerForNextTick` de BeginPlay ya expiró sin datos válidos.

## Stamina system (`TN_StaminaComponent`)
- `MaxStamina = 200.0f` (configurable desde Blueprint via `BlueprintReadWrite`).
- `SprintDrainPerSecond = 45.0f`, `RechargeDelaySeconds = 0.8f`.
- **Penalización por agotamiento**: si `CurrentStamina` llega a 0 mientras sprint, `bIsExhausted = true` y la recuperación se bloquea `ExhaustionPenaltySeconds = 1.0f` segundos ANTES de que empiece el `RechargeDelaySeconds` normal.
- `IsExhausted()` (BlueprintPure) disponible para UI y lógica BP.
- **NO hay rotación del mesh al sprintar** (`ApplySprintVisual` es no-op). El feedback visual de sprint viene solo del aumento de amplitud de piernas (60°→90°).
- **Sprint en cualquier dirección**: `RefreshSprintRequest` activa sprint si `LastMovementInput.SizeSquared() > 0.0625` — no requiere input hacia adelante.
- Replicación: `CurrentStamina` (owner-only), `bIsSprinting` (all), `bSprintRequested` (all), `bUnlimitedStamina` (owner-only), `bIsExhausted` (owner-only).

## Weight system (sistema de peso tipo Peak)
- `FTN_InventoryItem::ItemWeight` (float, default 0) — asignable en el DataTable o BP del ítem.
- `UTN_InventoryComponent::GetTotalCarriedWeight()` — suma `ItemWeight` de equipado + guardado.
- `UTN_StaminaComponent::StaminaPerWeightUnit = 20.f` — stamina perdida por unidad de peso (configurable).
- `GetEffectiveMaxStamina()` = `MaxStamina - (TotalWeight * StaminaPerWeightUnit)`, mínimo 1.
- `GetWeightPenalty()` = `MaxStamina - GetEffectiveMaxStamina()` (para la UI).
- La stamina se clampea instantáneamente al `EffectiveMaxStamina` cuando el jugador coge un objeto pesado.
- La recarga nunca supera `EffectiveMaxStamina`.
- Vínculo inventory↔stamina: `StaminaComponent->SetInventoryComponent(InventoryComponent)` en `ATortugaCharacter::BeginPlay()`.

## Player HUD (`TN_PlayerHUDWidget`)
- Widget C++ base para toda la UI en pantalla del jugador durante gameplay.
- Se crea en `MP_GamePlayerController::CreatePlayerHUD()` al hacer posesión; asignar `PlayerHUDWidgetClass` en `BP_GamePlayerController → Class Defaults`.
- Pollea `UTN_StaminaComponent` del pawn local cada ~50ms (throttle configurable).
- Widgets opcionales (nombres exactos en BP Designer):
  - `StaminaBar` (UProgressBar) → `Current / MaxStamina`
  - `WeightPenaltyBar` (UProgressBar) → `WeightPenalty / MaxStamina`. Superponer sobre `StaminaBar` con **Fill Direction = Right to Left** y color distinto (ej. marrón oscuro). Representa la zona de stamina bloqueada por peso.
  - `ExhaustedRoot` (cualquier widget) → visible solo durante penalización de agotamiento
  - `StaminaText` (UTextBlock) → "120 / 150" (actual vs effective max)
- Hooks BP: `OnStaminaUpdated(CurrentStamina, MaxStamina, bExhausted)` y `OnWeightUpdated(WeightPenalty, MaxStamina, EffectiveMaxStamina)`.
- Z-order en viewport: 4 (por debajo del CoopFlowHUD en 5 y VoiceIndicator en 10).

## Leg Animation (blockout en `TortugaCharacter`)
- Animación de patas inline en `Tick` → `TickLegAnimation()`. No usa `TN_LegAnimComponent` (ese es opcional para otros actores BP).
- Añadir en el Blueprint dos SceneComponents hijos con nombres exactos `Pata1` y `Pata2`. Origen del componente = pivote en la cadera (extremo superior del cubo).
- `LegWalkAmplitudeDeg = 60°` (ambas patas simétricas), `LegSprintAmplitudeDeg = 90°`.
- Las dos patas van en fase opuesta (+Angle / -Angle): efecto trote.
- `LegSwingAxis = (0,1,0)` = eje Y local (adelante/atrás). Cambiar si los ejes locales del componente difieren.
- **No hay rotación del mesh completo al sprintar** — ese comportamiento fue eliminado.

## Emote Audio
- Cada emote (0–9) puede tener un `USoundBase` asociado: array `EmoteSounds` en `TortugaCharacter`, asignable en BP Class Defaults.
- El audio se reproduce en loop mientras el emote está activo; se detiene al cancelar/terminar.
- Usa atenuación de proximidad idéntica al chat de voz: `EmoteAudioInnerRadius = 300cm` (3m), `EmoteAudioOuterRadius = 2500cm` (25m), `FalloffMode = NaturalSound`.
- `EmoteAudioComponent` se crea lazily en el primer uso, adjunto al root del personaje.
- Loop forzado via `OnAudioFinished` delegate → funciona con cualquier `USoundWave`/`SoundCue` sin necesidad de marcar el asset como looping.

## Camera y colisión entre personajes
- `GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore)` en el constructor de `ATortugaCharacter`. El spring arm del jugador local no choca con las cápsulas de otros jugadores.
- El spring arm ignora su propio pawn automáticamente (UE lo hace internamente). La línea anterior solo afecta a personajes remotos.

## Camera cinematográfica AAA (`TortugaCharacter`)
- Sistema de cámara estilo over-the-shoulder con lag de posición y rotación, zoom dinámico al sprint, FOV interpolado y eje Y invertible.
- **Spring Arm**: `bEnableCameraLag = true` + `bEnableCameraRotationLag = true`. Los lag speeds se sincronizan cada frame desde las UPROPERTYs para poder ajustarlos en runtime.
- **Zoom al sprint**: `TickCameraInterp(DeltaTime)` (solo en cliente local) interpola `CameraBoom->TargetArmLength` y `FollowCamera->FieldOfView` según `StaminaComponent->IsSprinting()`.
- **Defaults de los settings**:
  - `CameraArmLengthDefault = 350`, `CameraArmLengthSprint = 480`, `CameraArmLengthInterpSpeed = 5`
  - `CameraFOVDefault = 80`, `CameraFOVSprint = 90`, `CameraFOVInterpSpeed = 5`
  - `CameraPositionLagSpeed = 8`, `CameraRotationLagSpeed = 14`
  - `CameraSocketOffset = (0, 55, 65)` → over-the-shoulder derecho + elevada
  - `CameraBoomRelativeOffset = (0, 0, 40)` → pivot del boom 40 cm sobre la raíz
- **Eje Y invertido**: `bInvertCameraY = false` por defecto. Si el jugador quiere invertir pitch, marcar a `true` en BP_TortugaCharacter → Class Defaults.
- **Sensibilidad**: `LookSensitivityX = 1.0` (horizontal) y `LookSensitivityY = 0.5` (vertical, más lenta para evitar mareo). Ambas `EditDefaultsOnly + BlueprintReadWrite`.
- Todos los settings son `EditDefaultsOnly + BlueprintReadWrite` → ajustables en BP sin recompilar.
- `BeginPlay` aplica los valores de las UPROPERTYs al spring arm y cámara para respetar overrides hechos en el BP hijo.

## Knockdown replication
- `bIsKnockedDown` (ReplicatedUsing = OnRep_IsKnockedDown) se replica a todos los clientes.
- **`KnockdownComponentName`** (`EditDefaultsOnly`, default `"Cuerpo"`): nombre del SceneComponent en el BP que se rota durante el knockdown. `BeginPlay` y `ApplyKnockdownVisual` buscan primero por este nombre configurable; si no existe, fallback a primer StaticMesh/SkeletalMesh con asset. **Verificar en BP_TortugaCharacter que coincide con el componente principal del cuerpo.**
- `ApplyKnockdownVisual` usa `SetRelativeRotation` (NO `SetWorldRotation`) para que `CharacterMovementComponent` no sobreescriba la rotación en clientes remotos. Tilt de 180° (tortuga completamente patas arriba).
- **Fix NetworkSmoothing**: `ApplyKnockdownVisual(true)` pone `CMC->NetworkSmoothingMode = Disabled`; `ApplyKnockdownVisual(false)` lo restaura a `Exponential`. Sin esto, el smoothing exponencial del CMC sobreescribe la rotación del mesh cada frame en clientes remotos, haciendo invisible el tilt de knockdown.
- **`TickLegAnimation` suprimida durante knockdown**: si `bIsKnockedDown == true`, las patas no animan. Esto evita que la animación de piernas sobreescriba la rotación del componente de knockdown cada frame.
- El listen-server aplica visual y bloqueo de movimiento directamente en `ApplyKnockdown()`/`RecoverFromKnockdown()` porque `OnRep` no dispara localmente.
- Los clientes bloquean movimiento en `OnRep_IsKnockedDown` para no enviar inputs durante knockdown.
- `MulticastApplyKnockdownVisual` (NetMulticast, Reliable) garantiza que todos los clientes reciban el knockdown inmediatamente, sin depender solo de OnRep (que puede batching).


- `TN_CoopGameState` centraliza todo el estado replicado de partida: `MatchFlowState`, `ReadyPlayers`, `ConnectedPlayers`, `PlayersInStartZone`, `ExpectedPlayers`, `CountdownValue`, `ServerMatchElapsedTime`, `FinishedPlayers`.
- **Patrón crítico**: `OnRep_*` no dispara en la máquina que posee la variable (listen-server). Los game modes llaman `BroadcastFlowStateChange()` tras cambiar `MatchFlowState` para que el listen-server local también reciba la notificación via `OnMatchFlowStateChanged` delegate.
- `TN_CoopPlayerState` replica por jugador: `bIsInReadyZone`, `bHasFinishedRun`, `bIsAlive`, `DeathZoneTimeRemaining`, `EquippedHelmetId`, `FinishTimeSeconds`, `FinishRank`, **`bIsEliminated`** (true cuando el jugador murió en vez de llegar a la meta — reset a false en `PostLogin` de cada mapa).

## Results y espectador
- Al terminar la carrera o morir, `TN_RunGameMode` llama `MarkPlayerFinished()`/`MarkPlayerDead()` → asigna rank/tiempo en `TN_CoopPlayerState` → mueve al jugador a espectador via `MovePlayerToSpectator()`.
- **`MarkPlayerFinished`**: oculta el pawn (`SetActorHiddenInGame + disable collision`) y mueve a espectador. El personaje "desaparece" al terminar.
- **`MarkPlayerDead`**: oculta el pawn completamente (`SetActorHiddenInGame + disable collision`), spawnea un `TN_RescuePickup` en la posición de muerte, y mueve al jugador a espectador. **NO deja cadáver interactuable**.
- **`bIsEliminated`** (Replicated, `TN_CoopPlayerState`): `true` si el jugador murió en vez de terminar. `MarkPlayerDead` asigna `FinishRank = NextFinishRank++` Y `bIsEliminated = true`. La UI lee `bIsEliminated` para mostrar "ELIMINADO" y `FinishRank` para el orden en resultados. **No usar `FinishRank == 0` para detectar eliminados** — todos tienen rank ≥ 1.
- `MP_GamePlayerController` ofrece `EnterSpectateMode()`, `SpectateNextPlayer()`, `SpectatePreviousPlayer()` para navegar entre jugadores.
- **Candidatos de espectador**: `SpectateByDirection` incluye solo jugadores con pawn activo que están **vivos y no eliminados** (`bIsAlive && !bIsEliminated`). Los cadáveres y pawns ocultos de finishers no se espectean.
- `TN_CoopFlowHUDWidget` detecta `ETNMatchFlowState::Results` y muestra automáticamente el panel de resultados con rank, tiempo y countdown de vuelta a lobby. Ver `Docs/GUIA_PANTALLA_RESULTADOS.md` para la jerarquía de widgets requerida.
- Countdown de resultados configurable en `TN_RunGameMode::ResultsDurationSeconds` (default 8s).

## Death y Rescue Pickup system
- **`TN_RescuePickup`** (hereda `TN_InteractableBase`): actor de rescate que se spawnea cuando un jugador muere.
  - `DeadPlayerId` (Replicated): PlayerId del jugador muerto asociado.
  - Al interactuar: busca el PC del muerto, teletransporta su pawn a la posición del pickup, llama `TN_RunGameMode::RevivePlayer()`, se destruye.
  - Prompt: "Rescatar". Distancia de interacción: 300cm.
  - El jugador muerto no puede rescatarse a sí mismo (`CanInteract` lo impide).
- **`RescuePickupClass`** (`TSubclassOf<ATN_RescuePickup>`, EditDefaultsOnly en `TN_RunGameMode`): clase del pickup a spawnear. Asignar en `BP_RunGameMode → Class Defaults`. **Si no se asigna, los jugadores muertos no tendrán pickup y no podrán ser rescatados.**
- **`RescuePickups`** (`TMap<int32, TWeakObjectPtr<ATN_RescuePickup>>`): tracking map en `TN_RunGameMode` para limpiar pickups al revivir.
- **`DeadPlayerPawns`** (`TMap<int32, TWeakObjectPtr<APawn>>`): referencia al pawn guardada en `MarkPlayerDead` **ANTES** de `MovePlayerToSpectator`. Necesario porque `EnterSpectateMode()` → `ChangeState(Spectating)` → UE llama `UnPossess()` internamente → `PC->GetPawn()` devuelve nullptr. `RevivePlayer` y `TN_RescuePickup::Interact` usan `GetDeadPlayerPawn(PlayerId)` para recuperar el pawn.
- **Flujo completo**: muerte → guardar ref pawn → ocultar pawn → spawnear pickup → espectador → compañero interactúa con pickup → teletransportar pawn a pos del pickup → `RevivePlayer` re-posee pawn → pickup se destruye.
- **Revive vía Interact (DBNO)**: `TryInteract()` busca jugadores `bIsKnockedDown` (DBNO) dentro del `ReviveRadiusCm`. Si lo hay, envía `ServerTryReviveNearby()`. Los jugadores muertos (no DBNO) se rescatan vía el pickup.
- **`RevivePlayer` actualizado**: maneja tanto DBNO como jugadores muertos. Busca pawn en `DeadPlayerPawns` si `GetPawn()` es null. Restaura visibilidad del pawn (`SetActorHiddenInGame(false) + SetActorEnableCollision(true)`), `bIsAlive`, `bHasFinishedRun=false`, `bIsEliminated=false`. Llama `SetDeadVisual(false)` + `RecoverFromKnockdown()`. Re-posee el pawn y restaura input/movement. Otorga inmunidad temporal. Destruye el pickup de rescate. Limpia `DeadPlayerPawns`.

## Quick Chat (Rocket League style)
- `FTN_QuickChatEntry { SenderName, MessageID, Timestamp }` — struct en `TN_MatchFlowTypes.h`.
- `ATN_CoopGameState::QuickChatHistory` — `TArray<FTN_QuickChatEntry>` (max 10, Replicated + OnRep). `AddQuickChatEntry()` (server-only) añade un entry y hace broadcast local; `OnRep_QuickChatHistory` hace broadcast en clientes. Delegate: `OnQuickChatReceived(Entry)` (BlueprintAssignable).
- `AMP_GamePlayerController::SendQuickChat(int32 MessageID)` — callable desde Blueprint. Llama `ServerSendQuickChat` (Reliable RPC). Rate limit: 1 mensaje / `QuickChatCooldownSeconds` (default 2s) por PC en servidor. Valida `MessageID` contra `QuickChatMessages.Num()`.
- `QuickChatMessages` — `TArray<FString>` con 8 mensajes predefinidos, `EditDefaultsOnly + BlueprintReadWrite`, customizable en `BP_GamePlayerController → Class Defaults`. Default: "¡Vamos!", "Buen juego", "Lo siento", "Gracias", "¡Necesito ayuda!", "¡Cuidado!", "Nos vemos en la meta", "¡Eso estuvo bien!".
- **UI**: Bind `ATN_CoopGameState::OnQuickChatReceived` en `TN_CoopFlowHUDWidget` o en el BP del HUD. Cada entry contiene `SenderName` + índice `MessageID` (la UI localiza el string desde el array del PC local).

## Colisión de componentes del personaje
- La `Cola` (SceneComponent hijo en `BP_TortugaCharacter`) puede empujar a otros jugadores si sus meshes hijos tienen colisión con el canal Pawn. Fix aplicado en `ATortugaCharacter::BeginPlay`: tras encontrar el componente Cola por nombre, itera sus hijos con `GetChildrenComponents(true)` y setea `SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore)` en cada `UPrimitiveComponent`.

## Death zones
- `TN_DeathZoneVolume` hereda `ATriggerVolume`. No mata instantáneamente: usa un countdown por jugador (`SecondsInsideToDie`, default 3s) con ticks cada `CountdownTickInterval` (default 0.1s).
- Si el jugador sale del volumen antes de que expire el timer, se resetea.
- El tiempo restante se sincroniza a clients via `TN_CoopPlayerState::DeathZoneTimeRemaining`.
- `bDestroyOnlyDuringRun` (default true) permite que la zona solo mate durante la carrera, ignorando el lobby.
- Cuando el countdown expira, llama `TN_RunGameMode::MarkPlayerDead()` directamente — **muerte instantánea, sin DBNO/bleedout**. El pawn se oculta, se spawnea un `TN_RescuePickup` y el jugador pasa a espectador.

## DBNO system (Down But Not Out) — NO ACTIVO
- El sistema DBNO existe en el código (`EnterDBNO`, `TickDBNOBleedout`, `CheckAllAliveDBNO`) pero **NO se usa actualmente**. Las death zones llaman `MarkPlayerDead()` directamente → muerte instantánea.
- **El revive de jugadores muertos es SOLO vía `TN_RescuePickup`**: al morir se spawnea un pickup en el suelo, un compañero interactúa con él, y el jugador muerto respawnea en esa posición.
- El código DBNO se conserva por si en el futuro se quiere un estado intermedio de incapacitación antes de la muerte real. Para activarlo, cambiar `TN_DeathZoneVolume::HandlePlayerDeath` de `MarkPlayerDead` a `EnterDBNO`.
- Las propiedades `bIsDBNO` y `DBNOBleedoutTimeRemaining` en `TN_CoopPlayerState` siguen replicadas pero siempre valen `false` / `-1` en el flujo actual.

## Loading screen
- `MP_GameInstance` gestiona un loading screen global: `ShowLoadingScreen(Reason)`/`HideLoadingScreen()`.
- Se engancha a `FCoreUObjectDelegates::PreLoadMap`/`PostLoadMapWithWorld` para mostrar/ocultar automáticamente en ServerTravel.
- Widget class configurable via `LoadingScreenWidgetClass` (TSubclassOf). Widget base: `TN_LoadingScreenWidget` con `BindWidgetOptional` para `RootOverlay` y `StatusText`. El fallback C++ incluye fondo negro y texto centrado de tamaño 24.
- Los GameModes **no** llaman `ShowLoadingScreen` explícitamente antes de `ServerTravel`. El callback `HandlePreLoadMap` es el encargado principal de mostrar la loading screen para cualquier transición de mapa.
- `HandlePostLoadMap` oculta la loading screen cuando el mapa nuevo termina de cargar.
- `ShowLoadingScreen` usa `GetFirstLocalPlayerController()` como outer para `CreateWidget`. Si no hay PC, no se muestra (falla silenciosamente).
- Z-order: 100000 (por encima de todo otro widget).

## Interacción — jerarquía y extensión
- `TN_InteractableBase` (Abstract): base con mesh, prompt 3D (`TN_InteractPromptWidget` via `UWidgetComponent`), distancia configurable y `bInteractionEnabled` replicado. Hook de extensión: `OnInteracted` (`BlueprintNativeEvent`).
- `TN_DirectInteractableBase`: interacción directa con cooldown. Hook: `OnDirectInteraction` (`BlueprintImplementableEvent`).
- `TN_PickupInteractableBase`: recoge item al inventario, replica `bTaken`. Hook: `OnPickedUp` (`BlueprintImplementableEvent`). Soporta inicialización runtime via `InitializeFromInventoryItem()`.
- `TN_ThrowableItemActor`: proyectil con `UProjectileMovementComponent`. Usa `SetReplicateMovement(true)`: el servidor ejecuta la física y los clientes reciben posición replicada. Datos de lanzamiento agrupados en `FTN_ThrowLaunchData` (struct replicado). **`FTN_ThrowLaunchData` incluye `EquippedMesh` (TObjectPtr<UStaticMesh>)** para que los jugadores que se unan tarde (JIP) vean el mesh correcto en vez del placeholder gigante. Al golpear un jugador, aplica knockdown pero la bola **rebota** (no se destruye); `AlreadyHitPlayers` previene knockdowns duplicados en el mismo lanzamiento. Pickup se genera al detenerse (`OnProjectileStopped`) o expirar (`LifeSpanExpired`). Al detenerse, la bola pasa a `DORM_DormantAll` para no consumir bandwidth de red.

## VOIP — proximidad y seguridad en shutdown
- `ProximityVoiceComponent` captura audio con `FAudioCaptureSynth` (WASAPI en Windows).
- **Atenuación por distancia**: `PlaybackAudioComponent` usa `bOverrideAttenuation` con `InnerRadius` (300cm = 3m, volumen pleno) y `OuterRadius` (2500cm = 25m, silencio). `FalloffMode = NaturalSound`. `bAlwaysPlay = false` permite que UE descarte voces lejanas.
- **VOIP nativo desactivado**: `[Voice] bEnabled=false` en `DefaultEngine.ini`. Sin esto, UE captura/transmite audio en paralelo y se escucha doble en clientes.
- **Bug UE 5.6**: los handles WASAPI internos del synth pueden invalidarse (`INVALID_HANDLE_VALUE`) durante el ciclo de vida del mundo. Tanto `StopCapturing()` como el destructor intentan usar esos handles y causan `ACCESS_VIOLATION`. **Nunca** llamar a ninguno de los dos.
- **Patrón de shutdown validado (despawn-before-travel)**: antes de `ServerTravel`/`ClientTravel`, los GameModes **destruyen todos los Pawns** (`Pawn->Destroy()` para cada PC conectado). `EndPlay(Destroyed)` dispara `CleanupRuntimeResources(false)` **mientras WASAPI está todavía activo** → cleanup seguro sin crash. Esto reemplaza el antiguo patrón de `ShutdownAllCapture` directo.
  - `HandlePreLoadMap` en `MP_GameInstance` sigue llamando `ShutdownAllCapture` como red de seguridad, pero con los Pawns ya destruidos, todos los componentes tienen `bRuntimeResourcesCleanedUp=true` → la llamada es un no-op seguro.
  - **NO llamar `ShutdownAllCapture` directamente desde GameModes** — causar que fire con WASAPI en estado incierto produce `ACCESS_VIOLATION writing 0x24` o `reading 0xffffffffffffffff`.
- `CleanupRuntimeResources` **siempre** usa `Release()` para el capture synth (no destructor). El leak es ~KB por sesión y el SO lo recupera al cerrar el proceso.
- Safety net en `MP_GameInstance::HandlePreLoadMap` captura cualquier transición no cubierta explícitamente.
- `EndPlay` con `bRuntimeResourcesCleanedUp == true` es un no-op seguro.

## Workflows prácticos
- Compilar editor (ruta del README):
  - `Build.bat TortunaboEditor Win64 Development <Tortunabo.uproject> -WaitMutex -NoHotReload`
- Si cambias clases/módulos C++, regenera project files desde `Tortunabo.uproject` antes de usar `Tortunabo.sln`.
- El paso `Countdown -> Cinematic -> Travel` depende de `CinematicDelaySeconds` en `TN_HQGameMode`; valida ese delay en smoke tests de flujo.
- Validación funcional principal: probar en Editor/Standalone (PIE no siempre refleja Steam real).
- No hay suite de tests automatizados en repo; validar con smoke tests de flujo completo.
- Steam AppID se genera automáticamente en `Binaries/Win64/steam_appid.txt` via `MP_GameInstance::EnsureSteamAppIdFile()` (solo en non-shipping builds).

## Convenciones para cambios
- Mantener clases nuevas dentro de su dominio (`Core`, `Lobby`, `Game`, `Player`, `UI`, `Voice`, `Multiplayer`, `World`).
- En `.cpp`, incluir headers con ruta de módulo (`#include "Voice/ProximityVoiceComponent.h"`, `#include "UI/Menu/MP_MainMenuWidget.h"`).
- Evitar lógica persistente en carpetas generadas; son regenerables: `Binaries/`, `DerivedDataCache/`, `Intermediate/`, `Saved/`.
- Integraciones activas en `Source/Tortunabo/Tortunabo.Build.cs`:
  - **Public**: `Core`, `CoreUObject`, `Engine`, `InputCore`, `EnhancedInput`, `OnlineSubsystem`, `OnlineSubsystemUtils`, `UMG`, `Slate`, `SlateCore`, `AudioCapture`, `AudioCaptureCore`, `AudioMixer`, `SignalProcessing`.
  - **Dynamic**: `OnlineSubsystemSteam`.
  - Si tocas sesión/voz, verifica que sigan declaradas.
- Si agregas dependencias nuevas (UI avanzada, networking extra, online), actualiza primero `Source/Tortunabo/Tortunabo.Build.cs`.
- Widgets UMG C++ usan `BindWidget` (obligatorio) y `BindWidgetOptional` (tolerante) en meta. Los nombres del widget en el BP Designer **deben coincidir exactamente** con el nombre de la UPROPERTY en C++. Ver `TN_CoopFlowHUDWidget.h` y `Docs/GUIA_PANTALLA_RESULTADOS.md` como ejemplo.
- `TortugaCharacter` es tercera persona (spring arm 350 + follow camera, over-the-shoulder). `TortugaFirstPersonCharacter` hereda de él con arm length 0 (solo para cinemáticas).

## Estructura de Content recomendada
- Base creada para escalar: `Content/Maps/{Lobby,Run}`, `Content/UI/{Menu,HUD,Results}`, `Content/Input`, `Content/Blueprints/{Characters,Gameplay}`, `Content/Characters/Turtles`, `Content/Audio/Voice`.
- Mantener assets del flujo coop en esas carpetas para evitar acoplamiento y facilitar cook/package.
