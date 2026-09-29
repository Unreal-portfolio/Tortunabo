"""Herramienta de los .po del objetivo «Game» (Docs/Localizacion.md): sacar lo pendiente, aplicar traducciones y comprobarlas.

Uso (desde la raíz del repo):
    python Tools/Localization/po_tool.py todo <cultura> [salida.json]    # textos sin traducir (o cuyo origen cambió)
    python Tools/Localization/po_tool.py apply <cultura> <traducciones.json>
    python Tools/Localization/po_tool.py check <cultura>                  # marcadores, plurales, saltos de línea, largos
    python Tools/Localization/po_tool.py seed-en                          # nombres de sala ingleses desde room_names_en.csv
    python Tools/Localization/po_tool.py stats                            # traducidos por cultura

El JSON de «todo» es una lista de {"ctx": "Espacio,Clave", "src": texto origen en español, "where": dónde sale}; el de
«apply» es un objeto {"Espacio,Clave": "traducción"} (texto normal, sin escapar: los \\n se escriben como salto real).
Solo toca msgstr; el resto del .po queda igual (lo reescribe el motor en cada exportación).
"""
import csv
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
PO_DIR = os.path.join(ROOT, "Content", "Localization", "Game")
PLACEHOLDER = re.compile(r"\{[A-Za-z_][A-Za-z0-9_]*\}|\{[0-9]+\}")
PLURAL = re.compile(r"\|plural\(|\|gender\(")  # |ordinal( se puede añadir en el destino (1st, 2nd; 1er…)


def po_path(culture):
    return os.path.join(PO_DIR, culture, "Game.po")


def unescape(s):
    out, i = [], 0
    while i < len(s):
        c = s[i]
        if c == "\\" and i + 1 < len(s):
            n = s[i + 1]
            out.append({"n": "\n", "t": "\t", "r": "\r", '"': '"', "\\": "\\"}.get(n, "\\" + n))
            i += 2
        else:
            out.append(c)
            i += 1
    return "".join(out)


def escape(s):
    return s.replace("\\", "\\\\").replace('"', '\\"').replace("\r", "\\r").replace("\n", "\\n").replace("\t", "\\t")


class Entry:
    def __init__(self):
        self.lines = []      # líneas originales del bloque
        self.ctx = None
        self.msgid = ""
        self.msgstr = ""
        self.where = ""
        self.msgstr_span = None  # (inicio, fin) de las líneas de msgstr en self.lines


def parse(path):
    with open(path, "r", encoding="utf-8-sig") as f:
        text = f.read()
    blocks = re.split(r"\n\s*\n", text.replace("\r\n", "\n").strip("\n"))
    entries = []
    for block in blocks:
        e = Entry()
        e.lines = block.split("\n")
        field = None
        for idx, line in enumerate(e.lines):
            if line.startswith("#. SourceLocation:"):
                e.where = line.split(":", 1)[1].strip()
            m = re.match(r'^(msgctxt|msgid|msgstr) "(.*)"$', line)
            if m:
                field = m.group(1)
                val = unescape(m.group(2))
                if field == "msgctxt":
                    e.ctx = val
                elif field == "msgid":
                    e.msgid = val
                else:
                    e.msgstr = val
                    e.msgstr_span = (idx, idx + 1)
                continue
            m = re.match(r'^"(.*)"$', line)
            if m and field:
                val = unescape(m.group(1))
                if field == "msgctxt":
                    e.ctx += val
                elif field == "msgid":
                    e.msgid += val
                else:
                    e.msgstr += val
                    e.msgstr_span = (e.msgstr_span[0], idx + 1)
        entries.append(e)
    return entries


def write(path, entries):
    parts = []
    for e in entries:
        parts.append("\n".join(e.lines))
    with open(path, "w", encoding="utf-8-sig", newline="\n") as f:
        f.write("\n\n".join(parts) + "\n")


def is_message(e):
    return e.msgid != "" or e.ctx is not None


def cmd_todo(culture, out=None):
    items = [{"ctx": e.ctx, "src": e.msgid, "where": e.where} for e in parse(po_path(culture))
             if is_message(e) and e.msgid and not e.msgstr]
    data = json.dumps(items, ensure_ascii=False, indent=1)
    if out:
        with open(out, "w", encoding="utf-8") as f:
            f.write(data)
        print(f"{len(items)} pendientes en {culture} -> {out}")
    else:
        print(data)


def cmd_apply(culture, src_json):
    with open(src_json, "r", encoding="utf-8") as f:
        tr = json.load(f)
    path = po_path(culture)
    entries = parse(path)
    applied, unknown = 0, set(tr)
    for e in entries:
        if e.ctx in tr and e.msgstr_span:
            value = tr[e.ctx]
            unknown.discard(e.ctx)
            a, b = e.msgstr_span
            e.lines[a:b] = [f'msgstr "{escape(value)}"']
            e.msgstr_span = (a, a + 1)
            e.msgstr = value
            applied += 1
    write(path, entries)
    print(f"{culture}: {applied} aplicadas" + (f"; claves desconocidas: {len(unknown)}" if unknown else ""))
    for k in sorted(unknown)[:20]:
        print("  ?", k)


# Idiomas con una sola forma plural: el motor rechaza «|plural(...)» por redundante al compilar; se escribe la palabra sin él.
SINGLE_PLURAL = {"ja", "ko", "zh-Hans", "zh-Hant"}


def problems(e, culture=""):
    out = []
    src, dst = e.msgid, e.msgstr
    if set(PLACEHOLDER.findall(src)) != set(PLACEHOLDER.findall(dst)):
        out.append(f"marcadores {sorted(set(PLACEHOLDER.findall(src)))} != {sorted(set(PLACEHOLDER.findall(dst)))}")
    if culture in SINGLE_PLURAL:
        if "|plural(" in dst:
            out.append("«|plural(» sobra en un idioma de una sola forma plural (escribe la palabra sin él)")
    elif len(PLURAL.findall(src)) != len(PLURAL.findall(dst)):
        out.append("plurales/género distintos del origen")
    if src.count("\n") != dst.count("\n"):
        out.append(f"saltos de línea {src.count(chr(10))} != {dst.count(chr(10))}")
    if dst.count("{") != dst.count("}") or dst.count("(") != dst.count(")"):
        out.append("llaves o paréntesis desparejados")
    if dst != dst.strip() and src == src.strip():
        out.append("espacios al principio o al final")
    if len(src) >= 12 and len(dst) > 1.7 * len(src) + 8:
        out.append(f"largo {len(dst)} frente a {len(src)} del origen")
    return out


def cmd_check(culture):
    bad = 0
    total = done = 0
    for e in parse(po_path(culture)):
        if not is_message(e) or not e.msgid:
            continue
        total += 1
        if not e.msgstr:
            continue
        done += 1
        for p in problems(e, culture):
            bad += 1
            print(f"{e.ctx}: {p}\n    es: {e.msgid!r}\n    {culture}: {e.msgstr!r}")
    print(f"{culture}: {done}/{total} traducidos, {bad} avisos")
    return bad


def cmd_seed_en():
    csv_path = os.path.join(ROOT, "Tools", "Localization", "room_names_en.csv")
    tr = {}
    with open(csv_path, "r", encoding="utf-8-sig", newline="") as f:
        for row in csv.DictReader(f):
            tr[f"{row['Namespace']},{row['Key']}"] = row[[k for k in row if k.startswith("English")][0]]
    tmp = os.path.join(PO_DIR, "_seed_en.json")
    with open(tmp, "w", encoding="utf-8") as f:
        json.dump(tr, f, ensure_ascii=False)
    cmd_apply("en", tmp)
    os.remove(tmp)


def cmd_stats():
    for culture in sorted(os.listdir(PO_DIR)):
        if os.path.exists(po_path(culture)):
            es = [e for e in parse(po_path(culture)) if is_message(e) and e.msgid]
            print(f"{culture:8} {sum(1 for e in es if e.msgstr):5}/{len(es)}")


if __name__ == "__main__":
    # La consola de Windows puede estar en cp1252: los avisos llevan japonés, chino, ruso...
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    a = sys.argv[1:]
    if not a:
        print(__doc__)
    elif a[0] == "todo":
        cmd_todo(a[1], a[2] if len(a) > 2 else None)
    elif a[0] == "apply":
        cmd_apply(a[1], a[2])
    elif a[0] == "check":
        sys.exit(1 if cmd_check(a[1]) else 0)
    elif a[0] == "seed-en":
        cmd_seed_en()
    elif a[0] == "stats":
        cmd_stats()
    else:
        print(__doc__)
