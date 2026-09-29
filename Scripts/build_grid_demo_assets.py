"""Construye los materiales del terreno (antes, de la demo del mapa en grid).

Se ejecuta DENTRO del editor de Unreal (consola Python, MCP o -run=pythonscript):
    exec(open(r"<repo>/Scripts/build_grid_demo_assets.py", encoding="utf-8").read())

Crea en /Game/Blueprints/Gameplay/GridMap: material plano + instancias de color, los
materiales de terreno, de basura y del agua, y el GameMode de la demo. Los usan el mapa
volumetrico (SM_M_Mapa01_*, LVL_Mapa01) y build_water_toon.py.

El generador en rejilla (tiles, BP_GridMapGenerator y LVL_ProcGenDemo) se retiro el
2026-09-29: la version completa del script esta en Deprecado/Scripts/build_grid_demo_assets.py.

Idempotente: los assets que ya existen se reutilizan, no se recrean.
"""

import os

import unreal

ROOT = "/Game/Blueprints/Gameplay/GridMap"
TEXTURE_ROOT = "/Game/Textures/Terrain"
GRAIN_TEXTURE = "T_TerrainGrain"
GRAIN_SOURCE = "Scripts/textures/T_TerrainGrain.png"  # lo genera Scripts/gen_terrain_textures.py
# Normales de detalle (Scripts/gen_terrain_textures.py): suelo RG arena / BA camino, pared RG.
DETAIL_NORMALS = {"T_TerrainFloorN": "Scripts/textures/T_TerrainFloorN.png",
                  "T_TerrainWallN": "Scripts/textures/T_TerrainWallN.png"}
CHARACTER_BP = "/Game/Blueprints/Characters/BP_TortugaCharacter"


COLORS = {
    "MI_Grid_Path": (0.85, 0.70, 0.42),
    "MI_Grid_Wall": (0.30, 0.17, 0.08),
    "MI_Grid_Dune": (0.62, 0.47, 0.24),
    "MI_Grid_Rock": (0.22, 0.22, 0.24),
    "MI_Grid_Water": (0.05, 0.30, 0.55),
}

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
asset_lib = unreal.EditorAssetLibrary
subobjects = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)


def load_or_none(path):
    return asset_lib.load_asset(path) if asset_lib.does_asset_exist(path) else None


def build_flat_material():
    path = f"{ROOT}/M_GridFlat"
    existing = load_or_none(path)
    if existing:
        return existing

    material = asset_tools.create_asset("M_GridFlat", ROOT, unreal.Material, unreal.MaterialFactoryNew())
    mel = unreal.MaterialEditingLibrary
    color = mel.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -400, 0)
    color.set_editor_property("parameter_name", "Color")
    color.set_editor_property("default_value", unreal.LinearColor(0.5, 0.5, 0.5, 1.0))
    mel.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)

    roughness = mel.create_material_expression(material, unreal.MaterialExpressionConstant, -400, 300)
    roughness.set_editor_property("r", 0.9)
    mel.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)

    mel.recompile_material(material)
    asset_lib.save_loaded_asset(material)
    return material


def build_color_instance(name, rgb, parent):
    path = f"{ROOT}/{name}"
    existing = load_or_none(path)
    if existing:
        return existing

    instance = asset_tools.create_asset(
        name, ROOT, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel = unreal.MaterialEditingLibrary
    mel.set_material_instance_parent(instance, parent)
    mel.set_material_instance_vector_parameter_value(
        instance, "Color", unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
    mel.update_material_instance(instance)
    asset_lib.save_loaded_asset(instance)
    return instance


def create_blueprint(name, parent_class):
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", parent_class)
    return asset_tools.create_asset(name, ROOT, unreal.Blueprint, factory)


def build_grain_texture():
    """Importa la textura de grano del terreno desde el PNG versionado en Scripts/textures."""
    path = f"{TEXTURE_ROOT}/{GRAIN_TEXTURE}"
    existing = load_or_none(path)
    if existing:
        return existing

    source = os.path.join(unreal.Paths.project_dir(), GRAIN_SOURCE)
    if not os.path.isfile(source):
        raise RuntimeError(f"Falta {GRAIN_SOURCE}; genéralo con Scripts/gen_terrain_textures.py.")

    task = unreal.AssetImportTask()
    task.set_editor_property("filename", source)
    task.set_editor_property("destination_path", TEXTURE_ROOT)
    task.set_editor_property("destination_name", GRAIN_TEXTURE)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    asset_tools.import_asset_tasks([task])

    texture = load_or_none(path)
    if not texture:
        raise RuntimeError(f"La importación de {GRAIN_TEXTURE} no ha producido asset en {TEXTURE_ROOT}.")
    # El grano es un multiplicador, no un color: se muestrea en espacio lineal.
    texture.set_editor_property("srgb", False)
    texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_GRAYSCALE)
    asset_lib.save_loaded_asset(texture)
    return texture


def build_detail_normal_textures(reimport=False):
    """Importa las normales de detalle (XY empaquetadas, sin sRGB). Devuelve (suelo, pared)."""
    out = []
    for name, relative in DETAIL_NORMALS.items():
        path = f"{TEXTURE_ROOT}/{name}"
        existing = None if reimport else load_or_none(path)
        if existing:
            out.append(existing)
            continue
        source = os.path.join(unreal.Paths.project_dir(), relative)
        if not os.path.isfile(source):
            raise RuntimeError(f"Falta {relative}; genéralo con Scripts/gen_terrain_textures.py.")
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", source)
        task.set_editor_property("destination_path", TEXTURE_ROOT)
        task.set_editor_property("destination_name", name)
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_existing", True)
        asset_tools.import_asset_tasks([task])
        texture = load_or_none(path)
        if not texture:
            raise RuntimeError(f"La importación de {name} no ha producido asset en {TEXTURE_ROOT}.")
        # Son vectores, no color: lineales y con compresion de alta calidad (4 canales utiles).
        texture.set_editor_property("srgb", False)
        texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_BC7)
        asset_lib.save_loaded_asset(texture)
        out.append(texture)
    return tuple(out)


# Normal de detalle en espacio de mundo (el material no usa tangentes: los trozos en ProcMesh no
# las traen). Suelo: arena con rizos o arena pisada segun SandMask (alfa del color de vertice,
# 0 = camino); paredes: estratos y roca en las dos proyecciones laterales. Los tamanos de tile
# dividen 10 000 uu (un trozo) para que no haya costura entre trozos.
DETAIL_NORMAL_HLSL = """\
float3 N = normalize(Normal);
float3 W = pow(max(abs(N), 1e-4f), 4.0f);
W /= max(W.x + W.y + W.z, 1e-4f);
float4 F = Texture2DSample(FloorTex, FloorTexSampler, P.xy / max(FloorTile, 1.0f));
float2 s = F.rg * 2.0f - 1.0f;
// Rizos algo mas marcados (cartoon suave): la pendiente se curva hacia su signo, el borde de cada
// rizo se lee mas limpio sin cambiar su dibujo.
s = sign(s) * pow(abs(s), 0.7f);
float2 sand = s * SandStrength;
// El camino lleva los mismos rizos (mas suaves) con un poco de arena pisada encima: su color
// oscuro va en el color de vertice.
float2 path = s * PathStrength + (F.ba * 2.0f - 1.0f) * 0.25f;
float2 fl = lerp(path, sand, saturate(SandMask));
float2 wx = (Texture2DSample(WallTex, WallTexSampler, P.yz / max(WallTile, 1.0f)).rg * 2.0f - 1.0f) * WallStrength;
float2 wy = (Texture2DSample(WallTex, WallTexSampler, P.xz / max(WallTile, 1.0f)).rg * 2.0f - 1.0f) * WallStrength;
float3 d = W.z * float3(fl.x, fl.y, 0.0f) + W.x * float3(0.0f, wx.x, wx.y) + W.y * float3(wy.x, 0.0f, wy.y);
return normalize(N + d);
"""

DETAIL_NORMAL_INPUTS = ("P", "Normal", "FloorTex", "WallTex", "SandMask", "FloorTile", "WallTile",
                        "SandStrength", "PathStrength", "WallStrength")

# Variacion de color a gran escala (manchas de 50 m): rompe la uniformidad del color de vertice.
MACRO_HLSL = """\
return 1.0f + (Texture2DSample(Tex, TexSampler, P.xy / 5000.0f).r - 0.5f) * Contrast;
"""


def add_detail_normal(material, local_position, normal, vertex_color, detail_normals):
    """Conecta la normal de detalle al material (normal en espacio de mundo)."""
    mel = unreal.MaterialEditingLibrary
    floor_tex, wall_tex = detail_normals
    floor_obj = mel.create_material_expression(material, unreal.MaterialExpressionTextureObjectParameter, -1000, 900)
    floor_obj.set_editor_property("parameter_name", "FloorDetailNormal")
    floor_obj.set_editor_property("texture", floor_tex)
    floor_obj.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    wall_obj = mel.create_material_expression(material, unreal.MaterialExpressionTextureObjectParameter, -1000, 1020)
    wall_obj.set_editor_property("parameter_name", "WallDetailNormal")
    wall_obj.set_editor_property("texture", wall_tex)
    wall_obj.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    pins = {
        "P": local_position, "Normal": normal, "FloorTex": floor_obj, "WallTex": wall_obj,
        "FloorTile": scalar_parameter(material, "DetailFloorTile", 250.0, -1000, 1140),
        "WallTile": scalar_parameter(material, "DetailWallTile", 500.0, -1000, 1240),
        "SandStrength": scalar_parameter(material, "DetailSandStrength", 0.8, -1000, 1340),
        "PathStrength": scalar_parameter(material, "DetailPathStrength", 0.85, -1000, 1440),
        "WallStrength": scalar_parameter(material, "DetailWallStrength", 0.9, -1000, 1540),
    }
    custom = mel.create_material_expression(material, unreal.MaterialExpressionCustom, -600, 900)
    custom.set_editor_property("code", DETAIL_NORMAL_HLSL)
    custom.set_editor_property("description", "DetailNormalWS")
    custom.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    custom.set_editor_property("inputs", [custom_input(name) for name in DETAIL_NORMAL_INPUTS])
    for pin, expression in pins.items():
        mel.connect_material_expressions(expression, "", custom, pin)
    mel.connect_material_expressions(vertex_color, "A", custom, "SandMask")
    material.set_editor_property("tangent_space_normal", False)
    mel.connect_material_property(custom, "", unreal.MaterialProperty.MP_NORMAL)


# Proyección triplanar: tres muestreos en los planos locales XY, XZ e YZ, mezclados por la
# normal elevada a Sharpness y normalizada. Mantiene el tamaño de texel constante en
# cualquier pendiente, cosa que una UV planar no hace (el talud de una pared de 800 uu
# estiraba el texel un factor 2,3). Devuelve un multiplicador alrededor de 1 que modula el
# color de vértice sin desplazar su media.
TRIPLANAR_HLSL = """\
float3 N = abs(Normal);
N = pow(max(N, 1e-4f), max(Sharpness, 1.0f));
N /= max(N.x + N.y + N.z, 1e-4f);
// Pared (normal casi horizontal): grano mas fino y marcado que el del suelo.
float Steep = 1.0f - saturate((abs(Normal.z) - 0.75f) / 0.15f);
float Inv = 1.0f / max(TileSize * lerp(1.0f, WallTileScale, Steep), 1.0f);
float Gx = Texture2DSample(Tex, TexSampler, P.yz * Inv).r;
float Gy = Texture2DSample(Tex, TexSampler, P.xz * Inv).r;
float Gz = Texture2DSample(Tex, TexSampler, P.xy * Inv).r;
float G = Gx * N.x + Gy * N.y + Gz * N.z;
return 1.0f + (G - 0.5f) * Contrast * lerp(1.0f, WallContrastScale, Steep);
"""


TRIPLANAR_INPUTS = ("P", "Normal", "Tex", "TileSize", "Sharpness", "Contrast", "WallTileScale", "WallContrastScale")

# Arena mojada (uu): la ola llega de 10 a 40 uu sobre el agua y vuelve; la arena que moja se
# oscurece y se seca en 25 uu de altura. Multiplicador del color.
WET_HLSL = """\
float wave = 0.5 + 0.5 * sin(T * 6.2831853 / max(Period, 0.1));
float reach = WaterZ + 10.0 + 30.0 * wave;
float dry = saturate((Z - reach) / 25.0);
return lerp(0.55, 1.0, dry);
"""


def custom_input(name):
    """FCustomInput no acepta argumentos en su constructor de Python: se rellena a posteriori."""
    entry = unreal.CustomInput()
    entry.set_editor_property("input_name", name)
    return entry


def scalar_parameter(material, name, value, x, y):
    mel = unreal.MaterialEditingLibrary
    parameter = mel.create_material_expression(material, unreal.MaterialExpressionScalarParameter, x, y)
    parameter.set_editor_property("parameter_name", name)
    parameter.set_editor_property("default_value", value)
    return parameter


def build_terrain_material(grain_texture, name="M_GridTerrain", recreate=False, wall_tile_scale=1.0,
                           wall_contrast_scale=1.0, detail_normals=None):
    """Color de vértice (estratos, arena, moteado) modulado por grano triplanar y oscurecido
    donde la ola moja la arena. recreate=True borra y crea el asset (en el commandlet,
    delete_all_material_expressions sobre un material cargado revienta con !IsRooted())."""
    path = f"{ROOT}/{name}"
    if recreate and asset_lib.does_asset_exist(path):
        asset_lib.delete_asset(path)
    material = load_or_none(path)
    if not material:
        material = asset_tools.create_asset(name, ROOT, unreal.Material, unreal.MaterialFactoryNew())

    mel = unreal.MaterialEditingLibrary
    # Se reconstruye el grafo entero en vez de recrear el asset: así el material conserva su
    # path y las referencias del BP del tile siguen siendo válidas.
    mel.delete_all_material_expressions(material)

    # Posición local del tile: World - ObjectPosition. Restar dos posiciones LWC da un
    # float3 de precisión normal, que es lo que el nodo Custom puede consumir.
    world_position = mel.create_material_expression(material, unreal.MaterialExpressionWorldPosition, -1200, -200)
    object_position = mel.create_material_expression(material, unreal.MaterialExpressionObjectPositionWS, -1200, -20)
    local_position = mel.create_material_expression(material, unreal.MaterialExpressionSubtract, -1000, -120)
    mel.connect_material_expressions(world_position, "", local_position, "A")
    mel.connect_material_expressions(object_position, "", local_position, "B")

    normal = mel.create_material_expression(material, unreal.MaterialExpressionVertexNormalWS, -1000, 60)
    texture_object = mel.create_material_expression(
        material, unreal.MaterialExpressionTextureObjectParameter, -1000, 180)
    texture_object.set_editor_property("parameter_name", "GrainTexture")
    texture_object.set_editor_property("texture", grain_texture)

    tile_size = scalar_parameter(material, "GrainTileSize", 400.0, -1000, 340)
    sharpness = scalar_parameter(material, "GrainSharpness", 4.0, -1000, 440)
    contrast = scalar_parameter(material, "GrainContrast", 0.35, -1000, 540)

    triplanar = mel.create_material_expression(material, unreal.MaterialExpressionCustom, -600, 60)
    triplanar.set_editor_property("code", TRIPLANAR_HLSL)
    triplanar.set_editor_property("description", "TriplanarGrain")
    triplanar.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    triplanar.set_editor_property("inputs", [custom_input(name) for name in TRIPLANAR_INPUTS])
    # Pared distinta del suelo (1.0 = igual): escala del grano y del contraste en lo empinado.
    wall_tile = scalar_parameter(material, "WallTileScale", wall_tile_scale, -1000, 640)
    wall_contrast = scalar_parameter(material, "WallContrastScale", wall_contrast_scale, -1000, 740)
    for expression, pin in ((local_position, "P"), (normal, "Normal"), (texture_object, "Tex"),
                            (tile_size, "TileSize"), (sharpness, "Sharpness"), (contrast, "Contrast"),
                            (wall_tile, "WallTileScale"), (wall_contrast, "WallContrastScale")):
        mel.connect_material_expressions(expression, "", triplanar, pin)

    # Arena mojada: la ola sube y baja por la orilla (periodo WetPeriod) y oscurece la arena hasta
    # donde llega; mas arriba, seca. Mismo periodo que la espuma de M_TortunaboWaterToon. Cota del
    # mundo (la local se mide desde el centro de los limites de cada trozo y cambia de uno a otro:
    # salian franjas por trozo).
    local_z = mel.create_material_expression(material, unreal.MaterialExpressionComponentMask, -900, -380)
    # Solo Z: R y G vienen activados por defecto y el nodo recibia la X del mundo (la arena se
    # oscurecia al sur de X = WaterZ en una franja recta que cruzaba todo el mapa).
    local_z.set_editor_property("r", False)
    local_z.set_editor_property("g", False)
    local_z.set_editor_property("b", True)
    mel.connect_material_expressions(world_position, "", local_z, "")
    time = mel.create_material_expression(material, unreal.MaterialExpressionTime, -900, -300)
    water_z = scalar_parameter(material, "WaterZ", -400.0, -900, -220)
    period = scalar_parameter(material, "WetPeriod", 4.0, -900, -140)
    wet = mel.create_material_expression(material, unreal.MaterialExpressionCustom, -600, -380)
    wet.set_editor_property("code", WET_HLSL)
    wet.set_editor_property("description", "ArenaMojada")
    wet.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    wet.set_editor_property("inputs", [custom_input(name) for name in ("Z", "T", "WaterZ", "Period")])
    for expression, pin in ((local_z, "Z"), (time, "T"), (water_z, "WaterZ"), (period, "Period")):
        mel.connect_material_expressions(expression, "", wet, pin)

    vertex_color = mel.create_material_expression(material, unreal.MaterialExpressionVertexColor, -600, -180)
    grained = mel.create_material_expression(material, unreal.MaterialExpressionMultiply, -400, -100)
    mel.connect_material_expressions(vertex_color, "", grained, "A")
    mel.connect_material_expressions(triplanar, "", grained, "B")
    base_color = mel.create_material_expression(material, unreal.MaterialExpressionMultiply, -250, -160)
    mel.connect_material_expressions(grained, "", base_color, "A")
    mel.connect_material_expressions(wet, "", base_color, "B")
    if detail_normals is None:
        mel.connect_material_property(base_color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    else:
        macro = mel.create_material_expression(material, unreal.MaterialExpressionCustom, -400, -520)
        macro.set_editor_property("code", MACRO_HLSL)
        macro.set_editor_property("description", "ManchasGrandes")
        macro.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT1)
        macro.set_editor_property("inputs", [custom_input(name) for name in ("P", "Tex", "Contrast")])
        mel.connect_material_expressions(local_position, "", macro, "P")
        mel.connect_material_expressions(texture_object, "", macro, "Tex")
        mel.connect_material_expressions(scalar_parameter(material, "MacroContrast", 0.22, -900, -600), "", macro,
                                         "Contrast")
        varied = mel.create_material_expression(material, unreal.MaterialExpressionMultiply, -120, -220)
        mel.connect_material_expressions(base_color, "", varied, "A")
        mel.connect_material_expressions(macro, "", varied, "B")
        mel.connect_material_property(varied, "", unreal.MaterialProperty.MP_BASE_COLOR)
        add_detail_normal(material, local_position, normal, vertex_color, detail_normals)

    roughness = mel.create_material_expression(material, unreal.MaterialExpressionConstant, -300, 300)
    roughness.set_editor_property("r", 0.95)
    mel.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)

    mel.recompile_material(material)
    asset_lib.save_loaded_asset(material)
    return material


def build_water_material():
    path = f"{ROOT}/M_GridWater"
    existing = load_or_none(path)
    if existing:
        return existing

    material = asset_tools.create_asset("M_GridWater", ROOT, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property("translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_SURFACE)
    mel = unreal.MaterialEditingLibrary

    color = mel.create_material_expression(material, unreal.MaterialExpressionConstant3Vector, -400, 0)
    color.set_editor_property("constant", unreal.LinearColor(0.03, 0.22, 0.30, 1.0))
    mel.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    for value, prop, y in ((0.08, unreal.MaterialProperty.MP_ROUGHNESS, 200),
                           (0.78, unreal.MaterialProperty.MP_OPACITY, 400)):
        constant = mel.create_material_expression(material, unreal.MaterialExpressionConstant, -400, y)
        constant.set_editor_property("r", value)
        mel.connect_material_property(constant, "", prop)

    mel.recompile_material(material)
    asset_lib.save_loaded_asset(material)
    return material


def build_junk_material():
    """Material de los objetos de basura: el color llega por instancia (PerInstanceCustomData)."""
    path = f"{ROOT}/M_GridJunk"
    existing = load_or_none(path)
    if existing:
        return existing

    material = asset_tools.create_asset("M_GridJunk", ROOT, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property("used_with_instanced_static_meshes", True)
    mel = unreal.MaterialEditingLibrary
    color = mel.create_material_expression(material, unreal.MaterialExpressionPerInstanceCustomData3Vector, -400, 0)
    color.set_editor_property("data_index", 0)
    color.set_editor_property("const_default_value", unreal.LinearColor(0.2, 0.2, 0.2, 1.0))
    mel.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    roughness = mel.create_material_expression(material, unreal.MaterialExpressionConstant, -400, 300)
    roughness.set_editor_property("r", 0.7)
    mel.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(material)
    asset_lib.save_loaded_asset(material)
    return material


def build_game_mode():
    path = f"{ROOT}/BP_GridDemoGameMode"
    existing = load_or_none(path)
    if existing:
        return existing

    blueprint = create_blueprint("BP_GridDemoGameMode", unreal.GameModeBase)
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    character = asset_lib.load_blueprint_class(CHARACTER_BP)
    unreal.get_default_object(blueprint.generated_class()).set_editor_property("default_pawn_class", character)
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    asset_lib.save_loaded_asset(blueprint)
    return blueprint


def main():
    asset_lib.make_directory(ROOT)
    asset_lib.make_directory(TEXTURE_ROOT)
    flat = build_flat_material()
    for name, rgb in COLORS.items():
        build_color_instance(name, rgb, flat)
    build_terrain_material(build_grain_texture())
    build_junk_material()
    build_water_material()
    build_game_mode()
    unreal.log("[GridDemo] Materiales del terreno y GameMode listos.")


if __name__ == "__main__":
    main()
