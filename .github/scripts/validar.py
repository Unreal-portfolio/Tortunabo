"""Comprobaciones rápidas antes de entrar en main (.github/workflows/validar-main.yml, Docs/Flujo_Git.md).

Se ejecuta en los servidores de GitHub sin Unreal: revisa lo que se puede revisar sin compilar. También se puede lanzar en
local desde la raíz del repositorio:
    python .github/scripts/validar.py --base origin/main

Errores (paran la fusión):
  - archivos de Binaries/, Intermediate/, Saved/, DerivedDataCache/ o .vs/ en los cambios;
  - archivos de más de 50 MB en los cambios (GitHub rechaza los de más de 100 MB);
  - marcadores de conflicto (<<<<<<<, >>>>>>>) en archivos de texto;
  - código fuente (.h, .cpp, .cs) que no está en UTF-8;
  - scripts de Python con errores de sintaxis;
  - JSON mal formado (Tortunabo.uproject, manifiestos, .json de Scripts y Tools);
  - la misma clave de localización (NSLOCTEXT) con dos textos distintos;
  - traducciones (.po) con marcadores, plurales o saltos de línea rotos (Tools/Localization/po_tool.py check).
Avisos (no paran): archivos de más de 10 MB.
"""
import argparse
import json
import os
import re
import subprocess
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
FORBIDDEN_PREFIXES = ("Binaries/", "Intermediate/", "Saved/", "DerivedDataCache/", ".vs/")
FORBIDDEN_SUFFIXES = (".sln", ".VC.db", ".suo")
ERROR_MB, WARN_MB = 50, 10
TEXT_EXT = (".h", ".hpp", ".cpp", ".cs", ".ini", ".py", ".md", ".json", ".po", ".txt", ".uproject", ".uplugin", ".yml",
            ".yaml", ".bat", ".sh", ".csv")
SOURCE_EXT = (".h", ".hpp", ".cpp", ".cs")
CONFLICT = re.compile(r"^(<<<<<<< |>>>>>>> )", re.M)
NSLOCTEXT = re.compile(r'NSLOCTEXT\s*\(\s*"((?:[^"\\]|\\.)*)"\s*,\s*"((?:[^"\\]|\\.)*)"\s*,\s*((?:"(?:[^"\\]|\\.)*"\s*)+)\)', re.S)
LOCTEXT_NS = re.compile(r'#define\s+LOCTEXT_NAMESPACE\s+"([^"]*)"')
LOCTEXT = re.compile(r'(?<![A-Z_])LOCTEXT\s*\(\s*"((?:[^"\\]|\\.)*)"\s*,\s*((?:"(?:[^"\\]|\\.)*"\s*)+)\)', re.S)

errors, warnings = [], []


def gh(kind, msg, path=None, line=None):
    loc = ""
    if path:
        loc = f" file={path}" + (f",line={line}" if line else "")
    print(f"::{kind}{loc}::{msg}")


def error(msg, path=None, line=None):
    errors.append((msg, path))
    gh("error", msg, path, line)


def warn(msg, path=None, line=None):
    warnings.append((msg, path))
    gh("warning", msg, path, line)


def git(*args):
    return subprocess.run(["git", *args], cwd=ROOT, capture_output=True, text=True, encoding="utf-8", errors="replace")


def tracked(*patterns):
    out = git("ls-files", "--", *patterns).stdout.splitlines()
    return [p for p in out if os.path.isfile(os.path.join(ROOT, p))]


def read_text(path):
    with open(os.path.join(ROOT, path), "rb") as f:
        return f.read()


def changed_files(base):
    """(estado, ruta, blob) de lo que cambia entre base y HEAD; lista vacía si no hay base utilizable."""
    if not base or set(base) == {"0"} or git("cat-file", "-e", f"{base}^{{commit}}").returncode != 0:
        head_parent = git("rev-parse", "--verify", "HEAD~1")
        if head_parent.returncode != 0:
            print("Sin base con la que comparar: se omiten las comprobaciones de los cambios.")
            return []
        base = head_parent.stdout.strip()
    raw = git("diff", "--raw", "--no-renames", "-z", base, "HEAD")
    if raw.returncode != 0:
        warn(f"No se pudo comparar con {base}: {raw.stderr.strip()}")
        return []
    items, parts = [], raw.stdout.split("\0")
    i = 0
    while i < len(parts) - 1:
        meta = parts[i].split()
        if len(meta) >= 5:
            status, new_blob, path = meta[4], meta[3], parts[i + 1]
            items.append((status, path, new_blob))
        i += 2
    return items


def check_changes(base):
    changes = changed_files(base)
    print(f"Cambios revisados: {len(changes)} archivos.")
    for status, path, blob in changes:
        if status.startswith("D"):
            continue
        if path.startswith(FORBIDDEN_PREFIXES) or path.endswith(FORBIDDEN_SUFFIXES):
            error(f"No se suben archivos generados o locales: {path}", path)
            continue
        size = git("cat-file", "-s", blob)
        if size.returncode == 0:
            mb = int(size.stdout.strip()) / (1024 * 1024)
            if mb > ERROR_MB:
                error(f"Archivo de {mb:.1f} MB (el máximo es {ERROR_MB} MB): {path}", path)
            elif mb > WARN_MB:
                warn(f"Archivo grande ({mb:.1f} MB): {path}", path)


def check_text_files():
    files = [p for p in tracked("Source", "Config", "Docs", "Scripts", "Tools", ".github", "Content/Localization")
             if p.endswith(TEXT_EXT) and not p.startswith("Plugins/")]
    for path in files:
        data = read_text(path)
        if path.endswith(SOURCE_EXT):
            try:
                data.decode("utf-8-sig")
            except UnicodeDecodeError as exc:
                error(f"No está en UTF-8 ({exc.reason} en el byte {exc.start})", path)
                continue
        text = data.decode("utf-8-sig", errors="replace")
        if path.endswith(".md") or path == ".github/scripts/validar.py":
            continue  # los documentos pueden citar marcadores de conflicto como ejemplo
        m = CONFLICT.search(text)
        if m:
            error("Marcador de conflicto sin resolver", path, text.count("\n", 0, m.start()) + 1)
    print(f"Archivos de texto revisados: {len(files)}.")


def check_python():
    files = [p for p in tracked("*.py") if not p.startswith("Plugins/")]
    for path in files:
        try:
            compile(read_text(path).decode("utf-8-sig", errors="replace"), path, "exec")
        except SyntaxError as exc:
            error(f"Error de sintaxis de Python: {exc.msg}", path, exc.lineno)
    print(f"Scripts de Python revisados: {len(files)}.")


def check_json():
    files = [p for p in tracked("*.json", "*.uproject", "*.uplugin", "Content/Localization/*.manifest",
                                "Content/Localization/*.archive") if not p.startswith("Plugins/")]
    for path in files:
        data = read_text(path)
        try:
            # Los .archive y .manifest de Unreal están en UTF-16 con BOM; el resto, en UTF-8.
            utf16 = data.startswith(bytes([0xFF, 0xFE])) or data.startswith(bytes([0xFE, 0xFF]))
            json.loads(data.decode("utf-16") if utf16 else data.decode("utf-8-sig"))
        except (ValueError, UnicodeDecodeError) as exc:
            error(f"JSON mal formado: {exc}", path)
    print(f"Archivos JSON revisados: {len(files)}.")


def unquote(chunks):
    return "".join(re.findall(r'"((?:[^"\\]|\\.)*)"', chunks))


def check_loc_keys():
    seen = {}
    for path in tracked("Source"):
        if not path.endswith((".h", ".cpp")):
            continue
        text = read_text(path).decode("utf-8-sig", errors="replace")
        entries = [(m.group(1), m.group(2), unquote(m.group(3)), m.start()) for m in NSLOCTEXT.finditer(text)]
        ns = LOCTEXT_NS.search(text)
        if ns:
            entries += [(ns.group(1), m.group(1), unquote(m.group(2)), m.start()) for m in LOCTEXT.finditer(text)]
        for namespace, key, source, pos in entries:
            line = text.count("\n", 0, pos) + 1
            prev = seen.get((namespace, key))
            if prev is None:
                seen[(namespace, key)] = (source, path, line)
            elif prev[0] != source:
                error(f"La clave de localización «{namespace},{key}» tiene dos textos distintos (también en {prev[1]}:{prev[2]})",
                      path, line)
    print(f"Claves de localización revisadas: {len(seen)}.")


def check_po():
    tool = os.path.join(ROOT, "Tools", "Localization", "po_tool.py")
    loc_dir = os.path.join(ROOT, "Content", "Localization", "Game")
    if not (os.path.isfile(tool) and os.path.isdir(loc_dir)):
        print("Sin traducciones que revisar.")
        return
    for culture in sorted(os.listdir(loc_dir)):
        if not os.path.isfile(os.path.join(loc_dir, culture, "Game.po")):
            continue
        run = subprocess.run([sys.executable, tool, "check", culture], cwd=ROOT, capture_output=True, text=True,
                             encoding="utf-8", errors="replace", env={**os.environ, "PYTHONIOENCODING": "utf-8"})
        summary = run.stdout.strip().splitlines()[-1] if run.stdout.strip() else run.stderr.strip()
        print(f"  {summary}")
        if run.returncode != 0:
            error(f"Traducción «{culture}» con avisos: {summary} (python Tools/Localization/po_tool.py check {culture})",
                  f"Content/Localization/Game/{culture}/Game.po")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--base", default="", help="commit con el que comparar (la base de la pull request)")
    args = parser.parse_args()

    print("== Cambios"); check_changes(args.base)
    print("== Archivos de texto"); check_text_files()
    print("== Python"); check_python()
    print("== JSON"); check_json()
    print("== Claves de localización"); check_loc_keys()
    print("== Traducciones"); check_po()

    summary = os.environ.get("GITHUB_STEP_SUMMARY")
    lines = [f"### Comprobaciones: {'bien' if not errors else f'{len(errors)} errores'}, {len(warnings)} avisos", ""]
    lines += [f"- ❌ {m}" for m, _ in errors] + [f"- ⚠️ {m}" for m, _ in warnings]
    if summary:
        with open(summary, "a", encoding="utf-8") as f:
            f.write("\n".join(lines) + "\n")
    print("\n".join(lines))
    return 1 if errors else 0


if __name__ == "__main__":
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    sys.exit(main())
