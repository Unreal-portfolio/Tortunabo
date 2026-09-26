# Inventario de Scripts — Tortunabo

> Documento auxiliar para la defensa: lista exhaustiva de cabeceras C++ (`.h`)
> por dominio, con descripción breve y autores. Los autores provienen del
> historial de `git log --follow --format="%an"` deduplicado por archivo.
>
> Generado: 2026-05-13. Cobertura: **73 archivos** en `Source/Tortunabo/Public/`.

---

## Core/ — Estado replicado y tipos compartidos

| Archivo | Propósito | Autores |
|---|---|---|
| `ITN_EnemyTargetInterface.h` | Contrato común para enemigos vulnerables a ítems del jugador (stun/blind). | Rodrigo Fernandez |
| `TN_CoopGameState.h` | GameState replicado: fuente de verdad del estado de partida (flow, conteos, scoreboard, QuickChat). | Mokius, Rodrigo Fernandez |
| `TN_CoopPlayerState.h` | PlayerState replicado por jugador con flow individual, cosméticos y métricas de carrera. | Mokius, Rodrigo Fernandez |
| `TN_CosmeticsTypes.h` | Structs `FTN_HelmetData` y `FTN_SkinData` para DataTables de cosméticos. | Mokius, Rodrigo Fernandez |
| `TN_InventoryTypes.h` | Enum de tipos de uso + struct `FTN_InventoryItem` (fila de DataTable de ítems). | Mokius, Rodrigo Fernandez |
| `TN_LevelTargetSubsystem.h` | Registry global por tag (`FName → AActor`) para resolver targets desde chunks runtime. | Rodrigo Fernandez |
| `TN_MatchFlowTypes.h` | Enum `ETNMatchFlowState` + structs `FTN_QuickChatEntry` y `FTN_RaceResultEntry`. | Mokius, Rodrigo Fernandez |

## Game/ — Reglas de carrera

| Archivo | Propósito | Autores |
|---|---|---|
| `TN_RunGameMode.h` | GameMode de la fase Run: countdown, meta, DBNO, scoring (rank+pickups+timebonus), travel a HQ. | Mokius, Rodrigo Fernandez |
| `TN_ProcMapGameMode.h` | GameMode de LVL_ProcMap: rondas Coop/Carrera/2vs2, reaparición en pilas de huevos, tormenta por el camino, espera al mapa de todos. | Claude |
| `TN_ProcMapGameState.h` | GameState del mapa procedural: modo, ronda, objetivo y resultado de la ronda replicados. | Claude |

## Lobby/ — Lobby HQ

| Archivo | Propósito | Autores |
|---|---|---|
| `TN_HQGameMode.h` | GameMode del lobby HQ: ready-up, countdown, travel a Run, ruta inicial al tutorial. | Mokius, Rodrigo Fernandez |
| `TN_LobbyReadyZone.h` | TriggerBox que marca a los jugadores como ready cuando su pawn entra en la zona. | Mokius, Rodrigo Fernandez |
| `TN_ProcModeSelector.h` | Interactuable del lobby para elegir modo (Clásico/Coop/Carrera/2vs2) y dificultad. | Claude |

## Menu/ — Menú principal

| Archivo | Propósito | Autores |
|---|---|---|
| `MP_MenuGameMode.h` | GameMode mínimo del mapa de menú (LVL_Menu). | Mokius, Rodrigo Fernandez |
| `MP_MenuPlayerController.h` | PlayerController que crea/muestra el widget del menú principal. | Mokius, Rodrigo Fernandez |

## Multiplayer/ — Sesiones online y persistencia

| Archivo | Propósito | Autores |
|---|---|---|
| `MP_GameInstance.h` | GameInstance global: sesiones Steam, cosméticos, score, tutorial, loading screen, auto-rejoin. | Mokius, Rodrigo Fernandez |
| `TN_CosmeticSaveGame.h` | SaveGame con cascos desbloqueados, equipados (helmet+skin) y score acumulado. | Mokius, Rodrigo Fernandez |
| `TN_TutorialSaveGame.h` | SaveGame con el flag de tutorial completado del jugador local. | Mokius, Rodrigo Fernandez |

## Player/ — Personaje y componentes

| Archivo | Propósito | Autores |
|---|---|---|
| `MP_GamePlayerController.h` | PlayerController principal: HUD, espectador, cosméticos, ruedas radiales, QuickChat, VOIP. | Mokius, Rodrigo Fernandez |
| `TN_CarryComponent.h` | Coger y lanzar tortugas en caparazón o aturdidas: forcejeo de 2 s, temblor de cámara, rebote al caer. | Claude |
| `TN_InventoryComponent.h` | Inventario de 2 slots (equipado+guardado) replicado, con visual attacheado al socket. | Mokius, Rodrigo Fernandez |
| `TN_LegAnimComponent.h` | Componente de animación pendular genérico para personajes ensamblados con primitivas. | Mokius, Rodrigo Fernandez |
| `TN_ProcAnimInstance.h` | AnimInstance mínimo que permite a C++ controlar transforms de huesos sin AnimBP. | Rodrigo Fernandez |
| `TN_StaminaComponent.h` | Stamina con sprint, recarga, agotamiento, boost ilimitado y penalización post-boost. | Mokius, Rodrigo Fernandez |
| `TortugaCharacter.h` | Personaje principal: movimiento, dive aéreo, knockdown, items, cosméticos, emotes, VOIP. | Mokius, Rodrigo Fernandez |
| `TortugaFirstPersonCharacter.h` | Variante en primera persona usada en cinemáticas (hereda y fuerza SpringArm a 0). | Rodrigo Fernandez |

## Voice/ — VOIP por proximidad

| Archivo | Propósito | Autores |
|---|---|---|
| `ProximityVoiceComponent.h` | Componente VOIP WASAPI: captura, downsample, compresión y multicast filtrado por proximidad. | Mokius, Rodrigo Fernandez |

## UI/ — Widgets HUD, menús y radiales

| Archivo | Propósito | Autores |
|---|---|---|
| `Menu/MP_MainMenuWidget.h` | Widget del menú principal (Host/Find/Quit + status). | Mokius, Rodrigo Fernandez |
| `Voice/VoiceIndicatorWidget.h` | Indicador HUD del estado de VOIP del jugador local. | Mokius, Rodrigo Fernandez |
| `HUD/TN_CoopFlowHUDWidget.h` | HUD del flujo de partida: status, panel de Resultados, scoreboard y feed de QuickChat. | Mokius, Rodrigo Fernandez |
| `HUD/TN_CosmeticsMenuWidget.h` | Menú de cosméticos: grid de cascos desbloqueados con equip/unequip. | Rodrigo Fernandez |
| `HUD/TN_EmoteWheelDataAsset.h` | DataAsset con el catálogo de emotes para la rueda radial. | Mokius, Rodrigo Fernandez |
| `HUD/TN_InteractPromptWidget.h` | Widget del prompt de interacción ("Pulsa E para..."). | Rodrigo Fernandez |
| `HUD/TN_LoadingScreenWidget.h` | Loading screen entre transiciones de mapa. | Rodrigo Fernandez |
| `HUD/TN_PlayerHUDWidget.h` | HUD principal: stamina, inventario, RaceScore, hooks DBNO/Revive. | Mokius, Rodrigo Fernandez |
| `HUD/TN_QuickChatWheelDataAsset.h` | DataAsset con el catálogo de mensajes de Quick Chat. | Mokius, Rodrigo Fernandez |
| `HUD/TN_RadialWheelTypes.h` | Tipos para ruedas radiales: vista de entrada + enum de tipos. | Mokius, Rodrigo Fernandez |
| `HUD/TN_RadialWheelWidgetBase.h` | Base Blueprintable para ruedas radiales (input → selección, render fondo, hooks BP). | Mokius, Rodrigo Fernandez |

## World/ — Actores del nivel, hazards, items, enemigos

### Bases e interactuables

| Archivo | Propósito | Autores |
|---|---|---|
| `TN_InteractableBase.h` | Base abstracta de interactuables (CanInteract/Interact + prompt 3D + distancia). | Rodrigo Fernandez |
| `TN_DirectInteractableBase.h` | Interactuable de uso directo con cooldown y BP event para el efecto. | Rodrigo Fernandez |
| `TN_PickupInteractableBase.h` | Base de pickups del mundo que añaden ítems al inventario (data-driven via DataTable). | Rodrigo Fernandez |
| `TN_CosmeticsStationInteractable.h` | Estación de cosméticos del lobby; al interactuar abre el menú en el cliente. | Rodrigo Fernandez |
| `TN_TutorialEntryInteractable.h` | Interactuable que teletransporta al jugador a la zona de tutorial en LVL_HQ. | Mokius, Rodrigo Fernandez |
| `TN_SkinStatueActor.h` | Estatua del lobby que muestra un cosmético; al interactuar lo equipa. | Mokius |
| `TN_TotemInteractable.h` | Tótem del nivel; pickup que da el ítem Totem (auto-revive al morir). | Mokius |
| `TN_UmbrellaInteractable.h` | Sombrilla del nivel; al interactuar otorga protección contra hazards aéreos. | Mokius, Rodrigo Fernandez |
| `TN_ButtonInteractable.h` | Botón pulsable que dispara acciones (mover targets, replicado). | Mokius, Rodrigo Fernandez |
| `TN_ButtonGroupManager.h` | Manager de grupo de botones — dispara acciones cuando todos están activados. | Mokius, Rodrigo Fernandez |
| `TN_PressurePlate.h` | Placa de presión y manager de grupo (HoldDuration opcional). | Mokius, Rodrigo Fernandez |

### Pickups e ítems

| Archivo | Propósito | Autores |
|---|---|---|
| `TN_ConchPickup.h` | Concha trampa: pickup que al colocarse en suelo inmoviliza al primer pawn que pase. | Mokius, Rodrigo Fernandez |
| `TN_ScorePickup.h` | Pickup que otorga puntos al RaceScore al ser recogido. | Mokius |
| `TN_RescuePickup.h` | Pickup que respawnea a un jugador muerto en la ubicación al ser activado. | Rodrigo Fernandez |
| `TN_InkProjectile.h` | Proyectil de tinta: ciega temporalmente al jugador impactado. | Mokius, Rodrigo Fernandez |
| `TN_ThrowableItemActor.h` | Proyectil físico genérico para ítems lanzables (bola con replicación por struct). | Mokius, Rodrigo Fernandez |
| `TN_BananaPeel.h` | Plátano en suelo que hace resbalar al primer jugador que entra. | Mokius, Rodrigo Fernandez |

### Enemigos y hazards

| Archivo | Propósito | Autores |
|---|---|---|
| `TN_SeagullActor.h` | Gaviota legacy estática (deprecada; usar `TN_EnemySeagull` + `TN_SeagullSpawnZone`). | Mokius, Rodrigo Fernandez |
| `TN_EnemySeagull.h` | Gaviota dinámica: persigue al jugador, picada física, evita refugios. | Mokius, Rodrigo Fernandez |
| `TN_SeagullDroppingActor.h` | Excremento de gaviota que cae sobre el jugador como hazard. | Mokius, Rodrigo Fernandez |
| `TN_CrabActor.h` | Cangrejo enemigo con patrol points relativos al spawn (compatible con chunks). | Rodrigo Fernandez |
| `TN_JellyfishActor.h` | Medusa flotante con BounceZone superior y daño lateral. | Mokius, Rodrigo Fernandez |
| `TN_QuadActor.h` | Quad / pista que se desplaza entre dos puntos, mata por contacto. | Mokius, Rodrigo Fernandez |
| `TN_StormVolume.h` | Volumen de tormenta con post-process local y daño a quien no esté bajo sombrilla. | Mokius, Rodrigo Fernandez |
| `TN_SlowZoneVolume.h` | Volumen que limita MaxWalkSpeed mientras el pawn está dentro. | Mokius, Rodrigo Fernandez |
| `TN_DeathZoneVolume.h` | Volumen que mata al pawn tras un countdown configurable. | Mokius, Rodrigo Fernandez |
| `TN_ScriptedDeathZone.h` | Death zone disparada por evento (no por volumen estático), con acciones de activate/kill. | Mokius |
| `TN_BreakablePlatform.h` | Plataforma que se rompe al ser pisada tras un delay. | Mokius, Rodrigo Fernandez |
| `TN_BouncePhysicsObject.h` | Objeto físico que rebota cuando un jugador lo golpea (estilo balón). | Rodrigo Fernandez |
| `TN_PhysicsObjectActor.h` | Actor físico genérico con StaticMesh y material, simulación física básica. | Rodrigo Fernandez |

### Zonas, spawners y volúmenes funcionales

| Archivo | Propósito | Autores |
|---|---|---|
| `TN_ItemSpawnZone.h` | Zona que genera ítems aleatorios en BeginPlay validando suelo y obstáculos. | Rodrigo Fernandez |
| `TN_CrabSpawnZone.h` | Zona que spawnea cangrejos cuando un jugador entra (server-only). | Mokius, Rodrigo Fernandez |
| `TN_SeagullSpawnZone.h` | Zona que spawnea gaviotas dinámicas con cap concurrente. | Mokius, Rodrigo Fernandez |
| `TN_DroppingSpawnZone.h` | Zona que spawnea excrementos de gaviota a intervalos. | Mokius, Rodrigo Fernandez |
| `TN_CollectionZone.h` | Zona de recolección: cuenta ítems depositados y dispara goal al alcanzar RequiredCount. | Mokius, Rodrigo Fernandez |
| `TN_GoalZone.h` | Zona objetivo con contador 3D y acciones para puzzles. | Rodrigo Fernandez |
| `TN_FinishLineVolume.h` | Línea de meta de la Run (volumen, no Brush; compatible con chunks). | Mokius, Rodrigo Fernandez |
| `TN_RegisterAsTargetComponent.h` | Componente que registra al actor padre en `TN_LevelTargetSubsystem` con un tag. | Rodrigo Fernandez |

### Chunks (generación procedural)

| Archivo | Propósito | Autores |
|---|---|---|
| `TN_ChunkManager.h` | Manager del nivel procedural: ensambla chunks Easy/Medium/Hard y dispara FinalChunk. | Mokius, Rodrigo Fernandez |

### ProcMap (mapa procedural por módulos) — ver [`Mapa_Procedural.md`](Mapa_Procedural.md)

| Archivo | Propósito | Autores |
|---|---|---|
| `ProcMap/TN_ProcMapEnums.h` | Enums de bioma, modo, dificultad, módulos vacíos y tipo de cruce colosal. | Claude |
| `ProcMap/TN_ProcMapMath.h` | RNG determinista (SplitMix64) y ruido para la lógica pura. | Claude |
| `ProcMap/TN_ProcMapLayout.h` | Tipos del layout: módulos, ruta, portales, cruces, camino, ramas y features. | Claude |
| `ProcMap/TN_ProcMapModules.h` | Módulos irregulares siempre conexos sobre la rejilla. | Claude |
| `ProcMap/TN_ProcMapRoute.h` | Ruta por los módulos con pasos de cruce colosal. | Claude |
| `ProcMap/TN_ProcMapPath.h` | Portales, camino con meandros, anchos, alturas, ramas y carriles. | Claude |
| `ProcMap/TN_ProcMapFeatures.h` | Biomas por regiones, huecos, isletas, huevos, puzles 2vs2, río y reparto de peligros. | Claude |
| `ProcMap/TN_ProcMapTerrain.h` | Altura del terreno por vértice (biomas, camino, estructuras, bordes, costa). | Claude |
| `ProcMap/TN_ProcMapGenerate.h` | Orquestación y validación de la generación. | Claude |
| `ProcMap/TN_ProcMapTypes.h` | Perfiles, DataAssets de ajustes y de bioma, greybox por defecto. | Claude |
| `ProcMap/TN_ProcMapGenerator.h` | Actor que materializa el mapa (terreno, agua, estructuras, vegetación, actores). | Claude |
| `ProcMap/TN_ProcMapActorUtils.h` | Utilidades de red y tinte para los actores del mapa. | Claude |
| `ProcMap/TN_ProcTraversalActors.h` | Géiser, tobogán, zona de muerte y meta del mapa procedural. | Claude |
| `ProcMap/TN_ProcWaterActors.h` | Volumen de agua nadable, corrientes, remolinos, depredador y criaturas rebotadoras. | Claude |
| `ProcMap/TN_ProcEggNest.h` | Pila de huevos de reaparición. | Claude |
| `ProcMap/TN_ProcPuzzleActors.h` | Muro de lanzamiento, compuerta de sabotaje e interruptor del 2vs2. | Claude |
| `ProcMap/TN_PathStorm.h` | Tormenta que avanza por el camino (Coop). | Claude |

---

**Total**: 73 cabeceras públicas, 9 dominios (Core, Game, Lobby, Menu, Multiplayer, Player, Voice, UI, World).
