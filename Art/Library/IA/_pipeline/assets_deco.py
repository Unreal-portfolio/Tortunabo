"""Decoración de mapas por idioma (Docs/Catalogo-Mapas-2026-09-29.md) que no existe en ETNBeachElement.

torii (L10 ja, Fuji), pagoda (L12/L13 zh, Guilin/Taroko) y molino (L04 de, Rin; también pl). Origen en la base; el
molino separa las aspas (giran en X alrededor de su buje, socket Sails de la torre).
"""
import math

from ia_mesh import Builder, rot_x

CATEGORY = 'decoracion'


# ── Torii ───────────────────────────────────────────────────────────────────
TORII_Y = 200.0


def torii():
    b = Builder()
    for side in (1, -1):
        y = side * TORII_Y
        b.cyl((0, y, 0), (0, y, 440), 22.0, 'paint', seg=10, r1=18.5)
        b.cyl((0, y, 0), (0, y, 34), 27.0, 'trim', seg=10, r1=25.0)  # kamaki
        b.box((-8, y - 26, 328), (8, y + 26, 336), 'trim', 0.9)  # cuña del nuki
    b.box((-11, -265, 336), (11, 265, 364), 'paint')  # nuki
    b.box((-9, -34, 364), (9, 34, 422), 'trim')  # gakuzuka (placa)
    b.box((-10.5, -26, 372), (10.5, 26, 414), 'detail')
    b.box((-15, -285, 422), (15, 285, 446), 'paint', 0.95)  # shimaki
    # Kasagi negro con las puntas levantadas (loft de secciones a lo largo de Y).
    rings = []
    for k in range(11):
        t = -1.0 + 2.0 * k / 10
        y = t * 335.0
        lift = 30.0 * t ** 4
        half_x = 24.0 + 4.0 * t * t
        z0, z1 = 446.0 + lift * 0.7, 478.0 + lift
        rings.append([(-half_x, y, z0), (half_x, y, z0), (half_x + 3, y, z1), (-half_x - 3, y, z1)])
    b.loft(rings, 'trim', zone_of_band=lambda band, s: ('trim', 1.0 if s != 2 else 0.85))
    for side in (1, -1):
        b.col_box((-22, side * TORII_Y - 22, 0), (22, side * TORII_Y + 22, 440))
    return b


# ── Pagoda ──────────────────────────────────────────────────────────────────
TIERS = ((150.0, 170.0), (122.0, 140.0), (96.0, 120.0))


def _roof(b, half, z, eave):
    """Tejado de un piso: tronco de pirámide con alero y cuatro esquinas levantadas."""
    w = half + eave
    b.box((-w + 8, -w + 8, z - 6), (w - 8, w - 8, z + 4), 'paint', 0.8)  # canecillos
    b.hull([(sx * w, sy * w, z + 4) for sx in (-1, 1) for sy in (-1, 1)] +
           [(sx * w, sy * w, z + 14) for sx in (-1, 1) for sy in (-1, 1)] +
           [(sx * half * 0.55, sy * half * 0.55, z + 62) for sx in (-1, 1) for sy in (-1, 1)], 'dark')
    for sx in (-1, 1):
        for sy in (-1, 1):
            b.hull([(sx * (w - 30), sy * (w - 30), z + 8), (sx * (w - 30), sy * (w - 30), z + 30),
                    (sx * (w - 2), sy * (w - 30), z + 8), (sx * (w - 30), sy * (w - 2), z + 8),
                    (sx * (w + 22), sy * (w + 22), z + 44)], 'dark', 0.85)


def pagoda():
    b = Builder()
    b.box((-200, -200, 0), (200, 200, 36), 'trim')
    b.box((-70, -236, 0), (70, -200, 18), 'trim', 0.9)  # escalón
    z = 36.0
    for i, (half, h) in enumerate(TIERS):
        b.box((-half, -half, z), (half, half, z + h), 'detail')
        for sx in (-1, 1):
            for sy in (-1, 1):
                b.box((sx * half - 9, sy * half - 9, z), (sx * half + 9, sy * half + 9, z + h), 'paint')
        door_w = half * 0.45
        b.box((half - 2, -door_w, z), (half + 3, door_w, z + h * 0.72), 'paint', 0.9)  # puerta (+X)
        b.box((half + 3, -door_w + 8, z), (half + 5, door_w - 8, z + h * 0.64), 'trim', 0.6)
        eave = 62.0 - i * 8
        _roof(b, half, z + h, eave)
        if i == 0:
            w = half + eave
            for sx in (-1, 1):
                for sy in (-1, 1):
                    b.cyl((sx * (w + 8), sy * (w + 8), z + h + 30), (sx * (w + 8), sy * (w + 8), z + h - 2), 1.5,
                          'trim', seg=4)
                    b.sphere((sx * (w + 8), sy * (w + 8), z + h - 16), 13.0, 'light', seg=6, rings=4)
        z += h + 62.0
    for zz, r in ((z - 10, 20.0), (z + 18, 13.0), (z + 42, 9.0), (z + 64, 6.0)):  # remate
        b.sphere((0, 0, zz), r, 'detail', seg=8, rings=4)
    b.cyl((0, 0, z - 10), (0, 0, z + 92), 3.0, 'detail', seg=6, r1=0.0)
    b.col_box((-200, -200, 0), (200, 200, 36))
    b.col_box((-150, -150, 36), (150, 150, z))
    return b


# ── Molino ──────────────────────────────────────────────────────────────────
HUB = (236.0, 0.0, 590.0)  # por delante de la galería (r 206) para que las aspas no la corten


def molino_torre():
    b = Builder()
    b.lathe([(0, 0), (190, 0), (190, 64), (0, 64)], 'trim', seg=8, phase=math.pi / 8,
            zone_of_band=lambda band, s: ('trim', 1.0 if s % 2 else 0.9))
    b.lathe([(0, 60), (172, 60), (128, 540), (0, 540)], 'detail', seg=8, phase=math.pi / 8,
            zone_of_band=lambda band, s: ('detail', 1.0 if s % 2 else 0.92))
    b.lathe([(0, 250), (206, 250), (206, 264), (0, 264)], 'dark', seg=8, phase=math.pi / 8)  # galería
    for a in range(8):  # barandilla
        ang = math.pi / 8 + a * math.pi / 4
        b.box((math.cos(ang) * 198 - 4, math.sin(ang) * 198 - 4, 264), (math.cos(ang) * 198 + 4,
              math.sin(ang) * 198 + 4, 300), 'dark', 0.9)
    b.torus((0, 0, 298), (0, 0, 1), 198.0, 3.5, 'dark', major_seg=8, minor_seg=3, phase=math.pi / 8)
    # Caperuza en forma de barca, orientada a +X.
    b.hull([(math.cos(a) * 140, math.sin(a) * 140, 536) for a in (math.pi / 8 + k * math.pi / 4 for k in range(8))] +
           [(-150, 0, 610), (150, 0, 612), (-90, -70, 640), (-90, 70, 640), (90, -70, 640), (90, 70, 640),
            (-40, 0, 672), (40, 0, 672)], 'paint')
    b.hull([(110, y, z) for y in (-34, 34) for z in (HUB[2] - 30, HUB[2] + 34)] +
           [(HUB[0] - 30, y, z) for y in (-24, 24) for z in (HUB[2] - 22, HUB[2] + 26)], 'paint', 0.9)  # morro
    b.cyl((HUB[0] - 32, 0, HUB[2]), (HUB[0], 0, HUB[2]), 16.0, 'dark', seg=8)
    # Puerta y ventanas en la cara +X.
    b.box((168, -34, 64), (182, 34, 190), 'dark', 0.8)
    b.box((178, -40, 186), (184, 40, 198), 'paint')
    for z, r in ((360.0, 154.0), (460.0, 137.0)):
        b.box((r - 6, -20, z), (r + 6, 20, z + 44), 'light')
    for side in (1, -1):
        b.box((-20, side * 160 - 6, 340), (20, side * 160 + 6, 384), 'light', 0.9)
    b.socket('Sails', HUB)
    b.col_hull([(math.cos(a) * 190, math.sin(a) * 190, z) for a in (k * math.pi / 4 for k in range(8)) for z in (0, 64)] +
               [(math.cos(a) * 130, math.sin(a) * 130, 540) for a in (k * math.pi / 4 for k in range(8))])
    return b


def molino_aspas():
    """Cuatro aspas en el plano YZ: origen en el buje, giran alrededor de X."""
    b = Builder()
    b.cyl((-6, 0, 0), (26, 0, 0), 22.0, 'dark', seg=8)
    b.cyl((26, 0, 0), (34, 0, 0), 12.0, 'paint', seg=8, r1=0.0)
    for k in range(4):
        with b.frame(rot_x(45 + 90 * k)):
            b.box((4, -8, 18), (16, 8, 440), 'dark')  # vara
            for y in (12.0, 96.0):  # largueros del bastidor
                b.box((6, y - 4, 90), (14, y + 4, 430), 'dark', 0.9)
            for i in range(6):
                zz = 96 + i * 64
                b.box((6, 8, zz), (14, 96, zz + 7), 'dark', 0.85)
            b.box((0, 14, 100), (4, 94, 300), 'paint')  # lona recogida a medias
    return b


ASSETS = [
    dict(slug='torii', title='Torii (ja, L10 Fuji)', category=CATEGORY, budget=1500,
         prompt='Stylized low-poly vermilion torii gate: tapered pillars on black bases, nuki tie beam, central '
                'plaque, black kasagi top beam with upswept ends, flat-shaded',
         palette={'paint': 0xD8432F, 'detail': 0xE8C15A, 'dark': 0x3A2E27, 'trim': 0x23201F, 'light': 0xFFE8A3},
         parts=[dict(name='SM_TN_Torii', build=torii, budget=1500, pivot='base', role='decoración con colisión')],
         notes='4 m entre pilares, 5 m de alto: pasa una tortuga o el buggy (2,45 m).'),
    dict(slug='pagoda', title='Pagoda (zh, L12 Guilin / L13 Taroko)', category=CATEGORY, budget=1500,
         prompt='Stylized low-poly three-tier Chinese pagoda on a stone plinth: cream walls, red corner columns and '
                'doors, dark tiled roofs with upturned eaves, hanging lanterns and a golden finial, flat-shaded',
         palette={'paint': 0xC23B2B, 'detail': 0xF1E3C6, 'dark': 0x2F5D50, 'trim': 0x8C8479, 'light': 0xFFB347},
         parts=[dict(name='SM_TN_Pagoda', build=pagoda, budget=1500, pivot='base', role='decoración con colisión')]),
    dict(slug='molino', title='Molino (de L04 Rin / pl)', category=CATEGORY, budget=1500,
         prompt='Stylized low-poly European tower windmill: stone base, whitewashed octagonal tapered tower with a '
                'gallery, red boat-shaped cap, four lattice sails with half-furled canvas, flat-shaded',
         palette={'paint': 0xC9463D, 'detail': 0xF1EBDD, 'dark': 0x6B4A33, 'trim': 0x9C9186, 'light': 0xFFD27A},
         parts=[dict(name='SM_TN_MolinoTorre', build=molino_torre, budget=900, pivot='base', role='torre fija',
                     required_sockets=('Sails',)),
                dict(name='SM_TN_MolinoAspas', build=molino_aspas, budget=600, pivot='hub',
                     role='aspas (giran en X alrededor del origen)', preview_loc=HUB, preview_rot=(20, 0, 0))],
         notes='Las aspas se enganchan en el socket Sails y giran en X (RotatingMovementComponent).'),
]
