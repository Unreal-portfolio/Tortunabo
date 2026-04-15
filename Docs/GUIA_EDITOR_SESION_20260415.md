# Guía de Pasos en Editor — Sesión 2026-04-15

> **Estado:** C++ compilado y pusheado a `main`.  
> Estos pasos completan los fixes y features que requieren trabajo en el Editor de UE5.  
> Completar en orden. Cada paso indica qué tarea cierra y qué hay que testear tras hacerlo.

---

## ⚙️ Paso 0 — Compilar (OBLIGATORIO antes de abrir el editor)

```powershell
& "C:\Program Files\Epic Games\UE_5.6\Engine\Build\BatchFiles\Build.bat" TortunaboEditor Win64 Development "C:\Users\Rodrigo\CARRERA\PROYECTOS_PERSONALES\Unreal Engine\Tortunabo\Tortunabo.uproject" -WaitMutex -NoHotReload
```

Esperar `BUILD SUCCESSFUL`. Sin esto el editor no ve las clases nuevas.

---

## Paso 1 — #B5: Pelota física (BP hijo de TN_PhysicsObjectActor)

**Cierra:** Bug #B5 — pelota parada en cliente → snap a posición final  
**Clase C++ base:** `TN_PhysicsObjectActor`

### Si ya existe un BP de pelota
1. Content Browser → abrir el BP de la pelota
2. **File → Reparent Blueprint** → buscar `TN_PhysicsObjectActor` → confirmar
3. Verificar que `Mesh` (componente heredado) tiene el StaticMesh asignado
4. **Compile → Save**

### Si no existe BP de pelota aún
1. Content Browser → **Add → Blueprint Class** → buscar `TN_PhysicsObjectActor`
2. Nombre: `BP_PhysicsBall` (o el nombre del proyecto)
3. Seleccionar componente `Mesh` → asignar StaticMesh + material
4. Ajustar `Collision Profile = PhysicsActor` si no está ya
5. **Compile → Save**

### ⚠️ Qué testear
- PIE 2 jugadores → empujar la pelota → verificar que en la **ventana del cliente** la pelota se mueve en tiempo real (sin quedarse parada ni hacer snap)
- Dejar la pelota quieta varios segundos → empujar de nuevo → confirmar que sigue sincronizada

---

## Paso 2 — #3: Barrita Energética (BP pickup)

**Cierra:** Task #3 — ítem que restaura stamina al máximo  
**Método C++:** `UTN_StaminaComponent::RestoreStaminaToFull()`

### Crear el pickup
1. Content Browser → **Add → Blueprint Class** → buscar `ATN_PickupInteractableBase`
2. Nombre: `BP_EnergyBarPickup`
3. **Class Defaults:**
   - `Item Data Table` → `DT_Items`
   - `Item Row Name` → nombre de la fila (ej. `Item_EnergyBar`) — créala en DT_Items si no existe
4. **Event Graph → implementar `Event On Picked Up`:**
   ```
   On Picked Up (Interactor: Pawn)
     → Cast to TortugaCharacter
     → Get Component by Class (TN_StaminaComponent)
     → Restore Stamina To Full
   ```
5. Asignar mesh/icono del ítem en el StaticMeshComponent heredado
6. **Compile → Save**

### Añadir fila en DT_Items (si no existe)
1. Content Browser → abrir `DT_Items`
2. **Add Row** → nombre: `Item_EnergyBar`
3. Rellenar: `PickupActorClass = BP_EnergyBarPickup`, peso, nombre de display, icono
4. **Save**

### Colocar en el nivel / SpawnZone
- Colocar instancias de `BP_EnergyBarPickup` directamente en el nivel, o
- Añadir `Item_EnergyBar` al array `ItemRowNames` de algún `BP_ItemSpawnZone`

### ⚠️ Qué testear
- Recoger la barrita con stamina baja → stamina sube al máximo inmediatamente
- Esperar 2 segundos tras el efecto → aparece penalización de agotamiento (`PostBoostExhaustionSeconds = 2s`)
- Verificar que la UI de stamina refleja el estado correctamente en cliente y servidor

---

## Paso 3 — #21: Plataforma rompible (BP hijo de TN_BreakablePlatform)

**Cierra:** Task #21 — plataforma que se rompe al aguantar peso  
**Clase C++ base:** `TN_BreakablePlatform`

### Crear el BP
1. Content Browser → **Add → Blueprint Class** → buscar `TN_BreakablePlatform`
2. Nombre: `BP_BreakablePlatform`
3. **Class Defaults:**
   - Seleccionar `PlatformMesh` → asignar StaticMesh de plataforma
   - Seleccionar `StandTrigger` → ajustar `Box Extent` para cubrir la superficie del mesh  
     *(por defecto: 90×90×10 — la caja detecta pawns encima)*
   - `Time To Break` → segundos antes de romperse (default: `2.0`)
   - `Respawn Time` → `0` si no reaparece, `6.0` si sí reaparece

### Implementar eventos VFX (opcional pero recomendado)
En el **Event Graph**, implementar estos tres BlueprintImplementableEvents:

| Evento | Cuándo ocurre | Sugerencia de VFX |
|--------|--------------|-------------------|
| `On Platform Shake` | A mitad del timer (~1s antes de romperse) | Sonido de crujido + shake de cámara ligero |
| `On Platform Break` | Al romperse (en todas las máquinas) | Niagara de partículas + sonido de impacto |
| `On Platform Respawn` | Al reaparecer (en todas las máquinas) | Efecto de materialización + sonido |

4. **Compile → Save**

### Colocar en el nivel / chunks
- Colocar instancias de `BP_BreakablePlatform` en el nivel o dentro de chunks BP

### ⚠️ Qué testear
- Jugador A se sube → esperar `TimeToBreak` segundos → plataforma desaparece
- Jugador A cae → si `RespawnTime > 0`, plataforma reaparece tras N segundos
- **Multijugador:** Jugador B (cliente) ve la plataforma romperse y reaparecer en sincronía con el servidor
- Si el jugador se baja antes de que expire el timer → la plataforma **no** se rompe

---

## Paso 4 (opcional) — #17: Activar vuelo en Slow Zones existentes

**Cierra:** Task #17 — zona lenta con efecto de gravedad reducida  
*(Solo si quieres que alguna zona específica tenga efecto de flotación)*

Para cada instancia de `BP_SlowZoneVolume` que quieras que tenga vuelo:
1. Seleccionar la instancia en el nivel
2. Panel **Details** → sección **SlowZone|Gravity**
3. **bSlowFall** → ✓ (activar)
4. **Gravity Scale In Zone** → ajustar según efecto deseado:
   - `0.3` → caída lenta (default)
   - `0.1` → casi sin gravedad
   - `0.0` → gravedad cero (flotación completa)

> Las instancias con `bSlowFall = false` (default) no cambian comportamiento — son retrocompatibles.

### ⚠️ Qué testear
- Entrar en la zona → el jugador cae más lento + salto llega más alto
- Salir de la zona → gravedad vuelve a la normalidad inmediatamente
- En multijugador: todos los clientes experimentan el mismo efecto (lógica local, sin replicación extra)

---

## 🧪 Resumen de Tests Pendientes

Estos son los sistemas modificados esta sesión que requieren smoke test multijugador:

| Sistema | Test mínimo | Riesgo |
|---------|------------|--------|
| **#B3 — Revive + chunks** | Jugador muere en chunk alejado, otro avanza 3+ chunks, revivir → sin errores de replicación | Alto |
| **#B4/#B8 — Puerta + muerte** | Mover puerta, jugador muere, revivir → puerta en posición correcta, sin phase-through | Medio |
| **#B5 — Pelota** | Empujar pelota como cliente → movimiento en tiempo real sin snap | Medio |
| **#B6/#B7 — Pantalla resultados** | Terminar carrera con cliente → widget aparece en ambas ventanas | Alto |
| **#1 — Post-boost** | Recoger barrita → esperar fin → penalización de 2s de agotamiento | Bajo |
| **#3 — Barrita energética** | Recoger con stamina baja → stamina sube al máximo | Bajo |
| **#15 — Multi-target botón** | Botón con 2+ targets → todos se mueven al activar | Bajo |
| **#17 — Slow zone vuelo** | Entrar zona con bSlowFall → gravedad reducida; salir → normal | Bajo |
| **#21 — Plataforma rompible** | Subirse → esperar timer → plataforma desaparece; cliente ve lo mismo | Medio |
| **#31 — Velocidad tormenta** | Verificar que la tormenta avanza notablemente más rápido | Bajo |

---

## 📋 No requiere pasos en editor

Estos fixes son 100% C++ y funcionan automáticamente tras compilar:

- **#B1** — SpawnZone: ya estaba corregido en código anterior
- **#B4/#B8** — Puerta: `bAlwaysRelevant` en `TN_ButtonInteractable` — sin pasos de editor
- **#15** — Multi-target: añadir actores al array `AdditionalMoveTargets` en instancias existentes
- **#31** — Tormenta: `GrowthSpeed` ya cambiado en el default del `.h`

---

*Generado automáticamente — Sesión ULTRON 2026-04-15*
