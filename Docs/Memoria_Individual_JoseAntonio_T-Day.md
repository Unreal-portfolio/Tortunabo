<style>
body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; max-width: 950px; margin: 0 auto; padding: 40px 20px; color: #2c3e50; line-height: 1.7; }
h1 { color: #1a1a2e; border-bottom: 3px solid #2980b9; padding-bottom: 12px; font-size: 2em; }
h2 { color: #16213e; border-left: 4px solid #2980b9; padding-left: 12px; margin-top: 40px; }
h3 { color: #0f3460; margin-top: 24px; }
h4 { color: #1a6aba; }
table { width: 100%; border-collapse: collapse; margin: 20px 0; }
th, td { border: 1px solid #ddd; padding: 10px 14px; text-align: left; }
th { background-color: #1a1a2e; color: white; }
tr:nth-child(even) { background-color: #f8f9fa; }
code { background: #f0f0f0; padding: 2px 6px; border-radius: 3px; font-size: 0.9em; }
pre { background: #1e1e2e; color: #cdd6f4; padding: 16px; border-radius: 8px; overflow-x: auto; font-size: 0.85em; line-height: 1.5; }
pre code { background: none; color: inherit; padding: 0; }
.highlight { background: #d6eaf8; padding: 12px 16px; border-left: 4px solid #2980b9; margin: 16px 0; border-radius: 4px; }
.info { background: #d1ecf1; padding: 12px 16px; border-left: 4px solid #17a2b8; margin: 16px 0; border-radius: 4px; }
.warning { background: #fff3cd; padding: 12px 16px; border-left: 4px solid #ffc107; margin: 16px 0; border-radius: 4px; }
.section-box { background: #f8f9fa; border: 1px solid #dee2e6; border-radius: 8px; padding: 16px 20px; margin: 16px 0; }
.component-tag { display: inline-block; background: #2980b9; color: white; padding: 2px 8px; border-radius: 4px; font-size: 0.8em; margin-right: 4px; }
.file-tag { display: inline-block; background: #0f3460; color: white; padding: 2px 8px; border-radius: 4px; font-size: 0.8em; }
</style>

# Tortunabo — Memoria Individual

**Autor:** José Antonio Mota Lucas
**Motor:** Unreal Engine 5.6 | **Lenguaje:** C++ | **Red:** Steam Sockets
**Fecha:** Marzo 2026

---

## 1. Resumen de Responsabilidades

Mi parte del proyecto abarca la **base del personaje jugable** y todos sus subsistemas, la totalidad de la **interfaz de usuario**, el sistema de **cosmética persistente**, los **actores de mundo especiales** (gaviota, medusa, tormenta), el **motor de generación procedural de chunks**, el **sistema de VOIP de proximidad** (implementación y corrección del crash crítico) y la integración de la **visualización del chat rápido** en pantalla.

### Archivos desarrollados

| Carpeta | Archivos |
|---|---|
| `Player/` | `TortugaCharacter.h/.cpp`, `TortugaFirstPersonCharacter.h/.cpp`, `MP_GamePlayerController.h/.cpp`, `TN_StaminaComponent.h/.cpp`, `TN_InventoryComponent.h/.cpp`, `TN_LegAnimComponent.h/.cpp` |
| `Voice/` | `ProximityVoiceComponent.h/.cpp` |
| `UI/HUD/` | `TN_PlayerHUDWidget.h/.cpp`, `TN_CoopFlowHUDWidget.h/.cpp`, `TN_CosmeticsMenuWidget.h/.cpp`, `TN_InteractPromptWidget.h/.cpp`, `TN_LoadingScreenWidget.h/.cpp`, `TN_RadialWheelWidgetBase.h/.cpp`, `TN_RadialWheelTypes.h`, `TN_EmoteWheelDataAsset.h/.cpp`, `TN_QuickChatWheelDataAsset.h/.cpp` |
| `UI/Menu/` | `MP_MainMenuWidget.h/.cpp` |
| `UI/Voice/` | `VoiceIndicatorWidget.h/.cpp` |
| `Multiplayer/` | `TN_CosmeticSaveGame.h/.cpp` (guardado de cosméticos) |
| `World/` | `TN_ChunkManager.h/.cpp`, `TN_SeagullActor.h/.cpp`, `TN_JellyfishActor.h/.cpp`, `TN_StormVolume.h/.cpp`, `TN_FinishLineVolume.h/.cpp`, `TN_SkinStatueActor.h/.cpp`, `TN_CosmeticsStationInteractable.h/.cpp` |

---

## 2. Personaje — `ATortugaCharacter`

<span class="file-tag">TortugaCharacter.h</span> <span class="file-tag">TortugaCharacter.cpp</span>

El personaje es la clase central del gameplay del jugador. Hereda de `ACharacter` y usa el **Enhanced Input System** de UE5 con soft references para soportar teclado/ratón y mando de forma unificada.

### 2.1 Arquitectura por Componentes

La decisión de diseño principal fue separar los sistemas en componentes independientes en lugar de sobrecargar la clase del personaje:

```
ATortugaCharacter
├── UTN_StaminaComponent        → Resistencia, sprint, penalización por peso
├── UTN_InventoryComponent      → Dos slots de items, peso efectivo
├── UTN_LegAnimComponent        → Animación procedural de piernas
└── UProximityVoiceComponent    → VOIP de proximidad (WASAPI) — desarrollado por mí
```

`TortugaCharacter` actúa únicamente como punto de ensamblaje y coordinación. Cada componente tiene su propio ciclo de vida (`BeginPlay`/`EndPlay`) y expone sus variables de forma independiente en el Editor.

### 2.2 Movimiento

| Acción | Implementación |
|---|---|
| **Andar** | Orientación al movimiento activada. `MaxWalkSpeed` base configurable |
| **Sprint** | Toggle en mando (L3), mantener en teclado (Shift). `bIsSprinting` replicado a todos los clientes para sincronizar animaciones |
| **Saltar** | Salto estándar de UE5 con `CharacterMovementComponent` |
| **Cámara** | Spring arm con lag de posición y rotación, zoom dinámico al sprintar, FOV interpolado |

### 2.3 Enhanced Input — Soft References

Todos los assets de Input (`IMC_Player`, `IA_Move`, `IA_Look`, `IA_Jump`, etc.) se guardan como **soft references** en el personaje. Si falta un asset en el Content Browser, el sistema loguea un warning y desactiva silenciosamente la acción correspondiente, sin crashear. Esto fue crucial durante el desarrollo para evitar builds rotas por assets en progreso.

### 2.4 Sistema de Emotes Replicados

El personaje soporta hasta 9 emotes (`IA_Emote1`–`IA_Emote9`) seleccionados desde la rueda radial. Al activarse, el servidor ejecuta una `NetMulticast, Reliable` que aplica el emote en todos los clientes simultáneamente.

El índice `KNOCKDOWN_EMOTE_ID = 100` es un valor reservado especial: reutiliza el sistema de emotes para propagar el visual de knockdown a todos los clientes, sin necesidad de un Multicast separado. Esta solución surgió de una colaboración con Rodrigo para evitar duplicar lógica de replicación.

### 2.5 Cosmética — Raza Condition en Seamless Travel

Al viajar entre mapas con Seamless Travel, el `PlayerState` puede llegar antes o después de que el pawn esté poseído. Para cubrir los dos casos:

```cpp
// TortugaCharacter::OnRep_PlayerState() — se llama cuando llega el PlayerState
void ATortugaCharacter::OnRep_PlayerState()
{
    Super::OnRep_PlayerState();
    // Re-aplica helmet y skin aunque ya se haya llamado antes, por si llegó tarde
    if (ATN_CoopPlayerState* PS = GetPlayerState<ATN_CoopPlayerState>())
    {
        ApplyHelmet(PS->EquippedHelmetId);
        ApplySkin(PS->EquippedSkinId);
    }
}
```

En `PostSeamlessTravel` se añade además un retry timer (5 intentos × 0.1s) para cubrir el caso inverso (pawn no poseído aún cuando llega el PlayerState).

### 2.6 Knockdown Visual

`bIsKnockedDown` replica con `OnRep_IsKnockedDown`. Al aplicar el visual de knockdown se deshabilita temporalmente el `NetworkSmoothingMode` del `CharacterMovementComponent` para evitar que sobrescriba la rotación de 180°. Se usa `SetRelativeRotation` (no world) porque CMC trabaja con rotación relativa.

```cpp
void ATortugaCharacter::ApplyKnockdownVisual(bool bKnockedDown)
{
    GetCharacterMovement()->NetworkSmoothingMode =
        bKnockedDown ? ENetworkSmoothingMode::Disabled : ENetworkSmoothingMode::Linear;
    // Componente configurable desde Blueprint (KnockdownComponentName = "Cuerpo")
    if (USceneComponent* Body = FindComponentByTag<USceneComponent>(KnockdownComponentName))
        Body->SetRelativeRotation(bKnockedDown ? KnockdownRotation : FRotator::ZeroRotator);
}
```

### 2.7 ATortugaFirstPersonCharacter

Hereda de `TortugaCharacter` con la longitud del spring arm a 0. Se usa exclusivamente para las cinemáticas de la carrera (punto de vista en primera persona). No añade lógica propia — la herencia garantiza que cualquier mejora al personaje base se refleje automáticamente en cinemáticas.

---

## 3. Stamina — `UTN_StaminaComponent`

<span class="component-tag">COMPONENTE</span> <span class="file-tag">TN_StaminaComponent.h</span>

### 3.1 Parámetros Clave

| Variable | Valor por defecto | Descripción |
|---|---|---|
| `MaxStamina` | 200 | Stamina máxima base |
| `SprintDrainRate` | 45 / s | Drenaje por segundo al sprintar |
| `RechargeDelay` | 0.8 s | Espera antes de recargar tras dejar de sprintar |
| `ExhaustionPenaltyDuration` | 1.0 s | Bloqueo de recarga al agotar la stamina |
| `StaminaPerWeightUnit` | 20 | Reducción de stamina máxima por unidad de peso del inventario |

### 3.2 Stamina Efectiva con Peso de Inventario

El sistema de peso crea una **decisión táctica real**: cargar objetos limita la capacidad de sprint. La fórmula:

```cpp
float UTN_StaminaComponent::GetEffectiveMaxStamina() const
{
    float WeightPenalty = InventoryWeight * StaminaPerWeightUnit; // 20 por unidad
    return FMath::Max(0.f, MaxStamina - WeightPenalty);
}
```

Un jugador sin objetos puede sprintar a plena velocidad. Con dos objetos pesados, su stamina máxima se reduce hasta el punto de no poder mantener el sprint durante mucho tiempo. Esto obliga a que en el grupo haya roles: los que cargan y los que corren de apoyo.

### 3.3 Replicación

- `CurrentStamina`: replica **solo al propietario** (`COND_OwnerOnly`) para no saturar el ancho de banda con datos que solo importan al propio jugador.
- `bIsSprinting`: replica a **todos los clientes** para sincronizar animaciones.

---

## 4. Inventario — `UTN_InventoryComponent`

<span class="component-tag">COMPONENTE</span> <span class="file-tag">TN_InventoryComponent.h</span>

Sistema de **dos slots**: equipado (en mano) y almacenado (en espalda/mochila). El peso total de los items activos se comunica al `TN_StaminaComponent` para actualizar la stamina efectiva.

```cpp
// TortugaCharacter::BeginPlay() — conexión obligatoria entre componentes
void ATortugaCharacter::BeginPlay()
{
    Super::BeginPlay();
    StaminaComponent->SetInventoryComponent(InventoryComponent);
}
```

El jugador puede rotar entre el slot equipado y el almacenado (`IA_RotateInventory`) y soltar el item equipado al mundo (`IA_DropItem`).

---

## 5. Animación de Piernas — `UTN_LegAnimComponent`

<span class="component-tag">COMPONENTE</span> <span class="file-tag">TN_LegAnimComponent.h</span>

Animación que adapta los pies del personaje a la geometría del terreno mediante raycasts(actualmente sin implementar, solo animacion por codigo), evitando que los pies floten o se hundan en superficies irregulares. Especialmente relevante en los chunks procedurales, cuya geometría varía continuamente.

---

## 6. Rueda Radial de Emotes y Chat

<span class="file-tag">TN_RadialWheelWidgetBase.h</span> <span class="file-tag">TN_EmoteWheelDataAsset.h</span> <span class="file-tag">TN_QuickChatWheelDataAsset.h</span>

### 6.1 Arquitectura

El sistema de rueda radial usa una clase base `TN_RadialWheelWidgetBase` que implementa la lógica de detección angular del joystick/ratón y la selección del segmento. Las dos ruedas (emotes y chat rápido) son Widget Blueprints que extienden la misma clase base.

```
TN_RadialWheelWidgetBase  ←  lógica angular + input
├── BP_EmoteWheel         →  usa TN_EmoteWheelDataAsset
└── BP_QuickChatWheel     →  usa TN_QuickChatWheelDataAsset
```

### 6.2 Data Assets para el Contenido

Los emotes y mensajes de chat no están hardcodeados en C++. Viven en `UDataAsset`:

- `TN_EmoteWheelDataAsset`: array de entradas (icono, nombre, emote ID) editables desde el Editor.
- `TN_QuickChatWheelDataAsset`: array de mensajes de chat rápido localizables.

Esto permite al equipo de diseño añadir o reorganizar emotes y mensajes sin tocar código ni recompilar.

### 6.3 Replicación de Emotes

Al seleccionar un emote, el personaje ejecuta un `Server RPC` que valida y propaga a todos los clientes con `NetMulticast, Reliable`.

### 6.4 Chat Rápido — Integración de Pantalla y Correcciones

El backend del chat rápido (`ATN_CoopGameState::ServerSendQuickChat_Implementation`) fue implementado por Rodrigo: el servidor valida el mensaje y lo replica vía `NetMulticast`. Mi contribución fue **la integración completa en pantalla** y las correcciones de eficiencia:

- Integré la recepción del evento en `TN_CoopFlowHUDWidget`, suscribiéndome al delegate de `TN_CoopGameState` en lugar de polling por tick.
- Añadí la visualización de mensajes con cola, fade-out y límite de mensajes visibles simultáneos para evitar saturar la pantalla.
- Corregí el problema original donde los mensajes no se mostraban en el listen-server (mismo bug de OnRep que el MatchFlowState: el servidor no recibía la notificación de su propia variable) — la solución fue añadir `BroadcastQuickChat()` tras cada `ServerSendQuickChat`, siguiendo el mismo patrón que `BroadcastFlowStateChange`.

---

## 7. Interfaz de Usuario (UI/HUD)

Toda la UI está implementada con **UMG** usando clases C++ base que se extienden con Widget Blueprints en el Editor.

### 7.1 TN_PlayerHUDWidget

<span class="file-tag">TN_PlayerHUDWidget.h</span>

HUD principal del jugador durante la carrera. Muestra:
- Barra de stamina (replicada `COND_OwnerOnly`).
- Estado del inventario (slots equipado y almacenado).
- Indicador de knockdown propio.

### 7.2 TN_CoopFlowHUDWidget

<span class="file-tag">TN_CoopFlowHUDWidget.h</span>

Widget que refleja el flujo de partida (countdown, clasificación de llegada a meta, tiempo transcurrido). Escucha el delegate `OnMatchFlowStateChanged` de `TN_CoopGameState` para actualizarse automáticamente sin polling.

### 7.3 TN_InteractPromptWidget

<span class="file-tag">TN_InteractPromptWidget.h</span>

Prompt contextual que aparece en pantalla cuando el jugador está cerca de un interactuable. Muestra el nombre del objeto y la tecla/botón de interacción. Soporta prompts diferenciados según el tipo de interacción (recoger, usar, rescatar).

### 7.4 TN_LoadingScreenWidget

<span class="file-tag">TN_LoadingScreenWidget.h</span>

Pantalla de carga que se activa en el cliente durante el `ServerTravel`. Gestionada desde `MP_GameInstance` para garantizar que aparezca antes del freeze de carga y desaparezca después de `PostSeamlessTravel`.

### 7.5 Convención BindWidget

Todos los widgets C++ usan `UPROPERTY(meta = (BindWidget))` para los elementos requeridos y `BindWidgetOptional` para los opcionales. El nombre de la propiedad en C++ **debe coincidir exactamente** con el nombre del elemento en el Widget Blueprint Designer.

---

## 8. Cosmética Persistente

<span class="file-tag">TN_CosmeticsMenuWidget.h</span> <span class="file-tag">TN_CosmeticSaveGame.h</span>

### 8.1 Flujo Completo

```
TN_CosmeticsMenuWidget  →  selección de helmet/skin
    ↓  ServerRPC
TN_CoopPlayerState.EquippedHelmetId / EquippedSkinId  (replicados)
    ↓  OnRep_EquippedHelmetId / OnRep_EquippedSkinId
ATortugaCharacter::ApplyHelmet / ApplySkin
    ↓  guardado
TN_CosmeticSaveGame  →  USaveGame persistido en disco
    ↓  carga al iniciar
MP_GameInstance::LoadCosmeticsForLocalPlayer
```

### 8.2 TN_CosmeticsStationInteractable y TN_SkinStatueActor

Actores del mundo HQ que permiten al jugador acceder al menú de cosméticos. `TN_CosmeticsStationInteractable` hereda de `TN_DirectInteractableBase` y abre el `TN_CosmeticsMenuWidget` al interactuar. `TN_SkinStatueActor` es una estatua decorativa que previsualizaza los skins disponibles.

---

## 9. Actores de Mundo

### 9.1 TN_SeagullActor — Obstáculo Gaviota

<span class="file-tag">TN_SeagullActor.h</span>

Gaviota que patrulla un radio configurable (`PatrolRadius`, `PatrolHeight`) en torno a su posición de spawn. Al entrar en contacto con el jugador aplica knockdown.

**Patrón crítico de chunk**: es un Child Actor dentro de los chunk Blueprints. Usa `SetTimerForNextTick` para diferir la lectura de su posición un tick, garantizando que el chunk ya está en su posición final cuando ejecuta `BeginPlay`. Sin esta espera, la gaviota calcula su radio de patrulla desde `(0,0,0)`.

```cpp
void ATN_SeagullActor::BeginPlay()
{
    Super::BeginPlay();
    // Defer: el chunk aún no ha llegado a su posición final en este tick
    FTimerDelegate Del;
    Del.CreateUObject(this, &ATN_SeagullActor::InitPatrolAfterSpawn);
    GetWorldTimerManager().SetTimerForNextTick(Del);
}
```

El timer usa `FTimerDelegate::CreateUObject` (no lambda) para que `EndPlay → ClearAllTimersForObject` lo cancele correctamente si el chunk temporal es destruido.

### 9.2 TN_JellyfishActor — Obstáculo Medusa

<span class="file-tag">TN_JellyfishActor.h</span>

Medusa con VFX Niagara y SFX espacial. Al solapar con el jugador aplica knockdown con `MulticastApplyKnockdownVisual`. Como la gaviota, es Child Actor en los chunks y aplica el mismo patrón de `SetTimerForNextTick`.

### 9.3 TN_StormVolume

<span class="file-tag">TN_StormVolume.h</span>

Volumen de tormenta que modifica el estado visual y sonoro de la zona afectada. Activa efectos de viento y lluvia en los clientes mediante `NetMulticast`. Heredado de `AActor + UBoxComponent` (no `ATriggerVolume`) para poder ser Child Actor en chunks.

### 9.4 TN_FinishLineVolume

<span class="file-tag">TN_FinishLineVolume.h</span>

Volumen de la línea de meta. Al solapar, notifica al `TN_RunGameMode` que el jugador ha finalizado la carrera, asignándole su `FinishRank`. Contiene la corrección de un bug crítico donde la notificación se disparaba múltiples veces por el overlap repetido del CMC.

---

## 10. Sistema de Chunks — `ATN_ChunkManager`

<span class="file-tag">TN_ChunkManager.h</span> <span class="file-tag">TN_ChunkManager.cpp</span>

Motor de generación procedural de niveles. Es uno de los sistemas técnicamente más complejos del proyecto.

### 10.1 Arquitectura General

El `ATN_ChunkManager` mantiene una **ventana deslizante de 3 chunks activos**. Cuando un jugador activa el `EndTrigger` del chunk actual:

1. Destruye el chunk más antiguo.
2. Lee el transform de salida (`OutSocket`) del chunk actual.
3. Spawna el siguiente chunk en su posición final mediante el **patrón de dos fases**.
4. Incrementa el contador de chunks para escalar la dificultad: `Easy → Medium → Hard`.

### 10.2 Patrón de Spawn en Posición Final (Dos Fases)

El problema: los Child Actors dentro de un chunk Blueprint ejecutan `BeginPlay` en el momento del spawn. Si el chunk se spawna en `Identity` y luego se teleporta, los Child Actors ya leyeron `(0,0,0)` como su posición.

La solución, en dos fases:

```cpp
// Fase 1: actor temporal solo para leer el socket de entrada
AActor* TempActor = World->SpawnActor<AActor>(ChunkClass, FTransform::Identity);
FTransform InSocket = GetSocketTransform(TempActor, TEXT("InSocket"));
TempActor->Destroy();

// Fase 2: spawn real directamente en la posición final calculada
FTransform FinalTransform = InSocket.Inverse() * TargetTransform;
AActor* NewChunk = World->SpawnActor<AActor>(ChunkClass, FinalTransform);
// Ahora los Child Actors ejecutan BeginPlay en su posición definitiva
```

Este patrón garantiza que gaviotas, medusas, botones e item spawners embebidos en el chunk lean su posición correcta desde el primer tick.

### 10.3 Tres Pools de Dificultad

| Pool | Descripción |
|---|---|
| `Easy` | Chunks de inicio: plataformas amplias, pocos obstáculos |
| `Medium` | Dificultad media: zonas lentas, gaviotas |
| `Hard` | Máxima dificultad: medusas, botones trampa, layout complejo |

Los umbrales de transición entre pools (`EasyToMediumThreshold`, `MediumToHardThreshold`) y el número total de chunks antes del final (`TotalChunksBeforeFinal`) son variables `EditDefaultsOnly`.

### 10.4 Modo de Secuencia Personalizada

Con `bUseRandomGeneration = false`, el diseñador define explícitamente la dificultad de cada posición mediante `CustomChunkSequence[]`. Esto permite diseñar carreras con curva de dificultad controlada, útil para testing y para las builds de demostración.

---

## 11. VOIP de Proximidad — `UProximityVoiceComponent`

<span class="component-tag">COMPONENTE</span> <span class="file-tag">ProximityVoiceComponent.h</span>

Sistema de voz de proximidad implementado con **WASAPI** (`FAudioCaptureSynth`). El VOIP nativo de UE está desactivado en `DefaultEngine.ini` (`[Voice] bEnabled=false`) para evitar duplicación de audio.

### 11.1 Atenuación por Distancia

El volumen de la voz de otros jugadores se calcula en función de la distancia, con parámetros configurables desde el Editor:

| Variable | Descripción |
|---|---|
| `ProximityMaxDistance` | Distancia máxima a la que se escucha la voz |
| `ProximityFalloffExponent` | Curva de atenuación (mayor = caída más brusca) |

### 11.2 El Crash Crítico — `ACCESS_VIOLATION` en WASAPI

El bug más grave del proyecto de VOIP: llamar `ShutdownAllCapture` directamente desde un `GameMode` con WASAPI en estado indeterminado causaba `ACCESS_VIOLATION` y cerraba el juego.

**Diagnóstico**: `ShutdownAllCapture` es seguro solo si `FAudioCaptureSynth` está completamente inicializado y activo. Cuando el `GameMode` recibe la señal de `ServerTravel`, el estado de WASAPI puede ser cualquiera.

**Solución**: destruir los pawns explícitamente *antes* de `ServerTravel`. Esto dispara `EndPlay(Destroyed)` en el componente mientras WASAPI sigue activo y en estado válido:

```cpp
// TN_HQGameMode::BeginMatchTravel() — orden crítico
for (AController* PC : GetAllPlayerControllers())
{
    if (APawn* Pawn = PC->GetPawn())
        Pawn->Destroy(); // → EndPlay(Destroyed) → ProximityVoiceComponent::CleanupRuntimeResources()
                         // → ShutdownAllCapture() mientras WASAPI está activo ← SEGURO
}
// Solo entonces, viajar
ProcessServerTravel(TEXT("/Game/Maps/HQ/LVL_HQ?listen"));
```

Sin este orden, `ServerTravel` destruye los actores internamente cuando el NetDriver ya está en transición → estado indeterminado de WASAPI → crash.

---

## 12. Decisiones de Diseño

### ¿Por qué Data Assets para las ruedas radiales?

`TN_EmoteWheelDataAsset` y `TN_QuickChatWheelDataAsset` son `UDataAsset`, no arrays hardcodeados en C++. El equipo de diseño puede añadir, reordenar o cambiar emotes y mensajes sin tocar código. Es el patrón correcto para contenido que se itera frecuentemente sin lógica de gameplay asociada.

### ¿Por qué stamina con penalización por peso?

Sin el sistema de peso, el inventario siempre es la opción obvia: recoger un objeto nunca tiene coste. Con el peso, cargar objetos limita el sprint y crea una decisión táctica real en un juego cooperativo: los jugadores con más stamina se encargan de cargar; los demás sirven de apoyo rápido.

### ¿Por qué `UBoxComponent` y no `ATriggerVolume` en chunks?

`ATriggerVolume` hereda de `ABrush` (geometría BSP de Unreal), que tiene restricciones internas que impiden colocarlo como Child Actor dentro de un Blueprint Actor. Como todos los obstáculos de los chunks son Child Actors, usan `AActor + UBoxComponent`, que no tiene esas limitaciones y funciona igual de bien con replicación de red.

### ¿Por qué el patrón de dos fases en el spawn de chunks?

El patrón alternativo (spawn en Identity, luego `SetActorTransform`) hace que los Child Actors ejecuten `BeginPlay` en `(0,0,0)`. Cualquier lógica que lea posición en `BeginPlay` (como el radio de patrulla de la gaviota) queda irrecuperablemente corrompida. El coste de spawnar un actor temporal es despreciable frente a la fiabilidad garantizada.

### ¿Por qué `FTimerDelegate::CreateUObject` en lugar de lambdas?

Con lambdas, si el chunk temporal es destruido antes de que expire el timer, el callback referencia un puntero inválido → crash. Con `CreateUObject`, `ClearAllTimersForObject` en `EndPlay` cancela automáticamente todos los timers asociados al actor destruido.

---

## 13. Variables Serializables (Principales)

Todas las variables relevantes están expuestas como `UPROPERTY(EditDefaultsOnly)` o `UPROPERTY(EditAnywhere)` para ajuste desde el Editor sin recompilar:

| Sistema | Variables clave |
|---|---|
| **Stamina** | `MaxStamina`, `SprintDrainRate`, `RechargeDelay`, `ExhaustionPenaltyDuration`, `StaminaPerWeightUnit` |
| **Inventario** | `MaxSlots`, `DropImpulseStrength` |
| **Cámara** | `TargetArmLength`, `CameraLagSpeed`, `CameraRotationLagSpeed`, `SprintFOVOffset`, `SprintZoomDistance` |
| **Knockdown** | `KnockdownComponentName`, `KnockdownRotation` |
| **Chunk Manager** | `EasyToMediumThreshold`, `MediumToHardThreshold`, `TotalChunksBeforeFinal`, `bUseRandomGeneration`, `CustomChunkSequence[]` |
| **Seagull** | `PatrolRadius`, `PatrolHeight`, `PatrolSpeed`, `KnockdownForce` |
| **Jellyfish** | `KnockdownImpulse`, `VFXScale`, `SFXRange` |
| **Cosmética** | `HelmetCrateEntries[]` (id + peso de probabilidad, en `MP_GameInstance`) |
| **VOIP** | `ProximityMaxDistance`, `ProximityFalloffExponent` |

---

## 13. Conclusión

Mi trabajo en **Tortunabo** cubre la experiencia completa del jugador individual: desde el personaje que maneja, los sistemas que gobiernan sus recursos (stamina e inventario), la UI que le informa del estado de la partida (incluyendo la integración del chat rápido en pantalla), la cosmética con la que se identifica, el VOIP de proximidad que añade inmersión social (con la corrección del crash crítico de WASAPI), y los obstáculos y el nivel procedural que hacen cada carrera única. La arquitectura modular por componentes y el uso de Data Assets para contenido configurable permiten que el equipo de diseño itere sobre el gameplay sin pasar por compilación.
