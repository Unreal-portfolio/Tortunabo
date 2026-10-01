"""Mapa galería con todos los assets del juego (issue #312).

Importa los FBX de Art/Source y Art/Library/IA en /Game/Art/... (solo si faltan) y genera
/Game/Maps/Dev/LVL_GaleriaAssets con cada StaticMesh y SkeletalMesh de /Game y cada Blueprint de actor
con presencia en el mundo, en una cuadrícula agrupada por carpeta y con su nombre delante. Al final
comprueba que hay un actor por asset. Lo que el juego construye por código (p. ej. los elementos de playa
ATN_Beach*) no es un asset y no sale.

Uso (editor cerrado):
  UnrealEditor-Win64-DebugGame-Cmd.exe Tortunabo.uproject -run=pythonscript
    -script=Scripts/tools/galeria_assets.py -EnablePlugins=PythonScriptPlugin -unattended -nosplash -nullrhi
Variables: TN_GALERIA_REIMPORT=1 reimporta los FBX aunque ya existan.
"""

import os
from collections import defaultdict

import unreal

MAP_PATH = "/Game/Maps/Dev/LVL_GaleriaAssets"
ART_ROOTS = ("Art/Source", "Art/Library/IA")
EXCLUDED = ("/_Deprecado", "/Deprecado", "/Developers", "/OLD/", "/Game/Maps/Dev/")
MAX_ROW_CM = 30000.0
GAP_CM = 250.0
ROW_GAP_CM = 900.0
MIN_CELL_CM = 200.0
LOG_PREFIX = "[Galeria]"
# Blueprints sin malla propia o que no son objetos del mundo: modos, controladores, volúmenes y gestores.
BP_SKIP = ("GameMode", "GameInstance", "Controller", "Manager", "Zone", "Volume", "Spawner")


def log(msg):
    unreal.log(f"{LOG_PREFIX} {msg}")


def project_dir():
    return os.path.normpath(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))


def fbx_tasks():
    root = project_dir()
    reimport = os.environ.get("TN_GALERIA_REIMPORT") == "1"
    tasks = []
    for art_root in ART_ROOTS:
        base = os.path.join(root, art_root)
        for folder, _, files in os.walk(base):
            if "_pipeline" in folder:
                continue
            for name in files:
                if not name.lower().endswith(".fbx"):
                    continue
                rel = os.path.relpath(folder, root).replace("\\", "/")
                dest = "/Game/" + rel.replace("Art/Library/IA", "Art/IA").replace(" ", "_")
                asset_name = os.path.splitext(name)[0]
                if not reimport and unreal.EditorAssetLibrary.does_asset_exist(f"{dest}/{asset_name}"):
                    continue
                tasks.append(make_task(os.path.join(folder, name), dest))
    return tasks


def make_task(fbx_path, dest):
    ui = unreal.FbxImportUI()
    ui.set_editor_property("import_mesh", True)
    ui.set_editor_property("import_as_skeletal", False)
    ui.set_editor_property("import_materials", True)
    ui.set_editor_property("import_textures", False)
    ui.set_editor_property("import_animations", False)
    smd = ui.get_editor_property("static_mesh_import_data")
    smd.set_editor_property("combine_meshes", True)
    smd.set_editor_property("auto_generate_collision", False)
    smd.set_editor_property("vertex_color_import_option", unreal.VertexColorImportOption.REPLACE)
    smd.set_editor_property("normal_import_method", unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", fbx_path)
    task.set_editor_property("destination_path", dest)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", True)
    task.set_editor_property("options", ui)
    return task


def import_art():
    tasks = fbx_tasks()
    if tasks:
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
    imported = sum(len(t.get_editor_property("imported_object_paths") or []) for t in tasks)
    log(f"FBX importados: {len(tasks)} ficheros, {imported} assets")


def gather_meshes():
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    by_folder = defaultdict(list)
    for cls in ("StaticMesh", "SkeletalMesh"):
        flt = unreal.ARFilter(class_paths=[unreal.TopLevelAssetPath("/Script/Engine", cls)],
                              package_paths=["/Game"], recursive_paths=True)
        for data in registry.get_assets(flt):
            path = str(data.package_path)
            if any(token in path + "/" for token in EXCLUDED):
                continue
            by_folder[path].append(data)
    for folder in by_folder:
        by_folder[folder].sort(key=lambda d: str(d.asset_name))
    return dict(sorted(by_folder.items()))


def gather_blueprints():
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    flt = unreal.ARFilter(class_paths=[unreal.TopLevelAssetPath("/Script/Engine", "Blueprint")],
                          package_paths=["/Game"], recursive_paths=True)
    by_folder = defaultdict(list)
    for data in registry.get_assets(flt):
        path = str(data.package_path)
        native = str(data.get_tag_value("NativeParentClass") or "")
        if any(token in path + "/" for token in EXCLUDED) or any(token in native for token in BP_SKIP):
            continue
        if "Actor" not in native and "Character" not in native and "Pawn" not in native and "/Script/Tortunabo." not in native:
            continue
        by_folder[path].append(data)
    for folder in by_folder:
        by_folder[folder].sort(key=lambda d: str(d.asset_name))
    return dict(sorted(by_folder.items()))


def spawn_blueprint(actors, data, x, y):
    cls = unreal.EditorAssetLibrary.load_blueprint_class(str(data.package_name))
    if cls is None:
        return None, None
    actor = actors.spawn_actor_from_class(cls, unreal.Vector(0, 0, 0))
    if actor is None:
        return None, None
    origin, extent = actor.get_actor_bounds(False)
    size = extent * 2.0
    if size.x <= 0 or size.x > 50000:
        size = unreal.Vector(MIN_CELL_CM, MIN_CELL_CM, MIN_CELL_CM)
        origin, extent = unreal.Vector(0, 0, 0), size * 0.5
    actor.set_actor_location(unreal.Vector(x + extent.x - origin.x, y - origin.y, extent.z - origin.z), False, False)
    actor.set_actor_label(str(data.asset_name))
    return actor, size


def mesh_size(asset):
    if isinstance(asset, unreal.StaticMesh):
        box = asset.get_bounding_box()
        return box.max - box.min, box.min
    bounds = asset.get_bounds()
    ext, origin = bounds.box_extent, bounds.origin
    return ext * 2.0, origin - ext


def spawn_label(actors, text, location, height, color):
    label = actors.spawn_actor_from_class(unreal.TextRenderActor, location, unreal.Rotator(0, 0, 180))
    comp = label.get_editor_property("text_render")
    comp.set_editor_property("text", text)
    comp.set_editor_property("world_size", height)
    comp.set_editor_property("text_render_color", color)
    comp.set_editor_property("horizontal_alignment", unreal.HorizTextAligment.EHTA_CENTER)
    label.set_actor_label(f"Etiqueta_{text}")
    label.set_folder_path("Etiquetas")
    return label


def place_mesh(actors, asset, x, y):
    size, box_min = mesh_size(asset)
    loc = unreal.Vector(x - box_min.x, y - box_min.y - size.y * 0.5, -box_min.z)
    if isinstance(asset, unreal.StaticMesh):
        actor = actors.spawn_actor_from_class(unreal.StaticMeshActor, loc)
        actor.static_mesh_component.set_static_mesh(asset)
    else:
        actor = actors.spawn_actor_from_class(unreal.SkeletalMeshActor, loc)
        actor.skeletal_mesh_component.set_skinned_asset_and_update(asset)
    actor.set_actor_label(asset.get_name())
    return actor, size


def layout_blueprints(actors, by_folder, y):
    placed = 0
    for folder, items in by_folder.items():
        spawn_label(actors, "BP " + folder.replace("/Game/", ""), unreal.Vector(-600, y, 40), 120, unreal.Color(120, 200, 255, 255))
        x, row_depth = 0.0, 0.0
        for data in items:
            if x > MAX_ROW_CM * 0.8:
                y += row_depth + ROW_GAP_CM
                x, row_depth = 0.0, 0.0
            actor, size = spawn_blueprint(actors, data, x, y)
            if actor is None:
                log(f"No se pudo crear {data.package_name}")
                continue
            actor.set_folder_path(folder.replace("/Game/", "GaleriaBP/"))
            width = max(size.x, MIN_CELL_CM)
            spawn_label(actors, str(data.asset_name), unreal.Vector(x + width * 0.5, y - max(size.y, MIN_CELL_CM) * 0.5 - 60, 20), 28, unreal.Color(255, 255, 255, 255))
            x += width + GAP_CM
            row_depth = max(row_depth, size.y, MIN_CELL_CM)
            placed += 1
        y += row_depth + ROW_GAP_CM * 1.5
    return placed, y


def layout(actors, by_folder):
    placed = 0
    y = 0.0
    for folder, items in by_folder.items():
        spawn_label(actors, folder.replace("/Game/", ""), unreal.Vector(-600, y, 40), 120, unreal.Color(255, 220, 80, 255))
        x, row_depth = 0.0, 0.0
        for data in items:
            asset = data.get_asset()
            if asset is None:
                log(f"No se pudo cargar {data.package_name}")
                continue
            size, _ = mesh_size(asset)
            width = max(size.x, MIN_CELL_CM)
            if x > 0 and x + width > MAX_ROW_CM:
                y += row_depth + ROW_GAP_CM
                x, row_depth = 0.0, 0.0
            actor, _ = place_mesh(actors, asset, x, y)
            actor.set_folder_path(folder.replace("/Game/", "Galeria/"))
            spawn_label(actors, str(data.asset_name), unreal.Vector(x + width * 0.5, y - max(size.y, MIN_CELL_CM) * 0.5 - 60, 20), 28, unreal.Color(255, 255, 255, 255))
            x += width + GAP_CM
            row_depth = max(row_depth, size.y, MIN_CELL_CM)
            placed += 1
        y += row_depth + ROW_GAP_CM * 1.5
    return placed, y


def add_environment(actors, extent_y):
    floor = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(MAX_ROW_CM * 0.5, extent_y * 0.5, -2))
    floor.static_mesh_component.set_static_mesh(unreal.load_asset("/Engine/BasicShapes/Plane"))
    floor.set_actor_scale3d(unreal.Vector(MAX_ROW_CM / 100 + 40, extent_y / 100 + 40, 1))
    floor.set_actor_label("Suelo")
    sun = actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 3000), unreal.Rotator(0, -50, -40))
    sun.set_actor_label("Sol")
    actors.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 2000)).set_actor_label("Cielo")
    actors.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0)).set_actor_label("Atmosfera")
    camera = actors.spawn_actor_from_class(unreal.CameraActor, unreal.Vector(MAX_ROW_CM * 0.5, extent_y * 0.5, max(MAX_ROW_CM, extent_y) * 0.9),
                                           unreal.Rotator(0, -90, 0))
    camera.set_actor_label("CamaraCenital")


def main():
    import_art()
    by_folder = gather_meshes()
    total = sum(len(v) for v in by_folder.values())
    for folder, items in by_folder.items():
        log(f"{folder}: {len(items)}")
    log(f"Assets encontrados: {total} en {len(by_folder)} carpetas")

    if unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
        unreal.EditorAssetLibrary.delete_asset(MAP_PATH)
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if not levels.new_level(MAP_PATH):
        raise RuntimeError(f"No se pudo crear {MAP_PATH}")
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    placed, extent_y = layout(actors, by_folder)
    blueprints = gather_blueprints()
    bp_total = sum(len(v) for v in blueprints.values())
    bp_placed, extent_y = layout_blueprints(actors, blueprints, extent_y + ROW_GAP_CM * 2)
    log(f"Blueprints: {bp_placed} de {bp_total}")
    add_environment(actors, extent_y)
    levels.save_current_level()

    meshes_in_level = [a for a in actors.get_all_level_actors()
                       if isinstance(a, (unreal.StaticMeshActor, unreal.SkeletalMeshActor)) and a.get_actor_label() != "Suelo"
                       and str(a.get_folder_path()).startswith("Galeria/")]
    log(f"Colocados: {placed}; actores de malla en el mapa: {len(meshes_in_level)}")
    if bp_placed != bp_total:
        raise RuntimeError(f"Blueprints: {bp_total} encontrados y {bp_placed} colocados")
    if placed != total or len(meshes_in_level) != total:
        raise RuntimeError(f"Recuento distinto: {total} assets, {placed} colocados, {len(meshes_in_level)} en el mapa")
    log("OK")


main()
