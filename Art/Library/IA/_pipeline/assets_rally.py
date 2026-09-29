"""Rally Tortuga (Docs/Rally_E01B_y_Biplaza.md): caja de ítems, caracola de la artillera, rampa, meta/salida y checkpoint.

Escala: buggy biplaza de 4,2 × 2,45 × 1,4 m (Art/Source/Vehicles/Buggy); pistas de 12-14 m de ancho (§2.5), así que
los pórticos dejan 14 m libres. Origen en la base salvo la caracola (objeto de mano: origen en el agarre).
"""
import math

from ia_mesh import Builder, mirror_y, move, rot_x, rot_y, rot_z

CATEGORY = 'rally'
TYRE, TYRE_GROOVE = 0x2B2833, 0x4A4F57


# ── Caja de ítems ───────────────────────────────────────────────────────────
BOX, CHAMFER = 90.0, 12.0


def _question_mark(b, zone):
    """Signo «?» en relieve sobre la cara +X de la caja (local: u -> -Y, v -> Z)."""
    face_x = BOX / 2 + 1.5
    cz = BOX / 2
    path = []
    for i in range(8):
        a = math.radians(160 - i * 38)
        path.append((face_x, -(math.cos(a) * 13), cz + 12 + math.sin(a) * 13))
    path += [(face_x, 0.0, cz + 1.0), (face_x, 0.0, cz - 5.0)]
    b.sweep(path, 3.6, zone, seg=4, phase=math.pi / 4)
    b.box((face_x - 2.5, -4.0, cz - 17.0), (face_x + 1.5, 4.0, cz - 10.0), zone)


def caja_items():
    b = Builder()
    h, c = BOX / 2, CHAMFER
    pts = []
    for sx in (-1, 1):
        for sy in (-1, 1):
            for sz in (-1, 1):
                pts += [(sx * (h - c), sy * h, h + sz * h), (sx * h, sy * (h - c), h + sz * h),
                        (sx * h, sy * h, h + sz * (h - c))]
    b.hull(pts, 'paint')
    for sx in (-1, 1):  # aristas claras
        for sy in (-1, 1):
            b.box((sx * (h - 2) - 2.2, sy * (h - 2) - 2.2, c), (sx * (h - 2) + 2.2, sy * (h - 2) + 2.2, BOX - c), 'trim')
    for k in range(4):
        with b.frame(move(0, 0, 0) @ rot_z(90 * k)):
            _question_mark(b, 'light')
    with b.frame(move(0, 0, BOX + 1.0) @ rot_x(90)):
        star = []
        for i in range(10):
            a = math.pi / 2 + math.pi * i / 5
            r = 24.0 if i % 2 == 0 else 10.0
            star.append((math.cos(a) * r, math.sin(a) * r))
        b.prism(star, 2.4, 'detail')
    b.col_box((-h, -h, 0), (h, h, BOX))
    return b


# ── Caracola (bocina de la artillera) ───────────────────────────────────────

def caracola():
    """Caracola: la tortuga sopla por la punta (-X); la boca mira a +X (Muzzle). Origen en el agarre."""
    b = Builder()
    stations = [  # (x, z del centro, radio y, radio z)
        (-22.0, 3.0, 1.6, 1.6), (-19.0, 2.4, 2.8, 2.6), (-15.0, 1.6, 4.6, 4.2), (-10.0, 0.6, 7.2, 6.4),
        (-4.0, 0.0, 10.0, 9.0), (2.0, -0.4, 11.2, 10.0), (8.0, -0.6, 11.4, 10.6), (13.0, -0.6, 12.8, 12.0),
        (15.0, -0.6, 13.8, 13.2), (13.5, -0.6, 10.2, 9.6), (8.0, -0.6, 7.0, 6.4), (4.0, -0.6, 3.0, 2.8),
    ]
    seg = 10
    rings = [[(x, math.cos(a) * ry, z + math.sin(a) * rz) for a in (2 * math.pi * s / seg for s in range(seg))]
             for x, z, ry, rz in stations]

    def zone(band, s):
        if band >= 8:
            return ('light', 1.0 if band == 9 else 0.8)
        if band == 7:
            return ('detail', 1.0)
        return ('paint', 1.0 if s % 2 else 0.93)
    b.loft(rings, 'paint', zone_of_band=zone)
    # Cresta en espiral desde la punta hasta el hombro: lo que hace que se lea como caracola.
    helix = []
    for i in range(22):
        t = i / 21
        x = -21.0 + 19.0 * t
        k = next(j for j in range(len(stations) - 1) if stations[j][0] <= x <= stations[j + 1][0])
        (x0, z0, ry0, rz0), (x1, z1, ry1, rz1) = stations[k], stations[k + 1]
        f = (x - x0) / (x1 - x0)
        ry, rz, zc = ry0 + (ry1 - ry0) * f, rz0 + (rz1 - rz0) * f, z0 + (z1 - z0) * f
        a = t * 3.0 * 2 * math.pi
        helix.append((x, math.cos(a) * (ry + 0.3), zc + math.sin(a) * (rz + 0.3)))
    b.sweep(helix, [0.5 + 1.1 * i / 21 for i in range(22)], 'detail', seg=4)
    for k in range(6):  # púas del hombro
        a = 2 * math.pi * k / 6 + 0.4
        base = (-3.0, math.cos(a) * 9.4, math.sin(a) * 8.4)
        tip = (-1.0, math.cos(a) * 14.5, math.sin(a) * 13.0)
        b.cyl(base, tip, 2.2, 'detail', seg=5, r1=0.0)
    b.cyl((-25.0, 0, 3.4), (-21.5, 0, 3.0), 1.3, 'trim', seg=6, r1=1.7)  # boquilla
    b.socket('Grip', (0, 0, 0))
    b.socket('Muzzle', (15.5, 0, -0.6))
    return b


# ── Rampa de salto ──────────────────────────────────────────────────────────
RAMP_L, RAMP_W, RAMP_DEG = 800.0, 600.0, 14.0


def rampa_salto():
    b = Builder()
    top = RAMP_L * math.tan(math.radians(RAMP_DEG))
    deck = 12.0
    half = RAMP_W / 2
    # Largueros laterales (triángulos) y postes.
    for side in (1, -1):
        with b.frame(move(0, side * (half - 8), 0)):
            b.prism([(0, 0), (RAMP_L, 0), (RAMP_L, top - 2)], 16.0, 'dark')
        for x in (250.0, 500.0, 780.0):
            zt = x * math.tan(math.radians(RAMP_DEG))
            b.box((x - 10, side * (half - 30) - 10, 0), (x + 10, side * (half - 30) + 10, zt - 2), 'dark', 0.85)
    for x in (400.0, 780.0):
        b.box((x - 8, -half + 20, 30), (x + 8, half - 20, 46), 'dark', 0.8)
    # Tablero inclinado de tablones (a lo ancho) con chevrones.
    n = 10
    step = RAMP_L / math.cos(math.radians(RAMP_DEG)) / n
    with b.frame(rot_y(-RAMP_DEG)):
        for i in range(n):
            zone = 'paint' if i % 2 == 0 else 'detail'
            b.box((i * step + 1.5, -half, 0), ((i + 1) * step - 1.5, half, deck), zone, 1.0 if i % 2 else 0.95)
        for k in range(3):
            u = 180.0 + k * 230.0
            with b.frame(move(u, 0, deck + 0.8) @ rot_x(90)):
                b.prism([(-40, -160), (40, -160), (120, 0), (40, 160), (-40, 160), (40, 0)], 1.6, 'light')
        lip = RAMP_L / math.cos(math.radians(RAMP_DEG))
        b.box((lip - 12, -half, -2), (lip + 2, half, deck + 3), 'trim')
    b.box((-30, -half, 0), (8, half, 4), 'trim', 0.9)  # chapa de entrada
    b.col_hull([(x, y, z) for y in (-half, half) for x, z in ((-30, 0), (RAMP_L, 0), (RAMP_L, top + deck))])
    return b


# ── Pórtico de meta y de salida (arco de neumático) ─────────────────────────
ARCH_R, TUBE_R = 760.0, 48.0


def _tyre_arch(b):
    seg = 10
    path = [(0.0, ARCH_R * math.cos(math.radians(a)), ARCH_R * math.sin(math.radians(a))) for a in range(0, 181, 10)]

    def tread(band, s):
        if s in (2, 7):
            return ('paint', 1.0)  # franja del flanco
        return ('dark', 1.0) if (band + s) % 2 else ('trim', 0.9)
    b.sweep(path, TUBE_R, 'dark', seg=seg, zone_of_band=tread, end_dirs=((0, 0, 1), (0, 0, -1)))
    for side in (1, -1):  # neumáticos apilados en las patas
        for i, z in enumerate((28.0, 84.0)):
            b.torus((0, side * ARCH_R, z), (0, 0, 1), 72.0, 28.0, 'dark', major_seg=10, minor_seg=4,
                    phase=0.3 * i, minor_phase=math.pi / 4)
        b.col_box((-100, side * ARCH_R - 100, 0), (100, side * ARCH_R + 100, 260))


def _banner_ropes(b, half, z_low, z_high):
    for side in (1, -1):
        y = side * half
        arch_z = math.sqrt(ARCH_R ** 2 - (y * 1.02) ** 2)
        b.box((-3, y - 3, z_high), (3, y + 3, arch_z), 'trim')


def portico_meta():
    b = Builder()
    _tyre_arch(b)
    half, z0, z1 = 470.0, 500.0, 590.0
    with b.frame(rot_z(90)):
        b.slab_grid(-half, half, z0, z1, 6.0, 16, 2, lambda i, j: 'detail' if (i + j) % 2 else 'trim', side_zone='trim')
    _banner_ropes(b, half - 20, z0, z1)
    b.box((-10, -60, z1), (10, 60, z1 + 40), 'light')  # marcador de tiempo
    b.socket('FinishLine', (0, 0, 0))
    return b


def portico_salida():
    b = Builder()
    _tyre_arch(b)
    half, z0, z1 = 470.0, 500.0, 590.0
    with b.frame(rot_z(90)):
        b.slab_grid(-half, half, z0, z1, 6.0, 8, 1, lambda i, j: 'paint' if i % 2 else 'detail', side_zone='trim')
    _banner_ropes(b, half - 20, z0, z1)
    for y in (-70.0, 0.0, 70.0):  # semáforo de salida
        b.box((-12, y - 26, z0 - 64), (12, y + 26, z0), 'trim')
        b.sphere((13.0, y, z0 - 32), 17.0, 'light', seg=8, rings=4)
    b.socket('StartLine', (0, 0, 0))
    b.socket('StartLights', (14.0, 0, z0 - 32))
    return b


# ── Checkpoint ──────────────────────────────────────────────────────────────
CP_HALF, POLE_H = 700.0, 500.0


def checkpoint():
    b = Builder()
    for side in (1, -1):
        y = side * CP_HALF
        b.sphere((0, y, 31.2), 52.0, 'paint', seg=8, rings=4, squash=0.6, phase=math.pi / 8)
        for i in range(4):
            z0 = 40.0 + i * (POLE_H - 40) / 4
            b.cyl((0, y, z0), (0, y, z0 + (POLE_H - 40) / 4), 7.0, 'paint' if i % 2 else 'trim', seg=8)
        b.sphere((0, y, POLE_H + 14), 16.0, 'light', seg=8, rings=4)
        with b.frame(move(0, y + side * 6, POLE_H - 10) @ rot_z(90)):  # banderín hacia fuera, de cara al piloto
            b.prism([(0, 0), (0, -70), (side * 90, -35)], 2.0, 'detail')
    # Cuerda con banderines en catenaria.
    path, n = [], 12
    for i in range(n + 1):
        t = i / n
        y = -CP_HALF + 2 * CP_HALF * t
        path.append((0, y, POLE_H - 20 - 70 * (1 - (2 * t - 1) ** 2)))
    b.sweep(path, 2.2, 'trim', seg=4)
    for i in range(1, n):
        x, y, z = path[i]
        with b.frame(move(0, y, z) @ rot_z(90)):
            b.prism([(-26, 0), (26, 0), (0, -48)], 1.5, 'detail' if i % 2 else 'paint')
    b.socket('Gate', (0, 0, 0))
    for side in (1, -1):
        b.col_box((-40, side * CP_HALF - 40, 0), (40, side * CP_HALF + 40, POLE_H))
    return b


ASSETS = [
    dict(slug='caja_items', title='Caja de ítems del Rally', category=CATEGORY, budget=600,
         prompt='Stylized low-poly rally item box: chamfered cube with light edges, glowing raised question mark on '
                'each side and a starfish on top, flat-shaded, single material',
         palette={'paint': 0x2F80ED, 'detail': 0xFFD23F, 'dark': 0x2B2833, 'trim': 0xD5ECFF, 'light': 0xFFF08A},
         parts=[dict(name='SM_TN_CajaItemsRally', build=caja_items, budget=600, pivot='base', role='pickup')],
         notes='90 cm de lado, origen en el centro de la base (gira en Z). Filas de 4 cajas a 3 m (§2.5). Zona '
               'Paint = cuerpo (translúcido si el material lo permite), Light = interrogante emisivo.'),
    dict(slug='caracola', title='Bocina / caracola de la artillera', category=CATEGORY, budget=600,
         prompt='Stylized low-poly conch shell horn held by a turtle: spiral body with shoulder spikes, flared pink '
                'aperture facing forward, brass mouthpiece at the apex, flat-shaded',
         palette={'paint': 0xF4E1C6, 'detail': 0xD9A27A, 'dark': 0x6B4A33, 'trim': 0xD9A441, 'light': 0xF6A5B8},
         parts=[dict(name='SM_TN_Caracola', build=caracola, budget=600, pivot='grip', role='objeto de mano',
                     required_sockets=('Grip', 'Muzzle'))],
         notes='Acción A3 (cono de 60° y 30 m): Muzzle marca el origen y la dirección (+X) del cono.'),
    dict(slug='rampa_salto', title='Rampa de salto', category=CATEGORY, budget=800,
         prompt='Stylized low-poly wooden kicker ramp for buggies, 14 degrees, alternating painted planks with '
                'glowing chevrons, side stringers and posts, metal lip, flat-shaded',
         palette={'paint': 0xE4572E, 'detail': 0xF4EFE2, 'dark': 0x8A5A3B, 'trim': 0x5B6168, 'light': 0xFFE45C},
         parts=[dict(name='SM_TN_RampaSalto', build=rampa_salto, budget=800, pivot='base', role='prop con colisión',
                     size_cm=[(820, 845), (595, 605), (195, 225)])],
         notes='8 × 6 m, 14° (sube hacia +X; el borde de entrada está en X = -30). La rampa del trazado E01B '
               '(14° en 25 m) es terreno; esta es la pieza colocable. UCX = cuña única.'),
    dict(slug='portico_meta_salida', title='Meta y pórtico de salida (arco de neumático)', category=CATEGORY,
         budget=2000,
         prompt='Stylized low-poly finish arch shaped like a giant half tyre with tread, stacked tyres at the feet, '
                'hanging checkered banner; and a start arch with striped banner and three start lights, flat-shaded',
         palette={'paint': 0xE4572E, 'detail': 0xF4EFE2, 'dark': TYRE, 'trim': TYRE_GROOVE, 'light': 0xFFE45C},
         parts=[dict(name='SM_TN_PorticoMeta', build=portico_meta, budget=1000, pivot='base', role='meta',
                     required_sockets=('FinishLine',), size_cm=[(90, 210), (1600, 1720), (800, 815)]),
                dict(name='SM_TN_PorticoSalida', build=portico_salida, budget=1000, pivot='base', role='salida',
                     required_sockets=('StartLine', 'StartLights'), preview_loc=(-1400, 0, 0))],
         notes='Hueco libre ≈ 14 m (pistas de 12-14 m). Arco de 8 m de alto; banderola a 5,0-5,9 m. En la lámina la '
               'salida aparece detrás de la meta.'),
    dict(slug='checkpoint', title='Checkpoint', category=CATEGORY, budget=800,
         prompt='Stylized low-poly rally checkpoint gate: two striped poles on buoy bases with glowing lamps and '
                'flags, a sagging rope of pennants between them, flat-shaded',
         palette={'paint': 0xFF7A1A, 'detail': 0xFFD23F, 'dark': 0x2B2833, 'trim': 0xF4EFE2, 'light': 0x7CFFB0},
         parts=[dict(name='SM_TN_Checkpoint', build=checkpoint, budget=800, pivot='base', role='puerta de control',
                     required_sockets=('Gate',))],
         notes='Postes a ±7 m (14 m libres); socket Gate en el centro del paso para el trigger del checkpoint.'),
]
