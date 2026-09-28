# -*- coding: utf-8 -*-
"""Audio del montaje: carga de música y efectos, sustitutos sintéticos, mezcla con ducking y normalización.

- Todo se decodifica con ffmpeg (cualquier WAV/MP3/OGG, entero o de coma flotante) a float32 estéreo a 48 kHz.
- Sin música: metrónomo sintético al BPM de la lista. Sin un efecto concreto: sustituto sintético sencillo (solo para
  poder oír la sincronía; en cuanto aparece el WAV real en Saved/Trailer/sfx/ se usa ese).
- Cada efecto se alinea por su pico: el instante `t` de la lista de cortes cae sobre el punto más fuerte del efecto
  (en un golpe es el inicio; en un riser, el final; en un whoosh, la mitad).
- La música baja ligeramente bajo los efectos gordos («ducking») y el resultado se normaliza a -14 LUFS / -1 dBTP con
  loudnorm de ffmpeg en dos pasadas.
"""
from __future__ import annotations

import json
import os
import re
import subprocess

import numpy as np

SR = 48000

# Cuánto baja la música (dB) bajo cada efecto gordo. 0 = no la toca.
DUCK_DB = {
    "impact_big": -4.0, "explosion_toy": -4.0, "record_scratch": -4.0, "logo_sting": -3.5, "worm_chomp": -3.0,
    "crowd_cheer_short": -2.5, "splash": -2.0, "impact_small": -1.5, "gull_screech": -1.5, "boing": -1.0,
}
# Ganancia de partida de cada efecto respecto a su WAV (dB), para que ninguno tape a la música.
GANANCIA_DB = {
    "whoosh_short": -4.0, "whoosh_long": -4.0, "impact_big": -1.0, "impact_small": -4.0, "riser_2s": -6.0,
    "record_scratch": -2.0, "boing": -5.0, "splash": -4.0, "explosion_toy": -2.0, "gull_screech": -5.0,
    "crab_clack": -5.0, "worm_chomp": -3.0, "egg_crack": -5.0, "coin_pop": -6.0, "crowd_cheer_short": -5.0,
    "ui_tick": -7.0, "logo_sting": -2.0,
}


# Alineación por defecto de cada efecto con su instante «t»: el resto se alinea por el pico (punto más fuerte).
ALINEAR_DEFECTO = {"record_scratch": "inicio", "crowd_cheer_short": "inicio", "gull_screech": "inicio", "riser_2s": "fin"}


def ffmpeg_exe() -> str:
    import imageio_ffmpeg
    return imageio_ffmpeg.get_ffmpeg_exe()


def cargar(ruta: str) -> np.ndarray:
    """Decodifica un fichero de audio a float32 (n, 2) a 48 kHz."""
    cmd = [ffmpeg_exe(), "-v", "error", "-i", ruta, "-f", "f32le", "-ac", "2", "-ar", str(SR), "-"]
    r = subprocess.run(cmd, capture_output=True)
    if r.returncode != 0 or not r.stdout:
        raise RuntimeError(f"No se pudo leer el audio {ruta}: {r.stderr.decode(errors='replace')[:200]}")
    return np.frombuffer(r.stdout, np.float32).reshape(-1, 2).copy()


# ── Sustitutos sintéticos ───────────────────────────────────────────────────────────────────────────────────────────

def _t(dur):
    return np.arange(int(dur * SR), dtype=np.float64) / SR


def _est(x):
    return np.stack([x, x], 1).astype(np.float32)


def _filtro_fft(x, f_lo, f_hi):
    X = np.fft.rfft(x)
    fr = np.fft.rfftfreq(len(x), 1.0 / SR)
    X[(fr < f_lo) | (fr > f_hi)] = 0
    return np.fft.irfft(X, len(x))


def _ruido(n, seed):
    return np.random.default_rng(seed).standard_normal(n)


def _whoosh(dur, seed, subida=0.45):
    t = _t(dur)
    n = len(t)
    ruido = _ruido(n, seed)
    bandas = [(150, 700), (700, 2000), (2000, 5000), (5000, 11000)]
    y = np.zeros(n)
    for i, (a, b) in enumerate(bandas):
        centro = subida * dur * (0.55 + 0.25 * i) / 1.4
        y += _filtro_fft(ruido, a, b) * np.exp(-0.5 * ((t - centro) / (dur * 0.20)) ** 2) * (1.0 - 0.15 * i)
    env = np.sin(np.pi * np.clip(t / dur, 0, 1)) ** 1.2
    y = y * env
    return y / (np.max(np.abs(y)) + 1e-9)


def sintetizar(nombre: str) -> np.ndarray:
    """Efecto sintético sencillo con el mismo nombre que los reales (solo sustituto de trabajo)."""
    if nombre == "whoosh_short":
        y = _whoosh(0.35, 1) * 0.8
    elif nombre == "whoosh_long":
        y = _whoosh(0.9, 2, 0.6) * 0.8
    elif nombre in ("impact_big", "impact_small"):
        gran = nombre == "impact_big"
        t = _t(0.9 if gran else 0.35)
        f = 42 + (140 if gran else 190) * np.exp(-t * (18 if gran else 30))
        y = np.sin(2 * np.pi * np.cumsum(f) / SR) * np.exp(-t * (5.5 if gran else 12))
        y += _filtro_fft(_ruido(len(t), 3), 200, 6000) * np.exp(-t * (28 if gran else 60)) * 0.6
        y = np.tanh(y * 1.6)
    elif nombre == "riser_2s":
        t = _t(2.0)
        u = t / 2.0
        f = 220 * (1 + 7 * u ** 2.2)
        y = np.sin(2 * np.pi * np.cumsum(f) / SR) * 0.35 * u
        y += _filtro_fft(_ruido(len(t), 4), 800, 9000) * u ** 2.4 * 0.7
        y[-int(0.01 * SR):] *= np.linspace(1, 0, int(0.01 * SR))
    elif nombre == "record_scratch":
        t = _t(0.55)
        f = 900 * np.exp(-t * 4.2) + 60
        saw = 2 * ((np.cumsum(f) / SR) % 1.0) - 1
        y = (saw * 0.5 + _filtro_fft(_ruido(len(t), 5), 300, 5000) * 0.5) * np.clip(1 - t / 0.55, 0, 1) ** 0.6
    elif nombre == "boing":
        t = _t(0.55)
        f = 260 + 340 * np.exp(-t * 6) * (1 + 0.5 * np.sin(2 * np.pi * 14 * t))
        y = np.sin(2 * np.pi * np.cumsum(f) / SR) * np.exp(-t * 6.5)
    elif nombre == "splash":
        t = _t(0.7)
        y = _filtro_fft(_ruido(len(t), 6), 400, 9000) * np.exp(-t * 7) * (1 - np.exp(-t * 90))
    elif nombre == "explosion_toy":
        t = _t(1.1)
        y = _filtro_fft(_ruido(len(t), 7), 40, 2500) * np.exp(-t * 4.5)
        y += np.sin(2 * np.pi * (55 + 90 * np.exp(-t * 9)) * t) * np.exp(-t * 4)
        y = np.tanh(y * 1.4)
    elif nombre == "gull_screech":
        t = _t(0.8)
        f = 2100 + 700 * np.sin(np.pi * t / 0.8) + 60 * np.sin(2 * np.pi * 28 * t)
        y = (np.sin(2 * np.pi * np.cumsum(f) / SR) + 0.4 * np.sin(4 * np.pi * np.cumsum(f) / SR)) * np.sin(np.pi * t / 0.8) ** 0.7 * 0.5
    elif nombre == "crab_clack":
        y = np.zeros(int(0.35 * SR))
        for k, t0 in enumerate((0.0, 0.11)):
            tt = _t(0.05)
            c = np.sin(2 * np.pi * 2600 * tt) * np.exp(-tt * 120) + _filtro_fft(_ruido(len(tt), 8 + k), 1500, 7000) * np.exp(-tt * 200) * 0.6
            i0 = int(t0 * SR)
            y[i0:i0 + len(c)] += c
    elif nombre == "worm_chomp":
        t = _t(0.6)
        y = _filtro_fft(_ruido(len(t), 9), 80, 1500) * np.exp(-t * 9) * (1 + 0.6 * np.sin(2 * np.pi * 22 * t))
        y += np.sin(2 * np.pi * 70 * t) * np.exp(-t * 10)
    elif nombre == "egg_crack":
        t = _t(0.3)
        y = np.zeros_like(t)
        for t0 in (0.0, 0.06, 0.13):
            i0 = int(t0 * SR)
            n = int(0.04 * SR)
            y[i0:i0 + n] += _filtro_fft(_ruido(n, 11 + i0), 1500, 9000) * np.exp(-np.arange(n) / SR * 90)
    elif nombre == "coin_pop":
        t = _t(0.35)
        f = np.where(t < 0.07, 988, 1319)
        y = np.sin(2 * np.pi * np.cumsum(f) / SR) * np.exp(-t * 11)
    elif nombre == "crowd_cheer_short":
        t = _t(1.2)
        y = _filtro_fft(_ruido(len(t), 12), 300, 3500) * np.sin(np.pi * np.clip(t / 1.2, 0, 1)) ** 0.8
    elif nombre == "ui_tick":
        t = _t(0.05)
        y = np.sin(2 * np.pi * 1900 * t) * np.exp(-t * 130)
    elif nombre == "logo_sting":
        t = _t(1.8)
        y = np.zeros_like(t)
        for f in (261.6, 329.6, 392.0, 523.3):
            y += np.sin(2 * np.pi * f * t) * np.exp(-t * 2.2)
        y = y * 0.3 + np.sin(2 * np.pi * 55 * t) * np.exp(-t * 6) * 0.8
    else:
        y = _whoosh(0.3, 13) * 0.6
    y = y / (np.max(np.abs(y)) + 1e-9) * 0.8
    return _est(y)


def _sumar(y, i0, c):
    m = min(len(c), len(y) - i0)
    if m > 0 and i0 >= 0:
        y[i0:i0 + m] += c[:m]


def metronomo(duracion: float, bpm: float = 150.0, t0: float = 0.0) -> np.ndarray:
    """Metrónomo sintético: clic en cada pulso y acento más agudo y con bombo en cada compás."""
    n = int(duracion * SR)
    y = np.zeros(n)
    paso = 60.0 / bpm
    k = 0
    while t0 + k * paso < duracion:
        i0 = int((t0 + k * paso) * SR)
        acento = (k % 4 == 0)
        tt = _t(0.06)
        _sumar(y, i0, np.sin(2 * np.pi * (1760 if acento else 1175) * tt) * np.exp(-tt * 70) * (0.55 if acento else 0.32))
        if acento:
            tb = _t(0.16)
            _sumar(y, i0, np.sin(2 * np.pi * (48 + 90 * np.exp(-tb * 30)) * tb) * np.exp(-tb * 18) * 0.7)
        k += 1
    return _est(y)


# ── Mezcla ──────────────────────────────────────────────────────────────────────────────────────────────────────────

class Banco:
    """Carga (y guarda en caché) los efectos de sonido de una carpeta, con sustituto sintético si faltan."""

    def __init__(self, carpeta: str):
        self.carpeta = carpeta
        self._cache = {}
        self.sinteticos = set()

    def obtener(self, nombre: str):
        if nombre in self._cache:
            return self._cache[nombre]
        ruta = None
        for ext in (".wav", ".flac", ".ogg", ".mp3"):
            p = os.path.join(self.carpeta, nombre + ext)
            if os.path.exists(p):
                ruta = p
                break
        if ruta:
            try:
                x = cargar(ruta)
            except RuntimeError:
                x = sintetizar(nombre)
                self.sinteticos.add(nombre)
        else:
            x = sintetizar(nombre)
            self.sinteticos.add(nombre)
        # instante del pico (envolvente suavizada a ~8 ms)
        mono = np.abs(x).max(axis=1)
        k = max(1, int(0.008 * SR))
        env = np.convolve(mono, np.ones(k) / k, mode="same")
        pico = int(np.argmax(env)) if len(env) else 0
        self._cache[nombre] = (x, pico)
        return self._cache[nombre]


def _envolvente_ducking(n, eventos):
    """Curva de ganancia (0..1) de la música: baja rápido bajo cada efecto gordo y se recupera despacio."""
    g = np.ones(n, np.float32)
    for t_pico, db, largo in eventos:
        prof = 10 ** (db / 20.0)
        ataque = int(0.010 * SR)
        mantiene = int(min(max(largo * 0.55, 0.10), 0.5) * SR)
        libera = int(0.28 * SR)
        i0 = int(t_pico * SR) - ataque
        forma = np.concatenate([np.linspace(1.0, prof, ataque, endpoint=False), np.full(mantiene, prof),
                                prof + (1.0 - prof) * (1 - np.exp(-np.linspace(0, 5, libera)))]).astype(np.float32)
        a, b = max(i0, 0), min(i0 + len(forma), n)
        if b > a:
            g[a:b] = np.minimum(g[a:b], forma[a - i0:b - i0])
    return g


def mezclar(n_muestras: int, eventos: list, musica: np.ndarray | None, banco: Banco, bpm: float, ganancia_musica_db: float = 0.0,
            t0_metronomo: float = 0.0, log=print):
    """eventos: [{"t": s, "nombre": str, "db": float, "alinear": "pico"|"inicio"|"fin"}].
    Devuelve (mezcla float32 (n,2), informe)."""
    info = {"musica": bool(musica is not None), "sinteticos": []}
    if musica is None:
        base = metronomo(n_muestras / SR, bpm, t0_metronomo)
        info["metronomo"] = True
    else:
        base = np.zeros((n_muestras, 2), np.float32)
        m = musica[:n_muestras]
        base[:len(m)] = m
    base *= 10 ** (ganancia_musica_db / 20.0)
    duck = []
    capa = np.zeros((n_muestras, 2), np.float32)
    for ev in eventos:
        nombre = ev["nombre"]
        x, pico = banco.obtener(nombre)
        g = 10 ** ((GANANCIA_DB.get(nombre, -4.0) + ev.get("db", 0.0)) / 20.0)
        alin = ev.get("alinear") or ALINEAR_DEFECTO.get(nombre, "pico")
        if alin == "inicio":
            ref = 0
        elif alin == "fin":
            ref = len(x)
        else:
            ref = pico
        i0 = int(round(ev["t"] * SR)) - ref
        a, b = max(i0, 0), min(i0 + len(x), n_muestras)
        if b > a:
            capa[a:b] += x[a - i0:b - i0] * g
        db = DUCK_DB.get(nombre, 0.0) + min(ev.get("db", 0.0), 0.0) * 0.0
        if db < 0 and not ev.get("sin_duck"):
            duck.append((ev["t"], db, len(x) / SR))
    info["sinteticos"] = sorted(banco.sinteticos)
    if duck:
        base *= _envolvente_ducking(n_muestras, duck)[:, None]
    mezcla = base + capa
    # fundido de 40 ms al final para evitar el chasquido del corte
    f = int(0.04 * SR)
    mezcla[-f:] *= np.linspace(1, 0, f)[:, None].astype(np.float32)
    return mezcla, info


def normalizar(mezcla: np.ndarray, ruta_salida: str, objetivo_lufs: float = -14.0, pico_db: float = -1.5, log=print):
    """Limitador suave + loudnorm de ffmpeg en dos pasadas (mide y luego aplica una ganancia lineal). Escribe un WAV float de
    48 kHz. El limitador corta antes los picos que suman los efectos sobre la música (2 dB por debajo del pico objetivo)
    para que loudnorm pueda ir en modo lineal, sin comprimir dinámicamente el resultado."""
    exe = ffmpeg_exe()
    datos = np.ascontiguousarray(mezcla, np.float32).tobytes()
    entrada = ["-f", "f32le", "-ar", str(SR), "-ac", "2", "-i", "-"]
    lim = 10 ** ((pico_db - 0.5) / 20.0)
    r = subprocess.run([exe, "-hide_banner", "-nostats", "-v", "error"] + entrada +
                       ["-af", f"alimiter=limit={lim:.4f}:attack=3:release=80:level=disabled", "-f", "f32le", "-"],
                       input=datos, capture_output=True)
    if r.returncode == 0 and len(r.stdout) == len(datos):
        datos = r.stdout
    filtro1 = f"loudnorm=I={objetivo_lufs}:TP={pico_db}:LRA=11:print_format=json"
    r = subprocess.run([exe, "-hide_banner", "-nostats"] + entrada + ["-af", filtro1, "-f", "null", "-"],
                       input=datos, capture_output=True)
    txt = r.stderr.decode(errors="replace")
    m = re.search(r"\{[^{}]*\"input_i\"[^{}]*\}", txt, re.S)
    if m:
        med = json.loads(m.group(0))
        filtro2 = (f"loudnorm=I={objetivo_lufs}:TP={pico_db}:LRA=11:measured_I={med['input_i']}:measured_TP={med['input_tp']}:"
                   f"measured_LRA={med['input_lra']}:measured_thresh={med['input_thresh']}:offset={med['target_offset']}:linear=true:"
                   f"print_format=json")
        log(f"    audio: tras el limitador {float(med['input_i']):.1f} LUFS, pico {float(med['input_tp']):.1f} dBTP → {objetivo_lufs} LUFS")
    else:
        log("    audio: no se pudo medir la sonoridad; se aplica loudnorm en una pasada")
        filtro2 = f"loudnorm=I={objetivo_lufs}:TP={pico_db}:LRA=11:print_format=json"
    r = subprocess.run([exe, "-hide_banner", "-nostats", "-y"] + entrada + ["-af", filtro2 + ",aresample=48000", "-ar", str(SR),
                                                                             "-c:a", "pcm_f32le", ruta_salida],
                       input=datos, capture_output=True)
    if r.returncode != 0:
        raise RuntimeError("ffmpeg no pudo normalizar el audio: " + r.stderr.decode(errors="replace")[-400:])
    m2 = re.search(r"\"normalization_type\"\s*:\s*\"(\w+)\"", r.stderr.decode(errors="replace"))
    if m2 and m2.group(1) != "linear":
        log(f"    audio: aviso, loudnorm ha usado modo {m2.group(1)} (no lineal)")
