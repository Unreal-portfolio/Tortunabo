"""Importa el mapa volumetrico fijo y monta su nivel.

Se ejecuta DENTRO del editor de Unreal (headless):
    UnrealEditor-Win64-DebugGame-Cmd.exe <uproject> -run=pythonscript
        -script=<repo>/Scripts/import_terrain_mesh.py -EnablePlugins=PythonScriptPlugin
        -unattended -nosplash -nullrhi

Lee Scripts/terrain_volumes/<nombre>/manifest.json (lo escribe gen_terrain_volume.py) y:
    - crea un DA_<trozo> (UTN_TerrainMeshAsset) por trozo en /Game/Terrain/Volumes/<nombre>,
      cargado desde su binario;
    - crea o rehace el nivel /Game/Maps/Run/LVL_<nombre>: un ATN_TerrainMeshTile por trozo en su
      sitio, lamina de agua, sol, cielo, PlayerStart en el inicio y el GameMode de la demo.

Variables de entorno (opcionales):
    TN_VOLUME_DIR   carpeta con manifest.json (por defecto Scripts/terrain_volumes/Mapa01)
"""

import json
import math
import os

import unreal

GRIDMAP_ROOT = "/Game/Blueprints/Gameplay/GridMap"
TERRAIN_MATERIAL_PATH = f"{GRIDMAP_ROOT}/M_GridTerrain"
FOLIAGE_MATERIAL_PATH = f"{GRIDMAP_ROOT}/M_GridJunk"
WATER_MATERIAL_PATH = f"{GRIDMAP_ROOT}/M_GridWater"
GAME_MODE_PATH = f"{GRIDMAP_ROOT}/BP_GridDemoGameMode"
PLANE_MESH_PATH = "/Engine/BasicShapes/Plane"
TILE_CLASS_PATH = "/Script/Tortunabo.TN_TerrainMeshTile"
PLANE_SIZE_UU = 100.0
PLAYER_STARTS = 4
PLAYER_START_RING_UU = 250.0
PLAYER_START_LIFT_UU = 150.0

asset_lib = unreal.EditorAssetLibrary
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()


def project_dir():
    return unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())


def load_or_none(path):
    return asset_lib.load_asset(path) if asset_lib.does_asset_exist(path) else None


def build_assets(volume_dir, manifest, root):
    if asset_lib.does_directory_exist(root):
        if not asset_lib.delete_directory(root):
            raise RuntimeError(f"No se pudo borrar {root}: cierra el nivel que usa sus trozos")
    assets = {}
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.TN_TerrainMeshAsset)
    for cell in manifest["cells"]:
        name = f"DA_{cell['name']}"
        asset = asset_tools.create_asset(name, root, unreal.TN_TerrainMeshAsset, factory)
        path = os.path.join(volume_dir, cell["file"]).replace("\\", "/")
        if not asset.load_from_file(path):
            raise RuntimeError(f"{name}: LoadFromFile rechazo {path}")
        asset_lib.save_loaded_asset(asset)
        assets[cell["name"]] = asset
    return assets


def spawn(actor_class, label, location, rotation=unreal.Rotator(0.0, 0.0, 0.0)):
    actor = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).spawn_actor_from_class(actor_class, location, rotation)
    actor.set_actor_label(label)
    return actor


def build_level(manifest, assets, level_path):
    level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if asset_lib.does_asset_exist(level_path):
        level_subsystem.load_level(level_path)
        for actor in actor_subsystem.get_all_level_actors():
            actor_subsystem.destroy_actor(actor)
    else:
        level_subsystem.new_level(level_path)

    terrain_material = load_or_none(TERRAIN_MATERIAL_PATH)
    foliage_material = load_or_none(FOLIAGE_MATERIAL_PATH)
    tile_class = unreal.load_class(None, TILE_CLASS_PATH)
    for cell in manifest["cells"]:
        x, y = cell["center_uu"]
        tile = spawn(tile_class, f"Terrain_{cell['name']}", unreal.Vector(x, y, 0.0))
        tile.set_editor_property("terrain_material", terrain_material)
        tile.set_editor_property("foliage_material", foliage_material)
        tile.set_editor_property("mesh_asset", assets[cell["name"]])
        tile.set_folder_path("Terrain")

    # Lamina de agua: el plano del motor mide PLANE_SIZE_UU; cubre el mapa entero.
    extent = manifest["grid"] * manifest["cell_uu"]
    center = (manifest["grid"] - 1) * manifest["cell_uu"] * 0.5
    water = spawn(unreal.StaticMeshActor, "Water", unreal.Vector(center, center, manifest["water_uu"]))
    component = water.static_mesh_component
    component.set_static_mesh(unreal.load_asset(PLANE_MESH_PATH))
    component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    component.set_cast_shadow(False)
    water_material = load_or_none(WATER_MATERIAL_PATH)
    if water_material:
        component.set_material(0, water_material)
    water.set_actor_scale3d(unreal.Vector(extent / PLANE_SIZE_UU, extent / PLANE_SIZE_UU, 1.0))

    spawn(unreal.DirectionalLight, "Sun", unreal.Vector(0.0, 0.0, 5000.0), unreal.Rotator(0.0, -50.0, 35.0))
    spawn(unreal.SkyAtmosphere, "SkyAtmosphere", unreal.Vector(0.0, 0.0, 0.0))
    sky = spawn(unreal.SkyLight, "SkyLight", unreal.Vector(0.0, 0.0, 5000.0))
    sky_component = sky.get_component_by_class(unreal.SkyLightComponent)
    sky_component.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
    sky_component.set_editor_property("real_time_capture", True)
    sky_component.set_editor_property("intensity", 2.0)

    # PlayerStart en corro alrededor del inicio de la ruta, sobre el suelo del mapa.
    sx, sy, sz = manifest["start_uu"]
    for k in range(PLAYER_STARTS):
        angle = 2.0 * math.pi * k / PLAYER_STARTS
        location = unreal.Vector(sx + PLAYER_START_RING_UU * math.cos(angle), sy + PLAYER_START_RING_UU * math.sin(angle),
                                 sz + PLAYER_START_LIFT_UU)
        spawn(unreal.PlayerStart, f"PlayerStart_{k}", location)

    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    game_mode = asset_lib.load_blueprint_class(GAME_MODE_PATH) if asset_lib.does_asset_exist(GAME_MODE_PATH) else None
    if game_mode:
        world.get_world_settings().set_editor_property("default_game_mode", game_mode)
    level_subsystem.save_current_level()


def main():
    volume_dir = os.environ.get("TN_VOLUME_DIR", f"{project_dir()}/Scripts/terrain_volumes/Mapa01")
    with open(f"{volume_dir}/manifest.json", encoding="utf-8") as handle:
        manifest = json.load(handle)
    name = manifest["name"]
    assets = build_assets(volume_dir, manifest, f"/Game/Terrain/Volumes/{name}")
    build_level(manifest, assets, f"/Game/Maps/Run/LVL_{name}")
    unreal.log(f"[TerrainMesh] {len(assets)} trozos importados; nivel /Game/Maps/Run/LVL_{name} guardado")


main()
