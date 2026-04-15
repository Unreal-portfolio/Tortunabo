# BACKLOG — Tortunabo
**Evaluado por ULTRON · 2026-04-15**
**Contexto:** 2 semanas de desarrollo · Claude al 100% para código · MVP: conexiones estables, fluido, sin lag

---

## Leyenda

| Símbolo | Significado |
|---------|------------|
| 🔴 P1 | Semana 1 — Core del MVP o alto impacto/bajo coste |
| 🟡 P2 | Semana 2 — Enriquecen el juego si P1 completado |
| ⚪ P3 | Solo si sobra tiempo |
| 🟢 EASY | < 2h de implementación |
| 🟠 MED | 2–8h de implementación |
| 🔴 HARD | 8h+ · múltiples sistemas o replicación compleja |

---

## 🔴 P1 — Semana 1 (Core MVP)

### #31 · 🟢 EASY — Aumentar velocidad de la tormenta
> Ajustar el float de velocidad en el actor de tormenta que persigue a los jugadores.

- Localizar la variable de velocidad en el actor de tormenta/death zone
- Incrementar el valor (calibrar en PIE)
- Impacto directo en ritmo de juego — hacer el primer día

---

### #3 · 🟢 EASY — Barrita Energética
> Item recogible que rellena la stamina al máximo instantáneamente.

- Crear nuevo item en Item Table
- `SetCurrentStamina(MaxStamina)` en `TN_StaminaComponent`
- Sin efectos secundarios

---

### #4 · 🟠 MED — Item Table refactor (variables compartidas)
> Auditar y aislar variables que se comparten entre items sin necesidad.

- Mapear todas las variables actuales de la Item Table
- Identificar cuáles son específicas de un tipo de item y cuáles son genéricas
- Separar en structs por tipo de item
- **Hacer antes de añadir más items** — deuda técnica que escala mal

---

### #7 · 🟠 MED — Knockdown — caída con momentum (sin ragdoll)
> Al ser noqueado, el jugador cae manteniendo su velocidad actual. Sin SimulatePhysics. CMC activo.

- Obtener la velocidad actual del pawn en el momento del knockdown (`GetVelocity()`)
- Llamar `LaunchCharacter(CurrentVelocity + FVector(0,0,-DownwardImpulse), true, true)` para conservar momentum y añadir caída
- Bloquear input del jugador (`DisableInput`) durante el tiempo de knockdown
- El CMC sigue activo — el personaje cae con las físicas del CMC (no ragdoll)
- Al aterrizar (`OnLanded`) o tras timer máximo: restaurar input
- Replicar via `bIsKnockedDown` (ya existe) + Server RPC
- ✅ **Ventaja**: network-friendly, sin desincronías, mucho más simple que ragdoll

---

### #11 · 🔴 HARD — Gaviota real — aparece de repente, sigue al jugador, círculo se encoge, mata tras timer
> La gaviota aparece de golpe sobre un jugador. Lo sigue. El círculo de peligro en el suelo se va haciendo cada vez más pequeño. Tras X segundos: si el jugador sigue dentro → muerte.

- La gaviota aparece de repente (sin anticipación prolongada) sobre un jugador aleatorio
- Sigue al jugador a velocidad menor que él (el jugador puede escapar corriendo)
- Círculo/sombra en el suelo que decrece en escala con el tiempo (Decal Actor o SceneComponent escalado)
- Timer replicado en servidor: al llegar a 0, comprobar si el jugador está dentro del radio → muerte
- El jugador sobrevive saliendo del radio antes de que el timer expire
- ⚠️ **Implementar ANTES** que Sombrilla (#29), Big Head (#2) y Gaviota caca (#12) — son dependientes

---

### #26 · 🔴 HARD — Cronómetro + Puntuación + Supabase global
> Sistema completo de scoring: cronómetro de carrera, puntuación por posición, guardado local y leaderboard global.

**Subpasos (dividir en 3 partes):**

**Parte A — Cronómetro replicado**
- Cronómetro en `TN_CoopGameState` (replicado)
- Arrancar en `PostSeamlessTravel` / inicio de carrera
- Mostrar en HUD

**Parte B — Puntuación por posición + guardado local**
- Al cruzar meta: registrar tiempo + posición en `TN_CoopPlayerState`
- Calcular puntos según posición (1º > 2º > 3º > 4º)
- Guardar en `TN_CosmeticSaveGame` o nuevo SaveGame específico
- Pantalla de resultados actualizada con tiempos y puntos

**Parte C — Supabase leaderboard global**
- HTTP POST al terminar partida: enviar tiempo, puntuación, PlayerName
- HTTP GET al entrar en menú: cargar top scores
- 💡 Prototipar con BP HTTP nodes antes de C++ para ir más rápido

---

## 🟡 P2 — Semana 2

### #1 · 🟢 EASY — Stamina Boost: efecto negativo post-boost
> Tras acabar el efecto positivo del Stamina Boost, aplicar penalti (stamina reducida o recarga más lenta) durante X segundos.

- Añadir segundo timer al finalizar el boost actual
- Aplicar `SetStaminaRechargeMultiplier` reducido o cap temporal de max stamina

---

### #25 · 🟢 EASY — Botones: soporte 1 y 2 pulsados
> El actor botón soporta dos modos: acción única (un pulsado) o toggle/doble acción (dos pulsados).

- Añadir `EButtonMode { SinglePress, DoublePress }` al actor botón
- Contador de interacciones + lógica de dispatch según modo

---

### #15 · 🟢 EASY — Botones: array de objetos objetivo
> En lugar de un único objeto referenciado, el botón gestiona un TArray de targets.

- Cambiar `AActor* TargetActor` → `TArray<AActor*> TargetActors`
- Iterar al activar

---

### #17 · 🟢 EASY — Zona lenta (afecta XYZ incluyendo vuelo)
> Volumen que aplica multiplicador de velocidad en todos los ejes, incluyendo Z (salto, caída, vuelo).

- Modificar `MaxWalkSpeed`, `JumpZVelocity` y `GravityScale` al entrar/salir del volumen
- Replicar correctamente (flag en PlayerState o en el componente de movimiento)

---

### #21 · 🟢 EASY — Plataforma rompible
> Al pisarla: shake + desaparece tras timer. Opcional: reaparece tras Y segundos.

- `OnComponentBeginOverlap` → iniciar shake (oscilación SetActorLocation o CameraShake)
- Timer → `SetActorHiddenInGame(true)` + deshabilitar colisión
- Replicar visibilidad

---

### #27 · 🟢 EASY — Coleccionables que dan puntuación
> Items dispersos por el nivel que suman puntos al recogerlos.
> **Depende de #26 (sistema de puntuación).**

- Overlap → incrementar score en `TN_CoopPlayerState`
- VFX de recogida

---

### #2 · 🟠 MED — Big Head: gaviota da segunda oportunidad + efecto mareo
> Con Big Head activo, un impacto de gaviota quita el efecto en lugar de matar. Al acabar el Big Head: efecto de mareo.
> **Depende de #11 (gaviota dinámica).**

- Hook en el sistema de daño de la gaviota: si `bBigHeadActive` → quitar efecto en lugar de matar
- Al acabar Big Head (o al recibir el impacto): aplicar efecto mareo (oscilación de cámara + input reducido X segundos)

---

### #5 · 🟠 MED — Tótem (respawn de compañero muerto)
> Item/objeto del mundo. Al activarlo: respawnea a un jugador muerto aleatorio al lado del activador.

- Seleccionar aleatoriamente un `TN_CoopPlayerState` con `bIsEliminated = true`
- Destruir `TN_RescuePickup` asociado (si existe)
- Spawnear pawn del jugador en `ActivatorLocation + Offset`
- Limpiar `bIsEliminated`, restaurar `bIsAlive`
- Llamar al flujo inverso de `MarkPlayerDead`

---

### #6 · 🟠 MED — Cáscara de plátano
> Objeto en el suelo. Al pisarlo: knockdown + impulso en dirección de movimiento (deslizamiento).

- Actor con colisión de overlap
- `OnBeginOverlap`: llamar sistema de knockdown del jugador + `LaunchCharacter` con velocidad de movimiento actual

---

### #12 · 🟠 MED — Gaviota caca — sombra cae desde arriba, mata en el punto de impacto
> Tipo de gaviota distinto a la gaviota real. Aparece encima de un jugador. La caca cae en línea recta. La sombra en el suelo se va reduciendo (avisa dónde y cuándo cae). Al impactar: mata si hay jugador ahí.

- Gaviota caca aparece directamente encima de un jugador (o punto aleatorio)
- La caca desciende en línea recta (no persigue — el jugador solo necesita moverse)
- Sombra proyectada en el suelo que decrece en escala a medida que la caca baja (feedback visual claro)
- Al impactar el suelo: hitbox instantáneo en el punto de caída → si overlap con jugador → muerte
- Sistema completamente independiente de la gaviota real (#11)

---

### #13 · 🟠 MED — Tinta de calamar
> Item lanzable de un uso. Al impactar a un jugador: mancha negra en su pantalla durante X segundos.

- Proyectil estándar (heredar de clase proyectil existente o crear)
- Al impactar: RPC al cliente afectado → aplicar Post Process Material con mancha negra
- Timer → retirar PP Material

---

### #14 · 🟠 MED — DeathZone scripted con array de acciones
> Clase que hereda de la DeathZone base. Añade un array de acciones sobre objetos del mundo (mover, rotar, spawnear, destruir).

- Crear `TN_ScriptedDeathZone : ATN_DeathZoneVolume`
- `TArray<FWorldAction> Actions` donde `FWorldAction` es un struct con: `AActor* Target`, `EActionType Type`, parámetros
- Ejecutar acciones en el evento que corresponda (al activarse, al matar, etc.)
- Configurable desde Blueprint

---

### #19 · 🟠 MED — Tormenta de arena (Post Process + Niagara)
> Sistema ambiental que limita visibilidad y añade atmósfera. Activable por evento.

- Post Process Volume con material de niebla/partículas de arena
- Sistema Niagara de partículas flotantes
- Reducir `FogDensity` o distancia de visibilidad
- Audio ambiental de viento/arena
- Activable mediante trigger o evento del level

---

### #20 · 🟠 MED — Puentes que se rompen con X jugadores
> Actor de puente que detecta cuántos jugadores están encima. Si supera umbral: vibra y se destruye.

- Conteo de overlaps replicado en el servidor
- Si count >= Threshold: shake de actor + timer → `SetActorHiddenInGame` + deshabilitar colisión
- Configurable: umbral de jugadores + duración del shake + tiempo hasta destrucción

---

### #22 · 🟠 MED — Concha (trampa + item dual)
> Al pisarla: inmoviliza. También recogible como item. Al colocarse con E: trampa permanente.

- **Modo trampa pasiva**: overlap → bloquear movimiento del jugador (`DisableMovement`) durante X segundos
- **Modo item**: recogible al pasar encima (no activa la trampa)
- Al equipar y pulsar E: colocar en el suelo como trampa (spawn de actor trampa en la posición)
- La trampa colocada no se puede recoger ni quitar

---

### #23 · 🟠 MED — Objetos con físicas empujables
> Actores con SimulatePhysics. Al colisionar con una tortuga: reciben impulso. La tortuga frena ligeramente.

- Activar `SimulatePhysics` en los actores objetivo
- En `OnHit` del pawn: aplicar `AddImpulse` al actor basado en velocidad y masa del pawn
- Pequeño freno al pawn (reducir velocidad momentáneamente)
- ⚠️ Limitar a objetos decorativos para el MVP — la física en red es costosa

---

### #24 · 🟠 MED — Gestor de botones (todos pulsados → transform array)
> Actor gestor que escucha a un array de botones. Cuando todos han sido pulsados: aplica transforms a un array de objetos.

- `TArray<ATN_ButtonInteractable*> ManagedButtons`
- Suscribirse al evento de cada botón
- Cuando todos `bPressed == true`: iterar `TArray<FTransformAction>` y aplicar a actores objetivo
- Configurable desde Blueprint

---

### #28 · 🟠 MED — Objetos recogibles a zona (gestor de recogida)
> Jugadores recogen objetos y los depositan en zona específica. Tras X depósitos: activa transform.

- `TN_CollectionZone`: volumen que trackea objetos depositados
- Al depositar objeto (soltar en la zona): incrementar contador
- Al llegar a `RequiredCount`: activar array de transforms/eventos
- UI de progreso (X/N objetos depositados)

---

### #29 · 🟠 MED — Sombrilla (protección contra gaviota)
> Interactuable. Al pulsar E: abre sombrilla, jugador inmune a gaviota durante X segundos. Luego se cierra sola.
> **Depende de #11 (gaviota dinámica).**

- Actor interactuable con `OnInteract`
- Activar flag `bHasUmbrellaProtection` en el jugador (replicado)
- Timer → desactivar flag
- En el sistema de gaviota: comprobar `bHasUmbrellaProtection` antes de aplicar muerte

---

### #30 · 🟠 MED — Placas de activación (100% vivos encima)
> Cada jugador vivo debe estar encima de una placa simultáneamente para activar el evento.

- Array de `TN_PressurePlate` actors, cada uno trackea si tiene jugador encima
- Gestor comprueba: `PlayersOnPlates == AlivePlayers`
- Si condición cumplida durante X segundos: activar evento
- UI de estado (X/N placas activas)

---

### #10 · 🔴 HARD — Cangrejo (patrol lateral + persecución + animación)
> NPC enemigo. Patrol de lado a lado. Si jugador en rango: perseguir hasta límite de área. Mata al contacto.

- Actor con Skeletal Mesh y mini AnimBP (caminar lateral loop)
- Tick: lógica de patrol (lerp entre puntos A y B)
- Detección de jugador en rango (`SphereTrace` o `USphereComponent` de detección)
- Si detectado: mover hacia el jugador con límite de área (bounds check o radio máximo)
- Al contacto con jugador: aplicar knockdown o muerte
- Replicar posición y estado (patrol / chase)

---

### #16 · 🔴 HARD — Arena movediza
> Zona que ralentiza al jugador, lo hunde progresivamente y lo deja atascado.

- `TN_QuicksandVolume` con tres fases: **Entrada** (reducir velocidad), **Hundimiento** (mover Z hacia abajo interpolado), **Atascado** (MovementMode bloqueado)
- Cada fase con timer configurable
- Replicar estado de hundimiento (posición Z sincronizada)
- ⚠️ No tocar CMC directamente — usar flag replicado + override local en el cliente
- Sistema de rescate: otro jugador puede interactuar para sacar al atascado

---

### #18 · 🔴 HARD — Quad (coche gigante con franjas mortales)
> Evento scripted. Dos franjas horizontales marcadas en el mapa. Advertencia sonora. Coche gigante atraviesa el mapa y mata en sus franjas.

- Marcar dos franjas horizontales en el nivel (Decal o mesh plano)
- Audio de advertencia replicado X segundos antes
- Spawn del actor coche en el borde del mapa
- Movimiento lineal a alta velocidad (Lerp o Spline)
- Dos hitboxes de rueda (BoxComponent) que matan al overlap
- Desaparecer al salir del mapa
- Todo el timing replicado desde el servidor

---

## ⚪ P3 — Solo si sobra tiempo

### #8 · 🔴 HARD — Arpón
> Herramienta que lanza proyectil con cable. Engancha jugadores, items o pesca en charcos.

- Proyectil con Cable Component (visual del cable)
- Al impactar: enganchar target (jugador, item, actor de charco)
- Recoger: atraer el target hacia el lanzador
- ⚠️ Complejidad alta en red — dejar para el final

---

### #9 · 🟠 MED — Charcos de pesca (zonas para el arpón)
> Volúmenes en los que el arpón puede pescar objetos, items o jugadores caídos.
> **Bloqueado por: #8 (Arpón).**

---

### #32 · 🟠 MED — UI de selección de skins
> Pantalla para seleccionar skin de tortuga antes de la partida o en el lobby.

- Integración con `TN_CosmeticSaveGame` y `MP_GameInstance`
- Widget de selección con preview del personaje
- Persistir selección entre sesiones

---

## ⚠️ Riesgos críticos

| # | Riesgo | Mitigación |
|---|--------|-----------|
| #7 | Knockdown con LaunchCharacter — asegurar que OnLanded no interfiere con lógica de gameplay | Timer máximo de seguridad si OnLanded no dispara (ej. knockdown en el aire) |
| #11 | Gaviota dinámica es base de #2, #12, #29 | Implementar primero la gaviota, luego los dependientes |
| #26 | Supabase desde UE5 requiere HTTP manual | Prototipar con BP HTTP nodes antes de bajar a C++ |
| #23 | Física en red es costosa y desincronizable | Limitar a objetos decorativos en el MVP |
| #16 | Arena movediza puede corromper el CMC | Flag replicado + override local, no modificar CMC directamente |

---

## Orden de implementación sugerido

```
SEMANA 1
  Día 1     → #31 (velocidad tormenta) + #3 (barrita energética)
  Día 2-3   → #4  (Item Table refactor)
  Día 4-5   → #7  (knockdown con momentum — LaunchCharacter)
  Día 6-7   → #11 (gaviota dinámica)
  Día 8-10  → #26 (cronómetro → puntuación → Supabase)

SEMANA 2
  Empezar por los EASY de P2 para ganar tracción rápida,
  luego abordar los HARD según tiempo restante.
  Priorizar #2, #29 (dependen de gaviota ya implementada).
```
