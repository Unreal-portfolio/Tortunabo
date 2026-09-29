#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Montaje del tráiler de Tortunavy (ver CONTRATO.md).

Uso
    python montaje.py                      # los cuatro vídeos finales en Saved/Trailer/out/
    python montaje.py --preview            # versión a media resolución, preset rápido (sufijo _preview)
    python montaje.py --only 60_es,30_en   # solo algunos: 60_es 60_en 30_es 30_en
    python montaje.py --frames 1.6,3.2,56  # PNG sueltos de esos instantes (s) en Saved/Trailer/tmp/ (sin vídeo)
    python montaje.py --contacto 0:60:0.8  # hoja de contactos (inicio:fin:paso en s) en Saved/Trailer/tmp/
    python montaje.py --hoja               # qué segundos de cada plano necesita el montaje (para rodar)
    python montaje.py --check              # avisos de sincronía, solapes y planos que faltan; no renderiza
    Opciones: --workers N  --etiquetas (rotula el plano y su sustituto)  --estricto (falla si falta algún plano)
              --sin-audio  --formato (imprime esta documentación)

Requisitos: Python 3.14 con numpy, OpenCV y Pillow; ffmpeg de imageio_ffmpeg (libx264 y AAC). Nada más.

Cómo trabaja
    Cada fotograma es una función pura del número de fotograma, así que se reparte entre procesos (multiprocessing) y el
    proceso principal los escribe en orden por una tubería a ffmpeg (rawvideo → libx264 -crf 18 -preset slow, yuv420p,
    +faststart; la versión --preview usa media resolución y -preset veryfast). El audio se mezcla antes con numpy
    (música + efectos alineados por su pico, con la música bajando bajo los golpes gordos), se normaliza a -14 LUFS /
    -1 dBTP con loudnorm en dos pasadas y se codifica AAC a 192 kbps. Sin música hay un metrónomo sintético a 150 BPM y
    los efectos que falten se sustituyen por sonidos sintéticos sencillos.

    Planos que faltan: se busca el plano exacto en Saved/Trailer/footage/<plano>/ (PNG o JPG, frame_00000.*), luego las
    alternativas explícitas `alt`, luego el plano capturado con más etiquetas en común (`etiquetas` del corte o las de
    planos.json) y, en último caso, una tarjeta animada con el nombre del plano. Todo sustituto se avisa por consola.

Formato de la lista de cortes (edl_60.json, edl_30.json)
    Tiempos: un número son segundos; una cadena suma trozos con unidad: «12b» pulsos, «2c» compases (4 pulsos),
    «0.5s» segundos, «10f» fotogramas; se pueden sumar: «140b+0.9s». Con bpm=150 un pulso son 0,4 s = 12 fotogramas.

    Raíz:
        version, nombre, formato ("horizontal" 1920x1080 | "vertical" 1080x1920), fps (30), bpm (150),
        duracion ("150b"), musica y beatmap (rutas relativas a Saved/Trailer; opcionales, por defecto music/trailer_60.wav…),
        musica_db (ganancia de la música), objetivo_lufs,
        color {sat, calidez, contraste, brillo, vineta, grano}, zoom_formato (multiplica el zoom de todos los planos;
        el vertical usa más para recortar el 16:9 en la banda), banda {y, alto} (vertical, fracciones del lienzo),
        fundido_inicial / fundido_final (s), auto_golpes {amp, temblor, tipos}: pequeño zoom punch y temblor en cada golpe
        del beatmap real, ajuste_a_golpes (s): mueve rótulos y efectos de sonido al golpe del beatmap más cercano si está
        a menos de esa distancia (0 = desactivado),
        cortes [ … ], rotulos [ … ] y efectos [ … ] y sfx [ … ] con tiempos absolutos.

    Corte (se encadenan; cada uno empieza donde acaba el anterior salvo que tenga «at»):
        id, nota
        plano            nombre del plano capturado o «@logo» / «@marino» (fondos generados de la marca)
        alt              [planos alternativos si falta el principal]
        etiquetas        [etiquetas para buscar sustituto]; si no se ponen se toman de planos.json
                         planos.json también da por defecto el encuadre (foco, vfoco), el tramo bueno del plano («usar», en
                         fotogramas: --check avisa si un corte se sale) y efectos de sonido con «frame» (suenan cuando ese
                         fotograma del plano aparece en el corte)
        dur              duración del corte («2b»)
        at               (opcional) instante absoluto esperado; solo comprueba y avisa de derivas
        entrada          punto de entrada dentro del plano (s, o «36f» en fotogramas del plano)
        velocidad        número (0,5 = mitad), lista de [fracción_del_corte, velocidad] con rampas lineales, o un nombre:
                         camara_lenta, lento, golpe, entrada_rapida, acelerar, frenazo
        zoom             número o [z0, z1]: empuje suave durante el corte (por defecto [1.0, 1.06])
        foco / vfoco     [x, y] (0-1) del recuadro: reencuadre en horizontal / vertical
        espejo           true: voltea el plano
        fin              qué hacer si el plano se acaba: "hold" (por defecto), "bucle", "pingpong"
        transicion       en el inicio del corte: corte | whip_l | whip_r | whip_u | whip_d | zoom | flash | glitch |
                         fundido | dip_negro | dip_blanco (o {"tipo":…, "frames": 3})
        golpe            zoom punch al empezar el corte (amplitud, p. ej. 0.06)
        pulso            zoom punch suave en cada pulso dentro del corte
        temblor          temblor continuo (px a 1080p) o {"amp":…, "forma":"rampa"} durante todo el corte
        oscuro, desat    oscurece / desatura todo el corte (0-1)
        velo             {"alfa":0.65, "blur":10, "color":[b,g,r]}: velo azul marino desenfocado sobre el plano (fondo del logo)
        completo         (vertical) el plano ocupa todo el lienzo 9:16 en vez de la banda central
        efectos          lista de efectos con tiempos relativos al corte (ver abajo)
        congelar         {"t":…, "marco":"comic"|"simple"|"ninguno", "rot":-2.6, "escala":0.86, "flash":0.6}
        split            {"tipo":"2v"|"2h"|"3"|"4", "planos":[{plano, entrada, foco, velocidad, etiquetas, alt}…],
                          "hueco":10, "escalonado":0.1}  pantalla partida de 2 o 4 planos
        rotulos          lista de rótulos con «t» relativo al corte
        sfx              lista de efectos de sonido con «t» relativo al corte

    Efecto (dentro de «efectos»): {"fx": nombre, "t": inicio, "dur": …, "amp": …, "cada": intervalo, "n": veces, "forma": …}
        punch (zoom punch, amp 0.08)  shake (temblor con decaimiento, px)  flash (destello 0-1, "color":[b,g,r])
        rgb (separación RGB, px)      vineta (viñeteo extra)               desat (0-1)        oscurecer (0-1)
        glitch (px)                   inclinar (giro en grados)            zoomblur (0,08)
        forma: "decay" (por defecto, cae al final) | "const" | "rampa".

    Rótulo: {"tipo":"texto"|"logo"|"plataformas", "es":…, "en":…, "t": instante del GOLPE (cuando aterriza),
             "dur": cuánto dura desde el golpe ("cut", "fin" o tiempo), "estilo": marca|blanco|alerta|mar|verde|logo|
             marca_halo|cartel|cartel_azul|comic|chapa, "anim": slam|pop|caida|izq|der|arriba|abajo|zoom|fundido|ninguna,
             "salida": pop|fundido|caer|zoom|izq|der|corte, "pos": "centro"|"arriba"|"abajo"|"tercio_sup"|"tercio_inf"|[x,y],
             "tam": tamaño de letra a 1080 de ancho, "ancho": fracción máxima del lienzo (nunca se corta), "rot": grados,
             "giro0": inclinación inicial de la animación, "icono": concha|ola|huevo|calavera|chispa|monitor,
             "icono_lado": izq|der|arriba, "icono_n": copias, "icono_tam": relación con el texto, "icono_hueco": px,
             "cuenta": {"de":0, "a":5000, "dur":"2b"} con «{n}» en el texto,
             "impacto": {"shake":…, "flash":…, "rgb":…} o false, "sfx": nombre o false, "sfx_pre": nombre o false}
    El texto sale en español (es) o en inglés (en) según el vídeo; si falta «en» se usa «es».

    Efecto de sonido: {"t": instante, "nombre": "impact_big", "db": 0, "alinear": "pico"|"inicio"|"fin"}. El «pico» del WAV
    (su punto más fuerte) cae en «t»; un riser termina en «t» y un whoosh pasa por su mitad.
"""
from __future__ import annotations

import argparse
import bisect
import json
import math
import multiprocessing as mp
import os
import re
import subprocess
import sys
import time

import cv2
import numpy as np

AQUI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, AQUI)

import mt_audio as A  # noqa: E402
import mt_fx as X  # noqa: E402
import mt_texto as T  # noqa: E402

cv2.setNumThreads(1)

RAIZ = os.path.normpath(os.path.join(AQUI, "..", "..", "Saved", "Trailer"))
NOMBRES = {
    "60_es": ("edl_60.json", "es", "Tortunavy_Trailer_60s_ES"),
    "60_en": ("edl_60.json", "en", "Tortunavy_Trailer_60s_EN"),
    "30_es": ("edl_30.json", "es", "Tortunavy_Trailer_30s_vertical_ES"),
    "30_en": ("edl_30.json", "en", "Tortunavy_Trailer_30s_vertical_EN"),
}
FPS = 30

PRESETS_VEL = {
    "camara_lenta": 0.35, "lento": 0.55,
    "golpe": [[0, 1.6], [0.3, 0.3], [0.7, 0.3], [1, 1.8]],
    "entrada_rapida": [[0, 2.2], [0.45, 1.0], [1, 0.55]],
    "acelerar": [[0, 0.4], [1, 2.2]],
    "frenazo": [[0, 1.8], [0.5, 1.0], [0.85, 0.2], [1, 0.15]],
}

# Parámetros por defecto de cada efecto de imagen: (amp, dur en s, forma)
FX_DEFECTO = {
    "punch": (0.08, 0.14, "decay"), "shake": (12.0, 0.35, "decay"), "flash": (0.8, 0.14, "decay"),
    "rgb": (12.0, 0.30, "lineal"), "vineta": (0.25, 0.40, "decay"), "desat": (1.0, 0.50, "const"),
    "oscurecer": (0.6, 0.40, "const"), "glitch": (40.0, 0.25, "const"), "inclinar": (5.0, 0.40, "decay"),
    "zoomblur": (0.08, 0.25, "lineal"),
}

ZONAS = {
    "horizontal": {"centro": (0.5, 0.5), "arriba": (0.5, 0.17), "abajo": (0.5, 0.84), "tercio_sup": (0.5, 0.30), "tercio_inf": (0.5, 0.72)},
    "vertical": {"centro": (0.5, 0.5), "arriba": (0.5, 0.115), "abajo": (0.5, 0.885), "tercio_sup": (0.5, 0.30), "tercio_inf": (0.5, 0.72)},
}


def log(*a):
    print(*a, flush=True)


# ── Tiempo ──────────────────────────────────────────────────────────────────────────────────────────────────────────

class Reloj:
    def __init__(self, bpm: float, fps: int):
        self.bpm, self.fps = bpm, fps
        self.pulso = 60.0 / bpm

    def seg(self, x, defecto=None) -> float:
        """Segundos de «12b», «2c», «0.5s», «10f», «140b+0.9s» o un número."""
        if x is None:
            return defecto
        if isinstance(x, (int, float)):
            return float(x)
        s = str(x).strip().replace(" ", "")
        total, hallado = 0.0, False
        for signo, val, unidad in re.findall(r"([+-]?)(\d+(?:\.\d+)?|\.\d+)([bcsf]?)", s):
            v = float(val) * (-1 if signo == "-" else 1)
            u = unidad or "s"
            total += v * {"b": self.pulso, "c": self.pulso * 4, "s": 1.0, "f": 1.0 / self.fps}[u]
            hallado = True
        if not hallado:
            raise ValueError(f"tiempo no válido: {x!r}")
        return total

    def f(self, x, defecto=None) -> int:
        s = self.seg(x, None if defecto is None else defecto)
        return int(round(s * self.fps))


class Ev:
    """Evento de imagen en fotogramas absolutos."""
    __slots__ = ("tipo", "f0", "n", "amp", "tau", "forma", "extra", "sem")

    def __init__(self, tipo, f0, n, amp, tau=0.14, forma="decay", extra=None, sem=0):
        self.tipo, self.f0, self.n, self.amp, self.tau, self.forma, self.extra, self.sem = tipo, f0, max(int(n), 1), amp, tau, forma, extra, sem


def curva_velocidad(v):
    if v is None:
        return [(0.0, 1.0), (1.0, 1.0)]
    if isinstance(v, str):
        v = PRESETS_VEL[v]
    if isinstance(v, (int, float)):
        return [(0.0, float(v)), (1.0, float(v))]
    pts = sorted((float(a), float(b)) for a, b in v)
    if pts[0][0] > 0:
        pts.insert(0, (0.0, pts[0][1]))
    if pts[-1][0] < 1:
        pts.append((1.0, pts[-1][1]))
    return pts


def interp(pts, x):
    xs = [p[0] for p in pts]
    i = bisect.bisect_right(xs, x)
    if i <= 0:
        return pts[0][1]
    if i >= len(pts):
        return pts[-1][1]
    (x0, y0), (x1, y1) = pts[i - 1], pts[i]
    return y0 + (y1 - y0) * (x - x0) / max(x1 - x0, 1e-9)


class Tramo:
    """Un corte de la lista: plano (o celdas de pantalla partida), punto de entrada, velocidad y reencuadre."""

    def __init__(self, idx, d, f0, f1, es_celda=False):
        self.idx, self.d, self.f0, self.f1 = idx, d, f0, f1
        self.id = d.get("id", f"c{idx}")
        self.plano = d.get("plano", "")
        self.fuente = None
        self.modo = "tarjeta"
        self.nota = ""
        self.celdas: list[Tramo] = []
        self.pos = None
        self.entrada_f = 0.0
        self.fin = d.get("fin")
        self.es_celda = es_celda

    @property
    def n(self):
        return self.f1 - self.f0

    def pos_fuente(self, k: float) -> float:
        """Posición (fotograma fuente, fraccionaria) para el fotograma local k del corte."""
        if self.pos is None:
            return self.entrada_f + k
        n = len(self.pos)
        if 0 <= k < n - 1:
            i = int(math.floor(k))
            fr = k - i
            return float(self.pos[i] * (1 - fr) + self.pos[i + 1] * fr)
        if k < 0:
            return float(self.pos[0] + k * self.vel_ini)
        return float(self.pos[-1] + (k - (n - 1)) * self.vel_fin)


def forma_val(forma: str, x: float) -> float:
    """Envolvente de un efecto en x∈[0,1) de su duración."""
    if forma == "const":
        return 1.0
    if forma == "rampa":
        return x
    if forma == "lineal":
        return 1.0 - x
    return (1.0 - x) ** 2


def fila_iconos(ic: np.ndarray, n: int, hueco: int) -> np.ndarray:
    if n <= 1:
        return ic
    h, w = ic.shape[:2]
    out = np.zeros((h, w * n + hueco * (n - 1), 4), np.uint8)
    for i in range(n):
        out[:, i * (w + hueco):i * (w + hueco) + w] = ic
    return out


# ── Rótulos ─────────────────────────────────────────────────────────────────────────────────────────────────────────

class Rotulo:
    """Un rótulo (texto animado, contador, logo o placa de plataformas) con todos sus tiempos en fotogramas.
    `t` de la lista es el instante del GOLPE, cuando aterriza; la animación de entrada empieza un poco antes."""

    K_SALIDA = 4  # fotogramas de la animación de salida

    def __init__(self, d, m, base_s, fin_corte_s):
        self.d = d
        self.m = m
        self.tipo = d.get("tipo", "texto")
        r = m.reloj
        t_imp = m.ajustar_golpe(base_s + r.seg(d.get("t", 0)))
        self.t_imp = t_imp
        self.anim = d.get("anim", "slam")
        if self.tipo == "logo":
            pre = 1.1
        elif self.tipo == "plataformas":
            pre = T.PRE["slam"]
        else:
            pre = T.PRE.get(self.anim, 0.2)
        self.f_imp = int(round(t_imp * m.fps))
        self.f_ini = self.f_imp - int(round(pre * m.fps))
        dur = d.get("dur", "cut")
        if dur == "cut":
            fin_s = fin_corte_s
        elif dur == "fin":
            fin_s = m.n_frames / m.fps + 1.0
        else:
            fin_s = t_imp + r.seg(dur)
        self.f_fin = int(round(fin_s * m.fps))
        self.salida = d.get("salida", "pop")
        self.txt = d.get(m.idioma) or d.get("es") or d.get("txt") or ""
        self.giro0 = float(d.get("giro0", -9.0))
        self._sp = None
        self._logo = None

    # -- construcción de sprites --
    def _sprite(self, cuenta_txt=None):
        m, d = self.m, self.d
        tam = int(round(d.get("tam", 150) * m.u))
        ancho = int(d.get("ancho", 0.92) * m.W)
        estilo = d.get("estilo", "marca")
        txt = cuenta_txt if cuenta_txt is not None else self.txt
        icono = d.get("icono")
        if cuenta_txt is not None:
            final = self.txt.replace("{n}", str(int(d["cuenta"]["a"])))
            tam_f = T.tam_ajustado(final, estilo, tam, ancho, True)
            sp = T.sprite_texto(txt, estilo, tam_f, 0, True)
        else:
            sp = T.sprite_texto(txt, estilo, tam, ancho, False)
        if icono:
            ip = int(sp.shape[0] * d.get("icono_tam", 1.0) * (0.9 if d.get("icono_lado", "izq") == "arriba" else 0.95))
            ic = fila_iconos(T.sprite_icono(icono, max(ip, 8)), int(d.get("icono_n", 1)), int(6 * m.u))
            sp = T.combinar_icono(sp, ic, d.get("icono_lado", "izq"), int(d.get("icono_hueco", 14) * m.u))
        return sp

    def _pos(self):
        p = self.d.get("pos", "centro")
        if isinstance(p, str):
            p = ZONAS[self.m.formato][p]
        return p[0] * self.m.W, p[1] * self.m.H

    # -- dibujo --
    def dibujar(self, lienzo, f):
        if f < self.f_ini or f >= self.f_fin:
            return
        m = self.m
        if self.tipo == "logo":
            return self._logo_dibujar(lienzo, f)
        if self.tipo == "plataformas":
            return self._plataformas(lienzo, f)
        d = self.d
        tl = (f - self.f_ini) / m.fps
        if "cuenta" in d:
            c = d["cuenta"]
            dur = m.reloj.seg(c.get("dur", "2b"))
            u = min(max((f - self.f_imp) / m.fps / max(dur, 1e-6), 0.0), 1.0)
            v = c.get("de", 0) + (c["a"] - c.get("de", 0)) * (1 - (1 - u) ** 3)
            sp = self._sprite(self.txt.replace("{n}", str(int(round(v)))))
            fin_c = self.f_imp + int(round(dur * m.fps))
            rebote = 1.0 + 0.10 * math.exp(-(f - fin_c) / m.fps * 12) if f >= fin_c else 1.0
        else:
            if self._sp is None:
                self._sp = self._sprite()
            sp = self._sp
            rebote = 1.0
        if self.anim == "tecleo":
            n = int(len(self.txt) * min(1.0, (f - self.f_ini) / max(1, self.f_imp - self.f_ini)))
            sp = T.sprite_texto(self.txt[:max(n, 1)], d.get("estilo", "marca"), int(d.get("tam", 150) * m.u), int(d.get("ancho", 0.92) * m.W))
        sx, sy, rot, dx, dy, a = T.anim_entrada(self.anim if self.anim != "tecleo" else "ninguna", tl, self.giro0)
        k_out = self.f_fin - self.K_SALIDA
        if f >= k_out and self.salida != "corte":
            ox = T.anim_salida(self.salida, (f - k_out + 1) / m.fps, self.K_SALIDA / m.fps)
            sx, sy, rot, dx, dy, a = sx * ox[0], sy * ox[1], rot + ox[2], dx + ox[3], dy + ox[4], a * ox[5]
        # respiración suave mientras está quieto
        idle = 1.0 + 0.010 * math.sin(2 * math.pi * 1.3 * tl)
        cx, cy = self._pos()
        h, w = sp.shape[:2]
        T.pegar(lienzo, sp, cx + dx * w, cy + dy * h, sx * idle * rebote, sy * idle * rebote, rot + float(d.get("rot", 0.0)), a)

    def _logo_dibujar(self, lienzo, f):
        m, d = self.m, self.d
        if self._logo is None:
            ancho = int(d.get("ancho", 0.80) * m.W)
            self._logo = T.LogoTortunavy(ancho, int(d.get("tam", 300) * m.u), int(d.get("lineas", 1)))
        cx, cy = self._pos()
        self._logo.dibujar(lienzo, (f - self.f_imp) / m.fps, cx, cy, 1.0)

    def _plataformas(self, lienzo, f):
        """Hueco para las plataformas («PC» ahora; se pueden añadir más a la lista)."""
        m, d = self.m, self.d
        lista = d.get("lista", ["PC"])
        tam = int(d.get("tam", 70) * m.u)
        placas = []
        for nombre in lista:
            sp = T.sprite_texto(nombre, "cartel_azul", tam, 0)
            ic = T.sprite_icono("monitor", int(sp.shape[0] * 0.78))
            placas.append(T.combinar_icono(sp, ic, "izq", int(4 * m.u)))
        gap = int(24 * m.u)
        ancho_total = sum(p.shape[1] for p in placas) + gap * (len(placas) - 1)
        cx, cy = self._pos()
        x = cx - ancho_total / 2.0
        for i, p in enumerate(placas):
            tl = (f - (self.f_ini + int(round(0.12 * i * m.fps)))) / m.fps
            sx, sy, rot, dx, dy, a = T.anim_entrada("slam", tl, -7.0)
            T.pegar(lienzo, p, x + p.shape[1] / 2.0, cy + dy * p.shape[0], sx, sy, rot, a)
            x += p.shape[1] + gap


# ── El montaje ──────────────────────────────────────────────────────────────────────────────────────────────────────

AZUL_BGR = (75, 31, 11)   # azul marino de la marca en BGR
AZUL_OSC_BGR = (40, 14, 5)


class Montaje:
    """Una lista de cortes en un idioma: construye la línea de tiempo, los efectos, los rótulos y los efectos de sonido, y
    sabe pintar cualquier fotograma (`render_frame`)."""

    def __init__(self, edl_ruta, idioma, preview=False, resolucion=None, etiquetas=False, silencioso=False, footage=None):
        self.log = (lambda *a: None) if silencioso else log
        self.footage = footage or os.path.join(RAIZ, "footage")
        self.edl_ruta = edl_ruta
        self.idioma = idioma
        self.preview = preview
        self.debug_etiquetas = etiquetas
        with open(edl_ruta, encoding="utf-8") as fh:
            self.edl = json.load(fh)
        e = self.edl
        self.fps = int(e.get("fps", FPS))
        self.reloj = Reloj(float(e.get("bpm", 150)), self.fps)
        self.formato = e.get("formato", "horizontal")
        self.vertical = self.formato == "vertical"
        bw, bh = (1080, 1920) if self.vertical else (1920, 1080)
        esc = 0.5 if preview else 1.0
        self.W, self.H = int(bw * esc) // 2 * 2, int(bh * esc) // 2 * 2
        self.u = self.W / float(bw)
        self.n_frames = self.reloj.f(e["duracion"])
        self.zoom_formato = float(e.get("zoom_formato", 1.0))
        # viewport donde se ve el plano
        if self.vertical:
            b = e.get("banda", {"y": 0.229, "alto": 0.542})
            self.vy = int(self.H * b["y"]) // 2 * 2
            self.vh = int(self.H * b["alto"]) // 2 * 2
            self.vx, self.vw = 0, self.W
        else:
            self.vx = self.vy = 0
            self.vw, self.vh = self.W, self.H
        col = dict(sat=1.22, calidez=1.0, contraste=0.30, brillo=0.0, vineta=0.34, grano=3.0)
        col.update(e.get("color", {}))
        self.grado = Grado(self.vw, self.vh, col)
        # en el vertical los fondos de marca (@logo, @marino) ocupan todo el lienzo, sin la banda del plano
        self.grado_completo = Grado(self.W, self.H, col) if self.vertical else self.grado
        self.grano = X.Grado(self.W, self.H, sat=1.0, calidez=0.0, contraste=0.0, vineta=0.0, grano=col["grano"])
        # beatmap y música
        self.hits, self.beatmap = [], None
        rb = e.get("beatmap", "music/beatmap_30.json" if self.vertical else "music/beatmap_60.json")
        pb = os.path.join(RAIZ, rb)
        if os.path.exists(pb):
            try:
                with open(pb, encoding="utf-8") as fh:
                    self.beatmap = json.load(fh)
                self.hits = [(float(h["t"]), h.get("kind", "")) for h in self.beatmap.get("hits", [])]
            except (OSError, ValueError) as ex:
                self.log(f"  aviso: beatmap ilegible ({ex})")
        self.snap = float(e.get("ajuste_a_golpes", 0))
        # catálogo de planos previstos y material
        self.planos_info = {}
        pj = os.path.join(AQUI, "planos.json")
        if os.path.exists(pj):
            with open(pj, encoding="utf-8") as fh:
                self.planos_info = json.load(fh)
        self.proxy_dir = os.path.join(RAIZ, "tmp", "proxy") if preview else None
        self._res_previa = resolucion
        self.catalogo = None
        if resolucion is None:
            self.catalogo = X.Catalogo(self.footage, self.proxy_dir, self.log)
        self._fuentes: dict = {}
        self._usos_sust: dict = {}
        self.resolucion: dict = {}
        self._tarjetas: dict = {}
        self._cache_cong: tuple = (None, None)
        self.tramos: list[Tramo] = []
        self.rotulos: list[Rotulo] = []
        self.eventos: list[Ev] = []
        self.sfx: list[dict] = []
        self.avisos: list[str] = []
        self._construir()
        self.f_inicio = [t.f0 for t in self.tramos]
        self._fondo_v = None

    # ── construcción ────────────────────────────────────────────────────────────────────────────────────────────────
    def aviso(self, msg):
        if msg in self.avisos:  # los avisos repetidos (mismo plano en varios cortes) se cuentan una sola vez
            return
        self.avisos.append(msg)
        self.log("  aviso: " + msg)

    def ajustar_golpe(self, t):
        if self.snap <= 0 or not self.hits:
            return t
        mejor = min(self.hits, key=lambda h: abs(h[0] - t))
        return mejor[0] if abs(mejor[0] - t) <= self.snap else t

    def _fuente_de(self, info):
        if info is None:
            return None
        n = info["nombre"]
        if n not in self._fuentes:
            self._fuentes[n] = X.Fuente(n, info["carpeta"], info["ficheros"], info["etiquetas"], info["desc"], self.proxy_dir, info["fps"])
        return self._fuentes[n]

    def _resolver(self, clave, plano, alt, etiquetas):
        if plano.startswith("@"):
            return None, "generado", ""
        if self._res_previa is not None:
            r = self._res_previa.get(clave)
            if r is None:
                return None, "tarjeta", f"sin resolución previa para {clave}"
            self.resolucion[clave] = r
            return self._fuente_de(r["fuente"]), r["modo"], r["nota"]
        fu, modo, nota = self.catalogo.resolver(plano, alt, etiquetas)
        self.resolucion[clave] = {"modo": modo, "nota": nota, "fuente": fu.describir() if fu else None}
        return fu, modo, nota

    def _tabla_pos(self, tr, entrada_f, ratio):
        pts = curva_velocidad(tr.d.get("velocidad"))
        n = tr.n
        pos = np.zeros(n, np.float64)
        acc = float(entrada_f)
        for k in range(n):
            pos[k] = acc
            acc += interp(pts, (k + 0.5) / n) * ratio
        return pos, interp(pts, 0.0) * ratio, interp(pts, 1.0) * ratio

    def _preparar(self, tr, clave, dur_s):
        d = tr.d
        info = self.planos_info.get(tr.plano, {})
        etq = d.get("etiquetas") or self.planos_info.get(tr.plano, {}).get("etiquetas", [])
        fu, modo, nota = self._resolver(clave, tr.plano, d.get("alt", []), etq)
        tr.fuente, tr.modo, tr.nota = fu, modo, nota
        if modo in ("tarjeta",):
            self.aviso(nota)
        elif modo in ("alt", "etiquetas"):
            self.aviso(nota)
        ratio = (fu.fps / self.fps) if fu else 1.0
        ent_pedida = self.reloj.seg(d.get("entrada", 0))
        ent = ent_pedida
        tr.espejo = bool(d.get("espejo", False))
        if fu is not None and modo in ("alt", "etiquetas"):
            veces = self._usos_sust.get(fu.nombre, 0) + 1
            self._usos_sust[fu.nombre] = veces
            holgura = fu.segundos - dur_s
            ent = ((ent_pedida + (veces - 1) * 1.37) % holgura) if holgura > 0.3 else 0.0
            tr.espejo = tr.espejo ^ (veces % 2 == 0)
        tr.entrada_f = float(int(round(ent * (fu.fps if fu else self.fps))))
        pos, tr.vel_ini, tr.vel_fin = self._tabla_pos(tr, tr.entrada_f, ratio)
        tr.pos = pos
        if fu is not None and modo == "real":
            # comprobación de que el plano tiene material suficiente (con la entrada pedida)
            pos_p, _, _ = self._tabla_pos(tr, int(round(ent_pedida * fu.fps)), ratio)
            usar = info.get("usar")
            if usar:
                if d.get("congelar"):  # tras congelar solo se ve el fotograma congelado
                    pos_p = pos_p[:self.reloj.f(d["congelar"].get("t", 0)) + 1]
                lo, hi = int(math.floor(pos_p.min())), int(math.ceil(pos_p.max()))
                if not any(a <= lo and hi <= b for a, b in usar):
                    self.aviso(f"{tr.id}: {tr.plano} usa los fotogramas {lo}-{hi}, fuera del tramo bueno {usar}")
            if pos_p.max() + 1 > fu.n + 1:
                self.aviso(f"{tr.plano}: el corte {tr.id} necesita hasta {(pos_p.max() + 1) / fu.fps:.2f} s y el plano solo tiene {fu.segundos:.2f} s")
        tr.necesita_s = ((self._tabla_pos(tr, int(round(ent_pedida * (fu.fps if fu else self.fps))), ratio)[0].max() + 1) / (fu.fps if fu else self.fps))
        tr.ent_pedida = ent_pedida
        z = d.get("zoom", [1.0, 1.06])
        z = [z, z] if isinstance(z, (int, float)) else z
        tr.zoom = (float(z[0]) * self.zoom_formato, float(z[1]) * self.zoom_formato)
        info = self.planos_info.get(tr.plano, {})
        tr.foco = tuple(d.get("foco", info.get("foco", [0.5, 0.5])))
        tr.vfoco = tuple(d.get("vfoco", info.get("vfoco", d.get("foco", info.get("foco", [0.5, 0.5])))))
        tr.vel_max = float(np.max(np.abs(np.diff(pos)))) if len(pos) > 1 else 1.0
        tr.fin = d.get("fin") or ("hold" if modo == "real" else "pingpong")

    def _construir(self):
        e, r, fps = self.edl, self.reloj, self.fps
        f = 0
        cortes = e["cortes"]
        for i, d in enumerate(cortes):
            n = r.f(d["dur"])
            if "at" in d:
                dif = f - r.f(d["at"])
                if dif:
                    self.aviso(f"corte {d.get('id', i)}: empieza en el fotograma {f} pero «at» pide {r.f(d['at'])} (deriva de {dif} fotogramas)")
            tr = Tramo(i, d, f, f + n)
            self.tramos.append(tr)
            f += n
        if f != self.n_frames:
            self.aviso(f"los cortes suman {f} fotogramas y la duración pide {self.n_frames}")
            if f < self.n_frames:
                self.tramos[-1].f1 = self.n_frames
            else:
                while self.tramos and self.tramos[-1].f0 >= self.n_frames:
                    self.tramos.pop()
                self.tramos[-1].f1 = self.n_frames
        for tr in self.tramos:
            d = tr.d
            dur_s = tr.n / self.fps
            self._preparar(tr, tr.id, dur_s)
            if "split" in d:
                sp = d["split"]
                for j, cd in enumerate(sp["planos"]):
                    cd = dict(cd)
                    cd.setdefault("velocidad", d.get("velocidad"))
                    cd.setdefault("zoom", d.get("zoom", [1.0, 1.06]))
                    c = Tramo(-1, cd, tr.f0, tr.f1, True)
                    self._preparar(c, f"{tr.id}#{j}", dur_s)
                    tr.celdas.append(c)
            # congelado
            tr.cong = None
            if d.get("congelar"):
                c = dict(d["congelar"])
                tr.cong = {"k": r.f(c.get("t", 0)), "marco": c.get("marco", "comic"), "rot": float(c.get("rot", -2.6)),
                           "escala": float(c.get("escala", 0.86))}
                if c.get("flash", 0.6) > 0:
                    self._ev("flash", tr.f0 / fps + r.seg(c.get("t", 0)), amp=c.get("flash", 0.6), dur=0.16)
                if c.get("sfx"):
                    self.sfx.append({"t": tr.f0 / fps + r.seg(c.get("t", 0)), "nombre": c["sfx"], "db": 0.0})
            # transición de entrada
            tr.trans = None
            tt = d.get("transicion")
            if tt and tt != "corte" and tr.idx > 0:
                if isinstance(tt, str):
                    tt = {"tipo": tt}
                tipo = tt["tipo"]
                k = int(tt.get("frames", {"whip_l": 3, "whip_r": 3, "whip_u": 3, "whip_d": 3, "zoom": 4, "glitch": 3, "fundido": 6,
                                          "dip_negro": 4, "dip_blanco": 4, "flash": 1}.get(tipo, 3)))
                tr.trans = (tipo, k)
                tb = tr.f0 / fps
                if tipo.startswith("whip") or tipo == "zoom":
                    self.sfx.append({"t": tb, "nombre": "whoosh_short", "db": 0.0})
                elif tipo == "flash":
                    self._ev("flash", tb, amp=0.65, dur=0.18)
                elif tipo == "glitch":
                    self._ev("glitch", tb - k / fps, dur=2 * k / fps, amp=45)
                    self._ev("rgb", tb - k / fps, dur=3 * k / fps, amp=16)
            elif tt and tt != "corte" and tr.idx == 0:
                pass
            self._eventos_corte(tr)
            if d.get("sfx_auto", True):
                self._sfx_plano(tr)
            for rd in d.get("rotulos", []):
                self._rotulo(rd, tr.f0 / fps, tr.f1 / fps)
            for sd in d.get("sfx", []):
                self._sfx(sd, tr.f0 / fps)
        for rd in e.get("rotulos", []):
            self._rotulo(rd, 0.0, self.n_frames / fps)
        for fd in e.get("efectos", []):
            self._efecto(fd, 0.0, self.n_frames / fps)
        for sd in e.get("sfx", []):
            self._sfx(sd, 0.0)
        self._golpes_automaticos()
        self.eventos.sort(key=lambda ev: ev.f0)
        self.sfx.sort(key=lambda s: s["t"])
        self._limpiar_sfx()

    def _limpiar_sfx(self):
        """Quita efectos casi simultáneos que solo ensucian: un whoosh pegado a otro y un golpe pequeño pegado a uno gordo."""
        limpio, ultimo_whoosh = [], -9.0
        gordos = [e["t"] for e in self.sfx if e["nombre"] in ("impact_big", "explosion_toy", "logo_sting")]
        for e in self.sfx:
            n, t = e["nombre"], e["t"]
            if n.startswith("whoosh"):
                if t - ultimo_whoosh < 0.2:
                    continue
                ultimo_whoosh = t
            elif n == "impact_small" and any(abs(t - g) < 0.08 for g in gordos):
                continue
            limpio.append(e)
        self.sfx = limpio

    def _ev(self, tipo, t_s, dur=None, amp=None, forma=None, extra=None, tau=None, cada=None, n=None, fin_s=None):
        a0, d0, f0_ = FX_DEFECTO[tipo]
        amp = a0 if amp is None else amp
        dur = d0 if dur is None else dur
        forma = forma or f0_
        tau = tau or 0.14
        veces, tt = 0, t_s
        cada_s = self.reloj.seg(cada) if cada else None
        while True:
            f0 = int(round(tt * self.fps))
            nf = int(math.ceil(6 * tau * self.fps)) if tipo == "punch" else max(1, int(round(dur * self.fps)))
            self.eventos.append(Ev(tipo, f0, nf, amp, tau, forma, extra, f0))
            veces += 1
            if not cada_s:
                break
            tt = t_s + veces * cada_s
            if n and veces >= n:
                break
            if not n and (fin_s is None or tt >= fin_s - 1e-6):
                break

    def _efecto(self, d, base_s, fin_s):
        c = d.get("color")
        self._ev(d["fx"], base_s + self.reloj.seg(d.get("t", 0)), self.reloj.seg(d["dur"]) if "dur" in d else None, d.get("amp"),
                 d.get("forma"), tuple(c) if c else None, d.get("tau"), d.get("cada"), d.get("n"), fin_s)

    def _sfx(self, d, base_s):
        t = self.ajustar_golpe(base_s + self.reloj.seg(d.get("t", 0)))
        ev = {"t": t, "nombre": d["nombre"], "db": float(d.get("db", 0.0))}
        if d.get("alinear"):
            ev["alinear"] = d["alinear"]
        if d.get("sin_duck"):
            ev["sin_duck"] = True
        self.sfx.append(ev)

    def _sfx_plano(self, tr):
        """Efectos de sonido de planos.json: con «frame» suenan cuando ese fotograma del plano sale en el corte."""
        fps = self.fps
        for sd in self.planos_info.get(tr.plano, {}).get("sfx", []):
            if "frame" not in sd:
                self._sfx(sd, tr.f0 / fps)
                continue
            if tr.fuente is None or tr.pos is None or tr.modo != "real":
                continue
            for k in range(tr.n):
                if tr.pos[k] >= sd["frame"] - 0.01:
                    if k > 0 and tr.pos[0] > sd["frame"]:
                        break
                    self.sfx.append({"t": (tr.f0 + k) / fps, "nombre": sd["nombre"], "db": float(sd.get("db", 0.0))})
                    break

    def _eventos_corte(self, tr):
        d, r, fps = tr.d, self.reloj, self.fps
        t0, t1 = tr.f0 / fps, tr.f1 / fps
        g = d.get("golpe", self.edl.get("por_defecto", {}).get("golpe"))
        if g:
            self._ev("punch", t0, amp=float(g))
        if d.get("pulso"):
            b = t0 + r.pulso
            while b < t1 - 1e-6:
                self._ev("punch", b, amp=float(d["pulso"]), tau=0.12)
                b += r.pulso
        if d.get("temblor"):
            tb = d["temblor"]
            if isinstance(tb, (int, float)):
                tb = {"amp": tb}
            self._ev("shake", t0, dur=tr.n / fps, amp=float(tb["amp"]), forma=tb.get("forma", "const"))
        for fd in d.get("efectos", []):
            if isinstance(fd, str):
                fd = {"fx": fd}
            self._efecto(fd, t0, t1)

    def _rotulo(self, d, base_s, fin_corte_s):
        ro = Rotulo(d, self, base_s, fin_corte_s)
        self.rotulos.append(ro)
        fps = self.fps
        if ro.tipo == "logo":
            imp = d.get("impacto", {"shake": 22, "flash": 0.75, "rgb": 14})
            pass
        else:
            imp = d.get("impacto", {"shake": 9, "flash": 0.08} if ro.anim == "slam" else ({"shake": 4} if ro.anim in ("pop", "caida") else False))
        if imp and ro.tipo != "plataformas":
            if imp.get("shake"):
                self._ev("shake", ro.t_imp, amp=float(imp["shake"]), dur=0.26)
            if imp.get("flash"):
                self._ev("flash", ro.t_imp, amp=float(imp["flash"]), dur=0.12)
            if imp.get("rgb"):
                self._ev("rgb", ro.t_imp, amp=float(imp["rgb"]), dur=0.3)
        sfx = d.get("sfx", True)
        if ro.tipo == "texto" and sfx is not False:
            principal = sfx if isinstance(sfx, str) else {"slam": "impact_small", "pop": "coin_pop", "caida": "impact_small"}.get(ro.anim)
            pre = d.get("sfx_pre", True)
            if pre is not False and ro.anim in ("slam", "caida", "izq", "der", "arriba", "abajo", "zoom"):
                self.sfx.append({"t": ro.t_imp - 0.10, "nombre": pre if isinstance(pre, str) else "whoosh_short", "db": -2.0})
            if principal:
                self.sfx.append({"t": ro.t_imp, "nombre": principal, "db": float(d.get("sfx_db", 0.0))})
        elif ro.tipo == "logo" and sfx is not False:
            self.sfx.append({"t": ro.t_imp, "nombre": sfx if isinstance(sfx, str) else "logo_sting", "db": 0.0})
            self.sfx.append({"t": ro.t_imp - 0.35, "nombre": "whoosh_long", "db": -3.0})
        if "cuenta" in d:
            c = d["cuenta"]
            dur = self.reloj.seg(c.get("dur", "2b"))
            paso = 0.11
            tt = ro.t_imp
            while tt < ro.t_imp + dur - 0.05:
                self.sfx.append({"t": tt, "nombre": "ui_tick", "db": -4.0})
                tt += paso
            self.sfx.append({"t": ro.t_imp + dur, "nombre": c.get("sfx_fin", "coin_pop"), "db": 0.0})
            self._ev("punch", ro.t_imp + dur, amp=0.05)

    def _golpes_automaticos(self):
        ag = self.edl.get("auto_golpes")
        if not ag or not self.hits:
            return
        tipos = set(ag.get("tipos", []))
        for t, kind in self.hits:
            if tipos and kind not in tipos:
                continue
            if t >= self.n_frames / self.fps:
                continue
            self._ev("punch", t, amp=float(ag.get("amp", 0.05)))
            if ag.get("temblor"):
                self._ev("shake", t, amp=float(ag["temblor"]), dur=0.2)

    # ── estado de los efectos ───────────────────────────────────────────────────────────────────────────────────────
    def estado_fx(self, f):
        st = {"zoom": 1.0, "dx": 0.0, "dy": 0.0, "rot": 0.0, "rgb": 0.0, "flash": 0.0, "flash_col": (255, 255, 255), "vineta": 0.0,
              "desat": 0.0, "oscuro": 0.0, "glitch": 0.0, "zoomblur": 0.0}
        u, fps = self.u, self.fps
        for e in self.eventos:
            d = f - e.f0
            if d < 0:
                break
            if d >= e.n:
                continue
            x = d / e.n
            t = e.tipo
            if t == "punch":
                st["zoom"] += e.amp * math.exp(-d / fps / e.tau)
                continue
            v = e.amp * forma_val(e.forma, x)
            if t == "shake":
                r = X.azar(f, e.sem, 3)
                st["dx"] += r[0] * v * u
                st["dy"] += r[1] * v * u * 0.8
                st["rot"] += r[2] * v * u * 0.05
            elif t == "flash":
                st["flash"] = max(st["flash"], v)
                if e.extra:
                    st["flash_col"] = e.extra
            elif t == "rgb":
                st["rgb"] += v * u
            elif t == "vineta":
                st["vineta"] += v
            elif t == "desat":
                st["desat"] = max(st["desat"], v)
            elif t == "oscurecer":
                st["oscuro"] = max(st["oscuro"], v)
            elif t == "glitch":
                st["glitch"] = max(st["glitch"], v * u)
            elif t == "inclinar":
                st["rot"] += v
            elif t == "zoomblur":
                st["zoomblur"] += v
        return st

    # ── imagen ──────────────────────────────────────────────────────────────────────────────────────────────────────
    def _tarjeta(self, tr, vw, vh):
        clave = (tr.plano, vw, vh)
        if clave not in self._tarjetas:
            tema = {"@logo": "logo", "@marino": "marino"}.get(tr.plano)
            etq = tr.d.get("etiquetas") or self.planos_info.get(tr.plano, {}).get("etiquetas", [])
            self._tarjetas[clave] = X.Tarjeta(tr.plano.lstrip("@"), etq, vw, vh, etiqueta=tema is None, tema=tema)
        return self._tarjetas[clave]

    def _fuente_img(self, tr, k):
        fu = tr.fuente
        pos = tr.pos_fuente(k)
        n = fu.n
        if tr.fin == "bucle" and n > 1:
            pos = pos % n
        elif tr.fin == "pingpong" and n > 1:
            m = pos % (2 * (n - 1))
            pos = 2 * (n - 1) - m if m > n - 1 else m
        pos = min(max(pos, 0.0), n - 1.0)
        i0 = int(math.floor(pos))
        fr = pos - i0
        a = fu.frame(i0)
        if 0.10 < fr < 0.90 and i0 + 1 < n and abs(tr.pos_fuente(k + 1) - tr.pos_fuente(k)) < 0.9:
            b = fu.frame(i0 + 1)
            if b.shape == a.shape:
                return cv2.addWeighted(a, 1.0 - fr, b, fr, 0)
        return a if fr < 0.5 or i0 + 1 >= n else fu.frame(i0 + 1)

    def _camara_tramo(self, tr, k, vw, vh, st):
        n = tr.n
        x = min(max(k / max(n - 1, 1), 0.0), 1.0)
        z = (tr.zoom[0] + (tr.zoom[1] - tr.zoom[0]) * x)
        if st is not None:
            z *= st["zoom"]
        if tr.fuente is not None:
            src = self._fuente_img(tr, k)
        else:
            src = self._tarjeta(tr, vw, vh).frame(k / self.fps)
        foco = tr.vfoco if self.vertical else tr.foco
        espejo = tr.espejo and tr.fuente is not None  # las tarjetas llevan texto: no se voltean
        if st is None:
            return X.camara(src, vw, vh, z, foco, 0, 0, 0, espejo, 0)
        return X.camara(src, vw, vh, z, foco, st["dx"], st["dy"], st["rot"], espejo, st["rgb"])

    def _plano(self, tr, f, vw, vh, st):
        k = f - tr.f0
        if tr.celdas:
            return self._split(tr, f, vw, vh, st)
        if tr.cong and k >= tr.cong["k"]:
            clave = (tr.id, vw, vh)
            if self._cache_cong[0] != clave:
                self._cache_cong = (clave, self._camara_tramo(tr, tr.cong["k"], vw, vh, None))
            base = self._cache_cong[1]
            kk = int(k - tr.cong["k"])
            if tr.cong["marco"] == "comic":
                return X.marco_comic(base, kk, tr.cong["rot"], tr.cong["escala"])
            if tr.cong["marco"] == "simple":
                s = 1.0 + 0.0012 * kk
                return X.camara(base, vw, vh, s, (0.5, 0.5))
            return base
        return self._camara_tramo(tr, k, vw, vh, st)

    def _split(self, tr, f, vw, vh, st):
        sp = tr.d["split"]
        hueco = max(2, int(sp.get("hueco", 10) * self.u))
        rects = X.rejilla(sp["tipo"], vw, vh, hueco)
        lienzo = np.empty((vh, vw, 3), np.uint8)
        lienzo[:] = AZUL_BGR
        esc = sp.get("escalonado", 0.08)
        for j, (x, y, w, h) in enumerate(rects):
            c = tr.celdas[j % len(tr.celdas)]
            k = max(f - tr.f0 - j * esc * self.fps, -1.0)  # todas las celdas se ven ya en el primer fotograma, a medio deslizar
            img = self._camara_tramo(c, f - tr.f0, w, h, st)
            p = min((k + 2.0) / 5.0, 1.0)
            if p < 1.0:
                e = T.sale(p)
                cxm, cym = x + w / 2.0, y + h / 2.0
                sx = -1 if cxm < vw / 2.0 else 1
                sy = -1 if cym < vh / 2.0 else 1
                ox, oy = (sx * (1 - e) * w, 0) if sp["tipo"] != "2h" else (0, sy * (1 - e) * h)
                M = np.array([[1, 0, ox], [0, 1, oy]], np.float64)
                img = cv2.warpAffine(img, M, (w, h), flags=cv2.INTER_LINEAR, borderMode=cv2.BORDER_CONSTANT, borderValue=AZUL_BGR)
            lienzo[y:y + h, x:x + w] = img
            cv2.rectangle(lienzo, (x, y), (x + w - 1, y + h - 1), (255, 255, 255), max(2, int(3 * self.u)))
        return lienzo

    def _vista(self, i, f, vw, vh, st):
        tr = self.tramos[i]
        img = self._plano(tr, f, vw, vh, st)
        k_in = f - tr.f0
        if tr.trans and k_in < tr.trans[1]:
            tipo, k = tr.trans
            if tipo == "fundido":
                prev = self._plano(self.tramos[i - 1], f, vw, vh, st)
                a = (k_in + 1.0) / (k + 1.0)
                img = cv2.addWeighted(prev, 1.0 - a, img, a, 0)
            else:
                img = self._trans_imagen(img, tipo, (k - k_in) / k, "in", f, vw, vh)
        if i + 1 < len(self.tramos):
            nx = self.tramos[i + 1]
            if nx.trans and nx.trans[0] not in ("fundido", "flash") and nx.f0 - f <= nx.trans[1]:
                k = nx.trans[1]
                img = self._trans_imagen(img, nx.trans[0], (k - (nx.f0 - f) + 1.0) / k, "out", f, vw, vh)
        return img

    def _trans_imagen(self, img, tipo, p, lado, f, vw, vh):
        p = min(max(p, 0.0), 1.0)
        if tipo.startswith("whip"):
            dx, dy = {"whip_l": (-1, 0), "whip_r": (1, 0), "whip_u": (0, -1), "whip_d": (0, 1)}[tipo]
            sgn = 1 if lado == "out" else -1  # la imagen que sale sigue la dirección; la que entra viene de detrás
            tam = vw if dx else vh
            off = p * p * 0.55 * tam * sgn
            M = np.array([[1, 0, dx * off], [0, 1, dy * off]], np.float64)
            img = cv2.warpAffine(img, M, (vw, vh), flags=cv2.INTER_LINEAR, borderMode=cv2.BORDER_WRAP)  # panorámica continua
            return X.desenfoque_mov(img, p * 0.10 * tam, "h" if dx else "v")
        if tipo == "zoom":
            s = 1.0 + p * p * 0.9
            M = np.array([[s, 0, vw / 2.0 * (1 - s)], [0, s, vh / 2.0 * (1 - s)]], np.float64)
            img = cv2.warpAffine(img, M, (vw, vh), flags=cv2.INTER_LINEAR, borderMode=cv2.BORDER_REFLECT)
            return X.desenfoque_zoom(img, p * 0.10)
        if tipo == "glitch":
            return X.glitch(img, f, 60 * p * self.u)
        if tipo == "dip_negro":
            return X.oscurecer(img, p)
        if tipo == "dip_blanco":
            return X.destello(img, p)
        return img

    def _indice(self, f):
        return max(bisect.bisect_right(self.f_inicio, f) - 1, 0)

    def _fondo_vertical(self, vista):
        """Fondo del vertical: el propio plano desenfocado y teñido de azul marino, con degradado arriba y abajo."""
        H, W = self.H, self.W
        h, w = vista.shape[:2]
        ancho = int(h * 9 / 16.0)
        x0 = max((w - ancho) // 2, 0)
        pequeña = cv2.resize(vista[:, x0:x0 + ancho], (36, 64), interpolation=cv2.INTER_AREA)
        pequeña = cv2.GaussianBlur(pequeña, (0, 0), 1.2)
        fondo = cv2.resize(pequeña, (W, H), interpolation=cv2.INTER_CUBIC)
        fondo = cv2.addWeighted(fondo, 0.55, np.full_like(fondo, AZUL_BGR), 0.45, 0)
        if self._fondo_v is None:
            y = np.abs(np.linspace(-1, 1, H, dtype=np.float32))[:, None]
            a = np.clip((y - 0.52) / 0.48, 0, 1) ** 1.2 * 0.72
            self._fondo_v = np.repeat(a[:, :, None], W, axis=1).astype(np.float32)
        a = self._fondo_v
        oscuro = np.empty_like(fondo)
        oscuro[:] = AZUL_OSC_BGR
        fondo = (fondo.astype(np.float32) * (1 - a) + oscuro.astype(np.float32) * a).astype(np.uint8)
        return fondo

    def _componer_vertical(self, vista, f):
        lienzo = self._fondo_vertical(vista)
        y0, y1 = self.vy, self.vy + self.vh
        lienzo[y0:y1, self.vx:self.vx + self.vw] = vista
        g = max(3, int(round(7 * self.u)))
        for (y, dy) in ((y0, -1), (y1, 1)):
            cv2.rectangle(lienzo, (0, y - (g if dy < 0 else 0)), (self.W, y + (g if dy > 0 else 0)), AZUL_BGR, -1)
            yb = y - g - 2 if dy < 0 else y + g
            cv2.rectangle(lienzo, (0, yb), (self.W, yb + max(2, g // 2)), (255, 255, 255), -1)
        return lienzo

    def render_frame(self, f):
        f = min(max(int(f), 0), self.n_frames - 1)
        i = self._indice(f)
        tr = self.tramos[i]
        st = self.estado_fx(f)
        completo = self.vertical and not tr.celdas and (tr.plano.startswith("@") or bool(tr.d.get("completo")))
        vw, vh = (self.W, self.H) if completo else (self.vw, self.vh)
        grado = self.grado_completo if completo else self.grado
        vista = self._vista(i, f, vw, vh, st)
        d = tr.d
        velo = d.get("velo")
        if velo:  # velo desenfocado del color de la marca (fondo del logo sobre imagen del juego)
            b = float(velo.get("blur", 8)) * self.u
            if b > 0.5:
                vista = cv2.GaussianBlur(vista, (0, 0), b)
            vista = X.destello(vista, float(velo.get("alfa", 0.6)), tuple(velo.get("color", AZUL_BGR)))
        des = max(st["desat"], float(d.get("desat", 0.0)))
        if des > 0:
            vista = X.desaturar(vista, des)
        if st["zoomblur"] > 0.004:
            vista = X.desenfoque_zoom(vista, st["zoomblur"])
        vista = grado.color(vista)
        vista = grado.vigneta(vista, st["vineta"])
        if st["glitch"] > 0.5:
            vista = X.glitch(vista, f, st["glitch"])
        lienzo = vista if (not self.vertical or completo) else self._componer_vertical(vista, f)
        osc = max(st["oscuro"], float(d.get("oscuro", 0.0)))
        if osc > 0:
            lienzo = X.oscurecer(lienzo, osc)
        if st["flash"] > 0.004:
            lienzo = X.destello(lienzo, st["flash"], st["flash_col"])
        lienzo = np.ascontiguousarray(lienzo)
        for ro in self.rotulos:
            if ro.f_ini <= f < ro.f_fin:
                ro.dibujar(lienzo, f)
        lienzo = self.grano.grano_frame(lienzo, f)
        fi = self.edl.get("fundido_inicial", 0.0)
        ff = self.edl.get("fundido_final", 0.0)
        if fi and f < fi * self.fps:
            lienzo = X.oscurecer(lienzo, 1.0 - (f + 1) / (fi * self.fps))
        if ff and f >= self.n_frames - ff * self.fps:
            lienzo = X.oscurecer(lienzo, (f - (self.n_frames - ff * self.fps) + 1) / (ff * self.fps))
        if self.debug_etiquetas:
            marca = "" if tr.modo == "real" else " *"
            txt = f"{tr.id} {tr.plano}{marca} [{tr.modo}]"
            cv2.putText(lienzo, txt, (8, self.H - 10), cv2.FONT_HERSHEY_SIMPLEX, 0.5 * max(self.u * 1.6, 0.6), (0, 0, 0), 3, cv2.LINE_AA)
            cv2.putText(lienzo, txt, (8, self.H - 10), cv2.FONT_HERSHEY_SIMPLEX, 0.5 * max(self.u * 1.6, 0.6), (255, 255, 255), 1, cv2.LINE_AA)
        return lienzo


def Grado(w, h, col):
    """Grado de color de la lista de cortes (sat, calidez, contraste, brillo, vineta; el grano va aparte)."""
    return X.Grado(w, h, sat=col["sat"], calidez=col["calidez"], contraste=col["contraste"], brillo=col["brillo"],
                   vineta=col["vineta"], grano=0.0)


# ── Informes ────────────────────────────────────────────────────────────────────────────────────────────────────────

def _todos_los_tramos(m):
    for tr in m.tramos:
        yield tr
        for c in tr.celdas:
            yield c


def hoja_de_rodaje(m, salida=None):
    """Segundos que necesita el montaje de cada plano (con la entrada y velocidad pedidas) y cuántas veces se usa."""
    usos = {}
    for tr in _todos_los_tramos(m):
        if not tr.plano or tr.plano.startswith("@"):
            continue
        u = usos.setdefault(tr.plano, {"usos": 0, "necesita_s": 0.0, "modo": tr.modo, "tiempos": []})
        u["usos"] += 1
        u["necesita_s"] = max(u["necesita_s"], tr.necesita_s)
        u["tiempos"].append(round(tr.f0 / m.fps, 2))
        u["etiquetas"] = tr.d.get("etiquetas") or m.planos_info.get(tr.plano, {}).get("etiquetas", [])
    if salida:
        with open(salida, "w", encoding="utf-8") as fh:
            json.dump(usos, fh, ensure_ascii=False, indent=1)
    return usos


def comprobar(m):
    """Avisos de sincronía y de maquetación de la lista de cortes."""
    fps = m.fps
    out = []
    # planos
    cuenta = {}
    for tr in _todos_los_tramos(m):
        cuenta[tr.modo] = cuenta.get(tr.modo, 0) + 1
    out.append("planos por origen: " + ", ".join(f"{k}={v}" for k, v in sorted(cuenta.items())))
    # solapes de rótulos en la misma zona
    textos = [r for r in m.rotulos if r.tipo == "texto"]
    for i, a in enumerate(textos):
        for b in textos[i + 1:]:
            if str(a.d.get("pos", "centro")) == str(b.d.get("pos", "centro")) and a.f_imp < b.f_fin - 6 and b.f_imp < a.f_fin - 6:
                if b.f_imp > a.f_imp:
                    out.append(f"solape: «{a.txt[:24]}» ({a.t_imp:.2f}-{a.f_fin / fps:.2f}) y «{b.txt[:24]}» ({b.t_imp:.2f}) en la misma zona")
    # golpes del beatmap
    if m.hits:
        for r in textos:
            cerca = min(m.hits, key=lambda h: abs(h[0] - r.t_imp))
            dt = abs(cerca[0] - r.t_imp)
            if 1.0 / fps < dt <= 0.25:
                out.append(f"sincronía: el golpe de «{r.txt[:24]}» cae en {r.t_imp:.3f} s y el golpe de la música más cercano en {cerca[0]:.3f} s")
    else:
        out.append("sin beatmap: la sincronía sigue la rejilla de pulsos de la lista de cortes")
    # beatmap frente a la rejilla
    if m.beatmap and m.beatmap.get("beats"):
        dev = max(abs(b - round(b / m.reloj.pulso) * m.reloj.pulso) for b in m.beatmap["beats"])
        if dev > 1.0 / fps:
            out.append(f"el beatmap se desvía hasta {dev:.3f} s de la rejilla de {m.reloj.bpm} BPM")
    # fotogramas casi lisos o quemados: cámara dentro de una pared, tapada por una hoja, negro…
    for tr in _todos_los_tramos(m):
        if tr.fuente is None or tr.pos is None:
            continue
        for k in sorted({0, tr.n // 4, tr.n // 2, (3 * tr.n) // 4, tr.n - 1}):
            img = m._fuente_img(tr, k)
            g = cv2.cvtColor(cv2.resize(img, (160, 90), interpolation=cv2.INTER_AREA), cv2.COLOR_BGR2GRAY).astype(np.float32)
            if g.std() < 11.0 or g.mean() > 245 or g.mean() < 6:
                out.append(f"fotograma flojo: {tr.id} ({tr.plano}) en {(tr.f0 + k) / m.fps:.2f} s, fotograma {tr.pos_fuente(k):.0f} del plano "
                           f"(desviación {g.std():.1f}, media {g.mean():.0f})")
    out.extend("aviso: " + a for a in m.avisos)
    return out


# ── Audio ───────────────────────────────────────────────────────────────────────────────────────────────────────────

def preparar_audio(m, ruta_wav, sin_audio=False):
    e = m.edl
    n = int(round(m.n_frames / m.fps * A.SR))
    rm = e.get("musica", "music/trailer_30.wav" if m.vertical else "music/trailer_60.wav")
    pm = os.path.join(RAIZ, rm)
    musica = None
    if os.path.exists(pm) and not sin_audio:
        musica = A.cargar(pm)
        log(f"    música: {os.path.relpath(pm, RAIZ)} ({len(musica) / A.SR:.2f} s)")
    else:
        log("    música: no hay; se usa un metrónomo sintético a %g BPM" % m.reloj.bpm)
    banco = A.Banco(os.path.join(RAIZ, "sfx"))
    t0 = 0.0
    if m.beatmap and m.beatmap.get("beats"):
        t0 = float(m.beatmap["beats"][0])
    mezcla, info = A.mezclar(n, m.sfx, musica, banco, m.reloj.bpm, float(e.get("musica_db", 0.0)), t0)
    if info["sinteticos"]:
        log("    efectos sintéticos de sustitución: " + ", ".join(info["sinteticos"]))
    objetivo = float(e.get("objetivo_lufs", -14.0 if musica is not None else -20.0))
    A.normalizar(mezcla, ruta_wav, objetivo, -1.5, log)  # margen para el pico extra que añade el AAC
    return info


# ── Render y codificación ───────────────────────────────────────────────────────────────────────────────────────────

_M = None


def _init_worker(args):
    global _M
    cv2.setNumThreads(1)
    edl, idioma, preview, resolucion, etiquetas, footage = args
    _M = Montaje(edl, idioma, preview, resolucion=resolucion, etiquetas=etiquetas, silencioso=True, footage=footage)


def _trabajo(rango):
    a, b = rango
    return b"".join(np.ascontiguousarray(_M.render_frame(f)).tobytes() for f in range(a, b))


def codificar(m, salida, ruta_audio, preview, workers, etiquetas):
    import imageio_ffmpeg
    exe = imageio_ffmpeg.get_ffmpeg_exe()
    W, H, n = m.W, m.H, m.n_frames
    cmd = [exe, "-y", "-hide_banner", "-loglevel", "error", "-f", "rawvideo", "-pix_fmt", "bgr24", "-s", f"{W}x{H}", "-r", str(m.fps),
           "-i", "-", "-i", ruta_audio, "-map", "0:v:0", "-map", "1:a:0",
           "-vf", "scale=out_color_matrix=bt709:out_range=tv:flags=bicubic,format=yuv420p",
           "-c:v", "libx264", "-preset", "veryfast" if preview else "slow", "-crf", "26" if preview else "18",
           "-profile:v", "high", "-g", str(m.fps * 2), "-bf", "2",
           "-colorspace", "bt709", "-color_primaries", "bt709", "-color_trc", "bt709", "-color_range", "tv",
           "-x264-params", "colorprim=bt709:transfer=bt709:colormatrix=bt709:fullrange=off",
           "-c:a", "aac", "-b:a", "192k", "-ar", "48000", "-ac", "2", "-movflags", "+faststart", "-shortest", salida]
    log_ff = os.path.join(RAIZ, "tmp", "ffmpeg_ultimo.log")
    os.makedirs(os.path.dirname(log_ff), exist_ok=True)
    fl = open(log_ff, "wb")
    proc = subprocess.Popen(cmd, stdin=subprocess.PIPE, stderr=fl)
    t0 = time.time()
    trozo = 3
    rangos = [(a, min(a + trozo, n)) for a in range(0, n, trozo)]
    workers = max(1, workers)
    ctx = mp.get_context("spawn")
    try:
        with ctx.Pool(workers, initializer=_init_worker, initargs=((m.edl_ruta, m.idioma, preview, m.resolucion, etiquetas, m.footage),)) as pool:
            pendientes = []
            hechos, ultimo = 0, -1
            it = iter(rangos)
            listo = False
            while not listo or pendientes:
                while not listo and len(pendientes) < workers * 3:
                    try:
                        r = next(it)
                    except StopIteration:
                        listo = True
                        break
                    pendientes.append((r, pool.apply_async(_trabajo, (r,))))
                if pendientes:
                    r, res = pendientes.pop(0)
                    datos = res.get()
                    proc.stdin.write(datos)
                    hechos = r[1]
                    pct = int(hechos * 100 / n)
                    if pct // 10 != ultimo // 10:
                        ultimo = pct
                        seg = time.time() - t0
                        log(f"    {pct:3d}%  {hechos}/{n} fotogramas  {hechos / max(seg, 1e-3):.1f} fps  quedan ~{seg / max(hechos, 1) * (n - hechos):.0f} s")
        proc.stdin.close()
        rc = proc.wait()
    except Exception:
        try:
            proc.kill()
        except OSError:
            pass
        raise
    finally:
        fl.close()
    if rc != 0:
        with open(log_ff, "rb") as fh:
            raise RuntimeError("ffmpeg falló:\n" + fh.read().decode(errors="replace")[-800:])
    log(f"    listo en {time.time() - t0:.0f} s → {salida} ({os.path.getsize(salida) / 1e6:.1f} MB)")


def hoja_contactos(m, a, b, paso, salida):
    """Rejilla de miniaturas (con el instante) para revisar el ritmo de un vistazo."""
    tiempos = []
    t = a
    while t < b - 1e-6:
        tiempos.append(t)
        t += paso
    cols = 6 if not m.vertical else 8
    tw = 320 if not m.vertical else 180
    th = int(tw * m.H / m.W)
    filas = int(math.ceil(len(tiempos) / cols))
    hoja = np.zeros((filas * th, cols * tw, 3), np.uint8)
    for k, t in enumerate(tiempos):
        img = m.render_frame(int(round(t * m.fps)))
        mini = cv2.resize(img, (tw, th), interpolation=cv2.INTER_AREA)
        cv2.putText(mini, f"{t:.2f}", (4, 16), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 0, 0), 3, cv2.LINE_AA)
        cv2.putText(mini, f"{t:.2f}", (4, 16), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255, 255, 255), 1, cv2.LINE_AA)
        y, x = (k // cols) * th, (k % cols) * tw
        hoja[y:y + th, x:x + tw] = mini
    cv2.imwrite(salida, hoja)
    return salida


def verificar(ruta):
    """Duración, formato, fotogramas y sonoridad de un MP4 leyendo el propio fichero con ffmpeg (no hay ffprobe)."""
    import imageio_ffmpeg
    exe = imageio_ffmpeg.get_ffmpeg_exe()
    info = {"fichero": os.path.basename(ruta), "MB": round(os.path.getsize(ruta) / 1e6, 1)}
    txt = subprocess.run([exe, "-hide_banner", "-i", ruta], capture_output=True).stderr.decode(errors="replace")
    m = re.search(r"Duration: (\d+):(\d+):(\d+\.\d+)", txt)
    if m:
        info["duracion_s"] = round(int(m.group(1)) * 3600 + int(m.group(2)) * 60 + float(m.group(3)), 3)
    m = re.search(r"Video: (\w+)[^\n]*?, (\d+)x(\d+)[^\n]*?, (\d+(?:\.\d+)?) fps", txt)
    if m:
        info.update(video=m.group(1), ancho=int(m.group(2)), alto=int(m.group(3)), fps=float(m.group(4)))
    m = re.search(r"Video:[^\n]*?(\d+) kb/s", txt)
    if m:
        info["kbps_video"] = int(m.group(1))
    m = re.search(r"Audio: (\w+)[^\n]*?(\d+) Hz, (\w+)[^\n]*?(\d+) kb/s", txt)
    if m:
        info.update(audio=m.group(1), hz=int(m.group(2)), canales=m.group(3), kbps_audio=int(m.group(4)))
    r = subprocess.run([exe, "-hide_banner", "-i", ruta, "-map", "0:v:0", "-f", "null", "-"], capture_output=True).stderr.decode(errors="replace")
    m = re.findall(r"frame=\s*(\d+)", r)
    if m:
        info["fotogramas"] = int(m[-1])
    r = subprocess.run([exe, "-hide_banner", "-nostats", "-i", ruta, "-map", "0:a:0", "-af", "loudnorm=I=-14:TP=-1.5:LRA=11:print_format=json", "-f", "null", "-"],
                       capture_output=True).stderr.decode(errors="replace")
    m = re.search(r"\{[^{}]*\"input_i\"[^{}]*\}", r, re.S)
    if m:
        med = json.loads(m.group(0))
        info.update(LUFS=float(med["input_i"]), pico_dBTP=float(med["input_tp"]), LRA=float(med["input_lra"]))
    return info


def hoja_de_video(ruta, salida, paso=0.5):
    """Hoja de contactos sacada del propio MP4 final (una miniatura cada `paso` segundos, con su instante)."""
    import imageio_ffmpeg
    exe = imageio_ffmpeg.get_ffmpeg_exe()
    inf = verificar_rapido(ruta)
    w, h = inf["ancho"], inf["alto"]
    tw = 240 if w >= h else 135
    th = int(round(tw * h / w))
    cada = max(int(round(paso * FPS)), 1)  # un fotograma de cada `cada`: instantes exactos (el filtro fps redondea al más cercano)
    cmd = [exe, "-v", "error", "-i", ruta, "-vf", f"select=not(mod(n\\,{cada})),scale={tw}:{th}", "-vsync", "0", "-f", "rawvideo", "-pix_fmt", "bgr24", "-"]
    datos = subprocess.run(cmd, capture_output=True).stdout
    n = len(datos) // (tw * th * 3)
    cols = 8 if w >= h else 12
    filas = int(math.ceil(n / cols))
    hoja = np.zeros((filas * th, cols * tw, 3), np.uint8)
    for k in range(n):
        mini = np.frombuffer(datos, np.uint8, tw * th * 3, k * tw * th * 3).reshape(th, tw, 3).copy()
        t = f"{k * cada / FPS:.1f}"
        cv2.putText(mini, t, (4, 15), cv2.FONT_HERSHEY_SIMPLEX, 0.45, (0, 0, 0), 3, cv2.LINE_AA)
        cv2.putText(mini, t, (4, 15), cv2.FONT_HERSHEY_SIMPLEX, 0.45, (255, 255, 255), 1, cv2.LINE_AA)
        hoja[(k // cols) * th:(k // cols + 1) * th, (k % cols) * tw:(k % cols + 1) * tw] = mini
    cv2.imwrite(salida, hoja)
    return salida


def verificar_rapido(ruta):
    import imageio_ffmpeg
    txt = subprocess.run([imageio_ffmpeg.get_ffmpeg_exe(), "-hide_banner", "-i", ruta], capture_output=True).stderr.decode(errors="replace")
    m = re.search(r"Video: [^\n]*?, (\d+)x(\d+)", txt)
    return {"ancho": int(m.group(1)), "alto": int(m.group(2))}


# ── Línea de órdenes ────────────────────────────────────────────────────────────────────────────────────────────────

def main(argv=None):
    for flujo in (sys.stdout, sys.stderr):
        try:
            flujo.reconfigure(encoding="utf-8", errors="replace")
        except (AttributeError, ValueError):
            pass
    ap = argparse.ArgumentParser(description="Montaje del tráiler de Tortunavy (python montaje.py --formato para la documentación).")
    ap.add_argument("--only", default="", help="60_es,60_en,30_es,30_en (por defecto los cuatro)")
    ap.add_argument("--preview", action="store_true", help="media resolución y preset rápido (sufijo _preview)")
    ap.add_argument("--workers", type=int, default=0, help="procesos de render (por defecto ~un tercio de los núcleos)")
    ap.add_argument("--frames", default="", help="instantes en segundos, separados por comas: escribe PNG en Saved/Trailer/tmp/")
    ap.add_argument("--contacto", default="", help="inicio:fin:paso en segundos: hoja de contactos en Saved/Trailer/tmp/")
    ap.add_argument("--hoja", action="store_true", help="imprime los segundos que necesita cada plano")
    ap.add_argument("--check", action="store_true", help="comprobaciones sin renderizar")
    ap.add_argument("--etiquetas", action="store_true", help="rotula cada corte con su plano y su origen")
    ap.add_argument("--estricto", action="store_true", help="error si falta algún plano (para el montaje final)")
    ap.add_argument("--sin-audio", action="store_true", help="ignora la música real y usa el metrónomo")
    ap.add_argument("--formato", action="store_true", help="imprime la documentación del formato de la lista de cortes")
    ap.add_argument("--sin-hojas", action="store_true", help="no genera la hoja de contactos del MP4 final")
    ap.add_argument("--verificar", nargs="*", default=None, help="verifica MP4 (duración, formato, sonoridad); sin argumentos, los de out/")
    ap.add_argument("--footage", default="", help="carpeta de planos alternativa (por defecto Saved/Trailer/footage)")
    ap.add_argument("--salida", default="", help="carpeta de salida (por defecto Saved/Trailer/out)")
    a = ap.parse_args(argv)
    if a.formato:
        print(__doc__)
        return 0
    if a.verificar is not None:
        out_v = a.salida or os.path.join(RAIZ, "out")
        rutas = a.verificar or sorted(os.path.join(out_v, f) for f in os.listdir(out_v) if f.endswith(".mp4"))
        for r in rutas:
            print(json.dumps(verificar(r), ensure_ascii=False))
        return 0
    claves = [k.strip() for k in a.only.split(",") if k.strip()] or list(NOMBRES)
    for k in claves:
        if k not in NOMBRES:
            ap.error(f"--only: {k!r} no es uno de {', '.join(NOMBRES)}")
    out_dir = a.salida or os.path.join(RAIZ, "out")
    tmp_dir = os.path.join(RAIZ, "tmp")
    os.makedirs(out_dir, exist_ok=True)
    os.makedirs(tmp_dir, exist_ok=True)
    workers = a.workers or max(2, min(10, (os.cpu_count() or 4) // 3))
    resolucion_compartida = {}
    for clave in claves:
        edl, idioma, base = NOMBRES[clave]
        ruta_edl = os.path.join(AQUI, edl)
        log(f"== {clave}: {edl} [{idioma}]")
        m = Montaje(ruta_edl, idioma, a.preview, etiquetas=a.etiquetas, footage=a.footage or None)
        if a.estricto and any(tr.modo in ("tarjeta", "etiquetas", "alt") for tr in _todos_los_tramos(m)):
            faltan = sorted({tr.plano for tr in _todos_los_tramos(m) if tr.modo in ("tarjeta", "etiquetas", "alt")})
            log("  --estricto: faltan planos: " + ", ".join(faltan))
            return 2
        if a.hoja:
            u = hoja_de_rodaje(m, os.path.join(tmp_dir, f"hoja_{os.path.splitext(edl)[0]}.json"))
            for plano, v in sorted(u.items(), key=lambda kv: min(kv[1]["tiempos"])):
                log(f"  {plano:20s} usos={v['usos']:2d}  necesita>={v['necesita_s']:.2f} s  [{v['modo']}]  {v['etiquetas']}")
            continue
        if a.check:
            for linea in comprobar(m):
                log("  " + linea)
            continue
        if a.frames:
            for tt in a.frames.split(","):
                t = m.reloj.seg(tt.strip())
                img = m.render_frame(int(round(t * m.fps)))
                ruta = os.path.join(tmp_dir, f"{os.path.splitext(edl)[0]}_{idioma}_{t:06.2f}{'_p' if a.preview else ''}.png")
                cv2.imwrite(ruta, img)
                log("  " + ruta)
            continue
        if a.contacto:
            i0, i1, ip = (float(x) for x in a.contacto.split(":"))
            ruta = os.path.join(tmp_dir, f"contacto_{os.path.splitext(edl)[0]}_{idioma}_{i0:g}-{i1:g}.png")
            hoja_contactos(m, i0, i1, ip, ruta)
            log("  " + ruta)
            continue
        ruta_wav = os.path.join(tmp_dir, f"audio_{clave}.wav")
        log("  audio…")
        preparar_audio(m, ruta_wav, a.sin_audio)
        sufijo = "_preview" if a.preview else ""
        salida = os.path.join(out_dir, base + sufijo + ".mp4")
        log(f"  vídeo {m.W}x{m.H} {m.n_frames} fotogramas, {workers} procesos…")
        codificar(m, salida, ruta_wav, a.preview, workers, a.etiquetas)
        ver = verificar(salida)
        log("    verificación: " + json.dumps(ver, ensure_ascii=False))
        if not a.preview and not a.sin_hojas:
            log("    hoja de contactos: " + hoja_de_video(salida, os.path.splitext(salida)[0] + "_contacto.png"))
    return 0


if __name__ == "__main__":
    mp.freeze_support()
    sys.exit(main())
