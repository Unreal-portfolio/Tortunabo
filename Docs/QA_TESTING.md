# QA Testing — Tortunabo

Registro de bugs, mejoras y decisiones detectadas durante testing en PIE.

---

## ✍️ Zona de volcado

> Pega aquí texto libre. En la siguiente sesión lo formateo, clasifico por severidad y lo muevo a la sección correspondiente.

```
<!-- DROP_ZONE: pegar notas crudas debajo de esta línea -->

<!-- FIN_DROP_ZONE -->
```

---

## Leyenda

| Severidad | Significado |
|---|---|
| 🔴 Alta | Bug bloqueante o pérdida de game feel grave |
| 🟠 Media | Bug perceptible pero no bloqueante |
| 🟡 Baja | Mejora de pulido, UX o polish visual |
| 🔵 Decisión | Cambio de diseño validado — no es bug |

| Estado | Significado |
|---|---|
| ⬜ Abierto | Sin tocar |
| 🟦 En progreso | Alguien trabajándolo |
| ✅ Resuelto | Cerrado (incluir commit al cerrar) |

---

## Cross-cutting — Player & Movement

### CC-02 🔴 Cámara — clip con geometría
**Componente:** `ATortugaCharacter` (SpringArmComponent)
**Estado:** ✅ Resuelto en C++ (pendiente verificar override en BP)
**Origen:** Drop-zone 2026-04-24

**Observación:**
La cámara no debería clipear con ninguna geometría.

**Análisis:**
`CameraBoom->bDoCollisionTest=true`, `ProbeSize=14.f`, `ProbeChannel=ECC_Camera` ya están seteados en el constructor de `ATortugaCharacter` (líneas 81-83 del `.cpp`). Si aún clipea en PIE, el motivo es que **el BP_TortugaCharacter tiene Class Defaults que sobrescriben estos valores**.

**Acción (editor):**
- Abrir `BP_TortugaCharacter` → seleccionar `CameraBoom` → Details → verificar que `Do Collision Test = true` y `Probe Size = 14`.
- Si el BP los sobreescribe: dejar en blanco para que hereden el C++ default.

---

### CC-01 🟠 Second jump landing — clip y "pegado al suelo" al levantarse
**Componente:** `ATortugaCharacter` (CharacterMovementComponent / aterrizaje)
**Estado:** ⬜ Abierto

**Observación:**
A veces, tras caer con el **segundo salto**, se produce un pequeño **clip** con la geometría y al levantarse el jugador queda **pegado al suelo** (como si no pudiera moverse libremente un instante).

**Causas posibles a revisar:**
- Velocidad vertical en el momento del touchdown del segundo salto demasiado alta → penetración → depenetration con `MOVE_Walking` frena y pega al suelo.
- `MaxDepenetrationWithGeometry` muy bajo en el CMC.
- Falta de `FindFloor` inmediato tras el landing del 2º salto (flag `bForceNextFloorCheck`).
- Posible conflicto con el clamp de velocidad airborne (ver `project_ue5_patterns.md`).

**Acción:**
- Repro: saltar dos veces en distintas superficies y grabar los casos donde ocurre.
- Inspeccionar `ATortugaCharacter::Landed` + cualquier override de `OnLanded`.
- Si es clip: subir `MaxDepenetrationWithGeometry` o aplicar `AddImpulse` muy suave hacia arriba en el frame del landing para desempaquetar.

---

## Fase 1 — Hazards pasivos

### Q1-01 🔴 Knockdown — falta rotación simulada del cuerpo
**Componente:** `TortugaCharacter::ApplyKnockdownVisual` (cross-cutting, afecta a todas las fuentes de knockdown)
**Estado:** ✅ Resuelto — commit `beb9b53`

**Observación (original):**
El player agitaba los brazos (emote) pero la rotación simulada del cuerpo (pitch -180°) no se aplicaba. Se veía "flotando" en vez de tumbado.

**Causa raíz:**
`MulticastApplyKnockdownVisual` era un **no-op** tras la migración al sistema de emotes — `ApplyKnockdownVisual` existía pero no se invocaba desde ningún sitio.

**Fix:**
- `MulticastApplyKnockdownVisual` ahora delega en `ApplyKnockdownVisual(bKnocked)`.
- `ApplyKnockdown` invoca el multicast con `true`; `RecoverFromKnockdown` con `false`.
- Se ejecuta en listen-server + todos los clientes (incluido el dueño, que se salta `OnRep_ReplicatedEmoteIndex` por `COND_SkipOwner`).

---

### Q1-02 🟠 BreakablePlatform — respawn no observado
**Componente:** `ATN_BreakablePlatform`
**Estado:** ✅ Resuelto — commit `c3a0a12`

**Diagnóstico:**
La lógica de respawn ya funcionaba correctamente. Añadimos `UE_LOG` en `BreakPlatform` y `RespawnPlatform` para confirmar que el timer se agenda y dispara, y un `Warning` explícito si `RespawnTime=0` (posible error de configuración del BP).

---

### Q1-03 🟡 BreakablePlatform — falta feedback visual previo a romperse
**Componente:** `ATN_BreakablePlatform`
**Estado:** ✅ Resuelto — commit `65446f6`

**Implementación:**
Oscilación vertical visible + audio/VFX durante `ShakeDuration` segundos previos al break. Nuevos `UPROPERTY`: `ShakeDuration` (1s), `ShakeAmplitude` (3cm), `ShakeFrequencyHz` (25). `Multicast{Shake,StopShake}` (`Reliable`) porque es señal gameplay-crítica. `SetReplicateMovement(false)` para evitar jitter — la oscilación se calcula localmente en cada máquina.

---

### Q1-04 🔵 Quicksand — rediseño: slow + DeathZone + suelo sólido
**Componente:** `ATN_QuicksandVolume`
**Estado:** ✅ Resuelto — commit `9884bbd`

**Decisión aplicada:**
Eliminada la lógica de hundimiento progresivo (`SinkTime`, `SinkForce`, `ServerSandTick`, `RequestKill` por acumulación). El volumen ahora es solo slow + gravedad alta (`GravityScaleOverride=2.5`) + salto casi nulo (`JumpZVelocityOverride=100`). La muerte se delega al patrón compuesto del nivel:
1. `TN_QuicksandVolume` (este actor)
2. `TN_DeathZoneVolume` a ras de suelo
3. Static mesh sólido debajo

**Acción pendiente (nivel/BP):**
- Actualizar BPs de quicksand en el nivel para añadir el `DeathZoneVolume` compañero.

---

### Q1-05 🟠 PhysicsObject — atraviesa muros bajo presión alta
**Componente:** `ATN_PhysicsObjectActor`
**Estado:** ✅ Resuelto (fase 1) — commit `dc21d12`

**Fix aplicado:**
`Mesh->SetUseCCD(true)` en el ctor. Continuous Collision Detection evita el tunneling clásico cuando el jugador empuja el objeto contra un muro estático a alta velocidad simulada.

**Plan B si persiste:**
Detección de crush en Tick (overlap simultáneo jugador ↔ objeto ↔ muro) + `MulticastPoofAndDestroy()`. Solo implementar si tras testing se sigue observando tunneling bajo presión sostenida.

---

### Q1-06 🟡 PhysicsObject — masa 200000 se mueve con facilidad (informativo)
**Componente:** `ATN_PhysicsObjectActor`
**Estado:** 🟦 No acción — nota informativa

**Observación:**
Con `Mass=200000` el jugador aún puede empujarlo con facilidad. No es bug (el pawn usa Launch/impulse, no simulación masa a masa), pero **documentado por si en balance se quiere un objeto "inamovible"**.

**Si se quisiera arreglar:**
- `bLockXAxis=true` sobre eje ancho.
- `SetMassOverrideInKg` muy alto + `MaxAngularVelocity=0`.
- O cambiar a `ActorComponent` estático con físicas solo activables por trigger.

---

### Q1-07 🔴 Knockdown — rediseño necesario (sigue vertical con errores)
**Componente:** `ATortugaCharacter` (sistema de knockdown completo)
**Estado:** ✅ Resuelto en C++ (pendiente asignar PhysicsAsset al SkelMesh del BP)
**Origen:** Testing 2026-04-18 — followup de Q1-01

**Observación:**
Tras el fix de Q1-01 (`beb9b53`) el tilt 180° ya se aplica, pero el jugador **sigue viéndose vertical y con errores visuales** al recibir knockdown.

**Solución aplicada (opción 2 del QA: ragdoll físico breve):**

Nueva ruta A en `ApplyKnockdownVisual`: si `bUsePhysicsRagdoll == true` (default) y el SkelMesh tiene `PhysicsAsset` asignado, activa ragdoll físico completo:

1. Snapshot de `RelativeTransform` + `CollisionProfileName` del SkelMesh
2. `SetCollisionProfileName(RagdollCollisionProfile)` (default `"Ragdoll"`)
3. `SetSimulatePhysics(true)` + `WakeAllRigidBodies()`
4. Desactiva `NetworkSmoothingMode` en el CMC para evitar que la corrección de red "tire" del mesh ragdolleado

Al `RecoverFromKnockdown`:
- `SetSimulatePhysics(false)` → restaura colisión snapshot
- `AttachToComponent(Capsule)` con `KeepRelativeTransform` (SetSimulatePhysics(true) había detachado el mesh)
- Restaura el `RelativeTransform` snapshot → pose por defecto

Si el SkelMesh **no tiene** PhysicsAsset, cae silenciosamente a la Ruta B (tilt manual pre-existente) — no rompe nada.

**Queda pendiente (BP/editor):**
- Asignar un `PhysicsAsset` al `USkeletalMeshComponent` del `BP_TortugaCharacter` (Mesh component).
- Ajustar los bodies del PhysicsAsset para que el ragdoll caiga de forma natural ("panza arriba" es cuestión de densidades de masa + orientación inicial).
- Testear en PIE: si el ragdoll no luce bien, se puede destildar `bUsePhysicsRagdoll` desde el BP para volver al tilt manual sin recompilar.

---

### Q1-08 🟠 BreakablePlatform — bridge no se elimina
**Componente:** `ATN_BreakablePlatform` (uso cooperativo / puente)
**Estado:** ✅ Resuelto en C++ (pendiente validar BP en PIE)
**Origen:** Testing 2026-04-18

**Observación:**
El bridge (BreakablePlatform con `PlayerThreshold=2+`) **no se rompe** durante el test, incluso tras configurar la variable de respawn y sus subparámetros.

**Causas posibles:**
- Testeando con 1 solo player y `PlayerThreshold=2` → el timer nunca arranca (comportamiento correcto, pero confuso en testing).
- El StandTrigger del BP del bridge no está bien dimensionado y los pawns no generan overlap.
- Tras respawn, pawns que no salieron del trigger ya no generan `BeginOverlap` → plataforma "invencible" para ese player.

**Solución aplicada:**
1. Logs permanentes (`Verbose` en entries/exits, `Log` al cruzar threshold) para diagnosticar en runtime sin recompilar. Usar `LogTemp Verbose` en `Saved/Config/Windows/Engine.ini` o via `Log LogTemp Verbose` en consola.
2. `RespawnPlatform` ahora re-detecta pawns que permanezcan sobre el trigger al respawnear (via `GetOverlappingActors`) y re-arranca el ciclo shake+break si se cumple el threshold. Antes, un jugador que no salió del trigger quedaba "huérfano" y el puente no volvía a romperse para él.

**Queda por validar (BP/editor):**
- Que el `StandTrigger` en el BP hijo cubra toda la superficie del mesh del bridge.
- Que `PlayerThreshold` esté configurado correctamente (2 para puente cooperativo).
- Testing en PIE con 2 players simultáneos.

---

### Q1-09 🔵 Quicksand — deprecar, reemplazar por SlowZone "Sticky"
**Componente:** `ATN_QuicksandVolume` → `ATN_SlowZoneVolume` (variante sticky)
**Estado:** ✅ Resuelto en C++ (pendiente re-parentar BP en editor)
**Origen:** Decisión 2026-04-18 (invalida Q1-04)

**Solución aplicada:**

1. **`ATN_SlowZoneVolume` ahora expone `GravityScaleInZone` (default 0.35)** como `UPROPERTY`. Antes estaba hardcoded en 0.35f. Un BP hijo puede setear 2.5+ para conseguir el comportamiento sticky/quicksand original — gravedad alta pega al jugador al suelo.
2. **Eliminadas `TN_QuicksandVolume.h` y `TN_QuicksandVolume.cpp`** del módulo. Toda su funcionalidad es ahora un caso particular de `TN_SlowZoneVolume` configurando los defaults adecuados.

Los parámetros "sticky" recomendados para un BP hijo `BP_SlowZoneSticky`:
- `MaxSlowSpeed = 150` (antes `SlowSpeed` en Quicksand)
- `JumpVelocityInZone = 100` (antes `JumpZVelocityOverride`)
- `GravityScaleInZone = 2.5` (antes `GravityScaleOverride`)
- `MaxUpwardVelocity = 120` / `MaxFallVelocity = 1200` (alta velocidad de caída = sticky, no sirope)

**Queda pendiente (BP/editor):**
- Re-parentar `BP_QuicksandVolume` → `ATN_SlowZoneVolume` (o borrarlo y crear `BP_SlowZoneSticky` desde cero).
- Ajustar los defaults del BP sticky con los valores listados arriba.
- Sustituir instancias de `BP_QuicksandVolume` en los niveles por el nuevo BP.
- La muerte, si se quiere, sigue el mismo patrón compuesto (DeathZone a ras de suelo debajo).

---

### Q1-10 🟠 PhysicsObject — atasco en esquinas, bloquear rotación Z
**Componente:** `ATN_PhysicsObjectActor`
**Estado:** ✅ Resuelto (pendiente validar en PIE)
**Origen:** Testing 2026-04-18

**Observación:**
Al forzar al objeto contra ciertas esquinas geometría+geometría, entra en un estado inestable (oscila, vibra o queda trabado). Propuesta del usuario: **bloquear rotación en el eje vertical** para evitar que el cubo rote sobre sí mismo en Z y caiga en configuraciones imposibles.

**Solución aplicada:**
Tres `UPROPERTY EditDefaultsOnly` en la nueva categoría `Physics|Constraints` — `bLockRotationX` / `bLockRotationY` / `bLockRotationZ`, todas default `true`. En `BeginPlay` se aplican al `Mesh->BodyInstance` y se llama a `CreateDOFLock()` para forzar la reconstrucción del constraint DOF.

Con el default a `true/true/true` el cubo se comporta como caja slide-only (ni vuelca ni spinea). Cada BP hijo puede destildar los ejes que necesite desde el editor sin tocar C++.

---

### Q1-11 🔵 Bridge — vibración escalada por nº de jugadores
**Componente:** `ATN_BreakablePlatform` (variante bridge cooperativo)
**Estado:** ✅ Resuelto — commit `1e37de8`
**Origen:** Drop-zone 2026-04-19

**Fix aplicado:**
Nuevos `UPROPERTY` `bScaleShakeByPlayerCount` (default **false** — compat) + `TArray<float> ShakeAmplitudeByPlayerCount` (default `[3, 6, 10, 15]`). `MulticastShake(int32 PlayerCount)` ahora lleva el count del servidor; `FireShake()` server-only muestrea `PawnsOnPlatform.Num()` al dispararse. `EffectiveShakeAmplitude` se resuelve por clamp del índice y se usa en Tick en lugar del antiguo `ShakeAmplitude` fijo. Timers rebind a `FireShake` para preservar el count al disparar.

**Observación:**
El bridge actual (con `PlayerThreshold`) decide romperse de forma binaria: umbral cumplido → shake + break. Rodrigo quiere una curva: la intensidad de la vibración debe **escalar con el número de jugadores encima**, no ser un estado on/off.

**Diseño propuesto:**
- `UPROPERTY int32 NumPlayersOnBridge` (replicada, serializable para debug/observabilidad).
- Mapeo `NumPlayers → ShakeAmplitude / ShakeFrequencyHz`:
  - 1 jugador → amplitud baja (tambaleo), no rompe.
  - 2+ jugadores → amplitud intermedia, sigue sin romper.
  - `NumPlayers >= BreakThreshold` → amplitud máxima + timer de break.
- El shake actual ya existe (Q1-03) — reutilizar, añadir modulación por cantidad.
- `UPROPERTY` para los breakpoints (amplitud por nivel de carga) y para el threshold de rotura, todo editable desde BP.

**Acción:**
- Implementar en `ATN_BreakablePlatform` preservando compatibilidad con el modo "plataforma rompible simple" (un solo nivel de shake).
- Testear en PIE 2P: verificar que con 1 jugador vibra pero no rompe, con 2 vibra más fuerte, con threshold+ rompe.

---

## Fase 2 — Interactables

### Q2-01 🟠 Trampa — al pillar, vuelve a pickup
**Componente:** `ATN_ConchPickup` (concha marina, único ítem-trampa del juego)
**Estado:** ✅ Resuelto
**Origen:** Testing 2026-04-18

**Observación:**
Cuando la trampa desplegada pilla a un jugador, en vez de destruirse o quedarse consumida, **vuelve a convertirse en un pickup** recolectable. Efectivamente da usos infinitos a la trampa y rompe el balance del item.

**Causa raíz:**
`ATN_ConchPickup::RestoreMovement` re-armaba incondicionalmente la trampa (via `RearmTrap`) tras el timer de inmovilización. La concha persistía en el mapa indefinidamente y el sistema externo de pickup podía re-interpretarla como recolectable.

**Solución aplicada:**
Nuevo `UPROPERTY bDestroyAfterActivation` (default **true**). En modo one-shot (default) la concha llama a `SetLifeSpan(0.2f)` tras la primera activación → se destruye limpiamente, el multicast de VFX se entrega, y nunca más puede ser tratada como pickup. En modo persistente (si el BP lo destilda) mantiene el comportamiento anterior con `ResetCooldownSeconds`.

El `EditCondition = "!bDestroyAfterActivation"` oculta `ResetCooldownSeconds` del editor cuando es irrelevante, evitando confusión del diseñador.

---

### Q2-02 🟠 ItemSpawnZone — ubicación final debe ser offset relativo al spawn + escalado del Quad
**Componente:** `ATN_QuadSpawner` (el ticket original apuntaba a `ItemSpawnZone`, pero el bug real estaba en el spawner del quad)
**Estado:** ✅ Resuelto — commit `d17e1be`
**Origen:** Drop-zone 2026-04-19

**Fix aplicado:**
`UPROPERTY EndLocation` renombrado a `EndOffsetLocal` con docstring explícito "offset local desde el spawner, en cm". `SpawnQuad` ahora calcula `WorldEnd = GetActorTransform().TransformPosition(EndOffsetLocal)` antes de llamar a `InitializeTravel(WorldEnd, QuadSpeed)`. Con esto el quad viaja relativo al spawner, permitiendo mover/reparentar el spawner sin que el destino del quad quede desalineado.

**Nota al diseñador:** tras este cambio los BPs hijos pierden el valor serializado (UPROPERTY renombrado). Re-asignar `EndOffsetLocal` desde el editor. El "escalado del Quad" es asset-work en el BP (`QuadMesh->RelativeScale3D`); no requiere C++.

**Observación:**
El "quad spawner" (ItemSpawnZone) actualmente coloca el ítem en una ubicación absoluta. Debería calcularse como **offset relativo al spawn** para que al mover/reparentar el spawner el ítem viaje con él. Además hay problemas con el **escalado del Quad** (mesh de visualización): al escalarlo en el editor la zona de spawn y/o la visualización no coinciden.

**Causas posibles a revisar:**
- Cálculo de la posición final del ítem usa world transform directa en vez de `GetActorTransform().TransformPosition(LocalOffset)`.
- El Quad visual usa `SetRelativeScale3D` pero la lógica de spawn lee `GetActorScale()` o un tamaño hardcoded.
- Posible desincronización entre el `UBoxComponent` de spawn y la geometría del Quad visual.

**Acción:**
- Revisar `ATN_ItemSpawnZone::SpawnItem` (o equivalente) y pasar offsets por local space.
- Uniformar la fuente de verdad del tamaño de zona: o `UBoxComponent`, o `RelativeScale3D`, no ambas.
- Añadir `UPROPERTY FVector SpawnOffsetLocal` si falta, para que el diseñador pueda ajustar sin recompilar.
- Recordar: `ItemSpawnZone` es chunk-compatible — usar `SetTimerForNextTick` si se lee world position en BeginPlay (ver `CLAUDE.md`).

---

### Q2-03 🔵 PressurePlate — dos modos de activación (momentáneo / latch)
**Componente:** `ATN_PressurePlate` (placa de presión)
**Estado:** ✅ Resuelto — commit `4ebe3b7`
**Origen:** Drop-zone 2026-04-19

**Fix aplicado:**
Nuevo `UENUM(BlueprintType) EPressurePlateMode { Momentary, Latched }` + `UPROPERTY EPressurePlateMode Mode` (default **Momentary**, preserva comportamiento). En `Latched`, `RefreshOccupancy()` salta las transiciones a `false` cuando ya está activada. Método `ResetLatch()` BlueprintCallable (authority-only, no-op si `Momentary` o aún hay jugadores vivos encima). Reutiliza la replicación existente de `bOccupied` — no hay propiedad adicional.

**Diseño propuesto:**
La placa debe exponer dos modos via `UPROPERTY`:
1. **Momentáneo** (`EPressurePlateMode::Momentary`) — activa solo mientras hay un jugador encima. Al salir el último pawn, se desactiva.
2. **Latch** (`EPressurePlateMode::Latched`) — una vez pisada queda activada para siempre (o hasta reset externo).

Esto encaja con la lógica de managers: se pueden combinar varias placas momentáneas en un manager "todas-activas" para puertas que requieren mantenerse pisadas, o latch para triggers one-shot.

**Acción:**
- Crear `UENUM EPressurePlateMode` + `UPROPERTY` con default `Momentary` (preserva comportamiento actual).
- En `Latched`: al primer overlap marcar `bLatchedActive=true` y no desactivar en EndOverlap.
- Replicar `bLatchedActive` si procede (probablemente sí — el manager debe ver el estado en todas las máquinas).

---

### Q2-04 🔵 ButtonManager — rediseño: acción sumatoria, botones individuales indiferentes
**Componente:** `ATN_ButtonGroupManager` + `ATN_PressurePlateGroupManager`
**Estado:** ✅ Resuelto — commit `79f5174`
**Origen:** Drop-zone 2026-04-19

**Fix aplicado:**
Ambos managers reciben un nuevo `UPROPERTY int32 TriggerThreshold` con default `-1` (= "todos", compat con comportamiento previo). Helper `GetEffectiveThreshold()` que clampea a `[1, N]`. En `ATN_ButtonGroupManager::CheckAndTrigger`, sustituido el loop "todos activados" por `ActiveCount >= Required`. En `ATN_PressurePlateGroupManager::EvaluateCondition`, la comparación `PlatesOccupied >= GetEffectiveThreshold(AlivePlayers)` usa el threshold si está definido, o el count de vivos como antes (preservando la semántica original de "todos los vivos encima"). Simetría on/off respetada (off dispara `UntriggerActions` al bajar del umbral en ButtonGroupManager).

**Decisión de diseño:**
Los managers **no reaccionan a la acción individual de cada botón**. Su única responsabilidad es detectar cuándo **la suma de botones activados cruza el umbral configurado** (ej. "todos activados", "≥2 activados") y, en ese momento, aplicar un **offset al array final de objetos que maneja**.

Es decir:
- Botón individual → no hace nada observable por sí mismo.
- Manager → observa el estado booleano de sus N botones. Si `ActiveCount >= TriggerThreshold` (o `== N` para "todos"), aplica la acción al array de `ManagedActors`.

Esto simplifica drásticamente el modelo mental: los botones son sensores tontos, el manager es la lógica.

**Acción:**
- Revisar `ATN_ButtonManager` existente (o crearlo si aún no existe).
- `UPROPERTY TArray<TWeakObjectPtr<ATN_ButtonInteractable>> RegisteredButtons`.
- `UPROPERTY int32 TriggerThreshold` (default = `RegisteredButtons.Num()`, es decir "todos").
- `UPROPERTY TArray<AActor*> ManagedActors` — target de la acción.
- Método `OnButtonStateChanged` que recalcula `ActiveCount` y dispara la acción solo al cruzar el umbral (no cada cambio).
- Replicación: authority-only para el cálculo; el multicast/visual lo hacen los `ManagedActors`.

---

### Q2-05 🔵 Button/Manager — sistema de offsets Do/Undo + estados rotacionales múltiples
**Componente:** `ATN_ButtonInteractable`
**Estado:** ✅ Resuelto — commit `dde37a1`
**Origen:** Drop-zone 2026-04-19 (extensión de Q2-04)


**Fix aplicado:**
Nuevo `UENUM(BlueprintType) EButtonOffsetMode { DoUndo, CyclicStates }` (default **DoUndo**, comportamiento original preservado). `CyclicStates` añade `UPROPERTY TArray<FTransform> CyclicStateTransforms` (visible solo con `EditCondition`). `CurrentStateIndex` replicado con `OnRep_CurrentStateIndex` y getter `GetCurrentStateIndex()` expuesto a BP. En `Interact`, modo cíclico avanza `CurrentStateIndex = (CurrentStateIndex + 1) % N` y `bIsActivated` refleja "estado != 0" para mantener compat con `ButtonGroupManager`. `DeferredInit` precomputa `CyclicResolvedTransforms` (y equivalentes por `AdditionalMoveTargets`) para evitar recalcular cada Tick.

**Decisión de diseño:**
Cada botón expone un **offset que aplica (Do) y revierte (Undo)** al array de actores del manager. Además, se quiere soporte de **múltiples estados rotacionales** (ej. 0° → 90° → 180° → 270° → 0°), de modo que cada pulsación avance el botón un paso en el ciclo y el manager conozca la **posición actual** (índice de estado) de cada botón.

Esto desbloquea puzzles tipo "alinea los cuatro botones en la misma rotación" o "suma de rotaciones ≡ 0 mod 360°".

**Diseño propuesto:**
- `UENUM EButtonOffsetMode`:
  - `DoUndo` — dos estados: aplicar offset / revertirlo.
  - `CyclicStates` — N estados definidos por `TArray<FTransform>` o `TArray<float RotationDegrees>`.
- Cada botón expone su **estado actual** (`CurrentStateIndex`) replicado.
- Manager consulta `GetCurrentStateIndex()` de cada botón para resolver puzzles multi-botón.
- Métodos `ApplyOffset()` / `RevertOffset()` en modo Do/Undo; `AdvanceState()` en modo cíclico.

**Acción:**
- Implementar en el ButtonInteractable como `UPROPERTY EditDefaultsOnly` el modo + el array de estados (visible solo cuando `Mode == CyclicStates` via `EditCondition`).
- El manager ya no conoce "acciones" específicas, solo lee índices de estado y aplica la transformada resultante al `ManagedActors` array.
- Serialize-friendly para que el diseñador construya puzzles en editor sin C++.

---

## Fase 3 — Enemies

### Q3-01 🟡 Gaviota — sombra cambia de tamaño al spawn + velocidad y tasa serializables
**Componente:** `ATN_EnemySeagull` + `ATN_SeagullSpawnZone`
**Estado:** ✅ Resuelto — commit `5c9d5e4`
**Origen:** Drop-zone 2026-04-19

**Fix aplicado:**
1. `InitializeWithTarget()` ahora llama `UpdateDecalSize()` **antes** del resto de la inicialización → la sombra se dimensiona correctamente desde frame 1, sin pop visual.
2. Subido default `FollowSpeed` **280 → 350 cm/s** (ya era UPROPERTY serializable).
3. `SpawnInterval` del `SeagullSpawnZone` ya era UPROPERTY serializable (no requería cambio).
4. La sombra circular (material) es asset-work BP, fuera del scope C++.

**Observación:**
1. Al spawnear la gaviota, la **sombra aparece con un tamaño** y un instante después **cambia a otro** — se nota un pop visual. Debería estabilizarse desde el primer frame.
2. La sombra debería ser **circular** (actualmente probablemente es cuadrada / el quad del decal se ve).
3. **Velocidad de la gaviota** un poco baja — debería ser mayor. Exponer como `UPROPERTY` para tuning.
4. **Tasa de spawn** del `SeagullSpawnZone` no es `UPROPERTY` editable — debería serlo.

**Causas posibles (sombra):**
- El decal/quad de sombra se crea con tamaño default y se reescala en el siguiente tick cuando el `SetLifeSpan` o el `BeginPlay` termina de configurarlo.
- Ver patrón `SetTimerForNextTick` para chunk-compatible actors (`CLAUDE.md`) — probable causa: la sombra lee la world-position antes de estar correcta.

**Acción:**
- Corregir tamaño inicial de la sombra (que coincida con el valor final desde el primer frame).
- Cambiar el material/mesh de la sombra a un decal circular con alpha-mask circular.
- Añadir `UPROPERTY float FlightSpeed` (serializable) en `ATN_EnemySeagull`.
- Añadir `UPROPERTY float SpawnIntervalSeconds` (serializable) en `ATN_SeagullSpawnZone`.

---

### Q3-02 🟠 SeagullSpawnZone — gaviotas y cacas aparecen fuera de la zona de spawn
**Componente:** `ATN_SeagullSpawnZone` + `ATN_DroppingSpawnZone`
**Estado:** ✅ Resuelto — commit `6ee19d9`
**Origen:** Drop-zone 2026-04-19

**Clarificación (post-respuesta Rodrigo):** que la gaviota persiga al player fuera del zone es intencional. El bug era que **spawneaban** actores fuera (el target podía estar cerca del borde, y el spawn añadía offset que sacaba la posición del volumen).

**Fix aplicado:**
Nuevo helper `ClampXYToVolume(FVector) const` en ambos spawn zones: transforma el punto a local space del volumen, lo clampea a los extents del `UBoxComponent`, y lo convierte de vuelta a world. `TrySpawnSeagull` y `TrySpawnDropping` clampean la posición XY del target antes de generar el `SpawnLoc`. Así, aunque el player esté al borde, el enemy/caca siempre nace dentro del AABB del volumen.

**Observación:**
En el mapa de testing, se observan **gaviotas y cacas aparecer fuera de las zonas de spawn** colocadas. Revisar si:
- Ocurre sólo al salir de la zona (la gaviota deja un rastro fuera al terminar su trayectoria).
- O si algún actor está spawneando enemigos siempre, independientemente de las zonas.

**Causas posibles a revisar:**
- Alguna referencia al sistema deprecado `ATN_SeagullActor` quedó activa en el mapa testing (ver memoria `project_deprecated_systems.md`).
- El `SeagullSpawnZone` no respeta su propio `UBoxComponent` al generar la trayectoria — la gaviota empieza dentro de zona pero se desplaza fuera y sigue dropeando cacas.
- Volumen de zona mal configurado en el mapa (tamaño 0, o escala mal aplicada).

**Acción:**
- Grep en todo el mapa de testing para restos de `TN_SeagullActor` y sustituir por `TN_SeagullSpawnZone`.
- Confirmar que `ATN_EnemySeagull::Tick` (o equivalente) mantiene la lógica de drop **solo mientras está dentro del área de patrulla definida por la zona madre**.
- Log del spawn location para verificar en runtime que todo nace dentro de zonas.

---

### Q3-03 🟡 SeagullPoop — polish: sombra circular, sin movimiento, caída constante tras spawn
**Componente:** `ATN_SeagullDroppingActor`
**Estado:** ✅ Resuelto — commit `6196ebf`
**Origen:** Drop-zone 2026-04-19

**Fix aplicado:**
La caída vertical pura y el `SetAbsolute(true,true,true)` del decal ya existían (caída `Loc.Z -= FallSpeed * DeltaTime`, sin herencia de velocidad horizontal). Lo único que pulía era un pop inicial de la sombra antes de que replicara `ImpactXY`: añadido guard en `UpdateShadowScale()` que retorna temprano si `ImpactXY.IsNearlyZero() && GroundTargetZ == 0.f` — así la sombra queda oculta hasta que llegue el target real, evitando el flash de tamaño inicial.

La sombra circular/material es asset-work BP (no requiere C++).

**Observación:**
La caca de gaviota necesita tratamiento visual coherente con la gaviota:
- **Sombra circular reducida** (como la gaviota, escalada al tamaño de la caca).
- **Que no se mueva horizontalmente** — caída puramente vertical desde el spawn point (no herede la velocidad horizontal de la gaviota).
- **Caída constante** tras spawn (velocidad Z uniforme o gravedad estándar, no trayectoria curva extraña).
- **Visual de sombra** equivalente al de la gaviota (material + decal circular).

**Acción:**
- En el proyectil caca, anular la velocidad horizontal al spawnear: `ProjectileMovement->Velocity = FVector(0, 0, -Speed)`.
- Añadir decal de sombra al actor de caca (copiar patrón de la gaviota).
- Asegurar que el `LifeSpan` tras impacto deja tiempo para el `SetLifeSpan(0.2f)` de cleanup + VFX.

---

## Fase 4 — Items tirables

### Q4-01 🟠 InkProjectile — trayectoria debería ser parabólica, no recta
**Componente:** `ATN_InkProjectile`
**Estado:** ✅ Resuelto — commit `635377e`

**Fix aplicado:**
Nuevo `UPROPERTY GravityAcceleration` (default 980 cm/s²). En Tick, `LaunchVelocity.Z -= GravityAcceleration * DeltaTime` antes de aplicar el desplazamiento. El actor se rota hacia `LaunchVelocity.Rotation()` para que el mesh mire en dirección del vuelo. Ajustable por BP sin recompilar — `0` da trayectoria recta.

---

### Q4-02 🔴 Cliente no ve la bola lanzada hasta aparecer el pickup
**Componente:** `ATN_ThrowableItemActor`
**Estado:** ✅ Resuelto — pendiente de re-test en PIE 2P

**Causa raíz identificada:**
En el cliente-lanzador, `AActor::Instigator` puede replicarse **después** del primer `BeginPlay` si el bunch inicial se fragmenta. Como `Mesh->IgnoreActorWhenMoving(GetInstigator(), true)` sólo se llamaba en `BeginPlay`, si el Instigator era null en ese frame la llamada era no-op → al activar el `ProjectileMovement` la bola colisionaba con la cápsula del propio lanzador → rebote fuerte hacia atrás → salía de cámara antes de que el usuario la viera. El host no sufría esto porque en authority el Instigator está siempre resuelto. Todos los demás clientes tampoco lo veían porque su cámara no estaba detrás de la bola que rebotaba.

**Fix aplicado (cover-all):**
Nuevo helper `IgnoreInstigatorCollision()` llamado desde cuatro puntos idempotentes:
1. `BeginPlay` — intento inicial (ok en authority).
2. `OnRep_Instigator` (override) — cubre replicación tardía del Instigator.
3. `ApplyLaunchDataIfReady` — reintento antes de `ProjectileMovement->Activate`.
4. `MulticastLaunch_Implementation` — reintento antes de activar el mov. local.
Cualquier orden de llegada de replicación garantiza que el ignore está configurado antes de que la bola se mueva un solo frame.

---

### Q4-03 🟠 BananaPeel — no desliza hacia delante al pisarla
**Componente:** `ATN_BananaPeel` + `ATortugaCharacter::Tick` (ground lock)
**Estado:** ✅ Resuelto (2 fixes)

**Causa raíz (fase 1):**
1. El fallback para jugador **parado** usaba `-ActorForwardVector` → el impulso iba hacia **atrás**.
2. La componente Z era `200.f` hardcoded — muy baja. Slide de ~2 m, casi invisible.

**Fix fase 1 (`7dbc3e3`):**
- Fallback estacionario ahora usa `+ActorForwardVector`.
- Nueva `UPROPERTY SlideVerticalForce` (default 400). Log añadido.

**Causa raíz (fase 2 — reportada 2026-04-18 tras re-test):**
`ATortugaCharacter::Tick` hacía `MC->DisableMovement()` **en cuanto** `IsMovingOnGround()` daba `true` durante `bIsKnockedDown`. Justo al aterrizar el arco del plátano, el CMC entraba en `MOVE_None` → el impulso horizontal se perdía en seco. No había decaimiento por fricción, el slide se notaba sólo durante el vuelo. El momentum que traía el jugador no se respetaba en el suelo.

**Fix fase 2:**
Nueva `UPROPERTY KnockdownGroundLockSpeed` (default 50 cm/s) en `TortugaCharacter`. El Tick ahora sólo lockea el movimiento cuando `Velocity.Size2D() < KnockdownGroundLockSpeed` — mientras la velocidad horizontal siga siendo significativa, el CMC permanece en `MOVE_Walking` y la fricción natural desacelera el slide. Al caer por debajo del umbral se aplica el lock.

Efecto: el resbalón se extiende por el suelo post-aterrizaje hasta detenerse de forma orgánica, respetando el momentum del jugador. Compatible con PufferFish (que aterriza sin impulso horizontal → velocidad ~0 → lock inmediato, comportamiento inalterado).

---

### Q4-04 🔴 Cliente no ve la bola lanzada — regresión de Q4-02
**Componente:** `ATN_ThrowableItemActor`
**Estado:** ✅ Resuelto (pendiente de re-test en PIE 2P)
**Origen:** Testing 2026-04-19 (reaparece tras `84fd833`)

**Causa raíz (teleport-back race):**
En el cliente, al abrir el actor-channel, el orden REAL de procesamiento es:
1. Replicación inicial de props → `OnRep_ThrowData` → `ApplyLaunchDataIfReady` activa el `ProjectileMovement` → la bola empieza a volar localmente (varios frames).
2. Llega el RPC `MulticastLaunch` que fue emitido en el mismo bunch.
3. `MulticastLaunch_Implementation` **no comprobaba `bLaunchApplied`** → re-hacía `SetActorLocation(Origin)` sobre la bola ya volando → **rubberband** de vuelta al punto de lanzamiento + re-activación con la velocidad inicial. Con la velocidad alta (`ThrowSpeed=1800 cm/s`), el stutter visual desplaza la bola fuera de cámara antes de que el usuario la vea; solo reaparece como pickup final.

El fix Q4-02 (`84fd833`) cubrió el race de `Instigator`, pero `MulticastLaunch` quedó sin idempotencia frente a la ruta de OnRep, que es la que gana en la práctica.

**Fix aplicado:**
- `MulticastLaunch_Implementation` ahora sale temprano si `bLaunchApplied == true` (además del check de authority). El Multicast queda como fallback defensivo para el improbable caso de que OnRep no haya corrido aún.
- Belt-and-suspenders en ambos paths (`ApplyLaunchDataIfReady` + `MulticastLaunch`): `Mesh->SetHiddenInGame(false)` + `SetVisibility(true, true)` explícitos — protege frente a BPs con defaults de visibilidad raros.
- `UE_LOG Verbose` en `ApplyLaunchDataIfReady` para observabilidad futura (authority, origen, velocidad, mesh).

**Queda por validar (PIE):**
- Smoke test con listen-server + 1 cliente: el cliente-lanzador debe ver la bola en vuelo parabólico desde el primer frame hasta el impacto/pickup.
- Verificar que el host sigue viendo su propia bola (no debería haber regresión en authority path).

---

### Q4-06 🔴 BallThrowable — lag/problemas, buscar alternativa al sistema actual
**Componente:** `ATN_PhysicsObjectActor` + `ATN_BouncePhysicsObject`
**Estado:** ✅ Resuelto — commit `e50b056`
**Origen:** Drop-zone 2026-04-19

**Decisión (post-respuesta Rodrigo):** mantener simulación física, priorizar **consistencia visual cliente/servidor** en lugar de crear una clase cinemática nueva. Ajustes de tuning y flush explícito en hits.

**Fix aplicado:**
- `NetUpdateFrequency` **50→30 Hz** (suficiente para objetos no-player; reduce ancho de banda).
- `DormancyCheckInterval` **0.5→1.5 s** (reduce coste del timer de chequeo de velocidad sin perder precisión al dormir).
- `CrushCheckInterval` **1.0→2.0 s**.
- `bEnableCrushDetection = false` por default en `ATN_BouncePhysicsObject` — una bola que rebota libre no debe auto-destruirse, y así ahorra 6 raycasts/2s.
- `TryEnterDormancy` llama `ForceNetUpdate()` antes de `SetNetDormancy(DORM_DormantAll)` → el cliente recibe el estado final exacto justo antes del dormir.
- `OnKickHit` llama `FlushNetDormancy() + ForceNetUpdate()` al final → el empuje de la bola por el player se ve alineado cliente/servidor sin el retardo de ~33 ms típico de 30 Hz.

**Pendiente si persiste desync:** segundo commit con replicación explícita de `LinearVelocity`/`AngularVelocity` (ticket separado si el smoke test revela drift).

**Observación:**
El sistema de BallThrowable actual está causando **lag y problemas** no acotados en los tests. Se pide **explorar alternativas** antes de seguir parcheando síntomas.

**Hipótesis a validar:**
- La bola usa `ProjectileMovement` + simulación física al aterrizar → coste agregado por ticks de físicas + replicación de transform de alta frecuencia.
- El `TN_BouncePhysicsObject` (BALL commit `ada02ac`) podría estar saturando los overlaps/trace queries al tener rotación libre + bounce.
- Posible leak de actores si el `SetLifeSpan` no se aplica en alguna ruta.

**Alternativas candidatas:**
1. **Proyectil puramente cinemático + pickup spawn determinista** — sin physics simulation en la bola, solo trayectoria parabólica controlada (estilo `TN_InkProjectile` de Q4-01).
2. **Hit-scan con VFX animado** — si la mecánica de "bola que bota" no es core gameplay.
3. **Simulación física solo cliente (cosmética) + lógica server authority** — el servidor conoce la trayectoria exacta, el cliente solo renderiza algo vistoso.
4. **Reemplazar `BouncePhysicsObject` por un proyectil con bounce scripted** (reflect manual en el normal del hit) — más barato que simulación física completa.

**Acción:**
- Profiling primero: `stat unit`, `stat game`, `stat physics` durante una partida PIE con varias bolas activas. Medir coste real antes de elegir alternativa.
- Si el coste está en replicación: reducir `NetUpdateFrequency` de la bola y hacer interpolación cliente.
- Si el coste está en físicas: saltar a alternativa #1 o #4.
- **Pareja Terry ↔ Don Claudio** para este ticket (ver memoria `feedback_terry_don_pairing`): Don diseña, Terry implementa.

---

### Q4-05 🟠 Concha — spawn del PickUp debe diferirse hasta final del block timeout
**Componente:** `ATN_ConchPickup`
**Estado:** ⬜ Abierto
**Origen:** Testing 2026-04-19 (refinamiento de B3 `7089989`)

**Observación:**
Actualmente B3 spawnea el pickup de repuesto **inmediatamente** al auto-destruirse la trampa activada (`SetLifeSpan(0.2f)` → spawn en el mismo frame). Rodrigo quiere que el pickup aparezca al **finalizar el block timeout** del jugador atrapado — así la concha "se consume" durante el castigo y reaparece justo cuando el jugador se libera.

**Diseño propuesto:**
- Diferir el `SpawnActor` del nuevo `ATN_ConchPickup` ítem hasta que `RestoreMovement` (o equivalente del timer de inmovilización) dispare.
- Mantener el `SetLifeSpan(0.2f)` de la concha activada: el spawn del repuesto no depende de la vida de la trampa, sino del timer del player.
- Guardar la `SpawnLocation` + `GetClass()` antes de destruir la trampa.
- `UPROPERTY PickupRespawnDelaySeconds` (default = `BlockDuration`) para permitir ajuste independiente si el diseñador lo quiere desacoplado.

**Acción:**
- Implementar en C++ + testear en PIE 2P.

---

## Fase 5 — Especiales (Jellyfish, Storm, ScriptedDeathZone)

*(pendiente de testing)*

---

## Batch drop-zone 2026-04-19 (tarde)

### Q4-07 🔴 ThrowableItem — bola invisible en cliente (regresión de Q4-04)
**Componente:** `ATN_ThrowableItemActor`
**Estado:** ✅ Resuelto — commit `9c0c9dc`
**Origen:** Drop-zone 2026-04-19 (tarde) + log cliente `Tortunabo_2.log`

**Observación:**
Tras Q4-04, el cliente todavía pierde la parábola de la bola de forma intermitente. Log `Tortunabo_2.log` trace 2:

```
[964] MulticastLaunch auth=0 bLaunchApplied=0 v=(-1394,98,1135)
[964] OnRep_Instigator = BP_TortugaCharacter_C_1
[964] OnRep_ThrowData bReady=1 v=(-1394,98,1135) mesh=Sphere
[964] BeginPlay auth=0 bReady=1 bLaunchApplied=1 inst=BP_TortugaCharacter_C_1
```

Ningún `ApplyLaunchDataIfReady` entre OnRep_ThrowData y BeginPlay — lo cual indica early-return porque `bLaunchApplied` ya era `1` (lo puso el `MulticastLaunch` previo).

**Causa raíz:**
Cuando el RPC llega en el mismo bunch que la replicación inicial del actor, el orden puede ser **MulticastLaunch → OnReps → BeginPlay**. Al ejecutarse MulticastLaunch antes de BeginPlay, `Mesh` y `ProjectileMovement` aún no están registrados con la escena — `SetStaticMesh`, `SetActorLocation`, `SetVelocity`, `Activate(true)` son silently-noop. El flag `bLaunchApplied=true` que queda tras MulticastLaunch bloquea la ruta canónica (`OnRep_ThrowData → ApplyLaunchDataIfReady`) cuando ya sí podría aplicarlo.

**Fix aplicado:**
Early-return en `MulticastLaunch_Implementation` si `!HasActorBegunPlay()`. `bLaunchApplied` queda en `0`. Cuando después llegan OnReps + BeginPlay, la ruta canónica aplica el launch con el actor totalmente inicializado. Se añade `hasBP` al log para diagnosticar futuros casos.

---

### Q4-08 🟠 BouncePhysicsObject — feel de balón de fútbol en césped
**Componente:** `ATN_BouncePhysicsObject`
**Estado:** ✅ Resuelto — commit `63e63fa`
**Origen:** Drop-zone 2026-04-19 (tarde)

**Observación:**
La bola física rueda sin parar tras un kick, sin fricción perceptible, y tampoco entra en dormancy porque su velocidad a bajo tempo queda por encima del `SleepVelocityThreshold=15` del padre. Feel deseado por Rodrigo: *"balón de fútbol en césped — rueda, decelera y se detiene, no pegajoso"*.

**Fix aplicado:**
- Nueva UPROPERTY `LinearDamping` (default `0.3`) aplicada al `BodyInstance` en `BeginPlay` tras el Super.
- Nueva UPROPERTY `AngularDamping` (default `0.6`).
- `SleepVelocityThreshold` override `15→30` en constructor del hijo (para que la bola sí entre en dormancy tras rodar).

**Ajuste fino pendiente (si se nota):**
- Ajustar `Bounciness` en el `PhysicalMaterial` del mesh (asset-work BP).
- Si `KickVerticalBoost=350` sigue sintiéndose bajo, Rodrigo puede tunear desde el BP sin tocar C++.

---

### Q3-04 🟡 SeagullDropping — mesh encoge al caer + sombras circulares (asset)
**Componente:** `ATN_SeagullDroppingActor` + material de decal (shared con `ATN_EnemySeagull`)
**Estado:** ✅ Parcial — commit `2aae0dd` (C++ mesh shrink). Sombras circulares = asset-work BP.
**Origen:** Drop-zone 2026-04-19 (tarde)

**Observación:**
1. La caca cae a tamaño constante. Rodrigo pide que vaya **haciéndose más pequeña** conforme baja (feedback visual de distancia).
2. Las sombras (caca y gaviota) se ven como **cubos**, no como círculos.

**Fix aplicado (C++):**
- Nueva UPROPERTY `MinMeshScaleFactor` (default `0.35`) en `ATN_SeagullDroppingActor`.
- En `BeginPlay` se cachea `InitialMeshScaleCache = DroppingMesh->GetRelativeScale3D()` (preserva la escala del BP).
- En `UpdateShadowScale` (llamada cada Tick), el mesh escala linealmente desde `InitialMeshScaleCache` (spawn, `NormalizedHeight=1`) hasta `InitialMeshScaleCache * MinMeshScaleFactor` (impacto, `NormalizedHeight=0`), en paralelo al decal.

**Pendiente (asset-work BP, fuera de scope C++):**
- Crear/asignar un material de decal con máscara alpha circular al `ShadowDecal` del dropping y al `DangerDecal` del EnemySeagull. Con el material default de UE se ve el cuboide completo del decal → apariencia de cubo. El material correcto recorta con alpha circular y muestra solo la silueta redonda.
- Asignar en `BP_SeagullDropping` y `BP_EnemySeagull`, Details → Decal → Material.

---

### Q1-12 🟡 BreakablePlatform — modos Reversible/Latched
**Componente:** `ATN_BreakablePlatform`
**Estado:** ✅ Resuelto — commit `1951270`
**Origen:** Drop-zone 2026-04-19 (tarde)

**Observación:**
Rodrigo pide dos modos:
1. **Latched:** si el threshold (X jugadores) se alcanza, el puente cae aunque todos se bajen.
2. **Reversible:** si el threshold se alcanza pero un jugador se baja antes de la rotura, el puente recupera integridad.

El comportamiento actual (pre-fix) es Reversible por defecto.

**Fix aplicado:**
- Nuevo UENUM `EBreakablePlatformMode { Reversible, Latched }` (BlueprintType).
- UPROPERTY `BreakMode` default `Reversible` (retrocompat).
- En `OnStandTriggerEndOverlap`, si `BreakMode == Latched` y `BreakTimerHandle` está activo, early-return sin cancelar timers.
- El shake ya no se cancela al salir pawns (guard existente `if (bIsShaking) return`), así que Latched simplemente extiende esa política al período pre-shake.

---

## Batch drop-zone 2026-04-24

### NEW-01 🟠 PhysicsObject — sandwich cubo/pared al empujar
**Componente:** `ATN_PhysicsObjectActor`
**Estado:** ✅ Resuelto — pendiente compilar y verificar en PIE
**Origen:** Drop-zone 2026-04-24

**Observación:**
La tortuga podía crear un "sandwich": caminar contra el cubo hasta pegarlo a una pared, y `OnMeshHit` seguía aplicando `SetPhysicsLinearVelocity` apuntando a la pared → el cubo la traspasaba. El `PushForceFactor=0` del CMC ya desactivaba el push automático, pero el impulso manual en `OnMeshHit` persistía.

**Fix aplicado:**
Eliminado el bloque `Cast<ATortugaCharacter>` + `SetPhysicsLinearVelocity` de `OnMeshHit`. La tortuga **ya no puede aplicar fuerza al cubo caminando/corriendo**. El cap `MaxPushVelocity` para fuentes externas (bola lanzada) se mantiene. Eliminado `CharacterPushVelocity` del header (propiedad huérfana). Eliminado el include de `TortugaCharacter.h` del `.cpp`.

**Comportamiento resultante:**
- Tortuga camina contra cubo → bloqueada por la colisión, cubo no se mueve. ✅
- Cubo puede ser movido por bolas lanzadas (fuentes externas) con cap de velocidad. ✅
- `ATN_BouncePhysicsObject::OnKickHit` (kick explícito) funciona igual — usa `AddImpulse` directo, independiente de este bloque. ✅

---

### SKIN-01 🔵 Skins — nueva solución para SKM único
**Componente:** `ATortugaCharacter::UpdateSkinVisual` + `FTN_SkinData`
**Estado:** ⬜ Abierto (diseño documentado, pendiente implementar)
**Origen:** Drop-zone 2026-04-24

**Contexto:**
El personaje migró de múltiples StaticMeshComponents a un único SkeletalMeshComponent. El sistema de skins anterior aplicaba materiales por componente (Cuerpo, Pata1, Pata2, Cola, Cabeza). Ahora debe aplicarse por slot del SKM.

**Diseño propuesto:**
Ver `Docs/SISTEMA_SKINS.md` para el diseño completo, checklist de implementación y código propuesto.

Resumen de cambios:
- `DefaultBodyMaterials` (TMap de SMCs) → `DefaultSkelMeshMaterials` (TArray por slot)
- `BeginPlay` cachea slots del SKM en lugar de SMCs
- `UpdateSkinVisual` itera slots del SKM: Slot 0=BodyMaterial, Slot 1=BellyMaterial, Slot 2=SkinMaterial
- `DT_Skins` y `FTN_SkinData` no cambian
- Helmets (socket `Sombrero`) no cambian

**Pendiente editor (bloqueante):**
Verificar índices de material slots del nuevo SKM en UE5 antes de implementar el C++.

---

### SOCKET-01 🟠 Animaciones — rotaciones incorrectas tras migración a SKM
**Componente:** `ATortugaCharacter` (SkeletalMesh sockets + ABP)
**Estado:** ⬜ Abierto
**Origen:** Drop-zone 2026-04-24

**Contexto:**
Antes: múltiples SceneComponents con meshes individuales (cabeza, brazos, piernas, cola). Ahora: un único Skeletal Mesh. Los sockets del nuevo SM tienen rotaciones incorrectas → las animaciones de walk/run/emotes/look/jump muestran offsets erróneos en huesos.

**Capas del problema:**
1. **Editor (80%):** Los sockets deben reposicionarse/rerotarse en el Skeleton Editor del SM. No es código.
2. **C++ (20%):** Si hay nombres de sockets hardcodeados en código que no existen en el nuevo SM → attaches silenciosamente rotos.

**Audit de socket names en C++:**
Sockets referenciados en código:
- `"Sombrero"` — `HelmetSocketName` en `TortugaCharacter.h:407` (helmet attach)
- Bones: `Brazo1Bone`, `Brazo2Bone`, `ColaBone`, `CabezaBone`, `Pata1Bone`, `Pata2Bone` → resueltos en BeginPlay desde sockets del SM

**Acción:**
- En editor: abrir el nuevo SM → Skeleton → verificar que existen sockets con los nombres exactos `Brazo1`, `Brazo2`, `Pata1`, `Pata2`, `Cola`, `Cabeza`, `Sombrero`.
- Si los huesos/sockets del nuevo SM tienen nombres diferentes → actualizar `BeginPlay` en `TortugaCharacter.cpp` donde se resuelven los bone names.
- El ajuste de rotaciones de las animaciones es trabajo de editor (ABP blend spaces, pose correciva, o reposicionamiento de sockets).

---

### Q4-09 🟡 ThrowBall — torque de rotación en vuelo
**Componente:** `ATN_ThrowableItemActor`
**Estado:** ⬜ Abierto
**Origen:** Drop-zone 2026-04-24

**Observación:**
La bola lanzada vuela sin girar. Se quiere aplicar un **torque predefinido** (rotación constante, no dependiente de la dirección del lanzador) para dar realismo visual. No debe romper el sistema actual de `ProjectileMovement` ni la replicación.

**Diseño propuesto:**
- Nueva `UPROPERTY FVector ThrowAngularVelocityDegSec` (default `(0, 360, 0)` — giro sobre eje Y = backspin natural).
- En `ApplyLaunchDataIfReady`: `Mesh->SetPhysicsAngularVelocityInDegrees(ThrowAngularVelocityDegSec)`.
- No afecta la trayectoria (solo es cosmético sobre el mesh, el `ProjectileMovement` controla la posición).
- `(0,0,0)` = sin rotación (retrocompat).

---

### Q4-10 🟠 ThrowBall — dato no se envía correctamente de forma intermitente
**Componente:** `ATN_ThrowableItemActor`
**Estado:** ⬜ Abierto
**Origen:** Drop-zone 2026-04-24

**Observación:**
Muy de vez en cuando, la bola lanzada "se queda" — parece que el dato de lanzamiento no llega. Ocurre raramente pero es un bug real de replicación intermitente.

**Hipótesis más probable:**
El timeout del guard `bLaunchApplied` — si `ApplyLaunchDataIfReady` nunca se dispara (OnRep llega corrupto o se pierde en un bunch fragmentado), la bola queda en spawn sin activar. No hay retry activo.

**Acción propuesta:**
- En `BeginPlay`, después de N ticks sin `bLaunchApplied=true`, forzar `ApplyLaunchDataIfReady` si los datos están listos.
- Timer de 0.5s en clientes: si `bLaunchApplied==false && ThrowData.IsReady()` → reintentar.

---

### Q1-13 🟡 Muerte — aplicar ragdoll igual que knockdown
**Componente:** `ATortugaCharacter` (sistema de muerte + `bUsePhysicsRagdoll`)
**Estado:** ⬜ Abierto
**Origen:** Drop-zone 2026-04-24

**Observación:**
Cuando un jugador muere, debería caer con ragdoll físico (igual que el knockdown de Q1-07) en lugar de simplemente ocultarse o quedarse rígido. El ragdoll de Q1-07 ya está implementado y funciona — reutilizarlo para muerte.

**Diseño propuesto:**
- En `SetDeadVisual(true)`: si `bUsePhysicsRagdoll && GetMesh()->GetPhysicsAsset()` → activar ragdoll físico sin timer de recovery (muerte es permanente en este contexto).
- El `MulticastSetDeadVisual` ya existe y replica a todos — solo añadir la activación de ragdoll dentro de él.
- No hay recovery: `SetSimulatePhysics(false)` nunca se llama al morir.

---

### Q1-14 🔵 Revive item — ¿skeletal mesh con ragdoll activo?
**Componente:** `ATN_RescuePickup`
**Estado:** ⬜ Decisión pendiente
**Origen:** Drop-zone 2026-04-24

**Pregunta de diseño:**
¿Debería el item de revive (`TN_RescuePickup`) ser un skeletal mesh con ragdoll activo, que represente visualmente el cuerpo caído del jugador?

**Consideraciones:**
- El pawn muerto ya existe en el mundo como cadáver interactuable — hay un `ATortugaCharacter` con `bIsDead=true` en el punto de muerte.
- El `TN_RescuePickup` actual es un actor separado spawnedo en la posición de muerte.
- Si el pawn muerto ya tiene ragdoll (Q1-13), podría eliminarse el `RescuePickup` visual y usar el propio pawn como visual + trigger de revive.
- O: mantener el RescuePickup como interactuable pero hacerlo coincidir con el pawn ragdolleado visualmente.

**Acción:** Decisión de diseño antes de implementar. Depende de si Q1-13 queda bien visualmente.

---

## Batch drop-zone 2026-04-18 (tarde)

### B1a 🟡 BananaPeel — potencia excesiva en defaults
**Estado:** ✅ Resuelto (`574db5d`)
Bajar defaults del impulso de slide ahora que el momentum se preserva.
`SlideImpulseMultiplier 1.5→1.0`, `MinSlideForce 600→350`.

### B2 🔴 BreakablePlatform — vibración reinicia los timers
**Estado:** ✅ Resuelto (`0f7728a`)
El `StandTrigger` está attacheado al mesh y oscila con él durante el shake. Esa oscilación generaba `EndOverlap` espurios → cancelaba timers → `BeginOverlap` al re-tocar → loop infinito. Guard `if (bIsShaking) return;` en `OnStandTriggerEndOverlap`.

### B3 🟠 ConchPickup — trampa desaparece sin dejar pickup
**Estado:** ✅ Resuelto (`7089989`)
Al auto-destruirse tras atrapar, ahora spawnea un nuevo `ATN_ConchPickup` (via `GetClass()` para preservar el BP hijo) en modo ítem en la misma ubicación, que se puede recoger de nuevo.

### B4 🟠 StaminaBoost — penalización actual demasiado punitiva
**Estado:** ✅ Resuelto (`5d5a9f6`)
Penalización blanda post-boost: durante `PostBoostExhaustionSeconds` (4s) el jugador va a `0.75×` velocidad y el sprint drena a `2×`. No se resetea CurrentStamina ni se bloquea recuperación. Dos nuevas UPROPERTY (`PostBoostSpeedMultiplier`, `PostBoostDrainMultiplier`) + timer dedicado `PostBoostPenaltyTimer`.

### B5 🟠 PhysicsObject — clipping al quedar atrapados en geometría
**Estado:** ✅ Resuelto (`c4ba273`)
Timer periódico (`CrushCheckInterval=1s`, opt-in `bEnableCrushDetection`) lanza 3 pares de rays opuestos (±X ±Y ±Z) contra WorldStatic. Si ≥2 ejes bloqueados → `MulticastCrushPoof` + `SetLifeSpan(0.2)`. VFX/sonido opcionales.

### BALL 🔵 PhysicsObject — hijo con bote vertical y rotación libre
**Estado:** ✅ Resuelto (`ada082c`)
Nuevo `ATN_BouncePhysicsObject` hereda de `ATN_PhysicsObjectActor`, libera los 3 locks de rotación y al impactar con `ATortugaCharacter` aplica `LaunchCharacter` con componente Z (`PlayerBounceImpulseZ=700`) + empuje horizontal en dirección bola→jugador. `PerPlayerCooldown=0.25s` evita disparos múltiples por un mismo contacto.

---

## Historial de resoluciones

| ID | Fecha | Commit | Resumen |
|---|---|---|---|
| Q1-12 | 2026-04-19 | `1951270` | BreakablePlatform — UENUM `EBreakablePlatformMode` con modos Reversible/Latched |
| Q3-04 | 2026-04-19 | `2aae0dd` | SeagullDropping — mesh shrink lineal al caer (`MinMeshScaleFactor`) |
| Q4-08 | 2026-04-19 | `63e63fa` | BouncePhysicsObject — `LinearDamping`/`AngularDamping` + `SleepVelocityThreshold` 15→30 |
| Q4-07 | 2026-04-19 | `9c0c9dc` | ThrowableItem — defer MulticastLaunch si `!HasActorBegunPlay()` (ruta canónica aplica después) |
| Q4-06 | 2026-04-19 | `e50b056` | PhysicsObjectActor — NetUpdate 50→30Hz, ForceNetUpdate en hits, Crush off en Bounce |
| Q2-05 | 2026-04-19 | `dde37a1` | ButtonInteractable — `EButtonOffsetMode` con estados cíclicos replicados |
| Q2-04 | 2026-04-19 | `79f5174` | ButtonGroupManager + PressurePlateGroupManager — `TriggerThreshold` configurable |
| Q2-03 | 2026-04-19 | `4ebe3b7` | PressurePlate — modo `Momentary`/`Latched` + `ResetLatch()` |
| Q2-02 | 2026-04-19 | `d17e1be` | QuadSpawner — `EndLocation` → `EndOffsetLocal` (local space) |
| Q3-02 | 2026-04-19 | `6ee19d9` | SeagullSpawnZone + DroppingSpawnZone — clamp XY al volumen |
| Q1-11 | 2026-04-19 | `1e37de8` | BreakablePlatform — shake escalada por `PawnsOnPlatform.Num()` |
| Q3-03 | 2026-04-19 | `6196ebf` | SeagullDroppingActor — guard sombra hasta que replique `ImpactXY` |
| Q3-01 | 2026-04-19 | `5c9d5e4` | EnemySeagull — sombra estable frame 1 + `FollowSpeed` 280→350 |
| Q4-04 | 2026-04-19 | `b815205` | ThrowableItem — guard `bLaunchApplied` en MulticastLaunch (teleport-back race tras OnRep_ThrowData) |
| BALL | 2026-04-18 | `ada082c` | TN_BouncePhysicsObject — hijo con bote vertical + rotación libre |
| B5 | 2026-04-18 | `c4ba273` | PhysicsObject — crush detection anti-clipping (3 pares de rays) |
| B4 | 2026-04-18 | `5d5a9f6` | StaminaBoost — penalización blanda (speed×0.75, drain×2) |
| B3 | 2026-04-18 | `7089989` | ConchPickup — spawn pickup-item al auto-destruirse |
| B2 | 2026-04-18 | `0f7728a` | BreakablePlatform — ignorar EndOverlap espurios durante shake |
| B1a | 2026-04-18 | `574db5d` | BananaPeel — bajar potencia default del slide |
| Q4-03 fase 2 | 2026-04-18 | `2becf55` | BananaPeel — Tick ground-lock ahora respeta momentum (`KnockdownGroundLockSpeed`) |
| Q1-09 | 2026-04-18 | `903c293` | Quicksand deprecado — unificado en SlowZone con `GravityScaleInZone` expuesto |
| Q1-07 | 2026-04-18 | `5aca4a5` | Knockdown — ragdoll físico opcional (`bUsePhysicsRagdoll`, requiere PhysicsAsset en BP) |
| Q2-01 | 2026-04-18 | `7ef8d40` | ConchPickup — one-shot por defecto (bDestroyAfterActivation) |
| Q1-08 | 2026-04-18 | `2020d9c` | BreakablePlatform — re-detecta pawns al respawnear + logs permanentes |
| Q1-10 | 2026-04-18 | `6768e1c` | PhysicsObject — locks rotación X/Y/Z (default slide-only) |
| Q4-03 | 2026-04-18 | `7dbc3e3` | BananaPeel — slide hacia delante + `SlideVerticalForce` tunable |
| Q4-02 | 2026-04-18 | `84fd833` | ThrowableItem — cover-all de `IgnoreInstigatorCollision` (race con `OnRep_Instigator`) |
| Q4-01 | 2026-04-18 | `635377e` | InkProjectile parabólico (`GravityAcceleration`) |
| Q1-03 | 2026-04-18 | `65446f6` | BreakablePlatform shake antes de romperse |
| Q1-02 | 2026-04-18 | `c3a0a12` | BreakablePlatform logs de respawn |
| Q1-05 | 2026-04-18 | `dc21d12` | PhysicsObject CCD anti-tunneling |
| Q1-01 | 2026-04-18 | `beb9b53` | Knockdown tilt 180° sobre emote |
| Q1-04 | 2026-04-18 | `9884bbd` | Quicksand rediseño (slow + DeathZone) |

---

## Smoke test checklist — 2026-04-19 (batch cierre 9 tickets)

Validación en PIE listen-server **2 jugadores** tras compilar con `-NoHotReload`. Cada ítem ≤30 s. Marcar ✅ al verificar, ❌ si falla.

### Fase 3 — Enemies
- [ ] **Q3-01** — Spawnear gaviota: su sombra aparece con tamaño correcto desde el primer frame (sin pop). `FollowSpeed` default se percibe más ágil que antes (350 cm/s).
- [ ] **Q3-02** — Con el player al borde del `SeagullSpawnZone`/`DroppingSpawnZone`: las gaviotas/cacas **siempre** nacen dentro del volumen (no salen fuera aunque el target esté en la frontera).
- [ ] **Q3-03** — Caca de gaviota: sombra no parpadea antes de caer; caída vertical pura, sin arrastre horizontal.

### Fase 1 — Hazards
- [ ] **Q1-11** — BP_BreakablePlatform con `bScaleShakeByPlayerCount=true` + array `[3,6,10,15]`: 1 jugador encima → shake leve; 2 → shake intermedio; 3+ → shake fuerte. Con `bScaleShakeByPlayerCount=false` comportamiento original.

### Fase 2 — Interactables
- [ ] **Q2-02** — BP_QuadSpawner con `EndOffsetLocal=(5000,0,0)`: al mover el spawner por el mapa, el quad viaja 50 m en dirección forward del spawner (no al origen del mundo).
  *Nota: tras renombre, los valores BP previos se pierden — re-setear en editor.*
- [ ] **Q2-03** — Placa en modo `Latched`: al subir-bajar-subir-bajar, permanece activada hasta llamar `ResetLatch()`. En modo `Momentary` vuelve a false al salir.
- [ ] **Q2-04** — ButtonGroupManager con `TriggerThreshold=2` (4 botones): se dispara la acción con 2 botones activados. PlateGroupManager con `TriggerThreshold=-1`: se dispara cuando todos los vivos están encima (comportamiento original).
- [ ] **Q2-05** — Botón en modo `CyclicStates` con 4 entradas de rotación: cada pulsación avanza 90°, en la 4ª vuelve a 0°. `CurrentStateIndex` se ve correctamente replicado en el cliente.

### Fase 4 — Physics & throwables
- [ ] **Q4-06 (a)** — Con varias bolas activas y `stat net`: `OutBytes/s` por bola reducido respecto a antes (objetivo: <500 B/s vs ~800 B/s previo).
- [ ] **Q4-06 (b)** — Pegar a la bola con el player: cliente y servidor ven el empuje en el mismo instante (sin retardo perceptible de ~33 ms). Bola al parar queda en el mismo sitio en ambas máquinas.
- [ ] **Q4-06 (c)** — `stat unit`: frame time <16.6 ms con 2 bolas activas + 2 jugadores. Sin spikes al activarse/desactivarse dormancy.

### Regresiones a vigilar
- [ ] Los tickets previos (Q4-01..Q4-04, Q1-01..Q1-10, Q2-01, B1..B5, BALL) siguen funcionando; ningún cambio nuevo pisa sus fixes.
- [ ] Compilación limpia sin warnings nuevos en los archivos modificados.
- [ ] No hay pérdida visible de replicación de `bOccupied` (PressurePlate) ni de `bIsActivated`/`CurrentStateIndex` (Button) en tests con >1 cliente.

---

## Smoke test checklist — 2026-04-19 (tarde, batch Q4-07/Q4-08/Q3-04/Q1-12)

PIE listen-server 2 jugadores, `-NoHotReload`. Buscar regresiones de los fixes previos también.

- [ ] **Q4-07** — En un cliente no-host, pedir al host lanzar la bola (`ATN_ThrowableItem`). El cliente ve la **parábola completa** desde el spawn hasta el impacto. No hay frames en los que la bola "desaparezca y aparezca como pickup". Repetir 3×.
  - Si falla, buscar en `Saved/Logs/Tortunabo_2.log` el bloque `[TN_Throwable]` — el campo `hasBP=0` indicará si MulticastLaunch llegó pre-BeginPlay (el defer debería actuar).
- [ ] **Q4-08 (feel)** — Patear la bola: rueda un trecho, decelera visiblemente y se detiene. No rueda eternamente; no se pega al suelo. Al pararse, entra en dormancy (verificable con `stat net` observando los OutBytes/s de la bola bajar a ≈0).
- [ ] **Q4-08 (damping)** — Ajustar en BP `LinearDamping=0.6` y volver a patear: la bola decelera más rápido. Con `LinearDamping=0.0` rueda largo. (Confirma que el UPROPERTY se aplica al BodyInstance.)
- [ ] **Q3-04 (shrink)** — Spawnear una caca: conforme cae, el mesh disminuye linealmente de tamaño, llegando a `MinMeshScaleFactor × initial` al impactar. La sombra sigue encogiendo en paralelo.
- [ ] **Q3-04 (sombras)** — Una vez el artista asigne el material circular al decal en `BP_SeagullDropping` y `BP_EnemySeagull`, la sombra se ve redonda (no cuboide). *(Asset-work; C++ no bloquea este ticket.)*
- [ ] **Q1-12 (Reversible)** — BP_BreakablePlatform con `BreakMode=Reversible`: dos jugadores suben; antes de que rompa, uno sale → el shake se cancela, el puente no rompe. Comportamiento idéntico al pre-fix.
- [ ] **Q1-12 (Latched)** — BP_BreakablePlatform con `BreakMode=Latched`: dos jugadores suben, uno se baja antes del shake → el puente **igualmente** cae al completarse `TimeToBreak`. El shake sigue ejecutándose.

---

*QA Testing · Tortunabo · Última actualización: 2026-04-19 (batch cierre 9 tickets: Q3-01, Q3-02, Q3-03, Q1-11, Q2-02, Q2-03, Q2-04, Q2-05, Q4-06)*
