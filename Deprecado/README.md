# Deprecado

Código y scripts retirados del proyecto sin borrarlos, para no perder trabajo de diseño.
Nada de esta carpeta se compila ni se ejecuta: `Deprecado/Source` está fuera del módulo
`Source/Tortunabo`, y los assets de Content retirados viven en `/Game/_Deprecado`, que
`Config/DefaultGame.ini` excluye del cocinado (`DirectoriesToNeverCook`).

Para recuperar algo: `git mv Deprecado/<ruta> <ruta>` (conserva el historial) y compilar.
El inventario completo, con la evidencia de cada entrada, está en
`Docs/Limpieza-2026-09-29.md`.

## Entradas

| Ruta original | Origen | Motivo |
|---|---|---|
| `Source/Tortunabo/{Public,Private}/Player/TortugaFirstPersonCharacter.*` | Personaje en primera persona de la plantilla inicial | 0 referencias en C++, Config y Content (limpieza 1.3) |
| `Source/Tortunabo/{Public,Private}/UI/HUD/TN_CosmeticsMenuWidget.*` y `World/TN_CosmeticsStationInteractable.*` | Primer menú de cosméticos (Rodrigo y Mokius) | Ningún WBP hereda de él; la tienda vigente es `UI/Shop/TN_ShopWidgets` (limpieza 1.4) |
| `Source/Tortunabo/{Public,Private}/Player/TN_LegAnimComponent.*` | Animación procedural de patas (Rodrigo y Mokius) | Ninguna cabecera lo incluye y 0 referencias en Content (limpieza 1.5) |
| `build_check.bat` | Script de compilación de Mokius | Ruta fija a su equipo y configuración `Development`; el editor usa `DebugGame` (limpieza 1.6) |
| `Scripts/import_terrain_modules.py` | Importador de módulos del terreno en rejilla (Rodrigo) | Generaba las celdas `BP_M_Mapa01_*`, que ahora están en `/Game/_Deprecado`; lo sustituye el mapa volumétrico (limpieza 2.1) |
| `Scripts/terrain_presets/Mapa01/` | Salida de `gen_terrain_biomes.py` (115 ficheros) | Solo la leía `import_terrain_modules.py` (limpieza 2.1) |
| `Scripts/build_grid_demo_assets.py` | Versión completa, anterior al recorte | El script vivo solo crea materiales y GameMode; esta copia conserva los tiles, `BP_GridMapGenerator` y `LVL_ProcGenDemo` (limpieza 2.1) |

## Content retirado (`/Game/_Deprecado`, misma ruta que el original)

Movido con el editor (referencias corregidas y sin redirectores). Las clases C++ de los BP
retirados siguen compilando en `Source/Tortunabo` para que estos assets carguen.

- `Terrain/Volumes/Mapa01/DA_M_Mapa01_*` (36): intermedios de `import_terrain_mesh.py`.
- `Blueprints/Gameplay/GridMap/BP_GridMapGenerator`, `BP_GridTerrainTile`, `BP_GridFiller_*` y
  `BP_GridTile_*`, `Terrain/Presets/Mapa01/Cells` (72) y `Maps/Run/LVL_ProcGenDemo`: generador en rejilla.
- `Maps/Run/LVL_ProcMap_Terrain`: visor de terreno para comparar generadores (Mokius).
- `Blueprints/Characters/SM_Tortuga_Merged` (antes `SM_MERGED_TORTUGANIGGER_C_1`) y `BP_TortugaCharacter1`.
- `Blueprints/Characters/Meshes/Gorro11`, `Gorro21`, `Gorro31` y `Herizo` (Mokius; los sustituyen los `SM_Helmet_*`).
- `Blueprints/Gameplay/Hazards/BP_BreakablePlatform`, `BP_ScriptedDeathZone` y `BP_PhysicsObject`,
  `Items/BP_TotemInteractable`, `Interaction/BP_TutorialEntryInteractable`, `OLD/BP_FinishLineVolume_DEPRECATED`
  y `Debug/M_DebugVC`.
