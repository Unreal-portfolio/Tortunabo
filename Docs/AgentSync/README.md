# AgentSync

Esta carpeta sincroniza el trabajo entre los dos desarrolladores/agents de C++.

## Regla obligatoria
- Cada cambio relevante debe incluir una entrada en `SESSION_LOG.md` usando `MEMORY_DUMP_TEMPLATE.md`.
- El log debe explicar impacto en gameplay, red/replicacion y riesgos.

## Objetivo de Demo (fuente de verdad)
- Menu principal
- Lobby con inicio de partida
- Nivel para 2 tortugas con:
  - 2 interactuables directos
  - 2 objetos pickup
  - zonas de muerte (ej. gaviotas)
  - amenaza ambiental que avanza (ej. niebla)
- Pantalla final al ganar
- Pausa con volver a menu o lobby
- Sistema primitivo de cosmeticos (guardado local, GUI de equipar/desequipar)

## Flujo de trabajo sugerido
1. Crear rama de tarea.
2. Implementar con cambios pequeños y testeables.
3. Registrar memory dump tecnico en `SESSION_LOG.md`.
4. Compartir hallazgos de rendimiento/red antes de merge.

## Paso a paso en el Editor (estado actual)
1. Abre `LVL_HQ` y `LVL_Run` y verifica en **World Settings**:
   - `LVL_HQ` -> `GameMode Override` = `BP_HQGameMode` (o derivado de `TN_HQGameMode`).
   - `LVL_Run` -> `GameMode Override` = `BP_RunGameMode` (o derivado de `TN_RunGameMode`).
2. En `Content/Blueprints/Gameplay/Controls/`, crea las Input Actions faltantes:
   - `IA_Interact`
   - `IA_RotateInventory`
3. Abre `IMC_Player` y agrega mappings sugeridos:
   - `IA_Interact` -> `E`
   - `IA_RotateInventory` -> `R`
4. Crea Blueprints para los interactuables C++:
   - `BP_DirectInteractable` hijo de `ATN_DirectInteractableBase`.
   - `BP_PickupInteractable` hijo de `ATN_PickupInteractableBase`.
5. En `LVL_Run`, coloca al menos:
   - 2 instancias de `BP_DirectInteractable`.
   - 2 instancias de `BP_PickupInteractable`.
   - Varios `PlayerStart`.
6. Configura visuales básicos en cada BP:
   - Mesh del actor.
   - Texto de prompt (propiedad `PromptText`).
   - En pickups, define `PickupItem` (`ItemId` y mesh de equipado).
7. Prueba funcional recomendada en **Standalone**:
   - Cliente 1: Host desde menú.
   - Cliente 2: Join/Find.
   - En run map: recoger pickup, rotar inventario (`R`) e interactuar con directos (`E`).
8. Validación esperada:
   - El pickup desaparece al recogerse y no se duplica.
   - El slot equipado cambia al rotar inventario.
   - Interacciones directas se ejecutan solo con autoridad de servidor.

