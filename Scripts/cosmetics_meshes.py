"""Cascos de la tienda de Tortunavy: mallas low-poly de caras planas con color de vértice.

Python puro (sin unreal ni numpy): lo usan Scripts/build_cosmetics.py (dentro del editor, para crear los
SM_Helmet_*) y cualquier visor externo. Cada receta devuelve una lista de triángulos
(A, B, C, color sRGB 0xRRGGBB, brillo 0-1) con la cara visible ya orientada.

Espacio de diseño = espacio de componente de la malla de la tortuga (TotugaDemo_Rig, antes del escalado 2.5 del
personaje): X a la derecha de la tortuga, +Y hacia donde mira, Z arriba; el origen es la coronilla (el ancla
HeadTop de UTN_CosmeticLook, (0, 5.5, 51) en la postura de referencia). La cabeza, vista desde ahí: elipsoide de
radios 8 (X) y 9 (Y) con el centro 7 por debajo; los ojos saltones llegan a Z -1.4 por delante (Y 0-7).
"""

import math

TWO_PI = 2.0 * math.pi


# ── Vectores ─────────────────────────────────────────────────────────────────

def add(a, b):
    return (a[0] + b[0], a[1] + b[1], a[2] + b[2])


def sub(a, b):
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def mul(a, s):
    return (a[0] * s, a[1] * s, a[2] * s)


def dot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def norm(a):
    length = math.sqrt(dot(a, a))
    return (a[0] / length, a[1] / length, a[2] / length) if length > 1e-9 else (0.0, 0.0, 1.0)


def lerp(a, b, t):
    return add(a, mul(sub(b, a), t))


def rot_x(p, deg):
    """Giro alrededor de X: con grados positivos el frente (+Y) sube."""
    c, s = math.cos(math.radians(deg)), math.sin(math.radians(deg))
    return (p[0], p[1] * c - p[2] * s, p[1] * s + p[2] * c)


def rot_y(p, deg):
    """Giro alrededor de Y: con grados positivos la derecha (+X) baja."""
    c, s = math.cos(math.radians(deg)), math.sin(math.radians(deg))
    return (p[0] * c + p[2] * s, p[1], -p[0] * s + p[2] * c)


def rot_z(p, deg):
    c, s = math.cos(math.radians(deg)), math.sin(math.radians(deg))
    return (p[0] * c - p[1] * s, p[0] * s + p[1] * c, p[2])


def shade(rgb, f):
    """Aclara (f > 1) u oscurece (f < 1) un color sRGB 0xRRGGBB."""
    r, g, b = (rgb >> 16) & 255, (rgb >> 8) & 255, rgb & 255
    r, g, b = (min(255, max(0, int(round(v * f)))) for v in (r, g, b))
    return (r << 16) | (g << 8) | b


def hash01(*ints):
    h = 2166136261
    for i in ints:
        h = ((h ^ (i & 0xFFFFFFFF)) * 16777619) & 0xFFFFFFFF
    h ^= h >> 13
    h = (h * 0x5bd1e995) & 0xFFFFFFFF
    h ^= h >> 15
    return (h & 0xFFFF) / 65535.0


# ── Malla ────────────────────────────────────────────────────────────────────

class Mesh:
    def __init__(self):
        self.tris = []

    def tri(self, a, b, c, hint, color, shine=0.0):
        n = cross(sub(b, a), sub(c, a))
        if dot(n, n) < 1e-10:
            return
        if dot(n, hint) < 0.0:
            b, c = c, b
        self.tris.append((a, b, c, color, shine))

    def quad(self, a, b, c, d, hint, color, shine=0.0):
        self.tri(a, b, c, hint, color, shine)
        self.tri(a, c, d, hint, color, shine)

    def extend(self, other):
        self.tris.extend(other.tris)

    def transformed(self, fn):
        out = Mesh()
        out.tris = [(fn(a), fn(b), fn(c), col, sh) for (a, b, c, col, sh) in self.tris]
        return out

    # ── Cuerpos ──

    def loft(self, rings, color, shine=0.0, closed=True, tone=None):
        """Une anillos consecutivos (listas de puntos); cada cara mira fuera del eje de su anillo.

        color puede ser una función (índice de anillo, índice de lado) -> color.
        """
        for i in range(len(rings) - 1):
            r0, r1 = rings[i], rings[i + 1]
            c0 = mul(tuple(map(sum, zip(*r0))), 1.0 / len(r0))
            c1 = mul(tuple(map(sum, zip(*r1))), 1.0 / len(r1))
            axis_c = lerp(c0, c1, 0.5)
            count = len(r0) if closed else len(r0) - 1
            for k in range(count):
                k1 = (k + 1) % len(r0)
                mid = mul(add(add(r0[k], r0[k1]), add(r1[k], r1[k1])), 0.25)
                col = color(i, k) if callable(color) else color
                if tone:
                    col = shade(col, 1.0 + tone * (hash01(i, k, 7) - 0.5))
                self.quad(r0[k], r0[k1], r1[k1], r1[k], sub(mid, axis_c), col, shine)

    def fan(self, ring, apex, hint, color, shine=0.0):
        for k in range(len(ring)):
            self.tri(apex, ring[k], ring[(k + 1) % len(ring)], hint, color, shine)

    def lathe(self, profile, seg, color, shine=0.0, center=(0.0, 0.0, 0.0), squash=(1.0, 1.0), top=True, bottom=False,
              tone=None, phase=0.0):
        """Cuerpo de revolución: profile = [(radio, z)...] de abajo arriba; squash escala X e Y."""
        rings = []
        for (r, z) in profile:
            ring = []
            for k in range(seg):
                a = TWO_PI * (k + phase) / seg
                ring.append((center[0] + math.cos(a) * r * squash[0], center[1] + math.sin(a) * r * squash[1], center[2] + z))
            rings.append(ring)
        self.loft(rings, color, shine, True, tone)
        if top and profile[-1][0] > 1e-6:
            apex = (center[0], center[1], center[2] + profile[-1][1])
            col = color(len(profile) - 1, 0) if callable(color) else color
            self.fan(rings[-1], apex, (0.0, 0.0, 1.0), col, shine)
        if bottom and profile[0][0] > 1e-6:
            apex = (center[0], center[1], center[2] + profile[0][1])
            col = color(0, 0) if callable(color) else color
            self.fan(rings[0], apex, (0.0, 0.0, -1.0), shade(col, 0.8), shine)
        return rings

    def cylinder(self, a, b, ra, rb, seg, color, shine=0.0, caps=True):
        ax = norm(sub(b, a))
        u = norm(cross(ax, (0.0, 0.0, 1.0) if abs(ax[2]) < 0.9 else (1.0, 0.0, 0.0)))
        v = cross(ax, u)
        ring_a, ring_b = [], []
        for k in range(seg):
            ang = TWO_PI * k / seg
            off = add(mul(u, math.cos(ang)), mul(v, math.sin(ang)))
            ring_a.append(add(a, mul(off, ra)))
            ring_b.append(add(b, mul(off, rb)))
        for k in range(seg):
            k1 = (k + 1) % seg
            mid = mul(add(ring_a[k], ring_a[k1]), 0.5)
            self.quad(ring_a[k], ring_a[k1], ring_b[k1], ring_b[k], sub(mid, a), color, shine)
        if caps:
            if ra > 1e-6:
                self.fan(ring_a, a, mul(ax, -1.0), shade(color, 0.9), shine)
            if rb > 1e-6:
                self.fan(ring_b, b, ax, shade(color, 1.05), shine)

    def sphere(self, c, r, color, shine=0.0, seg=8, rings=5, squash=(1.0, 1.0, 1.0)):
        pts = []
        for i in range(1, rings):
            t = math.pi * i / rings
            ring = []
            for k in range(seg):
                a = TWO_PI * k / seg
                ring.append((c[0] + math.sin(t) * math.cos(a) * r * squash[0],
                             c[1] + math.sin(t) * math.sin(a) * r * squash[1],
                             c[2] + math.cos(t) * r * squash[2]))
            pts.append(ring)
        top = (c[0], c[1], c[2] + r * squash[2])
        bot = (c[0], c[1], c[2] - r * squash[2])
        for k in range(seg):
            k1 = (k + 1) % seg
            self.tri(top, pts[0][k], pts[0][k1], sub(mul(add(pts[0][k], pts[0][k1]), 0.5), c), color, shine)
            self.tri(bot, pts[-1][k], pts[-1][k1], sub(mul(add(pts[-1][k], pts[-1][k1]), 0.5), c), shade(color, 0.85), shine)
        for i in range(len(pts) - 1):
            for k in range(seg):
                k1 = (k + 1) % seg
                mid = mul(add(add(pts[i][k], pts[i][k1]), add(pts[i + 1][k], pts[i + 1][k1])), 0.25)
                self.quad(pts[i][k], pts[i][k1], pts[i + 1][k1], pts[i + 1][k], sub(mid, c), color, shine)

    def box(self, c, half, color, shine=0.0, rot=None):
        """Caja centrada en c; rot = función que gira un vector (se aplica antes de trasladar)."""
        rot = rot or (lambda p: p)

        def P(sx, sy, sz):
            return add(c, rot((sx * half[0], sy * half[1], sz * half[2])))

        faces = [
            ((-1, -1, 1), (1, -1, 1), (1, 1, 1), (-1, 1, 1), (0, 0, 1), 1.05),
            ((-1, -1, -1), (-1, 1, -1), (1, 1, -1), (1, -1, -1), (0, 0, -1), 0.8),
            ((1, -1, -1), (1, 1, -1), (1, 1, 1), (1, -1, 1), (1, 0, 0), 0.95),
            ((-1, -1, -1), (-1, -1, 1), (-1, 1, 1), (-1, 1, -1), (-1, 0, 0), 0.95),
            ((-1, 1, -1), (-1, 1, 1), (1, 1, 1), (1, 1, -1), (0, 1, 0), 1.0),
            ((-1, -1, -1), (1, -1, -1), (1, -1, 1), (-1, -1, 1), (0, -1, 0), 0.9),
        ]
        for a, b, cc, d, n, f in faces:
            self.quad(P(*a), P(*b), P(*cc), P(*d), rot(n), shade(color, f), shine)

    def beam(self, a, b, half, color, shine=0.0):
        self.cylinder(a, b, half, half, 4, color, shine, caps=True)

    def star(self, c, r_out, r_in, n, thick, color, shine=0.0, droop=0.0, bump_color=None):
        """Estrella de n puntas tumbada (estrella de mar); droop baja las puntas para abrazar la cabeza."""
        top, bot = [], []
        for i in range(2 * n):
            a = math.pi * i / n + math.pi / 2.0
            r = r_out if i % 2 == 0 else r_in
            dz = -droop * (r / r_out) ** 2
            top.append((c[0] + math.cos(a) * r, c[1] + math.sin(a) * r, c[2] + dz + thick * (0.35 if i % 2 == 0 else 1.0)))
            bot.append((c[0] + math.cos(a) * r, c[1] + math.sin(a) * r, c[2] + dz))
        ctop = (c[0], c[1], c[2] + thick * 1.6)
        for i in range(2 * n):
            i1 = (i + 1) % (2 * n)
            self.tri(ctop, top[i], top[i1], (0, 0, 1), color, shine)
            mid = mul(add(top[i], top[i1]), 0.5)
            self.quad(bot[i], bot[i1], top[i1], top[i], sub(mid, c), shade(color, 0.85), shine)
        if bump_color is not None:
            for i in range(0, 2 * n, 2):
                for t in (0.35, 0.62):
                    p = lerp(ctop, top[i], t)
                    self.sphere(add(p, (0, 0, 0.25)), 0.55, bump_color, shine, seg=5, rings=3)


# ── Recetas ──────────────────────────────────────────────────────────────────
# Todas se construyen alrededor de la "banda" (donde el sombrero se agarra a la cabeza) y luego band_frame() las
# inclina hacia atrás y las baja a la cabeza.

BAND_RX = 7.7
BAND_RY = 8.5


def band_frame(m, tilt=14.0, down=4.2, fwd=0.4):
    """Del espacio de la banda al de la coronilla: inclina (frente arriba) y baja la banda a la cabeza."""
    return m.transformed(lambda p: add(rot_x(p, tilt), (0.0, fwd, -down)))


def ellipse_ring(rx, ry, z, seg, phase=0.0, cy=0.0):
    return [(math.cos(TWO_PI * (k + phase) / seg) * rx, cy + math.sin(TWO_PI * (k + phase) / seg) * ry, z) for k in range(seg)]


def straw_hat():
    m = Mesh()
    straw, straw_dark, straw_light, band = 0xE9C46A, 0xC9A04A, 0xF4D98C, 0xE4572E
    seg = 16
    # Ala: anillo ancho que cae un poco hacia fuera, con cara de abajo más oscura.
    inner = ellipse_ring(BAND_RX + 0.2, BAND_RY + 0.2, 0.6, seg)
    mid = ellipse_ring(13.0, 13.6, 0.4, seg)
    outer = ellipse_ring(17.0, 17.6, -0.9, seg)
    for ring_a, ring_b in ((inner, mid), (mid, outer)):
        for k in range(seg):
            k1 = (k + 1) % seg
            col = straw if (k % 2 == 0) else shade(straw, 0.94)
            m.quad(ring_a[k], ring_a[k1], ring_b[k1], ring_b[k], (0, 0, 1), col)
            lo = [(p[0], p[1], p[2] - 0.7) for p in (ring_a[k], ring_a[k1], ring_b[k1], ring_b[k])]
            m.quad(lo[0], lo[1], lo[2], lo[3], (0, 0, -1), straw_dark)
    # Canto del ala.
    outer_lo = [(p[0], p[1], p[2] - 0.7) for p in outer]
    m.loft([outer_lo, outer], shade(straw, 0.85))
    inner_lo = [(p[0], p[1], p[2] - 0.7) for p in inner]
    m.loft([inner, inner_lo], straw_dark)
    # Copa con cinta roja y la tapa algo hundida.
    m.lathe([(BAND_RX, 0.4), (BAND_RX + 0.25, 2.4), (BAND_RX - 0.2, 2.5)], seg, band, squash=(1.0, BAND_RY / BAND_RX), top=False)
    m.lathe([(BAND_RX - 0.2, 2.5), (BAND_RX - 0.6, 5.6), (BAND_RX - 1.2, 6.6), (4.0, 6.9)], seg,
            lambda i, k: straw_light if (i + k) % 2 == 0 else straw, squash=(1.0, BAND_RY / BAND_RX), top=True)
    # Lazo de la cinta a la derecha.
    m.box((BAND_RX + 0.3, 1.0, 1.5), (0.5, 1.2, 0.9), shade(band, 1.1))
    m.box((BAND_RX + 0.4, 2.6, 0.8), (0.4, 1.0, 1.2), band, rot=lambda p: rot_x(p, -30))
    return band_frame(m, tilt=10.0, down=3.6)


def pirate_hat():
    m = Mesh()
    felt, felt_dark, gold, bone = 0x2B2833, 0x1C1A22, 0xFFCB3D, 0xF4EFE2
    seg = 24
    # Copa redondeada.
    m.lathe([(BAND_RX + 0.1, 0.0), (BAND_RX + 0.3, 2.5), (BAND_RX - 0.3, 4.8), (5.2, 6.6), (2.6, 7.4)], seg, felt,
            squash=(1.0, BAND_RY / BAND_RX), top=True, tone=0.12)
    # Ala de tres picos vuelta hacia arriba: el radio crece hacia las tres esquinas (delante y atrás a los lados).
    def corner(a):
        # Esquinas en 90° (delante), 210° y 330°.
        best = min(abs(math.atan2(math.sin(a - c), math.cos(a - c))) for c in (math.radians(90), math.radians(210), math.radians(330)))
        return max(0.0, 1.0 - best / math.radians(60)) ** 1.6

    base, brim, rim_hi = [], [], []
    for k in range(seg):
        a = TWO_PI * k / seg
        cx, sy = math.cos(a), math.sin(a)
        base.append((cx * (BAND_RX + 0.2), sy * (BAND_RY + 0.2), 0.3))
        r = 11.0 + 4.2 * corner(a)
        brim.append((cx * r, sy * r * 1.05, 3.4 + 2.2 * (1.0 - corner(a))))
        rim_hi.append((cx * (r + 0.35), sy * (r + 0.35) * 1.05, 4.2 + 2.2 * (1.0 - corner(a))))
    for k in range(seg):
        k1 = (k + 1) % seg
        mid = mul(add(base[k], brim[k1]), 0.5)
        m.quad(base[k], base[k1], brim[k1], brim[k], (mid[0] * 0.3, mid[1] * 0.3, -1.0), felt_dark)
        m.quad(base[k], base[k1], brim[k1], brim[k], (-mid[0], -mid[1], 1.0), felt)
        # Galón dorado en el borde.
        m.quad(brim[k], brim[k1], rim_hi[k1], rim_hi[k], (mid[0], mid[1], 0.3), gold, 0.85)
        m.quad(brim[k], brim[k1], rim_hi[k1], rim_hi[k], (-mid[0], -mid[1], -0.3), shade(gold, 0.8), 0.85)
    # Calavera en el frente de la copa.
    m.sphere((0.0, BAND_RY + 0.35, 3.6), 1.25, bone, seg=8, rings=4, squash=(1.0, 0.45, 1.0))
    m.box((0.0, BAND_RY + 0.45, 2.3), (0.75, 0.35, 0.45), bone)
    m.sphere((-0.45, BAND_RY + 0.85, 3.75), 0.33, felt_dark, seg=5, rings=3)
    m.sphere((0.45, BAND_RY + 0.85, 3.75), 0.33, felt_dark, seg=5, rings=3)
    m.box((0.0, BAND_RY + 0.35, 1.3), (1.9, 0.25, 0.28), bone, rot=lambda p: rot_y(p, 35))
    m.box((0.0, BAND_RY + 0.35, 1.3), (1.9, 0.25, 0.28), bone, rot=lambda p: rot_y(p, -35))
    return band_frame(m, tilt=12.0, down=3.8)


def crown():
    m = Mesh()
    gold, gold_dark, velvet, ruby, sapphire, pearl = 0xFFC93C, 0xD99A1E, 0xB3243B, 0xE0243C, 0x2F6FE0, 0xFFF6E8
    seg = 18
    squash = (1.0, BAND_RY / BAND_RX)
    # Terciopelo que asoma dentro.
    m.lathe([(BAND_RX - 0.3, 1.0), (BAND_RX - 1.5, 4.6), (3.0, 6.2), (0.8, 6.5)], seg, velvet, squash=squash, top=True, tone=0.1)
    # Aro de oro con canto de perlas.
    m.lathe([(BAND_RX + 0.2, 0.0), (BAND_RX + 0.4, 0.5), (BAND_RX + 0.4, 3.2), (BAND_RX + 0.2, 3.5)], seg, gold, 0.95, squash=squash, top=False)
    inner = []
    for k in range(seg):
        a = TWO_PI * k / seg
        inner.append((math.cos(a) * (BAND_RX - 0.2), math.sin(a) * (BAND_RY - 0.2), 3.5))
    outer = [(math.cos(TWO_PI * k / seg) * (BAND_RX + 0.2), math.sin(TWO_PI * k / seg) * (BAND_RY + 0.2), 3.5) for k in range(seg)]
    for k in range(seg):
        k1 = (k + 1) % seg
        m.quad(outer[k], outer[k1], inner[k1], inner[k], (0, 0, 1), gold_dark, 0.95)
    for k in range(0, seg, 2):
        a = TWO_PI * k / seg
        m.sphere((math.cos(a) * (BAND_RX + 0.55), math.sin(a) * (BAND_RY + 0.55), 0.35), 0.45, pearl, 0.3, seg=5, rings=3)
    # Seis picos con gema en la punta y gemas en el aro.
    for j in range(6):
        a = TWO_PI * j / 6 + math.pi / 2.0
        ca, sa = math.cos(a), math.sin(a)
        base_c = (ca * (BAND_RX + 0.3), sa * (BAND_RY + 0.3), 3.3)
        tangent = (-sa, ca, 0.0)
        left = add(base_c, mul(tangent, 2.1))
        right = add(base_c, mul(tangent, -2.1))
        tip = (ca * (BAND_RX - 0.2), sa * (BAND_RY - 0.2), 8.0)
        out = (ca, sa, 0.25)
        inset = mul((ca, sa, 0.0), -0.9)
        m.tri(left, right, tip, out, gold, 0.95)
        m.tri(add(left, inset), add(right, inset), add(tip, inset), mul(out, -1.0), gold_dark, 0.95)
        m.quad(left, add(left, inset), add(tip, inset), tip, tangent, shade(gold, 0.85), 0.95)
        m.quad(right, add(right, inset), add(tip, inset), tip, mul(tangent, -1.0), shade(gold, 0.85), 0.95)
        m.sphere(add(tip, (0, 0, 0.55)), 0.7, pearl, 0.4, seg=6, rings=3)
        gem = ruby if j % 2 == 0 else sapphire
        m.sphere((ca * (BAND_RX + 0.75), sa * (BAND_RY + 0.75), 1.8), 0.85, gem, 0.6, seg=6, rings=3, squash=(1.0, 1.0, 1.2))
    return band_frame(m, tilt=8.0, down=3.2)


def captain_cap():
    m = Mesh()
    navy, white, white_shadow, black, gold = 0x1D2F5A, 0xF5F7FA, 0xD5DCE6, 0x17191F, 0xFFCB3D
    seg = 20
    squash = (1.0, BAND_RY / BAND_RX)
    m.lathe([(BAND_RX + 0.2, 0.0), (BAND_RX + 0.35, 2.8)], seg, navy, squash=squash, top=False, tone=0.08)
    # Plato blanco que se ensancha (más hacia delante) y tapa algo abombada.
    m.lathe([(BAND_RX + 0.35, 2.8), (BAND_RX + 1.9, 4.6), (BAND_RX + 2.3, 5.4)], seg, white_shadow, squash=(1.0, 1.18), top=False)
    m.lathe([(BAND_RX + 2.3, 5.4), (BAND_RX + 1.6, 6.1), (5.0, 6.6), (0.5, 6.8)], seg, white, squash=(1.0, 1.18), top=True)
    # Cordón dorado y visera negra por delante, un poco caída.
    cord = [(math.cos(TWO_PI * k / 16) * (BAND_RX + 0.45), math.sin(TWO_PI * k / 16) * (BAND_RY + 0.45), 0.9) for k in range(16)]
    for k in range(4, 13):
        pass
    for k in range(16):
        if 2 <= k <= 6:
            a, b = cord[k], cord[(k + 1) % 16]
            m.beam(a, b, 0.22, gold, 0.9)
    visor_in, visor_out = [], []
    for k in range(9):
        a = math.radians(20 + 140 * k / 8)
        visor_in.append((math.cos(a) * (BAND_RX + 0.3), math.sin(a) * (BAND_RY + 0.3), 0.4))
        visor_out.append((math.cos(a) * (BAND_RX + 4.2) * 0.95, math.sin(a) * (BAND_RY + 5.2), -1.6))
    for k in range(8):
        m.quad(visor_in[k], visor_in[k + 1], visor_out[k + 1], visor_out[k], (0, 0.3, 1), black, 0.55)
        lo = [(p[0], p[1], p[2] - 0.45) for p in (visor_in[k], visor_in[k + 1], visor_out[k + 1], visor_out[k])]
        m.quad(lo[0], lo[1], lo[2], lo[3], (0, -0.3, -1), shade(black, 0.8), 0.55)
    m.loft([[(p[0], p[1], p[2] - 0.45) for p in visor_out], visor_out], shade(black, 1.2), 0.55, closed=False)
    # Ancla dorada en la cinta.
    y = BAND_RY + 0.5
    m.box((0.0, y, 1.6), (0.22, 0.2, 1.0), gold, 0.95)
    m.box((0.0, y, 2.35), (0.7, 0.2, 0.18), gold, 0.95)
    m.box((-0.55, y, 0.75), (0.45, 0.2, 0.18), gold, 0.95, rot=lambda p: rot_y(p, 35))
    m.box((0.55, y, 0.75), (0.45, 0.2, 0.18), gold, 0.95, rot=lambda p: rot_y(p, -35))
    m.sphere((0.0, y, 2.85), 0.35, gold, 0.95, seg=6, rings=3)
    return band_frame(m, tilt=6.0, down=3.2)


def sailor_cap():
    m = Mesh()
    white, white_shadow, blue, red = 0xF7F9FC, 0xDCE3EC, 0x2459B3, 0xE63946
    seg = 20
    squash = (1.0, BAND_RY / BAND_RX)
    m.lathe([(BAND_RX + 0.2, 0.0), (BAND_RX + 0.35, 2.2)], seg, blue, squash=squash, top=False, tone=0.08)
    m.lathe([(BAND_RX + 0.35, 2.2), (BAND_RX + 2.2, 3.3), (BAND_RX + 2.6, 4.1)], seg, white_shadow, squash=squash, top=False)
    m.lathe([(BAND_RX + 2.6, 4.1), (BAND_RX + 1.8, 4.8), (4.0, 5.3), (0.5, 5.4)], seg, white, squash=squash, top=True)
    m.sphere((0.0, 0.0, 6.6), 1.7, red, seg=8, rings=5)
    # Cintas que caen por detrás.
    for side in (-1.0, 1.0):
        a = (side * 1.2, -BAND_RY - 0.2, 1.0)
        b = (side * 2.4, -BAND_RY - 1.6, -4.5)
        m.box(lerp(a, b, 0.5), (0.9, 0.12, 3.0), blue, rot=lambda p, s=side: rot_x(rot_y(p, s * 12.0), -18.0))
    return band_frame(m, tilt=16.0, down=3.2)


def party_hat():
    m = Mesh()
    colors = [0xFF6FA8, 0xFFD23F, 0x4CC9F0]
    seg = 14
    prof = []
    for i in range(7):
        t = i / 6.0
        prof.append(((BAND_RX - 1.5) * (1.0 - t) + 0.25 * t, 16.0 * t))
    m.lathe(prof, seg, lambda i, k: colors[(i + (k // 2) % 2) % 3] if i < 6 else colors[0], top=True)
    m.sphere((0.0, 0.0, 16.6), 1.6, 0xFFFBF0, seg=8, rings=5)
    # Confeti en la base.
    for k in range(12):
        a = TWO_PI * k / 12
        m.sphere((math.cos(a) * (BAND_RX - 1.1), math.sin(a) * (BAND_RX - 1.1), 0.6), 0.6, colors[(k + 1) % 3], seg=5, rings=3)
    out = band_frame(m, tilt=4.0, down=3.0)
    return out.transformed(lambda p: rot_y(p, -14.0))


def propeller_beanie():
    m = Mesh()
    panels = [0xE63946, 0xFFD23F, 0x2F80ED, 0x3DDC62]
    seg = 16
    prof = [(BAND_RX + 0.2, 0.0), (BAND_RX + 0.2, 1.2)]
    for i in range(1, 6):
        t = i / 5.0
        prof.append(((BAND_RX + 0.2) * math.cos(t * math.pi / 2.0), 1.2 + 5.6 * math.sin(t * math.pi / 2.0)))
    m.lathe(prof, seg, lambda i, k: 0x2B2833 if i == 0 else panels[(k // 4) % 4], squash=(1.0, BAND_RY / BAND_RX), top=True)
    m.cylinder((0, 0, 6.6), (0, 0, 8.6), 0.35, 0.35, 6, 0xB8BEC8, 0.7)
    m.sphere((0, 0, 8.7), 0.7, 0xE63946, 0.3, seg=6, rings=3)
    for blade, col in ((0.0, 0xFFD23F), (180.0, 0x2F80ED)):
        m.box(rot_z((3.6, 0.0, 8.8), blade), (3.2, 0.9, 0.12), col, 0.2, rot=lambda p, b=blade: rot_z(rot_x(p, 18.0), b))
    return band_frame(m, tilt=10.0, down=4.0)


def hibiscus():
    m = Mesh()
    petal, petal_dark, center, stamen, leaf = 0xFF5DA2, 0xE0307A, 0xB0124F, 0xFFD23F, 0x3DAE5A
    for i in range(5):
        a = TWO_PI * i / 5
        dirv = (math.cos(a), math.sin(a), 0.0)
        side = (-math.sin(a), math.cos(a), 0.0)
        base = mul(dirv, 0.6)
        tip = add(mul(dirv, 5.6), (0, 0, 1.2))
        l = add(mul(dirv, 3.4), add(mul(side, 2.3), (0, 0, 0.8)))
        r = add(mul(dirv, 3.4), add(mul(side, -2.3), (0, 0, 0.8)))
        m.tri(base, l, tip, (0, 0, 1), petal)
        m.tri(base, tip, r, (0, 0, 1), shade(petal, 0.93))
        m.tri(base, l, tip, (0, 0, -1), petal_dark)
        m.tri(base, tip, r, (0, 0, -1), petal_dark)
        m.tri(base, mul(add(l, base), 0.5), mul(add(r, base), 0.5), (0, 0, 1), center)
    m.cylinder((0, 0, 0.3), (0.4, 0.8, 4.2), 0.3, 0.2, 5, stamen)
    for j in range(5):
        a = TWO_PI * j / 5
        m.sphere((0.4 + math.cos(a) * 0.6, 0.8 + math.sin(a) * 0.6, 4.3), 0.35, stamen, seg=5, rings=3)
    for a_deg in (200.0, 250.0):
        a = math.radians(a_deg)
        d = (math.cos(a), math.sin(a), 0.0)
        s = (-math.sin(a), math.cos(a), 0.0)
        b0 = mul(d, 2.0)
        tip = add(mul(d, 8.2), (0, 0, -0.8))
        l = add(mul(d, 5.0), mul(s, 1.8))
        r = add(mul(d, 5.0), mul(s, -1.8))
        m.tri(b0, l, tip, (0, 0, 1), leaf)
        m.tri(b0, tip, r, (0, 0, 1), shade(leaf, 0.85))
        m.tri(b0, l, tip, (0, 0, -1), shade(leaf, 0.7))
        m.tri(b0, tip, r, (0, 0, -1), shade(leaf, 0.7))
    # Sobre la sien derecha, mirando hacia fuera y algo adelante.
    return m.transformed(lambda p: add(rot_z(rot_y(rot_x(p, 10.0), 62.0), -8.0), (7.0, -1.8, -0.6)))


def starfish():
    m = Mesh()
    m.star((0.0, 0.0, 0.0), 9.0, 3.6, 5, 1.6, 0xFF8C42, droop=3.4, bump_color=0xFFE3B8)
    return m.transformed(lambda p: add(rot_z(rot_x(p, 12.0), 18.0), (0.0, 0.5, -1.0)))


def crab():
    m = Mesh()
    shell, shell_dark, belly, white, black = 0xE4572E, 0xB8391C, 0xFFB38A, 0xFFFFFF, 0x13233B
    m.sphere((0.0, 0.0, 2.4), 4.6, shell, seg=10, rings=5, squash=(1.0, 0.72, 0.52))
    m.sphere((0.0, -0.2, 1.7), 4.2, belly, seg=10, rings=4, squash=(1.0, 0.7, 0.35))
    for i in range(5):
        m.sphere((-2.4 + 1.2 * i, -0.3, 4.6), 0.55, shade(shell, 1.15), seg=5, rings=3)
    # Ojos en pedúnculo.
    for side in (-1.0, 1.0):
        base = (side * 1.3, 2.4, 3.2)
        top = (side * 1.7, 3.0, 6.0)
        m.cylinder(base, top, 0.35, 0.3, 5, shell_dark)
        m.sphere(top, 1.0, white, seg=6, rings=4)
        m.sphere(add(top, (side * 0.1, 0.7, 0.2)), 0.45, black, seg=5, rings=3)
    # Pinzas arriba.
    for side in (-1.0, 1.0):
        sh = (side * 4.2, 1.4, 2.6)
        el = (side * 6.3, 3.4, 4.6)
        m.cylinder(sh, el, 0.8, 0.7, 6, shell)
        claw_c = add(el, (side * 0.7, 1.6, 1.2))
        m.sphere(claw_c, 1.7, shell, seg=7, rings=4, squash=(0.9, 1.3, 0.8))
        m.box(add(claw_c, (side * 0.3, 1.9, 0.9)), (0.45, 1.3, 0.45), shade(shell, 1.1), rot=lambda p: rot_x(p, 35.0))
        m.box(add(claw_c, (side * 0.3, 1.9, -0.7)), (0.4, 1.2, 0.4), shell_dark, rot=lambda p: rot_x(p, -25.0))
    # Patas que abrazan la cabeza.
    for side in (-1.0, 1.0):
        for j in range(3):
            y = 0.2 - 1.6 * j
            a = (side * 3.8, y, 1.6)
            knee = (side * 6.3, y - 0.3, 2.4)
            foot = (side * 7.6, y - 0.6, -2.6)
            m.cylinder(a, knee, 0.42, 0.36, 5, shell_dark)
            m.cylinder(knee, foot, 0.36, 0.15, 5, shell_dark)
    return m.transformed(lambda p: add(rot_x(p, 8.0), (0.0, 0.6, -1.2)))


def halo():
    m = Mesh()
    gold = 0xFFD86B
    seg, tube_seg = 20, 6
    rr, tr = 6.8, 0.75
    rings = []
    for k in range(seg + 1):
        a = TWO_PI * k / seg
        c = (math.cos(a) * rr, math.sin(a) * rr, 0.0)
        out = (math.cos(a), math.sin(a), 0.0)
        ring = []
        for j in range(tube_seg):
            b = TWO_PI * j / tube_seg
            ring.append(add(c, add(mul(out, math.cos(b) * tr), (0.0, 0.0, math.sin(b) * tr))))
        rings.append((c, ring))
    for k in range(seg):
        c0, r0 = rings[k]
        c1, r1 = rings[k + 1]
        for j in range(tube_seg):
            j1 = (j + 1) % tube_seg
            mid = mul(add(add(r0[j], r0[j1]), add(r1[j], r1[j1])), 0.25)
            m.quad(r0[j], r0[j1], r1[j1], r1[j], sub(mid, lerp(c0, c1, 0.5)), gold, 1.0)
    return m.transformed(lambda p: add(rot_x(p, 10.0), (0.0, 0.8, 5.0)))


def jelly_hat():
    m = Mesh()
    bell, bell_dark, rim, tent = 0xC98BFF, 0x9A5CE0, 0xF4D4FF, 0xE9B8FF
    seg = 16
    prof = []
    for i in range(7):
        t = i / 6.0
        prof.append(((BAND_RX + 1.4) * math.cos(t * math.pi / 2.0) + 0.001, 0.4 + 7.4 * math.sin(t * math.pi / 2.0)))
    m.lathe(prof, seg, lambda i, k: bell if (k + i) % 2 else shade(bell, 1.07), squash=(1.0, BAND_RY / BAND_RX), top=True)
    # Volante del borde (dientes alternos).
    for k in range(seg):
        a0, a1 = TWO_PI * k / seg, TWO_PI * (k + 1) / seg
        am = (a0 + a1) * 0.5
        sq = BAND_RY / BAND_RX
        p0 = (math.cos(a0) * (BAND_RX + 1.4), math.sin(a0) * (BAND_RX + 1.4) * sq, 0.4)
        p1 = (math.cos(a1) * (BAND_RX + 1.4), math.sin(a1) * (BAND_RX + 1.4) * sq, 0.4)
        pm = (math.cos(am) * (BAND_RX + 2.0), math.sin(am) * (BAND_RX + 2.0) * sq, -1.0)
        m.tri(p0, p1, pm, (math.cos(am), math.sin(am), 0.0), rim)
        m.tri(p0, p1, pm, (-math.cos(am), -math.sin(am), 0.0), bell_dark)
    # Tentáculos ondulados por detrás y a los lados.
    for j in range(7):
        a = math.radians(200 + 20 * j)
        x0, y0 = math.cos(a) * (BAND_RX + 0.6), math.sin(a) * (BAND_RY + 0.6)
        prev = (x0, y0, 0.2)
        for s in range(1, 6):
            z = 0.2 - 2.0 * s
            wob = math.sin(s * 1.3 + j) * 0.8
            cur = (x0 * (1.0 + 0.04 * s) + wob * math.sin(a), y0 * (1.0 + 0.04 * s) - wob * math.cos(a), z)
            m.cylinder(prev, cur, 0.42 - 0.05 * s, 0.37 - 0.05 * s, 5, tent)
            prev = cur
    # Carita en la campana.
    m.sphere((-1.6, BAND_RY + 0.9, 3.4), 0.5, 0x13233B, seg=5, rings=3)
    m.sphere((1.6, BAND_RY + 0.9, 3.4), 0.5, 0x13233B, seg=5, rings=3)
    m.sphere((0.0, BAND_RY + 1.1, 2.3), 0.35, 0xFF6FA8, seg=5, rings=3)
    return band_frame(m, tilt=14.0, down=3.6)


# Id del estilo (sufijo del asset SM_Helmet_<Id> y fila de DT_Helmets), nombre en la tienda, lo que dice el tendero.
HELMETS = [
    ("Straw", straw_hat, "Sombrero de paja", "Fresquito para la playa. Con lazo coral, que es lo que se lleva."),
    ("Pirate", pirate_hat, "Tricornio pirata", "Perteneció a un capitán muy temido... o eso me dijeron."),
    ("Crown", crown, "Corona real", "Oro de verdad. Bueno, de verdad de la buena no, pero brilla igual."),
    ("Captain", captain_cap, "Gorra de capitán", "Para quien lleva el timón de la tripulación."),
    ("Sailor", sailor_cap, "Gorro de marinero", "Con pompón rojo y cintas al viento. Muy marinero."),
    ("Party", party_hat, "Gorro de fiesta", "¡Toda carrera terminada es motivo de celebración!"),
    ("Propeller", propeller_beanie, "Gorro de hélice", "No vuela. Lo he probado. Pero queda genial."),
    ("Hibiscus", hibiscus, "Flor tropical", "Un hibisco recién cogido. Huele a vacaciones."),
    ("Starfish", starfish, "Estrella de mar", "Se llama Estrellita. Es muy tranquila, no muerde."),
    ("Crab", crab, "Cangrejo de compañía", "Te acompaña a todas partes y te defiende de las gaviotas."),
    ("Jelly", jelly_hat, "Sombrero medusa", "Blandito y fresquito. No pica, te lo prometo."),
    ("Halo", halo, "Aureola", "Para las tortugas más buenas del arrecife."),
]


def build(style_id):
    for sid, fn, _, _ in HELMETS:
        if sid == style_id:
            return fn()
    raise KeyError(style_id)
