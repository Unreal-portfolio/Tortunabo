# -*- coding: utf-8 -*-
"""Material, tarjetas sustitutas y efectos de imagen del montaje del tráiler de Tortunavy.

Contiene:
- Fuente / Catalogo: los planos capturados (PNG o JPG a 30 fps) y la búsqueda de sustitutos por etiquetas.
- Tarjeta: plano generado (fondo animado con rayos, icono y nombre del plano pendiente) para poder montar sin material.
- Efectos: cámara (zoom, temblor, giro, separación RGB), destello, viñeteo, grano, color de feria, desenfoques de
  movimiento y de zoom, glitch, marco de cómic para los fotogramas congelados y pantalla partida.

Todas las funciones trabajan con imágenes BGR uint8 y son deterministas (el «azar» sale del número de fotograma), así
que un mismo fotograma sale idéntico se calcule en el proceso que se calcule.
"""
from __future__ import annotations

import json
import math
import os
import re
import unicodedata
from collections import OrderedDict

import cv2
import numpy as np

import mt_texto as T

RE_FOTOGRAMA = re.compile(r"^frame_(\d+)\.(png|jpg|jpeg)$", re.I)
PROXY_ANCHO = 960


def sin_acentos(s: str) -> str:
    return "".join(c for c in unicodedata.normalize("NFD", s.lower()) if unicodedata.category(c) != "Mn")


def azar(f: int, k: int = 0, n: int = 3) -> np.ndarray:
    """n números en [-1, 1] deterministas para el fotograma f y la semilla k."""
    return np.random.default_rng((int(f) * 7919 + int(k) * 104729 + 12345) & 0xFFFFFFFF).uniform(-1, 1, n)


# ── Material ────────────────────────────────────────────────────────────────────────────────────────────────────────

_CACHE_IMG: "OrderedDict[tuple, np.ndarray]" = OrderedDict()
_CACHE_MAX = 14


def _leer(ruta: str, proxy_dir: str | None):
    """Lee un fotograma; en previsualización usa (y crea) una copia reducida en caché de disco."""
    clave = (ruta, bool(proxy_dir))
    if clave in _CACHE_IMG:
        _CACHE_IMG.move_to_end(clave)
        return _CACHE_IMG[clave]
    img = None
    if proxy_dir:
        try:  # la fecha y el tamaño van en el nombre: si se vuelve a capturar un plano, la copia reducida se rehace sola
            st = os.stat(ruta)
            marca = f"{int(st.st_mtime)}_{st.st_size}"
        except OSError:
            marca = "0"
        rel = os.path.basename(os.path.dirname(ruta)) + "__" + os.path.splitext(os.path.basename(ruta))[0] + "__" + marca + ".jpg"
        pr = os.path.join(proxy_dir, rel)
        if os.path.exists(pr):
            img = cv2.imread(pr, cv2.IMREAD_COLOR)
        if img is None:
            img = cv2.imread(ruta, cv2.IMREAD_COLOR)
            if img is not None:
                if img.shape[1] > PROXY_ANCHO:
                    h = int(round(img.shape[0] * PROXY_ANCHO / img.shape[1]))
                    img = cv2.resize(img, (PROXY_ANCHO, h), interpolation=cv2.INTER_AREA)
                try:
                    os.makedirs(proxy_dir, exist_ok=True)
                    tmp = pr + f".{os.getpid()}.tmp.jpg"
                    cv2.imwrite(tmp, img, [cv2.IMWRITE_JPEG_QUALITY, 93])
                    os.replace(tmp, pr)
                except OSError:
                    pass
    else:
        img = cv2.imread(ruta, cv2.IMREAD_COLOR)
    if img is None:
        return None
    _CACHE_IMG[clave] = img
    if len(_CACHE_IMG) > _CACHE_MAX:
        _CACHE_IMG.popitem(last=False)
    return img


class Fuente:
    """Un plano capturado: carpeta con frame_00000.png/.jpg… a 30 fps."""

    def __init__(self, nombre, carpeta, ficheros, etiquetas=(), desc="", proxy_dir=None, fps=30.0):
        self.nombre = nombre
        self.carpeta = carpeta
        self.ficheros = list(ficheros)
        self.etiquetas = list(etiquetas)
        self.desc = desc
        self.proxy_dir = proxy_dir
        self.fps = fps
        self._ultimo = None

    @property
    def n(self):
        return len(self.ficheros)

    @property
    def segundos(self):
        return self.n / self.fps

    def frame(self, i: int) -> np.ndarray:
        i = min(max(int(i), 0), self.n - 1)
        img = _leer(self.ficheros[i], self.proxy_dir)
        if img is None:  # fotograma a medio escribir o ilegible: se repite el último bueno
            if self._ultimo is not None:
                return self._ultimo
            for j in range(i - 1, -1, -1):
                img = _leer(self.ficheros[j], self.proxy_dir)
                if img is not None:
                    break
            if img is None:
                img = np.zeros((1080, 1920, 3), np.uint8)
        self._ultimo = img
        return img

    def describir(self):
        return {"nombre": self.nombre, "carpeta": self.carpeta, "ficheros": self.ficheros, "etiquetas": self.etiquetas,
                "desc": self.desc, "fps": self.fps}


SINONIMOS = {
    "beach": "playa", "fly": "vuelo", "flight": "vuelo", "top": "cenital", "eggs": "huevos", "egg": "huevos", "hatch": "salida",
    "fortress": "fortaleza", "castle": "fortaleza", "catapult": "catapulta", "trampoline": "trampolin", "crab": "cangrejo",
    "gull": "gaviota", "quad": "quads", "quads": "quads", "mine": "mina", "storm": "tormenta", "worm": "gusano",
    "turtles": "tortugas", "turtle": "tortuga", "cliff": "meta", "chest": "cofre", "enemies": "enemigos", "shop": "tienda",
    "coop": "cooperativo", "jungle": "selva", "volcano": "volcan", "bridge": "puente", "cave": "cueva", "waterfall": "cascada",
    "group": "grupo", "podium": "campeon", "macro": "macro", "general": "general", "lobby": "lobby", "sea": "mar", "dive": "zambullida",
    "explosion": "explosion", "boom": "explosion", "ragdoll": "ragdoll", "run": "carrera", "race": "carrera", "climb": "tortuga",
    "orbit": "fortaleza", "launch": "vuelo",
}


def normalizar_etiquetas(etiquetas):
    out = set()
    for e in etiquetas:
        e = sin_acentos(str(e)).strip()
        if not e:
            continue
        out.add(e)
        if e in SINONIMOS:
            out.add(SINONIMOS[e])
    return out


def _json_tolerante(ruta):
    """Lee un JSON aunque esté a medio escribir (o relleno de ceros, como deja Windows un fichero recién creado)."""
    try:
        with open(ruta, "rb") as fh:
            datos = fh.read().replace(b"\x00", b"").strip()
        if not datos:
            return None
        return json.loads(datos.decode("utf-8"))
    except (OSError, ValueError):
        return None


class Catalogo:
    """Los planos que hay en Saved/Trailer/footage/ y la resolución de cada plano pedido por la lista de cortes."""

    def __init__(self, raiz: str, proxy_dir: str | None = None, log=print):
        self.raiz = raiz
        self.proxy_dir = proxy_dir
        self.log = log
        self.fuentes: dict[str, Fuente] = {}
        self._usos: dict[str, int] = {}
        self._escanear()

    def _escanear(self):
        indice = _json_tolerante(os.path.join(self.raiz, "index.json"))
        if isinstance(indice, list):
            indice = {(d.get("name") or d.get("nombre")): d for d in indice if isinstance(d, dict)}
        indice = indice if isinstance(indice, dict) else {}
        if not os.path.isdir(self.raiz):
            return
        for entrada in sorted(os.scandir(self.raiz), key=lambda e: e.name):
            if not entrada.is_dir():
                continue
            marcos = []
            for f in os.scandir(entrada.path):
                m = RE_FOTOGRAMA.match(f.name)
                if m:
                    marcos.append((int(m.group(1)), f.path))
            if not marcos:
                continue
            marcos.sort()
            meta = _json_tolerante(os.path.join(entrada.path, "shot.json")) or indice.get(entrada.name) or {}
            self.fuentes[entrada.name] = Fuente(
                entrada.name, entrada.path, [p for _, p in marcos], meta.get("tags", []) or [], meta.get("desc", "") or "",
                self.proxy_dir, float(meta.get("fps", 30) or 30))

    def resolver(self, plano, alt=(), etiquetas=(), permitir_test=False):
        """Devuelve (Fuente|None, modo, nota). Orden: plano exacto → alternativas explícitas → mejor coincidencia de
        etiquetas → None (tarjeta generada)."""
        if plano in self.fuentes:
            return self.fuentes[plano], "real", ""
        for a in alt or ():
            if a in self.fuentes:
                return self.fuentes[a], "alt", f"sustituye {plano} por {a}"
        pedidas = normalizar_etiquetas(etiquetas)
        mejor, mejor_p = None, 0.0
        for nombre, fu in self.fuentes.items():
            propias = normalizar_etiquetas(fu.etiquetas) | normalizar_etiquetas(re.split(r"[_\W]+", nombre))
            if "test" in propias and not permitir_test and "test" not in pedidas:
                continue
            comun = pedidas & propias
            if not comun:
                continue
            p = len(comun) - 0.15 * self._usos.get(nombre, 0) + min(fu.segundos, 6) * 0.01
            if p > mejor_p:
                mejor, mejor_p = fu, p
        if mejor is not None:
            self._usos[mejor.nombre] = self._usos.get(mejor.nombre, 0) + 1
            return mejor, "etiquetas", f"sustituye {plano} por {mejor.nombre} (etiquetas comunes: {sorted(pedidas & (normalizar_etiquetas(mejor.etiquetas) | normalizar_etiquetas(re.split(r'[_\W]+', mejor.nombre))))})"
        return None, "tarjeta", f"sin material para {plano}: tarjeta generada"


# ── Tarjetas generadas ──────────────────────────────────────────────────────────────────────────────────────────────

# claves de etiqueta → (color arriba BGR, color abajo BGR, icono)
_TEMAS = [
    (("volcan", "magma"), ((30, 60, 200), (20, 150, 250)), "calavera"),
    (("cueva",), ((90, 40, 70), (150, 80, 110)), "calavera"),
    (("selva", "jungla"), ((60, 150, 50), (110, 210, 120)), "concha"),
    (("cascada", "agua"), ((190, 150, 40), (240, 210, 120)), "ola"),
    (("puente",), ((60, 110, 150), (110, 180, 230)), "concha"),
    (("tormenta",), ((90, 70, 60), (170, 150, 130)), "ola"),
    (("gusano",), ((70, 110, 170), (120, 170, 220)), "calavera"),
    (("gaviota", "pelicano"), ((230, 190, 100), (250, 230, 170)), "calavera"),
    (("mina", "explosion"), ((20, 60, 170), (40, 170, 250)), "calavera"),
    (("huevos", "salida"), ((120, 190, 240), (200, 240, 255)), "huevo"),
    (("meta", "zambullida", "mar"), ((190, 140, 30), (230, 200, 90)), "ola"),
    (("campeon", "podio"), ((30, 150, 240), (110, 220, 255)), "concha"),
    (("lobby", "tienda", "general"), ((140, 80, 40), (60, 150, 240)), "concha"),
    (("playa", "vuelo", "cenital"), ((200, 160, 60), (150, 210, 240)), "ola"),
    (("fortaleza",), ((80, 150, 200), (150, 210, 240)), "concha"),
    (("macro", "escala"), ((70, 120, 160), (140, 200, 235)), "concha"),
]


class Tarjeta:
    """Plano generado: degradado, rayos giratorios, icono que rebota y, si `etiqueta`, el nombre del plano pendiente."""

    def __init__(self, nombre, etiquetas, w, h, etiqueta=True, tema=None):
        self.w, self.h, self.etiqueta = w, h, etiqueta
        self.nombre = nombre
        tags = normalizar_etiquetas(list(etiquetas) + re.split(r"[_\W]+", nombre))
        ca, cb, icono = (60, 60, 60), (150, 150, 150), "concha"
        for claves, cols, ic in _TEMAS:
            if any(k in tags for k in claves):
                (ca, cb), icono = cols, ic
                break
        else:
            hsh = sum(ord(c) for c in nombre)
            ca = (int(80 + hsh * 7 % 120), int(60 + hsh * 13 % 120), int(60 + hsh * 5 % 100))
            cb = (min(ca[0] + 70, 255), min(ca[1] + 70, 255), min(ca[2] + 70, 255))
        if tema == "logo":
            ca, cb, icono = (75, 35, 8), (150, 75, 25), None
        elif tema == "marino":
            ca, cb, icono = (60, 25, 6), (110, 55, 18), None
        self.tema = tema
        t = np.linspace(0, 1, h, dtype=np.float32)[:, None, None]
        base = np.array(ca, np.float32) * (1 - t) + np.array(cb, np.float32) * t
        self.base = np.repeat(base, w, axis=1).astype(np.uint8)
        # mapa angular a 1/4 de resolución para los rayos
        qh, qw = max(h // 4, 2), max(w // 4, 2)
        yy, xx = np.mgrid[0:qh, 0:qw].astype(np.float32)
        self.ang = (np.arctan2(yy - qh / 2.0, xx - qw / 2.0) / (2 * math.pi) + 0.5)
        self.rad = np.sqrt(((xx - qw / 2.0) / qw) ** 2 + ((yy - qh / 2.0) / qh) ** 2)
        u = w / 1920.0 if w >= h else h / 1920.0
        self.u = u
        self.icono = T.sprite_icono(icono, int(min(w, h) * 0.34)) if icono else None
        self.rot_rayos = 0.05 + (sum(ord(c) for c in nombre) % 7) * 0.01
        if etiqueta:
            self.rotulo = T.sprite_texto(nombre.upper().replace("_", " "), "blanco", int(58 * (min(w, h) / 1080.0)), int(w * 0.9))
            self.aviso = T.sprite_texto("PLANO PENDIENTE", "cartel_azul", int(34 * (min(w, h) / 1080.0)), int(w * 0.6))

    def frame(self, t: float) -> np.ndarray:
        k = 14 if self.tema != "logo" else 18
        fase = (self.ang * k + t * self.rot_rayos * k) % 1.0
        d = np.abs(fase - 0.5)                       # 0 en un extremo del rayo, 0,5 en el otro
        rayos = np.clip((d - 0.25) * 10.0 + 0.5, 0.0, 1.0)  # bordes suaves: nada de dientes de sierra
        atenuar = np.clip((self.rad - 0.03) * 8.0, 0.0, 1.0) * np.clip(1.25 - self.rad, 0.45, 1.0)
        m = (rayos * atenuar * 255).astype(np.uint8)
        m = cv2.resize(m, (self.w, self.h), interpolation=cv2.INTER_CUBIC)
        img = cv2.addWeighted(self.base, 1.0, cv2.merge([m, m, m]), 0.16 if self.tema != "logo" else 0.13, 0)
        if self.icono is not None:
            b = abs(math.sin(t * 2.6 * math.pi * 0.5))
            T.pegar(img, self.icono, self.w / 2, self.h * 0.47 - b * self.h * 0.03, 1.0 + 0.04 * math.sin(t * 5), rot=6 * math.sin(t * 2.2))
        if self.etiqueta:
            T.pegar(img, self.aviso, self.w / 2, self.h * 0.13)
            T.pegar(img, self.rotulo, self.w / 2, self.h * 0.85)
        return img


# ── Cámara ──────────────────────────────────────────────────────────────────────────────────────────────────────────

def camara(src: np.ndarray, vw: int, vh: int, zoom: float = 1.0, foco=(0.5, 0.5), dx: float = 0.0, dy: float = 0.0,
           rot: float = 0.0, espejo: bool = False, rgb: float = 0.0, borde=cv2.BORDER_REFLECT) -> np.ndarray:
    """Recorta/escala `src` a un viewport vw×vh con una sola transformación afín (zoom, foco, temblor, giro) y, si
    `rgb` > 0, con separación cromática radial (los canales R y B con zoom ligeramente distinto)."""
    sh, sw = src.shape[:2]
    if espejo:
        src = cv2.flip(src, 1)
        foco = (1.0 - foco[0], foco[1])
    s = max(vw / sw, vh / sh) * zoom
    if s < 0.7:  # reducción fuerte: se baja primero con INTER_AREA para que no se vean dientes
        p = min(1.0, s * 1.25)
        src = cv2.resize(src, (max(int(sw * p), 2), max(int(sh * p), 2)), interpolation=cv2.INTER_AREA)
        s /= p
        sh, sw = src.shape[:2]
    ww, hh = vw / s, vh / s  # tamaño de la ventana en píxeles de la fuente
    cx = min(max(foco[0] * sw, ww / 2.0), sw - ww / 2.0) if ww < sw else sw / 2.0
    cy = min(max(foco[1] * sh, hh / 2.0), sh - hh / 2.0) if hh < sh else sh / 2.0
    th = math.radians(rot)
    c, sn = math.cos(th) * s, math.sin(th) * s

    def matriz(esc=1.0):
        a, b, d, e = c * esc, -sn * esc, sn * esc, c * esc
        return np.array([[a, b, vw / 2.0 + dx - (a * cx + b * cy)], [d, e, vh / 2.0 + dy - (d * cx + e * cy)]], np.float64)

    if rgb <= 0.3:
        return cv2.warpAffine(src, matriz(), (vw, vh), flags=cv2.INTER_LINEAR, borderMode=borde)
    ch = cv2.split(src)
    salida = []
    for i, esc in enumerate((1.0 - rgb * 0.0016, 1.0, 1.0 + rgb * 0.0016)):  # B, G, R
        salida.append(cv2.warpAffine(ch[i], matriz(esc), (vw, vh), flags=cv2.INTER_LINEAR, borderMode=borde))
    d_px = int(round(rgb * 0.5))  # y un desplazamiento horizontal opuesto en R y B
    if d_px:
        salida[0] = np.roll(salida[0], -d_px, axis=1)
        salida[2] = np.roll(salida[2], d_px, axis=1)
    return cv2.merge(salida)


# ── Efectos de imagen ───────────────────────────────────────────────────────────────────────────────────────────────

class Grado:
    """Color de feria (cálido y saturado), viñeteo y grano; precalcula todo lo que depende del tamaño."""

    def __init__(self, w, h, sat=1.22, calidez=1.0, contraste=0.30, brillo=0.0, vineta=0.34, grano=3.0):
        self.w, self.h = w, h
        self.sat = sat
        self.vineta = vineta
        x = np.arange(256, dtype=np.float32) / 255.0
        s = x * x * (3 - 2 * x)
        y = x * (1 - contraste) + s * contraste + brillo
        y = np.clip(y, 0, 1)
        r = np.clip(y * (1 + 0.055 * calidez), 0, 1)
        g = np.clip(y * (1 + 0.006 * calidez), 0, 1)
        b = np.clip(y * (1 - 0.070 * calidez) + 0.012 * (1 - y), 0, 1)
        self.lut = np.stack([b, g, r], 1).reshape(256, 1, 3) * 255.0
        self.lut = np.clip(self.lut + 0.5, 0, 255).astype(np.uint8)
        # viñeteo
        yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
        d = np.sqrt(((xx - w / 2.0) / (w / 2.0)) ** 2 + ((yy - h / 2.0) / (h / 2.0)) ** 2) / math.sqrt(2)
        self.vig = np.clip((d - 0.45) / 0.55, 0, 1) ** 1.7
        # banco de grano (monocromo, centrado en 128)
        self.grano = grano
        rng = np.random.default_rng(2024)
        self.tiles = []
        if grano > 0:
            for _ in range(4):
                n = rng.normal(128, grano, (h + 24, w + 24)).clip(0, 255).astype(np.uint8)
                self.tiles.append(cv2.merge([n, n, n]))

    def color(self, img):
        img = cv2.LUT(img, self.lut)
        if abs(self.sat - 1.0) > 0.01:
            g = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
            g3 = cv2.merge([g, g, g])
            img = cv2.addWeighted(img, self.sat, g3, 1.0 - self.sat, 0)
        return img

    def vigneta(self, img, extra=0.0):
        k = self.vineta + extra
        if k <= 0.01:
            return img
        g = (255.0 * (1.0 - min(k, 0.95) * self.vig)).astype(np.uint8)
        return cv2.multiply(img, cv2.merge([g, g, g]), scale=1.0 / 255.0)

    def grano_frame(self, img, f):
        if not self.tiles:
            return img
        r = azar(f, 5, 3)
        t = self.tiles[int(abs(r[0]) * 3.99)]
        ox, oy = int((r[1] * 0.5 + 0.5) * 23), int((r[2] * 0.5 + 0.5) * 23)
        n = t[oy:oy + self.h, ox:ox + self.w]
        return cv2.addWeighted(img, 1.0, n, 1.0, -128.0)


def destello(img, a, color=(255, 255, 255)):
    """Mezcla hacia un color plano (BGR) con fuerza a∈[0,1]."""
    if a <= 0.004:
        return img
    a = min(a, 1.0)
    if color == (255, 255, 255):
        return cv2.convertScaleAbs(img, alpha=1.0 - a, beta=255.0 * a)
    plano = np.empty_like(img)
    plano[:] = color
    return cv2.addWeighted(img, 1.0 - a, plano, a, 0)


def oscurecer(img, a, color=(0, 0, 0)):
    return destello(img, a, color)


def desaturar(img, a):
    if a <= 0.004:
        return img
    g = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
    return cv2.addWeighted(img, 1.0 - a, cv2.merge([g, g, g]), a, 0)


def desenfoque_mov(img, longitud, eje="h"):
    """Desenfoque de movimiento lineal (caja) de `longitud` píxeles."""
    L = int(longitud)
    if L < 2:
        return img
    return cv2.blur(img, (L, 1) if eje == "h" else (1, L))


def desenfoque_zoom(img, cantidad, n=6):
    """Desenfoque radial aproximado: promedio de copias ampliadas desde el centro."""
    if cantidad <= 0.004:
        return img
    h, w = img.shape[:2]
    acc = img.astype(np.float32)
    for i in range(1, n):
        s = 1.0 + cantidad * i / (n - 1)
        M = np.array([[s, 0, w / 2.0 * (1 - s)], [0, s, h / 2.0 * (1 - s)]], np.float64)
        acc += cv2.warpAffine(img, M, (w, h), flags=cv2.INTER_LINEAR, borderMode=cv2.BORDER_REPLICATE).astype(np.float32)
    return (acc / n).astype(np.uint8)


def separar_rgb(img, px):
    """Separación cromática horizontal sobre una imagen ya compuesta."""
    d = int(round(px))
    if d == 0:
        return img
    b, g, r = cv2.split(img)
    return cv2.merge([np.roll(b, -d, axis=1), g, np.roll(r, d, axis=1)])


def glitch(img, f, amp):
    """Trozos horizontales desplazados + separación RGB: para el rayado de disco y los cortes «rotos»."""
    h, w = img.shape[:2]
    out = img.copy()
    r = azar(f, 9, 24)
    for i in range(6):
        y0 = int((r[i * 3] * 0.5 + 0.5) * (h - 8))
        alto = int((r[i * 3 + 1] * 0.5 + 0.5) * h * 0.10) + 4
        d = int(r[i * 3 + 2] * amp)
        out[y0:y0 + alto] = np.roll(out[y0:y0 + alto], d, axis=1)
    return separar_rgb(out, amp * 0.25)


# ── Marco de cómic para fotogramas congelados ───────────────────────────────────────────────────────────────────────

_CACHE_COMIC: dict = {}


def fondo_comic(w, h):
    clave = ("comic", w, h)
    if clave in _CACHE_COMIC:
        return _CACHE_COMIC[clave]
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    ang = np.arctan2(yy - h / 2.0, xx - w / 2.0) / (2 * math.pi) + 0.5
    rayos = (((ang * 22) % 1.0) < 0.5)
    base = np.empty((h, w, 3), np.float32)
    base[:] = (31, 210, 255)  # amarillo (BGR)
    base[rayos] = (20, 178, 255)
    # trama de puntos (halftone) que se pierde hacia el centro
    esp = max(10, int(min(w, h) / 46))
    gx, gy = (xx % esp) - esp / 2.0, (yy % esp) - esp / 2.0
    dist = np.sqrt(((xx - w / 2.0) / (w / 2.0)) ** 2 + ((yy - h / 2.0) / (h / 2.0)) ** 2)
    radio = np.clip((dist - 0.55) * esp * 0.75, 0, esp * 0.55)
    puntos = (np.sqrt(gx * gx + gy * gy) < radio)
    base[puntos] = (110, 60, 20)
    out = np.clip(base, 0, 255).astype(np.uint8)
    _CACHE_COMIC[clave] = out
    return out


def marco_comic(vista, frames_desde_congelar, rot=-2.6, escala=0.86):
    """Devuelve el fotograma congelado dentro de un panel de cómic (borde blanco y marino, sombra dura) sobre un
    fondo amarillo con rayos y trama de puntos. Los primeros fotogramas entran con un pequeño rebote."""
    h, w = vista.shape[:2]
    k = frames_desde_congelar
    ent = min(k / 5.0, 1.0)
    # rebote de la escala y del giro
    rebote = 1.0 + 0.05 * math.exp(-k * 0.55) * math.cos(k * 1.1)
    e = (1.0 - (1.0 - escala) * T.sale(ent) if k < 5 else escala) * (rebote if k >= 1 else 1.0)
    g = rot * T.sale(ent) * (1.0 + 0.4 * math.exp(-k * 0.5) * math.cos(k * 0.9))
    fondo = fondo_comic(w, h).copy()
    cx, cy = w / 2.0, h / 2.0
    pw, ph = w * e, h * e
    b1, b2 = max(4, int(min(w, h) * 0.011)), max(4, int(min(w, h) * 0.010))
    th = math.radians(g)
    c, s = math.cos(th), math.sin(th)

    def rect(ancho, alto, ox=0.0, oy=0.0):
        pts = []
        for sx, sy in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
            x, y = sx * ancho / 2.0, sy * alto / 2.0
            pts.append((cx + ox + x * c - y * s, cy + oy + x * s + y * c))
        return np.round(np.array(pts)).astype(np.int32)

    sombra = fondo.copy()
    cv2.fillConvexPoly(sombra, rect(pw + 2 * (b1 + b2), ph + 2 * (b1 + b2), w * 0.014, h * 0.024), (60, 25, 8))
    fondo = cv2.addWeighted(fondo, 0.45, sombra, 0.55, 0)
    cv2.fillConvexPoly(fondo, rect(pw + 2 * (b1 + b2), ph + 2 * (b1 + b2)), (75, 31, 11), lineType=cv2.LINE_AA)
    cv2.fillConvexPoly(fondo, rect(pw + 2 * b1, ph + 2 * b1), (255, 255, 255), lineType=cv2.LINE_AA)
    # imagen dentro del panel
    M = np.array([[c * e, -s * e, cx - (c * e * cx - s * e * cy)], [s * e, c * e, cy - (s * e * cx + c * e * cy)]], np.float64)
    mask = np.zeros((h, w), np.uint8)
    cv2.fillConvexPoly(mask, rect(pw, ph), 255, lineType=cv2.LINE_AA)
    warped = cv2.warpAffine(vista, M, (w, h), flags=cv2.INTER_LINEAR, borderMode=cv2.BORDER_REPLICATE)
    m3 = cv2.merge([mask, mask, mask]).astype(np.float32) / 255.0
    return (warped.astype(np.float32) * m3 + fondo.astype(np.float32) * (1 - m3)).astype(np.uint8)


# ── Pantalla partida ────────────────────────────────────────────────────────────────────────────────────────────────

def rejilla(tipo: str, w: int, h: int, hueco: int):
    """Rectángulos (x, y, ancho, alto) de las celdas de una pantalla partida."""
    if tipo in ("2v", "2"):
        a = (w - hueco) // 2
        return [(0, 0, a, h), (a + hueco, 0, w - a - hueco, h)]
    if tipo == "2h":
        a = (h - hueco) // 2
        return [(0, 0, w, a), (0, a + hueco, w, h - a - hueco)]
    if tipo == "4":
        aw, ah = (w - hueco) // 2, (h - hueco) // 2
        return [(0, 0, aw, ah), (aw + hueco, 0, w - aw - hueco, ah), (0, ah + hueco, aw, h - ah - hueco),
                (aw + hueco, ah + hueco, w - aw - hueco, h - ah - hueco)]
    if tipo == "3":
        aw, ah = (w - hueco) // 2, (h - hueco) // 2
        return [(0, 0, aw, h), (aw + hueco, 0, w - aw - hueco, ah), (aw + hueco, ah + hueco, w - aw - hueco, h - ah - hueco)]
    raise ValueError(f"tipo de pantalla partida desconocido: {tipo}")
