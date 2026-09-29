"""Comprueba que las fuentes del juego cubren los caracteres de cada idioma (Docs/Localizacion.md, «Fuentes»).

La interfaz usa la fuente compuesta del motor (Roboto + Droid Sans Fallback como reserva) mas, si existen en
Content/Slate/Fonts, las fuentes de reserva de cada idioma de Config/DefaultGame.ini (FontRegular / FontBold). Para cada idioma
mira que cada caracter que se va a escribir este en alguna de esas fuentes y avisa de los que solo salen de la reserva del motor
(se ven, pero con peor calidad: una sola grosor, sin variantes regionales).

Que caracteres mira, por idioma:
  * un texto de muestra del alfabeto (abajo) y el nombre del idioma escrito en su idioma;
  * si existe Content/Localization/Game/<cultura>/Game.po, todos los caracteres de sus traducciones (msgstr), que es lo que de
    verdad se va a ver. Se puede pasar --po con otra carpeta.

Uso (necesita fontTools: pip install fonttools):
    python Scripts/tools/check_font_coverage.py
    python Scripts/tools/check_font_coverage.py --engine "C:/Program Files/Epic Games/UE_5.6" --fonts Content/Slate/Fonts

No modifica nada ni descarga nada. Sale con codigo 1 si algun idioma tiene caracteres que ninguna fuente cubre.
"""

import argparse
import re
import sys
from pathlib import Path

try:
    from fontTools.ttLib import TTFont
except ImportError:
    print("Falta fontTools: pip install fonttools")
    sys.exit(2)

PROJECT = Path(__file__).resolve().parents[2]

SAMPLES = {
    "es-ES": "ÁÉÍÓÚÜÑáéíóúüñ¿¡«»—…€",
    "en": "The quick brown fox jumps over the lazy dog’“”",
    "fr": "àâæçéèêëîïôœùûüÿÀÂÆÇÉÈÊËÎÏÔŒÙÛÜŸ«»’",
    "de": "äöüßÄÖÜ„“",  # la ẞ mayuscula (U+1E9E) no esta en Roboto ni en la reserva: se evita en los textos (ToUpper deja la ß)
    "it": "àèéìíîòóùúÀÈÉÌÒÙ",
    "pt-BR": "ãõáâàçéêíóôúüÃÕÁÂÀÇÉÊÍÓÔÚ",
    "ru": "АБВГДЕЁЖЗИЙКЛМНОПРСТУФХЦЧШЩЪЫЬЭЮЯабвгдеёжзийклмнопрстуфхцчшщъыьэюя",
    "pl": "ąćęłńóśźżĄĆĘŁŃÓŚŹŻ",
    "tr": "çğıİöşüÇĞÖŞÜâîû",
    "ja": "日本語ひらがなカタカナ、。「」ー！？",
    "ko": "한국어조선말가나다라마바사아자차카타파하",
    "zh-Hans": "简体中文汉语龟壳游戏设置语言，。！？",
    "zh-Hant": "繁體中文漢語龜殼遊戲設定語言，。！？",
}

NAMES = {
    "es-ES": "Español (España)", "en": "English", "fr": "Français", "de": "Deutsch", "it": "Italiano", "pt-BR": "Português (Brasil)",
    "ru": "Русский", "pl": "Polski", "tr": "Türkçe", "ja": "日本語", "ko": "한국어", "zh-Hans": "简体中文", "zh-Hant": "繁體中文",
}


def load_cmap(path):
    try:
        return set(TTFont(str(path), lazy=True, fontNumber=0).getBestCmap().keys())
    except Exception as error:  # fuente rota o ilegible
        print("  ! no se pudo leer %s: %s" % (path, error))
        return set()


def read_languages(ini_path):
    """Los idiomas de [/Script/Tortunabo.TN_LanguageSettings]: cultura -> (FontRegular, FontBold)."""
    languages = {}
    if not ini_path.exists():
        return languages
    for line in ini_path.read_text(encoding="utf-8-sig").splitlines():
        match = re.match(r"\s*\+Languages=\((.*)\)\s*$", line)
        if not match:
            continue
        fields = dict(re.findall(r'(\w+)="([^"]*)"', match.group(1)))
        if "Culture" in fields:
            languages[fields["Culture"]] = (fields.get("FontRegular", ""), fields.get("FontBold", ""))
    return languages


def po_characters(po_path):
    """Los caracteres de las traducciones (msgstr) de un .po."""
    chars = set()
    if not po_path.exists():
        return chars
    for line in po_path.read_text(encoding="utf-8", errors="replace").splitlines():
        if line.startswith("msgstr") or line.startswith('"'):
            chars.update(re.sub(r"\\.", "", line))
    return {c for c in chars if ord(c) > 0x20 and c not in '"\\'}


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--engine", default="C:/Program Files/Epic Games/UE_5.6", help="carpeta del motor")
    parser.add_argument("--fonts", default=str(PROJECT / "Content" / "Slate" / "Fonts"), help="fuentes de reserva del proyecto")
    parser.add_argument("--po", default=str(PROJECT / "Content" / "Localization" / "Game"), help="carpeta con <cultura>/Game.po")
    args = parser.parse_args()
    sys.stdout.reconfigure(encoding="utf-8")

    engine_fonts = Path(args.engine) / "Engine" / "Content" / "Slate" / "Fonts"
    project_fonts = Path(args.fonts)
    roboto = {name: load_cmap(engine_fonts / name) for name in ("Roboto-Regular.ttf", "Roboto-Bold.ttf", "Roboto-Black.ttf")}
    fallback = load_cmap(engine_fonts / "DroidSansFallback.ttf")
    if not any(roboto.values()) or not fallback:
        print("No se encuentran las fuentes del motor en %s (usa --engine)." % engine_fonts)
        return 2
    primary = set.intersection(*[c for c in roboto.values() if c])  # lo que cubren todos los pesos de Roboto

    languages = read_languages(PROJECT / "Config" / "DefaultGame.ini") or {c: ("", "") for c in SAMPLES}
    failed = False
    print("Fuentes del motor: Roboto (%d caracteres en los tres pesos), Droid Sans Fallback (%d)." % (len(primary), len(fallback)))
    print("Fuentes de reserva del proyecto: %s\n" % (project_fonts if project_fonts.exists() else "(la carpeta no existe todavia)"))

    for culture, (regular, bold) in languages.items():
        reserve = set()
        found = []
        for name in (regular, bold):
            if name and (project_fonts / name).exists():
                reserve |= load_cmap(project_fonts / name)
                found.append(name)
        wanted = set(SAMPLES.get(culture, "")) | set(NAMES.get(culture, "")) | po_characters(Path(args.po) / culture / "Game.po")
        wanted = {c for c in wanted if not c.isspace()}
        cmap_all = primary | reserve | fallback
        missing = sorted(c for c in wanted if ord(c) not in cmap_all)
        weak = sorted(c for c in wanted if ord(c) not in primary and ord(c) not in reserve and ord(c) in fallback)
        state = "FALTAN %d" % len(missing) if missing else "ok"
        print("%-8s %-10s %4d caracteres | reserva propia: %s | solo con la reserva del motor: %d"
              % (culture, state, len(wanted), ", ".join(found) or "ninguna", len(weak)))
        if missing:
            failed = True
            print("         sin fuente: %s" % "".join(missing[:60]) + (" ..." if len(missing) > 60 else ""))
            print("         codigos:    %s" % " ".join("U+%04X" % ord(c) for c in missing[:20]))
        elif weak and len(weak) <= 40:
            print("         con la reserva del motor: %s" % "".join(weak))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
