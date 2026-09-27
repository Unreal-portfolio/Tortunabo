"""Agua cartoon de Tortunabo: translucida con luz de superficie, tres bandas de color por
profundidad, espuma de borde neto que sube y baja por la orilla (mismo periodo que la arena
mojada de M_GridTerrain), lineas de brillo que se desplazan y ondas suaves de vertice.

Se ejecuta DENTRO del editor de Unreal (headless o desde la consola de Python / MCP):
    UnrealEditor-Win64-DebugGame-Cmd.exe <uproject> -run=pythonscript
        -script=<repo>/Scripts/build_water_toon.py -EnablePlugins=PythonScriptPlugin
        -unattended -nosplash -nullrhi

Crea tambien M_GridTerrainWet (el material del terreno con la arena mojada) y pone los dos en
LVL_MapVariants: el agua en los actores Water y WaterHorizon, y el terreno en el cargador.
"""

import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import build_grid_demo_assets as grid  # noqa: E402
import build_water as water  # noqa: E402

MATERIAL_NAME = "M_TortunaboWaterToon"
LEVEL_PATH = "/Game/Maps/Run/LVL_MapVariants"
HORIZON_SCALE = 20000.0          # plano del motor de 1 m: 20 km

# Color por profundidad del agua (uu entre la superficie y el fondo) en tres bandas duras,
# espuma de borde neto donde el agua cubre poco (junto a la orilla), que avanza y se retira con
# la ola, y brillos en linea. La orilla sale de la profundidad y no de los campos de distancia:
# los trozos en ProcMesh (ATN_MapVariantLoader) no tienen campo de distancia.
TOON_HLSL = """\
float depth = max(SceneD - PixD, 0.0);
float3 c0 = float3(0.46, 0.93, 0.86);
float3 c1 = float3(0.10, 0.70, 0.80);
float3 c2 = float3(0.08, 0.45, 0.80);
float3 col = depth < 60.0 ? c0 : (depth < 220.0 ? c1 : c2);
float a = depth < 60.0 ? 0.55 : (depth < 220.0 ? 0.82 : 0.95);
float wave = 0.5 + 0.5 * sin(T * 6.2831853 / max(Period, 0.1));
float n = Texture2DSample(Foam, FoamSampler, P.xy / 600.0 + T * float2(0.010, 0.004)).r;
float foam = step(depth, (8.0 + 22.0 * wave) * (0.7 + 0.6 * n));
float glint = step(0.93, sin(dot(P.xy, float2(0.004, 0.0027)) + T * 0.9) * (0.6 + 0.4 * n));
col = lerp(col, float3(1.0, 1.0, 1.0), max(foam, 0.6 * glint));
return float4(col, max(a, foam));
"""


def build_material(foam_texture):
    path = f"{water.WATER_ROOT}/{MATERIAL_NAME}"
    if water.asset_lib.does_asset_exist(path):
        water.asset_lib.delete_asset(path)
    material = water.asset_tools.create_asset(MATERIAL_NAME, water.WATER_ROOT, unreal.Material,
                                              unreal.MaterialFactoryNew())
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    # Con luz (sin luz, la exposicion automatica de un dia soleado dejaba el agua casi negra).
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    material.set_editor_property("translucency_lighting_mode",
                                 unreal.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
    mel = water.mel
    world = mel.create_material_expression(material, unreal.MaterialExpressionWorldPosition, -1400, 0)
    time = mel.create_material_expression(material, unreal.MaterialExpressionTime, -1400, 150)
    scene = mel.create_material_expression(material, unreal.MaterialExpressionSceneDepth, -1400, 450)
    pixel = mel.create_material_expression(material, unreal.MaterialExpressionPixelDepth, -1400, 600)
    foam = water.texture_object(material, "WaterFoam", foam_texture, -1400, 750)
    period = water.scalar_parameter(material, "WetPeriod", 4.0, -1100, 850)
    toon = water.custom_node(material, TOON_HLSL, ("SceneD", "PixD", "P", "T", "Foam", "Period"),
                             unreal.CustomMaterialOutputType.CMOT_FLOAT4, -800, 300, "AguaCartoon")
    for source, pin in ((scene, "SceneD"), (pixel, "PixD"), (world, "P"), (time, "T"),
                        (foam, "Foam"), (period, "Period")):
        mel.connect_material_expressions(source, "", toon, pin)
    rgb = mel.create_material_expression(material, unreal.MaterialExpressionComponentMask, -500, 250)
    for channel in ("r", "g", "b"):
        rgb.set_editor_property(channel, True)
    alpha = mel.create_material_expression(material, unreal.MaterialExpressionComponentMask, -500, 400)
    alpha.set_editor_property("a", True)
    mel.connect_material_expressions(toon, "", rgb, "")
    mel.connect_material_expressions(toon, "", alpha, "")
    mel.connect_material_property(rgb, "", unreal.MaterialProperty.MP_BASE_COLOR)
    glow = mel.create_material_expression(material, unreal.MaterialExpressionMultiply, -300, 200)
    mel.connect_material_expressions(rgb, "", glow, "A")
    glow.set_editor_property("const_b", 0.35)
    mel.connect_material_property(glow, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    rough = mel.create_material_expression(material, unreal.MaterialExpressionConstant, -300, 500)
    rough.set_editor_property("r", 0.35)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(alpha, "", unreal.MaterialProperty.MP_OPACITY)
    amplitude = water.scalar_parameter(material, "WaveAmplitude", 0.35, -1100, -200)
    waves = water.custom_node(material, water.WAVES_HLSL, ("P", "T", "Amplitude"),
                              unreal.CustomMaterialOutputType.CMOT_FLOAT3, -800, -150, "Oleaje")
    for source, pin in ((world, "P"), (time, "T"), (amplitude, "Amplitude")):
        mel.connect_material_expressions(source, "", waves, pin)
    mel.connect_material_property(waves, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    mel.recompile_material(material)
    water.asset_lib.save_loaded_asset(material)
    return material


def main():
    terrain = grid.build_terrain_material(grid.build_grain_texture(), name="M_GridTerrainWet", recreate=True,
                                          wall_tile_scale=1.0, wall_contrast_scale=1.0,
                                          detail_normals=grid.build_detail_normal_textures(reimport=True))
    foam_texture = water.import_texture("T_WaterFoam", unreal.TextureCompressionSettings.TC_GRAYSCALE)
    material = build_material(foam_texture)
    # El agua es translucida y Nanite solo admite materiales opacos o enmascarados: sin esto la
    # superficie no se dibuja (los trozos del terreno si llevan Nanite).
    surface = unreal.load_asset(f"{water.WATER_ROOT}/{water.SURFACE_MESH}")
    if surface:
        nanite = surface.get_editor_property("nanite_settings")
        nanite.set_editor_property("enabled", False)
        surface.set_editor_property("nanite_settings", nanite)
        water.asset_lib.save_loaded_asset(surface)
    level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if not world or world.get_path_name().split(".")[0] != LEVEL_PATH:
        level.load_level(LEVEL_PATH)
    count = 0
    for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
        if actor.get_actor_label() in ("Water", "WaterHorizon"):
            actor.static_mesh_component.set_material(0, material)
            count += 1
        if actor.get_actor_label() == "WaterHorizon":
            # Hasta el horizonte de verdad (con 1,2 km se veia su borde como una franja negra).
            actor.set_actor_scale3d(unreal.Vector(HORIZON_SCALE, HORIZON_SCALE, 1.0))
        if actor.get_class().get_name() == "TN_MapVariantLoader":
            actor.set_editor_property("terrain_material", terrain)
            actor.set_editor_property("variant", "C01_camino")
            actor.recargar()
    level.save_current_level()
    unreal.log_warning(f"[WaterToon] {MATERIAL_NAME} en {count} actores de {LEVEL_PATH}")


if __name__ == "__main__":
    main()
