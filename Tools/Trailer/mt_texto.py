# -*- coding: utf-8 -*-
"""Tipografía, rótulos, iconos dibujados y animaciones de texto para el montaje del tráiler de Tortunavy.

Todo lo que se pinta aquí son «sprites» (imágenes BGRA de 8 bits con el alfa ya multiplicado) que el montaje coloca,
gira y escala fotograma a fotograma sobre el lienzo. Los sprites se cachean por texto, estilo y tamaño, así que el coste
por fotograma es una sola transformación afín por rótulo.

La marca es amarillo sobre azul marino: relleno amarillo con degradado, borde azul marino muy grueso, extrusión oscura
hacia abajo (efecto pegatina en relieve) y una sombra suave.
"""
from __future__ import annotations

import functools
import math

import cv2
import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

# ── Paleta de la marca (RGB) ────────────────────────────────────────────────────────────────────────────────────────
AZUL_MARINO = (11, 31, 75)
AZUL_OSCURO = (5, 14, 40)
AMARILLO = (255, 210, 31)
AMARILLO_CLARO = (255, 240, 140)
NARANJA = (255, 150, 20)
BLANCO = (255, 255, 255)
CREMA = (255, 244, 214)
CORAL = (255, 90, 70)
TURQUESA = (25, 181, 201)
VERDE = (61, 190, 90)
VERDE_OSC = (24, 120, 60)
ARENA = (242, 212, 155)
OLIVA = (86, 98, 46)

DIR_FUENTES = "C:/Windows/Fonts/"
FUENTES = {
    "impact": ["impact.ttf", "ariblk.ttf", "seguibl.ttf"],
    "negra": ["ariblk.ttf", "seguibl.ttf", "impact.ttf"],
    "seguro": ["seguibl.ttf", "ariblk.ttf", "impact.ttf"],
    "comic": ["comicbd.ttf", "seguibl.ttf", "ariblk.ttf"],
    "bahn": ["bahnschrift.ttf", "seguibl.ttf", "ariblk.ttf"],
}


@functools.lru_cache(maxsize=128)
def fuente(nombre: str, px: int) -> ImageFont.FreeTypeFont:
    """Carga una fuente gruesa de Windows con alternativas por si falta alguna."""
    for fichero in FUENTES.get(nombre, FUENTES["impact"]):
        try:
            return ImageFont.truetype(DIR_FUENTES + fichero, max(int(px), 4))
        except OSError:
            continue
    return ImageFont.load_default()


# ── Estilos de rótulo ───────────────────────────────────────────────────────────────────────────────────────────────
# relleno: tres paradas de arriba abajo (RGB). borde/extrusión/halo en fracciones del tamaño de letra.
ESTILOS = {
    "marca": dict(fuente="impact", relleno=[(255, 244, 160), (255, 214, 30), (255, 158, 10)], borde=AZUL_MARINO, bw=0.085,
                  extr=0.055, extr_col=AZUL_OSCURO, sombra=0.5, halo=None),
    "blanco": dict(fuente="impact", relleno=[(255, 255, 255), (255, 252, 238), (214, 226, 246)], borde=AZUL_MARINO, bw=0.085,
                   extr=0.055, extr_col=AZUL_OSCURO, sombra=0.5, halo=None),
    "alerta": dict(fuente="impact", relleno=[(255, 176, 140), (255, 86, 56), (206, 36, 44)], borde=AZUL_MARINO, bw=0.085,
                   extr=0.055, extr_col=AZUL_OSCURO, sombra=0.5, halo=None),
    "mar": dict(fuente="impact", relleno=[(190, 250, 255), (30, 200, 220), (10, 120, 170)], borde=AZUL_MARINO, bw=0.085,
                extr=0.055, extr_col=AZUL_OSCURO, sombra=0.5, halo=None),
    "verde": dict(fuente="impact", relleno=[(200, 255, 170), (80, 210, 90), (28, 140, 70)], borde=AZUL_MARINO, bw=0.085,
                  extr=0.055, extr_col=AZUL_OSCURO, sombra=0.5, halo=None),
    # Logo: letra ancha, borde marino con halo crema para que se lea también sobre fondo azul marino.
    "logo": dict(fuente="negra", relleno=[(255, 246, 170), (255, 212, 28), (255, 150, 10)], borde=AZUL_MARINO, bw=0.10,
                 extr=0.075, extr_col=AZUL_OSCURO, sombra=0.55, halo=(CREMA, 0.03)),
    # Con halo crema fino, para rótulos que caen sobre fondos oscuros o muy cargados.
    "marca_halo": dict(fuente="impact", relleno=[(255, 244, 160), (255, 214, 30), (255, 158, 10)], borde=AZUL_MARINO,
                       bw=0.085, extr=0.055, extr_col=AZUL_OSCURO, sombra=0.5, halo=(CREMA, 0.025)),
    # Cajas: texto plano sobre un cartel con borde.
    "cartel": dict(fuente="negra", plano=AZUL_MARINO, caja=dict(color=AMARILLO, borde=AZUL_MARINO, bw=0.12, radio=0.28,
                                                                 px=0.5, py=0.28, franja=NARANJA)),
    "cartel_azul": dict(fuente="negra", plano=AMARILLO, caja=dict(color=AZUL_MARINO, borde=CREMA, bw=0.07, radio=0.28,
                                                                   px=0.5, py=0.28, franja=None)),
    "comic": dict(fuente="comic", plano=(20, 20, 30), caja=dict(color=(255, 232, 90), borde=(20, 20, 30), bw=0.1, radio=0.08,
                                                                 px=0.4, py=0.22, franja=None)),
    "chapa": dict(fuente="seguro", plano=CREMA, caja=dict(color=OLIVA, borde=(38, 44, 20), bw=0.09, radio=0.10,
                                                         px=0.5, py=0.26, franja=(120, 134, 66), remaches=True)),
}


def _bgr(c):
    return (int(c[2]), int(c[1]), int(c[0]))


def _mezcla(a, b, t):
    return tuple(a[i] + (b[i] - a[i]) * t for i in range(3))


def _degradado(paradas, t):
    """Color de un degradado de tres paradas en t∈[0,1] (t puede ser un array)."""
    t = np.clip(t, 0.0, 1.0)
    a, b, c = (np.array(p, np.float32) for p in paradas)
    t = np.asarray(t, np.float32)[..., None]
    lo = a + (b - a) * np.clip(t * 2.0, 0, 1)
    hi = b + (c - b) * np.clip(t * 2.0 - 1.0, 0, 1)
    return np.where(t < 0.5, lo, hi)


def _sobre(P, A, color, a):
    """Compone una capa (color plano o imagen HxWx3 en 0..255, alfa a HxW en 0..1) bajo... encima del acumulado."""
    a3 = a[..., None]
    P = color * a3 + P * (1.0 - a3)
    A = a + A * (1.0 - a)
    return P, A


def _desplazar(m, dx, dy):
    """Desplaza una máscara float sin envolver los bordes."""
    out = np.zeros_like(m)
    h, w = m.shape
    xs, xd = (0, dx) if dx >= 0 else (-dx, 0)
    ys, yd = (0, dy) if dy >= 0 else (-dy, 0)
    ww, hh = w - abs(dx), h - abs(dy)
    if ww > 0 and hh > 0:
        out[yd:yd + hh, xd:xd + ww] = m[ys:ys + hh, xs:xs + ww]
    return out


def a_sprite(P, A):
    """(P premultiplicado RGB float 0..255, A 0..1) → BGRA uint8 premultiplicado."""
    out = np.empty(A.shape + (4,), np.uint8)
    out[..., 0] = np.clip(P[..., 2], 0, 255)
    out[..., 1] = np.clip(P[..., 1], 0, 255)
    out[..., 2] = np.clip(P[..., 0], 0, 255)
    out[..., 3] = np.clip(A * 255.0 + 0.5, 0, 255)
    return out


def _medidas_linea(f, linea, sw, tabular, celda):
    if tabular:
        ancho = 0.0
        for ch in linea:
            ancho += celda if ch.isdigit() else f.getlength(ch)
        alto = f.getbbox("H", anchor="ls", stroke_width=sw)
        return (0, alto[1], int(math.ceil(ancho)), alto[3])
    return f.getbbox(linea, anchor="ls", stroke_width=sw)


def _dibujar_linea(d, x, y, linea, f, fill, sw, tabular, celda):
    if not tabular:
        d.text((x, y), linea, font=f, fill=fill, stroke_width=sw, stroke_fill=fill, anchor="ls")
        return
    cx = x
    for ch in linea:
        if ch.isdigit():
            d.text((cx + celda / 2.0, y), ch, font=f, fill=fill, stroke_width=sw, stroke_fill=fill, anchor="ms")
            cx += celda
        else:
            d.text((cx, y), ch, font=f, fill=fill, stroke_width=sw, stroke_fill=fill, anchor="ls")
            cx += f.getlength(ch)


def sprite_texto(txt: str, estilo: str = "marca", tam: int = 150, ancho_max: int = 0, tabular: bool = False) -> np.ndarray:
    return _sprite_texto(txt, estilo, int(tam), int(ancho_max), tabular)[0]


def tam_ajustado(txt: str, estilo: str, tam: int, ancho_max: int = 0, tabular: bool = False) -> int:
    """Tamaño de letra al que se queda el texto tras ajustarlo a `ancho_max` (para contadores de tamaño fijo)."""
    return _sprite_texto(txt, estilo, int(tam), int(ancho_max), tabular)[1]


@functools.lru_cache(maxsize=768)
def _sprite_texto(txt: str, estilo: str = "marca", tam: int = 150, ancho_max: int = 0, tabular: bool = False):
    """Sprite BGRA premultiplicado del texto. `tam` es la altura de letra (px del lienzo). Si `ancho_max` > 0 y el
    texto es más ancho, se reduce para que quepa entero (nunca se corta)."""
    st = ESTILOS.get(estilo, ESTILOS["marca"])
    if "caja" in st:
        return _sprite_cartel(txt, st, tam, ancho_max, tabular)
    px = int(round(tam))
    lineas = txt.split("\n")
    for _ in range(3):
        f = fuente(st["fuente"], px)
        sw = max(1, int(round(px * st["bw"])))
        celda = max((f.getlength(str(d)) for d in range(10)), default=px * 0.5)
        med = [_medidas_linea(f, l, sw, tabular, celda) for l in lineas]
        ancho = max(m[2] - m[0] for m in med)
        extra = px * st["extr"] * 1.2 + px * 0.12
        if ancho_max and ancho + 2 * extra > ancho_max:
            px = max(8, int(px * (ancho_max / (ancho + 2 * extra)) * 0.985))
            continue
        break
    ex = int(round(px * st["extr"]))
    hw = int(round(px * st["halo"][1])) if st.get("halo") else 0
    cap = f.getbbox("H", anchor="ls")
    cap_h = -cap[1]
    paso = int(cap_h + max(sw * 1.8, 0.24 * px))
    pad = ex + hw + int(px * 0.10) + 4
    top = min(m[1] for m in med[:1])
    bot = med[-1][3]
    W = int(ancho + 2 * pad + 2)
    H = int((len(lineas) - 1) * paso + (bot - top) + 2 * pad + ex + 2)
    m_fill = Image.new("L", (W, H), 0)
    m_out = Image.new("L", (W, H), 0)
    m_halo = Image.new("L", (W, H), 0) if hw else None
    d_fill, d_out = ImageDraw.Draw(m_fill), ImageDraw.Draw(m_out)
    d_halo = ImageDraw.Draw(m_halo) if hw else None
    bases = []
    for i, l in enumerate(lineas):
        l0, t0, r0, b0 = med[i]
        x = pad + (ancho - (r0 - l0)) / 2.0 - l0 + sw * 0.0
        y = pad - top + i * paso
        bases.append(y)
        _dibujar_linea(d_fill, x, y, l, f, 255, 0, tabular, celda)
        _dibujar_linea(d_out, x, y, l, f, 255, sw, tabular, celda)
        if d_halo is not None:
            _dibujar_linea(d_halo, x, y, l, f, 255, sw + hw, tabular, celda)
    fill = np.asarray(m_fill, np.float32) / 255.0
    out = np.asarray(m_out, np.float32) / 255.0
    P = np.zeros((H, W, 3), np.float32)
    A = np.zeros((H, W), np.float32)
    # sombra suave
    if st["sombra"] > 0:
        dx, dy = int(px * 0.045), int(px * 0.10)
        base = out if not hw else np.asarray(m_halo, np.float32) / 255.0
        sh = _desplazar(base, dx, dy + ex)
        sh = cv2.GaussianBlur(sh, (0, 0), max(1.0, px * 0.035))
        P, A = _sobre(P, A, np.array(AZUL_OSCURO, np.float32), sh * st["sombra"])
    # extrusión (bloque oscuro hacia abajo)
    if ex > 0:
        ext = out.copy()
        for k in range(1, ex + 1):
            ext = np.maximum(ext, _desplazar(out, int(k * 0.35), k))
        P, A = _sobre(P, A, np.array(st["extr_col"], np.float32), ext)
    if hw:
        P, A = _sobre(P, A, np.array(st["halo"][0], np.float32), np.asarray(m_halo, np.float32) / 255.0)
    P, A = _sobre(P, A, np.array(st["borde"], np.float32), out)
    # relleno con degradado por línea y un destello suave arriba
    grad = np.zeros((H, W, 3), np.float32)
    ys = np.arange(H, dtype=np.float32)
    t_img = np.zeros(H, np.float32) + 0.5
    brillo = np.zeros(H, np.float32)
    for yb in bases:
        arriba = yb - cap_h
        sel = (ys >= arriba - px * 0.25) & (ys <= yb + px * 0.25)
        t_img[sel] = (ys[sel] - arriba) / max(cap_h, 1)
        brillo[sel] = np.clip(1.0 - (ys[sel] - arriba) / (cap_h * 0.55), 0, 1) ** 1.5
    col = _degradado(st["relleno"], t_img)  # (H,3)
    grad[:] = col[:, None, :]
    grad += (brillo[:, None, None] * 26.0)
    P, A = _sobre(P, A, np.clip(grad, 0, 255), fill)
    return a_sprite(P, A), px


def _sprite_cartel(txt, st, tam, ancho_max, tabular):
    """Texto sobre un cartel redondeado con borde (PRÓXIMAMENTE, chapas militares, leyendas de cómic)."""
    cj = st["caja"]
    px = int(round(tam))
    lineas = txt.split("\n")
    for _ in range(3):
        f = fuente(st["fuente"], px)
        celda = max((f.getlength(str(d)) for d in range(10)), default=px * 0.5)
        med = [_medidas_linea(f, l, 0, tabular, celda) for l in lineas]
        ancho = max(m[2] - m[0] for m in med)
        bw = int(px * cj["bw"])
        if ancho_max and ancho + 2 * (px * cj["px"] + bw) > ancho_max:
            px = max(8, int(px * ancho_max / (ancho + 2 * (px * cj["px"] + bw)) * 0.985))
            continue
        break
    cap_h = -f.getbbox("H", anchor="ls")[1]
    paso = int(cap_h * 1.32)
    bw = max(2, int(px * cj["bw"]))
    padx, pady = int(px * cj["px"]), int(px * cj["py"])
    sombra = int(px * 0.12)
    alto_txt = cap_h + (len(lineas) - 1) * paso
    W = int(ancho + 2 * (padx + bw) + 2 * sombra + 8)
    H = int(alto_txt + 2 * (pady + bw) + 2 * sombra + 8)
    img = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    x0, y0 = sombra + 2, sombra + 2
    x1, y1 = W - sombra - 3, H - sombra - 3
    r = int(min(x1 - x0, y1 - y0) * cj["radio"])
    # sombra dura desplazada
    d.rounded_rectangle((x0 + sombra * 0.6, y0 + sombra, x1 + sombra * 0.6, y1 + sombra), radius=r, fill=AZUL_OSCURO + (170,))
    d.rounded_rectangle((x0, y0, x1, y1), radius=r, fill=cj["borde"] + (255,))
    d.rounded_rectangle((x0 + bw, y0 + bw, x1 - bw, y1 - bw), radius=max(r - bw, 1), fill=cj["color"] + (255,))
    if cj.get("franja"):
        # banda inferior más oscura para dar volumen
        fr = cj["franja"]
        d.rounded_rectangle((x0 + bw, y0 + (y1 - y0) * 0.62, x1 - bw, y1 - bw), radius=max(r - bw, 1), fill=fr + (255,))
        d.rectangle((x0 + bw, y0 + (y1 - y0) * 0.62, x1 - bw, y0 + (y1 - y0) * 0.70), fill=cj["color"] + (255,))
    if cj.get("remaches"):
        rr = max(3, int(px * 0.07))
        for cx, cy in ((x0 + bw + rr * 2.2, y0 + bw + rr * 2.2), (x1 - bw - rr * 2.2, y0 + bw + rr * 2.2),
                       (x0 + bw + rr * 2.2, y1 - bw - rr * 2.2), (x1 - bw - rr * 2.2, y1 - bw - rr * 2.2)):
            d.ellipse((cx - rr, cy - rr, cx + rr, cy + rr), fill=(190, 200, 140, 255), outline=(30, 36, 16, 255), width=2)
    yb = y0 + bw + pady + cap_h
    for i, l in enumerate(lineas):
        l0, t0, r0, b0 = med[i]
        xl = (x0 + x1) / 2.0 - (r0 - l0) / 2.0 - l0
        yy = yb + i * paso
        if tabular:
            _dibujar_linea(d, xl, yy, l, f, st["plano"] + (255,), 0, True, celda)
        else:
            d.text((xl, yy), l, font=f, fill=st["plano"] + (255,), anchor="ls")
    a = np.asarray(img, np.float32)
    al = a[..., 3] / 255.0
    P = a[..., :3] * al[..., None]
    return a_sprite(P, al), px


# ── Iconos dibujados ────────────────────────────────────────────────────────────────────────────────────────────────

def _lienzo(base=512, ss=2):
    im = Image.new("RGBA", (base * ss, base * ss), (0, 0, 0, 0))
    return im, ImageDraw.Draw(im), ss


def _pol_hex(cx, cy, r, giro=30):
    return [(cx + r * math.cos(math.radians(giro + 60 * i)), cy + r * math.sin(math.radians(giro + 60 * i))) for i in range(6)]


def _sprite_desde_pil(im: Image.Image, px: int, margen=0.0):
    im = im.resize((px, px), Image.LANCZOS)
    a = np.asarray(im, np.float32)
    al = a[..., 3] / 255.0
    return a_sprite(a[..., :3] * al[..., None], al)


def _dibuja_concha(base=512):
    """Caparazón de tortuga: cúpula verde con placas hexagonales y borde marino."""
    im, d, ss = _lienzo(base)
    s = lambda v: v * ss  # noqa: E731
    ow = s(17)
    # borde inferior (faldón) color arena
    d.ellipse((s(36), s(268), s(476), s(452)), fill=AZUL_MARINO + (255,))
    d.ellipse((s(52), s(280), s(460), s(432)), fill=(236, 176, 84, 255))
    d.ellipse((s(52), s(280), s(460), s(410)), fill=(250, 208, 120, 255))
    # cúpula
    domo = Image.new("L", im.size, 0)
    dd = ImageDraw.Draw(domo)
    dd.ellipse((s(40), s(60), s(472), s(400)), fill=255)
    cap = Image.new("RGBA", im.size, (0, 0, 0, 0))
    dc = ImageDraw.Draw(cap)
    dc.rectangle((0, 0, im.size[0], im.size[1]), fill=VERDE + (255,))
    # placas hexagonales
    for fila in range(-2, 3):
        for col in range(-3, 4):
            cx = 256 + col * 108 + (54 if fila % 2 else 0)
            cy = 230 + fila * 94
            tono = (95, 218, 120) if (col + fila) % 2 == 0 else (70, 198, 100)
            dc.polygon([(s(x), s(y)) for x, y in _pol_hex(cx, cy, 58, 30)], fill=tono + (255,), outline=AZUL_MARINO + (255,))
            hx = [(s(x), s(y)) for x, y in _pol_hex(cx, cy, 58, 30)]
            dc.line(hx + [hx[0]], fill=AZUL_MARINO + (255,), width=ss * 9)
    # brillo
    dc.ellipse((s(100), s(90), s(230), s(150)), fill=(255, 255, 255, 110))
    im.paste(cap, (0, 0), domo)
    # contorno de la cúpula
    d = ImageDraw.Draw(im)
    d.arc((s(40), s(60), s(472), s(400)), 180, 360, fill=AZUL_MARINO + (255,), width=ow)
    d.arc((s(40), s(60), s(472), s(400)), 0, 180, fill=AZUL_MARINO + (255,), width=ow)
    return im


def _trazo(d, pts, ancho, color):
    """Línea gruesa con extremos redondos, sellando círculos (PIL deja picos con `joint` en curvas cerradas)."""
    r = ancho / 2.0
    for (x0, y0), (x1, y1) in zip(pts[:-1], pts[1:]):
        n = max(1, int(math.hypot(x1 - x0, y1 - y0) / max(r * 0.35, 1.0)))
        for k in range(n + 1):
            x, y = x0 + (x1 - x0) * k / n, y0 + (y1 - y0) * k / n
            d.ellipse((x - r, y - r, x + r, y + r), fill=color)


def _dibuja_ola(base=512):
    im, d, ss = _lienzo(base)
    s = lambda v: v * ss  # noqa: E731

    def ondulada(y0, fase, amp=34, x0=64, x1=448):
        pts = []
        for i in range(0, 61):
            x = x0 + (x1 - x0) * i / 60.0
            y = y0 + amp * math.sin(i / 60.0 * math.pi * 3.0 + fase)
            pts.append((s(x), s(y)))
        return pts

    filas = [(140, 0.0, (70, 215, 232)), (256, 1.4, (25, 181, 201)), (372, 2.8, (14, 140, 192))]
    for y0, fase, col in filas:
        _trazo(d, ondulada(y0, fase), s(88), AZUL_MARINO + (255,))
    for y0, fase, col in filas:
        pts = ondulada(y0, fase)
        _trazo(d, pts, s(58), col + (255,))
        _trazo(d, [(x, y - s(13)) for x, y in pts[4:-4]], s(9), (235, 252, 255, 230))
    return im


def _dibuja_huevo(base=512):
    im, d, ss = _lienzo(base)
    s = lambda v: v * ss  # noqa: E731
    cx, cy, rx, ry = 256, 262, 150, 205
    pts = []
    for i in range(0, 181):
        th = math.radians(i * 2)
        x = cx + rx * math.sin(th) * (1 - 0.20 * math.cos(th))
        y = cy - ry * math.cos(th)
        pts.append((s(x), s(y)))
    d.polygon(pts, fill=AZUL_MARINO + (255,))
    # interior: mismo contorno algo menor
    pts_i = []
    for i in range(0, 181):
        th = math.radians(i * 2)
        x = cx + (rx - 20) * math.sin(th) * (1 - 0.20 * math.cos(th))
        y = cy - (ry - 20) * math.cos(th)
        pts_i.append((s(x), s(y)))
    masc = Image.new("L", im.size, 0)
    ImageDraw.Draw(masc).polygon(pts_i, fill=255)
    interior = Image.new("RGBA", im.size, CREMA + (255,))
    di = ImageDraw.Draw(interior)
    di.ellipse((s(60), s(330), s(460), s(560)), fill=(240, 218, 170, 255))  # sombra inferior
    for (mx, my, mr) in ((330, 330, 28), (200, 380, 22), (300, 430, 16), (215, 300, 14), (360, 400, 12)):
        di.ellipse((s(mx - mr), s(my - mr * 0.8), s(mx + mr), s(my + mr * 0.8)), fill=(226, 160, 96, 255))
    di.ellipse((s(170), s(120), s(240), s(230)), fill=(255, 255, 255, 190))
    im.paste(interior, (0, 0), masc)
    d = ImageDraw.Draw(im)
    grieta = [(150, 236), (196, 200), (226, 250), (272, 196), (312, 252), (356, 214)]
    d.line([(s(x), s(y)) for x, y in grieta], fill=AZUL_MARINO + (255,), width=s(14), joint="curve")
    return im


def _dibuja_calavera(base=512):
    im, d, ss = _lienzo(base)
    s = lambda v: v * ss  # noqa: E731
    ow = s(16)
    # cráneo
    d.ellipse((s(70), s(30), s(442), s(340)), fill=AZUL_MARINO + (255,))
    d.rounded_rectangle((s(140), s(270), s(372), s(470)), radius=s(50), fill=AZUL_MARINO + (255,))
    d.ellipse((s(86), s(46), s(426), s(324)), fill=(250, 248, 240, 255))
    d.rounded_rectangle((s(156), s(286), s(356), s(454)), radius=s(38), fill=(250, 248, 240, 255))
    d.ellipse((s(100), s(70), s(200), s(150)), fill=(255, 255, 255, 255))
    # dientes
    for i in range(1, 4):
        x = 156 + (356 - 156) * i / 4.0
        d.line((s(x), s(372), s(x), s(452)), fill=AZUL_MARINO + (255,), width=s(10))
    d.line((s(160), s(372), s(352), s(372)), fill=AZUL_MARINO + (255,), width=s(10))
    # ojos saltones y bizcos
    for cx, px_, py_ in ((175, 12, 10), (337, -14, 10)):
        d.ellipse((s(cx - 64), s(150 - 66), s(cx + 64), s(150 + 66)), fill=AZUL_MARINO + (255,))
        d.ellipse((s(cx - 54), s(150 - 56), s(cx + 54), s(150 + 56)), fill=(255, 255, 255, 255))
        d.ellipse((s(cx + px_ - 22), s(150 + py_ - 22), s(cx + px_ + 22), s(150 + py_ + 22)), fill=AZUL_MARINO + (255,))
        d.ellipse((s(cx + px_ - 9), s(150 + py_ - 15), s(cx + px_ + 3), s(150 + py_ - 3)), fill=(255, 255, 255, 255))
    # nariz
    d.polygon([(s(256), s(230)), (s(228), s(290)), (s(284), s(290))], fill=AZUL_MARINO + (255,))
    # mejilla rosa
    d.ellipse((s(96), s(220), s(150), s(262)), fill=(255, 150, 140, 150))
    d.ellipse((s(362), s(220), s(416), s(262)), fill=(255, 150, 140, 150))
    return im


def _dibuja_chispa(base=512):
    im, d, ss = _lienzo(base)
    s = lambda v: v * ss  # noqa: E731
    pts = []
    for i in range(8):
        r = 240 if i % 2 == 0 else 74
        a = math.radians(-90 + 45 * i)
        pts.append((s(256 + r * math.cos(a)), s(256 + r * math.sin(a))))
    d.polygon(pts, fill=AZUL_MARINO + (255,))
    pts2 = []
    for i in range(8):
        r = 200 if i % 2 == 0 else 56
        a = math.radians(-90 + 45 * i)
        pts2.append((s(256 + r * math.cos(a)), s(256 + r * math.sin(a))))
    d.polygon(pts2, fill=AMARILLO + (255,))
    pts3 = []
    for i in range(8):
        r = 96 if i % 2 == 0 else 30
        a = math.radians(-90 + 45 * i)
        pts3.append((s(256 + r * math.cos(a)), s(256 + r * math.sin(a))))
    d.polygon(pts3, fill=(255, 255, 255, 255))
    return im


def _dibuja_monitor(base=512):
    im, d, ss = _lienzo(base)
    s = lambda v: v * ss  # noqa: E731
    d.rounded_rectangle((s(30), s(70), s(482), s(360)), radius=s(40), fill=AZUL_MARINO + (255,))
    d.rounded_rectangle((s(60), s(98), s(452), s(332)), radius=s(22), fill=(60, 190, 230, 255))
    d.polygon([(s(60), s(300)), (s(180), s(190)), (s(260), s(270)), (s(340), s(200)), (s(452), s(310)), (s(452), s(332)),
               (s(60), s(332))], fill=(90, 210, 120, 255))
    d.rectangle((s(226), s(360), s(286), s(420)), fill=AZUL_MARINO + (255,))
    d.rounded_rectangle((s(150), s(410), s(362), s(462)), radius=s(22), fill=AZUL_MARINO + (255,))
    return im


_DIBUJOS = {"concha": _dibuja_concha, "ola": _dibuja_ola, "huevo": _dibuja_huevo, "calavera": _dibuja_calavera,
            "chispa": _dibuja_chispa, "monitor": _dibuja_monitor}


@functools.lru_cache(maxsize=96)
def sprite_icono(nombre: str, px: int) -> np.ndarray:
    """Sprite BGRA premultiplicado cuadrado del icono («concha», «ola», «huevo», «calavera», «chispa», «monitor»)."""
    im = _DIBUJOS[nombre]()
    return _sprite_desde_pil(im, int(px))


def combinar_icono(sprite: np.ndarray, icono: np.ndarray, lado: str = "izq", hueco: int = 12) -> np.ndarray:
    """Une un sprite de texto con un icono al lado (izq, der, arriba)."""
    h1, w1 = sprite.shape[:2]
    h2, w2 = icono.shape[:2]
    if lado in ("izq", "der"):
        H, W = max(h1, h2), w1 + w2 + hueco
        out = np.zeros((H, W, 4), np.uint8)
        yt, yi = (H - h1) // 2, (H - h2) // 2
        if lado == "izq":
            _sobre_premult(out, icono, 0, yi)
            _sobre_premult(out, sprite, w2 + hueco, yt)
        else:
            _sobre_premult(out, sprite, 0, yt)
            _sobre_premult(out, icono, w1 + hueco, yi)
    else:
        H, W = h1 + h2 + hueco, max(w1, w2)
        out = np.zeros((H, W, 4), np.uint8)
        _sobre_premult(out, icono, (W - w2) // 2, 0)
        _sobre_premult(out, sprite, (W - w1) // 2, h2 + hueco)
    return out


def _sobre_premult(dst, src, x, y):
    """dst = src + dst·(1−αsrc) sobre un trozo del destino, todo BGRA premultiplicado uint8."""
    h, w = src.shape[:2]
    x0, y0 = max(x, 0), max(y, 0)
    x1, y1 = min(x + w, dst.shape[1]), min(y + h, dst.shape[0])
    if x1 <= x0 or y1 <= y0:
        return
    s = src[y0 - y:y1 - y, x0 - x:x1 - x].astype(np.float32)
    r = dst[y0:y1, x0:x1].astype(np.float32)
    r = s + r * (1.0 - s[..., 3:4] / 255.0)
    dst[y0:y1, x0:x1] = np.clip(r, 0, 255).astype(np.uint8)


# ── Colocación de sprites sobre el lienzo ───────────────────────────────────────────────────────────────────────────

def pegar(lienzo: np.ndarray, sprite: np.ndarray, cx: float, cy: float, sx: float = 1.0, sy: float | None = None,
          rot: float = 0.0, alfa: float = 1.0, brillo: float = 0.0, sesgo: float = 0.0) -> None:
    """Pega un sprite BGRA premultiplicado en (cx, cy) con escala, giro (grados, sentido horario en pantalla) y
    opacidad. Solo procesa el rectángulo afectado."""
    if sy is None:
        sy = sx
    if alfa <= 0.003 or sx <= 0.002 or sy <= 0.002:
        return
    h, w = sprite.shape[:2]
    th = math.radians(rot)
    c, s_ = math.cos(th), math.sin(th)
    # matriz sprite → lienzo: giro · escala (con sesgo horizontal opcional)
    a11, a12 = c * sx, -s_ * sy + c * sx * sesgo
    a21, a22 = s_ * sx, c * sy + s_ * sx * sesgo
    ox, oy = w / 2.0, h / 2.0
    esq = np.array([[0, 0], [w, 0], [w, h], [0, h]], np.float32)
    xs = a11 * (esq[:, 0] - ox) + a12 * (esq[:, 1] - oy) + cx
    ys = a21 * (esq[:, 0] - ox) + a22 * (esq[:, 1] - oy) + cy
    H, W = lienzo.shape[:2]
    x0, x1 = int(max(math.floor(xs.min()) - 1, 0)), int(min(math.ceil(xs.max()) + 1, W))
    y0, y1 = int(max(math.floor(ys.min()) - 1, 0)), int(min(math.ceil(ys.max()) + 1, H))
    if x1 <= x0 or y1 <= y0:
        return
    M = np.array([[a11, a12, cx - a11 * ox - a12 * oy - x0], [a21, a22, cy - a21 * ox - a22 * oy - y0]], np.float64)
    w_ = cv2.warpAffine(sprite, M, (x1 - x0, y1 - y0), flags=cv2.INTER_LINEAR, borderMode=cv2.BORDER_CONSTANT,
                        borderValue=(0, 0, 0, 0))
    roi = lienzo[y0:y1, x0:x1]
    nch = lienzo.shape[2]  # 3 (lienzo BGR) o 4 (capa BGRA premultiplicada)
    a = (w_[..., 3:4].astype(np.float32) * (alfa / 255.0))
    src = w_[..., :nch].astype(np.float32) * alfa
    if brillo:
        src[..., :3] += a * (255.0 * brillo)
    res = roi.astype(np.float32) * (1.0 - a) + src
    roi[:] = np.clip(res, 0, 255).astype(np.uint8)


def pegar_capa(lienzo, capa, x, y, alfa=1.0):
    """Pega una capa BGRA premultiplicada sin transformar."""
    if alfa >= 0.999:
        _sobre_premult_lienzo(lienzo, capa, x, y)
        return
    c = (capa.astype(np.float32) * alfa).astype(np.uint8)
    _sobre_premult_lienzo(lienzo, c, x, y)


def _sobre_premult_lienzo(lienzo, src, x, y):
    h, w = src.shape[:2]
    x0, y0 = max(x, 0), max(y, 0)
    x1, y1 = min(x + w, lienzo.shape[1]), min(y + h, lienzo.shape[0])
    if x1 <= x0 or y1 <= y0:
        return
    s = src[y0 - y:y1 - y, x0 - x:x1 - x].astype(np.float32)
    roi = lienzo[y0:y1, x0:x1]
    res = s[..., :3] + roi.astype(np.float32) * (1.0 - s[..., 3:4] / 255.0)
    roi[:] = np.clip(res, 0, 255).astype(np.uint8)


# ── Curvas y animaciones ────────────────────────────────────────────────────────────────────────────────────────────

def suave(u):
    u = min(max(u, 0.0), 1.0)
    return u * u * (3 - 2 * u)


def sale_atras(u, s=1.70158):
    """easeOutBack: pasa de largo y vuelve."""
    u = min(max(u, 0.0), 1.0) - 1.0
    return 1.0 + u * u * ((s + 1.0) * u + s)


def sale(u):
    u = min(max(u, 0.0), 1.0)
    return 1.0 - (1.0 - u) ** 3


def bote(tau, dur, altura, r_t=0.56, r_h=0.28, max_botes=5):
    """Desplazamiento vertical (negativo = hacia arriba) de una pelota que rebota tras aterrizar: parábolas cada vez más
    cortas y bajas. `tau` son los segundos desde el primer contacto. Termina siempre (número máximo de botes)."""
    for _ in range(max_botes):
        if tau <= dur:
            u = tau / dur
            return -altura * 4.0 * u * (1.0 - u)
        tau -= dur
        dur *= r_t
        altura *= r_h
    return 0.0


def resorte(tau, freq=2.6, amort=11.0):
    """Oscilación amortiguada que empieza en 1 (para el rebote tras aterrizar)."""
    return math.exp(-amort * tau) * math.cos(2 * math.pi * freq * tau)


# tiempo (s) que dura la aproximación hasta el instante del golpe, según la animación
PRE = {"slam": 0.20, "pop": 0.15, "caida": 0.30, "izq": 0.22, "der": 0.22, "arriba": 0.22, "abajo": 0.22, "zoom": 0.18,
       "fundido": 0.18, "ninguna": 0.0, "tecleo": 0.0}


def anim_entrada(nombre: str, tl: float, giro0: float = -9.0):
    """Estado de un rótulo a `tl` segundos del inicio de su animación, con el golpe en tl = PRE[nombre].
    Devuelve (sx, sy, rot, dx, dy, alfa) donde dx, dy son fracciones del tamaño del sprite."""
    pre = PRE.get(nombre, 0.2)
    if nombre in ("ninguna", "tecleo"):
        return (1.0, 1.0, 0.0, 0.0, 0.0, 1.0 if tl >= 0 else 0.0)
    if tl < 0:
        return (1, 1, 0, 0, 0, 0.0)
    if nombre == "slam":
        if tl < pre:
            u = tl / pre
            e = u * u
            s = 1.0 + 1.3 * (1.0 - e)
            return (s, s, giro0 * (1.0 - e), 0.0, -0.06 * (1.0 - e), min(1.0, tl / 0.05))
        tau = tl - pre
        d = resorte(tau, 2.8, 10.5)
        return (1.0 + 0.13 * d, 1.0 - 0.20 * d, giro0 * 0.25 * math.exp(-8 * tau) * math.sin(2 * math.pi * 3.0 * tau), 0.0, 0.0, 1.0)
    if nombre == "pop":
        if tl < pre:
            e = sale_atras(tl / pre, 2.4)
            return (e, e, giro0 * 0.6 * (1 - tl / pre), 0.0, 0.0, min(1.0, tl / 0.04))
        tau = tl - pre
        d = resorte(tau, 3.2, 12.0)
        return (1.0 + 0.06 * d, 1.0 - 0.06 * d, 0.0, 0.0, 0.0, 1.0)
    if nombre == "caida":
        if tl < pre:
            u = tl / pre
            return (1.0, 1.0, giro0 * (1 - u * u), 0.0, -1.6 * (1 - u * u), min(1.0, tl / 0.05))
        tau = tl - pre
        return (1.0, 1.0, 0.0, 0.0, bote(tau, 0.16, 0.16, 0.62, 0.3), 1.0)
    if nombre in ("izq", "der", "arriba", "abajo"):
        e = sale_atras(tl / pre, 1.4) if tl < pre else 1.0
        rest = 1.0 - e
        if tl >= pre:
            rest = -0.03 * resorte(tl - pre, 3.0, 12.0)
        dx = {"izq": -1.6, "der": 1.6}.get(nombre, 0.0) * rest
        dy = {"arriba": -1.6, "abajo": 1.6}.get(nombre, 0.0) * rest
        return (1.0, 1.0, giro0 * 0.4 * rest, dx, dy, min(1.0, tl / 0.05))
    if nombre == "zoom":
        if tl < pre:
            u = tl / pre
            s = 0.2 + 0.8 * sale_atras(u, 2.0)
            return (s, s, 0.0, 0.0, 0.0, min(1.0, tl / 0.05))
        return (1.0, 1.0, 0.0, 0.0, 0.0, 1.0)
    if nombre == "fundido":
        return (1.0, 1.0, 0.0, 0.0, 0.0, min(1.0, tl / pre))
    return (1.0, 1.0, 0.0, 0.0, 0.0, 1.0)


def anim_salida(nombre: str, ts: float, dur: float = 0.14):
    """Estado de salida a `ts` segundos de haber empezado a salir. (sx, sy, rot, dx, dy, alfa)."""
    if ts <= 0 or nombre == "corte":
        return (1, 1, 0, 0, 0, 1.0)
    u = min(ts / dur, 1.0)
    if nombre == "pop":
        s = max(0.0, 1.0 - u * u) if u < 1 else 0.0
        return (s * (1 + 0.18 * u), s * (1 + 0.18 * u), 6.0 * u, 0.0, 0.0, 1.0 if u < 1 else 0.0)
    if nombre == "fundido":
        return (1, 1, 0, 0, 0, 1.0 - u)
    if nombre == "caer":
        return (1.0, 1.0, 22.0 * u * u, 0.0, 2.2 * u * u, 1.0 - max(0.0, (u - 0.7) / 0.3))
    if nombre == "zoom":
        s = 1.0 + 1.8 * u * u
        return (s, s, 0.0, 0.0, 0.0, 1.0 - u)
    if nombre == "izq":
        return (1, 1, -4 * u, -2.2 * u * u, 0.0, 1.0 - max(0.0, (u - 0.6) / 0.4))
    if nombre == "der":
        return (1, 1, 4 * u, 2.2 * u * u, 0.0, 1.0 - max(0.0, (u - 0.6) / 0.4))
    return (1, 1, 0, 0, 0, 1.0 - u)


# ── Logo animado ────────────────────────────────────────────────────────────────────────────────────────────────────

def _ruido(i, k=0):
    """Número pseudoaleatorio determinista en [-1, 1] (para inclinaciones iniciales de las letras)."""
    x = math.sin(i * 12.9898 + k * 78.233) * 43758.5453
    return (x - math.floor(x)) * 2.0 - 1.0


class LogoTortunavy:
    """TORTUNAVY: las letras caen una a una y rebotan (la última cae justo antes del golpe), la concha se planta encima en
    el golpe con destello, y después un brillo diagonal recorre el logo de vez en cuando. Con `lineas=2` se apila como
    TORTU / NAVY (más grande en el vertical)."""

    TEXTO = "TORTUNAVY"

    def __init__(self, ancho: int, tam_letra: int, lineas: int = 1):
        st = ESTILOS["logo"]
        partes = [self.TEXTO] if lineas == 1 else ["TORTU", "NAVY"]
        tam = tam_letra
        for _ in range(3):
            f = fuente(st["fuente"], tam)
            kern = tam * 0.015
            anchos = [sum(f.getlength(c) for c in p) + kern * (len(p) - 1) for p in partes]
            if max(anchos) > ancho:
                tam = int(tam * ancho / max(anchos) * 0.98)
                continue
            break
        self.tam = tam
        self.letras = []  # (sprite, x, fila)
        for fila, texto in enumerate(partes):
            adv = [f.getlength(c) for c in texto]
            x = -anchos[fila] / 2.0
            for c, a_ in zip(texto, adv):
                self.letras.append((sprite_texto(c, "logo", tam, 0), x + a_ / 2.0, fila))
                x += a_ + kern
        self.ancho = max(anchos)
        self.alto = self.letras[0][0].shape[0]
        self.filas = len(partes)
        self.paso_fila = self.alto * 0.80
        self.concha = sprite_icono("concha", int(tam * (1.05 if lineas == 1 else 0.95)))
        self.chispa = sprite_icono("chispa", int(tam * 0.30))

    def dibujar(self, lienzo, t_rel: float, cx: float, cy: float, escala: float = 1.0):
        """t_rel: segundos respecto al golpe (0 = golpe). Las letras caen antes, la concha en el golpe."""
        n = len(self.letras)
        paso = 0.062          # entre aterrizajes de letras consecutivas
        t_caida = 0.26        # duración de la caída
        pad = int(self.tam * 0.9)
        extra = (self.filas - 1) * self.paso_fila
        W, H = int(self.ancho + 2 * pad), int(self.alto + extra + 2 * pad + self.tam * 1.3)
        buf = np.zeros((H, W, 4), np.uint8)
        bx, by0 = W / 2.0, pad + self.tam * 1.15 + self.alto / 2.0
        alguna = False
        for i, (sp, lx, fila) in enumerate(self.letras):
            by = by0 + fila * self.paso_fila
            t_land = -(n - 1 - i) * paso - 0.03  # la última letra aterriza un pelín antes del golpe
            tl = t_rel - (t_land - t_caida)
            if tl < 0:
                continue
            alguna = True
            tau = t_rel - t_land
            if tau < 0:
                u = 1.0 - (-tau) / t_caida  # 0 arriba … 1 abajo
                y = -(1.0 - u * u) * (self.alto * 3.2 + H * 0.35)
                rot = _ruido(i) * 28.0 * (1.0 - u)
                sx = sy = 1.0
            else:
                y = bote(tau, 0.20, self.alto * 0.34)
                aplasta = math.exp(-tau * 16.0) * math.cos(tau * 30.0)
                sx, sy = 1.0 + 0.16 * aplasta, 1.0 - 0.24 * aplasta
                rot = _ruido(i, 1) * 5.0 * math.exp(-tau * 7.0) * math.sin(tau * 22.0)
            if tau > 0.9:  # balanceo suave cuando ya está asentado
                y += math.sin((tau - 0.9) * 2.6 - i * 0.55) * self.tam * 0.018
            pegar(buf, sp, bx + lx, by + y, sx, sy, rot, 1.0)
        # concha: cae sobre el centro de la primera fila en el golpe
        tc = t_rel + 0.34
        if tc >= 0:
            if tc < 0.34:
                u = tc / 0.34
                yc = -(1.0 - u * u) * (self.alto * 3.0 + H * 0.4)
                rot = -20.0 * (1 - u)
                sx = sy = 1.0
            else:
                tau = tc - 0.34
                yc = bote(tau, 0.24, self.alto * 0.30, 0.55, 0.3)
                ap = math.exp(-tau * 14.0) * math.cos(tau * 28.0)
                sx, sy = 1.0 + 0.2 * ap, 1.0 - 0.28 * ap
                rot = 5.0 * math.exp(-tau * 6.0) * math.sin(tau * 18.0)
                if tau > 0.9:
                    yc += math.sin((tau - 0.9) * 3.0) * self.tam * 0.03
            pegar(buf, self.concha, bx, by0 - self.tam * 0.36 - self.concha.shape[0] * 0.40 + yc, sx, sy, rot, 1.0)
        elif not alguna:
            return
        # brillo diagonal que recorre las letras (solo dentro del alfa del logo)
        for t0 in (0.55, 2.3):
            u = (t_rel - t0) / 0.55
            if 0.0 <= u <= 1.0:
                xs = np.arange(W, dtype=np.float32)[None, :]
                ys = np.arange(H, dtype=np.float32)[:, None]
                pos = -0.2 * W + u * 1.4 * W
                d = (xs - pos) + (ys - H / 2.0) * 0.45
                banda = np.clip(1.0 - np.abs(d) / (self.tam * 0.55), 0, 1) ** 1.6
                a = buf[..., 3:4].astype(np.float32) / 255.0
                buf[..., :3] = np.clip(buf[..., :3].astype(np.float32) + banda[..., None] * a * 150.0, 0, 255).astype(np.uint8)
        pegar(lienzo, buf, cx, cy, escala, escala, 0.0, 1.0)
        # chispas al golpe y con cada brillo
        for t0, px_, py_ in ((0.12, -0.42, -0.22), (0.20, 0.38, -0.30), (0.30, 0.05, 0.36), (0.60, -0.2, 0.05), (2.38, 0.22, 0.12), (2.5, -0.36, -0.1)):
            u = (t_rel - t0) / 0.42
            if 0.0 <= u <= 1.0:
                s_ = math.sin(u * math.pi)
                pegar(lienzo, self.chispa, cx + px_ * self.ancho * escala, cy + py_ * (self.alto + extra) * escala * 1.4, s_ * escala, s_ * escala,
                      u * 90.0, 1.0)
