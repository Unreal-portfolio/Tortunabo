"""Paises enteros del catalogo (Docs/Catalogo-Mapas-2026-09-29.md, «Decision del director»): uno por idioma mas
Filipinas. Modo del catalogo; los «Ambos» salen como Rally (la calzada que cruza el pais); Italia y Filipinas,
Todos contra Todos. Las cajas solo recortan territorios lejanos (Okinawa, Canarias, islas del Atlantico...).

Ademas (encargo del 2026-09-30): el mundo entero (W01, sin pais: todas las tierras de Natural Earth dentro de la
caja, cortado por el Atlantico para que Bering quede en el centro y se pueda cruzar) y cuatro paises elegidos por
su forma (Estados Unidos contiguos, Mexico, Australia y Chile).
"""

from __future__ import annotations

from .country import CountryPreset, Crossing

JAPAN_CROSSINGS = (
    Crossing("Kanmon", (130.93, 33.97), (130.96, 33.93)),
    Crossing("Seikan", (140.37, 41.25), (140.20, 41.41)),
    Crossing("Akashi-Kaikyo", (135.03, 34.64), (134.99, 34.59)),
    Crossing("Naruto", (134.72, 34.25), (134.63, 34.21)),
    Crossing("Seto-Ohashi", (133.81, 34.46), (133.83, 34.33)),
    Crossing("Shimanami", (133.20, 34.40), (133.00, 34.07)),
)

WORLD_CUT_LON = -26.0               # el mapa del mundo se corta por el Atlantico (entre Brasil y Africa)
WORLD_CROSSINGS = (
    Crossing("Gibraltar", (-5.45, 36.10), (-5.40, 35.85)),
    Crossing("Bosforo", (28.95, 41.05), (29.10, 41.00)),
    Crossing("Suez", (32.50, 30.00), (32.62, 30.00)),
    Crossing("Bering", (-169.70, 66.05), (-168.10, 65.62)),
    Crossing("Panama", (-83.50, 9.60), (-75.50, 7.00)),      # a esta escala el istmo no llega a 1 m: puente
    Crossing("Malaca", (102.20, 2.20), (101.70, 1.75)),
    Crossing("Torres", (142.50, -10.75), (142.20, -9.20)),
    Crossing("Canal de la Mancha", (1.35, 51.10), (1.85, 50.95)),
)

COUNTRIES: dict[str, CountryPreset] = {p.key: p for p in (
    CountryPreset(
        "L10_japon_v2", "JPN", "ja", "rally",
        "Japón entero (Rally): Kyūshū, Honshū, Shikoku y Hokkaidō con el relieve y la costa reales, el Fuji y los "
        "Alpes japoneses; calzada tallada de Kagoshima a Wakkanai con puentes naturales en Kanmon, Seikan y el Seto.",
        bbox=(128.5, 30.9, 146.0, 45.7), rotation_deg=33.0, start=(130.56, 31.60), end=(141.70, 45.38),
        crossings=JAPAN_CROSSINGS),
    CountryPreset(
        "I01_filipinas_v2", "PHL", "tl", "tct",
        "Filipinas entera (Todos contra Todos): Luzón, Visayas y Mindanao con el relieve y la costa reales, llanos en "
        "terrazas para pelear y puentes naturales entre islas próximas; caer al agua es la muerte.",
        min_island_m2=20.0, bridge_max_m=22.0),
    CountryPreset(
        "L02_reino_unido", "GBR", "en", "rally",
        "Reino Unido entero (Rally): Gran Bretaña e Irlanda del Norte con el relieve y la costa reales; calzada de "
        "Land's End a John o' Groats.", bbox=(-8.8, 49.8, 2.0, 61.0), start=(-5.60, 50.10), end=(-3.10, 58.60)),
    CountryPreset(
        "L03_francia", "FRA", "fr", "rally",
        "Francia metropolitana entera (Rally) con Córcega, relieve y contorno reales (la frontera es costa); calzada "
        "de los Pirineos al Norte.", bbox=(-5.3, 41.2, 9.7, 51.2)),
    CountryPreset(
        "L04_alemania", "DEU", "de", "rally",
        "Alemania entera (Rally) con el relieve y el contorno reales (la frontera es costa); calzada de los Alpes "
        "bávaros al Báltico."),
    CountryPreset(
        "L05_italia", "ITA", "it", "tct",
        "Italia entera (Todos contra Todos) con Sicilia y Cerdeña, relieve y contorno reales; llanos en terrazas y "
        "puentes naturales entre islas próximas.", bridge_max_m=22.0, min_island_m2=20.0),
    CountryPreset(
        "L06_brasil", "BRA", "pt-BR", "rally",
        "Brasil entero (Rally) con el relieve y el contorno reales (la frontera es costa); calzada del Sur al Norte.",
        bbox=(-74.5, -34.0, -34.7, 5.4)),
    CountryPreset(
        "L07_rusia", "RUS", "ru", "rally",
        "Rusia entera (Rally), compactada a la mitad de Este a Oeste, con el relieve y el contorno reales (la frontera "
        "es costa); calzada del Cáucaso al Ártico.", bbox=(19.5, 41.0, 180.0, 78.0), squash_e=0.5, rotation_deg=0.0),
    CountryPreset(
        "L08_polonia", "POL", "pl", "rally",
        "Polonia entera (Rally) con el relieve y el contorno reales (la frontera es costa); calzada de los Tatras al "
        "Báltico."),
    CountryPreset(
        "L09_turquia", "TUR", "tr", "rally",
        "Turquía entera (Rally) con el relieve y el contorno reales (la frontera es costa); calzada de Anatolia al "
        "mar Negro."),
    CountryPreset(
        "L11_corea_sur", "KOR", "ko", "rally",
        "Corea del Sur entera (Rally) con el relieve y la costa reales (la frontera del Norte es costa); calzada de "
        "Busan al Norte."),
    CountryPreset(
        "L12_china", "CHN", "zh-Hans", "rally",
        "China continental (Rally), solo geografía: relieve y contorno reales (la frontera es costa), sin fronteras "
        "internas; calzada del Sur al Norte."),
    CountryPreset(
        "L13_taiwan", "TWN", "zh-Hant", "rally",
        "Isla de Taiwán (Rally) con el relieve y la costa reales; calzada del Sur al Norte por la costa y la "
        "cordillera Central."),
    CountryPreset(
        "W01_mundo", None, "mul", "rally",
        "El mundo entero (Rally): los continentes como islas con su costa y su relieve reales, sin la Antártida; "
        "calzada de Ciudad del Cabo a la Patagonia por el Sinaí, Asia, el puente de Bering y Panamá, con puentes "
        "naturales en los estrechos reales.",
        bbox=(WORLD_CUT_LON, -56.0, WORLD_CUT_LON + 360.0, 75.0), lon_cut=WORLD_CUT_LON, rotation_deg=0.0,
        start=(19.5, -33.5), end=(-71.5, -52.0), crossings=WORLD_CROSSINGS, auto_bridges=True, bridge_max_m=12.0,
        min_island_m2=20.0),
    CountryPreset(
        "L14_estados_unidos", "USA", "en-US", "rally",
        "Estados Unidos contiguos (Rally), sin Alaska ni Hawái, con el relieve y el contorno reales (la frontera es "
        "costa); calzada de costa a costa, de Florida a Seattle.",
        bbox=(-125.5, 24.0, -66.5, 49.8), rotation_deg=0.0, start=(-80.6, 26.2), end=(-122.2, 47.8)),
    CountryPreset(
        "L15_mexico", "MEX", "es-MX", "rally",
        "México entero (Rally) con el relieve y el contorno reales (la frontera es costa); calzada de Cancún a Los "
        "Cabos: Yucatán, el istmo, la costa del Pacífico y toda la península de Baja California.",
        start=(-87.3, 20.8), end=(-109.9, 23.2)),
    CountryPreset(
        "L16_australia", "AUS", "en-AU", "rally",
        "Australia entera (Rally) con Tasmania, relieve y costa reales; calzada de Hobart al cabo York con un puente "
        "natural sobre el estrecho de Bass.",
        bbox=(112.5, -44.0, 154.5, -9.5), rotation_deg=0.0, start=(147.2, -42.6), end=(142.6, -11.3),
        crossings=(Crossing("Bass", (146.40, -41.10), (146.30, -38.95)),)),
    CountryPreset(
        "L17_chile", "CHL", "es-CL", "rally",
        "Chile entero (Rally), de punta a punta, con el relieve y el contorno reales (la frontera es costa); calzada "
        "de Punta Arenas a Arica entre los Andes y el Pacífico, con puentes naturales entre los fiordos.",
        bbox=(-76.5, -56.0, -66.0, -17.2), start=(-71.0, -53.0), end=(-70.2, -18.6), auto_bridges=True),
)}
