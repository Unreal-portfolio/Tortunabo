# A · Coop sobre mapas volumétricos fijos (UE 5.6, HEAD 41a0ee8a5)

Solo lectura. Cifras marcadas «est.» son estimaciones; el resto, medido en el repo.

## 1. Arquitectura: fuente de terreno «preparada»

**Principio:** tras importar, el nivel manda. Terreno = StaticMesh; datos de juego (camino, alturas, máscara) = DataAsset cocinable; lo editable (nidos, meta, mecánicas) = marcadores en el nivel.

Piezas nuevas:
- `UTN_CoopMapData : UPrimaryDataAsset` (Public/World/ProcMap/TN_CoopMapData.h): `Main`/`Branches` (muestras P, Z, Width), rejilla horneada (`TopZ` u16, `SafeLow`/`SafeHigh` u8, `PathDist` u16), `KillBoxes`, `Zones` (tramo de S → `ETNProcBiome`), `WaterZ`, puntos de vegetación. `LoadFromManifest(path)` WITH_EDITOR, mismo patrón que `UTN_TerrainMeshAsset::LoadFromFile`.
- `ATN_CoopMarker` (no replicado, se carga con el nivel en todas las máquinas): `Kind` = Start / Finish / EggNest(Order) / BeachElement(`ETNBeachElement` + spec) / Vegetation, `StableId` (FName del manifest).
- `UTN_SafeGroundSubsystem : UWorldSubsystem` + interfaz `ITN_SafeGroundProvider` (`IsAllowed`, `FindRescueSpot`, `CourseBackAt`). La implementan `ATN_BeachRaceGenerator` (lógica actual) y `ATN_ProcMapGenerator` (máscara horneada).

Cambios en `ATN_ProcMapGenerator`:
- `UPROPERTY(EditAnywhere) ETNTerrainSource Source {Procedural, Prepared}` + `TObjectPtr<UTN_CoopMapData> CoopData`.
- `BuildLayout()` (TN_ProcMapGenerator.cpp:237): en Prepared, `TNProcMap::LayoutFromCoopData(Data, Markers, Layout)` en vez de `GenerateLayout`. Main/Branches del DA; Features (EggNest, Finish, StartArea) desde los **marcadores del nivel** (respeta lo que movió el diseñador); `StartPoint`/`EndPoint` desde los marcadores Start/Finish; `WorldSize = grid × cell_uu`; `Modules` sintéticos, uno por zona, para que fauna y peligros tengan bioma.
- `BuildTerrain()` → `BuildTerrainFromBake()`: rellena `Heights` (LatticeSpacing = 100, origen = −cell/2, NX = grid·100+1), `PathDist`; sin mallas, `TerrainDetail` inactivo. `BuildStructures`/`BuildScatter`/`BuildFlora` se saltan, pero **la fauna se spawnea dentro de BuildStructures** (Build.cpp:3305): extraerla a `SpawnFauna()`.
- `BuildWater()`: sin `WaterPlane` (el nivel ya trae la lámina); las cajas nadables (Build.cpp:1696-1742) se reutilizan tal cual sobre `Heights` si `water.swim`.
- Z: el actor generador se coloca en Z = `water_uu` (−400). Así `TNProcMap::SeaLevel = 0` (TN_ProcMapTerrain.h:96) cae en la lámina real sin tocar las decenas de usos de `SeaLevel`; el DA guarda Z en espacio de mapa (Z_mundo − water_uu).

Flujo: manifest v2 + `coop_bake.bin` → (Python, editor) `DA_Coop_<n>` + marcadores + trozos SM → `LVL_Coop_<n>` → runtime: `BuildLayout` (DA + marcadores) → `BuildTerrainFromBake` → `SpawnTraversalActors` (todas) → `SpawnServerActors`/`SpawnHazards`/`SpawnShells` (servidor) → `BuildProgressIndex` (TN_ProcMapGenerator.cpp:502, sin cambios si Main existe).

**Red:** el servidor elige el mapa y viaja a `/Game/Maps/Coop/LVL_Coop_<n>`; los clientes cargan el mismo paquete (seamless), así que DA y marcadores ya están en cada máquina. `FTNProcMapNetConfig` añade `FName PreparedMapId`; en `OnRep_NetConfig` el cliente compara con su `CoopData` y registra error si no casa. `Seed` se sigue replicando: sigue decidiendo conchas y peligros. Late-join: el camino existente de `BeginPlay` (TN_ProcMapGenerator.cpp:64) basta. Selección: `UMP_GameInstance::SelectedCoopMap` + catálogo `UTN_CoopMapCatalog` (Id, `TSoftObjectPtr<UWorld>`, nombre, preview). `ATN_HQGameMode` arma `TravelURL = CoopLevel + ?ProcStart=…` en vez de `ProcMapPath` (TN_HQGameMode.h:115, .cpp:427). `?CoopMap=<id>` solo para pruebas sin lobby.

## 2. Manifest v2

Compatibilidad: se conservan todas las claves v1 (`name, seed, format, cell_uu, grid, water_uu, start_uu, end_uu, cells, description, recorrible, kill_boxes_uu, jellyfish_uu, style, peak_above_water_m`). `ATN_MapVariantLoader` e `import_terrain_mesh.py` ignoran lo nuevo. Se añade:

```json
"manifest_version": 2,
"coop": {
  "bounds_uu": [-5000,-5000,55000,55000],
  "path": {"step_uu":200, "main":[[x,y,z,ancho]], "branches":[{"fork":41,"rejoin":88,"points":[[x,y,z,ancho]]}]},
  "start": {"id":"start","pos":[x,y,z],"yaw":90},
  "finish": {"id":"finish","pos":[x,y,z],"dir":[dx,dy],"width_uu":1500,"depth_uu":1200,"in_water":false},
  "nests": [{"id":"nest_01","pos":[x,y,z],"order":1}],
  "water": {"z_uu":-400,"swim":false},
  "zones": [{"name":"cliffs","biome":"Rocky","s0_uu":0,"s1_uu":12000}],
  "beach_elements": [{"id":"cat_01","element":"Catapult","pos":[x,y,z],"yaw":90,"params":{}}],
  "vegetation": {"file":"coop/vegetation.bin","count":5400},
  "bake": {"file":"coop/bake.bin","origin_uu":[-5000,-5000],"step_uu":100,"nx":601,"ny":601,"z0_uu":-1000,
           "zstep_uu":50,"layers":["top_z_cm:u16","safe_low:u8","safe_high:u8","path_dist_dm:u16"]}
}
```

Todas las `pos` llevan Z del **suelo caminable** (no de la cara superior): resuelve nidos/meta en túneles y voladizos. `bake.bin`: cabecera `TNCB` + capas zlib, como TNTM2. E01: 601² × 6 B = 2,2 MB en bruto; tras zlib, 0,3-0,6 MB (est.).

Productor (Python, `Scripts/terrain_vol/export.py`):
- `write_map` ya recibe lo necesario: `route_points` y `chunk.top` (`global_top`, export.py:173) hoy solo alimentan la preview. Añadir `write_coop(out, …)`.
- Camino: C01 tiene `model.route` ordenado. P01 **no**: su `route` es la concatenación de los puntos de los puentes (gen_terrain_platforms.py:110), sin orden inicio→final. Para P01/E01/F01, `walk()` (gen_terrain_volume.py:57) es un BFS: guardando el padre de cada celda sale el camino más corto inicio→final. Luego se simplifica cada 2 m y el ancho sale de la transformada de distancia de lo caminable.
- `safe_low/high` = nivel (k) más bajo y más alto de `seen` (celdas alcanzables desde el inicio) por columna: es la máscara de zonas permitidas, que ya se calcula para validar `recorrible`.
- `nests`: cada N m de S sobre Main (heurística inicial); `beach_elements` y `vegetation`: del boceto (clase de color) o reglas por zona.
- C01/P01/E01/F01 se regeneran con su semilla (E01 necesita el MDE en disco).

## 3. Import a StaticMesh empaquetable

Ya existe el 80 %: `UTN_TerrainMeshAsset::BuildStaticMesh` (TN_TerrainMeshAsset.cpp:50-150) crea SM con Nanite (Keep 100 %, Fallback 10 %) y `CTF_UseComplexAsSimple`; `import_terrain_mesh.py` monta el nivel. Propuesta, `Scripts/import_terrain_coop.py`, derivado de ese script:
- Nivel persistente `LVL_Coop_<n>` (generador Prepared, GameMode `ATN_ProcMapGameMode`, luz/cielo) + subniveles siempre cargados: `_Terrain` (trozos + agua; lo posee el script), `_Markers` (lo crea el script, lo retoca el diseñador) y `_Design` (**el script nunca lo abre**). Opcional: *Use External Actors* para que Alvaro y Rodrigo no choquen en un `.umap` binario.
- `/Game/Maps` ya está en `DirectoriesToAlwaysCook` (DefaultGame.ini:22): los `LVL_Coop_*` se cocinan solos. Los `DA_<trozo>` (TNTM2 crudo) no los referencia el nivel y no se cocinan; `DA_Coop_<n>` sí (lo referencia el generador).

Coste, medido en Mapa01: 36 trozos, 1,78 M triángulos. TNTM2 ocupa 13 MB, SM 31 MB y DA_ 41 MB (uasset de editor). Son unos 17 MB por millón de triángulos en SM de editor.

| Mapa | Triángulos | TNTM2 | SM editor (est.) |
|---|---|---|---|
| P01 | 0,40 M | 2,5 MB | ~7 MB |
| C01 (156 trozos con corona) | 1,06 M | 4,2 MB | ~18 MB |
| E01 | 1,08 M | 5,8 MB | ~19 MB |
| F01 | 0,98 M | 4,4 MB | ~17 MB |

ProcMesh actual: cada máquina lee `Scripts/` (TN_MapVariantLoader.cpp:157, no empaquetado), infla zlib y cocina la colisión de ~1 M triángulos **síncrona en el hilo de juego** (:344), sin Nanite ni LOD: parón de 2-6 s por máquina (est.; medir con el log de :358). StaticMesh: colisión ya cocinada en el paquete, streaming Nanite, nada en BeginPlay. Paquete cocinado 15-40 MB por mapa; colisión trimesh de E01, 30-45 MB de RAM (est.; medir con `UnrealPak -List`). Sin World Partition ni HLOD: mapas de 300-600 m.

## 4. Seguridad «fuera de zona»

Estado: el coop no tiene red de seguridad; la carrera sí (`ATN_BeachRaceGameMode::RescueTurtle`, TN_BeachRaceGameMode.cpp:2259, `ResolveRescueTarget`, `UnderSandWatch`).

Propuesta (solo servidor):
1. `IsAllowed(P)`: celda de la máscara con `safe_low ≤ k(P.Z) ≤ safe_high + 1` y dentro de `bounds_uu`.
2. Vigilante en `ATN_ProcMapGameMode`, cada 0,25 s por pawn: en suelo permitido guarda `LastSafe` (posición + progreso). En suelo **no permitido** más de 1,5 s (pozo inalcanzable, catapulta o trampolín fuera del recorrido) o fuera de `bounds`, se rescata. Agua y barrancos siguen siendo muerte por `kill_boxes_uu` y reaparición en nido.
3. Destino: la muestra de Main de mayor S ≤ `LastSafe.Progress`, en anillos si está ocupada. Z = Z de la muestra + media cápsula, validada con traza **desde la Z del camino + 2 m** (desde +30 m da el techo del túnel). Reutilizar `TNBeach::ReleaseTurtle` + teletransporte + aturdimiento (TN_BeachStun.cpp:341-365).
4. Gaviotas: `CourseBack` hoy es −X sin generador de playa (TN_BeachGullZone.cpp:323-331); pasa a `Provider->CourseBackAt(P)` = −dirección del camino. Al soltar (`ReleaseCarried`, :1247), si el punto de caída previsto no está permitido, se rescata al aterrizar.
5. `TNBeach::FindOpenSandSpot` (TN_BeachStun.cpp:373-377, 409-411) exige `ATN_BeachRaceGenerator` y límites de la playa. Hay que consultar primero el subsistema; si el proveedor es el coop, se usa la máscara.

## 5. Flujo del diseñador y del agua

1. Boceto PNG con leyenda de colores (plataformas, río, puente; nuevos: nido, meta, catapulta, trampolín, vegetación) → `extract_layout.py` → `*_layout.png`. El diseño adaptado por IA produce el mismo PNG de clases.
2. `gen_terrain_*.py` → `Variants/<n>/`. `write_map` borra la carpeta (export.py:151): nada manual ahí.
3. Vista previa sin importar: `ATN_MapVariantLoader`.
4. `import_terrain_coop.py` (headless) → SM, `DA_Coop_<n>`, `LVL_Coop_<n>` con subniveles.
5. Retoque: mover o borrar marcadores en `_Markers`, colocar lo que quiera en `_Design`.
6. `TN_REGENERATE=1`: recarga en sitio SM y DA (mismas rutas y referencias). En `_Markers` solo crea ids **nunca emitidos** (lista en `ATN_CoopMapAnchor.KnownIds`): id conocido sin actor = lo borró el diseñador, no se recrea. Nunca mueve un marcador.
7. Esculpir en Modeling Mode deja viejo el bake: `CallInEditor RebakeHeightsFromLevel` retraza `TopZ` (361 k trazas en E01); la máscara exige volver a Python.

Agua: lámina `StaticMeshActor` a `water_uu` (hoy −400 uu = −4 m, `WATER_M` en terrain_vol/layout.py:22) sin colisión, con `M_GridWater`, como hace ya import_terrain_mesh.py:136-146. Etiqueta `TN_Water` para superficie, foley y audio. Nadable solo si `water.swim` (C01 río); P01/E01/F01, muerte por caja.

## 6. Tareas (≈ 64 h) y riesgos

| # | Tarea | h |
|---|---|---|
| 1 | Python: camino por BFS con padres, ancho, `safe_low/high`, `bake.bin`, manifest v2 + pytest | 10 |
| 2 | C++: `UTN_CoopMapData` + `LoadFromManifest` + test de automatización | 5 |
| 3 | C++: `Source=Prepared` en `BuildLayout`/`BuildTerrainFromBake`, módulos sintéticos, `SpawnFauna` extraída | 9 |
| 4 | C++: correcciones de la lista de riesgos (meta, nidos, suelo bajo el pawn) | 4 |
| 5 | C++: `ATN_CoopMarker`; el servidor spawnea mecánicas Beach desde marcadores con su Z | 4 |
| 6 | Python: `import_terrain_coop.py` (subniveles, ids estables, lápidas) | 6 |
| 7 | C++: `UTN_SafeGroundSubsystem`, vigilante coop, gaviotas, `FindOpenSandSpot` | 9 |
| 8 | `ATN_BeachDecorField`: entrada de puntos del DA | 3 |
| 9 | Catálogo, lobby, `TravelURL`, pantalla de carga, validación de `PreparedMapId` | 4 |
| 10 | Pruebas: smoke `-game` headless por mapa, PIE 4P listen, medición de cook, carga y colisión | 10 |

Riesgos:
- **R1** TN_ProcMapGenerator.cpp:382: `MapCollisionUnder` solo acepta `Hit.GetActor()==this`. Con trozos StaticMeshActor el pawn queda congelado hasta 10 s (:359). Aceptar etiqueta `TN_MapTerrain`.
- **R2** TN_ProcMapGenerator.cpp:85: la celebración de meta asume la meta orientada a +Y (compara `P.Y` con `Location.Y + Length`). Usar `F.Dir`.
- **R3** TN_ProcMapGenerator_Spawn.cpp:444: el volumen de meta va a `SeaLevel − 300`. En P01 la meta está a +25 m (end_uu z = 2594): el volumen quedaría 30 m por debajo. Usar la Z del marcador si `in_water=false`.
- **R4** Spawn.cpp:431 (nidos) y :231 (salidas): `TerrainHeightMap` da el techo en túnel. Usar la Z del marcador o del camino.
- **R5** Build.cpp:3305 + TN_ProcFauna.cpp:259: la fauna nace en `BuildStructures` y exige `Layout.Modules`. Saltarse las estructuras la apaga en silencio.
- **R6** TN_AmbientSoundscape.cpp:424: `CoastY` (TN_ProcMapLayout.h:942) es ruido de semilla y no vale para mapas fijos; hace falta un `SeaNear` desde la máscara de agua.
- **R7** TN_ProcMapGenerator.cpp:520: `ProgressOrigin` fijo en −20000 con `WorldSize`. Vale si `WorldSize = grid·cell_uu`; hay que fijarlo.
- **R8** TN_LoadingScreenSubsystem.cpp:539/614/1145: detecta partida por `MapName.Contains("ProcMap")`. `LVL_Coop_*` rompe la espera del generador.
- **R9** TN_TerrainMeshAsset.cpp:130-140: Nanite con reserva al 10 % y colisión compleja. Hay que verificar en 5.6 que la colisión sale de la malla fuente y no de la de reserva (traza contra SM frente a TNTM2 en Mapa01).
- **R10** 22 ficheros llaman a `ATN_BeachRaceGenerator::Find` (TN_BeachMine, TN_BeachEnemy, TN_BeachLoot, TN_RaceGullStrike…): fuera de la playa pueden quedar inertes. Auditar cada mecánica que pase al coop.
- **R11** DefaultGame.ini:22 cocina todo `/Game/Maps`, también los niveles de prueba.
- **R12** `.umap` binario compartido con Alvaro: los subniveles mitigan, no eliminan.
