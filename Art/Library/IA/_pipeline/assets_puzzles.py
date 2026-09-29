"""Puzzles (Docs/superpowers/specs/2026-09-29-foto-a-mapa-design.md §6): piezas de las plantillas coop y 2 vs 2.

Escala: tortuga de ~115 cm de pie. Origen en la base (suelo, Z = 0) salvo las piezas que giran o se deslizan, que
llevan el origen en su bisagra; la pieza fija tiene un socket con el mismo nombre (Pivot, Leaf, Platform...) donde se
engancha la móvil.
"""
import math
import random

from ia_mesh import Builder, mirror_y, move, rot_x, rot_z

CATEGORY = 'puzzles'
STONE, SAND, WOOD, WOOD_DARK = 0x9C9186, 0xE9D3A1, 0x9A6A44, 0x6B4A33


def scallop(b, size, zone, depth=1.6, lip_zone=None):
    """Concha de vieira plana en el plano XZ (charnela en el origen, abanico hacia +Z), grosor en Y."""
    pts = [(-0.18 * size, -0.05 * size), (0.18 * size, -0.05 * size)]
    n = 11
    for i in range(n + 1):  # borde festoneado de 15° a 165°
        a = math.radians(15 + 150 * i / n)
        r = size * (1.0 if i % 2 == 0 else 0.9)
        pts.append((math.cos(a) * r, math.sin(a) * r))
    b.prism(pts, depth, zone)
    with b.frame(move(0, -depth * 0.6, 0)):
        b.prism([(-0.28 * size, -0.12 * size), (0.28 * size, -0.12 * size), (0.2 * size, 0.1 * size),
                 (-0.2 * size, 0.1 * size)], depth, lip_zone or zone)


def star(b, r_out, r_in, depth, zone, points=5):
    poly = []
    for i in range(points * 2):
        a = math.pi / 2 + math.pi * i / points
        r = r_out if i % 2 == 0 else r_in
        poly.append((math.cos(a) * r, math.sin(a) * r))
    b.prism(poly, depth, zone)


# ── Palanca con base ────────────────────────────────────────────────────────
PIVOT_Z = 46.0


def palanca_base():
    b = Builder()
    b.hull(mirror_y([(-36, 26, 0), (36, 26, 0), (-36, 26, 24), (36, 26, 24), (-31, 21, 31), (31, 21, 31)]), 'dark')
    b.box((-22, -14, 30), (22, 14, 33), 'trim', 0.9)
    for side in (1, -1):  # carrilleras que sujetan el eje
        b.hull([(x, side * y, z) for x, y, z in ((-12, 8, 30), (12, 8, 30), (-9, 8, 52), (9, 8, 52),
                                                   (-12, 12, 30), (12, 12, 30), (-9, 12, 52), (9, 12, 52))], 'paint')
        b.cyl((0, side * 12, PIVOT_Z), (0, side * 14.5, PIVOT_Z), 3.4, 'trim', seg=8)
    b.cyl((0, -12, PIVOT_Z), (0, 12, PIVOT_Z), 2.0, 'trim', seg=6)
    # Ranura de recorrido con marcas de posición.
    for x, zone in ((-16, 'detail'), (16, 'light')):
        b.box((x - 2.5, -5, 33), (x + 2.5, 5, 34.2), zone)
    with b.frame(move(36.6, 0, 7.0) @ rot_z(90)):  # vieira en la cara delantera
        scallop(b, 11.0, 'detail', lip_zone='light')
    b.socket('Pivot', (0, 0, PIVOT_Z))
    b.col_box((-36, -26, 0), (36, 26, 31))
    return b


def palanca_brazo():
    """Brazo de la palanca: origen en el eje (gira en Y), en reposo vertical."""
    b = Builder()
    b.cyl((0, -7.5, 0), (0, 7.5, 0), 4.6, 'trim', seg=8)
    b.hull(mirror_y([(-3.2, 2.6, 0), (3.2, 2.6, 0), (-2.4, 2.2, 52), (2.4, 2.2, 52)]), 'dark')
    b.cyl((0, 0, 47), (0, 0, 55), 2.8, 'trim', seg=8)
    b.sphere((0, 0, 61), 7.5, 'paint', seg=8, rings=6)
    b.col_hull([(0, 0, 0), (0, 0, 68)] + [(x, y, z) for x in (-7.5, 7.5) for y in (-7.5, 7.5) for z in (54, 68)])
    return b


# ── Cesta de playa con aro ──────────────────────────────────────────────────
RIM = (44.0, 0.0, 250.0)
BOARD_Z = 268.0


def _surfboard(half_w, half_h, n=16):
    """Contorno de tabla de surf: nariz en punta arriba, cola redondeada abajo (u -> Y, v -> Z)."""
    pts = []
    for i in range(n):
        a = 2 * math.pi * i / n
        v = math.cos(a)
        width = half_w * (1.0 - 0.45 * max(0.0, v) ** 2) * (0.82 if v < -0.6 else 1.0)
        pts.append((math.sin(a) * width, v * half_h))
    return pts


def cesta_aro():
    b = Builder()
    b.lathe([(0, 0), (46, 0), (44, 10), (36, 16), (0, 17)], 'dark', seg=8, phase=math.pi / 8,
            zone_of_band=lambda band, s: ('dark', 1.0 if s % 2 else 0.9))
    for i in range(5):  # poste a franjas (acaba detrás del tablero)
        z0 = 16 + i * 55
        b.cyl((0, 0, z0), (0, 0, z0 + 55), 5.0, 'paint' if i % 2 == 0 else 'trim', seg=8, phase=math.pi / 8)
    # Tablero con forma de tabla de surf y su franja.
    with b.frame(move(10, 0, BOARD_Z) @ rot_z(90)):
        b.prism(_surfboard(36, 62), 5.0, 'paint', side_zone='trim')
        with b.frame(move(0, -2.2, 0)):
            b.prism([(-5, -56), (5, -56), (5, 58), (-5, 58)], 2.0, 'detail')
    for z in (240.0, 290.0):
        b.box((0, -6, z), (10, 6, z + 8), 'trim')
    # Aro y cesta de mimbre (tejido en damero), abierta arriba.
    b.box((12.5, -5, RIM[2] - 5), (17.5, 5, RIM[2] + 5), 'trim')
    b.torus(RIM, (0, 0, 1), 28.0, 2.2, 'detail', major_seg=12, minor_seg=4, minor_phase=math.pi / 4)
    prof = [(0, -38), (17, -36), (24, -19), (27, -2), (25.5, -2), (22.5, -18), (15.5, -34), (0, -36)]
    b.lathe(prof, 'dark', seg=12, origin=RIM, zone_of_band=lambda band, s: (
        ('detail', 0.95) if (band + s) % 2 and band in (1, 2) else ('dark', 1.0 if band < 4 else 0.8)))
    b.socket('Goal', (RIM[0], RIM[1], RIM[2] - 18))
    b.col_box((-5, -5, 0), (5, 5, 291))
    b.col_box((-46, -46, 0), (46, 46, 17))
    b.col_box((7, -36, BOARD_Z - 62), (13, 36, BOARD_Z + 62))
    return b


# ── Placa de presión ────────────────────────────────────────────────────────

def _placa(pad_top, emblem_zone):
    b = Builder()
    for lo, hi in (((-62, -62, 0), (62, -50, 7)), ((-62, 50, 0), (62, 62, 7)),
                   ((-62, -50, 0), (-50, 50, 7)), ((50, -50, 0), (62, 50, 7))):
        b.box(lo, hi, 'trim')
    b.box((-50, -50, 0), (50, 50, 1.5), 'trim', 0.7)
    b.hull([(x * s, y * s, z) for x, y in ((1, 1), (1, -1), (-1, 1), (-1, -1)) for s, z in
            ((49.0, 1.5), (49.0, pad_top - 2.5), (45.0, pad_top))], 'paint')
    for x, y in ((56, 56), (56, -56), (-56, 56), (-56, -56)):
        b.cyl((x, y, 7), (x, y, 8.4), 2.6, 'detail', seg=6)
    with b.frame(move(0, 0, pad_top + 0.6) @ rot_x(90)):
        star(b, 30.0, 13.0, 1.2, emblem_zone)
    b.col_box((-62, -62, 0), (62, 62, pad_top))
    return b


def placa_subida():
    return _placa(11.0, 'detail')


def placa_bajada():
    return _placa(4.0, 'light')


# ── Puerta de madera y conchas ──────────────────────────────────────────────
DOOR_W, DOOR_H = 220.0, 255.0


def _log(b, p0, p1, r, rng, zone='dark'):
    mid = [(a + c) / 2 + rng.uniform(-2.5, 2.5) for a, c in zip(p0, p1)]
    near0 = [a + (c - a) * 0.08 for a, c in zip(p0, p1)]  # puntas rectas: la base apoya plana en el suelo
    near1 = [c + (a - c) * 0.08 for a, c in zip(p0, p1)]
    b.sweep([p0, near0, mid, near1, p1], [r, r, r * 0.93, r * 0.97, r * 0.97], zone, seg=7, phase=rng.uniform(0, 1),
            zone_of_band=lambda band, s: (zone, 1.0 if s % 2 else 0.9))


def puerta_marco():
    b = Builder()
    rng = random.Random(7)
    post_y = DOOR_W / 2 + 16
    for side in (1, -1):
        _log(b, (0, side * post_y, 0), (0, side * post_y, 318), 16.0, rng)
        b.box((-7, side * (DOOR_W / 2 + 1), 0), (7, side * (DOOR_W / 2 + 4), 280), 'trim')  # guía
        b.lathe([(0, 0), (24, 0), (20, 14), (0, 16)], 'trim', seg=7, origin=(0, side * post_y, 0))  # zapata
        for z in (40.0, 300.0):
            b.torus((0, side * post_y, z), (0, 0, 1), 16.5, 2.0, 'detail', major_seg=8, minor_seg=3)
        with b.frame(move(16.5, side * post_y, 150) @ rot_z(90)):
            scallop(b, 13.0, 'light', lip_zone='detail')
    _log(b, (0, -post_y - 40, 300), (0, post_y + 40, 300), 17.0, rng)
    b.box((-8, -post_y, 268), (8, post_y, 286), 'dark', 0.85)
    with b.frame(move(19.0, 0, 292) @ rot_z(90)):
        scallop(b, 34.0, 'light', depth=3.0, lip_zone='detail')
    b.socket('Leaf', (0, 0, 0))
    b.socket('LeafOpen', (0, 0, DOOR_H + 10))
    for side in (1, -1):
        b.col_box((-17, side * (post_y - 17), 0), (17, side * (post_y + 17), 318))
    b.col_box((-18, -post_y - 55, 283), (18, post_y + 55, 318))
    return b


def puerta_hoja():
    """Hoja deslizante: origen en el centro de su borde inferior; sube por las guías del marco."""
    b = Builder()
    rng = random.Random(11)
    n = 5
    w = DOOR_W / n
    for i in range(n):
        y0 = -DOOR_W / 2 + i * w
        top = DOOR_H - rng.uniform(0, 8)
        b.box((-4, y0 + 0.6, 0), (4, y0 + w - 0.6, top), 'dark', 1.0 if i % 2 else 0.88)
    for z in (40.0, 205.0):
        b.box((4, -DOOR_W / 2 + 6, z), (8, DOOR_W / 2 - 6, z + 18), 'paint')
    b.hull([(4, -DOOR_W / 2 + 12, 52), (8, -DOOR_W / 2 + 12, 52), (4, -DOOR_W / 2 + 26, 52),
            (4, DOOR_W / 2 - 26, 205), (8, DOOR_W / 2 - 12, 205), (4, DOOR_W / 2 - 12, 205),
            (8, -DOOR_W / 2 + 26, 52), (8, DOOR_W / 2 - 26, 205)], 'paint', 0.92)
    for y, z, s in ((-60, 130, 15.0), (0, 108, 20.0), (60, 130, 15.0)):
        with b.frame(move(8.5, y, z) @ rot_z(90)):
            scallop(b, s, 'light', lip_zone='detail')
    b.torus((9.5, 0, 75), (1, 0, 0), 9.0, 1.4, 'trim', major_seg=8, minor_seg=4)
    b.col_box((-8, -DOOR_W / 2, 0), (8, DOOR_W / 2, DOOR_H))
    return b


# ── Ascensor de contrapeso ──────────────────────────────────────────────────
TOP_Z, POST_Y, WEIGHT_X = 620.0, 128.0, -150.0


def ascensor_marco():
    b = Builder()
    for side in (1, -1):
        y = side * POST_Y
        b.box((-13, y - 13, 0), (13, y + 13, TOP_Z + 20), 'dark')
        b.box((-20, y - 20, 0), (20, y + 20, 18), 'trim')
        b.box((13, y - 3 * side - 2, 0), (16, y - 3 * side + 2, TOP_Z - 10), 'trim')  # carril
        # Tornapuntas del brazo del contrapeso: por encima del recorrido de la plataforma (z > 400).
        b.hull([(-13, y - 5, TOP_Z - 130), (-13, y + 5, TOP_Z - 130), (-13, y - 5, TOP_Z - 114),
                (-13, y + 5, TOP_Z - 114), (WEIGHT_X + 40, -5, TOP_Z), (WEIGHT_X + 40, 5, TOP_Z),
                (WEIGHT_X + 25, -5, TOP_Z), (WEIGHT_X + 25, 5, TOP_Z)], 'dark', 0.85)
    b.box((-14, -POST_Y - 24, TOP_Z), (14, POST_Y + 24, TOP_Z + 26), 'dark')
    b.box((WEIGHT_X - 16, -14, TOP_Z), (14, 14, TOP_Z + 22), 'dark', 0.9)
    for x in (0.0, WEIGHT_X):  # poleas
        b.box((x - 5, -12, TOP_Z + 20), (x + 5, 12, TOP_Z + 46), 'trim')
        b.cyl((x, -6, TOP_Z + 46), (x, 6, TOP_Z + 46), 20.0, 'paint', seg=10)
        b.cyl((x, -8, TOP_Z + 46), (x, 8, TOP_Z + 46), 6.0, 'trim', seg=6)
    b.box((WEIGHT_X - 18, -18, 0), (WEIGHT_X + 18, 18, 8), 'trim', 0.8)  # tope del contrapeso
    b.socket('Platform', (0, 0, 0))
    b.socket('PulleyPlatform', (0, 0, TOP_Z + 46))
    b.socket('PulleyWeight', (WEIGHT_X, 0, TOP_Z + 46))
    b.socket('Weight', (WEIGHT_X, 0, 8))
    for side in (1, -1):
        b.col_box((-10, side * POST_Y - 10, 0), (10, side * POST_Y + 10, TOP_Z + 20))
    b.col_box((-14, -POST_Y - 24, TOP_Z), (14, POST_Y + 24, TOP_Z + 66))
    return b


def ascensor_plataforma():
    """Plataforma de 2 × 2 m con estribo: origen en el centro de su base, sube por los carriles del marco."""
    b = Builder()
    for i in range(5):
        x0 = -100 + i * 40
        b.box((x0 + 0.8, -108, 4), (x0 + 39.2, 108, 14), 'dark', 1.0 if i % 2 else 0.9)
    for lo, hi in (((-104, -112, 0), (104, -104, 16)), ((-104, 104, 0), (104, 112, 16)),
                   ((-104, -104, 0), (-96, 104, 6)), ((96, -104, 0), (104, 104, 6))):
        b.box(lo, hi, 'paint')
    for side in (1, -1):
        b.box((-5, side * 104 - 4, 16), (5, side * 104 + 4, 230), 'paint')
        b.box((5, side * (POST_Y - 3) - 4, 20), (8, side * (POST_Y - 3) + 4, 60), 'trim')  # patín
    b.box((-6, -112, 222), (6, 112, 234), 'paint', 0.9)
    b.torus((0, 0, 242), (1, 0, 0), 8.0, 1.8, 'trim', major_seg=8, minor_seg=4)
    b.socket('Rope', (0, 0, 250))
    b.col_box((-104, -112, 0), (104, 112, 16))
    return b


def ascensor_contrapeso():
    """Cubo de arena con asa: origen en la base; socket Rope en la argolla."""
    b = Builder()
    b.lathe([(0, 0), (26, 0), (32, 58), (34, 62), (31, 62), (0, 50)], 'paint', seg=10,
            zone_of_band=lambda band, s: ('trim', 1.0) if band == 3 else ('paint', 1.0 if s % 2 else 0.92))
    b.lathe([(0, 44), (29, 53), (0, 58)], 'detail', seg=10)  # arena asomando
    for z in (14.0, 38.0):
        b.torus((0, 0, z), (0, 0, 1), 27.5 + (z / 58) * 6, 1.4, 'trim', major_seg=10, minor_seg=3)
    arc = [(0, -33 * math.cos(math.radians(a)), 62 + 30 * math.sin(math.radians(a))) for a in range(0, 181, 30)]
    b.sweep(arc, 1.6, 'trim', seg=4)
    b.torus((0, 0, 97), (1, 0, 0), 5.0, 1.3, 'trim', major_seg=8, minor_seg=4)
    b.socket('Rope', (0, 0, 102))
    b.col_box((-34, -34, 0), (34, 34, 62))
    return b


# ── Boquilla de géiser orientable ───────────────────────────────────────────
BALL_Z = 44.0


def geiser_base():
    b = Builder()
    rng = random.Random(3)
    pts = []
    for i in range(10):
        a = 2 * math.pi * i / 10 + rng.uniform(-0.1, 0.1)
        r0, r1 = rng.uniform(64, 76), rng.uniform(34, 42)
        pts += [(math.cos(a) * r0, math.sin(a) * r0, 0.0), (math.cos(a) * r1, math.sin(a) * r1, rng.uniform(18, 26))]
    b.hull(pts, 'trim')
    b.torus((0, 0, 24), (0, 0, 1), 24.0, 5.0, 'dark', major_seg=10, minor_seg=4)
    b.cyl((0, 0, 20), (0, 0, 36), 13.0, 'dark', seg=10)
    b.sphere((0, 0, BALL_Z), 12.0, 'detail', seg=10, rings=5)
    b.socket('Pivot', (0, 0, BALL_Z))
    b.col_hull([(x, y, 0) for x, y in ((-76, -76), (76, -76), (76, 76), (-76, 76))] +
               [(x, y, 26) for x, y in ((-40, -40), (40, -40), (40, 40), (-40, 40))])
    return b


def geiser_boquilla():
    """Tobera de latón: origen en la rótula; apunta a +Z en reposo; Muzzle en la boca con +X hacia fuera."""
    b = Builder()
    prof = [(0, -3), (13, -3), (14.5, 4), (14.5, 12), (11, 16), (7, 52), (9.5, 60), (10, 64), (6.5, 64), (0, 58)]
    bands = {2: ('paint', 1.0), 3: ('paint', 0.9), 6: ('trim', 1.0), 7: ('trim', 0.9)}
    b.lathe(prof, 'detail', seg=10, zone_of_band=lambda band, s: bands.get(band, ('detail', 1.0 if s % 2 else 0.92)))
    for side in (1, -1):  # asas para orientarla
        b.sweep([(0, side * 11, 22), (0, side * 20, 26), (0, side * 21, 36), (0, side * 9, 42)], 1.6, 'trim', seg=5)
    b.socket('Muzzle', (0, 0, 64), (0, -90, 0))
    return b


ASSETS = [
    dict(slug='palanca', title='Palanca con base', category=CATEGORY, budget=800,
         prompt='Stylized low-poly puzzle lever on a sandstone block base, painted side cheeks holding the axle, '
                'wooden lever arm with a round knob, scallop shell on the front, two position marks, flat-shaded',
         palette={'paint': 0x2F80ED, 'detail': 0xF28C28, 'dark': 0xC9A574, 'trim': 0x5B6168, 'light': 0x7CFFB0},
         parts=[dict(name='SM_TN_PalancaBase', build=palanca_base, budget=500, pivot='base', role='base fija',
                     required_sockets=('Pivot',)),
                dict(name='SM_TN_PalancaBrazo', build=palanca_brazo, budget=300, pivot='hinge',
                     role='brazo (gira en Y alrededor del origen)', preview_loc=(0, 0, PIVOT_Z),
                     preview_rot=(0, 28, 0))],
         notes='El brazo se engancha en el socket Pivot de la base y gira en Y (±30°). Zona Light = marca de '
               'posición activa (emisiva). Plantillas basket_hold y lever_relay.'),
    dict(slug='cesta_aro', title='Cesta de playa con aro', category=CATEGORY, budget=1200,
         prompt='Stylized low-poly beach basket goal: striped pole on a weighted base, surfboard-shaped backboard, '
                'orange rim with an open woven wicker basket hanging under it, flat-shaded',
         palette={'paint': 0x2EA8A0, 'detail': 0xF28C28, 'dark': 0xC8A165, 'trim': 0xF4EFE2, 'light': 0xFFF4C2},
         parts=[dict(name='SM_TN_CestaAro', build=cesta_aro, budget=1200, pivot='base', role='prop fijo',
                     required_sockets=('Goal',))],
         notes='Socket Goal en el centro de la cesta (volumen de detección). La cesta necesita colisión compleja '
               '(Use Complex As Simple) si ha de retener la bola; los UCX solo cubren poste, base y tablero.'),
    dict(slug='placa_presion', title='Placa de presión (subida y bajada)', category=CATEGORY, budget=800,
         prompt='Stylized low-poly square pressure plate with a stone frame, raised painted pad and a starfish '
                'emblem; two states: up (emblem plain) and down (emblem glowing), flat-shaded',
         palette={'paint': 0xE4572E, 'detail': 0xFFD23F, 'dark': 0x6B4A33, 'trim': STONE, 'light': 0x7CFFB0},
         parts=[dict(name='SM_TN_PlacaPresion_Subida', build=placa_subida, budget=400, pivot='base',
                     role='estado subido', preview_loc=(0, -75, 0)),
                dict(name='SM_TN_PlacaPresion_Bajada', build=placa_bajada, budget=400, pivot='base',
                     role='estado bajado (emblema emisivo)', preview_loc=(0, 75, 0))],
         notes='Dos mallas completas (una por estado) según el encargo; alternativa barata: marco fijo + pad móvil.'),
    dict(slug='puerta_puzzle', title='Puerta de puzzle de madera y conchas', category=CATEGORY, budget=1500,
         prompt='Stylized low-poly puzzle gate made of driftwood logs lashed with rope, a sliding plank door with '
                'painted cross braces and scallop shells, big scallop on the lintel, flat-shaded',
         palette={'paint': 0x2EA8A0, 'detail': 0xD8C49A, 'dark': WOOD, 'trim': 0x5B6168, 'light': 0xF6B7A6},
         parts=[dict(name='SM_TN_PuertaMarco', build=puerta_marco, budget=900, pivot='base', role='marco fijo',
                     required_sockets=('Leaf', 'LeafOpen')),
                dict(name='SM_TN_PuertaHoja', build=puerta_hoja, budget=600, pivot='base',
                     role='hoja (sube por las guías hasta LeafOpen)')],
         notes='Hueco libre 220 × 255 cm. La hoja se desliza de Leaf a LeafOpen (TimelineComponent o Mover).'),
    dict(slug='ascensor_contrapeso', title='Ascensor de contrapeso', category=CATEGORY, budget=1500,
         prompt='Stylized low-poly wooden counterweight lift: two tall posts with rails, top beam and arm with two '
                'painted pulleys, a 2x2 m plank platform with a stirrup and rope ring, and a sand bucket '
                'counterweight, flat-shaded',
         palette={'paint': 0xE4572E, 'detail': SAND, 'dark': WOOD, 'trim': 0x5B6168, 'light': 0xFFF4C2},
         parts=[dict(name='SM_TN_AscensorMarco', build=ascensor_marco, budget=700, pivot='base', role='marco fijo',
                     required_sockets=('Platform', 'PulleyPlatform', 'PulleyWeight', 'Weight')),
                dict(name='SM_TN_AscensorPlataforma', build=ascensor_plataforma, budget=450, pivot='base',
                     role='plataforma móvil (Z)', preview_loc=(0, 0, 40), required_sockets=('Rope',)),
                dict(name='SM_TN_AscensorContrapeso', build=ascensor_contrapeso, budget=350, pivot='base',
                     role='contrapeso móvil (Z)', preview_loc=(WEIGHT_X, 0, 380), required_sockets=('Rope',))],
         notes='Recorrido de la plataforma 0-380 cm. Las cuerdas no van en la malla: UCableComponent de '
               'PulleyPlatform a Rope y de PulleyWeight a Rope del contrapeso. Plantilla counterweight_lift.'),
    dict(slug='boquilla_geiser', title='Boquilla de géiser orientable', category=CATEGORY, budget=800,
         prompt='Stylized low-poly geyser nozzle: rocky collar set in the sand with a ball joint, and a separate '
                'brass flared nozzle with two side handles that can be aimed, flat-shaded',
         palette={'paint': 0x2F80ED, 'detail': 0xD9A441, 'dark': 0x4A4F57, 'trim': 0xA39C92, 'light': 0x9EE7FF},
         parts=[dict(name='SM_TN_GeiserBase', build=geiser_base, budget=400, pivot='base', role='base fija',
                     required_sockets=('Pivot',)),
                dict(name='SM_TN_GeiserBoquilla', build=geiser_boquilla, budget=400, pivot='hinge',
                     role='tobera (rótula en el origen, apunta a +Z)', preview_loc=(0, 0, BALL_Z),
                     preview_rot=(0, 25, 0), required_sockets=('Muzzle',))],
         notes='La tobera gira alrededor del origen (rótula) enganchada en Pivot; Muzzle con +X hacia fuera para el '
               'Niagara del chorro. Plantilla geyser_aim.'),
]
