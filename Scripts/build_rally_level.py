"""Crea /Game/Maps/Rally/LVL_Rally: el nivel del Rally Tortuga (ATN_RallyGameMode).

Headless, con el editor cerrado (en Git Bash, MSYS_NO_PATHCONV=1):

    UnrealEditor-Win64-DebugGame-Cmd.exe <uproject> -ExecCmds="py <ruta>/Scripts/build_rally_level.py,QUIT_EDITOR"
        -EnablePlugins=PythonScriptPlugin -nullrhi -unattended -nosplash -NoSteam

(-ExecutePythonScript cierra el editor antes de que termine el guardado.)

Contenido:
- Sol, SkyAtmosphere, SkyLight y agua de LVL_Demo01, tal cual: se abre LVL_Demo01 y se guarda como LVL_Rally
  sin guardar nunca LVL_Demo01.
- Niebla (ExponentialHeightFog) con los ajustes y la posición de la de LVL_TestMap (LVL_Demo01 no tiene).
- ATN_MapVariantLoader con I03R_tortuga_magna (la carrera cambia de variante con ?Variant=...), un ATN_RallyTrack y un
  PlayerStart en la salida de la variante.
- WorldSettings: GameMode override = ATN_RallyGameMode.

Idempotente: si LVL_Rally ya existe, lo abre y solo corrige lo que falte.
"""

import json
import os

import unreal

RALLY_LEVEL = "/Game/Maps/Rally/LVL_Rally"
LIGHT_SOURCE = "/Game/Maps/Run/LVL_Demo01"
FOG_SOURCE = "/Game/Maps/Run/LVL_TestMap"
VARIANT = "I03R_tortuga_magna"
PLAYER_START_LIFT_UU = 150.0

FOG_PROPERTIES = (
    "fog_density", "fog_height_falloff", "second_fog_data", "fog_inscattering_luminance",
    "sky_atmosphere_ambient_contribution_color_scale", "inscattering_color_cubemap", "inscattering_color_cubemap_angle",
    "inscattering_texture_tint", "fully_directional_inscattering_color_distance",
    "non_directional_inscattering_color_distance", "directional_inscattering_exponent",
    "directional_inscattering_start_distance", "directional_inscattering_luminance", "fog_max_opacity", "start_distance",
    "end_distance", "fog_cutoff_distance", "enable_volumetric_fog", "volumetric_fog_scattering_distribution",
    "volumetric_fog_albedo", "volumetric_fog_emissive", "volumetric_fog_extinction_scale", "volumetric_fog_distance",
    "volumetric_fog_start_distance", "volumetric_fog_near_fade_in_distance",
    "volumetric_fog_static_lighting_scattering_intensity", "override_light_colors_with_fog_inscattering_colors",
    "holdout", "render_in_main_pass", "visible_in_reflection_captures", "visible_in_real_time_sky_captures",
)

project = unreal.Paths.project_dir()
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assets = unreal.EditorAssetLibrary


def log(message):
    unreal.log(f"[rally] {message}")


def world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def level_actors(cls):
    return [a for a in actors.get_all_level_actors() if isinstance(a, cls)]


def read_fog():
    """Transform y ajustes de la niebla de LVL_TestMap (solo lectura: no se guarda)."""
    levels.load_level(FOG_SOURCE)
    fogs = level_actors(unreal.ExponentialHeightFog)
    if not fogs:
        raise RuntimeError(f"{FOG_SOURCE} no tiene ExponentialHeightFog")
    fog = fogs[0]
    component = fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
    values = {}
    for name in FOG_PROPERTIES:
        try:
            values[name] = component.get_editor_property(name)
        except Exception:  # noqa: BLE001 - propiedad que no existe en esta versión: se informa y se sigue
            log(f"niebla: la propiedad {name} no existe en UE 5.6, se omite")
    return fog.get_actor_transform(), values


def open_target():
    """Abre LVL_Rally si existe; si no, LVL_Demo01 (que se guardará con otro nombre, nunca con el suyo)."""
    if assets.does_asset_exist(RALLY_LEVEL):
        levels.load_level(RALLY_LEVEL)
        return False
    levels.load_level(LIGHT_SOURCE)
    return True


def ensure_fog(transform, values):
    fogs = level_actors(unreal.ExponentialHeightFog)
    fog = fogs[0] if fogs else actors.spawn_actor_from_class(unreal.ExponentialHeightFog, transform.translation)
    fog.set_actor_transform(transform, False, True)
    fog.set_actor_label("ExponentialHeightFog")
    component = fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
    for name, value in values.items():
        component.set_editor_property(name, value)


def ensure_loader():
    loaders = level_actors(unreal.TN_MapVariantLoader)
    loader = loaders[0] if loaders else actors.spawn_actor_from_class(unreal.TN_MapVariantLoader, unreal.Vector(0, 0, 0))
    for extra in loaders[1:]:
        actors.destroy_actor(extra)
    loader.set_editor_property("variant", unreal.Name(VARIANT))
    loader.set_actor_label("MapVariantLoader")
    return loader


def ensure_track():
    tracks = level_actors(unreal.TN_RallyTrack)
    track = tracks[0] if tracks else actors.spawn_actor_from_class(unreal.TN_RallyTrack, unreal.Vector(0, 0, 0))
    for extra in tracks[1:]:
        actors.destroy_actor(extra)
    track.set_actor_label("RallyTrack")
    return track


def ensure_player_start(manifest):
    """Un PlayerStart en la salida (el GameMode sienta a cada jugador en su buggy; esto es solo el punto de vista)."""
    starts = level_actors(unreal.PlayerStart)
    for extra in starts[1:]:
        actors.destroy_actor(extra)
    x, y, z = manifest["start_uu"]
    location = unreal.Vector(x, y, z + PLAYER_START_LIFT_UU)
    rotation = unreal.Rotator(0.0, 0.0, float(manifest.get("start_yaw", 0.0)))
    start = starts[0] if starts else actors.spawn_actor_from_class(unreal.PlayerStart, location, rotation)
    start.root_component.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
    start.set_actor_location_and_rotation(location, rotation, False, True)
    start.set_actor_label("PlayerStart")
    start.tags = [unreal.Name("MapVariantStart")]


def set_game_mode():
    settings = world().get_world_settings()
    settings.set_editor_property("default_game_mode", unreal.TN_RallyGameMode.static_class())


def describe():
    for actor in actors.get_all_level_actors():
        log(f"actor {actor.get_class().get_name()} '{actor.get_actor_label()}' en {actor.get_actor_location()}")
    log(f"GameMode override: {world().get_world_settings().get_editor_property('default_game_mode')}")


def main():
    path = os.path.join(project, "Scripts", "terrain_volumes", "Variants", VARIANT, "manifest.json")
    with open(path, encoding="utf-8") as f:
        manifest = json.load(f)
    fog_transform, fog_values = read_fog()
    is_new = open_target()
    ensure_fog(fog_transform, fog_values)
    ensure_loader()
    ensure_track()
    ensure_player_start(manifest)
    set_game_mode()
    describe()
    if is_new:
        ok = unreal.EditorLoadingAndSavingUtils.save_map(world(), RALLY_LEVEL)
    else:
        ok = levels.save_current_level()
    log(f"{RALLY_LEVEL} guardado={ok}")


main()
