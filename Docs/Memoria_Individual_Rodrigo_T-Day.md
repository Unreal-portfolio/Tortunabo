<style>
body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; max-width: 950px; margin: 0 auto; padding: 40px 20px; color: #2c3e50; line-height: 1.7; }
h1 { color: #1a1a2e; border-bottom: 3px solid #8e44ad; padding-bottom: 12px; font-size: 2em; }
h2 { color: #16213e; border-left: 4px solid #8e44ad; padding-left: 12px; margin-top: 40px; }
h3 { color: #0f3460; margin-top: 24px; }
h4 { color: #6c3483; }
table { width: 100%; border-collapse: collapse; margin: 20px 0; }
th, td { border: 1px solid #ddd; padding: 10px 14px; text-align: left; }
th { background-color: #1a1a2e; color: white; }
tr:nth-child(even) { background-color: #f8f9fa; }
code { background: #f0f0f0; padding: 2px 6px; border-radius: 3px; font-size: 0.9em; }
pre { background: #1e1e2e; color: #cdd6f4; padding: 16px; border-radius: 8px; overflow-x: auto; font-size: 0.85em; line-height: 1.5; }
pre code { background: none; color: inherit; padding: 0; }
.highlight { background: #f0e6f6; padding: 12px 16px; border-left: 4px solid #8e44ad; margin: 16px 0; border-radius: 4px; }
.info { background: #d1ecf1; padding: 12px 16px; border-left: 4px solid #17a2b8; margin: 16px 0; border-radius: 4px; }
.warning { background: #fff3cd; padding: 12px 16px; border-left: 4px solid #ffc107; margin: 16px 0; border-radius: 4px; }
.section-box { background: #f8f9fa; border: 1px solid #dee2e6; border-radius: 8px; padding: 16px 20px; margin: 16px 0; }
.component-tag { display: inline-block; background: #8e44ad; color: white; padding: 2px 8px; border-radius: 4px; font-size: 0.8em; margin-right: 4px; }
.file-tag { display: inline-block; background: #0f3460; color: white; padding: 2px 8px; border-radius: 4px; font-size: 0.8em; }
</style>

# Tortunabo — Memoria Individual

**Autor:** Rodrigo Fernández Carnicer
**Motor:** Unreal Engine 5.6 | **Lenguaje:** C++ | **Red:** Steam Sockets (OnlineSubsystemSteam)
**Fecha:** Marzo 2026

---

## 1. Resumen de Responsabilidades

Mi parte del proyecto abarca toda la **infraestructura de red multijugador**, el **flujo completo de partida** (menú → lobby → carrera → resultados → lobby), los **game modes** de cada mapa, el **estado replicado central**, la **death/rescue loop**, los **actores de mundo interactivos** y el **backend del sistema de chat rápido**.

### Archivos desarrollados

| Carpeta | Archivos |
|---|---|
| `Multiplayer/` | `MP_GameInstance.h/.cpp`, `TN_CosmeticSaveGame.h/.cpp` (integración de guardado) |
| `Core/` | `TN_CoopGameState.h/.cpp`, `TN_CoopPlayerState.h/.cpp`, `TN_MatchFlowTypes.h`, `TN_CosmeticsTypes.h`, `TN_InventoryTypes.h` |
| `Game/` | `TN_RunGameMode.h/.cpp` |
| `Lobby/` | `TN_HQGameMode.h/.cpp`, `TN_LobbyReadyZone.h/.cpp` |
| `Menu/` | `MP_MenuGameMode.h/.cpp`, `MP_MenuPlayerController.h/.cpp` |
| `Player/` | `MP_GamePlayerController.h/.cpp` (controlador durante la carrera) |
| `World/` | `TN_DeathZoneVolume.h/.cpp`, `TN_SlowZoneVolume.h/.cpp`, `TN_RescuePickup.h/.cpp`, `TN_ThrowableItemActor.h/.cpp`, `TN_ItemSpawnZone.h/.cpp`, `TN_ButtonInteractable.h/.cpp`, `TN_InteractableBase.h/.cpp`, `TN_DirectInteractableBase.h/.cpp`, `TN_PickupInteractableBase.h/.cpp` |

---

## 2. Sesiones Steam y Matchmaking — `UMP_GameInstance`

<span class="file-tag">MP_GameInstance.h</span> <span class="file-tag">MP_GameInstance.cpp</span>

`MP_GameInstance` es el eje de toda la infraestructura de red. Persiste durante toda la sesión de juego (no se destruye entre mapas) y gestiona:

### 2.1 Flujo de Matchmaking

```
Menú → HostSession()        → CreateSession (OnlineSubsystemSteam)
     → FindAndJoinSession()  → FindSessions → JoinSession
            ↓
     ServerTravel a LVL_HQ  (Seamless = true)
```

Los callbacks de `IOnlineSession` (`OnCreateSessionComplete`, `OnFindSessionsComplete`, `OnJoinSessionComplete`) gestionan las transiciones y los errores de red. Si la sesión falla, se notifica al `MP_MainMenuWidget` para mostrar el error al jugador.

### 2.2 PendingTravelPlayerCount

Variable crítica que persiste entre mapas para sincronizar cuántos jugadores se esperan en el destino:

```cpp
// En TN_HQGameMode — antes del viaje a carrera
GameInstance->PendingTravelPlayerCount = GetNumPlayers();
AGameMode::ProcessServerTravel(TEXT("/Game/Maps/Run/LVL_Run?listen"));

// En TN_RunGameMode::BeginPlay() — espera a esa cantidad
WaitForPlayerCount = GameInstance->PendingTravelPlayerCount;
```

Sin este mecanismo, el `TN_RunGameMode` no sabe cuántos jugadores vienen y arrancaría la carrera antes de que todos estuviesen conectados.

### 2.3 Loading Screen Client-Side

La pantalla de carga se activa en el cliente desde `MP_GameInstance` justo antes del `ServerTravel` y se desactiva en `PostSeamlessTravel`. Esto cubre el freeze perceptible del cliente durante la carga del nuevo mapa, sin exponer el estado de transición.

### 2.4 Invitaciones y Desconexión

`MP_GameInstance` gestiona el callback `OnDestroySessionComplete` para limpiar la sesión correctamente cuando un jugador abandona o la partida termina. También procesa invitaciones de amigos de Steam a través de `OnSessionUserInviteAccepted`.

---

## 3. Seamless Travel — Viaje Sin Desconexión

La decisión más relevante de arquitectura de red del proyecto fue usar **Seamless Travel** en lugar de Non-Seamless Travel.

### 3.1 Por qué Seamless Travel

El Non-Seamless Travel destruye el `NetDriver` entre mapas, desconectando a todos los clientes y obligando a una nueva sesión de Steam. Para un juego de fiesta cooperativo donde la experiencia social continua es el núcleo, eso es inadmisible.

Con Seamless Travel:
- El `NetDriver` de Steam **nunca se destruye** entre viajes.
- `PlayerController` y `PlayerState` persisten entre mapas.
- El tiempo de transición se reduce a una pantalla de carga local.

### 3.2 HandleSeamlessTravelPlayer vs PostLogin

Para viajeros seamless, `PostLogin` **no se llama**. Los jugadores llegan a través de `HandleSeamlessTravelPlayer`:

```cpp
void ATN_RunGameMode::HandleSeamlessTravelPlayer(AController*& C)
{
    Super::HandleSeamlessTravelPlayer(C);
    // Aquí se re-aplican cosméticos, se inicializa el estado del jugador,
    // y se decrementa WaitForPlayerCount hasta que todos hayan llegado
    --WaitForPlayerCount;
    if (WaitForPlayerCount <= 0)
        StartRace();
}
```

### 3.3 PostSeamlessTravel

Tras el viaje, `PostSeamlessTravel` aplica cosméticos con un retry timer (5 intentos × 0.1s) para cubrir la race condition entre la llegada del `PlayerState` y la posesión del pawn:

```cpp
void ATN_HQGameMode::PostSeamlessTravel()
{
    Super::PostSeamlessTravel();
    // Retry timer: PlayerState y Pawn pueden no estar sincronizados aún
    GetWorldTimerManager().SetTimer(CosmeticRetryTimer,
        this, &ATN_HQGameMode::TryApplyAllCosmeticsWithRetry,
        0.1f, true);
}
```

---

## 4. Lobby HQ — `ATN_HQGameMode`

<span class="file-tag">TN_HQGameMode.h</span> <span class="file-tag">TN_HQGameMode.cpp</span>

### 4.1 Cuenta Atrás por Zona

El lobby usa un `TN_LobbyReadyZone`: un volumen `AActor + UBoxComponent` que detecta qué jugadores están dentro. La cuenta atrás se inicia cuando **todos los jugadores conectados** están en la zona:

```cpp
void ATN_HQGameMode::TickCountdown(float DeltaTime)
{
    // Si alguien sale, resetear
    if (PlayersInZone < GetNumPlayers())
    {
        ResetCountdown();
        return;
    }
    CountdownRemaining -= DeltaTime;
    CoopGameState->CountdownValue = FMath::CeilToInt(CountdownRemaining);
    CoopGameState->BroadcastFlowStateChange();
    if (CountdownRemaining <= 0.f)
        BeginMatchTravel();
}
```

La cuenta atrás no espera a un número fijo de jugadores (`LobbyExpectedPlayers`) sino a **todos los que están conectados en ese momento**, para soportar partidas de 1 a 4 jugadores sin configuración extra.

### 4.2 Destrucción de Pawns antes del Viaje

Antes de llamar a `ProcessServerTravel`, todos los pawns se destruyen explícitamente. Esto dispara `EndPlay(Destroyed)` en `ProximityVoiceComponent`, que limpia WASAPI mientras aún está activo. Si el `ServerTravel` destruyera los pawns internamente, WASAPI podría estar en estado indeterminado → `ACCESS_VIOLATION`.

---

## 5. Carrera — `ATN_RunGameMode`

<span class="file-tag">TN_RunGameMode.h</span> <span class="file-tag">TN_RunGameMode.cpp</span>

### 5.1 Inicio de Carrera

`BeginPlay` espera a recibir `PendingTravelPlayerCount` jugadores. Solo cuando llegan todos activa el `ATN_ChunkManager` y cambia `MatchFlowState` a `Racing`.

### 5.2 Gestión de Muertes y Rescates

Al morir un jugador:

```cpp
void ATN_RunGameMode::MarkPlayerDead(AController* Controller)
{
    // Guardar el pawn ANTES de UnPossess — GetPawn() devuelve null después
    APawn* DeadPawn = Controller->GetPawn();
    DeadPlayerPawns.Add(DeadPawn);

    // Spawn del pickup de rescate en el punto exacto de muerte
    FVector DeathLocation = DeadPawn->GetActorLocation();
    World->SpawnActor<ATN_RescuePickup>(RescuePickupClass, DeathLocation, FRotator::ZeroRotator);

    // Mover al jugador a espectador
    MovePlayerToSpectator(Controller);

    // Actualizar PlayerState
    if (ATN_CoopPlayerState* PS = Controller->GetPlayerState<ATN_CoopPlayerState>())
        PS->bIsEliminated = true;
}
```

### 5.3 Candidatos a Espectador

Solo son candidatos válidos para observar los jugadores con `bIsAlive && !bIsEliminated && !bHasFinishedRun`. Un jugador que ha terminado la carrera no aparece como candidato de espectado.

### 5.4 Fin de Carrera y Vuelta al Lobby

Cuando todos los jugadores activos terminan o son eliminados, el `TN_RunGameMode` recopila los rankings finales en `TN_CoopGameState` y lanza `ServerTravel` de vuelta a `LVL_HQ` con las estadísticas codificadas en la URL de viaje.

---

## 6. Estado Replicado — `ATN_CoopGameState`

<span class="file-tag">TN_CoopGameState.h</span> <span class="file-tag">TN_CoopGameState.cpp</span>

`TN_CoopGameState` es la **única fuente de verdad** replicada del partido. Cualquier widget o sistema que necesite datos del partido solo observa un delegate de este actor.

### 6.1 Variables Replicadas Clave

| Variable | Tipo | Descripción |
|---|---|---|
| `MatchFlowState` | `ETNMatchFlowState` | Estado actual de la partida (Lobby, Countdown, Racing, Finished…) |
| `CountdownValue` | `int32` | Valor del countdown visible en pantalla |
| `ServerMatchElapsedTime` | `float` | Tiempo de carrera transcurrido |
| `FinishedPlayersCount` | `int32` | Cuántos jugadores han llegado a meta |
| `PlayerRankings[]` | `TArray<FTNPlayerRank>` | Clasificación final al terminar |

### 6.2 BroadcastFlowStateChange — El Problema del OnRep en Listen-Server

`OnRep_*` **no dispara en la máquina que posee la variable**. En un listen-server (el host es también jugador), el servidor nunca recibiría la notificación de cambio de estado. La solución:

```cpp
// Patrón obligatorio: llamar SIEMPRE tras modificar MatchFlowState
void ATN_CoopGameState::SetMatchFlowState(ETNMatchFlowState NewState)
{
    MatchFlowState = NewState;
    BroadcastFlowStateChange(); // listen-server recibe la notificación vía delegate
    // Los clientes la recibirán via OnRep_MatchFlowState → también llama a Broadcast
}

void ATN_CoopGameState::BroadcastFlowStateChange()
{
    OnMatchFlowStateChanged.Broadcast(MatchFlowState);
}
```

### 6.3 Chat Rápido — Backend Replicado

Implementé el sistema de chat rápido en `TN_CoopGameState`: `ServerSendQuickChat_Implementation` valida el mensaje en el servidor y lo propaga a todos los clientes con `NetMulticast`. La capa de visualización en HUD (cola de mensajes, fade-out, corrección del bug de OnRep en listen-server) fue integrada y corregida por José Antonio.

---

## 7. Estado por Jugador — `ATN_CoopPlayerState`

<span class="file-tag">TN_CoopPlayerState.h</span> <span class="file-tag">TN_CoopPlayerState.cpp</span>

| Variable | Replicación | Descripción |
|---|---|---|
| `bIsAlive` | Todos | El jugador está vivo |
| `bIsEliminated` | Todos | Murió definitivamente en la carrera |
| `bHasFinishedRun` | Todos | Cruzó la línea de meta |
| `FinishRank` | Todos | Posición de llegada (1, 2, 3, 4) — nunca 0 |
| `EquippedHelmetId` | Todos | ID del helmet equipado (cosmética) |
| `EquippedSkinId` | Todos | ID del skin equipado (cosmética) |

**Regla crítica**: nunca usar `FinishRank == 0` para detectar jugadores eliminados. Todos los jugadores reciben un rango ≥ 1 al finalizar la partida. Usar siempre `bIsEliminated`.

Los campos de cosmética (`EquippedHelmetId`, `EquippedSkinId`) fueron añadidos en colaboración con José Antonio para que `OnRep_PlayerState` en el pawn pueda re-aplicar los cosméticos cuando el `PlayerState` llega tarde vía Seamless Travel.

---

## 8. Death, Rescue & Spectate

### 8.1 TN_DeathZoneVolume

<span class="file-tag">TN_DeathZoneVolume.h</span>

Volumen `AActor + UBoxComponent`. Implementa un **countdown de 3 segundos** mientras el jugador está dentro: si el jugador sale antes de que expire, el countdown se resetea. Si llega a 0, llama a `TN_RunGameMode::MarkPlayerDead()` directamente (sin DBNO/bleedout — el sistema de bleedout existe en código pero está desactivado para el flujo actual).

```cpp
void ATN_DeathZoneVolume::OnPlayerInsideCountdown()
{
    KillCountdownRemaining -= GetWorld()->GetDeltaSeconds();
    if (KillCountdownRemaining <= 0.f)
        RunGameMode->MarkPlayerDead(OverlappingController);
}
```

### 8.2 TN_RescuePickup

<span class="file-tag">TN_RescuePickup.h</span>

Objeto interactuable que se spawna en el punto exacto de muerte del jugador. Al interactuar con él, el compañero rescata al jugador muerto: el pawn se reactiva, el `PlayerController` lo re-posee y `bIsEliminated` vuelve a `false`.

La referencia al pawn del jugador muerto se guarda **antes** de `UnPossess` porque `GetPawn()` devuelve `null` después de que el controller es movido a espectador.

### 8.3 Flujo Completo de Muerte y Rescate

```
Jugador entra en DeathZone
    → Countdown 3s
    → MarkPlayerDead()
        → DeadPawn guardado antes de MovePlayerToSpectator
        → TN_RescuePickup spawneado en DeathLocation
        → Controller movido a espectador
        → bIsEliminated = true en PlayerState
    → Compañero interactúa con TN_RescuePickup
        → Pawn reactivado
        → Controller re-posee el pawn
        → bIsEliminated = false
        → RescuePickup destruido
```

---

## 9. Actores de Mundo Interactivos

### 9.1 Jerarquía de Interactuables

Se diseñó conjuntamente con José Antonio una jerarquía de interacción compartida:

```
TN_InteractableBase
├── TN_DirectInteractableBase     → interacción instantánea (botones, estaciones)
└── TN_PickupInteractableBase     → interacción de recogida (items, rescues)
```

Esta estructura garantiza que el sistema de interacción del `TortugaCharacter` (detección de rango, prompt UI, RPC de interacción) funcione de forma uniforme con todos los interactuables del mundo, independientemente de si son de José Antonio o de Rodrigo.

### 9.2 ATN_ThrowableItemActor

<span class="file-tag">TN_ThrowableItemActor.h</span>

Items físicos que el jugador puede recoger y lanzar. La física del lanzamiento se calcula en el servidor y se replica a los clientes. El item tiene peso, que afecta a la stamina efectiva del jugador que lo carga.

### 9.3 ATN_ItemSpawnZone

<span class="file-tag">TN_ItemSpawnZone.h</span>

Volumen que spawna items aleatorios de un pool configurable. Es Child Actor en los chunks y aplica el patrón de `SetTimerForNextTick` para diferir la lectura de posición, por las mismas razones que la gaviota y la medusa.

### 9.4 ATN_ButtonInteractable

<span class="file-tag">TN_ButtonInteractable.h</span>

Botón de activación en el mundo. Al interactuar, dispara un evento configurable (puede abrir una puerta, activar una trampa, etc.). Hereda de `TN_DirectInteractableBase`. También es Child Actor en chunks con el patrón de `SetTimerForNextTick`.

### 9.5 Pez Globo (Pufferfish)

Obstáculo con hitbox de contacto que aplica un empuje fuerte al jugador (tipo `LaunchCharacter`). A diferencia de la medusa (que aplica knockdown), el pez globo solo desestabiliza la trayectoria del jugador, permitiendo recuperarse más rápido.

### 9.6 TN_SlowZoneVolume

<span class="file-tag">TN_SlowZoneVolume.h</span>

Volumen que ralentiza al jugador al entrar. La ralentización no es fija: está modulada por el peso del inventario del jugador. Un jugador con objetos es más lento en la zona que uno sin ellos, reforzando el sistema de decisión táctica de inventario.

---

## 10. Patrones de Networking Documentados

Durante el desarrollo se consolidaron los siguientes patrones para evitar los bugs más frecuentes de multijugador en UE5:

| Patrón | Descripción |
|---|---|
| **OnRep en listen-server** | `OnRep_*` no dispara en el servidor que posee la variable. Siempre llamar `BroadcastFlowStateChange()` tras cambiar `MatchFlowState` |
| **HandleSeamlessTravelPlayer** | Para viajeros seamless, usar este método en lugar de `PostLogin`. `PostLogin` no se llama para viajeros |
| **Guardar pawn antes de UnPossess** | `GetPawn()` devuelve null después de `MovePlayerToSpectator`. Guardar la referencia antes |
| **No usar FinishRank == 0 para detectar eliminados** | Todos los jugadores reciben rango ≥ 1. Usar `bIsEliminated` en `TN_CoopPlayerState` |
| **Destruir pawns antes de ServerTravel** | Patrón descubierto y aplicado por José Antonio para garantizar el shutdown seguro de WASAPI (VOIP) |
| **PendingTravelPlayerCount** | Persiste en `MP_GameInstance` entre mapas para que `TN_RunGameMode` sepa cuántos jugadores esperar |

---

## 11. Variables Serializables (Principales)

| Sistema | Variables clave |
|---|---|
| **Sesión Steam** | `SessionName`, `MaxPublicConnections`, `bIsLANMatch` |
| **Countdown lobby** | `CountdownDuration` (default 5s), tiempo configurable antes del viaje |
| **Carrera** | `WaitForPlayerCount` (derivado de `PendingTravelPlayerCount`) |
| **Death Zone** | `KillCountdownDuration` (default 3s) |
| **Slow Zone** | `SpeedMultiplier`, `WeightInfluence`, `FadeInDuration` |
| **Interactuables** | `InteractionRange`, `InteractionPromptText` (en `TN_InteractableBase`) |
| **ThrowableItem** | `ItemWeight`, `ThrowImpulse`, `PickupRange` |

---

## 12. Conclusión

Mi trabajo en **Tortunabo** constituye el sustrato que hace posible que el juego sea multijugador: sin las sesiones Steam, el Seamless Travel, los game modes correctamente coordinados, el estado replicado centralizado y la gestión de muerte/rescate, no habría experiencia cooperativa. La arquitectura de `TN_CoopGameState` como fuente de verdad única y el patrón de `BroadcastFlowStateChange` eliminaron toda una clase de bugs de sincronización. El backend del chat rápido (RPC + Multicast en `TN_CoopGameState`) sienta la base de comunicación en tiempo real que José Antonio completó con la integración visual en pantalla.
