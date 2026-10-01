"""Catálogos de arte: crea DA_Arte_Lobby y DA_Arte_ProcMap en /Game/Art y mete todas las piezas vacías.

Docs/Arte_Assets.md. La lista de piezas sale de la tabla del C++ (Source/Tortunabo/Private/Art/TN_ArtSlots_*.inl), la
misma que usan TN.Art.Slots y los tests, así que no hace falta abrir el juego para conocerlas.

Dentro del editor (Herramientas > Ejecutar script de Python, o en la consola de Python):
    exec(open(r"<repo>/Scripts/arte/rellenar_catalogos.py", encoding="utf-8").read())

Sin interfaz, con el editor cerrado:
    UnrealEditor-Cmd.exe "<repo>/Tortunabo.uproject" -run=pythonscript -script="<repo>/Scripts/arte/rellenar_catalogos.py"

Fuera de Unreal escribe la lista de piezas en Markdown (la de Docs/Arte_Assets.md):
    uv run python Scripts/arte/rellenar_catalogos.py --markdown

Idempotente: crea lo que falta y nunca toca la malla, los materiales, el ajuste ni la colisión que Arte ya haya puesto. Solo
reescribe el texto «Info» de cada pieza y avisa de las que están en un catálogo pero ya no en el código.
"""

from __future__ import annotations

import os
import re
import sys
from collections import OrderedDict

# Zona del nombre de la pieza → data asset que la guarda (los dos mapas procedurales, el cooperativo y la carrera de la
# playa, comparten el suyo).
CATALOGS = OrderedDict([
    ("Lobby", "/Game/Art/DA_Arte_Lobby"),
    ("ProcMap", "/Game/Art/DA_Arte_ProcMap"),
    ("Beach", "/Game/Art/DA_Arte_ProcMap"),
])

# Una pieza por TN_ART_SLOT( a principio de línea (los comentarios de la cabecera no cuentan).
SLOT_PATTERN = re.compile(
    r'(?m)^TN_ART_SLOT\(\s*"(?P<name>[^"]*)"\s*,\s*"(?P<kind>[^"]*)"\s*,\s*"(?P<source>[^"]*)"\s*,\s*"(?P<what>[^"]*)"\s*,'
    r'\s*"(?P<size>[^"]*)"\s*,\s*"(?P<pivot>[^"]*)"\s*\)')


def repo_root() -> str:
    """Raíz del repositorio: la carpeta del .uproject (desde Unreal o desde este fichero)."""
    try:
        import unreal  # noqa: F401
        return os.path.normpath(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
    except ImportError:
        return os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))


def read_slots(root: str | None = None) -> list[dict]:
    """Piezas de la tabla del C++, en orden (Lobby, ProcMap, Beach)."""
    art_dir = os.path.join(root or repo_root(), "Source", "Tortunabo", "Private", "Art")
    slots = []
    for zone in ("Lobby", "ProcMap", "Beach"):
        path = os.path.join(art_dir, f"TN_ArtSlots_{zone}.inl")
        if not os.path.isfile(path):
            continue
        with open(path, encoding="utf-8") as f:
            text = f.read()
        for match in SLOT_PATTERN.finditer(text):
            slots.append(match.groupdict())
    return slots


def info_text(slot: dict) -> str:
    """Texto «Info» de la entrada del catálogo."""
    return (f"{slot['what']}.\nTipo: {slot['kind']}. Tamaño: {slot['size']}.\nPivote: {slot['pivot']}.\n"
            f"Sale de Source/Tortunabo/Private/{slot['source']}.")


def group_of(name: str) -> str:
    """«Lobby.Castle» de «Lobby.Castle.Tower»."""
    parts = name.split(".")
    return ".".join(parts[:2])


def markdown(slots: list[dict]) -> str:
    """Tablas por zona y grupo para Docs/Arte_Assets.md."""
    lines = []
    zone_titles = {"Lobby": "Lobby (DA_Arte_Lobby)", "ProcMap": "Mapa procedural (DA_Arte_ProcMap)",
                   "Beach": "Carrera de la playa (también en DA_Arte_ProcMap)"}
    current_zone = None
    current_group = None
    for slot in slots:
        zone = slot["name"].split(".")[0]
        if zone != current_zone:
            current_zone = zone
            count = sum(1 for s in slots if s["name"].startswith(zone + "."))
            lines += ["", f"### {zone_titles.get(zone, zone)} — {count} piezas"]
            current_group = None
        group = group_of(slot["name"])
        if group != current_group:
            current_group = group
            lines += ["", f"#### {group}", "",
                      "| Pieza | Tipo | Qué es | Tamaño (cm) | Pivote | Fichero |",
                      "|---|---|---|---|---|---|"]
        lines.append(f"| `{slot['name']}` | {slot['kind']} | {slot['what']} | {slot['size']} | {slot['pivot']} | "
                     f"`{slot['source']}` |")
    return "\n".join(lines).lstrip("\n") + "\n"


def fill_catalogs() -> None:
    """Dentro de Unreal: crea los catálogos que falten y añade las piezas que falten, vacías."""
    import unreal

    slots = read_slots()
    if not slots:
        unreal.log_error("[Arte] No encuentro la tabla de piezas (Source/Tortunabo/Private/Art/TN_ArtSlots_*.inl).")
        return
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    by_path = OrderedDict((path, []) for path in CATALOGS.values())
    for slot in slots:
        by_path[CATALOGS[slot["name"].split(".")[0]]].append(slot)

    for path, path_slots in by_path.items():
        folder, name = path.rsplit("/", 1)
        catalog = unreal.EditorAssetLibrary.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
        if catalog is None:
            factory = unreal.DataAssetFactory()
            factory.set_editor_property("data_asset_class", unreal.TN_ArtCatalog)
            catalog = asset_tools.create_asset(name, folder, unreal.TN_ArtCatalog, factory)
            if catalog is None:
                unreal.log_error(f"[Arte] No se ha podido crear {path}.")
                continue
            unreal.log(f"[Arte] Creado {path}.")
        pieces = catalog.get_editor_property("pieces")
        known = {str(key) for key in pieces.keys()}
        added = 0
        for slot in path_slots:
            key = slot["name"]
            if key in known:
                entry = pieces[key]
            else:
                entry = unreal.TNArtOverride()
                added += 1
            entry.set_editor_property("info", info_text(slot))
            pieces[key] = entry
        wanted = {slot["name"] for slot in path_slots}
        for stale in sorted(known - wanted):
            unreal.log_warning(f"[Arte] {path}: la pieza {stale} ya no la genera el código (se deja, sin efecto).")
        catalog.set_editor_property("pieces", pieces)
        unreal.EditorAssetLibrary.save_loaded_asset(catalog, only_if_is_dirty=False)
        unreal.log(f"[Arte] {path}: {len(wanted)} piezas ({added} nuevas).")


def main(argv: list[str]) -> int:
    if "--markdown" in argv:
        sys.stdout.reconfigure(encoding="utf-8")
        sys.stdout.write(markdown(read_slots()))
        return 0
    try:
        import unreal  # noqa: F401
    except ImportError:
        print("Fuera de Unreal solo vale --markdown (ver la cabecera del script).")
        return 1
    fill_catalogs()
    return 0


if __name__ == "__main__" or "unreal" in sys.modules:
    _result = main(sys.argv[1:])
    if __name__ == "__main__" and "unreal" not in sys.modules:
        sys.exit(_result)
