<style>
body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; max-width: 960px; margin: 0 auto; padding: 40px 20px; color: #2c3e50; line-height: 1.7; }
h1 { color: #1a1a2e; border-bottom: 3px solid #2ecc71; padding-bottom: 12px; font-size: 2em; }
h2 { color: #16213e; border-left: 4px solid #2ecc71; padding-left: 12px; margin-top: 40px; }
h3 { color: #0f3460; margin-top: 24px; }
h4 { color: #27ae60; }
table { width: 100%; border-collapse: collapse; margin: 20px 0; }
th, td { border: 1px solid #ddd; padding: 10px 14px; text-align: left; }
th { background-color: #1a1a2e; color: white; }
tr:nth-child(even) { background-color: #f8f9fa; }
code { background: #f0f0f0; padding: 2px 6px; border-radius: 3px; font-size: 0.9em; }
pre { background: #1e1e2e; color: #cdd6f4; padding: 16px; border-radius: 8px; overflow-x: auto; font-size: 0.85em; line-height: 1.5; }
pre code { background: none; color: inherit; padding: 0; }
.highlight { background: #d4edda; padding: 12px 16px; border-left: 4px solid #2ecc71; margin: 16px 0; border-radius: 4px; }
.info { background: #d1ecf1; padding: 12px 16px; border-left: 4px solid #17a2b8; margin: 16px 0; border-radius: 4px; }
.warning { background: #fff3cd; padding: 12px 16px; border-left: 4px solid #ffc107; margin: 16px 0; border-radius: 4px; }
.section-box { background: #f8f9fa; border: 1px solid #dee2e6; border-radius: 8px; padding: 16px 20px; margin: 16px 0; }
.component-tag { display: inline-block; background: #2ecc71; color: white; padding: 2px 8px; border-radius: 4px; font-size: 0.8em; margin-right: 4px; }
.file-tag { display: inline-block; background: #0f3460; color: white; padding: 2px 8px; border-radius: 4px; font-size: 0.8em; }
.author-jose { display: inline-block; background: #2980b9; color: white; padding: 2px 8px; border-radius: 4px; font-size: 0.8em; }
.author-rodrigo { display: inline-block; background: #8e44ad; color: white; padding: 2px 8px; border-radius: 4px; font-size: 0.8em; }
.author-common { display: inline-block; background: #27ae60; color: white; padding: 2px 8px; border-radius: 4px; font-size: 0.8em; }
</style>

# Tortunabo — Memoria de Equipo (Programación)

**Motor:** Unreal Engine 5.6 | **Lenguaje:** C++ | **Red:** Steam Sockets (SteamDevAppId=480)
**Fecha:** Marzo 2026

---

## 1. Descripción del Juego

**Tortunabo** es un juego cooperativo multijugador (1–4 jugadores) en tercera persona desarrollado en **Unreal Engine 5.6** con C++. Los jugadores controlan tortugas antropomórficas que compiten y colaboran a través de niveles generados proceduralmente, con mecánicas de objetos, emotes, personalización de cosméticos y comunicación por voz de proximidad.

El núcleo del juego es la **carrera cooperativa**: todos los jugadores avanzan juntos por un mapa generado dinámicamente por chunks, enfrentándose a obstáculos (zonas lentas, peces globo, medusas, gaviotas), recogiendo y lanzando objetos, y tratando de llegar a la meta. Los jugadores muertos pueden ser rescatados por sus compañeros antes de que expire un temporizador.

El flujo de partida es:
```
Menú Principal → Lobby HQ → Cuenta Atrás → Cinemática → Carrera → Resultados → Lobby HQ
```

Las transiciones entre mapas usan **Seamless Travel** para mantener la sesión Steam viva sin desconexiones.

---

## 2. Equipo de Programación

| Nombre                         | Alias Git           | Rol principal                               | Commits |
| ------------------------------ | ------------------- | ------------------------------------------- | ------- |
| **José Antonio Mota Lucas**    | `Mokius`            | Personaje, UI, sistemas de mundo, cosmética | 37      |
| **Rodrigo Fernández Carnicer** | `Rodrigo Fernandez` | Red/Networking, flujo de partida, gameplay  | 30      |

---

## 3. Arquitectura del Proyecto

### 3.1 Por qué esta estructura y cómo favorece la modularidad

El proyecto sigue el **patrón de arquitectura por dominios** propio de Unreal Engine, donde cada carpeta agrupa clases con la misma responsabilidad funcional. Esta decisión se tomó desde el inicio por tres razones concretas:

**1. Separación de responsabilidades entre programadores sin conflictos de merge.**
Al tener carpetas `Player/`, `World/`, `UI/` y `Core/` bien delimitadas, cada programador podía trabajar en su área sin pisar los archivos del otro. En la práctica: José Antonio trabajaba en `Player/` y `UI/`, Rodrigo en `Core/`, `Game/`, `Lobby/` y `Multiplayer/`, y los conflictos de git se redujeron a los archivos verdaderamente compartidos (principalmente `TortugaCharacter.h`).

**2. Escalabilidad sin refactorización.**
Añadir un nuevo obstáculo de mundo (p.ej. `TN_JellyfishActor`) solo requiere crear el archivo en `World/` y referenciarlo desde el chunk Blueprint. No hay que tocar nada del sistema de red ni del personaje. Lo mismo aplica a un nuevo widget de UI: vive en `UI/HUD/` y se conecta al `TN_CoopGameState` via delegate, sin modificar ningún GameMode.

**3. El personaje usa composición, no herencia profunda.**
En lugar de una cadena `ABaseCharacter → ACombatCharacter → AInventoryCharacter...`, toda la lógica auxiliar vive en componentes (`TN_StaminaComponent`, `TN_InventoryComponent`, `ProximityVoiceComponent`). Cada componente se desarrolla, serializa y prueba de forma independiente. `TortugaCharacter` actúa únicamente como punto de ensamblaje.

```
ATortugaCharacter  ←  clase ligera, solo coordinación
├── UTN_StaminaComponent    → lógica de resistencia (aislada, testeable)
├── UTN_InventoryComponent  → lógica de objetos (aislada, testeable)
└── ProximityVoiceComponent → VOIP (aislado, con su propio ciclo de vida)
```

Esta arquitectura permite que en el futuro se puedan añadir modos de juego con distintas reglas de stamina o inventario simplemente swapeando o configurando los componentes, sin modificar la clase base del personaje.

### 3.2 Diagrama de Módulos

```
┌──────────────────────────────────────────────────────────────────┐
│                        MP_GameInstance                            │
│  (Sesiones Steam, guardado de cosméticos, loading screen)        │
├──────────────────────────────────────────────────────────────────┤
│  ATN_CoopGameState  ←─── fuente de verdad replicada del partido  │
│  ATN_CoopPlayerState ←── estado por jugador (rango, vida, etc.)  │
└──────────────────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────────────────┐
│                      ATortugaCharacter                            │
│  (ACharacter + Enhanced Input + Cosmética + Replicación)         │
├────────────────┬───────────────────┬─────────────────────────────┤
│UTN_Stamina     │UTN_Inventory      │ProximityVoiceComponent      │
│Component       │Component          │(WASAPI VOIP de proximidad)  │
├────────────────┴───────────────────┴─────────────────────────────┤
│               ATortugaFirstPersonCharacter                        │
│               (hereda TortugaCharacter — cinemáticas)            │
└──────────────────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────────────────┐
│                    Sistemas de Mundo                              │
├────────────────┬───────────────────┬─────────────────────────────┤
│ATN_ChunkManager│ATN_DeathZoneVolume│ATN_SlowZoneVolume           │
│(generación     │(muerte + rescate  │(ralentización con           │
│ procedural)    │ + espectador)     │ peso del inventario)        │
├────────────────┼───────────────────┼─────────────────────────────┤
│ATN_SeagullActor│ATN_JellyfishActor │ATN_ThrowableItemActor       │
│(obstáculo radio│(obstáculo cuerpo) │(objetos lanzables)          │
├────────────────┼───────────────────┼─────────────────────────────┤
│ATN_ButtonInter.│ATN_ItemSpawnZone  │ATN_RescuePickup             │
│(botón de mundo)│(spawn aleatorio)  │(rescate de jugador muerto)  │
└────────────────┴───────────────────┴─────────────────────────────┘

┌──────────────────────────────────────────────────────────────────┐
│                   Game Modes por Mapa                            │
├────────────────┬───────────────────┬─────────────────────────────┤
│MP_MenuGameMode │TN_HQGameMode      │TN_RunGameMode               │
│(LVL_Menu)      │(LVL_HQ / Lobby)   │(LVL_Run / Carrera)         │
└────────────────┴───────────────────┴─────────────────────────────┘
```

### 3.3 Estructura de Carpetas (Source)

```
Source/Tortunabo/
├── Core/        → Estado de partida replicado (TN_CoopGameState, TN_CoopPlayerState, tipos)
├── Game/        → Reglas de carrera (TN_RunGameMode)
├── Lobby/       → Lógica del lobby HQ (TN_HQGameMode, TN_LobbyReadyZone)
├── Menu/        → Menú principal (MP_MenuGameMode, MP_MenuPlayerController)
├── Multiplayer/ → Sesiones Steam, guardado, cosméticos (MP_GameInstance, TN_CosmeticSaveGame)
├── Player/      → Personaje, controlador, stamina, inventario, animación de piernas
├── Voice/       → VOIP de proximidad (ProximityVoiceComponent / WASAPI)
├── World/       → Interactuables, volúmenes, objetos, chunk manager
└── UI/          → HUD, rueda radial, cosmetics menu, indicador de voz
```

**Por qué un módulo único (`Tortunabo`) y no varios módulos separados:**
Con un solo módulo de compilación, cualquier clase puede incluir cualquier otra sin dependencias entre módulos. Para un equipo de dos personas en un prototipo, esto reduce la fricción de desarrollo. La separación en carpetas proporciona el orden conceptual sin el coste de mantener múltiples `Build.cs`.

---

## 4. División de Tareas

### 4.1 Trabajo Común — <span class="author-common">José Antonio + Rodrigo</span>

Hubo áreas en las que ambos programadores intervinieron en distintos momentos del desarrollo, bien por iteraciones sucesivas, bien por integración de sistemas que pertenecían a los dos:

| Sistema | Descripción |
|---|---|
| **Flujo de partida completo** | La cadena Menu→Lobby→Carrera→Resultados→Lobby requirió coordinación constante: Rodrigo construyó los game modes y el Seamless Travel, José Antonio integró la UI de cada fase y los cosméticos en cada transición |
| **Replicación de emotes y knockdown** | El sistema de emotes (José Antonio) se reaprovechó para propagar el knockdown visual (Rodrigo): `KNOCKDOWN_EMOTE_ID = 100` como índice reservado, eliminando la necesidad de un Multicast separado |
| **TN_CoopPlayerState** | Rodrigo definió la estructura de estado por jugador; José Antonio añadió los campos de cosmética (`EquippedHelmetId`, `EquippedSkinId`) y se aseguró de que `OnRep_PlayerState` en el pawn re-aplicase los cosméticos al llegar tarde vía Seamless Travel |
| **Sistema de interacción** | La jerarquía `TN_InteractableBase → TN_DirectInteractableBase / TN_PickupInteractableBase` fue diseñada conjuntamente para que tanto los items de Rodrigo (`TN_RescuePickup`, `TN_ItemSpawnZone`) como los actores de mundo de José Antonio (`TN_CosmeticsStationInteractable`, `TN_SkinStatueActor`) compartiesen el mismo flujo de interacción del personaje |
| **Depuración multijugador** | Los bugs de red más complejos (race conditions en cosmética, `NetChecksumMismatch`, crash de VOIP) fueron investigados y resueltos en conjunto, ya que cruzaban la frontera entre sistemas de ambos |
| **Config crítica** | `DefaultEngine.ini`: `GameInstanceClass`, rutas de mapas, `bUseSeamlessTravel`, `[Voice] bEnabled=false` — consensuada entre los dos para que ningún cambio rompiese el otro sistema |

---

### 4.2 José Antonio Mota Lucas — <span class="author-jose">Mokius</span>

Responsable de la **base del personaje jugable**, todos los **sistemas de UI e interacción**, los **actores de mundo especiales**, la **cosmética**, la implementación en C++ del **sistema de chunks procedurales**, el **VOIP de proximidad** y la integración de la pantalla de **chat rápido**.

| Sistema | Descripción |
|---|---|
| **TortugaCharacter** | Clase base del personaje: movimiento (andar/sprint), cámara third-person con lag y zoom dinámico, Enhanced Input con soporte mando y teclado, sistema de emotes replicados |
| **TN_StaminaComponent** | MaxStamina=200, drenaje al sprint (45/s), recharge delay, penalización por agotamiento, reducción de stamina máxima por peso del inventario |
| **TN_InventoryComponent** | Sistema de dos slots (equipado + almacenado), peso de item que afecta la stamina efectiva |
| **TN_LegAnimComponent** | Animación procedural de piernas para adaptarse al terreno |
| **Radial Emotes & Chat** | `TN_RadialWheelWidgetBase`, `TN_EmoteWheelDataAsset`, `TN_QuickChatWheelDataAsset` — rueda radial dual (emotes + chat rápido); integración de la visualización en HUD y correcciones de eficiencia del sistema de chat rápido implementado por Rodrigo |
| **UI / HUD** | `TN_PlayerHUDWidget`, `TN_InteractPromptWidget`, `TN_LoadingScreenWidget`, `TN_CoopFlowHUDWidget` (incluye pantalla de mensajes de chat rápido) |
| **Cosmetics UI** | `TN_CosmeticsMenuWidget`, `TN_CosmeticSaveGame` — guardado persistente de helmet y skin con `USaveGame` |
| **ProximityVoiceComponent** | VOIP de proximidad con `FAudioCaptureSynth` (WASAPI); patrón de shutdown seguro destruyendo pawns antes de `ServerTravel`; corrección del crash `ACCESS_VIOLATION` al llamar `ShutdownAllCapture` desde GameMode |
| **TN_SeagullActor** | Obstáculo: gaviota que patrulla un radio configurado con sonido espacial |
| **TN_JellyfishActor** | Obstáculo: medusa con VFX/SFX, colisión que aplica knockdown |
| **TN_StormVolume** | Volumen de tormenta que modifica el estado visual/sonoro de la zona |
| **TN_ChunkManager** | Motor de generación procedural: 3 pools (Easy/Medium/Hard), sistema de sockets InSocket/OutSocket, spawn en posición final para evitar BeginPlay en posición errónea, modo de secuencia personalizable |
| **ATortugaFirstPersonCharacter** | Hereda de TortugaCharacter con brazo de cámara a 0 — uso exclusivo en cinemáticas |
| **Correcciones críticas** | Fix de `TN_FinishLineVolume`, corrección del sistema de respawn, fix de colisiones en chunks finales, corrección del crash de VOIP (`ACCESS_VIOLATION`), 5 bugs de gameplay multijugador |

#### Detalle técnico — Sistema de Stamina con Peso

```cpp
// TN_StaminaComponent — Stamina efectiva reducida por items cargados
float GetEffectiveMaxStamina() const
{
    float WeightPenalty = InventoryWeight * StaminaPerWeightUnit; // 20 por unidad
    return FMath::Max(0.f, MaxStamina - WeightPenalty);
}
```

El peso del inventario reduce el techo de stamina disponible, forzando al jugador a elegir entre velocidad y utilidad táctica de los objetos.

#### Detalle técnico — Spawn de Chunks en Posición Final

```cpp
// TN_ChunkManager — Patrón de dos fases para evitar BeginPlay en origen
// Fase 1: actor temporal para leer el socket de entrada
AActor* TempActor = World->SpawnActor<AActor>(ChunkClass, ...);
FTransform InSocket = GetSocketTransform(TempActor, "InSocket");
TempActor->Destroy();

// Fase 2: spawn real en la posición final calculada
FTransform FinalTransform = InSocket.Inverse() * TargetTransform;
AActor* NewChunk = World->SpawnActor<AActor>(ChunkClass, FinalTransform);
```

Este patrón garantiza que los Child Actors dentro de cada chunk ejecuten su `BeginPlay` ya en la posición definitiva del mundo. Sin él, los actores embebidos (gaviotas, botones, item spawners) leen posición `(0,0,0)` en su primer tick.

---

### 4.3 Rodrigo Fernández Carnicer — <span class="author-rodrigo">Rodrigo Fernandez</span>

Responsable de toda la **infraestructura de red multijugador**, el **flujo completo de partida**, los **game modes**, la **death/rescue loop** y el **backend del chat rápido**.

| Sistema | Descripción |
|---|---|
| **Seamless Travel** | `MP_GameInstance` con `bUseSeamlessTravel = true`; `HandleSeamlessTravelPlayer` / `PostSeamlessTravel` en lugar de `PostLogin`; `PendingTravelPlayerCount` para sincronizar cuántos jugadores se esperan en el mapa destino |
| **MP_GameInstance** | Sesiones Steam (host/find&join/destroy), callbacks de OnlineSubsystem, invitación de amigos, loading screen client-side en viaje, manejo de desconexión |
| **TN_HQGameMode** | Lógica del lobby: `TN_LobbyReadyZone`, cuenta atrás replicada cuando todos los jugadores están en la zona, escritura de `PendingTravelPlayerCount` antes del viaje |
| **TN_RunGameMode** | Inicio de carrera (espera N jugadores del viaje), gestión de muertes, rescates, finalistas, espectadores, vuelta al lobby con estadísticas |
| **TN_CoopGameState** | Fuente de verdad replicada: `MatchFlowState`, contadores de jugadores, `CountdownValue`, `ServerMatchElapsedTime`, rankings, `ServerSendQuickChat` (RPC y Multicast del chat rápido) |
| **TN_CoopPlayerState** | Estado por jugador: `bIsAlive`, `bIsEliminated`, `bHasFinishedRun`, `FinishRank` |
| **TN_DeathZoneVolume** | Countdown de 3s → muerte directa → spawn de `TN_RescuePickup` en el punto de muerte → jugador a espectador |
| **TN_RescuePickup** | Objeto interactuable que revive al jugador eliminado; referencia al pawn guardada antes de `UnPossess` |
| **ATN_ThrowableItemActor** | Items físicos lanzables con replicación de estado |
| **ATN_ItemSpawnZone** | Zona de spawn aleatorio de items en los chunks |
| **ATN_ButtonInteractable** | Botón de activación en el mundo con lógica de interacción replicada |
| **Pufferfish (Pez Globo)** | Obstáculo: pez globo con hitbox y efectos de empuje/daño |
| **Correcciones de red** | `NetChecksumMismatch`, race conditions en `PostLogin`, lobby find/join con SteamDevAppId=480 |

#### Detalle técnico — BroadcastFlowStateChange (OnRep en listen-server)

```cpp
// OnRep no dispara en el servidor que posee la variable → notificación manual
void ATN_CoopGameState::BroadcastFlowStateChange()
{
    OnMatchFlowStateChanged.Broadcast(MatchFlowState);
}

// En TN_HQGameMode tras cada transición:
CoopGameState->MatchFlowState = ETNMatchFlowState::Countdown;
CoopGameState->BroadcastFlowStateChange(); // el listen-server también recibe la notificación
```

#### Detalle técnico — Patrón de Rescate

```cpp
// Guardar el pawn ANTES de UnPossess — GetPawn() devuelve null después
APawn* DeadPawn = PlayerState->GetPawn();
DeadPlayerPawns.Add(DeadPawn);
MovePlayerToSpectator(Controller);

// Spawn del pickup en el punto exacto de muerte
World->SpawnActor<ATN_RescuePickup>(RescuePickupClass, DeadLocation, FRotator::ZeroRotator);
```

---

## 5. Controles

### PC

| Acción | Tecla |
|---|---|
| Movimiento | WASD |
| Cámara | Ratón |
| Saltar | Espacio |
| Sprint | Shift (toggle configurable) |
| Interactuar | E |
| Abrir rueda de emotes | Tab (mantener) |
| Abrir rueda de chat | Q (mantener) |
| Equipar / Rotar item | Rueda del ratón |
| Soltar item | G |

### Mando

| Acción | Botón |
|---|---|
| Movimiento | Stick izquierdo |
| Cámara | Stick derecho |
| Saltar | A / Cruz |
| Sprint | L3 (toggle) |
| Interactuar | X / Cuadrado |
| Rueda emotes | LB / L1 (mantener) |
| Rueda chat | RB / R1 (mantener) |
| Rotar inventario | D-Pad izq/der |
| Soltar item | B / Círculo |

---

## 6. Decisiones de Diseño y Justificación Técnica

### ¿Por qué Seamless Travel y no Non-Seamless?

El Non-Seamless Travel destruye el `NetDriver`, desconectando a todos los clientes entre cada mapa y obligando a una nueva ronda de matchmaking. Para un juego de fiesta cooperativo donde la experiencia social continua es el núcleo, eso es inadmisible. El Seamless Travel mantiene la sesión Steam activa, las `PlayerController` y `PlayerState` persisten entre viajes, y el tiempo de transición se reduce a una pantalla de carga local. Esto también sienta las bases para añadir mapas adicionales en el futuro sin coste arquitectural extra.

### ¿Por qué `TN_CoopGameState` como fuente de verdad central?

Distribuir el estado de partida entre múltiples actores introduce incoherencias: si el GameMode sabe algo que el HUD no sabe aún, se producen bugs visuales. Al centralizar toda la información replicada en un único `GameState` (flujo de juego, contadores, rankings, chat), cualquier widget o sistema solo necesita observar un delegate. Esto hace que añadir una nueva pantalla de UI no requiera tocar ningún GameMode.

### ¿Por qué componentes en el personaje y no una clase monolítica?

Un `ATortugaCharacter` con 2000 líneas que gestione stamina, inventario, voz, cosmética y movimiento es imposible de mantener en equipo. Al separar en `TN_StaminaComponent`, `TN_InventoryComponent` y `ProximityVoiceComponent`:
- Cada componente tiene un ciclo de vida propio (`BeginPlay`/`EndPlay`), minimizando bugs de inicialización.
- Se puede serializar y ajustar cada sistema por separado en el Editor sin recompilar.
- Añadir un nuevo sistema (p.ej. un `TN_AbilityComponent` para un futuro modo de juego) es agregar un componente, no refactorizar el personaje.

### ¿Por qué `UBoxComponent` en volúmenes dentro de chunks y no `ATriggerVolume`?

`ATriggerVolume` hereda de `ABrush` (actor de geometría BSP), que tiene restricciones internas de Unreal que impiden colocarlo como Child Actor dentro de un Blueprint Actor. Como los chunks son Blueprint Actors con Child Actors embebidos, todos sus volúmenes de detección usan `AActor + UBoxComponent`, que no tiene esas limitaciones y funciona igual de bien en red.

### ¿Por qué stamina con penalización por peso?

El sistema de peso crea una decisión táctica real: recoger un objeto aumenta las capacidades del jugador (lanzar a obstáculos, rescatar compañeros) pero limita cuánto puede sprintar. Esto evita que el inventario sea siempre la opción obvia y añade profundidad cooperativa — en un grupo, los jugadores con más stamina se pueden encargar de cargar objetos mientras otros sirven de apoyo.

### ¿Por qué Data Assets para las ruedas radiales?

`TN_EmoteWheelDataAsset` y `TN_QuickChatWheelDataAsset` son `UDataAsset`, no arrays hardcodeados en el código. Esto permite al equipo de diseño añadir, reordenar o cambiar emotes y mensajes de chat sin modificar ni recompilar C++. Es el patrón correcto para contenido que se itera frecuentemente y que no tiene lógica de gameplay asociada.

---

## 7. Sistemas Clave — Detalle Técnico

### 7.1 Flujo de Matchmaking con Steam

```
Menú → HostSession() / FindAndJoinSession()
     → [Steam OnlineSubsystem] CreateSession / FindSessions / JoinSession
     → ServerTravel a LVL_HQ  (Seamless = true)
     → PostSeamlessTravel → aplica cosméticos con retry timer (5 × 0.1s)
     → Todos en ready zone → countdown → ServerTravel a LVL_Run
     → PostSeamlessTravel → espera PendingTravelPlayerCount jugadores
     → Carrera → todos terminan / muertos → ServerTravel a LVL_HQ
```

El `NetDriver` de Steam **nunca se destruye** durante el viaje, por lo que las `PlayerController` y `PlayerState` persisten. El `GameMode` recibe a los viajeros en `HandleSeamlessTravelPlayer`, no en `PostLogin`.

### 7.2 Sistema de Cosmética Persistente

```
TN_CosmeticSaveGame (USaveGame)
    ↓ Save/Load vía MP_GameInstance
TN_CoopPlayerState.EquippedHelmetId / EquippedSkinId  (replicados)
    ↓ OnRep_PlayerState() en TortugaCharacter
ATortugaCharacter::ApplyHelmet / ApplySkin
    ↓ Retry timer en PostSeamlessTravel (resuelve race condition PlayerState vs Pawn)
```

### 7.3 Chunk Manager — Generación Procedural

El `ATN_ChunkManager` mantiene una **ventana deslizante de 3 chunks activos**. Cuando un jugador activa el `EndTrigger` del chunk actual:

1. Destruye el chunk más antiguo.
2. Calcula el transform de salida (`OutSocket`) del chunk actual.
3. Spawna el siguiente chunk con el patrón de dos fases.
4. Incrementa el contador de chunks para escalar la dificultad: **Easy → Medium → Hard**.

El modo de secuencia personalizada (`bUseRandomGeneration = false`) permite definir explícitamente la dificultad de cada posición para diseñar carreras con curva de dificultad controlada.

### 7.4 Knockdown y Replicación Visual

El knockdown aplica 180° de rotación al personaje. Para evitar que el `CharacterMovementComponent` anule la rotación via network smoothing:

```cpp
GetCharacterMovement()->NetworkSmoothingMode = ENetworkSmoothingMode::Disabled;
SetRelativeRotation(KnockdownRotation); // Relativo, no world — CMC usa rotación relativa

// MulticastApplyKnockdownVisual (Reliable) — todos los clientes lo reciben inmediatamente
```

El `KNOCKDOWN_EMOTE_ID = 100` es un índice reservado que reutiliza el sistema de emotes para propagar el knockdown visual, evitando duplicar lógica de replicación.

---

## 8. Patrones de Networking Consolidados

Durante el desarrollo se documentaron los siguientes patrones que evitan los bugs más frecuentes de multijugador en UE5:

| Patrón | Descripción |
|---|---|
| **OnRep en listen-server** | `OnRep_*` no dispara en la máquina que posee la variable. Siempre llamar `BroadcastFlowStateChange()` tras modificar `MatchFlowState` |
| **HandleSeamlessTravelPlayer** | Para viajeros seamless, usar este método en lugar de `PostLogin` — `PostLogin` no se llama para viajeros |
| **Guardar pawn antes de UnPossess** | `GetPawn()` devuelve null después de `UnPossess`. Guardar la referencia antes de mover al jugador a espectador |
| **No usar FinishRank == 0 para detectar eliminados** | Todos los jugadores reciben rango ≥ 1. Usar `bIsEliminated` en `TN_CoopPlayerState` |
| **Spawn de Child Actors en posición final** | Nunca spawnear en Identity y teleportar — los Child Actors ya ejecutaron `BeginPlay` en la posición incorrecta |
| **SetTimerForNextTick en Child Actors de chunks** | Actores embebidos deben diferir lecturas de posición un tick para que el chunk esté ya en su lugar |
| **FTimerDelegate::CreateUObject en chunks** | Usar `CreateUObject` (no lambdas) para que `EndPlay → ClearAllTimersForObject` cancele timers al destruir chunks temporales |
| **Shutdown seguro de WASAPI** | Destruir los pawns antes de `ServerTravel`; WASAPI se limpia en `EndPlay(Destroyed)`. Llamar `ShutdownAllCapture` directamente desde un GameMode causa `ACCESS_VIOLATION` — patrón descubierto y resuelto por José Antonio al corregir el crash de VOIP |

---

## 9. Tecnologías Utilizadas

| Tecnología | Uso |
|---|---|
| **Unreal Engine 5.6** | Motor de juego |
| **C++** | 100% lógica de gameplay (Blueprints solo para assets y parámetros) |
| **Enhanced Input System** | Input de teclado/ratón y mando unificado con soft references |
| **OnlineSubsystemSteam** | Sesiones P2P, matchmaking, invitaciones a amigos |
| **Steam Sockets (NetDriver)** | Transporte de red con SteamDevAppId=480 para testing |
| **Seamless Travel** | Transición entre mapas sin destruir el NetDriver ni desconectar clientes |
| **USaveGame** | Persistencia de cosméticos entre sesiones |
| **UMG (Unreal Motion Graphics)** | Toda la interfaz de usuario |
| **UDataAsset** | Configuración de ruedas radiales sin código |
| **Niagara Particle System** | VFX de medusas, tormenta, knockdown |
| **WASAPI (FAudioCaptureSynth)** | Captura de audio para VOIP de proximidad |

---

## 10. Variables Serializables Principales

Todas las variables relevantes de gameplay están expuestas como `UPROPERTY(EditDefaultsOnly)` o `UPROPERTY(EditAnywhere)` para ajuste desde el Editor sin recompilar:

| Sistema | Variables clave |
|---|---|
| **Stamina** | `MaxStamina`, `SprintDrainRate`, `RechargeDelay`, `ExhaustionPenaltyDuration`, `StaminaPerWeightUnit` |
| **Cámara** | `TargetArmLength`, `CameraLagSpeed`, `SprintFOVOffset`, `SprintZoomDistance` |
| **Inventario** | `MaxSlots`, `ItemWeightMap`, `DropImpulseStrength` |
| **Chunk Manager** | `EasyToMediumThreshold`, `MediumToHardThreshold`, `TotalChunksBeforeFinal`, `bUseRandomGeneration`, `CustomChunkSequence` |
| **Slow Zone** | `SpeedMultiplier`, `WeightInfluence`, `FadeInDuration` |
| **Death Zone** | `KillCountdownDuration` (default 3s) |
| **Cosmética** | `HelmetCrateEntries[]` (id + peso de probabilidad en `MP_GameInstance`) |
| **VOIP** | `ProximityMaxDistance`, `ProximityFalloffExponent` |

---

## 11. Estructura de Archivos Desarrollados

| Carpeta | Archivos C++ |
|---|---|
| `Core/` | `TN_CoopGameState`, `TN_CoopPlayerState`, `TN_MatchFlowTypes`, `TN_CosmeticsTypes`, `TN_InventoryTypes` |
| `Game/` | `TN_RunGameMode` |
| `Lobby/` | `TN_HQGameMode`, `TN_LobbyReadyZone` |
| `Menu/` | `MP_MenuGameMode`, `MP_MenuPlayerController` |
| `Multiplayer/` | `MP_GameInstance`, `TN_CosmeticSaveGame` |
| `Player/` | `TortugaCharacter`, `TortugaFirstPersonCharacter`, `MP_GamePlayerController`, `TN_StaminaComponent`, `TN_InventoryComponent`, `TN_LegAnimComponent` |
| `Voice/` | `ProximityVoiceComponent` |
| `UI/HUD/` | `TN_PlayerHUDWidget`, `TN_CoopFlowHUDWidget`, `TN_CosmeticsMenuWidget`, `TN_InteractPromptWidget`, `TN_LoadingScreenWidget`, `TN_RadialWheelWidgetBase`, `TN_RadialWheelTypes`, `TN_EmoteWheelDataAsset`, `TN_QuickChatWheelDataAsset` |
| `UI/Menu/` | `MP_MainMenuWidget` |
| `UI/Voice/` | `VoiceIndicatorWidget` |
| `World/` | `TN_ChunkManager`, `TN_DeathZoneVolume`, `TN_SlowZoneVolume`, `TN_StormVolume`, `TN_FinishLineVolume`, `TN_SeagullActor`, `TN_JellyfishActor`, `TN_ThrowableItemActor`, `TN_ItemSpawnZone`, `TN_RescuePickup`, `TN_ButtonInteractable`, `TN_InteractableBase`, `TN_DirectInteractableBase`, `TN_PickupInteractableBase`, `TN_SkinStatueActor`, `TN_CosmeticsStationInteractable` |

---

## 12. Conclusión

**Tortunabo** es un proyecto significativamente más ambicioso que el anterior en cuanto a infraestructura técnica: multiplayer en red con Steam Sockets, Seamless Travel entre tres mapas, generación procedural de niveles y VOIP de proximidad, todo construido en C++ sobre Unreal Engine 5.6.

La arquitectura por dominios (carpetas por responsabilidad funcional) y el uso de composición sobre herencia en el personaje han demostrado ser las decisiones más beneficiosas del proyecto: permitieron que dos programadores trabajasen en paralelo casi sin conflictos de merge, y que cada nuevo sistema se añadiese en su carpeta correspondiente sin modificar código existente.

Los patrones de networking consolidados durante el desarrollo constituyen una base técnica documentada que evita los errores más costosos de multijugador en UE5 — errores que, sin esta documentación, requieren días de depuración para cada proyecto nuevo.
