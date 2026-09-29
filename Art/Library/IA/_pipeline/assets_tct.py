"""Todos contra Todos (Docs/2026-09-29-Plan-Maestro-Modos-y-Mapas.md §3.4): armas de mano y balón.

Escala: tortuga de ~115 cm de pie (asientos del buggy, turtle_pose.py); armas de 45-75 cm. Origen = punto de agarre
(socket Grip en el origen, la mano cierra alrededor del eje Y), cañón hacia +X, socket Muzzle en la boca.
"""
import math

from ia_mesh import Builder, axis_matrix, mirror_y, move, rot_x

CATEGORY = 'todos_contra_todos'
WOOD, WOOD_DARK = 0x8A5A3B, 0x5E3A26
GUNMETAL = 0x3E4148


def _grip(b, zone='dark', lean=4.0, bottom=-8.0, top=6.0, half_y=1.7, depth=4.2):
    """Empuñadura inclinada hacia atrás con el origen en el centro de la mano."""
    pts = []
    for z, dx in ((bottom, -lean * 0.6), (top, lean * 0.4)):
        for x in (dx - depth / 2, dx + depth / 2):
            pts += [(x, half_y, z), (x, -half_y, z)]
    pts += [(-lean * 0.6 - depth / 2 - 0.8, 0.0, bottom + 1.2)]  # talón
    b.hull(pts, zone)
    b.hull(mirror_y([(-lean * 0.6 - 2.8, half_y + 0.2, bottom - 1.2), (-lean * 0.6 + 2.6, half_y + 0.2, bottom - 1.2),
                     (-lean * 0.6 - 2.8, half_y + 0.2, bottom + 0.4), (-lean * 0.6 + 2.6, half_y + 0.2, bottom + 0.4)]),
           'trim', 0.9)  # pomo


def _trigger(b, x=2.2, z_top=5.5):
    b.hull(mirror_y([(x, 0.4, z_top), (x + 1.2, 0.4, z_top), (x + 0.2, 0.4, z_top - 3.2), (x + 1.0, 0.4, z_top - 3.0)]),
           'trim')
    b.sweep([(x + 4.5, 0, z_top + 0.5), (x + 4.5, 0, z_top - 3.5), (x + 2.5, 0, z_top - 5.0),
             (x - 1.0, 0, z_top - 4.5)], 0.45, 'trim', seg=4)


# ── Trabuco de aire ─────────────────────────────────────────────────────────

def trabuco_aire():
    b = Builder()
    _grip(b)
    _trigger(b)
    # Culata corta de madera detrás de la empuñadura.
    b.hull(mirror_y([(3.0, 2.0, 13.0), (3.0, 2.0, 5.0), (-4.0, 2.2, 13.0), (-20.0, 2.6, 9.0), (-22.0, 2.6, 7.5),
                     (-22.0, 2.6, -2.5), (-19.0, 2.4, -3.5), (-6.0, 2.0, 4.0)]), 'dark')
    b.box((-22.6, -2.8, -3.6), (-21.6, 2.8, 9.2), 'trim', 0.85)  # cantonera
    # Cajón (tintable) sobre la empuñadura.
    b.hull(mirror_y([(-4.0, 2.6, 8.0), (-4.0, 2.6, 13.5), (-2.5, 2.0, 15.0), (10.0, 2.8, 7.0), (10.0, 2.8, 14.0),
                     (9.0, 2.2, 15.2)]), 'paint')
    # Cañón acampanado de latón (torno a lo largo de +X), boca abierta hacia dentro.
    prof = [(0, 0), (3.2, 0), (3.2, 2.5), (2.6, 5), (2.5, 21), (3.3, 29), (5.6, 35), (6.4, 36.6), (5.1, 36.6), (0, 33)]
    bands = {1: ('paint', 1.0), 2: ('paint', 0.9), 7: ('trim', 1.0)}
    b.lathe(prof, 'detail', seg=10, origin=(8.0, 0, 11.0), axis=(1, 0, 0),
            zone_of_band=lambda band, s: bands.get(band, ('detail', 1.0 if s % 2 else 0.93)))
    for x in (15.0, 24.0):  # abrazaderas
        b.cyl((x, 0, 11.0), (x + 1.4, 0, 11.0), 3.0, 'trim', seg=10)
    # Fuelle de acordeón (tintable) bajo el cañón: el «aire» del trabuco.
    pleats = [(0, 0), (2.4, 0)]
    for i in range(6):
        pleats += [(3.6, 1.2 + i * 2.8), (2.6, 2.6 + i * 2.8)]
    pleats += [(2.6, 18.2), (0, 18.2)]
    b.lathe(pleats, 'paint', seg=8, origin=(5.0, 0, 3.0), axis=(1, 0, 0), phase=math.pi / 8,
            zone_of_band=lambda band, s: ('paint', 1.0 if band % 2 else 0.8))
    b.cyl((4.0, 0, 3.0), (5.2, 0, 3.0), 3.8, 'detail', seg=8, phase=math.pi / 8)
    b.cyl((23.0, 0, 3.0), (24.4, 0, 3.0), 3.8, 'detail', seg=8, phase=math.pi / 8)
    b.cyl((20.0, 0, 3.5), (20.0, 0, 9.0), 1.0, 'trim', seg=6)  # tubo de aire al cañón
    # Asa delantera del fuelle.
    b.hull(mirror_y([(22.5, 1.5, 1.0), (25.0, 1.5, 1.0), (22.0, 1.4, -6.0), (24.4, 1.4, -6.0)]), 'dark')
    # Manómetro sobre el cajón.
    b.cyl((3.5, 0, 14.4), (3.5, 0, 17.2), 2.8, 'trim', seg=10)
    b.cyl((3.5, 0, 17.2), (3.5, 0, 17.6), 2.2, 'light', seg=10)
    b.box((3.3, -0.25, 17.6), (5.3, 0.25, 17.9), 'trim')
    b.socket('Grip', (0, 0, 0))
    b.socket('Muzzle', (44.6, 0, 11.0))
    return b


# ── Pistola de noqueo + dardo de medusa ─────────────────────────────────────

def pistola_noqueo():
    b = Builder()
    _grip(b)
    _trigger(b)
    # Cuerpo bulboso tipo pistola de rayos (torno sobre +X).
    prof = [(0, 0), (3.0, 0), (4.4, 3), (5.0, 7.5), (4.6, 12), (3.4, 15.5), (2.2, 17.5), (0, 18)]
    b.lathe(prof, 'paint', seg=10, origin=(-9.0, 0, 10.0), axis=(1, 0, 0),
            zone_of_band=lambda band, s: ('detail', 1.0) if band == 3 else ('paint', 1.0 if band != 1 else 0.9))
    # Aletas traseras.
    for ang in (90.0, 210.0, 330.0):
        with b.frame(move(-6.0, 0, 10.0) @ rot_x(ang)):
            b.prism([(-4.0, 3.5), (3.0, 3.5), (-2.0, 8.0), (-5.0, 8.0)], 0.9, 'detail')
    # Cámara de cristal con el dardo dentro (zona luz: rosa emisivo).
    b.torus((0.5, 0, 14.2), (0, 0, 1), 3.9, 0.7, 'trim', major_seg=10, minor_seg=4)
    b.sphere((0.5, 0, 17.0), 4.2, 'light', seg=10, rings=6)
    # Cañón con anillos de rayos.
    b.cyl((8.0, 0, 10.0), (21.0, 0, 10.0), 1.8, 'trim', seg=8)
    for i, x in enumerate((11.0, 14.5, 18.0)):
        b.cyl((x, 0, 10.0), (x + 1.0, 0, 10.0), 3.6 - i * 0.5, 'detail', seg=10)
    b.cyl((21.0, 0, 10.0), (24.0, 0, 10.0), 1.8, 'light', seg=8, r1=2.8)
    b.socket('Grip', (0, 0, 0))
    b.socket('Muzzle', (24.0, 0, 10.0))
    return b


def dardo_medusa():
    """Proyectil: campana de medusa delante (+X) y tentáculos que hacen de aletas. Origen en el centro de masa."""
    b = Builder()
    prof = [(0, -1.2), (3.2, -0.6), (4.0, 0.8), (3.7, 2.6), (2.6, 4.2), (0, 5.2)]
    b.lathe(prof, 'light', seg=8, axis=(1, 0, 0),
            zone_of_band=lambda band, s: ('detail', 1.0) if band == 1 else ('light', 1.0 if s % 2 else 0.92))
    b.cyl((5.0, 0, 0), (8.5, 0, 0), 0.55, 'trim', seg=4, r1=0.0)  # aguijón
    for k in range(4):
        a = math.pi / 4 + k * math.pi / 2
        cy, cz = math.cos(a), math.sin(a)
        path = []
        for i in range(5):
            r = 2.2 + 0.6 * math.sin(i * 1.6)
            path.append((-0.6 - i * 3.0, cy * r, cz * r))
        b.sweep(path, [0.75, 0.65, 0.55, 0.45, 0.3], 'detail', seg=4)
    return b


# ── Pistola de tinta ────────────────────────────────────────────────────────

def pistola_tinta():
    b = Builder()
    _grip(b, zone='paint')
    _trigger(b)
    # Cuerpo alargado de pistola de agua.
    b.hull(mirror_y([(-10.0, 2.6, 6.0), (-11.0, 2.6, 12.0), (-9.0, 2.2, 14.0), (14.0, 2.6, 6.5), (15.0, 2.4, 12.5),
                     (13.0, 2.0, 14.0), (-4.0, 2.6, 5.0)]), 'paint')
    b.hull(mirror_y([(4.0, 2.7, 9.0), (13.0, 2.7, 9.0), (4.0, 2.7, 11.0), (13.5, 2.7, 11.0)]), 'detail', 0.95)
    # Depósito de tinta encima (zona dark = tinta), con tapas y soporte.
    b.cyl((-13.0, 0, 18.2), (3.0, 0, 18.2), 4.2, 'dark', seg=10, phase=math.pi / 10)
    b.cyl((-14.0, 0, 18.2), (-13.0, 0, 18.2), 4.6, 'trim', seg=10, phase=math.pi / 10)
    b.cyl((3.0, 0, 18.2), (4.0, 0, 18.2), 4.6, 'trim', seg=10, phase=math.pi / 10)
    b.cyl((4.0, 0, 18.2), (5.2, 0, 18.2), 1.6, 'detail', seg=6)  # tapón
    b.box((-8.0, -1.5, 13.5), (-2.0, 1.5, 14.8), 'trim')
    # Burbujas pintadas en el depósito.
    for x, y, z, r in ((-8.5, -3.6, 19.8, 1.0), (-5.0, -3.9, 17.0, 0.7), (-1.5, -3.5, 20.2, 0.55)):
        b.sphere((x, y, z), r, 'light', seg=6, rings=3)
    # Boquilla larga con punta de calamar.
    b.cyl((14.0, 0, 10.0), (27.0, 0, 10.0), 1.4, 'detail', seg=8)
    b.cyl((27.0, 0, 10.0), (31.0, 0, 10.0), 2.4, 'paint', seg=8, r1=1.2)
    for ang in (0.0, 120.0, 240.0):
        with b.frame(move(27.5, 0, 10.0) @ rot_x(ang)):
            b.prism([(-3.0, 1.8), (1.5, 1.8), (-2.0, 4.2)], 0.6, 'paint')
    # Bomba bajo la boquilla (se tira con la otra mano).
    b.cyl((8.0, 0, 4.2), (25.0, 0, 4.2), 1.1, 'trim', seg=6)
    b.hull(mirror_y([(15.0, 2.2, 2.5), (22.0, 2.2, 2.5), (15.0, 2.2, 5.8), (22.0, 2.2, 5.8), (16.0, 1.6, 1.4),
                     (21.0, 1.6, 1.4)]), 'dark')
    b.socket('Grip', (0, 0, 0))
    b.socket('Muzzle', (31.0, 0, 10.0))
    return b


# ── Garfio con ancla ────────────────────────────────────────────────────────

def garfio():
    b = Builder()
    _grip(b)
    _trigger(b)
    b.hull(mirror_y([(-24.0, 2.4, 13.0), (-24.0, 2.4, 1.0), (-18.0, 2.2, 13.0), (-6.0, 2.0, 9.0), (-18.0, 2.2, 5.0)]),
           'dark')  # culata
    b.cyl((-18.0, 0, 11.0), (10.0, 0, 11.0), 2.4, 'paint', seg=8, phase=math.pi / 8)
    b.cyl((10.0, 0, 11.0), (26.0, 0, 11.0), 3.2, 'trim', seg=10)
    b.torus((26.0, 0, 11.0), (1, 0, 0), 3.2, 0.8, 'detail', major_seg=10, minor_seg=4)
    # Carrete de cuerda en el costado izquierdo.
    b.cyl((0.0, 2.2, 6.5), (0.0, 7.4, 6.5), 1.0, 'trim', seg=6)
    b.cyl((0.0, 2.6, 6.5), (0.0, 3.2, 6.5), 5.0, 'trim', seg=10)
    b.cyl((0.0, 6.2, 6.5), (0.0, 6.8, 6.5), 5.0, 'trim', seg=10)
    b.cyl((0.0, 3.2, 6.5), (0.0, 6.2, 6.5), 4.0, 'detail', seg=10)
    b.hull([(0.0, 7.2, 6.0), (0.0, 7.2, 7.0), (5.0, 7.6, 2.0), (5.0, 7.6, 3.0),
            (0.0, 8.0, 6.0), (0.0, 8.0, 7.0), (5.0, 8.2, 2.0), (5.0, 8.2, 3.0)], 'trim')  # manivela
    b.cyl((5.2, 7.8, 2.4), (5.2, 10.8, 2.4), 0.8, 'dark', seg=6)
    b.socket('Grip', (0, 0, 0))
    b.socket('Muzzle', (27.0, 0, 11.0))
    return b


def ancla():
    """Proyectil del garfio: brazos hacia +X para engancharse; socket Rope en la argolla (-X)."""
    b = Builder()
    b.cyl((-10.0, 0, 0), (9.0, 0, 0), 1.2, 'trim', seg=6)
    b.torus((-12.2, 0, 0), (0, 1, 0), 2.1, 0.55, 'trim', major_seg=8, minor_seg=4)
    b.cyl((-8.0, -6.5, 0), (-8.0, 6.5, 0), 0.8, 'dark', seg=6)  # cepo
    for y in (-6.5, 6.5):
        b.sphere((-8.0, y, 0), 1.2, 'dark', seg=6, rings=3)
    b.sphere((9.2, 0, 0), 1.9, 'trim', seg=6, rings=4)  # cruz
    for side in (1, -1):
        path = [(9.2, 0, 0), (9.6, 0, side * 3.0), (8.4, 0, side * 6.2), (5.8, 0, side * 8.4), (2.5, 0, side * 9.2)]
        b.sweep(path, [1.3, 1.2, 1.1, 1.0, 0.9], 'trim', seg=6)
        b.hull([(2.0, 0.5, side * 9.0), (2.0, -0.5, side * 9.0), (5.5, 2.6, side * 10.4), (5.5, -2.6, side * 10.4),
                (-0.8, 0.4, side * 10.6), (-0.8, -0.4, side * 10.6), (4.5, 0.4, side * 12.6),
                (4.5, -0.4, side * 12.6)], 'paint')  # uña
    b.socket('Rope', (-14.3, 0, 0), (0, 0, 180.0))
    return b


# ── Pala de mano ────────────────────────────────────────────────────────────

def pala_mano():
    """Pala de playa de juguete: mango en D (origen en el travesaño), hoja hacia +X; Muzzle = punta de golpe."""
    b = Builder()
    b.cyl((0, -4.2, 0), (0, 4.2, 0), 1.35, 'paint', seg=8)
    for side in (1, -1):
        b.sweep([(0, side * 4.2, 0), (2.5, side * 4.0, 0), (6.0, side * 2.2, 0), (8.5, side * 0.6, 0)],
                 1.15, 'paint', seg=6)
    b.cyl((7.5, 0, 0), (42.0, 0, 0), 1.5, 'dark', seg=8)
    b.cyl((40.0, 0, 0), (45.5, 0, 0), 2.1, 'paint', seg=8, r1=2.6)
    # Hoja cóncava: loft de secciones curvadas a lo largo de X.
    sections = [(44.5, 5.5, 0.6), (48.0, 9.0, 1.4), (56.0, 10.0, 2.0), (64.0, 9.0, 2.0), (69.0, 5.8, 1.4),
                (71.5, 1.5, 0.6)]
    rings = []
    for x, half_w, curl in sections:
        top, bot = [], []
        for i in range(7):
            t = -1.0 + 2.0 * i / 6
            y, z = t * half_w, curl * t * t
            top.append((x, y, z + 0.5))
            bot.append((x, y, z - 0.5))
        rings.append(top + list(reversed(bot)))
    b.loft(rings, 'detail', zone_of_band=lambda band, s: ('detail', 1.0 if s < 6 else 0.88))
    b.socket('Grip', (0, 0, 0))
    b.socket('Muzzle', (71.5, 0, 0.8))
    return b


# ── Balón de playa ──────────────────────────────────────────────────────────

GORES = ('paint', 'trim', 'dark', 'trim', 'detail', 'trim')


def balon_playa():
    b = Builder()
    rings = 9

    def zone(band, s):
        if band == 0 or band == rings - 1:
            return ('trim', 0.95)
        return (GORES[(s // 2) % 6], 1.0)

    b.sphere((0, 0, 0), 25.0, 'trim', seg=12, rings=rings, zone_of_band=zone)
    b.cyl((0, 0, 24.6), (0, 0, 26.2), 1.3, 'detail', seg=6)  # válvula en el polo blanco
    b.socket('Grip', (0, 0, 0))
    return b


def _weapon(slug, title, prompt, palette, gun, budget, extra=None, notes=''):
    parts = [dict(name='SM_TN_' + gun.__name__.title().replace('_', ''), build=gun, budget=budget[0],
                  pivot='grip', role='arma (malla en la mano)', required_sockets=('Grip', 'Muzzle'))]
    if extra:
        parts.append(extra)
    return dict(slug=slug, title=title, category=CATEGORY, budget=sum(budget), prompt=prompt, palette=palette,
                parts=parts, notes=notes)


ASSETS = [
    _weapon('trabuco_aire', 'Trabuco de aire',
            'Stylized low-poly toy blunderbuss powered by an accordion air bellows, flared brass bell muzzle, short '
            'wooden stock, pressure gauge on top, flat-shaded faceted, beach-toy proportions, single material with '
            'color zones, no textures',
            {'paint': 0x2EA8A0, 'detail': 0xE0B04A, 'dark': WOOD, 'trim': GUNMETAL, 'light': 0xFFF4C2},
            trabuco_aire, (1500,),
            notes='Cono corto de aire: el socket Muzzle marca el centro de la campana.'),
    _weapon('pistola_noqueo', 'Pistola de noqueo con dardo de medusa',
            'Stylized low-poly retro ray-gun toy pistol with a glass dome chamber holding a jellyfish dart, three '
            'tail fins, ringed barrel, flat-shaded, single material with color zones; plus a separate jellyfish '
            'dart projectile (bell forward, tentacles as fins)',
            {'paint': 0x7B5CD6, 'detail': 0xFFC857, 'dark': 0x2B2833, 'trim': 0x80878C, 'light': 0xFF8FC8},
            pistola_noqueo, (1500, 300),
            extra=dict(name='SM_TN_DardoMedusa', build=dardo_medusa, budget=300, pivot='center',
                       role='proyectil (origen en el centro, +X delante)', preview_loc=(38.0, 0, 10.0)),
            notes='El dardo es un proyectil lento y visible: zona Light emisiva. En la lámina aparece delante de la boca.'),
    _weapon('pistola_tinta', 'Pistola de tinta',
            'Stylized low-poly squid ink water pistol (super-soaker style) with a top ink tank, long nozzle ending '
            'in a squid-head tip, pump under the nozzle, painted bubbles, flat-shaded, single material',
            {'paint': 0xF26B8A, 'detail': 0xFFD23F, 'dark': 0x3B1F5E, 'trim': 0x5B6168, 'light': 0xB9A6FF},
            pistola_tinta, (1500,),
            notes='Zona Dark = tinta del depósito (morada); se puede cambiar por el color del efecto de tinta.'),
    _weapon('garfio_ancla', 'Garfio con ancla',
            'Stylized low-poly harpoon-style grappling launcher with a side rope reel and crank, wooden stock; plus '
            'a separate small iron anchor projectile with flukes forward and a rope ring at the back, flat-shaded',
            {'paint': 0xD9463B, 'detail': 0xD8C49A, 'dark': WOOD, 'trim': 0x4A4F57, 'light': 0xFFF4C2},
            garfio, (750, 450),
            extra=dict(name='SM_TN_Ancla', build=ancla, budget=450, pivot='center',
                       role='proyectil (socket Rope para el UCableComponent)', preview_loc=(40.0, 0, 11.0),
                       required_sockets=('Rope',)),
            notes='Presupuesto total 1200 (lanzador 750 + ancla 450). La cuerda la pone UCableComponent entre '
                  'Muzzle y Rope.'),
    _weapon('pala_mano', 'Pala de mano',
            'Stylized low-poly beach toy spade with D-handle, wooden shaft and a slightly concave plastic blade, '
            'flat-shaded, single material with color zones',
            {'paint': 0x2F80ED, 'detail': 0xFFD23F, 'dark': 0xB98552, 'trim': 0x4A4F57, 'light': 0xFFF4C2},
            pala_mano, (800,),
            notes='Arma cuerpo a cuerpo: Muzzle = punta de la hoja (origen del barrido de golpe, alcance 1,5 m).'),
    dict(slug='balon_playa', title='Balón de playa', category=CATEGORY, budget=400,
         prompt='Stylized low-poly inflatable beach ball, six gores red/white/blue/white/yellow/white with white '
                'polar caps and a valve, flat-shaded, single material',
         palette={'paint': 0xE8412F, 'detail': 0xFFD23F, 'dark': 0x2E6FE0, 'trim': 0xF7F4EA, 'light': 0xFFFFFF},
         parts=[dict(name='SM_TN_BalonPlaya', build=balon_playa, budget=400, pivot='center',
                     role='lanzable (origen en el centro)', required_sockets=('Grip',), size_cm=[(49, 51), (49, 51), (49, 53)])],
         notes='Diámetro 50 cm. Aquí Trim = blanco (gajos y casquetes).'),
]
