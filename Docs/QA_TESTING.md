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

## Fase 3 — Enemies

*(pendiente de testing)*

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

*QA Testing · Tortunabo · Última actualización: 2026-04-19 (Q4-04 resuelto — guard `bLaunchApplied` en MulticastLaunch)*
