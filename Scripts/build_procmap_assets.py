"""Crea los assets del mapa procedural por módulos (World/ProcMap) y el nivel LVL_ProcMap.

Se ejecuta DENTRO del editor de Unreal, con el C++ ya compilado:
    exec(open(r"<repo>/Scripts/build_procmap_assets.py", encoding="utf-8").read())

Crea en /Game/ProcMap:
  - Materiales greybox: M_ProcTerrain (color de vértice), M_ProcFlat (+ MI de roca,
    madera, lava y tobogán), M_ProcWater (translúcido, + MI del mar) y M_ProcFoliage
    (color de vértice, para la vegetación procedural instanciada).
  - M_ProcWaterAnim: agua animada (dos capas de ondas que se desplazan, color por
    profundidad y espuma en las orillas) con MI_ProcSeaAnim (mar y lagunas) y
    MI_ProcSlideWaterAnim (toboganes, más clara y rápida).
  - DA_Biome_<Bioma> (UTN_ProcBiomeDataAsset) x8 rellenos con el greybox del código.
  - DA_ProcMapSettings (UTN_ProcMapSettings) con materiales, biomas y los 9 perfiles
    (Coop/Carrera/2vs2 x Fácil/Normal/Difícil) listos para ajustar.
  - BP_ProcMapGameMode (hijo de ATN_ProcMapGameMode) con los BP del juego.
Coloca en /Game/Maps/Lobby/LVL_HQ dos selectores (modo y dificultad) junto a la zona de
listos y crea /Game/Maps/Run/LVL_ProcMap (luz, cielo, niebla, generador y GameMode), que
queda abierto al terminar: basta con darle a Play.

Guarda antes lo que tengas abierto: el script cambia de nivel dos veces.

Idempotente: lo que ya existe se reutiliza sin tocarlo (para no pisar ajustes hechos a
mano). Para regenerar un asset, bórralo y vuelve a ejecutar el script.
"""

import unreal

ROOT = "/Game/ProcMap"
MATERIALS = f"{ROOT}/Materials"
BIOMES = f"{ROOT}/Biomes"
MAP_PATH = "/Game/Maps/Run/LVL_ProcMap"
LOBBY_MAP = "/Game/Maps/Lobby/LVL_HQ"

CHARACTER_BP = "/Game/Blueprints/Characters/BP_TortugaCharacter"
CONTROLLER_BP = "/Game/Blueprints/Gameplay/Controllers/BP_GamePlayerController"
RESCUE_BP = "/Game/Blueprints/Gameplay/Items/BP_RescuePickUp"

# (nombre del enum en Python, sufijo del asset)
BIOME_LIST = [
    ("JUNGLE", "Jungle"), ("BEACH", "Beach"), ("DESERT", "Desert"), ("VOLCANIC", "Volcanic"),
    ("WATER", "Water"), ("ROCKY", "Rocky"), ("MANGROVE", "Mangrove"), ("HUMAN", "Human"),
]

# MI sobre M_ProcFlat: color, rugosidad, emisivo.
FLAT_INSTANCES = {
    "MI_ProcRock": ((0.30, 0.29, 0.27), 0.9, 0.0),
    "MI_ProcWood": ((0.42, 0.27, 0.13), 0.8, 0.0),
    "MI_ProcLava": ((1.00, 0.32, 0.05), 0.6, 6.0),
    "MI_ProcSlideWater": ((0.20, 0.55, 0.80), 0.1, 0.4),
}

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
asset_lib = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary


def require_cpp():
    for name in ("TN_ProcMapGameMode", "TN_ProcMapGenerator", "TN_ProcMapSettings",
                 "TN_ProcBiomeDataAsset", "TN_ProcModeSelector"):
        if not hasattr(unreal, name):
            raise RuntimeError(f"No existe la clase {name}: compila el proyecto (Tortunabo) antes de ejecutar el script.")


def load_or_none(path):
    return asset_lib.load_asset(path) if asset_lib.does_asset_exist(path) else None


def save(asset):
    asset_lib.save_loaded_asset(asset)
    return asset


# ── Materiales ────────────────────────────────────────────────────────────────

def build_terrain_material():
    path = f"{MATERIALS}/M_ProcTerrain"
    existing = load_or_none(path)
    if existing:
        return existing
    material = asset_tools.create_asset("M_ProcTerrain", MATERIALS, unreal.Material, unreal.MaterialFactoryNew())
    # El terreno lleva el bioma, el camino y la roca en el color de vértice.
    vertex_color = mel.create_material_expression(material, unreal.MaterialExpressionVertexColor, -400, 0)
    mel.connect_material_property(vertex_color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    roughness = mel.create_material_expression(material, unreal.MaterialExpressionConstant, -400, 250)
    roughness.set_editor_property("r", 0.9)
    mel.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(material)
    return save(material)


def build_flat_material():
    path = f"{MATERIALS}/M_ProcFlat"
    existing = load_or_none(path)
    if existing:
        return existing
    material = asset_tools.create_asset("M_ProcFlat", MATERIALS, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property("used_with_instanced_static_meshes", True)

    color = mel.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -600, 0)
    color.set_editor_property("parameter_name", "Color")
    color.set_editor_property("default_value", unreal.LinearColor(0.5, 0.5, 0.5, 1.0))
    mel.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)

    roughness = mel.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -600, 250)
    roughness.set_editor_property("parameter_name", "Roughness")
    roughness.set_editor_property("default_value", 0.85)
    mel.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)

    emissive = mel.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -600, 400)
    emissive.set_editor_property("parameter_name", "Emissive")
    emissive.set_editor_property("default_value", 0.0)
    multiply = mel.create_material_expression(material, unreal.MaterialExpressionMultiply, -300, 350)
    mel.connect_material_expressions(color, "", multiply, "A")
    mel.connect_material_expressions(emissive, "", multiply, "B")
    mel.connect_material_property(multiply, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    mel.recompile_material(material)
    return save(material)


SIMPLE_GRASS_WIND = "/Engine/Functions/Engine_MaterialFunctions01/WorldPositionOffset/SimpleGrassWind.SimpleGrassWind"


def build_foliage_material():
    """Vegetación procedural (TN_ProcMapGenerator_Flora.cpp): mallas instanciadas con el color en el
    vértice y viento simulado del motor (SimpleGrassWind). El alfa del color de vértice es el peso de
    balanceo (0 en troncos, rocas y objetos; más en las copas y en las puntas de la hierba) y la
    intensidad sube y baja en rachas lentas que recorren el mapa. De una cara: frondas y hojas de hierba
    ya traen las dos caras en la malla. Si el material existe sin viento, se rehace su grafo."""
    path = f"{MATERIALS}/M_ProcFoliage"
    material = load_or_none(path)
    if material and mel.get_material_property_input_node(material, unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET):
        return material
    if material:
        mel.delete_all_material_expressions(material)
    else:
        material = asset_tools.create_asset("M_ProcFoliage", MATERIALS, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property("used_with_instanced_static_meshes", True)
    try:
        material.set_editor_property("max_world_position_offset_displacement", 120.0)
    except Exception as error:  # solo acota los límites de la malla desplazada
        unreal.log_warning(f"[ProcMap] M_ProcFoliage sin tope de desplazamiento: {error}")

    def expr(cls, x, y):
        return mel.create_material_expression(material, cls, x, y)

    def scalar(name, value, x, y):
        e = expr(unreal.MaterialExpressionScalarParameter, x, y)
        e.set_editor_property("parameter_name", name)
        e.set_editor_property("default_value", value)
        return e

    def const(value, x, y):
        e = expr(unreal.MaterialExpressionConstant, x, y)
        e.set_editor_property("r", value)
        return e

    vertex_color = expr(unreal.MaterialExpressionVertexColor, -500, 0)
    mel.connect_material_property(vertex_color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(const(0.85, -500, 200), "", unreal.MaterialProperty.MP_ROUGHNESS)

    # Rachas: sin(Tiempo * RachaVelocidad + X / 5000) entre 0,55 y 1,15 de la intensidad.
    time = expr(unreal.MaterialExpressionTime, -1500, 420)
    gust_speed = scalar("GustSpeed", 0.35, -1500, 500)
    time_scaled = expr(unreal.MaterialExpressionMultiply, -1300, 440)
    mel.connect_material_expressions(time, "", time_scaled, "A")
    mel.connect_material_expressions(gust_speed, "", time_scaled, "B")
    world = expr(unreal.MaterialExpressionWorldPosition, -1500, 620)
    mask_x = expr(unreal.MaterialExpressionComponentMask, -1300, 620)
    mask_x.set_editor_property("r", True)
    mask_x.set_editor_property("g", False)
    mask_x.set_editor_property("b", False)
    mask_x.set_editor_property("a", False)
    mel.connect_material_expressions(world, "", mask_x, "")
    phase = expr(unreal.MaterialExpressionDivide, -1150, 620)
    mel.connect_material_expressions(mask_x, "", phase, "A")
    mel.connect_material_expressions(const(5000.0, -1300, 720), "", phase, "B")
    arg = expr(unreal.MaterialExpressionAdd, -1000, 520)
    mel.connect_material_expressions(time_scaled, "", arg, "A")
    mel.connect_material_expressions(phase, "", arg, "B")
    sine = expr(unreal.MaterialExpressionSine, -850, 520)
    mel.connect_material_expressions(arg, "", sine, "")
    gust = expr(unreal.MaterialExpressionMultiply, -700, 520)
    mel.connect_material_expressions(sine, "", gust, "A")
    mel.connect_material_expressions(const(0.3, -850, 620), "", gust, "B")
    gust_bias = expr(unreal.MaterialExpressionAdd, -550, 520)
    mel.connect_material_expressions(gust, "", gust_bias, "A")
    mel.connect_material_expressions(const(0.85, -700, 620), "", gust_bias, "B")
    intensity = scalar("WindIntensity", 0.5, -700, 400)
    gusty = expr(unreal.MaterialExpressionMultiply, -400, 440)
    mel.connect_material_expressions(intensity, "", gusty, "A")
    mel.connect_material_expressions(gust_bias, "", gusty, "B")

    wind = expr(unreal.MaterialExpressionMaterialFunctionCall, -200, 420)
    wind.set_editor_property("material_function", unreal.load_asset(SIMPLE_GRASS_WIND))
    mel.connect_material_expressions(gusty, "", wind, "WindIntensity")
    mel.connect_material_expressions(vertex_color, "A", wind, "WindWeight")
    mel.connect_material_expressions(scalar("WindSpeed", 0.6, -400, 560), "", wind, "WindSpeed")
    # La función exige su entrada de desplazamiento adicional (sin ella el material no compila y se ve gris).
    zero = expr(unreal.MaterialExpressionConstant3Vector, -400, 700)
    zero.set_editor_property("constant", unreal.LinearColor(0.0, 0.0, 0.0, 0.0))
    mel.connect_material_expressions(zero, "", wind, "AdditionalWPO")
    mel.connect_material_property(wind, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    mel.recompile_material(material)
    return save(material)


WATER_NORMAL = "/Engine/Functions/Engine_MaterialFunctions02/ExampleContent/Textures/water_n.water_n"


def build_water_anim_material():
    """Agua animada: ondas (dos capas del normal de agua del motor desplazándose a distinta escala y
    velocidad, en coordenadas de mundo), color de somera a profunda según el fondo (DepthFade),
    espuma blanca pegada a las orillas y opacidad que crece con la profundidad. FlowSpeed acelera
    las ondas (toboganes)."""
    path = f"{MATERIALS}/M_ProcWaterAnim"
    existing = load_or_none(path)
    if existing:
        return existing
    material = asset_tools.create_asset("M_ProcWaterAnim", MATERIALS, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    try:
        material.set_editor_property("translucency_lighting_mode",
                                     unreal.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
    except Exception as error:  # el nombre del enum cambia entre versiones; el material funciona sin esto
        unreal.log_warning(f"[ProcMap] M_ProcWaterAnim sin iluminación por píxel: {error}")

    def expr(cls, x, y):
        return mel.create_material_expression(material, cls, x, y)

    def scalar(name, value, x, y):
        e = expr(unreal.MaterialExpressionScalarParameter, x, y)
        e.set_editor_property("parameter_name", name)
        e.set_editor_property("default_value", value)
        return e

    def vector(name, rgb, x, y):
        e = expr(unreal.MaterialExpressionVectorParameter, x, y)
        e.set_editor_property("parameter_name", name)
        e.set_editor_property("default_value", unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
        return e

    # Tiempo acelerable y coordenadas de mundo en planta.
    time = expr(unreal.MaterialExpressionTime, -1600, -200)
    flow = scalar("FlowSpeed", 1.0, -1600, -100)
    flow_time = expr(unreal.MaterialExpressionMultiply, -1400, -150)
    mel.connect_material_expressions(time, "", flow_time, "A")
    mel.connect_material_expressions(flow, "", flow_time, "B")
    world = expr(unreal.MaterialExpressionWorldPosition, -1600, 100)
    plan = expr(unreal.MaterialExpressionComponentMask, -1400, 100)
    plan.set_editor_property("r", True)
    plan.set_editor_property("g", True)
    mel.connect_material_expressions(world, "", plan, "")

    normals = []
    for i, (scale, speed) in enumerate(((900.0, (0.03, 0.018)), (2600.0, (-0.012, 0.026)))):
        uv = expr(unreal.MaterialExpressionDivide, -1200, 50 + 250 * i)
        uv.set_editor_property("const_b", scale)
        mel.connect_material_expressions(plan, "", uv, "A")
        pan = expr(unreal.MaterialExpressionPanner, -1000, 50 + 250 * i)
        pan.set_editor_property("speed_x", speed[0])
        pan.set_editor_property("speed_y", speed[1])
        mel.connect_material_expressions(uv, "", pan, "Coordinate")
        mel.connect_material_expressions(flow_time, "", pan, "Time")
        tex = expr(unreal.MaterialExpressionTextureSample, -800, 50 + 250 * i)
        tex.set_editor_property("texture", unreal.load_asset(WATER_NORMAL))
        tex.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        mel.connect_material_expressions(pan, "", tex, "UVs")
        normals.append(tex)
    both = expr(unreal.MaterialExpressionAdd, -550, 150)
    mel.connect_material_expressions(normals[0], "RGB", both, "A")
    mel.connect_material_expressions(normals[1], "RGB", both, "B")
    flat = expr(unreal.MaterialExpressionConstant3Vector, -550, 300)
    flat.set_editor_property("constant", unreal.LinearColor(0.0, 0.0, 1.0, 1.0))
    calm = scalar("Calm", 0.35, -550, 400)
    normal = expr(unreal.MaterialExpressionLinearInterpolate, -350, 200)
    mel.connect_material_expressions(both, "", normal, "A")
    mel.connect_material_expressions(flat, "", normal, "B")
    mel.connect_material_expressions(calm, "", normal, "Alpha")
    mel.connect_material_property(normal, "", unreal.MaterialProperty.MP_NORMAL)

    # Color: de somera a profunda según lo que hay debajo, y espuma pegada a las orillas.
    depth = expr(unreal.MaterialExpressionDepthFade, -800, -500)
    depth.set_editor_property("fade_distance_default", 500.0)
    depth_range = scalar("DepthRange", 500.0, -1000, -450)
    mel.connect_material_expressions(depth_range, "", depth, "FadeDistance")
    shallow = vector("ShallowColor", (0.08, 0.5, 0.52), -800, -750)
    deep = vector("DeepColor", (0.02, 0.16, 0.3), -800, -650)
    water_color = expr(unreal.MaterialExpressionLinearInterpolate, -550, -650)
    mel.connect_material_expressions(shallow, "", water_color, "A")
    mel.connect_material_expressions(deep, "", water_color, "B")
    mel.connect_material_expressions(depth, "", water_color, "Alpha")
    shore = expr(unreal.MaterialExpressionDepthFade, -800, -350)
    shore.set_editor_property("fade_distance_default", 70.0)
    foam_width = scalar("FoamWidth", 70.0, -1000, -300)
    mel.connect_material_expressions(foam_width, "", shore, "FadeDistance")
    foam = expr(unreal.MaterialExpressionOneMinus, -600, -350)
    mel.connect_material_expressions(shore, "", foam, "")
    foam_color = vector("FoamColor", (0.9, 0.95, 0.95), -550, -500)
    color = expr(unreal.MaterialExpressionLinearInterpolate, -300, -550)
    mel.connect_material_expressions(water_color, "", color, "A")
    mel.connect_material_expressions(foam_color, "", color, "B")
    mel.connect_material_expressions(foam, "", color, "Alpha")
    mel.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)

    # Opacidad: transparente en la orilla, más opaca con la profundidad (y la espuma se ve).
    opacity_depth = expr(unreal.MaterialExpressionDepthFade, -800, 550)
    opacity_depth.set_editor_property("fade_distance_default", 300.0)
    opacity = expr(unreal.MaterialExpressionLinearInterpolate, -550, 550)
    mel.connect_material_expressions(scalar("OpacityShallow", 0.35, -800, 650), "", opacity, "A")
    mel.connect_material_expressions(scalar("OpacityDeep", 0.85, -800, 750), "", opacity, "B")
    mel.connect_material_expressions(opacity_depth, "", opacity, "Alpha")
    opacity_foam = expr(unreal.MaterialExpressionMax, -300, 550)
    mel.connect_material_expressions(opacity, "", opacity_foam, "A")
    mel.connect_material_expressions(foam, "", opacity_foam, "B")
    mel.connect_material_property(opacity_foam, "", unreal.MaterialProperty.MP_OPACITY)

    roughness = expr(unreal.MaterialExpressionConstant, -300, 800)
    roughness.set_editor_property("r", 0.05)
    mel.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)

    mel.recompile_material(material)
    return save(material)


def build_fx_soft_material():
    """Efectos suaves (vapor de los géiseres, bruma y espuma de las cascadas): translúcido sin
    iluminación, del color del vértice, con la opacidad del alfa del vértice por Opacity y fundido
    suave donde toca otras superficies (DepthFade). Para mallas instanciadas."""
    path = f"{MATERIALS}/M_ProcFXSoft"
    existing = load_or_none(path)
    if existing:
        return existing
    material = asset_tools.create_asset("M_ProcFXSoft", MATERIALS, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("two_sided", True)
    material.set_editor_property("used_with_instanced_static_meshes", True)

    def expr(cls, x, y):
        return mel.create_material_expression(material, cls, x, y)

    vertex_color = expr(unreal.MaterialExpressionVertexColor, -700, 0)
    mel.connect_material_property(vertex_color, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    opacity = expr(unreal.MaterialExpressionScalarParameter, -700, 250)
    opacity.set_editor_property("parameter_name", "Opacity")
    opacity.set_editor_property("default_value", 0.55)
    alpha = expr(unreal.MaterialExpressionMultiply, -500, 200)
    mel.connect_material_expressions(vertex_color, "A", alpha, "A")
    mel.connect_material_expressions(opacity, "", alpha, "B")
    fade = expr(unreal.MaterialExpressionDepthFade, -300, 200)
    fade.set_editor_property("fade_distance_default", 80.0)
    mel.connect_material_expressions(alpha, "", fade, "Opacity")
    mel.connect_material_property(fade, "", unreal.MaterialProperty.MP_OPACITY)
    mel.recompile_material(material)
    return save(material)


def build_bird_material():
    """Pájaros de las bandadas (TN_ProcMapAmbientFX.h): color de vértice y aleteo en vertical con el alfa
    del vértice como peso (0 en el cuerpo, 1 en las puntas de las alas), desfasado por instancia."""
    path = f"{MATERIALS}/M_ProcBird"
    existing = load_or_none(path)
    if existing:
        return existing
    material = asset_tools.create_asset("M_ProcBird", MATERIALS, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property("two_sided", True)
    material.set_editor_property("used_with_instanced_static_meshes", True)

    def expr(cls, x, y):
        return mel.create_material_expression(material, cls, x, y)

    def scalar(name, value, x, y):
        e = expr(unreal.MaterialExpressionScalarParameter, x, y)
        e.set_editor_property("parameter_name", name)
        e.set_editor_property("default_value", value)
        return e

    vertex_color = expr(unreal.MaterialExpressionVertexColor, -900, 0)
    mel.connect_material_property(vertex_color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    time = expr(unreal.MaterialExpressionTime, -1100, 300)
    rate = expr(unreal.MaterialExpressionMultiply, -900, 300)
    mel.connect_material_expressions(time, "", rate, "A")
    mel.connect_material_expressions(scalar("FlapSpeed", 9.0, -1100, 400), "", rate, "B")
    rnd = expr(unreal.MaterialExpressionPerInstanceRandom, -1100, 520)
    offset = expr(unreal.MaterialExpressionMultiply, -900, 520)
    offset.set_editor_property("const_b", 6.2832)
    mel.connect_material_expressions(rnd, "", offset, "A")
    phase = expr(unreal.MaterialExpressionAdd, -700, 380)
    mel.connect_material_expressions(rate, "", phase, "A")
    mel.connect_material_expressions(offset, "", phase, "B")
    sine = expr(unreal.MaterialExpressionSine, -550, 380)
    mel.connect_material_expressions(phase, "", sine, "")
    weighted = expr(unreal.MaterialExpressionMultiply, -400, 380)
    mel.connect_material_expressions(sine, "", weighted, "A")
    mel.connect_material_expressions(vertex_color, "A", weighted, "B")
    amount = expr(unreal.MaterialExpressionMultiply, -250, 380)
    mel.connect_material_expressions(weighted, "", amount, "A")
    mel.connect_material_expressions(scalar("FlapAmount", 22.0, -400, 500), "", amount, "B")
    zero2 = expr(unreal.MaterialExpressionConstant2Vector, -250, 250)
    wpo = expr(unreal.MaterialExpressionAppendVector, -100, 300)
    mel.connect_material_expressions(zero2, "", wpo, "A")
    mel.connect_material_expressions(amount, "", wpo, "B")
    mel.connect_material_property(wpo, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    mel.recompile_material(material)
    return save(material)


def build_cascade_material():
    """Agua de las cascadas-tobogán: ondas del normal de agua del motor que corren a lo largo de la UV
    V (ladera abajo; la malla la da en metros recorridos) en dos capas a distinta velocidad, del
    color del vértice (espuma blanca en los bordes y al pie) y con la opacidad del alfa del vértice.
    FlowSpeed acelera la corriente."""
    path = f"{MATERIALS}/M_ProcCascade"
    existing = load_or_none(path)
    if existing:
        return existing
    material = asset_tools.create_asset("M_ProcCascade", MATERIALS, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    try:
        material.set_editor_property("translucency_lighting_mode",
                                     unreal.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
    except Exception as error:
        unreal.log_warning(f"[ProcMap] M_ProcCascade sin iluminación por píxel: {error}")

    def expr(cls, x, y):
        return mel.create_material_expression(material, cls, x, y)

    def scalar(name, value, x, y):
        e = expr(unreal.MaterialExpressionScalarParameter, x, y)
        e.set_editor_property("parameter_name", name)
        e.set_editor_property("default_value", value)
        return e

    vertex_color = expr(unreal.MaterialExpressionVertexColor, -600, -300)
    mel.connect_material_property(vertex_color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    opacity = expr(unreal.MaterialExpressionMultiply, -350, 350)
    mel.connect_material_expressions(vertex_color, "A", opacity, "A")
    mel.connect_material_expressions(scalar("Opacity", 0.85, -600, 420), "", opacity, "B")
    mel.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY)

    time = expr(unreal.MaterialExpressionTime, -1600, 0)
    flow = scalar("FlowSpeed", 1.0, -1600, 100)
    flow_time = expr(unreal.MaterialExpressionMultiply, -1400, 50)
    mel.connect_material_expressions(time, "", flow_time, "A")
    mel.connect_material_expressions(flow, "", flow_time, "B")
    uv = expr(unreal.MaterialExpressionTextureCoordinate, -1600, 250)
    normals = []
    for i, (scale, sx, sy) in enumerate(((1.0, 0.02, -0.9), (0.45, -0.03, -0.55))):
        scaled = expr(unreal.MaterialExpressionMultiply, -1300, 200 + 220 * i)
        scaled.set_editor_property("const_b", scale)
        mel.connect_material_expressions(uv, "", scaled, "A")
        pan = expr(unreal.MaterialExpressionPanner, -1100, 200 + 220 * i)
        pan.set_editor_property("speed_x", sx)
        pan.set_editor_property("speed_y", sy)
        mel.connect_material_expressions(scaled, "", pan, "Coordinate")
        mel.connect_material_expressions(flow_time, "", pan, "Time")
        tex = expr(unreal.MaterialExpressionTextureSample, -850, 200 + 220 * i)
        tex.set_editor_property("texture", unreal.load_asset(WATER_NORMAL))
        tex.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        mel.connect_material_expressions(pan, "", tex, "UVs")
        normals.append(tex)
    blend = expr(unreal.MaterialExpressionAdd, -550, 300)
    mel.connect_material_expressions(normals[0], "RGB", blend, "A")
    mel.connect_material_expressions(normals[1], "RGB", blend, "B")
    normal = expr(unreal.MaterialExpressionNormalize, -400, 300)
    mel.connect_material_expressions(blend, "", normal, "")
    mel.connect_material_property(normal, "", unreal.MaterialProperty.MP_NORMAL)
    roughness = expr(unreal.MaterialExpressionConstant, -350, 550)
    roughness.set_editor_property("r", 0.08)
    mel.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(material)
    return save(material)


def build_water_material():
    path = f"{MATERIALS}/M_ProcWater"
    existing = load_or_none(path)
    if existing:
        return existing
    material = asset_tools.create_asset("M_ProcWater", MATERIALS, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    try:
        material.set_editor_property("translucency_lighting_mode",
                                     unreal.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
    except Exception as error:  # el nombre del enum cambia entre versiones; el material funciona sin esto
        unreal.log_warning(f"[ProcMap] M_ProcWater sin iluminación por píxel: {error}")

    color = mel.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -600, 0)
    color.set_editor_property("parameter_name", "Color")
    color.set_editor_property("default_value", unreal.LinearColor(0.05, 0.32, 0.45, 1.0))
    mel.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)

    opacity = mel.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -600, 250)
    opacity.set_editor_property("parameter_name", "Opacity")
    opacity.set_editor_property("default_value", 0.7)
    mel.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY)

    roughness = mel.create_material_expression(material, unreal.MaterialExpressionConstant, -600, 400)
    roughness.set_editor_property("r", 0.05)
    mel.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)

    mel.recompile_material(material)
    return save(material)


def build_instance(name, parent, vectors=None, scalars=None):
    path = f"{MATERIALS}/{name}"
    existing = load_or_none(path)
    if existing:
        return existing
    instance = asset_tools.create_asset(
        name, MATERIALS, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(instance, parent)
    for key, rgb in (vectors or {}).items():
        mel.set_material_instance_vector_parameter_value(instance, key, unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
    for key, value in (scalars or {}).items():
        mel.set_material_instance_scalar_parameter_value(instance, key, value)
    mel.update_material_instance(instance)
    return save(instance)


def build_materials():
    asset_lib.make_directory(MATERIALS)
    flat = build_flat_material()
    water = build_water_material()
    water_anim = build_water_anim_material()
    result = {
        "terrain": build_terrain_material(),
        "foliage": build_foliage_material(),
        "fx_soft": build_fx_soft_material(),
        "cascade": build_cascade_material(),
        "bird": build_bird_material(),
        "water": build_instance("MI_ProcSea", water, {"Color": (0.05, 0.30, 0.45)}, {"Opacity": 0.72}),
    }
    for name, (rgb, rough, emissive) in FLAT_INSTANCES.items():
        result[name] = build_instance(name, flat, {"Color": rgb}, {"Roughness": rough, "Emissive": emissive})
    result["sea_anim"] = build_instance("MI_ProcSeaAnim", water_anim)
    result["slide_anim"] = build_instance("MI_ProcSlideWaterAnim", water_anim,
                                          {"ShallowColor": (0.55, 0.8, 1.0), "DeepColor": (0.3, 0.6, 0.9), "FoamColor": (0.97, 0.99, 1.0)},
                                          {"FlowSpeed": 5.0, "DepthRange": 60.0, "FoamWidth": 25.0, "OpacityShallow": 0.6, "Calm": 0.15})
    return result


# ── DataAssets ────────────────────────────────────────────────────────────────

def create_data_asset(name, folder, asset_class):
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", asset_class)
    return asset_tools.create_asset(name, folder, asset_class, factory)


def build_biomes():
    asset_lib.make_directory(BIOMES)
    assets = []
    for enum_name, label in BIOME_LIST:
        path = f"{BIOMES}/DA_Biome_{label}"
        asset = load_or_none(path)
        if not asset:
            asset = create_data_asset(f"DA_Biome_{label}", BIOMES, unreal.TN_ProcBiomeDataAsset)
            asset.set_editor_property("biome", getattr(unreal.TNProcBiome, enum_name))
            # Colores, vegetación y peligros greybox del código, listos para cambiar por arte.
            asset.reset_to_greybox_defaults()
            save(asset)
        assets.append(asset)
    return assets


def build_settings(materials, biomes):
    path = f"{ROOT}/DA_ProcMapSettings"
    existing = load_or_none(path)
    if existing:
        # Ajustes añadidos después de crear el asset: solo si faltan o siguen con el greybox de antes.
        changed = False
        if not existing.get_editor_property("foliage_material"):
            existing.set_editor_property("foliage_material", materials["foliage"])
            changed = True
        for prop, old, new in (("water_material", "MI_ProcSea", "sea_anim"), ("slide_water_material", "MI_ProcSlideWater", "slide_anim")):
            current = existing.get_editor_property(prop)
            if not current or current.get_name() == old:
                existing.set_editor_property(prop, materials[new])
                changed = True
        if changed:
            save(existing)
        return existing
    settings = create_data_asset("DA_ProcMapSettings", ROOT, unreal.TN_ProcMapSettings)
    settings.set_editor_property("biomes", biomes)
    settings.set_editor_property("terrain_material", materials["terrain"])
    settings.set_editor_property("water_material", materials["sea_anim"])
    settings.set_editor_property("lava_material", materials["MI_ProcLava"])
    settings.set_editor_property("rock_material", materials["MI_ProcRock"])
    settings.set_editor_property("wood_material", materials["MI_ProcWood"])
    settings.set_editor_property("slide_water_material", materials["slide_anim"])
    settings.set_editor_property("foliage_material", materials["foliage"])
    settings.fill_default_profiles()
    return save(settings)


# ── GameMode ──────────────────────────────────────────────────────────────────

def build_game_mode(settings):
    path = f"{ROOT}/BP_ProcMapGameMode"
    existing = load_or_none(path)
    if existing:
        return existing
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", unreal.TN_ProcMapGameMode)
    blueprint = asset_tools.create_asset("BP_ProcMapGameMode", ROOT, unreal.Blueprint, factory)
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)

    defaults = unreal.get_default_object(blueprint.generated_class())
    defaults.set_editor_property("map_settings", settings)
    # El constructor C++ ya los busca; aquí quedan explícitos en el BP.
    for prop, bp_path in (("default_pawn_class", CHARACTER_BP),
                          ("player_controller_class", CONTROLLER_BP),
                          ("rescue_pickup_class", RESCUE_BP)):
        if asset_lib.does_asset_exist(bp_path):
            defaults.set_editor_property(prop, asset_lib.load_blueprint_class(bp_path))
        else:
            unreal.log_warning(f"[ProcMap] No existe {bp_path}: {prop} queda con el valor de C++.")
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    return save(blueprint)


# ── Niveles ───────────────────────────────────────────────────────────────────

def build_map(game_mode_bp, settings):
    level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

    if asset_lib.does_asset_exist(MAP_PATH):
        level_subsystem.load_level(MAP_PATH)
    else:
        level_subsystem.new_level(MAP_PATH)

    by_label = {actor.get_actor_label(): actor for actor in actor_subsystem.get_all_level_actors()}

    def ensure_actor(label, actor_class, location=unreal.Vector(0, 0, 0), rotation=unreal.Rotator(0, 0, 0)):
        if label in by_label:
            return by_label[label]
        actor = actor_subsystem.spawn_actor_from_class(actor_class, location, rotation)
        actor.set_actor_label(label)
        return actor

    # unreal.Rotator(roll, pitch, yaw)
    ensure_actor("Sun", unreal.DirectionalLight, unreal.Vector(0, 0, 20000), unreal.Rotator(0.0, -42.0, 30.0))
    ensure_actor("SkyAtmosphere", unreal.SkyAtmosphere)
    sky_light = ensure_actor("SkyLight", unreal.SkyLight, unreal.Vector(0, 0, 20000))
    sky_light.get_component_by_class(unreal.SkyLightComponent).set_editor_property("real_time_capture", True)
    ensure_actor("HeightFog", unreal.ExponentialHeightFog, unreal.Vector(0, 0, -500))

    # El GameMode genera el mapa en runtime a partir de la semilla; el generador del
    # nivel solo aporta los ajustes (y el botón GenerateInEditor para previsualizar).
    generator = ensure_actor("ProcMapGenerator", unreal.TN_ProcMapGenerator)
    generator.set_editor_property("settings", settings)

    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    world.get_world_settings().set_editor_property("default_game_mode", game_mode_bp.generated_class())
    level_subsystem.save_current_level()


def place_lobby_selectors():
    if not asset_lib.does_asset_exist(LOBBY_MAP):
        unreal.log_warning(f"[ProcMap] No existe {LOBBY_MAP}: coloca los ATN_ProcModeSelector a mano.")
        return

    level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    level_subsystem.load_level(LOBBY_MAP)
    actors = actor_subsystem.get_all_level_actors()

    if any(isinstance(actor, unreal.TN_ProcModeSelector) for actor in actors):
        unreal.log("[ProcMap] LVL_HQ ya tiene selectores de modo: no se tocan.")
        return

    # Junto a la zona de listos del lobby (o en el origen si no la hay), a ras de suelo.
    anchor = None
    ready_zone_class = getattr(unreal, "TN_LobbyReadyZone", None)
    if ready_zone_class:
        anchor = next((actor for actor in actors if isinstance(actor, ready_zone_class)), None)
    if anchor:
        origin, extent = anchor.get_actor_bounds(False)
        base = unreal.Vector(origin.x - extent.x - 250.0, origin.y, origin.z - extent.z)
    else:
        unreal.log_warning("[ProcMap] No hay TN_LobbyReadyZone en LVL_HQ: selectores en el origen, muévelos a mano.")
        base = unreal.Vector(0, 0, 0)

    for label, kind, offset in (("ProcModeSelector_Mode", unreal.TNProcSelectorKind.MODE, -220.0),
                                ("ProcModeSelector_Difficulty", unreal.TNProcSelectorKind.DIFFICULTY, 220.0)):
        selector = actor_subsystem.spawn_actor_from_class(
            unreal.TN_ProcModeSelector, base + unreal.Vector(0.0, offset, 0.0), unreal.Rotator(0.0, 0.0, 0.0))
        selector.set_actor_label(label)
        selector.set_editor_property("kind", kind)

    level_subsystem.save_current_level()
    unreal.log("[ProcMap] Selectores de modo y dificultad colocados en LVL_HQ (revisa su posición).")


def main():
    require_cpp()
    asset_lib.make_directory(ROOT)
    materials = build_materials()
    biomes = build_biomes()
    settings = build_settings(materials, biomes)
    game_mode_bp = build_game_mode(settings)
    place_lobby_selectors()
    # El último nivel que se abre es LVL_ProcMap: queda listo para darle a Play.
    build_map(game_mode_bp, settings)
    unreal.log("[ProcMap] Listo: LVL_ProcMap abierto (Play = Coop Normal 6x6). "
               "Modo/dificultad sin lobby: BP_ProcMapGameMode > Mode/Difficulty Without Lobby.")


main()
