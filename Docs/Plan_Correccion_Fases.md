# Plan de corrección y optimización — Tortunabo

> Generado en la sesión de macro-audit (/maxdual · ULTRON + peer Codex).

## Estado (actualizado — sesión autónoma 2026-07-06)

**Todo lo de abajo está compilado en verde** (`TortunaboEditor Win64 DebugGame`) — se
verificó que el motor UE5.6 está instalado y se compiló tras cada cambio. Falta la
prueba en PIE (no ejecutable sin display); los cambios de red se validan en runtime.

Hecho y pusheado a `entrega-memoria`:
- **6 fixes de bugs** (Fase 0): sample-rate voz, grief return-to-menu, revive-borra-score,
  crash SpectateByDirection, item-loss en drop, score del host en vivo.
- **3 fixes nocturnos** (cierre de sesión 2026-07-06): ref-count de SlowZones solapadas
  (sirope, `0cabb5f`), un jugador muerto ya no mantiene una placa de presión pulsada
  (`7955a6c`), DRY del reset de PlayerState en `ResetForNewRace()` (`f7fc526`).
- **Sesión 2026-07-07**: higiene de logs Fase 1.3 (`289159d`), código muerto eliminado
  (`fe0e258`), y fix de replicación: el target de la gaviota se replica como
  `APlayerState*` en vez de índice de `PlayerArray` (`c047f2a`) — el índice apuntaba al
  jugador equivocado tras un disconnect/JIP mid-race.
- **Refactor**: `ATN_SpawnZoneBase` (dedup spawn zones).
- **Perf**: roof-check de gaviota O(mundo)→O(1).
- **Hardening**: `WithValidation` en RPCs de stamina y voz.
- **Higiene**: `.codegraph/` en gitignore.
- **God class troceado**: `TortugaCharacter.cpp` **4364 → 1448 líneas (-67%)**, repartido en
  `_Dive` / `_Knockdown` / `_Revive` / `_Emote` / `_Cosmetics` / `_Interaction` (splits de
  unidad de traducción — misma clase, sin cambios de lógica ni replicación; comportamiento
  idéntico). Cada extracción compilada en verde por separado. El archivo principal conserva
  el core: ctor, BeginPlay/Tick, input, movimiento/cámara, leg-anim, head-look, jump-anim.

- **Sesión 2026-07-10 — Fase 2 CERRADA** (4 commits, todos compilados en verde):
  stun/blind del cangrejo por timestamp (`ef3b90c`), medusa sin tick idle +
  cooldowns lazy + `DORM_DormantAll` (`6f04b95`), spawn zones vía `PlayerArray`
  (`ac0c561`), cangrejo a 25Hz + relevancy por distancia (`85c6cc7`).
  Decisiones (Rodrigo): 25Hz en vez de los 10-15 del plan (AActor no interpola
  en cliente → 10Hz = stutter visible); relevancy solo al cangrejo; rueda radial
  y punto 6 (RMS voz / HUD cache) **declinados** — ganancia ≈ 0 medida contra coste.

Pendiente (necesita PIE o decisión): extracción a UComponents
reales (Fase 3, cambia replicación → requiere PIE), pooling (declinado), tests (Fase 4).

---

## De qué va el juego (contexto arquitectónico)

Tortunabo es un **race co-op para hasta 4 jugadores** (tortugas antropomórficas) en
**UE5.6**, **listen-server + Steam** (OnlineSubsystemSteam + SteamSockets). Tres mapas
encadenados por Seamless Travel: `LVL_Menu` → `LVL_HQ` (lobby/ready-up) → `LVL_Run`
(la carrera).

**Loop de partida** (`TN_RunGameMode`): `WaitingForPlayers` → `Countdown` → `InProgress`
→ `Results` → vuelta a HQ. El nivel de carrera se genera con **streaming procedural de
chunks** (`TN_ChunkManager`, server-only, pools Easy/Medium/Hard alineados por
sockets In/Out + un único `EndTrigger` activo en el frente).

**Estado replicado**: `TN_CoopGameState` (MatchFlowState, RaceResults, QuickChat) +
`TN_CoopPlayerState` por jugador (bIsAlive, bIsDBNO, RaceScore, cosméticos). Autoridad
en el listen-server. Un patrón recurrente y delicado: **OnRep no dispara en el host**,
así que el código difunde delegates a mano en varios sitios (fuente de bugs — ver Fase 0).

**Sistemas**: pawn `TortugaCharacter` (movimiento, dive, knockdown/ragdoll, emotes,
revive/DBNO, cosméticos, inventario 2-slots), enemigos (cangrejo, gaviota, medusa),
hazards (death/slow/storm zones), items (concha, tinta, plátano, lanzables), scoring
(pickups + zonas de colecta + bonus de meta → progresión de cosméticos), VOIP de
proximidad (`ProximityVoiceComponent`, captura WASAPI).

**Salud actual**: base sólida y con mucho cuidado en netcode, pero con **1 god class**
(`TortugaCharacter`, 4364 líneas), **acoplamiento directo a `GameMode` concreto** en
capas World/Player, y **cero cobertura de tests**. Nota interna del harness: core 9.44.

---

## Fase 0 — Correcciones ya aplicadas (esta sesión) ✅

Verificadas contra el código real; **pendientes de compilar** (`TortunaboEditor Win64
DebugGame`) y probar.

| Commit | Bug | Severidad |
|--------|-----|-----------|
| `67ef1e8` | Voz: `SenderSampleRate` de red sin acotar → crash de audio en receptores | Media |
| `06e8ae4` | Grief: cualquier cliente cerraba la sesión de todos (`ServerRequestReturnToMenu` sin gate de host) | Alta |
| `7c2d750` | Revivir a un compañero **borraba su RaceScore** (`RevivePlayer` reseteaba a 0) | Alta |
| `8c6c082` | Crash null-deref en `SpectateByDirection` (GameState null durante travel) | Media |
| `b111365` | Dropear ítem con `PickupActorClass` null / spawn fallido → **ítem perdido** | Baja-Media |
| `80cb604` | HUD de score del **host** congelado (OnRep no dispara en autoridad) | Media (host) |

**Gate de salida de Fase 0**: compilar y probar en PIE 4P + Standalone loopback
(ver checklist de pruebas al final).

---

## Fase 1 — Bugs restantes confirmados (bajo riesgo, tras compilar Fase 0)

1. ~~**Persistencia de score con race de replicación**~~ **HECHO (2026-07-07, `5f5ef84`)**:
   persistencia **por delta** — `PersistedScoreThisRace` acumula lo ya guardado y cada
   llamada suma solo la diferencia; `OnRep_RaceScore` reinvoca la persistencia, así que si
   `Results` llega antes que el último update del score, el delta pendiente se guarda al
   replicar. Cubre también el caso de score *parcial* (que el guard `>0` propuesto
   originalmente no cubría) y es idempotente en el host (broadcast manual + OnRep).
   *Verificación pendiente en PIE con `Net PktLag` (checklist).*

2. ~~**Consistencia de `ITN_EnemyTargetInterface`**~~ **RESUELTO (2026-07-07)**: Rodrigo
   confirma que gaviota y medusa **no tienen vida por diseño** — la inmunidad a
   tinta/lanzables es intencional. Solo el cangrejo es golpeable. No hay trabajo pendiente.

3. ~~**Higiene de logs**~~ **HECHO (2026-07-07)**: los logs per-spawn de las spawn zones ya
   estaban en `Verbose` (bajados con el refactor `ATN_SpawnZoneBase`); se bajaron los dos
   restantes: init per-spawn de `TN_EnemySeagull` y cambio de fuente de stamina en
   `TN_PlayerHUDWidget`. Compilado en verde (DebugGame).

---

## Fase 2 — Optimización y fluidez (object pooling + hot paths) ✅ CERRADA (2026-07-10)

> Ganancia real **media** (los spawns están gated por timer, no es bullet-hell), pero es lo
> que pediste explícitamente y además elimina duplicación.
>
> **Estado final**: 2 (`ac0c561`, vía `PlayerArray` — más simple que overlap-list),
> 3 (roof-check O(1), sesión anterior), 7 (gaviota `b50e5b1` + cangrejo `ef3b90c`),
> 8 (medusa `6f04b95`, cangrejo 25Hz `85c6cc7`), 9 (medusa `6f04b95`) → **HECHOS**.
> 1 (pooling), 4 (rueda radial), 6 (RMS/HUD), tick-interval del cangrejo (parte de 9)
> → **DECLINADOS** con decisión razonada. 5 → hecho **solo cangrejo** (decisión).

1. **`ATN_SpawnZoneBase` + object pooling** (la petición de "pool en vez de crear/destruir").
   - Candidatos de churn: `TN_SeagullDroppingActor` (por caída) y `TN_EnemySeagull` (por spawn).
   - Pool pre-spawneado (4-8 instancias) reusado con `SetActorHiddenInGame` +
     `SetActorEnableCollision` + un `ResetState()` explícito, en vez de `SpawnActor`/`Destroy`.
   - **Beneficio doble**: elimina el **copy-paste drift** ya detectado entre
     `TN_SeagullSpawnZone` y `TN_DroppingSpawnZone` (`GetPlayersInsideZone`/`ClampXYToVolume`
     duplicados con divergencia iniciada) al mover lo común a la base.

2. **Detección de jugadores por overlap, no por iteración**. `GetPlayersInsideZone` recorre
   `TActorIterator<ATortugaCharacter>` (todos los actores) en cada disparo de timer.
   Cambiar a lista mantenida por `OnComponentBeginOverlap`/`EndOverlap` del volumen.

3. **`HasRoofBetweenSeagullAndTarget`** itera **todas** las cacas y physics objects del mundo
   cada `RoofCheckInterval` (0.25s) **por gaviota**, solo para construir la ignore-list del
   trace. Con varias gaviotas + muchos objetos es O(gaviotas × actores). Cambiar a un canal
   de colisión dedicado o a `QueryParams` con object-type filtrado en vez de iterar.

4. **Timer de rueda radial a 1/60s** (`MP_GamePlayerController`) → dirigido por evento de input
   en vez de polling a 60Hz mientras la rueda está abierta.

5. **Revisar `bAlwaysRelevant = true`** en gaviota/cangrejo/collection-zone/physics-object: en
   un nivel con streaming largo, replican a todos sin cull por distancia. Evaluar relevancy
   por distancia si el tráfico de red crece con muchos actores.

6. **(Opcional, bajo)** RMS de voz incremental y HUD `FindComponentByClass` cacheado por
   ViewTarget — impacto real pequeño; solo si se perfila un cuello.

**Añadido — audit de software de enemigos (2026-07-07):**

7. **Timers replicados → timestamps**. `CountdownRemaining` (gaviota) y
   `StunRemaining`/`BlindRemaining` (cangrejo) son floats que cambian cada tick y están
   replicados → updates continuos por actor. Replicar una vez el instante de inicio
   (`GetServerWorldTimeSeconds()`) + duración; el cliente deriva el valor localmente.
   Menos tráfico y countdown más suave en clientes.
8. **Dormancy + NetUpdateFrequency de enemigos**. Medusa estática tras replicar
   `InitialLocation` → `DORM_DormantAll` + `FlushNetDormancy()` al rebotar (patrón de la
   bola). Cangrejo/medusa a 10-15 Hz de NetUpdateFrequency (default 100). Se suma al
   punto 5 (`bAlwaysRelevant`).
9. **Ticks innecesarios**. Medusa: tick solo durante el squish (`SetActorTickEnabled`) y
   cooldowns lazy por timestamp (verificado: el TMap sí purga entradas al expirar — no hay
   leak). Cangrejo: `SetActorTickInterval(0.1f)` en Patrol, intervalo completo solo en
   Chase/Attack.

---

## Fase 3 — Arquitectura / mantenibilidad (mayor esfuerzo, compilación incremental)

> **Replication-critical** → Plan Mode + compilar tras cada extracción. Orden por
> (beneficio / riesgo).

1. **Extraer `UReviveChannelComponent`** de `TortugaCharacter` (revive/DBNO channel). Sin
   estado replicado propio → **riesgo mínimo**, buen primer corte del god class.
2. **Extraer `UEmoteComponent`** (13 métodos de emote + audio). Riesgo medio (toca
   `ReplicatedEmoteIndex`).
3. **Extraer `UKnockdownComponent`** (knockdown/ragdoll). Riesgo mayor: rewiring de
   replicación.
4. **`ITN_GameFlowInterface`** para desacoplar las 6 clases de World/ (+ TortugaCharacter) que
   llaman `GetAuthGameMode<ATN_RunGameMode>()` directo. Testabilidad y menos rigidez.
5. **`ITN_PlayerCharacter`** para sustituir los 30+ `Cast<ATortugaCharacter>` repartidos por
   World/Core/UI.
6. Marcar `ATN_SeagullActor` como `UCLASS(Deprecated)` y eliminarlo tras confirmar que no
   queda referenciado en niveles.
7. **FSM explícita en la gaviota** (audit 2026-07-07): el estado vive en 6 flags sueltos
   (`bIsStriking`, `bIsRetreating`, `bStrikeGoingDown`, `bAttackResolved`…) → estados
   ilegales representables. Un `ETNSeagullState {Following, Striking, Retreating}` como
   el `ETNCrabState` del cangrejo: mismo comportamiento, replicable para anim/VFX cliente.
8. **Dedup del patrón "actor spawneado en chunk"** (audit 2026-07-07): el deferred-init +
   captura/replicación de posición inicial está copiado en SeagullActor, medusa y cangrejo
   (fue fuente de bugs). Extraer a base común o componente, como se hizo con
   `ATN_SpawnZoneBase`. Ídem los 6+ multicast `PlayXEffects` (sonido+Niagara en ubicación)
   → helper estático.

---

## Fase 4 — Hardening y robustez

1. ~~**`WithValidation`** en los Server RPC que reciben escalares del cliente~~ **HECHO
   (2026-07-10, `f9290ee`)**: los 4 RPCs restantes con parámetros del cliente —
   `ServerSetEmote` (rango [-1,255]), `ServerUpdateHeadRotation` (IsFinite; cerraba un
   hueco: NaN atravesaba el clamp), `Server_StartDive` (sin NaN, magnitud ≤10),
   `ServerSyncUnlockedHelmets` (Num ≤256 anti-flooding). Cotas generosas, patrón de la
   voz. No validados a propósito: `ServerSendQuickChat` (uint8 + catálogo), FNames de
   cosméticos (su validación útil es la fuente de confianza → punto 2), RPCs sin params.
2. **`ServerSyncUnlockedHelmets`**: no confiar en la lista de cascos que reporta el cliente;
   validar contra una fuente de confianza (Steam inventory / DLC / lista server). Cosmético,
   prioridad baja salvo economía real.
3. **Tests**: ~~el módulo no tiene **ninguna** cobertura~~ **ARRANCADO (2026-07-10,
   `95d0d00`)**: decisiones de enemigos extraídas a funciones puras en
   `TN_EnemyDecisions.h` (`TNSeagullLogic`/`TNCrabLogic`) — los actores delegan, así
   que lo testeado es lo que corre en juego. 6 tests Automation
   (`Tortunabo.Enemies.*`) verificados headless 6/6 Success. Cubren: countdown y
   radio por timestamp, decisión de `ResolveAttack`, escape timer, stun/blind
   (extensión sin acortar) y transición de Chase, con los bordes de gameplay
   (`==MinKillRadius` mata, `==AttackRadius` ataca). **Pendiente de ampliar**:
   scoring, inventario 2-slots, selección de chunk.
4. ~~**Tooling de playtest**~~ **HECHO (2026-07-07)**: categoría `LogTortunabo` propia
   (`2c1a8dc`, 288 usos migrados en 42 archivos — `log LogTortunabo Verbose` en consola) y
   CVar `TN.Enemy.Debug` (`1bcae8c`): círculo real del servidor vs círculo del cliente
   (naranja) de la gaviota, radios/FSM/stun del cangrejo, BounceZone de la medusa.
   ECVF_Cheat, no-op en Shipping.

---

## Checklist de pruebas (tras compilar cada fase)

- [ ] Voz sigue oyéndose entre jugadores en PIE 4P (fix sample-rate).
- [ ] Return-to-menu: host termina para todos; cliente remoto sale solo él (no cae la partida).
- [ ] Revivir a un compañero conserva su score (marcador no baja a 0).
- [ ] Espectar con rueda del ratón tras morir durante un travel no crashea.
- [ ] Dropear un ítem siempre deja el pickup en el suelo (o conserva el ítem si no se pudo).
- [ ] El score del **host** sube en vivo al recoger pickups / completar zonas / cruzar meta.
- [ ] Salir de una de dos zonas de sirope solapadas mantiene el slow hasta salir de ambas
  (y la velocidad se restaura al salir de la última).
- [ ] Morir sobre una placa de presión la libera (la puerta/mecanismo vuelve a su estado).
- [ ] Volver a HQ tras una carrera resetea el PlayerState completo (score, vida, DBNO).
- [ ] El círculo de la gaviota se pinta sobre el jugador correcto en clientes, también
  después de que otro jugador desconecte mid-race (fix `c047f2a`).
- [ ] El score acumulado (progresión de cosméticos) crece exactamente lo ganado en la
  carrera, también en un cliente con `Net PktLag=200` (fix `5f5ef84` — mirar el log
  `Persisted RaceScore delta=` en `LogTortunabo`).
- [ ] **Cangrejo (Fase 2)**: stun (bola) y blind (tinta) duran lo esperado en cliente y
  host, y el VFX cliente coincide (`ef3b90c` — timestamps). Si algún BP/AnimBP leía
  `StunRemaining`/`BlindRemaining` (eran BlueprintReadOnly), re-wirear a
  `GetStunRemaining()`/`GetBlindRemaining()`.
- [ ] **Cangrejo (Fase 2)**: se mueve razonablemente suave en clientes a 25Hz
  (`85c6cc7`); al viajar lejos y volver, reaparece sin quedarse invisible ni
  congelado (relevancy por distancia).
- [ ] **Medusa (Fase 2)**: rebote funciona igual (impulso + sonido + VFX + squish en
  todas las máquinas), el cooldown por jugador sigue funcionando, y un cliente JIP
  ve la medusa en su posición correcta (`6f04b95` — dormancy + tick gating).
- [ ] **Medusa (Fase 2)**: con `TN.Enemy.Debug 1` activado a mitad de partida, las
  medusas idle vuelven a pintar su BounceZone (callback del CVar).
- [ ] **Spawn zones (Fase 2)**: gaviotas y cacas siguen spawneando sobre jugadores
  dentro de la zona (`ac0c561` — PlayerArray).
- [ ] Sin regresiones en cosméticos, chunks, DBNO/revive, knockdown.

---

## Notas de método

- Auditoría con 4 agentes en paralelo (metadata/perf/arch/security) + verificación 1-a-1
  contra el código real vía codegraph + segunda pasada adversarial con Codex (peer read-only).
- Varias severidades de los agentes se **corrigieron a la baja** tras verificar (stamina ya
  clampeada, RMS de voz de bajo impacto, gaviota sin colisión = diseño). Kirkardo: no afirmar
  sin verificar.
- **Ningún cambio de Fase 0 está compilado** en este entorno (UE necesita el editor/UBT).
  Compilar en DebugGame antes de dar por bueno.
