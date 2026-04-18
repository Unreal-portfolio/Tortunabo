# QA Testing — Tortunabo

Registro de bugs, mejoras y decisiones detectadas durante testing en PIE.

---

## ✍️ Zona de volcado

> Pega aquí texto libre. En la siguiente sesión lo formateo, clasifico por severidad y lo muevo a la sección correspondiente.

```
<!-- DROP_ZONE: pegar notas crudas debajo de esta línea -->
A veces, tras caer con el segundo salto, se puede ocurrir que haya un pequeño clip y que el jugador al levantarse este pegado al suelo, revisar

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

## Fase 1 — Hazards pasivos

### Q1-01 🔴 Knockdown — falta rotación simulada del cuerpo
**Componente:** `TortugaCharacter::ApplyKnockdownVisual` (cross-cutting, afecta a todas las fuentes de knockdown)
**Estado:** ⬜ Abierto

**Observación:**
Cuando un jugador recibe knockdown, el player agita los brazos (animación) pero **no se aplica la rotación simulada** al cuerpo. Se ve "flotando" en vez de tumbado.

**Causas posibles a revisar:**
- `SetRelativeRotation(180°)` puede estar siendo pisado por la animación activa en el Anim Blueprint.
- El `NetworkSmoothingMode` — recordar que se desactiva durante knockdown; si algo lo reactiva, el CMC pisa la rotación.
- Puede que el hueso "Cuerpo" (`KnockdownComponentName`) no tenga el nombre correcto en el BP.

**Acción:**
- Revisar `ApplyKnockdownVisual` y confirmar que la rotación se aplica al **Mesh** o al componente correcto, no al RootComponent.
- Alternativa: ragdoll físico breve durante el knockdown (más realista, pero coste de replicación).

---

### Q1-02 🟠 BreakablePlatform — respawn no observado
**Componente:** `ATN_BreakablePlatform`
**Estado:** ⬜ Abierto

**Observación:**
La plataforma rompe correctamente cuando el jugador se queda encima el tiempo configurado, **pero no se ha visto respawnear** durante el test.

**Verificar:**
- ¿`bRespawns=true` en la instancia testeada?
- ¿`RespawnDelay` tiene valor? Si es 0 → posible edge case.
- ¿El timer de respawn se dispara en server? (`GetWorld()->GetTimerManager()`)

---

### Q1-03 🟡 BreakablePlatform — falta feedback visual previo a romperse
**Componente:** `ATN_BreakablePlatform`
**Estado:** ⬜ Abierto

**Mejora propuesta:**
Antes de romperse, la plataforma debería **agitarse** (temblor visible) para dar telegrafía al jugador.

**Implementación sugerida:**
- Añadir `Timeline` o `InterpTo` en los últimos 0.5s antes del break que aplique una oscilación pequeña en `RelativeLocation` (ej. ±3uu en Z).
- Opcional: spawnear partículas de polvo/crujido + sonido de madera cediendo.

---

### Q1-04 🔵 Quicksand — rediseño: suelo-como-DeathZone + suelo sólido
**Componente:** `ATN_QuicksandVolume`
**Estado:** ⬜ Abierto

**Contexto:**
Durante el test de arenas movedizas:
- Caigo **demasiado rápido** — debería ser muy, muy lento para que dé tiempo a pedir rescate.
- Los saltos **no ayudan** a salir — y probablemente no deberían, pero el feel actual es que no tiene salida ni tiempo de reacción.

**Decisión de diseño:**
Reemplazar la lógica actual de hundimiento-hasta-morir por un patrón compuesto:
1. **`SlowZoneVolume`** a ras de suelo → ralentiza al jugador (ya existe).
2. **`DeathZoneVolume` como suelo** → mata por countdown estándar (3-5s, ajustable).
3. **Suelo físico sólido** debajo para que el pawn no siga cayendo indefinidamente — solo se queda "atrapado" visualmente.

**Beneficio:** se elimina el código de hundimiento progresivo (`SinkTime`, `SinkDepth`) y se reutilizan dos sistemas ya probados.

**Acción:**
- Evaluar si mantener `ATN_QuicksandVolume` como actor compuesto (que internamente tenga ambos volúmenes + static mesh del suelo) o deprecarlo del todo.
- Si se mantiene: simplificar el `.cpp` eliminando la lógica de hundimiento.

---

### Q1-05 🟠 PhysicsObject — atraviesa muros bajo presión alta
**Componente:** `ATN_PhysicsObjectActor`
**Estado:** ⬜ Abierto

**Observación:**
Si el jugador empuja el objeto contra un muro y mantiene la presión, el objeto **atraviesa la geometría** (collision tunneling clásico a alta velocidad simulada).

**Propuesta de diseño:**
Cuando el objeto sufra **presión excesiva** entre el jugador y un collider estático:
- Detectar la compresión (overlap simultáneo jugador ↔ objeto ↔ muro, o stress de constraint).
- Destruir el objeto con "poof" → partículas + sonido + `Destroy()`.

**Implementación sugerida:**
- En Tick (o en `OnHit` con cooldown), comprobar si el objeto está atrapado entre `PhysicsObject_Player_Contact` y otro `Static/WorldStatic` con normales opuestas.
- Si lo está durante > `CrushTimeThreshold` (ej. 0.3s) → `MulticastPoofAndDestroy()`.

**Alternativas:**
- Subir `CCD` (Continuous Collision Detection) en el BP para reducir tunneling (solución barata si el bug no es frecuente).
- Bajar `MaxDepenetrationVelocity` en el componente físico.

---

### Q1-06 🟡 PhysicsObject — masa 200000 se mueve con facilidad (informativo)
**Componente:** `ATN_PhysicsObjectActor`
**Estado:** 🟦 No acción — nota informativa

**Observación:**
Con `Mass=200000` el jugador aún puede empujarlo con facilidad. No es bug en el sentido estricto (el pawn del Character usa Launch/impulse, no simulación realista masa a masa), pero **documentado por si en balance se quiere un objeto "inamovible"**.

**Si se quisiera arreglar:**
- Usar `bLockXAxis=true` sobre eje ancho.
- Usar `SetMassOverrideInKg` muy alto + `MaxAngularVelocity=0`.
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
**Estado:** ⬜ Abierto

**Observación:**
La tinta del calamar actualmente viaja en **línea recta** hasta impactar o agotar su lifetime. Debería describir una **parábola** — más orgánico, más coherente con "escupir tinta", y da al objetivo opción de leer la trayectoria y esquivar.

**Implementación sugerida:**
- Configurar el `ProjectileMovementComponent`:
  - `ProjectileGravityScale = 1.0` (o valor ajustable en UPROPERTY para tuning)
  - `InitialSpeed` adecuado para que el arco natural llegue a la distancia típica de tiro
  - `bShouldBounce = false` (la tinta al tocar suelo mancha, no rebota)
- Alternativa si se quiere más control: `Velocity = ForwardVector * InitialSpeed + UpVector * ArcBoost` al spawnear.
- Tuning sugerido por defecto: `InitialSpeed=1800`, `ProjectileGravityScale=1.2` — ajustar en BP sin recompilar.

**Consideraciones multiplayer:**
- El `ProjectileMovementComponent` simula determinísticamente en cliente y server si se replica `Velocity` inicial → no debería requerir ajustes extra de replicación.
- Si se ve desincronización visible, replicar `InitialVelocity` en el `PostInitializeComponents`.

---

## Fase 5 — Especiales (Jellyfish, Storm, ScriptedDeathZone)

*(pendiente de testing)*

---

## Historial de resoluciones

*(vacío — añadir entradas al cerrar bugs con formato `Q1-XX | fecha | commit`)*

---

*QA Testing · Tortunabo · Última actualización: 2026-04-18*
