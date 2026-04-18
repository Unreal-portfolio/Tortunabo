# QA Testing — Tortunabo

> Documento vivo. Edita libremente: rellena los campos `📝 Notas`, marca `[x]` lo verificado, sube/baja la severidad si lo ves necesario. Cada bloque es un sistema independiente — pueden testarse en cualquier orden.

**Versión del documento:** 2026-04-18
**Última build verificada:** commit `65bdb28` (compilada OK)

---

## 🔑 Leyenda

| Símbolo | Significado |
|---|---|
| `[ ]` | Pendiente de testear |
| `[x]` | Pasado |
| `[!]` | Falla (anotar en Notas) |
| `[~]` | Pasa parcial / con caveat |
| `P0` | Bloqueante — rompe el juego para todos |
| `P1` | Crítico — degrada la experiencia notablemente |
| `P2` | Pulido — molesto pero jugable |
| `LS` | Listen-Server (host) |
| `C1` | Cliente 1 (segunda máquina) |
| `C2+` | Tercer/cuarto cliente |

**Setup mínimo recomendado:** 2 máquinas físicas con Steam abierto (`SteamDevAppId=480`). PIE no replica todos los bugs reales — usar Standalone + Steam siempre que sea posible.

---

## 1. Bugs corregidos en commit `65bdb28` — Verificación de regresión

### 1.1 — Cabeza encallada en clamp ±90° tras girar mucho `[P1]`

**Qué se arregló:** `TickHeadLook` usaba acumulador de yaw. Tras varias vueltas seguidas en el mismo sentido, la cabeza se quedaba pegada al límite y había que dar el mismo número de vueltas en sentido opuesto para liberarla. Ahora usa `FRotator::NormalizeAxis`.

| # | Escenario | LS | C1 | C2+ | Resultado esperado |
|---|---|---|---|---|---|
| 1.1.1 | Girar la cámara 5 vueltas continuas a la derecha | [ ] | [ ] | [ ] | La cabeza se mantiene clamp en +90°, NO queda atascada al frenar |
| 1.1.2 | Girar 5 vueltas a la derecha y luego 1 sola a la izquierda | [ ] | [ ] | [ ] | La cabeza vuelve al centro inmediatamente |
| 1.1.3 | Cambios rápidos de dirección con cámara | [ ] | [ ] | [ ] | El head-look responde fluido sin saltos |

📝 **Notas:**
```


```

---

### 1.2 — Proyectil throwable congelado/teletransportado en clientes lejanos `[P1]`

**Qué se arregló:** `ATN_ThrowableItemActor` no era relevante para clientes lejanos. El RPC `MulticastLaunch` no llegaba y veían el item parado en origen → teletransporte al impacto. Ahora `bAlwaysRelevant=true` + `FlushNetDormancy` + `ForceNetUpdate`.

| # | Escenario | LS | C1 | C2+ | Resultado esperado |
|---|---|---|---|---|---|
| 1.2.1 | C1 lanza un throwable cerca del LS | [ ] | [ ] | — | Trayectoria suave en LS desde el origen |
| 1.2.2 | LS lanza un throwable y C2+ está en el lado opuesto del mapa (>5000 cm) | — | — | [ ] | C2+ ve la trayectoria completa, no aparece de la nada |
| 1.2.3 | Lanzamientos consecutivos rápidos | [ ] | [ ] | [ ] | Cada uno se ve en todos los clientes |

📝 **Notas:**
```


```

---

### 1.3 — Caracola se podía pisar varias veces seguidas `[P2]`

**Qué se arregló:** `bTrapUsed` no se reseteaba. Si un mismo jugador o varios la pisaban en secuencia, no aplicaba slow al segundo. Ahora hay `ResetCooldownSeconds=1.0` y `RearmTrap` la rearma.

| # | Escenario | LS | C1 | C2+ | Resultado esperado |
|---|---|---|---|---|---|
| 1.3.1 | Pisar la caracola, esperar 1.5s, pisar otra vez | [ ] | [ ] | — | Aplica slow las dos veces |
| 1.3.2 | LS pisa, C1 pisa inmediatamente después (<1s) | [ ] | [ ] | — | Solo el primero recibe el efecto (cooldown activo) |
| 1.3.3 | Configurar `ResetCooldownSeconds=0` en BP | [ ] | [ ] | — | Re-arma instantáneamente (cada pisada aplica) |

📝 **Notas:**
```


```

---

### 1.4 — Cámara atravesaba paredes en esquinas `[P1]`

**Qué se arregló:** `CameraBoom->bDoCollisionTest` podía estar a `false` en el BP. Ahora se fija explícitamente en el ctor C++ con `ProbeChannel=ECC_Camera` y `ProbeSize=14`.

| # | Escenario | LS | C1 | C2+ | Resultado esperado |
|---|---|---|---|---|---|
| 1.4.1 | Pegarse de espaldas a una pared y mirar a cámara | [ ] | [ ] | [ ] | El brazo se acorta, no atraviesa |
| 1.4.2 | Esquina interior 90° | [ ] | [ ] | [ ] | Sin clipping a través de la geometría |
| 1.4.3 | Pasar bajo techos bajos | [ ] | [ ] | [ ] | Cámara baja sin atravesar |

📝 **Notas:**
```


```

---

### 1.5 — Tinta invisible (proyectil sin mesh) `[P1]`

**Qué se arregló:** `ATN_InkProjectile` solo era visible si el BP asignaba un mesh. Ahora hay fallback a `/Engine/BasicShapes/Sphere` autoescalado a `ProjectileRadius`.

| # | Escenario | LS | C1 | C2+ | Resultado esperado |
|---|---|---|---|---|---|
| 1.5.1 | Lanzar tinta con BP_InkProjectile sin mesh asignado | [ ] | [ ] | [ ] | Esfera blanca visible volando |
| 1.5.2 | Asignar mesh custom en BP_InkProjectile | [ ] | [ ] | [ ] | Mesh custom visible (no se pisa por el fallback) |
| 1.5.3 | Cambiar `ProjectileRadius` en runtime via BP defaults | [ ] | [ ] | [ ] | El fallback se reescala automáticamente |
| 1.5.4 | Impacto sobre jugador → ink overlay | [ ] | [ ] | [ ] | Pantalla del afectado se ennegrece (efecto local) |

📝 **Notas:**
```


```

---

### 1.6 — Chunks: clientes pierden actualizaciones de hijos tras morir/respawnear `[P0]`

**Qué se arregló:** Los chunks usaban `DORM_Initial` y al cambiar ViewTarget en muerte, los `ChildActorComponent` (puertas, botones, triggers) dejaban de actualizar. Ahora `bAlwaysRelevant=true` sin DORM_Initial.

| # | Escenario | LS | C1 | C2+ | Resultado esperado |
|---|---|---|---|---|---|
| 1.6.1 | C1 muere mientras LS pulsa botón en chunk lejano | — | [ ] | — | Al respawnear C1 ve el botón pulsado/puerta abierta |
| 1.6.2 | C1 muere, espectatea a LS, vuelve a vivir, prueba un trigger no visitado | — | [ ] | — | El trigger se activa correctamente |
| 1.6.3 | LS y C1 atraviesan toda la run, comprobar persistencia de estados de chunk | [ ] | [ ] | [ ] | Sin desfases de estado |

📝 **Notas:**
```


```

---

### 1.7 — Pickup fantasma en (0,0,0) en cada partida `[P1]`

**Qué se arregló:** `TN_ItemSpawnZone` usaba `SetTimerForNextTick`, que no se cancela con `ClearAllTimersForObject`. Cuando el ChunkManager destruía un chunk temporal (cálculo de InSocket), la zone seguía spawneando un pickup en el origen del mundo. Ahora `SetTimer` con `FTimerHandle` + `EndPlay`/`ClearTimer` + sanity-check `Z<-10000`.

| # | Escenario | LS | C1 | C2+ | Resultado esperado |
|---|---|---|---|---|---|
| 1.7.1 | Iniciar run, ir al origen del mapa (0,0,0) | [ ] | — | — | NO hay pickup flotando ahí |
| 1.7.2 | Iniciar 5 runs consecutivas | [ ] | — | — | Ningún pickup fantasma en ninguna |
| 1.7.3 | Logs de Output Log buscar `[ItemSpawnZone]` | [ ] | — | — | Solo logs en zonas reales, sin warnings de chunk temporal |
| 1.7.4 | Verificar que las zonas reales sí spawean items | [ ] | [ ] | [ ] | Los pickups aparecen en las posiciones esperadas |

📝 **Notas:**
```


```

---

## 2. Networking — Patrones críticos

### 2.1 — Seamless Travel HQ → Run → HQ

| # | Escenario | LS | C1 | C2+ | Resultado esperado |
|---|---|---|---|---|---|
| 2.1.1 | Lobby completo de 4 → countdown → cinemática → run | [ ] | [ ] | [ ] | Nadie se desconecta, NetDriver vivo |
| 2.1.2 | Final de run → results → vuelta a HQ | [ ] | [ ] | [ ] | Cosméticos persisten, controllers persisten |
| 2.1.3 | C1 muere y se queda como espectador hasta el final | — | [ ] | — | Spectate funciona, vuelve a HQ con su PlayerState |

📝 **Notas:**
```


```

---

### 2.2 — Cosméticos (helmet/skin)

| # | Escenario | LS | C1 | C2+ | Resultado esperado |
|---|---|---|---|---|---|
| 2.2.1 | LS equipa helmet en HQ y va a run | [ ] | [ ] | [ ] | Todos lo ven con helmet en la run |
| 2.2.2 | C1 entra al lobby con helmet ya equipado del save | — | [ ] | — | LS y otros lo ven con helmet desde el primer frame |
| 2.2.3 | Cambiar helmet en runtime durante HQ | [ ] | [ ] | [ ] | Cambio replica a todos sin destellos del modelo |

📝 **Notas:**
```


```

---

### 2.3 — Knockdown (visual + lógica)

| # | Escenario | LS | C1 | C2+ | Resultado esperado |
|---|---|---|---|---|---|
| 2.3.1 | C1 cae knockdown | — | [ ] | — | LS y otros ven el modelo girado 180° instantáneo |
| 2.3.2 | Knockdown durante salto | [ ] | [ ] | — | El visual aplica, no se "endereza" en el aire |
| 2.3.3 | Recuperación de knockdown → vuelve a corretear | [ ] | [ ] | — | Modelo vuelve a posición normal en todos |

📝 **Notas:**
```


```

---

### 2.4 — Voz proximal (VOIP)

| # | Escenario | LS | C1 | C2+ | Resultado esperado |
|---|---|---|---|---|---|
| 2.4.1 | LS y C1 cerca → hablar | [ ] | [ ] | — | Audio claro en ambas direcciones |
| 2.4.2 | Alejarse hasta el límite de range | [ ] | [ ] | — | Atenuación gradual, no corte abrupto |
| 2.4.3 | LS hace ServerTravel → ¿WASAPI sobrevive? | [ ] | [ ] | — | NO crash, voz sigue funcionando en HQ |

📝 **Notas:**
```


```

---

## 3. Input & Controles

| # | Escenario | LS | C1 | C2+ | Resultado esperado |
|---|---|---|---|---|---|
| 3.1 | Verificar IMC_Player asignado en BP_TortugaCharacter | [ ] | — | — | Sin warning en logs por asset faltante |
| 3.2 | Sprint funciona y consume stamina | [ ] | [ ] | [ ] | Stamina baja a 45/s, recharge tras 0.8s parado |
| 3.3 | Drop item con `IA_DropItem` | [ ] | [ ] | [ ] | Item cae al suelo, no se pierde |
| 3.4 | Emote wheel `IA_Emote*` (1–9) | [ ] | [ ] | [ ] | Wheel aparece, animación sale en todos |
| 3.5 | Inventario rotate con `IA_RotateInventory` | [ ] | [ ] | [ ] | Cambia equipped/stored sin perderlos |

📝 **Notas:**
```


```

---

## 4. Sistemas de mundo (chunks + actores)

### 4.1 — Generación procedural

| # | Escenario | LS | C1 | C2+ | Resultado esperado |
|---|---|---|---|---|---|
| 4.1.1 | Inicio de run → chunks generados (Easy → Medium → Hard) | [ ] | [ ] | [ ] | Sin gaps visibles entre chunks |
| 4.1.2 | InSocket alineado correctamente (no chunks volados) | [ ] | [ ] | [ ] | Suelo continuo |
| 4.1.3 | BP_Chunk_Final aparece al final | [ ] | [ ] | [ ] | Tiene el FinishLine accesible |

📝 **Notas:**
```


```

---

### 4.2 — Actores chunk-compatibles

| Actor | BP existe | Funciona en chunk | Notas |
|---|---|---|---|
| `TN_ItemSpawnZone` | ✓ | [ ] | Verificar bug 1.7 (sin pickups fantasma) |
| `TN_CrabActor` | ✓ | [ ] | Defer 1 tick OK |
| `TN_JellyfishActor` | ✓ | [ ] | Defer 1 tick OK |
| `TN_BananaPeel` | ✓ | [ ] | SetLifeSpan post-multicast OK |
| `TN_BreakablePlatform` | ✓ | [ ] | Multicast Reliable shake |
| `TN_ButtonInteractable` | (sin BP propio) | [ ] | Usado dentro de chunks |

📝 **Notas:**
```


```

---

### 4.3 — Enemigos / hazards

| Sistema | Test | LS | C1 | C2+ |
|---|---|---|---|---|
| Enemy Seagull (nuevo) | Avista, picotea, retira | [ ] | [ ] | — |
| Crab + CrabSpawnZone | Patrullan y atacan al jugador | [ ] | [ ] | — |
| Seagull Droppings + DroppingSpawnZone | Caen y aplican efecto | [ ] | [ ] | — |
| Quad enemy | Persigue / ataca | [ ] | [ ] | — |
| Quicksand volume | Frena en aire (no solo en suelo) | [ ] | [ ] | — |
| Slow zone volume | Misma cosa pero menos brutal | [ ] | [ ] | — |
| Storm volume | Efecto activo, replica | [ ] | [ ] | — |
| Pressure plate group | Todas pisadas → activa puerta/etc | [ ] | [ ] | — |

📝 **Notas:**
```


```

---

## 5. Muerte, rescate y espectador

| # | Escenario | LS | C1 | C2+ | Resultado esperado |
|---|---|---|---|---|---|
| 5.1 | C1 cae a death zone, espera 3s | — | [ ] | — | Pawn oculto, RescuePickup spawneado |
| 5.2 | Otro jugador rescata el pickup | [ ] | [ ] | — | C1 vuelve con pawn nuevo |
| 5.3 | Si nadie rescata → marca como `bIsEliminated` | — | [ ] | — | Va a spectate, no a "finish rank 0" |
| 5.4 | Espectar otro jugador rota entre vivos no eliminados | — | [ ] | — | Skip de finished/eliminated |
| 5.5 | Llegada a meta → results screen ⚠️ | [ ] | [ ] | [ ] | **Falta WBP_ResultsScreen — ver §7** |

📝 **Notas:**
```


```

---

## 6. Ítems y throwables

| # | Escenario | LS | C1 | C2+ | Resultado esperado |
|---|---|---|---|---|---|
| 6.1 | Recoger ConchPickup | [ ] | [ ] | — | Va al inventory, no se duplica en client |
| 6.2 | Equipar JellyfishActor → lanzar como throwable | [ ] | [ ] | [ ] | Trayectoria visible en todos (bug 1.2) |
| 6.3 | Equipar InkProjectile → lanzar | [ ] | [ ] | [ ] | Mesh visible (bug 1.5) |
| 6.4 | Inventory weight afecta stamina | [ ] | [ ] | — | MaxStamina efectiva baja con item pesado |
| 6.5 | Drop item al morir → recogible por otro | [ ] | [ ] | — | El item queda en world |

📝 **Notas:**
```


```

---

## 7. UI / Widgets

### 7.1 — Widgets existentes

| Widget | Aparece | Funciona | Notas |
|---|---|---|---|
| `WBP_MainMenuWidget` | [ ] | [ ] | |
| `WBP_LoadingScreenWidget` | [ ] | [ ] | |
| `WBP_VoiceIndicator` | [ ] | [ ] | Habla → indicador encima |
| `WBP_PlayerHUDWidget` | [ ] | [ ] | Stamina + inventory |
| `WBP_QuickChatFeedEntry` | [ ] | [ ] | Mensajes corren |
| `WBP_CoopFlowHUDWidget` | [ ] | [ ] | Estados de match |
| `WBP_EmoteWheel` | [ ] | [ ] | Hold → suelta emote |
| `WBP_QuickChatWheel` | [ ] | [ ] | Hold → mensaje |

### 7.2 — Widgets faltantes ⚠️

| # | Pendiente | Severidad | Notas |
|---|---|---|---|
| 7.2.1 | **WBP_ResultsScreen** — pantalla de resultados al final de la run | P1 | No existe ningún widget de resultados. El flujo `Finish → Results → HQ` no tiene representación visual. Ver `Docs/MISSING_ASSETS.md` y `Docs/GUIA_PANTALLA_RESULTADOS.md` |

📝 **Notas:**
```


```

---

## 8. Configuración / Setup obligatorio

### Pre-flight check antes de cada test session

- [ ] Editor cerrado antes de compilar (sin Live Coding)
- [ ] Ambas máquinas con la **misma DLL compilada** (sino → `NetChecksumMismatch`)
- [ ] Compilado con `-NoHotReload` (`build_editor.bat`)
- [ ] LVL_Menu, LVL_HQ, LVL_Run tienen su BP GameMode asignado en WorldSettings
- [ ] `DefaultEngine.ini` con `[Voice] bEnabled=false` (UE VOIP nativo desactivado)
- [ ] Steam abierto con AppID 480 en ambas máquinas

📝 **Notas pre-flight:**
```


```

---

## 9. Bugs descubiertos durante este QA (apéndice)

> Añade aquí los bugs nuevos que encuentres, con repro mínimo. Yo los cojo de aquí para fixearlos.

### Bug #N — Título corto

- **Severidad:** P? 
- **Repro:** 
- **Esperado:** 
- **Visto:** 
- **Notas:** 

---

### Bug #N — Título corto

- **Severidad:** P? 
- **Repro:** 
- **Esperado:** 
- **Visto:** 
- **Notas:** 

---

## 10. Notas generales

📝 **Sensación de juego, ritmo, pulido visual, lo que sea:**
```


```

---

*Generado 2026-04-18. Don Claudio + Terry Davis al servicio de la Famiglia.*
