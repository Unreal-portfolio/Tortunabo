"""Construye los assets greybox de la demo del mapa en grid.

Se ejecuta DENTRO del editor de Unreal (consola Python, MCP o -run=pythonscript):
    exec(open(r"<repo>/Scripts/build_grid_demo_assets.py", encoding="utf-8").read())

Crea en /Game/Blueprints/Gameplay/GridMap: material plano + instancias de color,
tiles greybox de recta y giro, tres rellenos, el material y el BP del tile de terreno,
el material del agua, el GameMode de la demo y BP_GridMapGenerator (en modo terreno).
Crea el mapa /Game/Maps/Run/LVL_ProcGenDemo con luz, cielo, generador y PlayerStart.

Idempotente: los assets que ya existen se reutilizan, no se recrean.
El generador queda en modo terreno con celdas de TERRAIN_CELL_SIZE. Para probar el modo
greybox hay que vaciar TerrainTileClass y poner CellSize = CELL_SIZE en el generador.
"""

import unreal

ROOT = "/Game/Blueprints/Gameplay/GridMap"
MAP_PATH = "/Game/Maps/Run/LVL_ProcGenDemo"
CUBE_PATH = "/Engine/BasicShapes/Cube"
CHARACTER_BP = "/Game/Blueprints/Characters/BP_TortugaCharacter"
GENERATOR_CLASS = "/Script/Tortunabo.TN_GridMapGenerator"
TERRAIN_TILE_CLASS = "/Script/Tortunabo.TN_GridTerrainTile"

CELL_SIZE = 2000.0  # tiles greybox
TERRAIN_CELL_SIZE = 4000.0  # modo terreno: debe casar con FTNGridTerrainSettings
CUBE_SIZE = 100.0
WALL_THICKNESS = 50.0
WALL_HEIGHT = 400.0
FLOOR_THICKNESS = 50.0

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


def add_box(blueprint, name, size, location, material, cube, yaw=0.0):
    """Añade un StaticMeshComponent con forma de caja (size y location en uu)."""
    root_handle = subobjects.k2_gather_subobject_data_for_blueprint(blueprint)[0]
    params = unreal.AddNewSubobjectParams(
        parent_handle=root_handle, new_class=unreal.StaticMeshComponent, blueprint_context=blueprint)
    handle, fail_reason = subobjects.add_new_subobject(params)
    if not unreal.SubobjectDataBlueprintFunctionLibrary.is_handle_valid(handle):
        raise RuntimeError(f"No se pudo añadir '{name}' a {blueprint.get_name()}: {fail_reason}")

    subobjects.rename_subobject(handle, unreal.Text(name))
    data = subobjects.k2_find_subobject_data_from_handle(handle)
    component = unreal.SubobjectDataBlueprintFunctionLibrary.get_associated_object(data)
    component.set_editor_property("static_mesh", cube)
    component.set_editor_property("relative_location", unreal.Vector(*location))
    component.set_editor_property("relative_rotation", unreal.Rotator(0.0, 0.0, yaw))
    component.set_editor_property(
        "relative_scale3d", unreal.Vector(size[0] / CUBE_SIZE, size[1] / CUBE_SIZE, size[2] / CUBE_SIZE))
    component.set_editor_property("override_materials", [material])
    component.set_editor_property("mobility", unreal.ComponentMobility.STATIC)


def build_tile(name, boxes, cube):
    """boxes: lista de (nombre, size, location, material[, yaw])."""
    path = f"{ROOT}/{name}"
    existing = load_or_none(path)
    if existing:
        return existing

    blueprint = create_blueprint(name, unreal.Actor)
    for box in boxes:
        add_box(blueprint, box[0], box[1], box[2], box[3], cube, box[4] if len(box) > 4 else 0.0)
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    asset_lib.save_loaded_asset(blueprint)
    return blueprint


def build_tiles(materials, cube):
    half = CELL_SIZE / 2.0
    wall_offset = half - WALL_THICKNESS / 2.0
    wall_z = WALL_HEIGHT / 2.0
    floor = lambda mat: ("Floor", (CELL_SIZE, CELL_SIZE, FLOOR_THICKNESS), (0.0, 0.0, -FLOOR_THICKNESS / 2.0), mat)
    wall_along_x = lambda n, y: (n, (CELL_SIZE, WALL_THICKNESS, WALL_HEIGHT), (0.0, y, wall_z), materials["MI_Grid_Wall"])
    wall_along_y = lambda n, x: (n, (WALL_THICKNESS, CELL_SIZE, WALL_HEIGHT), (x, 0.0, wall_z), materials["MI_Grid_Wall"])

    path_mat = materials["MI_Grid_Path"]
    return {
        # Recta canónica: abierta a lo largo de X, paredes en ±Y.
        "straight": build_tile("BP_GridTile_Straight", [
            floor(path_mat), wall_along_x("WallLeft", -wall_offset), wall_along_x("WallRight", wall_offset),
        ], cube),
        # Giro canónico: abierto por -X y +Y, paredes en +X y -Y, poste en la esquina interior.
        "turn": build_tile("BP_GridTile_Turn", [
            floor(path_mat), wall_along_y("WallFront", wall_offset), wall_along_x("WallLeft", -wall_offset),
            ("CornerPost", (WALL_THICKNESS, WALL_THICKNESS, WALL_HEIGHT), (-wall_offset, wall_offset, wall_z),
             materials["MI_Grid_Wall"]),
        ], cube),
        "fillers": [
            build_tile("BP_GridFiller_Dune", [
                floor(materials["MI_Grid_Dune"]),
                ("Mound", (1400.0, 1400.0, 300.0), (0.0, 0.0, 150.0), materials["MI_Grid_Dune"]),
                ("Crest", (800.0, 800.0, 250.0), (100.0, -100.0, 425.0), materials["MI_Grid_Dune"], 30.0),
            ], cube),
            build_tile("BP_GridFiller_Rock", [
                floor(materials["MI_Grid_Dune"]),
                ("RockBig", (1000.0, 900.0, 800.0), (-150.0, 100.0, 400.0), materials["MI_Grid_Rock"], 25.0),
                ("RockSmall", (500.0, 450.0, 400.0), (550.0, -450.0, 200.0), materials["MI_Grid_Rock"], -15.0),
            ], cube),
            build_tile("BP_GridFiller_Water", [
                ("Water", (CELL_SIZE, CELL_SIZE, 20.0), (0.0, 0.0, -70.0), materials["MI_Grid_Water"]),
            ], cube),
        ],
    }


def build_terrain_material():
    """El color del terreno llega ya calculado en el color de vértice."""
    path = f"{ROOT}/M_GridTerrain"
    existing = load_or_none(path)
    if existing:
        return existing

    material = asset_tools.create_asset("M_GridTerrain", ROOT, unreal.Material, unreal.MaterialFactoryNew())
    mel = unreal.MaterialEditingLibrary
    vertex_color = mel.create_material_expression(material, unreal.MaterialExpressionVertexColor, -400, 0)
    mel.connect_material_property(vertex_color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    roughness = mel.create_material_expression(material, unreal.MaterialExpressionConstant, -400, 300)
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


def build_terrain_tile(terrain_material):
    path = f"{ROOT}/BP_GridTerrainTile"
    blueprint = load_or_none(path)
    if not blueprint:
        blueprint = create_blueprint("BP_GridTerrainTile", unreal.load_class(None, TERRAIN_TILE_CLASS))
        unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)

    unreal.get_default_object(blueprint.generated_class()).set_editor_property("terrain_material", terrain_material)
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    asset_lib.save_loaded_asset(blueprint)
    return blueprint


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


def build_generator(tiles, terrain_tile_bp, water_material):
    path = f"{ROOT}/BP_GridMapGenerator"
    blueprint = load_or_none(path)
    if not blueprint:
        blueprint = create_blueprint("BP_GridMapGenerator", unreal.load_class(None, GENERATOR_CLASS))
        unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)

    defaults = unreal.get_default_object(blueprint.generated_class())
    defaults.set_editor_property("straight_tile_class", tiles["straight"].generated_class())
    defaults.set_editor_property("turn_tile_class", tiles["turn"].generated_class())
    defaults.set_editor_property("filler_tile_classes", [bp.generated_class() for bp in tiles["fillers"]])
    defaults.set_editor_property("cell_size", TERRAIN_CELL_SIZE)
    # Con TerrainTileClass asignado el generador trabaja en modo terreno; los tiles
    # greybox se quedan configurados como alternativa (basta con vaciar esta propiedad).
    defaults.set_editor_property("terrain_tile_class", terrain_tile_bp.generated_class())
    defaults.set_editor_property("water_material", water_material)
    # Sin recompilar, las instancias nuevas no heredan los defaults recién escritos en el CDO.
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    asset_lib.save_loaded_asset(blueprint)
    return blueprint


def build_map(generator_bp, game_mode_bp):
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

    ensure_actor("Sun", unreal.DirectionalLight, unreal.Vector(0, 0, 3000), unreal.Rotator(0.0, -50.0, 35.0))
    ensure_actor("SkyAtmosphere", unreal.SkyAtmosphere)
    sky_light = ensure_actor("SkyLight", unreal.SkyLight, unreal.Vector(0, 0, 3000))
    # Luz de cielo móvil y reforzada: el terreno es un cañón y las paredes en sombra se
    # quedaban negras con los valores por defecto.
    sky_component = sky_light.get_component_by_class(unreal.SkyLightComponent)
    sky_component.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
    sky_component.set_editor_property("real_time_capture", True)
    sky_component.set_editor_property("intensity", 2.0)

    generator = ensure_actor("GridMapGenerator", generator_bp.generated_class())
    generator.call_method("Generate")

    # Con semilla fija el inicio del camino es estable: el PlayerStart se coloca sobre él.
    # Los tiles son Transient y EditorActorSubsystem no los lista: se buscan por tag en el mundo.
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    start_tiles = unreal.GameplayStatics.get_all_actors_with_tag(world, "TNGridStart")
    if not start_tiles:
        raise RuntimeError("Generate() no ha producido tile de inicio; revisa el Output Log ([GridMap]).")
    player_start = ensure_actor("PlayerStart_PathStart", unreal.PlayerStart)
    player_start.set_actor_location(
        start_tiles[0].get_actor_location() + unreal.Vector(-TERRAIN_CELL_SIZE * 0.3, 0.0, 250.0), False, False)

    world.get_world_settings().set_editor_property("default_game_mode", game_mode_bp.generated_class())
    level_subsystem.save_current_level()


def main():
    asset_lib.make_directory(ROOT)
    cube = asset_lib.load_asset(CUBE_PATH)
    flat = build_flat_material()
    materials = {name: build_color_instance(name, rgb, flat) for name, rgb in COLORS.items()}
    tiles = build_tiles(materials, cube)
    terrain_tile_bp = build_terrain_tile(build_terrain_material())
    generator_bp = build_generator(tiles, terrain_tile_bp, build_water_material())
    game_mode_bp = build_game_mode()
    build_map(generator_bp, game_mode_bp)
    unreal.log("[GridDemo] Assets y mapa de la demo listos.")


main()
