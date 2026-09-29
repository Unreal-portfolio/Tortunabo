# G · Doble salto físico (lanzar, pendiente, estampado)

UE 5.6 · `main` 41a0ee8a5 · solo lectura, sin runtime. Rutas relativas a `Source/Tortunabo/`.

## 0. Conclusión

El doble salto **es** el panzazo (Dive). De las tres peticiones: (1) ya existe; (2) existe la física pero el ajuste la anula en arena, que es todo el mapa; (3) no existe en vuelo: contra una pared la tortuga resbala y se para.

## 1. Estado actual

**Doble salto = Dive.** `ATortugaCharacter::Jump` (`Private/Player/TortugaCharacter.cpp:1121`): si `IsFalling()` llama a `TryDive()` (:1142-1146). `OnJumped` (:1103) guarda `JumpStartHorizontalVelocity` para el bonus de inercia.

**Dive** (`Private/Player/TortugaCharacter_Dive.cpp`):
- `TryDive` (:71): solo en el dueño; dirección = WASD o cámara (cambio de Rubén de hoy, a9fd09892). Solo envía `Server_StartDive` (Reliable, WithValidation :108).
- `Server_StartDive_Implementation` (:116): velocidad = `DiveDir·(DiveForwardSpeed + bonus)` limitada a `DiveMaxTotalSpeed` + Z `-DiveDownwardSpeed`; `LaunchCharacter` **solo en el servidor** (:207); `DiveSerial++`, `bIsDiving` (replicados, TortugaCharacter.cpp:1639-1640) y `Multicast_OnDiveVisual` (:216, cápsula encogida + suavizado desactivado).
- **Hallazgo de red:** el impulso del dive no se predice. El cliente dueño no hace `LaunchCharacter` en local; lo recibe como corrección (`ClientAdjustPosition`) tras un RTT. Con 100-150 ms de ping se nota como tirón. Ya existe `FTNTurtleNetworkMoveDataContainer` (TN_TurtleMovementComponent.h:31), que es donde encajaría la predicción.
- `TickDive` (:324): fin en el servidor al levantarse, al entrar en el agua, con `MOVE_None`, en caparazón o parado en el aire (`Speed2D ≤ DiveStopSpeedThreshold` tras `DiveMinLockDuration`, :515).

**Arrastre sobre la tripa** (`Private/Player/TN_TurtleMovementComponent.cpp`), predicho con `FTNSavedMove_Turtle` (fase, tiempo, serial y cápsula por movimiento):
- `ProcessLanded` (:447) → `StartBellySlide` → `RedirectAlongFloor` (:368): quita la componente normal, **aplana a XY** (se pierde un `cos θ` de velocidad en bajada), ×`BellyLandingKeep` 0,9 y tope `BellyMaxEntrySpeed` 850.
- `CalcBellySlideVelocity` (:515): `a = g·1,15·senθ·cosθ − μ_superficie·cosθ − 1,5·v`, subpasos de 1/60 s, rampa de rozamiento desde 1,2 s y tope `BellyMaxSeconds` 2,6 s.
- Cuenta: con μ arena = 800, solo acelera si `senθ > 800/1127` (θ > 45,2°), que ya no es suelo caminable (≈44,8°). **En arena nunca desliza cuesta abajo por sí sola**; se para en unos 0,57 s. En roca con θ = 35°, la velocidad terminal es de unos 134 cm/s por el `BellyDrag`.
- Rebote: `HandleImpact` (:682) + `OnMovementUpdated` (:702), **solo en fase Slide y en el suelo**. En el vuelo del dive no hay rebote ni estampado.

**Lanzar al agarrado:** ya hecho. `Server_StartDive` llama a `CarryComponent->ThrowWithDive` (Dive.cpp:204 → TN_CarryComponent.cpp:266). Suma el lanzamiento base (~25°, `GetThrowDirection`), la parte horizontal del dive ×`DiveThrowCarryFactor` 0,6 y la Z del salto ×0,5, con tope de 2300. El agarrado siempre va en caparazón (`CanBeGrabbed` :32, `ServerGrab` :174), así que sale como `ATN_ShellBody` (física replicada con interpolación predictiva) mediante `Release` → `StartBody` (:341-377).

**Derribo** (`TortugaCharacter_Knockdown.cpp`): `ApplyKnockdown` (:33) solo en el servidor, con `LaunchCharacter` e `ImpulseOverride`. Si `bUsePhysicsRagdoll` = true (.h:1079), `ApplyKnockdownVisual` activa un ragdoll **local en cada máquina** con `SetReplicateMovement(false)`. Al recuperarse, cada máquina coloca la cápsula donde acabó su propio ragdoll (:358), y el servidor reactiva la replicación (:400). `bRagdollFrozen`/`RagdollFrozenLoc` + `ServerFreezeRagdoll` (:915) **solo existen para la muerte**. Sigue abierto el desync de posición descrito en `project_ragdoll_red`.

## 2. Diseño

Convención: `UPROPERTY(EditDefaultsOnly)` para el ajuste; sonido y FX en C++ con `SpawnSoundAtLocation`/`SpawnSystemAtLocation`, sin BIE. La lógica pura va en un `Public/Player/TN_DiveDecisions.h` nuevo (`namespace TNDiveLogic`), que llaman el componente y el personaje.

### 2.1 Lanzar al agarrado (ajuste, no mecánica nueva)
- Revisar con el director el ángulo y la fuerza: `DiveThrowCarryFactor`, `DiveThrowJumpFactor` y `DiveThrowMaxSpeed` ya son editables.
- Riesgo a comprobar: la caja lanzada nace 70 cm delante y 140 cm arriba mientras la portadora sale en panzazo en la misma dirección. Hay que confirmar que la cápsula de la portadora no choca con la caja; si choca, ignorar entre ambas unos 0,3 s, igual que `RestoreCollisionWith`.

### 2.2 Pendiente: «sigue cayendo»
Todo dentro del CMC, predicho como hasta ahora:
- `BellySlopeMinAngle` (12°): a partir de ahí, el rozamiento se multiplica por `BellySlopeFrictionScale` (0,3) y el freno por `BellySlopeDragScale` (0,4).
- Rampa de rozamiento y `BellyMaxSeconds` **pausados mientras la aceleración a lo largo de la pendiente sea mayor que 0** (`BellyTime` solo corre en llano o en subida). Tope de seguridad aparte: `BellySlopeMaxSeconds` (6 s).
- `RedirectAlongFloor` conserva el módulo 3D (`|Along|` en lugar de `|Along.XY|`) y usa un tope de entrada propio en bajada (`BellyMaxEntrySpeedDownhill` 1000).
- Pendiente no caminable: el CMC ya está en `Falling` y resbala. Se deja así; `TickFallRules` (TortugaCharacter.cpp:1565) la convierte en bola al bajar más de 500 cm, que es coherente con «a pura física».
- Seguridad de red: no hace falta estado nuevo en el SavedMove. Las CVars nuevas deben ser `ECVF_Cheat` e iguales en todas las máquinas.

### 2.3 Estampado contra pared
Se reparte en dos capas:
1. **Rebote predicho (CMC).** Se amplía `HandleImpact` a la fase de vuelo del dive (`IsDiving`, `IsFalling`, fase `None`, sin repetición). Cuenta como pared una normal con `N.Z < DiveWallMaxNormalZ` (0,35), para no confundirla con una pendiente. La velocidad de impacto es `Into = −(V − V_otro)·N_plano`: es relativa, así sirve para un buggy u objetos móviles. Si `Into ≥ BellyBounceMinSpeed`, se aplica un reflejo con `DiveWallRestitution` (0,45) y `DiveWallTangentKeep` (0,6). Lo simulan el dueño y el servidor por igual, así que no hay desync.
2. **Estampado autoritativo (servidor).** Si `Into ≥ DiveSplatMinSpeed` (650 cm/s) y el árbitro lo permite, el CMC del servidor apunta `PendingSplat{Normal, Point, Vel}`. **No se crean actores dentro del ServerMove**: lo consume `TickDive`, que llama a `ATortugaCharacter::ServerDiveSplat`. Pasos:
   - Comprobar el árbitro (`TNBeach::ResolveMover`/`CanStunOver`, TN_BeachStun.h:69/117). Si la mueve Launch, Held, StormKick, SafetyNet o Eaten, no hay estampado.
   - `EndDive()`, `ShellComponent->ForceEnterShell(false)` y `StartBody(Reflejada·DiveSplatKeep, true, /*bExitOnRest*/true)`. Es la caja física replicada que ya usan la caída larga, los lanzamientos y la catapulta: posición idéntica en todas las máquinas.
   - `Multicast_DiveSplatFX(Point, Normal)`: `DiveSplatSound`, `DiveSplatFX` (Niagara), aplastado cosmético de la malla durante `DiveSplatSquashSeconds` (0,25 s) y sacudida de cámara solo para el dueño. Opcional: pajaritos (`DizzyBirds`) mientras dure la bola.
   - **Descartado:** usar el ragdoll del derribo. Simula en local y desincroniza la posición, lo que incumple la decisión de Rodrigo del 22-09. Solo sería válido tras el arreglo de congelado autoritativo del derribo.

Filtro de superficie: canal `WorldStatic`/`WorldDynamic`. Nunca `APawn`, `ATN_ShellBody` ni actores con la etiqueta `TN_NoSplat` (trampolín, vallas blandas).

Coste en el dueño: el rebote es inmediato (predicho) y la bola llega por réplica (~RTT/2). Es aceptable.

## 3. Interacciones

- **Caparazón:** `TryDive` y `Jump` están bloqueados en caparazón. `CanEnterShell` rechaza `bIsDiving`, por eso hay que llamar a `EndDive` antes de forzar la bola (`ForceEnterShell` no pasa por `CanEnterShell`). `TickDive` ya cierra el dive si entra en caparazón.
- **Portadora que se estampa:** `ThrowWithDive` suelta al agarrado antes de lanzarse, así que no se puede estampar llevando a otra.
- **Agarrado:** no puede saltar (`Jump`, :1124), así que no aplica.
- **Gaviotas:** no eligen `IsBellyPoseActive()` ni `IsInShell()` (TN_BeachGullZone.cpp:1154). Ni el dive ni el estampado son agarrables, y es coherente. `TN_BeachEnemy` (:1211) desactiva el movimiento: el árbitro Held bloquea el estampado.
- **Trampolín / catapulta:** la catapulta reserva `Launch` en el árbitro, que manda sobre el estampado. Una tortuga que salta del trampolín y hace el dive puede superar 650 cm/s: es deseable que se estampe, pero hay que ajustarlo en playtest.
- **Agua:** `ProcessLanded` no arranca el arrastre en el agua y `TickDive` termina al nadar. La caja en el agua sale y nada (`ATN_ShellBody::ServerChecks`). No hace falta nada nuevo.
- **Zonas de muerte:** la bola cuenta como jugador. Más deslizamiento en cuesta implica más caídas por bordes (`FatalFallHeight` 3500, `AutoShellFallHeight` 500). El estampado no mata nunca.
- **Buggy futuro:** usar la velocidad relativa y la base del CMC; si está montada o se apoya en un vehículo, no se estampa. Reservar un `ETNBeachMover::Vehicle`.

## 4. Tests y tareas

`Public/Player/TN_DiveDecisions.h` + `Private/Tests/TN_DiveDecisionsTest.cpp` (`Tortunabo.Dive.*`):
- `StepBellyVelocity(V, N, μ, drag, params, dt)`: llano en arena parada en menos de 0,8 s (regresión); caso negativo: con los valores actuales, arena a 30° no acelera; arena a 25° con los parámetros nuevos sigue a más de 200 cm/s tras 2 s; subida a 20° frena antes que en llano.
- `ShouldAdvanceBellyTimer(SlopeAccelAlongV)`: falso en bajada.
- `LandingSlideSpeed(V, N, Keep, Cap)`: conserva el módulo 3D y respeta el tope.
- `ClassifyDiveImpact(N, V, VOther, params)`: pendiente → None; pared lenta → None; pared a 400 → Bounce; pared a 700 → Splat; objeto que se aleja a la misma velocidad → None (velocidad relativa).
- `ReflectVelocity`: la componente normal se invierte con la restitución y la tangencial se escala.
- `CanSplat(Mover)`, que reutiliza la tabla del árbitro.

| # | Tarea | h |
|---|-------|---|
| 0 | Playtest con `TN.Dive.Debug 1` y el director: confirmar que (1) le vale y qué significa «estamparse» | 1 |
| 1 | Extraer las cuentas del arrastre a `TN_DiveDecisions.h` + tests de regresión (sin cambiar el comportamiento) | 3 |
| 2 | Pendiente: rozamiento por ángulo, temporizador pausado y entrada 3D + tests | 3 + 2 de ajuste |
| 3 | Rebote en vuelo predicho en `HandleImpact`/`OnMovementUpdated` + tests | 3 |
| 4 | `PendingSplat` → `ServerDiveSplat` (árbitro, `EndDive`, `StartBody`, multicast de FX y aplastado) | 4 |
| 5 | Comprobar la colisión caja lanzada ↔ portadora en `ThrowWithDive` | 1 |
| 6 | Recomendado: predecir el impulso del dive (DiveDir en `FTNTurtleNetworkMoveData` y lanzarlo dentro de `PerformMovement`) | 6 |
| 7 | PIE con 4 jugadores y `NetEmulation` (PktLag 150, pérdida 2 %), y prueba de 8 jugadores en standalone | 2 |

Total: 19 h sin la tarea 6 y 25 h con ella.

**Riesgos:** (a) crear actores dentro del `ServerMove`: diferirlo a `TickDive`. (b) Los valores de BP pueden sobrescribir los por defecto de C++: no está verificado sin abrir `BP_Tortuga`. (c) Rubén tocó `TortugaCharacter_Dive.cpp` hoy: hay que coordinarse para evitar conflictos. (d) Deslizar más aumenta las muertes por borde y los atajos en carrera. (e) Sin la tarea 6, el tirón del dive en clientes remotos se mantiene: el estampado no lo empeora, pero tampoco cumple del todo el requisito de fluidez. (f) Cambio que afecta a la replicación y a más de 5 ficheros: toca Plan Mode.
