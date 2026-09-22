# Bitácora de desarrollo

Registro cronológico de sesiones. Solo se añade; una entrada por sesión, con lo hecho,
lo verificado y lo que queda. El detalle técnico vive en los commits y en `Docs/`.

## 2026-09-22 — Merge de DisenoIng, plugin MCP y terreno por módulos de 400 m

Rama de trabajo: `feat/procgen-terrain` (HEAD `49079d5`, 24 commits sobre main, sin push).
main: `c231ef3`.

### Repositorio

- Merge de `DisenoIng` (Mokius, 2026-04-29) en main: registro por tag de botones y
  placas, `BP_Chunk_Medium_Personaliced`, fallback de ragdoll portado a
  `TortugaCharacter_Knockdown.cpp`, `MaxConcurrent` en `ATN_SpawnZoneBase`. Binarios en
  conflicto (`BP_ChunkManager`, `BP_QuadActor`): se conservó main. Pendiente manual:
  volver a añadir `BP_Chunk_Medium_Personaliced` al pool de `BP_ChunkManager`.
- `.gitattributes`: `.uasset`, `.umap` y similares como binarios.
- `UnrealMCPython` versionado en main (Source/Content/Resources, 1,8 MB); el script
  `setup-unreal-mcp.ps1` queda para reinstalar o registrar el servidor en Claude Code.
- Ramas locales borradas: `merge/diseno-ing`, `feat/procgen-grid-demo`.

### Terreno por módulos (decisión de Rodrigo: celda de 400 m, módulos como plantilla)

- Runtime: `UTN_TerrainModuleAsset`, `ATN_TerrainModuleTile`,
  `TN_TerrainModuleDecisions.h`, `TN_TerrainModuleWallDecisions.h`, modo módulos en
  `ATN_GridMapGenerator` (grid 4x4, celda 40 000 en `LVL_ProcGenDemo`).
- Pipeline: `Scripts/gen_terrain_modules.py` (PNG 16 bits, borde canónico verificado)
  → `Scripts/import_terrain_modules.py` (DA_M_* + BP_M_* en `/Game/Terrain/Modules`).
- Verificado en editor headless: 300 módulos importados, 5 tests
  `Tortunabo.TerrainModule.*` verdes, smoke `-game` del nivel de demo OK, con rutas
  secundarias (atajos, rutas altas) y puentes como instancias.
- Hecho pero solo con build del target de juego: muros de basura en bocas no usadas
  (`BlockedExits` + `WallSeed` replicados), cobertura de salidas (T y cruz valen para
  rectas), test `CoveringAndWalls`, librería de 600 PNG con 5 estilos.

### Siguiente sesión

1. Cerrar editor, `Build.bat TortunaboEditor Win64 DebugGame`.
2. `import_terrain_modules.py` (600 módulos, headless).
3. `Automation RunTests Tortunabo.TerrainModule` (6) y smoke `-game`.
4. Reabrir editor con `UnrealEditor-Win64-DebugGame.exe`, mirar muros y puentes en PIE.
5. Push de `feat/procgen-terrain` y merge a main.

Kanban: `card-1790093849652-b7kug3`. Diseño: `Docs/2026-09-22-Terreno-Modulos-400m.md`.
