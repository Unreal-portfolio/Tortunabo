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
**Estado:** ⬜ Abierto — followup de Q1-01
**Origen:** Testing 2026-04-18

**Observación:**
Tras el fix de Q1-01 (`beb9b53`) el tilt 180° ya se aplica, pero el jugador **sigue viéndose vertical y con errores visuales** al recibir knockdown. Posibles causas:
- El emote de knockdown sobreescribe la rotación relativa del componente durante el blend.
- El hueso sobre el que aplicamos tilt (`KnockdownComponentName = "Cuerpo"`) puede no ser el correcto en el BP actual.
- La rotación se aplica relativa al mesh pero el mesh tiene su propia rotación por `RelativeRotation` (-90º típico) — componer mal da resultados raros.

**Acción:**
- Grabar un PIE con 2 players: uno sufre knockdown, otro observa. Comparar con animación objetivo (tumbado panza arriba).
- Considerar **ragdoll físico breve** como alternativa: `GetMesh()->SetSimulatePhysics(true)` durante la duración, y al recuperar volver a `Animation`. Coste de replicación aceptable para eventos puntuales.
- Alternativa simple: animación de knockdown dedicada en lugar de pitch por código — el anim blueprint controla la pose completa.

---

### Q1-08 🟠 BreakablePlatform — bridge no se elimina
**Componente:** `ATN_BreakablePlatform` (uso cooperativo / puente)
**Estado:** ⬜ Abierto
**Origen:** Testing 2026-04-18

**Observación:**
El bridge (BreakablePlatform con `PlayerThreshold=2+`) **no se rompe** durante el test, incluso tras configurar la variable de respawn y sus subparámetros.

**Causas posibles:**
- Testeando con 1 solo player y `PlayerThreshold=2` → el timer nunca arranca (comportamiento correcto, pero confuso en testing).
- El StandTrigger del BP del bridge no está bien dimensionado y los pawns no generan overlap.
- `PawnsOnPlatform` no cuenta bien si ambos jugadores entran simultáneamente (con el `Remove(nullptr)` previo se filtran los weak refs).

**Acción:**
- Verificar en PIE con 2 players simultáneamente encima.
- Añadir un `UE_LOG` temporal en `OnStandTriggerBeginOverlap` mostrando `PawnsOnPlatform.Num()` y `PlayerThreshold`.
- Confirmar que el BP hijo del bridge no pisa `PlayerThreshold=1` ni el StandTrigger.

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
**Estado:** ⬜ Abierto
**Origen:** Testing 2026-04-18

**Observación:**
Al forzar al objeto contra ciertas esquinas geometría+geometría, entra en un estado inestable (oscila, vibra o queda trabado). Propuesta del usuario: **bloquear rotación en el eje vertical** para evitar que el cubo rote sobre sí mismo en Z y caiga en configuraciones imposibles.

**Acción:**
- En el ctor: `Mesh->BodyInstance.bLockZRotation = true;` (o `SetConstraintMode(EDOFMode::SixDOF)` + lockar rotaciones X/Y/Z según el eje de pie).
- Alternativa más conservadora: `Mesh->BodyInstance.bLockXRotation = true` + `bLockYRotation = true` (mantener solo yaw). Evita que caiga de lado.
- Tuning a ojo en PIE tras probar cada combinación.

---

## Fase 2 — Interactables

### Q2-01 🟠 Trampa — al pillar, vuelve a pickup
**Componente:** Trap/Trampa (BP pickup-to-deploy)
**Estado:** ⬜ Abierto
**Origen:** Testing 2026-04-18

**Observación:**
Cuando la trampa desplegada pilla a un jugador, en vez de destruirse o quedarse consumida, **vuelve a convertirse en un pickup** recolectable. Efectivamente da usos infinitos a la trampa y rompe el balance del item.

**Acción:**
- Localizar el BP/actor de la trampa (probable `ATN_ThrowableItemActor` en modo trap, o derivado de `ATN_PickupInteractableBase`).
- Tras `OnTrigger`: `Destroy()` (o `SetLifeSpan(0.5f)` para que el multicast de efectos se entregue antes).
- Confirmar que no hay un `Spawn` de pickup en la cadena de callbacks (OnRep, etc.).

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
**Componente:** proyectil tirable (probable `ATN_ThrowableItemActor` o equivalente del sistema de throwables).
**Estado:** ⬜ Abierto

**Observación:** cuando el **host** lanza la bola, todos ven la trayectoria correctamente. Cuando el **cliente** lanza la bola, el cliente no ve el proyectil en vuelo — sólo aparece al rebotar/aterrizar como pickup. El host sí ve el proyectil del cliente.

**Causas probables:**
- Spawn del proyectil sólo ocurre en el servidor (OK) pero el actor no está marcado `bReplicates = true` en el ctor.
- Falta `SetReplicateMovement(true)` o `bAlwaysRelevant = true` → el cliente que lanzó no está dentro del radio de relevancia mientras el proyectil se mueve.
- El proyectil se oculta localmente en el lanzador (anti-clip visual) pero no se vuelve a mostrar hasta el impacto.

**Acción sugerida:** auditar ctor del throwable — debe replicar movimiento como `ATN_InkProjectile`:
```cpp
bReplicates = true;
bAlwaysRelevant = true; // o garantizar relevancy por distancia
SetReplicateMovement(true);
```
Si se oculta localmente al lanzar (para no chocar con la cámara), añadir un timer que lo muestre de nuevo tras 0.1–0.2s.

---

### Q4-03 🟠 BananaPeel — no desliza hacia delante al pisarla
**Componente:** `ATN_BananaPeel`
**Estado:** ⬜ Abierto

**Observación:** pisar la cáscara dispara el knockdown correctamente, pero el personaje **no desliza** — se queda caído en el sitio como si `SlideImpulse = 0`.

**Causa probable:** en `ATortugaCharacter::ApplyKnockdown`, si `ImpulseOverride.IsZero()` (default del BP de BananaPeel) el impulso se calcula desde la velocidad actual. Si el jugador venía quieto o casi quieto, el resultado es un impulso insignificante y no desliza.

**Acción sugerida:**
1. Configurar `SlideImpulse` con un forward-vector fuerte en `BP_BananaPeel` (ej. `(800, 0, 200)` en local space del peel, transformado a world en C++).
2. Fallback en `ApplyKnockdown`: si `ImpulseOverride.IsZero() && Velocity.Size() < UmbralMin`, aplicar impulso default en `GetActorForwardVector() * DefaultSlideSpeed`.

---

## Fase 5 — Especiales (Jellyfish, Storm, ScriptedDeathZone)

*(pendiente de testing)*

---

## Historial de resoluciones

| ID | Fecha | Commit | Resumen |
|---|---|---|---|
| Q4-01 | 2026-04-18 | `635377e` | InkProjectile parabólico (`GravityAcceleration`) |
| Q1-03 | 2026-04-18 | `65446f6` | BreakablePlatform shake antes de romperse |
| Q1-02 | 2026-04-18 | `c3a0a12` | BreakablePlatform logs de respawn |
| Q1-05 | 2026-04-18 | `dc21d12` | PhysicsObject CCD anti-tunneling |
| Q1-01 | 2026-04-18 | `beb9b53` | Knockdown tilt 180° sobre emote |
| Q1-04 | 2026-04-18 | `9884bbd` | Quicksand rediseño (slow + DeathZone) |

---

*QA Testing · Tortunabo · Última actualización: 2026-04-18 (drop zone procesada)*
