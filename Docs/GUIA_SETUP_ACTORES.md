# Guía de Setup — Death Zone · Button Interactable · Item Spawn Zone · Puffer Fish

> Fecha: 2026-03-26 | Válida para UE 5.6 — Tortunabo

---

## 1. Death Zone (`ATN_DeathZoneVolume`)

### Qué hace
Actor con `UBoxComponent` que mata a cualquier tortuga que permanezca dentro más de `SecondsInsideToDie` segundos. Con `bDestroyOnlyDuringRun = true` solo actúa durante la carrera (ignorada en el lobby). **Chunk-compatible**: puede colocarse dentro de un Blueprint Actor (chunk) spawneado en runtime.

### Pasos

#### A) Crear Blueprint hijo (recomendado para chunks)
| # | Paso |
|---|------|
| 1 | Content Browser → clic derecho → **Blueprint Class** → buscar `TN_DeathZoneVolume` como padre. |
| 2 | Nombrar `BP_DeathZoneVolume`. |
| 3 | Abrir el BP → seleccionar `TriggerBox` → ajustar **Box Extent** al tamaño deseado. |
| 4 | En Class Defaults → **DeathZone**: configurar `SecondsInsideToDie`, `bDestroyOnlyDuringRun`, etc. |

#### B) Colocar directamente en el nivel (uso simple)
| # | Paso |
|---|------|
| 1 | Arrastra `BP_DeathZoneVolume` (o la clase C++ directamente) al nivel. |
| 2 | Redimensiona el `TriggerBox` en el viewport con la herramienta Scale o desde Details → Box Extent. |
| 3 | Ajusta propiedades en Details → DeathZone. |

#### C) Colocar dentro de un Chunk BP
| # | Paso |
|---|------|
| 1 | Abre el BP del chunk → Add Child Actor Component → clase `BP_DeathZoneVolume`. |
| 2 | Posiciona y escala el child actor dentro del chunk. |
| 3 | Las propiedades se heredan del BP hijo. |

### Notas
- El actor es solo servidor. Los clientes ven el efecto a través de `TN_CoopPlayerState::DeathZoneTimeRemaining` (replicado).
- Si quieres **feedback visual** del countdown en la zona, añade un widget o VFX por Blueprint en tu mapa (no hace falta tocar C++).
- Puedes superponer varios volúmenes; cada uno lleva su propio countdown independiente por jugador.

---

## 2. Button Interactable (`ATN_ButtonInteractable`)

### Qué hace
Botón físico en el mundo que, al pulsar **IA_Interact**, mueve de forma fluida un actor (puertas, plataformas, palancas, etc.) a través de una lista de waypoints. Cada interacción avanza al siguiente waypoint (cíclico). Multijugador: `CurrentWaypointIndex` se replica a todos los clientes; tanto servidor como clientes interpolan el MoveTarget localmente en `Tick` hacia el waypoint actual.

### Pasos

#### A) Crear el Blueprint hijo
| # | Paso |
|---|------|
| 1 | Content Browser → clic derecho → **Blueprint Class**. |
| 2 | Busca la clase padre `TN_ButtonInteractable` y selecciónala. |
| 3 | Nómbrala `BP_ButtonInteractable` (o el nombre que quieras). |
| 4 | Abre el BP → pestaña **Viewport**: asigna un **Static Mesh** al componente `Mesh` raíz (ej. un cubo para el botón físico). |
| 5 | En **Class Defaults → Interaction**: |
| | • `PromptText` → "Pulsar" (o el texto que muestre el prompt). |
| | • `InteractionDistance` → distancia de interacción (cm), default 250. |
| | • `CooldownSeconds` → tiempo mínimo entre interacciones (default 0.25 s). |
| | • `MoveSpeed` → velocidad de movimiento del target (cm/s). |
| | • `RotateSpeed` → velocidad de rotación del target (°/s). |
| 6 | **Opcional — evento BP**: el nodo `OnDirectInteraction` se dispara en cada pulsación. Úsalo para sonidos/VFX del botón. |

#### B) Preparar el actor que se moverá
| # | Paso |
|---|------|
| 1 | Coloca en el nivel el actor que quieres que se mueva (ej. una plataforma, puerta). |
| 2 | Selecciónalo → Details → **Replication**: activa **Replicates**. |
| 3 | No es necesario activar **Replicate Movement**: tanto servidor como clientes interpolan localmente usando `CurrentWaypointIndex`. |

#### C) Colocar y configurar en el nivel
| # | Paso |
|---|------|
| 1 | Arrastra `BP_ButtonInteractable` al nivel, colócalo donde esté el botón físico. |
| 2 | Selecciónalo → Details → **Button**: |
| | • `MoveTarget` → con el **eyedropper** selecciona el actor del nivel que debe moverse. |
| | • `Waypoints` → añade tantos elementos como posiciones necesites. Cada elemento es un `FTransform` (Location + Rotation + Scale en espacio mundo). Usa "Copy Actor Transform" en el nivel para obtener los valores exactos. |
| 3 | Pulsa **Play** → acércate al botón → **E** (IA_Interact) → el actor debe moverse al waypoint 0, 1, 2... cíclicamente. |

#### C-bis) Dentro de un Chunk BP (runtime)
| # | Paso |
|---|------|
| 1 | En el chunk BP, añade un **Child Actor Component** para la puerta/plataforma. Dale un **Actor Tag** (ej. `Door`). |
| 2 | Añade otro Child Actor Component con `BP_ButtonInteractable`. |
| 3 | En el BP_ButtonInteractable Class Defaults: |
| | • `MoveTarget` → dejar vacío. |
| | • `MoveTargetTag` → `Door` (el tag del Child Actor del paso 1). |
| | • `bUseRelativeWaypoints` → **true** (los waypoints se interpretan como offsets relativos al MoveTarget). |
| | • `Waypoints` → definir como offsets locales del MoveTarget (ej. `(0, 0, 300)` = "sube el target 300 cm desde su posición inicial"). |
| 4 | Al spawnear el chunk, `BeginPlay` busca el `MoveTarget` por tag automáticamente y convierte waypoints a mundo usando el transform del MoveTarget como base. |

#### D) Waypoints: cómo obtener los valores
1. Coloca un **Empty Actor** (o un cubo temporal) en la posición destino.
2. En Details copia la **World Location** y **World Rotation**.
3. Pégalos en el elemento correspondiente de `Waypoints` del botón.
4. Borra el actor temporal.

---

## 3. Item Spawn Zone (`ATN_ItemSpawnZone`)

### Qué hace
Actor que al arrancar el servidor genera X ítems aleatorios dentro de un área de caja. Valida que cada posición tiene suelo y no hay obstáculos. Usa el `DT_Items` para elegir qué items spawnear.

### Requisito previo — DataTable `DT_Items`

Si no existe aún:
| # | Paso |
|---|------|
| 1 | Content Browser → clic derecho → **Miscellaneous → Data Table**. |
| 2 | En el diálogo selecciona el Row Type: busca `TN_InventoryItem`. |
| 3 | Nómbrala `DT_Items` y guárdala en `/Game/Blueprints/Gameplay/`. |
| 4 | Abre la tabla → **Add** → dale un **Row Name** (ej. `Item_ThrowableBall`). |
| 5 | Rellena los campos: |
| | • `ItemId` = nombre único (mismo que el Row Name). |
| | • `EquippedMesh` = StaticMesh que se ve en mano y en el suelo. |
| | • `EquippedMeshScale` = escala del mesh (ej. `(0.25, 0.25, 0.25)` para bola pequeña). |
| | • `UseType` = `Throwable` (para la bola) / `PufferFish` (para el pez). |
| | • `ThrowSpeed` = velocidad de lanzamiento (cm/s), default 1800. |
| | • `ItemWeight` = penalización de stamina (0 = sin coste). |
| | • `PickupActorClass` = `BP_GenericPickup`. |
| | • `ThrowableActorClass` = `BP_GenericThrowable` (o `BP_PufferFish` para el pez). |

### Pasos — colocar la zona

| # | Paso |
|---|------|
| 1 | Arrastra `TN_ItemSpawnZone` (clase C++) al nivel. |
| 2 | Redimensiona el `SpawnBox` (componente Box visible en editor) para cubrir la zona deseada. Escala el actor o edita `Box Extent` en Details. |
| 3 | Selecciona el actor → Details → **SpawnZone**: |
| | • `ItemDataTable` → asigna tu `DT_Items`. |
| | • `ItemRowNames` → añade los Row Names de los ítems que puede spawnear esta zona (ej. `["Item_ThrowableBall", "Item_PufferFish"]`). Cada spawn elige uno aleatorio del array. |
| | • `SpawnCount` → cuántos ítems genera (ej. 5). |
| | • `MinSpacing` → distancia mínima entre ítems (cm). Aumentar si los ítems se solapan. |
| | • `MaxRetries` → intentos por ítem si la posición no es válida (default 10). |
| 4 | Pulsa **Play** → el servidor genera los ítems automáticamente en `BeginPlay`. |

### Consejos
- La zona solo spawnea en el **servidor**. Los clientes ven los pickups porque `ATN_PickupInteractableBase` está configurado para replicar.
- Si los ítems clipean con el suelo, revisa el `MeshFloorOffset` en el BP del pickup o en el DataTable. Para pickups dinámicos, `InitializeFromInventoryItem` calcula el offset automáticamente.
- Puedes colocar varias `TN_ItemSpawnZone` en distintas zonas del mapa con diferentes `ItemRowNames`.

---

## 4. Puffer Fish (`ATN_PufferFishActor`)

### Qué hace
Throwable especial: al lanzarse, tras un delay aleatorio (`InflateDelayMin`–`InflateDelayMax` segundos) se infla a `InflateScale` veces su tamaño y empuja con fuerza a todos los jugadores en `InflateRadius` cm. Si la fuerza supera `MinKnockdownForce` → knockdown. Después se desinfla y se convierte en pickup recogible.

### A) Crear el Blueprint del proyectil

| # | Paso |
|---|------|
| 1 | Content Browser → clic derecho → **Blueprint Class**. |
| 2 | Padre: `TN_PufferFishActor`. |
| 3 | Nombra `BP_PufferFish`. |
| 4 | Viewport → componente `Mesh` raíz → asigna el StaticMesh del pez (ej. una esfera o mesh custom). |
| 5 | **Class Defaults → PufferFish**: |
| | • `InflateDelayMin` = 1.0 s (mínimo antes de inflarse). |
| | • `InflateDelayMax` = 3.0 s (máximo antes de inflarse — el delay es aleatorio). |
| | • `InflateScale` = 5.0 (se pone 5x más grande). |
| | • `InflateRadius` = 400 cm (radio del empuje). |
| | • `InflatePushForce` = 1500 cm/s (fuerza de empuje). |
| | • `InflateDuration` = 1.5 s (tiempo inflado antes de desinflarse). |
| | • `MinKnockdownForce` = 800 cm/s (fuerza mínima para noquear). |
| | • `PufferKnockdownDuration` = 2.0 s (duración del knockdown). |
| 6 | **Class Defaults → Throwable**: |
| | • `Bounciness` = 0.55 (rebote). |
| | • `RollingFriction` = 0.25. |
| | • `MaxLifeSeconds` = 8.0 (si no se detiene, se destruye solo). |
| | • `MinKnockdownSpeed` = 600 (velocidad mínima del proyectil para knockdown al impactar directamente). |
| 7 | Compila y guarda. |

### B) Añadir el Puffer Fish al DataTable

| # | Paso |
|---|------|
| 1 | Abre `DT_Items`. |
| 2 | **Add Row** → Row Name: `Item_PufferFish`. |
| 3 | Rellena: |
| | • `ItemId` = `Item_PufferFish`. |
| | • `EquippedMesh` = el StaticMesh del pez. |
| | • `EquippedMeshScale` = escala en mano (ej. `(0.3, 0.3, 0.3)`). |
| | • `UseType` = **PufferFish**. |
| | • `ThrowSpeed` = 1200 (un poco más lento que la bola para que sea lanzable pero no instantáneo). |
| | • `ItemWeight` = 1 (penalización de stamina mientras lo llevas). |
| | • `PickupActorClass` = `BP_GenericPickup`. |
| | • `ThrowableActorClass` = **`BP_PufferFish`** (el BP que creaste). |

### C) Crear el pickup en el suelo

| # | Paso |
|---|------|
| 1 | Crea un BP hijo de `ATN_PickupInteractableBase` → nómbralo `BP_PufferFishPickup`. |
| 2 | Viewport → `Mesh` → asigna el mismo StaticMesh del pez. |
| 3 | Class Defaults: |
| | • `ItemDataTable` = `DT_Items`. |
| | • `ItemRowName` = `Item_PufferFish`. |
| 4 | **Alternativa**: usar directamente `BP_GenericPickup` (si existe) y setear `ItemRowName = Item_PufferFish` por instancia. |

### D) Añadir el pez a la Item Spawn Zone

En la `TN_ItemSpawnZone` correspondiente, añade `Item_PufferFish` al array `ItemRowNames`.

### Flujo completo en juego
```
Jugador recoge pickup (IA_Interact)
  → InventoryComponent guarda Item_PufferFish
  → Jugador lanza (IA_DropItem / IA_UseItem)
  → Servidor spawnea BP_PufferFish + aplica velocidad con arco parabólico
  → El pez vuela 1–3s (aleatorio)
  → SE INFLA → empuja a todos en radio 400cm
  → Si fuerza > MinKnockdownForce → ApplyKnockdown
  → 1.5s después → SE DESINFLA → spawn BP_PufferFishPickup en el suelo
  → Cualquier jugador puede recogerlo de nuevo
```

---

## 5. Resumen de relaciones entre sistemas

```
DT_Items (DataTable)
  ├─ Item_ThrowableBall  → PickupActorClass: BP_GenericPickup
  │                        ThrowableActorClass: BP_GenericThrowable
  └─ Item_PufferFish     → PickupActorClass: BP_GenericPickup (o BP_PufferFishPickup)
                           ThrowableActorClass: BP_PufferFish

TN_ItemSpawnZone (en nivel) → referencia DT_Items + ItemRowNames
  └─ spawnea BP_GenericPickup con ItemRowName configurado

ATN_ButtonInteractable (en nivel)
  └─ MoveTarget → cualquier Actor replicado del nivel
  └─ Waypoints  → array de FTransform (posiciones destino)

ATN_DeathZoneVolume (en nivel)
  └─ sin referencias externas — standalone
```

---

## 6. Checklist rápido de errores comunes

| Síntoma | Causa | Solución |
|---------|-------|----------|
| El botón no mueve nada | `MoveTarget` no asignado | Usar el eyedropper en Details para asignarlo por instancia (`EditInstanceOnly`) |
| El actor movido se ve diferente en clientes | `Replicate Movement` desactivado | Activar en el actor target → Details → Replication |
| Los ítems no spawnean | `ItemDataTable` o `ItemRowNames` vacíos | Asignar DT_Items y añadir al menos un Row Name válido |
| Los ítems clipean con el suelo | `MeshFloorOffset` = 0 en el pickup | Ajustar `MeshFloorOffset` en el BP del pickup, o en el DataTable |
| El pez no se infla | `UseType` en DT_Items no es `PufferFish` | Cambiar UseType en la fila de DT_Items |
| El pez se infla pero no noquea | `MinKnockdownForce` muy alto | Reducir `MinKnockdownForce` en BP_PufferFish Class Defaults |
| Death zone mata en el lobby | `bDestroyOnlyDuringRun` = false | Asegurarse que está en **true** |
| Death zone no mata en la carrera | GameMode no es `TN_RunGameMode` | Verificar WorldSettings → GameMode Override = `BP_RunGameMode` |
| Prompt de interacción no aparece | `PromptText` vacío o `PromptWidgetClass` sin asignar | Rellenar en BP_ButtonInteractable Class Defaults |

