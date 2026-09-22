"""Importa la libreria de modulos de terreno al proyecto como assets.

Se ejecuta DENTRO del editor de Unreal (consola Python, MCP o headless):
    UnrealEditor-Win64-DebugGame-Cmd.exe <uproject> -run=pythonscript
        -script=<repo>/Scripts/import_terrain_modules.py -EnablePlugins=PythonScriptPlugin
        -unattended -nosplash -nullrhi

Lee Scripts/terrain_modules/manifest.json (lo escribe gen_terrain_modules.py) y crea, por
cada modulo, en /Game/Terrain/Modules/<Topologia>/:
    DA_<nombre>   UTN_TerrainModuleAsset con el heightfield del PNG de 16 bits
    BP_<nombre>   Blueprint hijo de ATN_TerrainModuleTile con ese asset y el material
                  triplanar del terreno (M_GridTerrain)

Ademas deja BP_GridMapGenerator en modo modulos: ModuleClasses con todos los BP importados,
celda del tamano del manifest y grid de 6x6 con camino de 6 a 12 celdas.

Variables de entorno (opcionales), para probar una muestra sin tocar la libreria:
    TN_MODULES_DIR    carpeta con manifest.json y los PNG (por defecto Scripts/terrain_modules)
    TN_MODULES_ROOT   carpeta de destino en /Game (por defecto /Game/Terrain/Modules)

Idempotente: los assets que ya existen se actualizan, no se duplican. El PNG se decodifica
con la libreria estandar (zlib), porque el Python del editor no trae Pillow.
"""

import json
import os
import struct
import zlib

import unreal

MODULES_ROOT = os.environ.get("TN_MODULES_ROOT", "/Game/Terrain/Modules")
GRIDMAP_ROOT = "/Game/Blueprints/Gameplay/GridMap"
TERRAIN_MATERIAL_PATH = f"{GRIDMAP_ROOT}/M_GridTerrain"
GENERATOR_BP_PATH = f"{GRIDMAP_ROOT}/BP_GridMapGenerator"
TILE_CLASS_PATH = "/Script/Tortunabo.TN_TerrainModuleTile"

GENERATOR_GRID_SIZE = 6
GENERATOR_MIN_PATH = 6
GENERATOR_MAX_PATH = 12

TOPOLOGY_ENUM = {
    "Straight": unreal.TNTerrainModuleTopology.STRAIGHT,
    "CurveLeft": unreal.TNTerrainModuleTopology.CURVE_LEFT,
    "CurveRight": unreal.TNTerrainModuleTopology.CURVE_RIGHT,
    "TLeft": unreal.TNTerrainModuleTopology.T_LEFT,
    "TRight": unreal.TNTerrainModuleTopology.T_RIGHT,
    "Cross": unreal.TNTerrainModuleTopology.CROSS,
}

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
asset_lib = unreal.EditorAssetLibrary


def project_dir():
    return unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())


def load_or_none(path):
    return asset_lib.load_asset(path) if asset_lib.does_asset_exist(path) else None


# ── PNG de 16 bits en escala de grises, sin entrelazado ──────────────────────────
def read_png16(path):
    """Devuelve (ancho, alto, lista de enteros fila a fila)."""
    with open(path, "rb") as handle:
        data = handle.read()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError(f"{path}: no es un PNG")

    pos = 8
    width = height = 0
    idat = b""
    while pos < len(data):
        length, kind = struct.unpack(">I4s", data[pos:pos + 8])
        chunk = data[pos + 8:pos + 8 + length]
        pos += 12 + length
        if kind == b"IHDR":
            width, height, depth, color, _, _, interlace = struct.unpack(">IIBBBBB", chunk)
            if depth != 16 or color != 0 or interlace != 0:
                raise ValueError(f"{path}: se esperaba gris de 16 bits sin entrelazar")
        elif kind == b"IDAT":
            idat += chunk
        elif kind == b"IEND":
            break

    raw = zlib.decompress(idat)
    stride = width * 2
    previous = bytearray(stride)
    values = []
    offset = 0
    for _ in range(height):
        filter_type = raw[offset]
        line = bytearray(raw[offset + 1:offset + 1 + stride])
        offset += 1 + stride
        unfilter(line, previous, filter_type, bytes_per_pixel=2)
        values.extend(struct.unpack(f">{width}H", bytes(line)))
        previous = line
    return width, height, values


def unfilter(line, previous, filter_type, bytes_per_pixel):
    """Deshace el filtro PNG de una fila in situ (tipos 0-4)."""
    if filter_type == 0:
        return
    for i in range(len(line)):
        left = line[i - bytes_per_pixel] if i >= bytes_per_pixel else 0
        up = previous[i]
        up_left = previous[i - bytes_per_pixel] if i >= bytes_per_pixel else 0
        if filter_type == 1:
            line[i] = (line[i] + left) & 0xFF
        elif filter_type == 2:
            line[i] = (line[i] + up) & 0xFF
        elif filter_type == 3:
            line[i] = (line[i] + ((left + up) >> 1)) & 0xFF
        elif filter_type == 4:
            p = left + up - up_left
            pa, pb, pc = abs(p - left), abs(p - up), abs(p - up_left)
            predictor = left if pa <= pb and pa <= pc else (up if pb <= pc else up_left)
            line[i] = (line[i] + predictor) & 0xFF
        else:
            raise ValueError(f"filtro PNG desconocido: {filter_type}")


# ── Assets ────────────────────────────────────────────────────────────────────────
def make_bridge(data):
    """Puente del manifest (metros) a FTNTerrainModuleBridge (uu)."""
    bridge = unreal.TNTerrainModuleBridge()
    bridge.set_editor_property("center", unreal.Vector2D(data["x_m"] * 100.0, data["y_m"] * 100.0))
    bridge.set_editor_property("yaw", float(data["yaw_deg"]))
    bridge.set_editor_property("length", data["length_m"] * 100.0)
    bridge.set_editor_property("width", data["width_m"] * 100.0)
    bridge.set_editor_property("thickness", data["thickness_m"] * 100.0)
    bridge.set_editor_property("deck_height", data["deck_m"] * 100.0)
    return bridge


def make_flat_area(data):
    """Plaza del manifest (metros) a FTNTerrainModuleFlatArea (uu)."""
    area = unreal.TNTerrainModuleFlatArea()
    area.set_editor_property("center", unreal.Vector2D(data["x_m"] * 100.0, data["y_m"] * 100.0))
    area.set_editor_property("radius", data["radius_m"] * 100.0)
    area.set_editor_property("height", data["height_m"] * 100.0)
    return area


def build_module_asset(folder, module, manifest, heights):
    name = f"DA_{module['name']}"
    path = f"{folder}/{name}"
    asset = load_or_none(path)
    if not asset:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.TN_TerrainModuleAsset)
        asset = asset_tools.create_asset(name, folder, unreal.TN_TerrainModuleAsset, factory)

    asset.set_editor_property("topology", TOPOLOGY_ENUM[module["topology"]])
    asset.set_editor_property("seed", int(module["seed"]))
    if not asset.set_heightfield(int(manifest["resolution"]), float(manifest["height_scale_uu"]),
                                 int(manifest["height_zero"]), heights):
        raise RuntimeError(f"{name}: SetHeightfield rechazo el heightfield")
    asset.set_editor_property("bridges", [make_bridge(b) for b in module.get("bridges", [])])
    asset.set_editor_property("flat_areas", [make_flat_area(a) for a in module.get("flat_areas", [])])
    asset_lib.save_loaded_asset(asset)
    return asset


def build_module_blueprint(folder, module, asset, material, module_size):
    name = f"BP_{module['name']}"
    path = f"{folder}/{name}"
    blueprint = load_or_none(path)
    if not blueprint:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", unreal.load_class(None, TILE_CLASS_PATH))
        blueprint = asset_tools.create_asset(name, folder, unreal.Blueprint, factory)
        unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)

    defaults = unreal.get_default_object(blueprint.generated_class())
    defaults.set_editor_property("module_asset", asset)
    defaults.set_editor_property("module_size", module_size)
    if material:
        defaults.set_editor_property("terrain_material", material)
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    asset_lib.save_loaded_asset(blueprint)
    return blueprint


def configure_generator(module_blueprints, module_size):
    blueprint = load_or_none(GENERATOR_BP_PATH)
    if not blueprint:
        unreal.log_warning(f"{GENERATOR_BP_PATH} no existe: el generador no se configura")
        return
    defaults = unreal.get_default_object(blueprint.generated_class())
    defaults.set_editor_property("module_classes", [bp.generated_class() for bp in module_blueprints])
    defaults.set_editor_property("cell_size", module_size)
    defaults.set_editor_property("grid_size", GENERATOR_GRID_SIZE)
    defaults.set_editor_property("min_path_length", GENERATOR_MIN_PATH)
    defaults.set_editor_property("max_path_length", GENERATOR_MAX_PATH)
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    asset_lib.save_loaded_asset(blueprint)


def main():
    library_dir = os.environ.get("TN_MODULES_DIR", f"{project_dir()}/Scripts/terrain_modules")
    with open(f"{library_dir}/manifest.json", encoding="utf-8") as handle:
        manifest = json.load(handle)

    module_size = float(manifest["size_uu"])
    resolution = int(manifest["resolution"])
    material = load_or_none(TERRAIN_MATERIAL_PATH)
    if not material:
        unreal.log_warning(f"{TERRAIN_MATERIAL_PATH} no existe: los modulos quedan sin material")

    blueprints = []
    with unreal.ScopedSlowTask(len(manifest["modules"]), "Importando modulos de terreno") as task:
        task.make_dialog(True)
        for module in manifest["modules"]:
            if task.should_cancel():
                break
            task.enter_progress_frame(1, module["name"])
            width, height, heights = read_png16(f"{library_dir}/{module['file']}")
            if width != resolution or height != resolution:
                raise ValueError(f"{module['name']}: {width}x{height}, se esperaba {resolution}")
            folder = f"{MODULES_ROOT}/{module['topology']}"
            asset = build_module_asset(folder, module, manifest, heights)
            blueprints.append(build_module_blueprint(folder, module, asset, material, module_size))

    configure_generator(blueprints, module_size)
    unreal.log(f"[TerrainModules] {len(blueprints)} modulos importados en {MODULES_ROOT}")


main()
