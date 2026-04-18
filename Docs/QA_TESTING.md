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

## Fase 2 — Interactables

*(pendiente de testing)*

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

*QA Testing · Tortunabo · Última actualización: 2026-04-18*
