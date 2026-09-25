"""Monta /Game/Maps/Run/LVL_MapVariants: el nivel donde los disenadores prueban las variantes
de mapa de Scripts/terrain_volumes/Variants/ (generadas aparte, indexadas en su index.json).

Se ejecuta DENTRO del editor de Unreal (headless), igual que Scripts/import_terrain_mesh.py:
    UnrealEditor-Win64-DebugGame-Cmd.exe <uproject> -run=pythonscript
        -script=<repo>/Scripts/build_variants_level.py -EnablePlugins=PythonScriptPlugin
        -unattended -nosplash -nullrhi

Crea, si no existe, un ATN_MapVariantLoader (C++, Source/Tortunabo/Public/World/
TN_MapVariantLoader.h) con la primera variante de Variants/index.json, sol y cielo igual que
LVL_Mapa01, la misma agua que build_water.py (reutiliza build_water.build_material /
build_surface_mesh: se importa ese modulo en vez de duplicar el material) y un PlayerStart con
el tag "MapVariantStart" que ATN_MapVariantLoader mueve solo al inicio de la variante elegida.
El GameMode override es el mismo Blueprint que usa LVL_Mapa01.

Nota: NO se importa Scripts/import_terrain_mesh.py. Su main() se ejecuta entero al importarlo
(no esta protegido con "if __name__ == '__main__'") y con LVL_Mapa01 ya existente lanza
RuntimeError sin TN_REGENERATE=1; con TN_REGENERATE=1 si que importaria, pero de paso
reconstruiria los 36 StaticMesh de Mapa01 en cada ejecucion de este script, un coste que no
compensa reusar tres funciones de una linea. Esas (spawn, configuracion del sol, actores por
etiqueta) se repiten aqui, iguales a como las usa import_terrain_mesh.py. build_water.py no
tiene ese problema: su main() esta protegido con "if __name__ == '__main__'" (a diferencia de
import_terrain_mesh.py), asi que importarlo no ejecuta nada por si solo y su build queda bien
igualmente si se repite; por eso se importa tal cual.

Variables de entorno (opcionales):
    TN_VARIANTS_DIR   carpeta con index.json (por defecto Scripts/terrain_volumes/Variants)
    TN_VARIANT        nombre de variante a elegir al crear el loader (por defecto la primera
                       de index.json; no tiene efecto si el loader ya existe en el nivel)
"""

import json
import os
import sys

import unreal

# El commandlet no pone la carpeta del script en sys.path: build_water vive al lado.
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import build_water  # noqa: E402

LEVEL_PATH = "/Game/Maps/Run/LVL_MapVariants"
LOADER_LABEL = "MapVariantLoader"
PLAYER_START_TAG = "MapVariantStart"
GAME_MODE_PATH = "/Game/Blueprints/Gameplay/GridMap/BP_GridDemoGameMode"  # el mismo que LVL_Mapa01
WATER_CENTER_UU = (0.0, 0.0)
WATER_Z_UU = -400.0
HORIZON_SCALE = 1200.0

asset_lib = unreal.EditorAssetLibrary
actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def project_dir():
    return unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())


def variants_dir():
    return os.environ.get("TN_VARIANTS_DIR", f"{project_dir()}/Scripts/terrain_volumes/Variants")


def first_variant_name(directory):
    """Nombre de la variante a elegir al crear el loader: TN_VARIANT si se fijo, si no la
    primera de index.json."""
    forced = os.environ.get("TN_VARIANT")
    if forced:
        return forced
    index_path = f"{directory}/index.json"
    with open(index_path, encoding="utf-8") as handle:
        entries = json.load(handle)
    if not entries:
        raise RuntimeError(f"{index_path} esta vacio: no hay ninguna variante que elegir")
    return entries[0]["name"]


def existing_labels():
    return {a.get_actor_label(): a for a in actor_subsystem.get_all_level_actors()}


def spawn(actor_class, label, location, rotation=unreal.Rotator(0.0, 0.0, 0.0)):
    actor = actor_subsystem.spawn_actor_from_class(actor_class, location, rotation)
    actor.set_actor_label(label)
    return actor


def configure_sun(sun):
    """Misma configuracion que import_terrain_mesh.configure_sun: sombras suaves sobre el
    terreno de marching cubes."""
    light = sun.get_component_by_class(unreal.DirectionalLightComponent)
    light.set_editor_property("shadow_bias", 0.9)
    light.set_editor_property("shadow_slope_bias", 0.9)
    light.set_editor_property("light_source_angle", 1.5)


def place_water(actors):
    """Misma agua que build_water.py (mismo material y malla, reutilizados por import):
    build_water.place_in_level esta atada a LVL_Mapa01 y no sirve tal cual en este nivel."""
    if "Water" in actors:
        return
    normal_texture = build_water.import_texture("T_WaterNormal", unreal.TextureCompressionSettings.TC_VECTOR_DISPLACEMENTMAP)
    foam_texture = build_water.import_texture("T_WaterFoam", unreal.TextureCompressionSettings.TC_GRAYSCALE)
    material = build_water.build_material(normal_texture, foam_texture)
    surface = build_water.build_surface_mesh(material)

    cx, cy = WATER_CENTER_UU

    def water_actor(label, mesh, location, scale):
        actor = spawn(unreal.StaticMeshActor, label, location)
        actor.set_folder_path("Water")
        actor.set_actor_scale3d(unreal.Vector(scale, scale, 1.0))
        component = actor.static_mesh_component
        component.set_static_mesh(mesh)
        component.set_material(0, material)
        component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        component.set_cast_shadow(False)
        return actor

    water_actor("Water", surface, unreal.Vector(cx, cy, WATER_Z_UU), 1.0)
    water_actor("WaterHorizon", unreal.load_asset("/Engine/BasicShapes/Plane"), unreal.Vector(cx, cy, WATER_Z_UU - 12.0), HORIZON_SCALE)


def main():
    directory = variants_dir()
    level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    current = world.get_path_name().split(".")[0] if world else ""
    if current == LEVEL_PATH:
        pass                                            # ya abierto en el editor: se usa tal cual
    elif asset_lib.does_asset_exist(LEVEL_PATH) or unreal.load_asset(LEVEL_PATH):
        level_subsystem.load_level(LEVEL_PATH)          # se conserva todo lo que haya
    else:
        level_subsystem.new_level(LEVEL_PATH)

    actors = existing_labels()

    # PlayerStart ANTES que el loader: ATN_MapVariantLoader mueve el PlayerStart con este tag al
    # construirse (OnConstruction), y eso solo funciona si ya existe en el nivel.
    if not any(PLAYER_START_TAG in a.tags for a in actors.values()):
        start = spawn(unreal.PlayerStart, "MapVariantStart", unreal.Vector(0.0, 0.0, 100.0))
        start.tags = [PLAYER_START_TAG]
        actors = existing_labels()

    if LOADER_LABEL not in actors:
        loader_class = unreal.load_class(None, "/Script/Tortunabo.TN_MapVariantLoader")
        loader = spawn(loader_class, LOADER_LABEL, unreal.Vector(0.0, 0.0, 0.0))
        loader.set_editor_property("variant", first_variant_name(directory))
        actors = existing_labels()

    if "Sun" in actors:
        configure_sun(actors["Sun"])
    else:
        sun = spawn(unreal.DirectionalLight, "Sun", unreal.Vector(0.0, 0.0, 5000.0), unreal.Rotator(0.0, -50.0, 35.0))
        configure_sun(sun)
        actors = existing_labels()

    if "SkyAtmosphere" not in actors:
        spawn(unreal.SkyAtmosphere, "SkyAtmosphere", unreal.Vector(0.0, 0.0, 0.0))
    if "SkyLight" not in actors:
        sky = spawn(unreal.SkyLight, "SkyLight", unreal.Vector(0.0, 0.0, 5000.0))
        sky_component = sky.get_component_by_class(unreal.SkyLightComponent)
        sky_component.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
        sky_component.set_editor_property("real_time_capture", True)
        sky_component.set_editor_property("intensity", 2.0)

    place_water(existing_labels())

    game_mode = asset_lib.load_blueprint_class(GAME_MODE_PATH) if asset_lib.does_asset_exist(GAME_MODE_PATH) else None
    if game_mode:
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        world.get_world_settings().set_editor_property("default_game_mode", game_mode)

    level_subsystem.save_current_level()
    unreal.log(f"[MapVariants] {LEVEL_PATH} listo")


main()
