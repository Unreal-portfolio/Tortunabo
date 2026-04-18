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
**Estado:** ⬜ Abierto
**Origen:** Decisión 2026-04-18 (invalida Q1-04)

**Decisión:**
Tras el rediseño de Q1-04 el Quicksand quedó funcionalmente muy cerca del SlowZone. En vez de mantener dos actores paralelos, **eliminar `ATN_QuicksandVolume` por completo** y crear una **copia/variante del SlowZone** con slow mucho más agresivo ("atascándote muchos más") — mismo patrón, parámetros más duros.

**Acción:**
- Crear `ATN_SlowZoneStickyVolume` (o exponer `SlowMultiplier` muy bajo como parámetro en `ATN_SlowZoneVolume` y usar un BP hijo "BP_SlowZoneSticky").
- Sustituir usos de `BP_QuicksandVolume` en niveles por el nuevo BP.
- Eliminar `TN_QuicksandVolume.{h,cpp}` del módulo una vez migrado.
- La muerte, si se quiere, sigue el mismo patrón compuesto (DeathZone a ras de suelo).

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
**Componente:** `ATN_BananaPeel`
**Estado:** ✅ Resuelto — pendiente de re-test en PIE

**Causa raíz:**
1. El fallback para jugador **parado** usaba `-ActorForwardVector` → el impulso iba hacia **atrás**, no hacia delante como espera el usuario.
2. La componente Z era `200.f` hardcoded — muy baja. LaunchCharacter sí entraba en MOVE_Falling, pero el arco duraba ~0.4 s. Al aterrizar, la fricción del suelo mataba el deslizamiento en 0.2 s más → el slide quedaba en ~2 m, casi invisible dentro de la animación del knockdown.

**Fix aplicado:**
- Fallback estacionario ahora usa `+ActorForwardVector` (resbalón cartoon clásico — los pies van hacia delante, el cuerpo cae hacia atrás a partir del tilt -180° del knockdown).
- Nueva `UPROPERTY SlideVerticalForce` (default 400) — sustituye el hardcode Z=200. Tunable desde `BP_BananaPeel` sin recompilar. 400 da ~0.8 s de vuelo → slide visible de ~4–7 m según la velocidad previa.
- Log añadido para diagnosticar dirección e intensidad desde el output log.

---

## Fase 5 — Especiales (Jellyfish, Storm, ScriptedDeathZone)

*(pendiente de testing)*

---

## Historial de resoluciones

| ID | Fecha | Commit | Resumen |
|---|---|---|---|
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

*QA Testing · Tortunabo · Última actualización: 2026-04-18 (drop zone procesada)*
