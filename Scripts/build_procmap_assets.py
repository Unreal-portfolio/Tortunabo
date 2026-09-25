"""Crea los assets del mapa procedural por módulos (World/ProcMap) y el nivel LVL_ProcMap.

Se ejecuta DENTRO del editor de Unreal, con el C++ ya compilado:
    exec(open(r"<repo>/Scripts/build_procmap_assets.py", encoding="utf-8").read())

Crea en /Game/ProcMap:
  - Materiales greybox: M_ProcTerrain (color de vértice), M_ProcFlat (+ MI de roca,
    madera, lava y tobogán), M_ProcWater (translúcido, + MI del mar) y M_ProcFoliage
    (color de vértice, para la vegetación procedural instanciada).
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


def build_foliage_material():
    path = f"{MATERIALS}/M_ProcFoliage"
    existing = load_or_none(path)
    if existing:
        return existing
    material = asset_tools.create_asset("M_ProcFoliage", MATERIALS, unreal.Material, unreal.MaterialFactoryNew())
    # Vegetación procedural (TN_ProcMapGenerator_Flora.cpp): mallas instanciadas con el color en
    # el vértice. De una cara: frondas y hojas de hierba ya traen las dos caras en la malla (con un
    # material de dos caras se pelearían en profundidad).
    material.set_editor_property("used_with_instanced_static_meshes", True)
    vertex_color = mel.create_material_expression(material, unreal.MaterialExpressionVertexColor, -400, 0)
    mel.connect_material_property(vertex_color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    roughness = mel.create_material_expression(material, unreal.MaterialExpressionConstant, -400, 250)
    roughness.set_editor_property("r", 0.85)
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
    result = {
        "terrain": build_terrain_material(),
        "foliage": build_foliage_material(),
        "water": build_instance("MI_ProcSea", water, {"Color": (0.05, 0.30, 0.45)}, {"Opacity": 0.72}),
    }
    for name, (rgb, rough, emissive) in FLAT_INSTANCES.items():
        result[name] = build_instance(name, flat, {"Color": rgb}, {"Roughness": rough, "Emissive": emissive})
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
        # Ajustes añadidos después de crear el asset: solo si faltan.
        if not existing.get_editor_property("foliage_material"):
            existing.set_editor_property("foliage_material", materials["foliage"])
            save(existing)
        return existing
    settings = create_data_asset("DA_ProcMapSettings", ROOT, unreal.TN_ProcMapSettings)
    settings.set_editor_property("biomes", biomes)
    settings.set_editor_property("terrain_material", materials["terrain"])
    settings.set_editor_property("water_material", materials["water"])
    settings.set_editor_property("lava_material", materials["MI_ProcLava"])
    settings.set_editor_property("rock_material", materials["MI_ProcRock"])
    settings.set_editor_property("wood_material", materials["MI_ProcWood"])
    settings.set_editor_property("slide_water_material", materials["MI_ProcSlideWater"])
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
