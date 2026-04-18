# Missing Assets & BP Setup Gaps

> Reporte de huecos entre el código C++ y los assets `.uasset` del proyecto. Generado tras escanear `Source/Tortunabo/Public/` vs `Content/Blueprints/`.

**Fecha:** 2026-04-18
**Commit:** `65bdb28`

---

## 🔴 Críticos — Clases C++ Blueprintable SIN BP correspondiente

Estas clases están marcadas `UCLASS(Blueprintable)` pero no tienen un `BP_*` que las extienda. Sin BP no se pueden colocar en mapas con configuración custom (mesh, sound, defaults), ni asignar como ChildActorComponent en chunks.

| Clase C++ | Path esperado del BP | Bloqueante para |
|---|---|---|
| `ATN_CrabSpawnZone` | `Content/Blueprints/Gameplay/Enemies/BP_CrabSpawnZone.uasset` | Spawneo procedural de cangrejos en chunks |
| `ATN_DroppingSpawnZone` | `Content/Blueprints/Gameplay/Enemies/BP_DroppingSpawnZone.uasset` | Spawneo procedural de cacas de gaviota |
| `ATN_CollectionZone` | `Content/Blueprints/Gameplay/Interaction/BP_CollectionZone.uasset` | Mecánica de recolección (verificar uso real) |
| `ATN_UmbrellaInteractable` | `Content/Blueprints/Gameplay/Interaction/BP_UmbrellaInteractable.uasset` | Interactable de paraguas (refugio de tormenta?) |
| `ATN_ScorePickup` | `Content/Blueprints/Gameplay/Items/BP_ScorePickup.uasset` | Pickup de puntos sueltos |
| `ATN_ScriptedDeathZone` | `Content/Blueprints/Gameplay/Enemies/BP_ScriptedDeathZone.uasset` | Death zones triggered por evento (no volumen estático) |
| `ATN_ButtonGroupManager` | `Content/Blueprints/Gameplay/Interaction/BP_ButtonGroupManager.uasset` | Grupo de botones (vs `BP_PressurePlateGroupManager` que sí existe) |

**Acción recomendada:** crear un BP child en su carpeta correspondiente (Right click → Blueprint Class → buscar la clase) para cada uno que se vaya a usar realmente. Si alguno está obsoleto/no se usa, marcar para eliminación del C++.

---

## 🟡 BPs huérfanos / duplicados / deprecados

### `BP_FinishLineVolume` — DUPLICADO

Existen dos BPs idénticos en distintas carpetas:
- `Content/Blueprints/Gameplay/Enemies/BP_FinishLineVolume.uasset`
- `Content/Blueprints/Gameplay/Interaction/BP_FinishLineVolume.uasset`

**Acción:** decidir cuál es el canónico (probablemente el de `Interaction/` por semántica), borrar el otro y reasignar referencias en mapas/chunks.

### `BP_SeagullActor` — DEPRECADO

`ATN_SeagullActor` está marcado como deprecado en memoria (`feedback_no_blueprintimplementableevents.md` lo menciona, sustituido por `EnemySeagull` + `SeagullSpawnZone`). El BP `BP_SeagullActor.uasset` sigue presente.

**Acción:**
1. Comprobar que ningún chunk/mapa lo referencia (Right click BP → Reference Viewer).
2. Si limpio → borrar BP.
3. Considerar borrar también `TN_SeagullActor.h/.cpp` si nadie más lo referencia.

### `BP_QuadSpawner` — BP SIN C++

`Content/Blueprints/Gameplay/Enemies/BP_QuadSpawner.uasset` existe pero no hay clase `TN_QuadSpawner` en el código. Probablemente:
- Es un BP-only Actor que compone varios `BP_QuadActor` mediante ChildActorComponents.
- O es un wrapper antiguo no migrado.

**Acción:** abrirlo en editor, ver de qué clase hereda y qué hace. Si compone `BP_QuadActor`, está OK. Si replica funcionalidad de `BP_*SpawnZone`, considerar promoverlo a C++ (`ATN_QuadSpawnZone`).

---

## ⚠️ Widget faltante — End-game results

No existe `WBP_ResultsScreen` ni equivalente. El flujo del juego es:

```
Run → Finish/Spectate → [Results] → HQ
```

El estado `Results` no tiene representación visual. `Docs/GUIA_PANTALLA_RESULTADOS.md` describe la jerarquía esperada pero el widget nunca se creó.

**Widgets actuales en `Content/Blueprints/Gameplay/Widgets/`:**
- `WBP_MainMenuWidget`
- `WBP_LoadingScreenWidget`
- `WBP_VoiceIndicator`
- `WBP_PlayerHUDWidget`
- `WBP_QuickChatFeedEntry`
- `WBP_CoopFlowHUDWidget`
- `WBP_EmoteWheel`
- `WBP_QuickChatWheel`

**Acción:** crear `WBP_ResultsScreen.uasset` siguiendo la guía. Datos disponibles desde `TN_CoopGameState`:
- `FinishRank` por jugador
- `bIsEliminated` flag
- Cosméticos equipados (helmet/skin)
- Tiempo total / tiempo por checkpoint si se exponen

---

## 📊 Resumen ejecutivo

| Categoría | Cuántos | Prioridad |
|---|---|---|
| Clases C++ Blueprintable sin BP | 7 | Alta — bloquea uso en mapas |
| BPs duplicados | 1 (`BP_FinishLineVolume`) | Media — confusión |
| BPs deprecados | 1 (`BP_SeagullActor`) | Baja — limpieza |
| BPs sin C++ pair | 1 (`BP_QuadSpawner`) | Baja — verificar |
| Widgets faltantes | 1 (`WBP_ResultsScreen`) | Alta — flujo cojo |

**Total estimado de trabajo:** una sesión de editor de ~2-3h para crear los BPs faltantes + borrar duplicados/deprecados, más una sesión separada para el widget de resultados.

---

## ✅ Sanity check positivo — Lo que SÍ está bien

26 clases C++ Blueprintable verificadas; las que SÍ tienen BP:
- Personaje (`BP_TortugaCharacter`)
- Game Modes (Menu/HQ/Run)
- Player Controllers (Menu/Game)
- Game Instance
- Items (Conch, Generic, RescuePickup, ItemSpawnZone, Throwable, Jellyfish, Ink)
- Enemies (DeathZone, Seagull(deprecado)/EnemySeagull/SeagullSpawnZone/SeagullDropping, Crab, BananaPeel, BreakablePlatform, PhysicsObject, QuadActor, QuadSpawner, PressurePlate(+Group), QuicksandVolume)
- Interaction (FinishLine, Storm, TutorialEntry, Button)
- Cosmetics (HatStatue, SkinStatue)
- Chunks (ChunkManager + 7 chunk variants + Final)

**El stack de gameplay-core está completo.** Lo que falta son sistemas secundarios y el widget de results.

---

*Generado tras audit del 2026-04-18.*
