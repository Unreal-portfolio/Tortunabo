"""Agua de Tortunabo: material de agua de capa unica (Single Layer Water de UE5), superficie
subdividida con oleaje y un plano hasta el horizonte. Monta todo en LVL_Mapa01.

Se ejecuta DENTRO del editor de Unreal (consola Python, MCP o headless):
    UnrealEditor-Win64-DebugGame-Cmd.exe <uproject> -run=pythonscript
        -script=<repo>/Scripts/build_water.py -EnablePlugins=PythonScriptPlugin
        -unattended -nosplash -nullrhi

Antes: uv run --with numpy --with pillow python Scripts/gen_water_textures.py

Look (tropical, arena clara): turquesa donde cubre poco y azul hondo donde cubre mucho (lo dan
la absorcion y la dispersion del agua, no un color pintado), reflejos con tres capas de
ondulacion que se mueven, oleaje suave de verdad (desplaza los vertices) y espuma en la orilla
que late con las olas (distancia al terreno: campos de distancia de malla).
"""

import os

import unreal

WATER_ROOT = "/Game/Environment/Water"
MATERIAL_NAME = "M_TortunaboWater"
SURFACE_MESH = "SM_WaterSurface"
LEVEL_PATH = "/Game/Maps/Run/LVL_Mapa01"
MAP_CENTER_UU = (25000.0, 25000.0)
WATER_Z_UU = -400.0
HORIZON_SCALE = 1200.0                  # plano del motor (1 m) -> 120 km hasta el horizonte

asset_lib = unreal.EditorAssetLibrary
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary


def load_or_none(path):
    if asset_lib.does_asset_exist(path):
        return asset_lib.load_asset(path)
    return unreal.load_asset(f"{path}.{path.rsplit('/', 1)[-1]}")


def import_texture(name, compression):
    source = os.path.join(unreal.Paths.project_dir(), "Scripts", "textures", f"{name}.png")
    if not os.path.isfile(source):
        raise RuntimeError(f"Falta {source}: genera las texturas con Scripts/gen_water_textures.py")
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", source)
    task.set_editor_property("destination_path", WATER_ROOT)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    asset_tools.import_asset_tasks([task])
    texture = load_or_none(f"{WATER_ROOT}/{name}")
    # Se leen en crudo desde nodos Custom: sin sRGB ni compresion de normales.
    texture.set_editor_property("srgb", False)
    texture.set_editor_property("compression_settings", compression)
    asset_lib.save_loaded_asset(texture)
    return texture


def custom_input(name):
    entry = unreal.CustomInput()
    entry.set_editor_property("input_name", name)
    return entry


def custom_node(material, code, inputs, output_type, x, y, description):
    node = mel.create_material_expression(material, unreal.MaterialExpressionCustom, x, y)
    node.set_editor_property("code", code)
    node.set_editor_property("output_type", output_type)
    node.set_editor_property("description", description)
    node.set_editor_property("inputs", [custom_input(n) for n in inputs])
    return node


def vector_parameter(material, name, value, x, y):
    node = mel.create_material_expression(material, unreal.MaterialExpressionVectorParameter, x, y)
    node.set_editor_property("parameter_name", name)
    node.set_editor_property("default_value", unreal.LinearColor(*value, 1.0))
    return node


def scalar_parameter(material, name, value, x, y):
    node = mel.create_material_expression(material, unreal.MaterialExpressionScalarParameter, x, y)
    node.set_editor_property("parameter_name", name)
    node.set_editor_property("default_value", value)
    return node


def texture_object(material, name, texture, x, y):
    node = mel.create_material_expression(material, unreal.MaterialExpressionTextureObjectParameter, x, y)
    node.set_editor_property("parameter_name", name)
    node.set_editor_property("texture", texture)
    return node


# Normal de detalle: tres capas del mismo mapa a escalas, direcciones y velocidades distintas.
NORMAL_HLSL = """\
float2 p = P.xy;
float3 n1 = Texture2DSample(Tex, TexSampler, p / 2400.0 + T * float2( 0.010,  0.004)).xyz * 2.0 - 1.0;
float3 n2 = Texture2DSample(Tex, TexSampler, p / 1100.0 + T * float2(-0.007,  0.011)).xyz * 2.0 - 1.0;
float3 n3 = Texture2DSample(Tex, TexSampler, p / 7000.0 + T * float2( 0.003, -0.002)).xyz * 2.0 - 1.0;
float2 xy = (n1.xy + 0.4 * n2.xy + 0.7 * n3.xy) * Strength;
return normalize(float3(xy, 1.0));
"""

# Espuma de orilla: franja junto al terreno (distancia de campos de distancia) con un patron
# que se mueve y lineas que avanzan hacia la orilla, como el borde de una ola al romper.
FOAM_HLSL = """\
float shore = 1.0 - saturate(D / 130.0);
float2 uv = P.xy / 520.0;
float f1 = Texture2DSample(Foam, FoamSampler, uv + T * float2(0.011, 0.006)).r;
float f2 = Texture2DSample(Foam, FoamSampler, uv * 1.7 - T * float2(0.006, 0.012)).r;
float pattern = saturate(f1 * f2 * 2.6);
// Lineas de espuma que llegan a la orilla y se deshacen (latido de las olas).
float lines = smoothstep(0.55, 0.95, 0.5 + 0.5 * sin(D * 0.07 - T * 1.6));
float edge = saturate(1.0 - D / 22.0);
return saturate(shore * shore * shore * (0.55 * pattern + 0.6 * lines * pattern) + edge * 0.55 * (0.4 + 0.6 * pattern));
"""

# Oleaje: tres ondas largas que desplazan la superficie (solo en vertical, suave).
WAVES_HLSL = """\
float h = 0.0;
h += 7.0 * sin(dot(P.xy, float2( 0.0019,  0.0012)) + T * 0.85);
h += 4.5 * sin(dot(P.xy, float2(-0.0010,  0.0026)) + T * 1.20);
h += 2.0 * sin(dot(P.xy, float2( 0.0041, -0.0029)) + T * 1.90);
return float3(0.0, 0.0, h * Amplitude);
"""


def build_material(normal_texture, foam_texture):
    path = f"{WATER_ROOT}/{MATERIAL_NAME}"
    # Se rehace desde cero: delete_all_material_expressions no borra el nodo de salida del agua
    # y dos salidas de agua no compilan.
    if asset_lib.does_asset_exist(path) or unreal.load_asset(f"{path}.{MATERIAL_NAME}"):
        asset_lib.delete_asset(path)
    material = asset_tools.create_asset(MATERIAL_NAME, WATER_ROOT, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_SINGLE_LAYER_WATER)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)

    world = mel.create_material_expression(material, unreal.MaterialExpressionWorldPosition, -1400, 0)
    time = mel.create_material_expression(material, unreal.MaterialExpressionTime, -1400, 150)
    distance = mel.create_material_expression(material, unreal.MaterialExpressionDistanceToNearestSurface, -1400, 300)
    normal_tex = texture_object(material, "WaterNormal", normal_texture, -1400, 450)
    foam_tex = texture_object(material, "WaterFoam", foam_texture, -1400, 650)

    strength = scalar_parameter(material, "RippleStrength", 0.22, -1100, 400)
    normal = custom_node(material, NORMAL_HLSL, ("P", "T", "Tex", "Strength"),
                         unreal.CustomMaterialOutputType.CMOT_FLOAT3, -800, 350, "Ondulacion")
    for source, pin in ((world, "P"), (time, "T"), (normal_tex, "Tex"), (strength, "Strength")):
        mel.connect_material_expressions(source, "", normal, pin)
    mel.connect_material_property(normal, "", unreal.MaterialProperty.MP_NORMAL)

    foam = custom_node(material, FOAM_HLSL, ("D", "P", "T", "Foam"),
                       unreal.CustomMaterialOutputType.CMOT_FLOAT1, -800, 650, "Espuma de orilla")
    for source, pin in ((distance, "D"), (world, "P"), (time, "T"), (foam_tex, "Foam")):
        mel.connect_material_expressions(source, "", foam, pin)

    foam_color = vector_parameter(material, "FoamColor", (0.92, 0.95, 0.97), -500, 750)
    base = mel.create_material_expression(material, unreal.MaterialExpressionMultiply, -300, 700)
    mel.connect_material_expressions(foam, "", base, "A")
    mel.connect_material_expressions(foam_color, "", base, "B")
    mel.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(foam, "", unreal.MaterialProperty.MP_OPACITY)

    rough = mel.create_material_expression(material, unreal.MaterialExpressionLinearInterpolate, -300, 900)
    calm = mel.create_material_expression(material, unreal.MaterialExpressionConstant, -500, 900)
    calm.set_editor_property("r", 0.03)
    foamy = mel.create_material_expression(material, unreal.MaterialExpressionConstant, -500, 1000)
    foamy.set_editor_property("r", 0.55)
    mel.connect_material_expressions(calm, "", rough, "A")
    mel.connect_material_expressions(foamy, "", rough, "B")
    mel.connect_material_expressions(foam, "", rough, "Alpha")
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)

    amplitude = scalar_parameter(material, "WaveAmplitude", 1.0, -1100, -200)
    waves = custom_node(material, WAVES_HLSL, ("P", "T", "Amplitude"),
                        unreal.CustomMaterialOutputType.CMOT_FLOAT3, -800, -150, "Oleaje")
    for source, pin in ((world, "P"), (time, "T"), (amplitude, "Amplitude")):
        mel.connect_material_expressions(source, "", waves, pin)
    mel.connect_material_property(waves, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)

    # Color del agua por fisica: absorbe rojo y algo de verde (turquesa en lo somero, azul hondo
    # en lo profundo) y dispersa un poco de verde-azul (el brillo lechoso del agua tropical).
    water_out = mel.create_material_expression(material, unreal.MaterialExpressionSingleLayerWaterMaterialOutput, -200, -400)
    # Afinado en el editor (2026-09-25): transparente en lo somero (se ve la arena), turquesa a
    # media profundidad y azul en lo hondo.
    scattering = vector_parameter(material, "Scattering", (0.002, 0.012, 0.013), -500, -500)
    absorption = vector_parameter(material, "Absorption", (0.09, 0.016, 0.013), -500, -350)
    phase = scalar_parameter(material, "PhaseG", 0.35, -500, -200)
    mel.connect_material_expressions(scattering, "", water_out, "ScatteringCoefficients")
    mel.connect_material_expressions(absorption, "", water_out, "AbsorptionCoefficients")
    mel.connect_material_expressions(phase, "", water_out, "PhaseG")

    mel.recompile_material(material)
    asset_lib.save_loaded_asset(material)
    return material


def build_surface_mesh(material):
    """Rejilla subdividida (Scripts/textures/WaterSurface.bin, TNTM1) convertida en StaticMesh
    con la misma via que los trozos del terreno (UTN_TerrainMeshAsset::BuildStaticMesh)."""
    data_path = f"{WATER_ROOT}/DA_WaterSurface"
    data = load_or_none(data_path)
    if not data:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.TN_TerrainMeshAsset)
        data = asset_tools.create_asset("DA_WaterSurface", WATER_ROOT, unreal.TN_TerrainMeshAsset, factory)
    source = os.path.join(unreal.Paths.project_dir(), "Scripts", "textures", "WaterSurface.bin").replace("\\", "/")
    if not data.load_from_file(source):
        raise RuntimeError(f"No se pudo cargar {source}: genera con Scripts/gen_water_textures.py")
    asset_lib.save_loaded_asset(data)
    static_mesh = data.build_static_mesh(WATER_ROOT, SURFACE_MESH, material)
    if not static_mesh:
        raise RuntimeError("BuildStaticMesh del agua fallo")
    asset_lib.save_loaded_asset(static_mesh)
    return static_mesh


def place_in_level(surface, material):
    level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    current = world.get_path_name().split(".")[0] if world else ""
    if current != LEVEL_PATH:
        level_subsystem.load_level(LEVEL_PATH)
    actors = {a.get_actor_label(): a for a in actor_subsystem.get_all_level_actors()}

    def water_actor(label, mesh, location, scale):
        actor = actors.get(label)
        if actor is None:
            actor = actor_subsystem.spawn_actor_from_class(unreal.StaticMeshActor, location, unreal.Rotator(0, 0, 0))
            actor.set_actor_label(label)
            actor.set_folder_path("Water")
        actor.set_actor_location(location, False, False)
        actor.set_actor_scale3d(unreal.Vector(scale, scale, 1.0))
        component = actor.static_mesh_component
        component.set_static_mesh(mesh)
        component.set_material(0, material)
        component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        component.set_cast_shadow(False)
        return actor

    cx, cy = MAP_CENTER_UU
    water_actor("Water", surface, unreal.Vector(cx, cy, WATER_Z_UU), 1.0)
    # Horizonte: plano grande algo por debajo; lo tapa la superficie cerca del mapa.
    water_actor("WaterHorizon", unreal.load_asset("/Engine/BasicShapes/Plane"),
                unreal.Vector(cx, cy, WATER_Z_UU - 12.0), HORIZON_SCALE)
    level_subsystem.save_current_level()


def main():
    normal_texture = import_texture("T_WaterNormal", unreal.TextureCompressionSettings.TC_VECTOR_DISPLACEMENTMAP)
    foam_texture = import_texture("T_WaterFoam", unreal.TextureCompressionSettings.TC_GRAYSCALE)
    material = build_material(normal_texture, foam_texture)
    surface = build_surface_mesh(material)
    place_in_level(surface, material)
    unreal.log(f"[Water] {MATERIAL_NAME} y {SURFACE_MESH} listos en {LEVEL_PATH}")


if __name__ == "__main__":
    main()
