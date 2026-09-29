#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
musica.py - Música original y biblioteca de efectos del tráiler de Tortunavy.

Todo se sintetiza aquí con numpy (sin muestras externas, sin descargas) y es reproducible: la semilla es fija
(SEED) y cada generador aleatorio se deriva del nombre de su pieza. Ejecutar sin argumentos lo regenera todo:

    python Tools/Trailer/musica.py            # música 60 s y 30 s, efectos, verificación y espectrogramas
    python Tools/Trailer/musica.py 60         # solo la pista de 60 s (más su comprobación)
    python Tools/Trailer/musica.py 30 sfx     # combinaciones libres: 60 | 30 | sfx | verify

Salidas (todo dentro de Saved/Trailer/, que no se versiona):
    music/trailer_60.wav, music/trailer_30.wav          48 kHz, estéreo, 16 bits, -14 LUFS aprox., pico -1 dBTP
    music/beatmap_60.json, music/beatmap_30.json         pulsos, primeros pulsos de compás, tramos y golpes
    music/espectrograma_60.png, espectrograma_30.png     revisión visual de la estructura
    music/informe.json                                   mediciones de la comprobación final
    sfx/*.wav + sfx/index.json                           biblioteca de efectos (48 kHz, estéreo, 16 bits)

TEMPO: 150 BPM  ->  pulso 0,4 s (19200 muestras), compás 1,6 s (76800), semicorchea 0,1 s (4800).
TONALIDAD: sol mayor. Progresión del estribillo: G | D | Em | C. Los tramos misteriosos usan mi menor.

TRAMOS DEL PRINCIPAL (60 s):
    S1  0,0- 6,4  Gancho: pad oscuro de mi menor, campanas de sonar con eco, tics de reloj militar, golpes graves en
                  1,6 / 3,2 / 4,8, cresta de silbato de émbolo + ruido + redoble en el último compás. Hueco de 80 ms.
    S2  6,4-19,2  Cooperativo (8 compases): bombo a 1 y 3 (cada 2 pulsos), palmas a 2 y 4, marimba con el gancho,
                  bajo de tumbao, claves, hi-hats; a mitad entran steel drum, congas, cencerro y arpegios; los dos
                  últimos compases suben con bombo a negras, riser y redoble. Corte seco exacto en 19,2.
    S3 19,2-22,4  Corte: rayado de disco en 19,2, silencio, drones y cuenta 3-2-1 (20,0 / 20,8 / 21,6) sobre un riser y
                  un redoble que se aceleran. Hueco de 80 ms antes de la caída.
    S4 22,4-41,6  Carrera (12 compases): bombo en cada pulso, palmas+caja en 2 y 4, hi-hats dobles, golpe grave en cada
                  compás, bajo a contratiempo (después rodado), pads con compresión lateral, arpegios chiptune, gancho
                  en steel+marimba (A), en sierra tipo corneta (B) y a tope con todo (C). Relleno y hueco final.
    S5 41,6-51,2  Características (6 compases): golpes secos de acorde (pulso o medio pulso), silencio entre ellos;
                  la densidad crece compás a compás; el último compás es la subida con hueco antes del estribillo.
    S6 51,2-60,0  Final: estribillo 51,2-56,0 (3 compases, G | Em | C-D) con todo y lead de sierra + latón; pulso muerto
                  55,6-56,0 con swell inverso; «sting» del logo en 56,0 y cola hasta 60,0.

TRAMOS DEL VERTICAL (30 s): V1 0-3,2 (golpes en 0,0 y 1,6 + subida) | V2 3,2-19,2 (10 compases de la caída de S4)
    | V3 19,2-25,6 (compases 1, 2, 5 y 6 del patrón de S5) | V4 25,6-30,0 (sting del logo con cola).

INSTRUMENTOS: bombo (seno con barrido de tono + saturación), caja + palmas, hi-hats (ruido + cuadradas PolyBLEP), shaker,
    cencerro, congas, claves, marimba y steel drum (aditivas inarmónicas), bajo (sierra PolyBLEP + sub senoidal,
    filtro de estado variable con envolvente), lead de sierra (3 voces desafinadas + sub), latón, arpegios chiptune
    (pulso PolyBLEP), pads de supersierra con barrido de filtro, campanas FM, risers, swells inversos, golpes graves.
PARÁMETROS DE MEZCLA Y MASTER:
    compresión lateral del bombo (bajo 0,85 / pads 0,60 / arpegios 0,35 / lead 0,30; recuperación 0,11 s),
    automatización de energía por tramos (S1 -6 dB ... S4 0 dB ... S6 +0,5 dB, ver Song.energy),
    reverb convolucional larga (RT60 1,9 s) y corta (0,45 s) que se cortan en los cortes secos (Song.edges),
    filtro paso alto causal de 30 Hz, compresor de bus 2:1 (umbral -15 dBFS RMS, ataque 10 ms, suelta 150 ms),
    saturador suave (rodilla en 0,62), limitador de anticipación de 3 ms con suelta de 70 ms (techo -1,3 dBFS,
    bajado sola hasta que el pico verdadero x4 sea <= -1 dBTP) y calibración iterativa a -14 LUFS integrados
    (BS.1770-4: ponderación K, bloques de 400 ms, puertas -70 LU y -10 LU; error < 0,06 LU).
GOLPES (`hits` del beatmap): impact = golpe grave grande | drop = inicio de tramo a tope | accent = golpe de compás en
    S4/S6 | cut = acento cada 2 pulsos en S2 y golpe seco de S5 | count = cuenta 3-2-1 | scratch = rayado | sting = logo.
"""
import json
import sys
import time
import wave
import zlib
from pathlib import Path

import numpy as np

# ═══════════════════════════════════════════════════════════════════════════
# 0. Constantes
# ═══════════════════════════════════════════════════════════════════════════
SR = 48000
BPM = 150
BEAT = 60.0 / BPM          # 0,4 s
BAR = 4.0 * BEAT           # 1,6 s
SLOT = BEAT / 4.0          # 0,1 s (semicorchea)
GAP = 0.08                 # hueco de silencio previo a una caída
SEED = 20260928
TARGET_LUFS = -14.0
CEILING_DB = -1.3          # techo de muestra del limitador (deja margen al pico verdadero)
TP_MAX_DB = -1.0           # pico verdadero máximo exigido

ROOT = Path(__file__).resolve().parents[2]
OUT_MUSIC = ROOT / "Saved" / "Trailer" / "music"
OUT_SFX = ROOT / "Saved" / "Trailer" / "sfx"

# Ganancias de mezcla de los instrumentos (lineales, pico de cada golpe antes del bus)
G_KICK, G_SNARE, G_CLAP, G_HAT, G_OPEN, G_SHAKE = 0.62, 0.90, 0.75, 0.23, 0.19, 0.15
G_COW, G_CONGA, G_TICK, G_CLAVE = 0.25, 0.40, 0.30, 0.35
G_BASS, G_PAD, G_ARP, G_LEAD, G_MARIMBA, G_STEEL, G_BRASS = 0.70, 0.40, 0.60, 0.30, 0.55, 0.45, 0.28
G_BOOM, G_CRASH, G_RISER, G_SWELL = 0.60, 0.30, 0.34, 0.32


def smp(t):
    return int(round(float(t) * SR))


def rng_for(name):
    """Generador determinista por nombre (crc32 no cambia entre ejecuciones, a diferencia de hash())."""
    return np.random.default_rng((zlib.crc32(name.encode("utf-8")) ^ SEED) & 0xFFFFFFFF)


def hz(m):
    return 440.0 * 2.0 ** ((np.asarray(m, dtype=np.float64) - 69.0) / 12.0)


def tt(n):
    return np.arange(n, dtype=np.float64) / SR


def nrm(x, peak=1.0):
    return x * (peak / max(1e-12, float(np.max(np.abs(x)))))


def next_fast(n):
    """Menor tamaño >= n de la forma 2^a 3^b 5^c (las FFT de numpy van más rápido con ellos)."""
    best = 1 << max(0, (n - 1).bit_length())
    f5 = 1
    while f5 < best:
        f35 = f5
        while f35 < best:
            m = f35
            while m < n:
                m <<= 1
            best = min(best, m)
            f35 *= 3
        f5 *= 5
    return best


# ═══════════════════════════════════════════════════════════════════════════
# 1. Filtros, ventanas y osciladores
# ═══════════════════════════════════════════════════════════════════════════
def cos_ramp(n):
    return 0.5 - 0.5 * np.cos(np.linspace(0.0, np.pi, n))          # 0 -> 1


def fade_out(x, n):
    n = min(int(n), x.shape[-1])
    if n > 0:
        x[..., -n:] *= cos_ramp(n)[::-1].astype(x.dtype)
    return x


def fade_in(x, n):
    n = min(int(n), x.shape[-1])
    if n > 0:
        x[..., :n] *= cos_ramp(n).astype(x.dtype)
    return x


def pan2(x, p):
    """Mono -> estéreo con ley de potencia constante (p en [-1, 1])."""
    a = (float(np.clip(p, -1.0, 1.0)) + 1.0) * np.pi / 4.0
    return np.stack([x * np.cos(a), x * np.sin(a)]).astype(np.float32)


def lp_mask(f, fc, order=2):
    return 1.0 / np.sqrt(1.0 + (f / fc) ** (2 * order))


def hp_mask(f, fc, order=2):
    return 1.0 / np.sqrt(1.0 + (fc / np.maximum(f, 1e-3)) ** (2 * order))


def bp_mask(f, fc, sigma):
    """Campana gaussiana en octavas: f (bins,), fc (cuadros,) -> (cuadros, bins)."""
    lf = np.log2(np.maximum(f, 1.0))[None, :] - np.log2(fc)[:, None]
    return np.exp(-0.5 * (lf / sigma) ** 2)


def fft_filter(x, hp=None, lp=None, order=2):
    """Filtro estático de fase cero por FFT (con relleno para no envolver la señal)."""
    n = x.shape[-1]
    nf = next_fast(2 * n) if n < 200000 else next_fast(n + 20000)
    spec = np.fft.rfft(x, nf, axis=-1)
    f = np.fft.rfftfreq(nf, 1.0 / SR)
    m = np.ones_like(f)
    if hp:
        m = m * hp_mask(f, hp, order)
    if lp:
        m = m * lp_mask(f, lp, order)
    return np.fft.irfft(spec * m, nf, axis=-1)[..., :n]


def biquad_ir(b, a, n):
    """Respuesta al impulso de un biquad (recursión directa)."""
    h = np.zeros(n)
    x1 = x2 = y1 = y2 = 0.0
    for i in range(n):
        x0 = 1.0 if i == 0 else 0.0
        y0 = b[0] * x0 + b[1] * x1 + b[2] * x2 - a[1] * y1 - a[2] * y2
        h[i] = y0
        x2, x1, y2, y1 = x1, x0, y1, y0
    return h


def hp_causal(x, fc=30.0, dur=0.35):
    """Paso alto Butterworth de 2.º orden causal (sin pre-eco): el corte seco no se anticipa."""
    w = np.tan(np.pi * fc / SR)
    k = np.sqrt(2.0)
    n0 = 1.0 / (1.0 + k * w + w * w)
    b = [n0, -2.0 * n0, n0]
    a = [1.0, 2.0 * (w * w - 1.0) * n0, (1.0 - k * w + w * w) * n0]
    h = biquad_ir(b, a, int(dur * SR))
    return np.stack([fft_conv(c, h) for c in x]) if x.ndim == 2 else fft_conv(x, h)


def stft_apply(x, mask_fn, n_fft=2048):
    """Filtro que varía en el tiempo: mask_fn(f, t_centros) -> ganancias (cuadros, bins). Ventana de seno, salto n/4."""
    hop = n_fft // 4
    n = len(x)
    pad = n_fft
    nfr = (n + pad) // hop + 1
    buf = np.zeros((nfr + 3) * hop)
    buf[pad:pad + n] = x
    out = np.zeros((nfr + 3) * hop)
    win = np.sin(np.pi * (np.arange(n_fft) + 0.5) / n_fft)
    f = np.fft.rfftfreq(n_fft, 1.0 / SR)
    tc = (np.arange(nfr) * hop + n_fft / 2.0 - pad) / SR
    ob = out.reshape(-1, hop)
    for c0 in range(0, nfr, 1024):
        c1 = min(nfr, c0 + 1024)
        idx = np.arange(c0, c1)[:, None] * hop + np.arange(n_fft)[None, :]
        sp = np.fft.rfft(buf[idx] * win, axis=1)
        sp *= mask_fn(f, tc[c0:c1])
        fr = np.fft.irfft(sp, n_fft, axis=1) * win
        for k in range(4):
            ob[c0 + k:c1 + k] += fr[:, k * hop:(k + 1) * hop]
    return out[pad:pad + n] / 2.0


def fft_conv(x, h):
    """Convolución por FFT recortada a la longitud de x."""
    n = len(x) + len(h) - 1
    nf = next_fast(n)
    return np.fft.irfft(np.fft.rfft(x, nf) * np.fft.rfft(h, nf), nf)[:len(x)]


def band_noise(rng, n, lo=None, hi=None, order=3):
    return fft_filter(rng.standard_normal(n), hp=lo, lp=hi, order=order)


def _blep(t, dt):
    """Corrección PolyBLEP de 2 muestras (t, dt: fase y paso de fase, con broadcast)."""
    shape = np.broadcast(t, dt).shape
    t = np.broadcast_to(t, shape)
    dt = np.broadcast_to(dt, shape)
    out = np.zeros(shape)
    m1 = t < dt
    x = t[m1] / dt[m1]
    out[m1] = x + x - x * x - 1.0
    m2 = t > 1.0 - dt
    x = (t[m2] - 1.0) / dt[m2]
    out[m2] = x * x + x + x + 1.0
    return out


def osc_saw(ph, dt):
    return 2.0 * ph - 1.0 - _blep(ph, dt)


def osc_pulse(ph, dt, pw=0.5):
    v = np.where(ph < pw, 1.0, -1.0)
    return v + _blep(ph, dt) - _blep((ph - pw) % 1.0, dt)


def osc_const(f, n, kind="saw", pw=0.5, ph0=0.0):
    """Oscilador PolyBLEP de frecuencia constante."""
    dt = f / SR
    ph = (ph0 + dt * np.arange(n)) % 1.0
    return osc_saw(ph, dt) if kind == "saw" else osc_pulse(ph, dt, pw)


def svf_lp_batch(X, FC, q):
    """Paso bajo de estado variable (TPT) sobre un lote de notas (filas) con corte variable en el tiempo."""
    m, L = X.shape
    g = np.tan(np.pi * FC / SR)
    k = 1.0 / q
    a1 = 1.0 / (1.0 + g * (g + k))
    a2 = g * a1
    a3 = g * a2
    Xt = np.ascontiguousarray(X.T)
    A1, A2, A3 = (np.ascontiguousarray(a.T) for a in (a1, a2, a3))
    out = np.empty_like(Xt)
    ic1 = np.zeros(m)
    ic2 = np.zeros(m)
    for i in range(L):
        v3 = Xt[i] - ic2
        v1 = A1[i] * ic1 + A2[i] * v3
        v2 = ic2 + A2[i] * ic1 + A3[i] * v3
        ic1 = 2.0 * v1 - ic1
        ic2 = 2.0 * v2 - ic2
        out[i] = v2
    return np.ascontiguousarray(out.T)


# ═══════════════════════════════════════════════════════════════════════════
# 2. Reverberación
# ═══════════════════════════════════════════════════════════════════════════
def make_ir(rt60, predelay, name, hf=0.4):
    """Respuesta al impulso estéreo: ruido decorrelado con caída por bandas (los agudos mueren antes)."""
    rng = rng_for(name)
    n = smp(rt60 * 1.1 + predelay)
    t = tt(n)
    ir = np.zeros((2, n))
    for c in range(2):
        nz = rng.standard_normal(n)
        lo = fft_filter(nz, lp=600.0)
        mid = fft_filter(nz, hp=600.0, lp=4500.0)
        hi = fft_filter(nz, hp=4500.0, lp=14000.0)
        y = (lo * np.exp(-6.91 * t / rt60) + 0.9 * mid * np.exp(-6.91 * t / (rt60 * 0.75))
             + 0.6 * hi * np.exp(-6.91 * t / (rt60 * hf)))
        y[:smp(predelay)] = 0.0
        ir[c] = y
    build = min(n, smp(0.012))
    ir[:, smp(predelay):smp(predelay) + build] *= cos_ramp(build)[None, :]
    # reflexiones tempranas
    for k in range(7):
        i = smp(predelay + rng.uniform(0.004, 0.045))
        for c in range(2):
            ir[c, i] += rng.choice([-1.0, 1.0]) * rng.uniform(0.25, 0.6) * np.abs(ir[c]).max() * 0.5
    return ir / np.sqrt((ir[0] ** 2).sum() + (ir[1] ** 2).sum()) * np.sqrt(2.0)


_IRS = {}


def get_ir(kind):
    if kind not in _IRS:
        _IRS[kind] = make_ir(1.9, 0.016, "ir_larga") if kind == "long" else make_ir(0.45, 0.004, "ir_corta", hf=0.35)
    return _IRS[kind]


def reverb_mono(x, kind="long"):
    ir = get_ir(kind)
    return np.stack([fft_conv(x, ir[0]), fft_conv(x, ir[1])])


# ═══════════════════════════════════════════════════════════════════════════
# 3. Muestras de percusión y efectos base
# ═══════════════════════════════════════════════════════════════════════════
def make_kick(rng, f_end, f_start, tau_p, tau_a, dur, drive, click):
    n = smp(dur)
    t = tt(n)
    f = f_end + (f_start - f_end) * np.exp(-t / tau_p)
    ph = 2.0 * np.pi * np.cumsum(f) / SR
    body = np.sin(ph) * (1.0 - np.exp(-t / 0.0006)) * np.exp(-t / tau_a)
    ck = band_noise(rng, n, 2200, 10000) * np.exp(-t / 0.0016)
    ck = ck / (np.abs(ck).max() + 1e-9) * click
    ck += np.sin(2 * np.pi * 1700 * t) * np.exp(-t / 0.003) * click * 0.5
    y = np.tanh(drive * (body + ck)) / np.tanh(drive)
    fade_out(y, smp(0.03))
    return nrm(y)


def make_snare(rng, f0=175.0, dur=0.34, noise_dec=0.085):
    n = smp(dur)
    t = tt(n)
    f = f0 + 85.0 * np.exp(-t / 0.011)
    tone = np.sin(2 * np.pi * np.cumsum(f) / SR) * np.exp(-t / 0.05)
    body = band_noise(rng, n, 1400, 13000, 2) * np.exp(-t / noise_dec)
    snap = band_noise(rng, n, 3500, 14000) * np.exp(-t / 0.005)
    y = (0.75 * tone + 0.55 * body + 0.5 * snap) * (1.0 - np.exp(-t / 0.0004))
    y = np.tanh(1.5 * y) / np.tanh(1.5)
    fade_out(y, smp(0.02))
    return nrm(y)


def make_clap(rng, dur=0.36):
    n = smp(dur)
    nz = band_noise(rng, n, 900, 4200, 3)
    env = np.zeros(n)
    for t0, a in zip([0.0, 0.011, 0.023], [0.55, 0.7, 0.85]):
        i = smp(t0)
        env[i:] += a * np.exp(-np.arange(n - i) / (0.006 * SR))
    i = smp(0.035)
    k = np.arange(n - i) / SR
    env[i:] += np.exp(-k / 0.006) * 0.9 + np.exp(-k / 0.07) * 0.8
    y = nz * env
    fade_out(y, smp(0.03))
    return nrm(y)


def make_hat(rng, dec, dur):
    n = smp(dur)
    t = tt(n)
    nz = band_noise(rng, n, 7000, 18000, 3)
    metal = np.zeros(n)
    for f in (205.3, 304.4, 369.6, 522.7, 540.0, 800.0):
        metal += osc_const(f * 2.0, n, "pulse", 0.5, rng.random())
    metal = fft_filter(metal, hp=7000, order=3)
    y = 0.7 * nz / nz.std() + 0.35 * metal / (metal.std() + 1e-9)
    y *= (1.0 - np.exp(-t / 0.0004)) * np.exp(-t / dec)
    fade_out(y, smp(0.01))
    return nrm(y)


def make_shaker(rng, dur=0.16):
    n = smp(dur)
    t = tt(n)
    y = band_noise(rng, n, 4500, 13000, 2) * (1.0 - np.exp(-t / 0.006)) * np.exp(-t / 0.028)
    fade_out(y, smp(0.01))
    return nrm(y)


def make_cowbell(rng, dur=0.34):
    n = smp(dur)
    t = tt(n)
    y = osc_const(540.0, n, "pulse") + osc_const(800.0, n, "pulse")
    y = fft_filter(y, hp=450, lp=3600, order=2)
    y = y * (0.55 * np.exp(-t / 0.012) + 0.45 * np.exp(-t / 0.10)) * (1.0 - np.exp(-t / 0.0005))
    fade_out(y, smp(0.02))
    return nrm(y)


def make_conga(rng, f0, dur=0.28):
    n = smp(dur)
    t = tt(n)
    f = f0 + 0.45 * f0 * np.exp(-t / 0.018)
    y = np.sin(2 * np.pi * np.cumsum(f) / SR) * np.exp(-t / 0.085)
    y += 0.4 * np.sin(2 * np.pi * np.cumsum(f * 2.3) / SR) * np.exp(-t / 0.03)
    y += band_noise(rng, n, 1800, 6000, 2) * np.exp(-t / 0.006) * 0.5
    y *= (1.0 - np.exp(-t / 0.0005))
    fade_out(y, smp(0.02))
    return nrm(y)


def make_wood(rng, f=1150.0, dec=0.022, dur=0.16):
    n = smp(dur)
    t = tt(n)
    y = np.sin(2 * np.pi * f * t) * np.exp(-t / dec) + 0.5 * np.sin(2 * np.pi * f * 2.42 * t) * np.exp(-t / (dec * 0.5))
    y += band_noise(rng, n, 2500, 9000, 2) * np.exp(-t / 0.002) * 0.4
    y *= (1.0 - np.exp(-t / 0.0003))
    fade_out(y, smp(0.02))
    return nrm(y)


def make_crash(rng, dur=2.8):
    n = smp(dur)
    t = tt(n)
    nz = band_noise(rng, n, 3200, 17000, 2)
    metal = np.zeros(n)
    for r in (1.0, 1.4471, 1.7091, 1.9545, 2.5, 2.7364, 3.3, 3.9091):
        metal += osc_const(320.0 * r, n, "pulse", 0.5, rng.random())
    metal = fft_filter(metal, hp=3000, order=2)
    y = 0.8 * nz / nz.std() + 0.4 * metal / (metal.std() + 1e-9)
    y *= (1.0 - np.exp(-t / 0.0006)) * (0.55 * np.exp(-t / 0.14) + 0.45 * np.exp(-t / 0.95))
    fade_out(y, smp(0.05))
    return nrm(y)


def make_boom(rng, dur, f0, f1, tau_f, tau_a, drive, thump=0.5):
    n = smp(dur)
    t = tt(n)
    f = f1 + (f0 - f1) * np.exp(-t / tau_f)
    ph = 2.0 * np.pi * np.cumsum(f) / SR
    body = (np.sin(ph) + 0.3 * np.sin(2 * ph) * np.exp(-t / 0.15)) * (1.0 - np.exp(-t / 0.0015)) * np.exp(-t / tau_a)
    th = fft_filter(rng.standard_normal(n), lp=500) * np.exp(-t / 0.04)
    th = th / (np.abs(th).max() + 1e-9) * thump
    y = np.tanh(drive * (body + th)) / np.tanh(drive)
    fade_out(y, smp(0.08))
    return nrm(y)


def make_glock(m, dur=1.6):
    f = float(hz(m))
    n = smp(dur)
    t = tt(n)
    y = np.zeros(n)
    for r, a, tau in ((1.0, 1.0, 0.9), (2.76, 0.45, 0.35), (5.4, 0.22, 0.14), (8.9, 0.1, 0.07)):
        if f * r < 20000:
            y += a * np.sin(2 * np.pi * f * r * t) * np.exp(-t / tau)
    y *= (1.0 - np.exp(-t / 0.0004))
    fade_out(y, smp(0.05))
    return nrm(y)


def make_fm_bell(m, dur=2.4, ratio=3.5, index=2.6):
    f = float(hz(m))
    n = smp(dur)
    t = tt(n)
    idx = index * np.exp(-t / 0.35)
    y = np.sin(2 * np.pi * f * t + idx * np.sin(2 * np.pi * f * ratio * t)) * np.exp(-t / 0.9)
    y += 0.3 * np.sin(2 * np.pi * f * 2.0 * t) * np.exp(-t / 0.5)
    y *= (1.0 - np.exp(-t / 0.0008))
    fade_out(y, smp(0.06))
    return nrm(y)


def make_riser(dur, seed, f_lo=250.0, f_hi=9000.0, curve=2.0, whistle=True, tone=True):
    """Riser: ruido con paso banda que sube + barrido de sierra + silbato de émbolo (toque cartoon)."""
    rng = rng_for(seed)
    n = smp(dur)
    t = tt(n)
    u = t / dur
    fc = f_lo * (f_hi / f_lo) ** (u ** 1.3)
    nz = stft_apply(rng.standard_normal(n), lambda f, tc: bp_mask(f, np.interp(tc, t, fc), 0.9))
    nz = nz / (nz.std() + 1e-9)
    amp = u ** curve
    y = nz * amp
    if tone:
        fsw = 110.0 * (2200.0 / 110.0) ** u
        dt = fsw / SR
        s = osc_saw(np.cumsum(dt) % 1.0, dt)
        s = fft_filter(s, lp=5000)
        y += 0.9 * s * amp
    if whistle:
        fw = 450.0 * (2700.0 / 450.0) ** (u ** 1.4)
        vib = 1.0 + 0.03 * np.sin(2 * np.pi * 6.5 * t) * u
        y += 0.5 * np.sin(2 * np.pi * np.cumsum(fw * vib) / SR) * amp
    fade_in(y, 16)
    return nrm(y)


def make_scratch(rng, dur=0.85):
    """Rayado de disco: un acorde de sierras leído hacia delante y hacia atrás y frenado al final."""
    n_src = smp(0.6)
    src = np.zeros(n_src)
    for m, a in ((55, 1.0), (62, 0.8), (71, 0.7), (79, 0.5)):
        src += a * osc_const(float(hz(m)), n_src, "saw", ph0=rng.random())
    src = fft_filter(src, hp=150, lp=3800)
    n = smp(dur)
    t = tt(n)
    kt = [0.0, 0.06, 0.12, 0.19, 0.25, 0.31, 0.36, 0.40]
    kp = [0.05, 0.24, 0.07, 0.27, 0.10, 0.29, 0.13, 0.31]
    pos = np.where(t < 0.40, np.interp(t, kt, kp), 0.31 + 0.09 * (1.0 - np.exp(-(t - 0.40) / 0.12)))
    k = 96
    pos = np.convolve(np.pad(pos, (k, k), mode="edge"), np.ones(k) / k, mode="same")[k:-k]
    rate = np.abs(np.gradient(pos, 1.0 / SR))
    y = np.interp(pos * SR, np.arange(n_src), src)
    y *= np.tanh(2.2 * rate)
    crackle = band_noise(rng, n, 1500, 9000, 2) * np.tanh(2.0 * rate) * 0.28 * y.std()
    y = y + crackle
    y *= np.exp(-np.maximum(t - 0.55, 0.0) / 0.10)
    fade_out(y, smp(0.03))
    return nrm(y)


class Bank:
    """Muestras de percusión y efectos, generadas una sola vez (semilla fija)."""

    def __init__(self):
        r = rng_for("banco")
        self.kick = make_kick(r, 50.0, 205.0, 0.026, 0.115, 0.40, 2.7, 0.35)
        self.kick_deep = make_kick(r, 46.0, 150.0, 0.036, 0.21, 0.62, 1.7, 0.16)
        self.snare = [make_snare(r, 175.0), make_snare(r, 182.0), make_snare(r, 168.0)]
        self.clap = [make_clap(r), make_clap(r)]
        self.hat_closed = [make_hat(r, 0.024, 0.16), make_hat(r, 0.032, 0.18), make_hat(r, 0.028, 0.17)]
        self.hat_open = make_hat(r, 0.10, 0.45)
        self.shaker = [make_shaker(r), make_shaker(r)]
        self.cowbell = make_cowbell(r)
        self.conga_hi = make_conga(r, 330.0)
        self.conga_lo = make_conga(r, 210.0)
        self.clave = make_wood(r, 2300.0, 0.014, 0.12)
        self.wood = make_wood(r, 980.0, 0.024, 0.18)
        self.crash = make_crash(r, 2.8)
        self.boom_big = make_boom(r, 2.6, 118.0, 40.0, 0.17, 0.62, 1.9, 0.55)
        self.boom_small = make_boom(r, 0.7, 92.0, 44.0, 0.05, 0.18, 1.6, 0.35)
        self.tom = make_conga(r, 100.0, 0.4)


_BANK = None


def bank():
    global _BANK
    if _BANK is None:
        _BANK = Bank()
    return _BANK


_NOTE_CACHE = {}


def marimba_sample(m):
    key = ("mar", int(m))
    if key not in _NOTE_CACHE:
        f = float(hz(m))
        rng = rng_for("marimba%d" % m)
        tau = float(np.clip(0.62 * (261.6 / f) ** 0.45, 0.14, 1.1))
        n = smp(min(tau * 5.0, 1.6))
        t = tt(n)
        y = np.sin(2 * np.pi * f * t) * np.exp(-t / tau)
        y += 0.38 * np.sin(2 * np.pi * f * 3.98 * t + 0.3) * np.exp(-t / (tau * 0.30))
        if f * 9.6 < 20000:
            y += 0.10 * np.sin(2 * np.pi * f * 9.6 * t + 1.1) * np.exp(-t / (tau * 0.10))
        y += band_noise(rng, n, 900, 4500, 2) * np.exp(-t / 0.004) * 0.16
        y *= (1.0 - np.exp(-t / 0.0004))
        fade_out(y, smp(0.02))
        _NOTE_CACHE[key] = nrm(y)
    return _NOTE_CACHE[key]


def steel_sample(m):
    key = ("steel", int(m))
    if key not in _NOTE_CACHE:
        f = float(hz(m))
        rng = rng_for("steel%d" % m)
        n = smp(1.4)
        t = tt(n)
        y = np.zeros(n)
        for r, a, tau in ((1.0, 1.0, 0.55), (2.0, 0.65, 0.42), (3.0, 0.30, 0.22), (4.02, 0.16, 0.12), (5.1, 0.07, 0.07)):
            if f * r > 19000:
                continue
            for det in (-1.0, 1.0):
                fr = f * r * (1.0 + det * 0.0011 * (1.0 + 0.3 * r))
                y += 0.5 * a * np.sin(2 * np.pi * fr * t + rng.uniform(0, 6.28)) * np.exp(-t / tau)
        y *= 1.0 + 0.05 * np.sin(2 * np.pi * 5.2 * t)
        y += band_noise(rng, n, 1800, 6000, 2) * np.exp(-t / 0.006) * 0.22
        y *= (1.0 - np.exp(-t / 0.0005))
        fade_out(y, smp(0.04))
        _NOTE_CACHE[key] = nrm(y)
    return _NOTE_CACHE[key]


# ═══════════════════════════════════════════════════════════════════════════
# 4. Instrumentos por lotes (sierra / pulso PolyBLEP + filtro de estado variable)
# ═══════════════════════════════════════════════════════════════════════════
# voces: (desafinación en cents, panorama, onda, nivel, ancho de pulso)
SPECS = {
    "bass": dict(stem="bass", voices=[(0, 0.0, "saw", 0.62, 0.5)], sub=0.60, att=0.003, dec=None, sus=1.0, rel=0.03,
                 fc0=380, fc1=2400, tau=0.07, q=1.5, kt=0.0, drive=1.6, gain=G_BASS, sl=0.0, ss=0.0, ph="zero"),
    "lead": dict(stem="lead", voices=[(-10, -0.7, "saw", 0.36, 0.5), (0, 0.0, "saw", 0.36, 0.5),
                                      (10, 0.7, "saw", 0.36, 0.5), (-1200, 0.0, "pulse", 0.16, 0.5)],
                 att=0.006, dec=0.5, sus=0.75, rel=0.07, fc0=2600, fc1=5200, tau=0.14, q=1.0, kt=0.35,
                 vib=(5.6, 10.0, 0.12), gain=G_LEAD, sl=0.22, ss=0.05),
    "arp": dict(stem="arps", voices=[(0, -0.4, "pulse", 0.5, 0.25), (6, 0.4, "pulse", 0.45, 0.5)],
                att=0.001, dec=0.07, sus=0.0, rel=0.01, fc0=4200, fc1=6500, tau=0.05, q=0.9, kt=0.3,
                gain=G_ARP, sl=0.14, ss=0.0),
    "stab": dict(stem="lead", voices=[(-9, -0.8, "saw", 0.4, 0.5), (0, 0.0, "saw", 0.4, 0.5), (9, 0.8, "saw", 0.4, 0.5)],
                 att=0.002, dec=None, sus=1.0, rel=0.02, fc0=3000, fc1=4500, tau=0.05, q=0.9, kt=0.2,
                 gain=0.22, sl=0.0, ss=0.0),
    "brass": dict(stem="lead", voices=[(-7, -0.5, "saw", 0.5, 0.5), (7, 0.5, "saw", 0.5, 0.5)],
                  att=0.035, dec=None, sus=1.0, rel=0.06, fc0=2800, fc1=-1700, tau=0.07, q=0.8, kt=0.3,
                  vib=(5.2, 14.0, 0.15), gain=G_BRASS, sl=0.2, ss=0.0),
}


def render_group(song, sp, items, chunk=40):
    rel = sp.get("rel", 0.05)
    rng = rng_for("grupo_%s_%d" % (sp["stem"], len(items)))
    vib = sp.get("vib")
    for c0 in range(0, len(items), chunk):
        ch = items[c0:c0 + chunk]
        m = len(ch)
        L = smp(max(it["dur"] for it in ch) + 6.0 * rel)
        t = tt(L)
        f0 = hz([it["m"] for it in ch])[:, None]
        dur = np.array([it["dur"] for it in ch])[:, None]
        vel = np.array([it["vel"] for it in ch])
        if vib:
            rate, cents, delay = vib
            ramp = np.clip((t - delay) / 0.15, 0.0, 1.0)
            F = f0 * np.exp(np.log(2.0) * (cents / 1200.0) * np.sin(2 * np.pi * rate * t) * ramp)[None, :]
        else:
            F = np.repeat(f0, L, axis=1)
        oL = np.zeros((m, L))
        oR = np.zeros((m, L))
        for cents, pan, wave_, lvl, pw in sp["voices"]:
            dt = F * (2.0 ** (cents / 1200.0)) / SR
            ph0 = rng.random((m, 1)) if sp.get("ph", "rand") == "rand" else np.zeros((m, 1))
            ph = (ph0 + np.cumsum(dt, axis=1)) % 1.0
            w = osc_saw(ph, dt) if wave_ == "saw" else osc_pulse(ph, dt, pw)
            a = (np.clip(pan, -1, 1) + 1.0) * np.pi / 4.0
            oL += lvl * w * (np.cos(a) * np.sqrt(2.0))
            oR += lvl * w * (np.sin(a) * np.sqrt(2.0))
        sus = sp.get("sus", 1.0)
        dec = sp.get("dec")
        A = np.minimum(1.0, t / sp["att"])
        D = (sus + (1.0 - sus) * np.exp(-t / dec)) if dec else np.ones(L)
        gate = np.where(t[None, :] < dur, 1.0, np.exp(-(t[None, :] - dur) / rel))
        env = (A * D)[None, :] * gate * vel[:, None]
        fc = (sp["fc0"] + sp["fc1"] * np.exp(-t / sp["tau"]))[None, :] * (f0 / 440.0) ** sp.get("kt", 0.0)
        fc = np.clip(fc * (0.55 + 0.45 * vel)[:, None], 60.0, 0.45 * SR)
        Y = svf_lp_batch(np.concatenate([oL * env, oR * env]), np.concatenate([fc, fc]), sp.get("q", 0.8))
        yL, yR = Y[:m], Y[m:]
        sub = sp.get("sub", 0.0)
        if sub:
            s = sub * np.sin(2 * np.pi * f0 * t[None, :]) * env
            yL = yL + s
            yR = yR + s
        drv = sp.get("drive", 1.0)
        if drv != 1.0:
            yL = np.tanh(drv * yL) / drv
            yR = np.tanh(drv * yR) / drv
        for i, it in enumerate(ch):
            n_i = min(L, smp(it["dur"] + 5.0 * rel))
            sig = np.stack([yL[i, :n_i], yR[i, :n_i]]).astype(np.float32)
            pan = it.get("pan", 0.0)
            if pan:
                a = (np.clip(pan, -1, 1) + 1.0) * np.pi / 4.0
                sig[0] *= np.cos(a) * np.sqrt(2.0)
                sig[1] *= np.sin(a) * np.sqrt(2.0)
            fade_out(sig, 64)
            song.put(sp["stem"], sig, it["t"], gain=sp["gain"], sl=sp.get("sl", 0.0), ss=sp.get("ss", 0.0))


# ═══════════════════════════════════════════════════════════════════════════
# 5. La canción: pistas, cortes secos, compresión lateral, mezcla
# ═══════════════════════════════════════════════════════════════════════════
class Stem:
    def __init__(self, N, depth):
        self.dry = np.zeros((2, N), np.float32)
        self.sl = np.zeros(N, np.float32)          # envío a la reverb larga (mono)
        self.ss = np.zeros(N, np.float32)          # envío a la reverb corta (mono)
        self.depth = depth                          # profundidad de la compresión lateral


STEM_DEPTH = {"kick": 0.0, "drums": 0.0, "bass": 0.85, "pads": 0.60, "arps": 0.35, "lead": 0.30, "fx": 0.0}
PRIO = {"sting": 7, "scratch": 6, "drop": 5, "impact": 4, "count": 3, "accent": 2, "cut": 1}


class Song:
    def __init__(self, name, duration, cuts):
        self.name = name
        self.duration = duration
        self.N = smp(duration)
        self.edges = sorted(smp(c) for c in cuts)   # cortes secos: cortan notas y colas de reverb
        self.stems = {k: Stem(self.N, d) for k, d in STEM_DEPTH.items()}
        self.duck_ev = []
        self.hits_raw = []
        self.queue = {k: [] for k in SPECS}
        self.pad_lp = []
        self.energy = []                 # (t, dB): automatización de energía de toda la mezcla (subida S2 -> S4)
        self.dry_mode = False            # sin envíos de reverb (golpes secos de S5)
        self.rng = rng_for("cancion_" + name)

    # ── bajo nivel ─────────────────────────────────────────────────────────
    def cut_after_sample(self, i):
        for e in self.edges:
            if e > i:
                return e
        return self.N

    def put(self, stem, sig, t0, pan=0.0, gain=1.0, sl=0.0, ss=0.0, cut=True):
        i0 = smp(t0)
        if i0 >= self.N or i0 < 0:
            return
        sig = np.asarray(sig, np.float32)
        n = sig.shape[-1]
        limit = min(self.cut_after_sample(i0) if cut else self.N, self.N)
        if i0 + n > limit:
            n = limit - i0
            if n <= 0:
                return
            sig = sig[..., :n].copy()
            fade_out(sig, min(n, 96))
        end = i0 + n
        st = self.stems[stem]
        if self.dry_mode:
            sl = ss = 0.0
        stereo = pan2(sig, pan) if sig.ndim == 1 else sig
        st.dry[:, i0:end] += stereo * gain
        if sl or ss:
            mono = sig if sig.ndim == 1 else sig.mean(axis=0)
            if sl:
                st.sl[i0:end] += mono * (gain * sl)
            if ss:
                st.ss[i0:end] += mono * (gain * ss)

    def duck(self, t, strength=1.0, tau=0.11):
        self.duck_ev.append((t, strength, tau))

    def hit(self, t, kind):
        if 0.0 <= t < self.duration:
            self.hits_raw.append((round(t, 4), kind))

    def note(self, grp, t, m, dur, vel=1.0, pan=0.0):
        if t < self.duration:
            self.queue[grp].append(dict(t=t, m=m, dur=dur, vel=vel, pan=pan))

    # ── percusión ───────────────────────────────────────────────────────────
    def kick(self, t, vel=1.0, deep=False):
        b = bank()
        self.put("kick", b.kick_deep if deep else b.kick, t, gain=G_KICK * vel)
        self.duck(t, 1.0, 0.18 if deep else 0.11)

    def snare(self, t, vel=1.0, pan=0.0):
        b = bank()
        self.put("drums", b.snare[int(self.rng.integers(len(b.snare)))], t, pan=pan, gain=G_SNARE * vel, ss=0.12)

    def clap(self, t, vel=1.0, pan=0.0):
        b = bank()
        self.put("drums", b.clap[int(self.rng.integers(len(b.clap)))], t, pan=pan, gain=G_CLAP * vel, ss=0.16, sl=0.05)

    def hat(self, t, vel=1.0, open_=False, pan=0.0):
        b = bank()
        if open_:
            sig = b.hat_open[:smp(0.22)].copy()
            fade_out(sig, 360)
            self.put("drums", sig, t, pan=pan, gain=G_OPEN * vel)
        else:
            self.put("drums", b.hat_closed[int(self.rng.integers(len(b.hat_closed)))], t, pan=pan, gain=G_HAT * vel)

    def shaker(self, t, vel=1.0, pan=0.0):
        b = bank()
        self.put("drums", b.shaker[int(self.rng.integers(len(b.shaker)))], t, pan=pan, gain=G_SHAKE * vel)

    def perc(self, kind, t, vel=1.0, pan=0.0):
        b = bank()
        table = {"cowbell": (b.cowbell, G_COW, 0.06), "conga_hi": (b.conga_hi, G_CONGA, 0.06),
                 "conga_lo": (b.conga_lo, G_CONGA, 0.06), "clave": (b.clave, G_CLAVE, 0.08),
                 "wood": (b.wood, G_TICK, 0.14), "tom": (b.tom, 0.5, 0.05)}
        sig, g, ss = table[kind]
        self.put("drums", sig, t, pan=pan, gain=g * vel, ss=ss)

    def roll(self, segs, v0, v1, pan_w=0.3):
        """Redoble de caja: segs = [(t_ini, t_fin, paso)], crescendo de v0 a v1."""
        times = []
        for a, b_, step in segs:
            k = int(round((b_ - a) / step))
            times += [a + i * step for i in range(k)]
        for i, t in enumerate(times):
            u = i / max(1, len(times) - 1)
            self.snare(t, v0 + (v1 - v0) * u, pan=pan_w * (1 if i % 2 else -1))
            if u > 0.6 and i % 2 == 0:
                self.clap(t, 0.5 * (v0 + (v1 - v0) * u))

    # ── efectos ─────────────────────────────────────────────────────────────
    def boom(self, t, size=1.0):
        b = bank()
        big = size > 0.7
        self.put("fx", b.boom_big if big else b.boom_small, t, gain=G_BOOM * size, sl=0.16 if big else 0.05)

    def crash(self, t, vel=1.0):
        self.put("fx", bank().crash, t, gain=G_CRASH * vel, sl=0.35)

    def swell(self, t_end, dur, vel=1.0, end_gap=0.0):
        """Golpe de platillo al revés que acaba en t_end - end_gap (un respiro delante del golpe lo hace más seco)."""
        sig = bank().crash[:smp(dur * 1.5)][::-1].copy()
        sig = sig[-smp(dur):]
        sig = sig * (np.linspace(0, 1, len(sig)) ** 1.5)
        fade_out(sig, 96)
        self.put("fx", sig, t_end - end_gap - dur, gain=G_SWELL * vel, sl=0.1, cut=False)

    def riser(self, t0, t1, vel=1.0, **kw):
        sig = make_riser(t1 - t0, "riser%.3f" % (t1 - t0), **kw)
        self.put("fx", sig, t0, gain=G_RISER * vel, sl=0.15)

    def impact(self, t, size=1.0, kind="impact", crash=False):
        self.boom(t, size)
        self.duck(t, min(1.0, size), 0.30)
        if crash:
            self.crash(t, 0.55 + 0.45 * size)
        self.hit(t, kind)

    def sub_tone(self, t, m, dur, vel=1.0, tau=None):
        n = smp(dur)
        tv = tt(n)
        env = (1.0 - np.exp(-tv / 0.006)) * (np.exp(-tv / tau) if tau else 1.0)
        y = np.sin(2 * np.pi * float(hz(m)) * tv) * env
        fade_out(y, smp(0.05))
        self.put("fx", y, t, gain=0.5 * vel)

    # ── tonales ─────────────────────────────────────────────────────────────
    def marimba(self, t, m, vel=1.0, pan=0.0, ring=None):
        sig = marimba_sample(m)
        if ring is not None:
            sig = sig[:smp(ring)].copy()
            fade_out(sig, 900)
        self.put("lead", sig, t, pan=pan, gain=G_MARIMBA * vel, sl=0.18, ss=0.04)

    def steel(self, t, m, vel=1.0, pan=0.0, ring=None):
        sig = steel_sample(m)
        if ring is not None:
            sig = sig[:smp(ring)].copy()
            fade_out(sig, 1200)
        self.put("lead", sig, t, pan=pan, gain=G_STEEL * vel, sl=0.26)

    def glock(self, t, m, vel=1.0, pan=0.0):
        self.put("lead", make_glock(m), t, pan=pan, gain=0.30 * vel, sl=0.4)

    def bell(self, t, m, vel=1.0, pan=0.0, sl=0.5):
        self.put("lead", make_fm_bell(m), t, pan=pan, gain=0.30 * vel, sl=sl)

    def pad(self, t0, dur, midis, gain=1.0, att=0.06, rel=0.12, width=0.8, sl=0.35, decay=None, det=(-13, 0, 13)):
        n = smp(dur + rel * 5.0)
        t = tt(n)
        env = np.minimum(1.0, t / att) * np.where(t < dur, 1.0, np.exp(-(t - dur) / rel))
        if decay:
            env = env * np.exp(-t / decay)
        oL = np.zeros(n)
        oR = np.zeros(n)
        idx = np.arange(n)
        for m in midis:
            for k, c in enumerate(det):
                dt = float(hz(m)) * 2.0 ** (c / 1200.0) / SR
                w = osc_saw((self.rng.random() + dt * idx) % 1.0, dt)
                a = ((k - (len(det) - 1) / 2.0) * width * 2.0 / max(1, len(det) - 1) + 1.0) * np.pi / 4.0
                a = np.clip(a, 0.0, np.pi / 2.0)
                oL += w * np.cos(a)
                oR += w * np.sin(a)
        norm = 1.0 / np.sqrt(len(midis) * len(det))
        sig = np.stack([oL, oR]) * (env * norm * gain)[None, :]
        fade_out(sig, 96)
        self.put("pads", sig, t0, gain=G_PAD, sl=sl)

    # ── mezcla ──────────────────────────────────────────────────────────────
    def render_groups(self):
        for name, items in self.queue.items():
            if items:
                render_group(self, SPECS[name], sorted(items, key=lambda it: it["dur"]))
        self.queue = {k: [] for k in SPECS}

    def duck_curve(self):
        d = np.zeros(self.N, np.float32)
        for t, s, tau in self.duck_ev:
            i = smp(t)
            if i >= self.N:
                continue
            L = int(tau * 6.0 * SR)
            seg = s * np.exp(-np.arange(L) / (tau * SR))
            j = min(self.N, i + L)
            d[i:j] = np.maximum(d[i:j], seg[:j - i])
            a = min(i, 96)
            if a:
                d[i - a:i] = np.maximum(d[i - a:i], np.linspace(0, s, a, endpoint=False))
        return d

    def pad_filter(self, x):
        pts = sorted(self.pad_lp) if self.pad_lp else [(0.0, 4000.0), (self.duration, 4000.0)]
        ts = np.array([p[0] for p in pts])
        lf = np.log([p[1] for p in pts])

        def mask(f, tc):
            fc = np.exp(np.interp(tc, ts, lf))
            f2 = np.maximum(f, 1.0)[None, :]
            return (1.0 / np.sqrt(1.0 + (f2 / fc[:, None]) ** 4)) * (1.0 / np.sqrt(1.0 + (170.0 / f2) ** 4))

        y = np.stack([stft_apply(x[0], mask), stft_apply(x[1], mask)])
        act = (np.abs(x).max(axis=0) > 1e-9).astype(np.float64)              # el filtro no debe rebasar los cortes
        k = 48
        act = np.convolve(act, np.ones(2 * k + 1), mode="same") > 0
        return y * act[None, :]

    def reverb(self, sl, ss):
        """Reverbs por dominios: entre dos cortes secos la cola no atraviesa el corte."""
        wet = np.zeros((2, self.N))
        bounds = [0] + self.edges + [self.N]
        for a, b in zip(bounds[:-1], bounds[1:]):
            if b - a < 16:
                continue
            for bus, kind in ((sl, "long"), (ss, "short")):
                seg = bus[a:b].astype(np.float64)
                if np.abs(seg).max() < 1e-7:
                    continue
                w = reverb_mono(seg, kind)
                fade_out(w, 96)
                wet[:, a:b] += w
        return wet

    def mixdown(self):
        self.render_groups()
        d = self.duck_curve()
        dry = np.zeros((2, self.N))
        sl = np.zeros(self.N)
        ss = np.zeros(self.N)
        for name, st in self.stems.items():
            g = (1.0 - st.depth * d).astype(np.float64) if st.depth > 0 else 1.0
            x = st.dry.astype(np.float64)
            if name == "pads":
                x = self.pad_filter(x)
            dry += x * g
            sl += st.sl * g
            ss += st.ss * g
        out = dry + self.reverb(sl, ss)
        if self.energy:
            pts = sorted(self.energy)
            g = 10.0 ** (np.interp(np.arange(self.N) / SR, [p[0] for p in pts], [p[1] for p in pts]) / 20.0)
            out = out * g[None, :]
        return out

    def hits(self):
        best = {}
        for t, k in self.hits_raw:
            if t not in best or PRIO[k] > PRIO[best[t]]:
                best[t] = k
        return [{"t": t, "kind": best[t]} for t in sorted(best)]


# ═══════════════════════════════════════════════════════════════════════════
# 6. Material musical
# ═══════════════════════════════════════════════════════════════════════════
PROG = ["G", "D", "Em", "C"]
CH = {
    "G":  dict(bass=31, pcs=[7, 11, 2, 9], pad=[55, 59, 62, 67, 71, 74], stab=[55, 62, 67, 71, 74]),
    "D":  dict(bass=38, pcs=[2, 6, 9, 4], pad=[57, 62, 66, 69, 74, 78], stab=[57, 62, 66, 69, 74]),
    "Em": dict(bass=40, pcs=[4, 7, 11, 2], pad=[55, 59, 64, 67, 71, 76], stab=[55, 59, 64, 67, 71]),
    "C":  dict(bass=36, pcs=[0, 4, 7, 2], pad=[55, 60, 64, 67, 72, 76], stab=[55, 60, 64, 67, 72]),
}

# Gancho (slot, midi, duración en semicorcheas). A: sincopado, cantable. B: arpegio de corneta. Notas de sol pentatónico.
HOOK_A = [
    [(0, 79, 2), (3, 79, 1), (4, 76, 2), (7, 74, 1), (8, 76, 2), (11, 74, 1), (12, 71, 4)],
    [(0, 81, 2), (3, 81, 1), (4, 78, 2), (7, 76, 1), (8, 78, 2), (11, 76, 1), (12, 74, 4)],
    [(0, 79, 2), (3, 79, 1), (4, 76, 2), (7, 74, 1), (8, 71, 2), (11, 74, 1), (12, 76, 4)],
    [(0, 76, 2), (3, 76, 1), (4, 72, 2), (7, 74, 1), (8, 76, 2), (11, 79, 1), (12, 79, 2), (14, 76, 2)],
]
HOOK_B = [
    [(0, 86, 2), (2, 83, 2), (4, 79, 2), (6, 83, 2), (8, 86, 2), (10, 83, 2), (12, 79, 4)],
    [(0, 81, 2), (2, 78, 2), (4, 74, 2), (6, 78, 2), (8, 81, 2), (10, 78, 2), (12, 74, 4)],
    [(0, 83, 2), (2, 79, 2), (4, 76, 2), (6, 79, 2), (8, 83, 2), (10, 79, 2), (12, 76, 4)],
    [(0, 84, 2), (2, 79, 2), (4, 76, 2), (6, 79, 2), (8, 84, 2), (10, 79, 2), (12, 76, 2), (14, 79, 2)],
]
HOOK_END = [(0, 76, 2), (3, 76, 1), (4, 72, 2), (7, 74, 1), (8, 78, 2), (10, 81, 2)]     # C | D, para el final

BASS_SPARSE = [(0, 0, 3.4), (6, 0, 1.5), (8, 0, 3.2), (14, 12, 1.0)]
BASS_TUMBAO = [(0, 0, 2.6), (3, 12, 0.8), (6, 7, 1.7), (8, 0, 2.6), (11, 12, 0.8), (14, 7, 1.6)]
BASS_PUMP = [(2, 0, 1.4), (3, 0, 0.8), (6, 0, 1.4), (7, 12, 0.8), (10, 0, 1.4), (11, 7, 0.8), (14, 0, 1.4), (15, 12, 0.8)]
BASS_ROLL = [(0, 0, 1.0), (2, 12, 1.0), (3, 0, 0.8), (4, 0, 1.0), (6, 12, 1.0), (7, 7, 0.8),
             (8, 0, 1.0), (10, 12, 1.0), (11, 0, 0.8), (12, 0, 1.0), (14, 12, 1.0), (15, 7, 0.8)]
ARP_PAT = [0, 2, 4, 2, 1, 3, 5, 3, 2, 4, 6, 4, 3, 5, 7, 5]

# Patrón de S5: por compás, acorde por corchea y posiciones de golpe (en corcheas: 0 = pulso 1, 1 = «y» del 1...).
CUT_BARS = [
    dict(ch=["Em"] * 8, hits=[0, 3, 4]),
    dict(ch=["C"] * 8, hits=[0, 2, 4, 6]),
    dict(ch=["G"] * 8, hits=[0, 1, 3, 4, 6, 7]),
    dict(ch=["D"] * 8, hits=[0, 2, 3, 4, 5, 6, 7]),
    dict(ch=["Em"] * 4 + ["C"] * 4, hits=[0, 1, 2, 3, 4, 5, 6, 7]),
    dict(ch=["D"] * 8, hits=[0, 2], build=True),
]


def arp_notes(ch):
    pcs = CH[ch]["pcs"]
    return [n for n in range(64, 93) if n % 12 in pcs]


def bar_bass(song, tb, pattern, ch, vel=1.0, upto=16, ch2=None, split=8):
    for s, off, ln in pattern:
        if s >= upto:
            continue
        root = CH[ch2 if (ch2 and s >= split) else ch]["bass"]
        song.note("bass", tb + s * SLOT, root + off, ln * SLOT * 0.92, vel)


def hook_bar(song, tb, notes, inst, octv=0, vel=1.0, pan=0.0, upto=16):
    for slot, m, ln in notes:
        if slot >= upto:
            continue
        t = tb + slot * SLOT
        v = vel * (1.0 if slot % 4 == 0 else 0.86)
        mm = m + 12 * octv
        d = ln * SLOT * 0.95
        if inst == "marimba":
            song.marimba(t, mm, v, pan, ring=d + 0.16)
        elif inst == "steel":
            song.steel(t, mm, v, pan, ring=d + 0.45)
        else:
            song.note(inst, t, mm, d, v, pan)


def arp_bar(song, tb, ch, vel=1.0, upto=16, start=0):
    notes = arp_notes(ch)
    for s in range(start, min(16, upto)):
        m = notes[min(ARP_PAT[s], len(notes) - 1)]
        song.note("arp", tb + s * SLOT, m, SLOT * 0.85, vel * (1.0 if s % 4 == 0 else 0.72), pan=-0.55 if s % 2 == 0 else 0.55)


def hats16(song, tb, vel=1.0, open_off=True, upto=16):
    acc = [1.0, 0.42, 0.72, 0.48]
    for s in range(min(16, upto)):
        if open_off and s % 4 == 2:
            continue
        song.hat(tb + s * SLOT, vel * acc[s % 4] * (1.0 + 0.08 * song.rng.uniform(-1, 1)), pan=0.25 if s % 2 else -0.25)
    if open_off:
        for b in range(4):
            if b * 4 + 2 < upto:
                song.hat(tb + b * BEAT + 2 * SLOT, vel * 0.62, open_=True, pan=0.15)


# ═══════════════════════════════════════════════════════════════════════════
# 7. Tramos
# ═══════════════════════════════════════════════════════════════════════════
def sec_intro(song, t0, nb, imp_bars, sizes, end_gap):
    """S1 / V1: escaso y misterioso, golpes graves en los compases indicados y subida final."""
    T = nb * BAR
    tend = t0 + T
    song.pad_lp += [(t0, 240.0), (tend - end_gap, 1500.0)]
    song.pad(t0, T - end_gap, [40, 47, 52, 55, 59], gain=0.40, att=1.4 if nb > 2 else 0.8, rel=0.15, sl=0.5)
    song.sub_tone(t0, 28, T - end_gap, 0.07)
    for k, sz in zip(imp_bars, sizes):
        tk = t0 + k * BAR
        song.impact(tk, sz, "impact", crash=(sz > 0.9))
        if tk - 1.0 > t0:
            song.swell(tk, 0.9, 0.45, end_gap=0.05)
    # campanas de sonar con eco (tres repeticiones que se apagan)
    pings = [(0.8, 76), (2.4, 71), (4.0, 79)] if nb > 2 else [(0.8, 71)]
    for tp, m in pings:
        for r, a in enumerate((1.0, 0.5, 0.26, 0.13)):
            if t0 + tp + r * 0.6 < tend - end_gap - 0.1:
                song.bell(t0 + tp + r * 0.6, m, 0.8 * a, pan=(-0.5 if r % 2 == 0 else 0.5), sl=0.6)
    # tics de reloj militar
    step = BEAT
    k = 0
    tcur = t0 + (BEAT if nb > 2 else 0.8)
    while tcur < tend - end_gap - 0.3:
        u = (tcur - t0) / T
        if tcur > tend - BAR - 0.05:
            break
        song.perc("wood", tcur, 0.18 + 0.4 * u, pan=(-0.3 if k % 2 == 0 else 0.3))
        k += 1
        tcur += step if u < 0.5 else step / 2
    # subida del último compás
    rs = tend - BAR if nb > 2 else t0 + BAR
    song.riser(rs, tend - end_gap, 1.0)
    segs = [(rs + 0.4, rs + 0.8, 0.2), (rs + 0.8, rs + 1.2, 0.1), (rs + 1.2, tend - end_gap, 0.05)]
    song.roll(segs, 0.15, 0.85)


def sec_coop(song, t0):
    """S2: cooperativo, 8 compases, ritmo tropical-electrónico juguetón que sube de energía."""
    prog = PROG * 2
    song.pad_lp += [(t0, 1300.0), (t0 + 4 * BAR, 1800.0), (t0 + 8 * BAR - 0.05, 7500.0)]
    song.impact(t0, 1.0, "drop", crash=True)
    for j in range(8):
        tb = t0 + j * BAR
        ch = prog[j]
        ks = [0, 4, 8, 12] if j >= 6 else [0, 8]
        for s in ks:
            song.kick(tb + s * SLOT, 1.0 if s == 0 else 0.92)
        for s in (0, 8):                                  # un acento cada 2 pulsos: pensado para los cortes de S2
            song.hit(tb + s * SLOT, "drop" if (j == 0 and s == 0) else "cut")
        if j >= 1 and j != 7:
            for s in (4, 12):
                song.clap(tb + s * SLOT, 0.9)
        if j >= 4 and j != 7:
            for s in (4, 12):
                song.snare(tb + s * SLOT, 0.6)
        # hi-hats y shaker
        for s in (2, 6, 10, 14):
            song.hat(tb + s * SLOT, 0.55, open_=(j >= 2 and s in (6, 14)), pan=0.2)
        if j >= 4:
            for s in range(16):
                if s % 4 != 2:
                    song.hat(tb + s * SLOT, 0.42 * (1.0 if s % 2 == 0 else 0.6), pan=-0.2)
        if j >= 2:
            for s in range(16):
                song.shaker(tb + s * SLOT, 0.55 + 0.35 * (s % 4 == 0), pan=0.3 if s % 2 else -0.3)
            cl = [0, 6, 12] if j % 2 == 0 else [4, 8]
            for s in cl:
                song.perc("clave", tb + s * SLOT, 0.8, pan=-0.35)
        if j >= 4:
            for s in (0, 3, 6, 8, 11, 14):
                song.perc("cowbell", tb + s * SLOT, 0.55 + 0.2 * (s == 0), pan=0.4)
            for i, s in enumerate((3, 6, 7, 10, 14, 15)):
                song.perc("conga_hi" if i % 2 == 0 else "conga_lo", tb + s * SLOT, 0.6, pan=-0.45 if i % 2 == 0 else 0.45)
        # bajo
        bar_bass(song, tb, BASS_SPARSE if j < 2 else BASS_TUMBAO, ch, 1.0)
        # melodía
        hb = HOOK_A[j % 4]
        hook_bar(song, tb, hb, "marimba", 0, 0.95 if j < 4 else 0.85, pan=-0.1)
        if j >= 4:
            hook_bar(song, tb, hb, "steel", 1, 0.70, pan=0.25)
        # pads y arpegios
        if j >= 4:
            song.pad(tb, BAR + 0.02, CH[ch]["pad"], gain=0.7 + 0.1 * (j - 4), sl=0.35)
        if j >= 5:
            arp_bar(song, tb, ch, 0.5 if j == 5 else 0.8, start=0 if j > 5 else 8)
    # crash de mitad y subida final
    song.crash(t0 + 4 * BAR, 0.5)
    song.hit(t0 + 4 * BAR, "cut")
    song.riser(t0 + 6 * BAR, t0 + 8 * BAR, 0.9)
    tb = t0 + 7 * BAR
    song.roll([(tb + 0.4, tb + 0.8, 0.2), (tb + 0.8, tb + 1.2, 0.1), (tb + 1.2, tb + 1.6, 0.05)], 0.3, 1.0)


def sec_tension(song, t0):
    """S3: rayado de disco, silencio con tensión y cuenta 3-2-1 (20,0 / 20,8 / 21,6)."""
    tend = t0 + 2 * BAR
    song.hit(t0, "scratch")
    song.put("fx", make_scratch(rng_for("rayado")), t0, gain=0.55, sl=0.2)
    song.pad_lp += [(t0, 280.0), (tend - GAP, 3200.0)]
    song.pad(t0 + 0.7, tend - GAP - t0 - 0.7, [40, 47, 52, 55, 59, 64], gain=0.5, att=1.2, rel=0.15, sl=0.6)
    song.sub_tone(t0 + 0.8, 28, tend - GAP - t0 - 0.8, 0.07)
    for i, (tc, m) in enumerate(((t0 + 0.8, 76), (t0 + 1.6, 79), (t0 + 2.4, 83))):
        song.hit(tc, "count")
        song.bell(tc, m, 0.95, pan=0.0, sl=0.45)
        song.perc("tom", tc, 0.8)
        song.duck(tc, 0.6, 0.2)
        song.perc("wood", tc + BEAT, 0.3, pan=0.3 * (-1) ** i)
    song.riser(t0 + 0.8, tend - GAP, 1.0)
    song.roll([(t0 + 2.4, t0 + 2.8, 0.1), (t0 + 2.8, tend - GAP, 0.05)], 0.15, 0.9)


def sec_drop(song, t0, nb):
    """S4 / V2: la caída. Bombo en cada pulso, palmas, hi-hats dobles, golpe grave por compás y el gancho en tres fases."""
    tend = t0 + nb * BAR
    song.pad_lp += [(t0, 6000.0), (tend - GAP, 6000.0)]
    for j in range(nb):
        tb = t0 + j * BAR
        ch = PROG[j % 4]
        ph = 0 if j < 4 else (1 if j < 8 else 2)
        last = j == nb - 1
        grp_end = j % 4 == 3
        ups = 8 if last else 16                       # el último compás: relleno en la segunda mitad
        # bombo
        for b in range(4):
            if last and b >= 2:
                break
            song.kick(tb + b * BEAT, 1.0 if b == 0 else 0.92)
        # golpe de compás
        if j == 0:
            song.impact(tb, 1.0, "drop", crash=True)
        elif j % 4 == 0:
            song.impact(tb, 0.8, "impact", crash=True)
        else:
            song.impact(tb, 0.42, "accent")
        # caja y palmas
        for s in (4, 12):
            if s < ups:
                song.clap(tb + s * SLOT, 1.0)
                song.snare(tb + s * SLOT, 0.75)
        # hi-hats dobles
        hats16(song, tb, 1.0 if ph else 0.85, True, upto=ups)
        if ph >= 1 and j % 2 == 1 and not last:
            for s in (14, 15):
                song.hat(tb + (s + 0.5) * SLOT, 0.45, pan=0.2)
        if ph >= 1:
            for s in (0, 3, 6, 8, 11, 14):
                if s < ups:
                    song.perc("cowbell", tb + s * SLOT, 0.5, pan=0.4)
        if ph == 1:
            for i, s in enumerate((3, 7, 10, 15)):
                if s < ups:
                    song.perc("conga_hi" if i % 2 == 0 else "conga_lo", tb + s * SLOT, 0.55, pan=-0.45 if i % 2 == 0 else 0.45)
        # bajo
        pat = BASS_ROLL if ph == 2 else BASS_PUMP
        bar_bass(song, tb, pat, ch, 1.0, upto=ups)
        # pads
        song.pad(tb, BAR + 0.02 if not last else 0.8, CH[ch]["pad"], gain=0.55 + 0.10 * ph, sl=0.3)
        # melodía
        upto = 8 if last else 16
        if ph == 0:
            hook_bar(song, tb, HOOK_A[j % 4], "steel", 0, 0.9, pan=0.15, upto=upto)
            hook_bar(song, tb, HOOK_A[j % 4], "marimba", 0, 0.75, pan=-0.2, upto=upto)
            if j >= 2:
                hook_bar(song, tb, HOOK_A[j % 4], "lead", 0, 0.55, upto=upto)
            if j >= 2:
                arp_bar(song, tb, ch, 0.45, upto=ups)
        elif ph == 1:
            hook_bar(song, tb, HOOK_B[j % 4], "lead", 0, 0.9, upto=upto)
            hook_bar(song, tb, HOOK_B[j % 4], "steel", 0, 0.55, pan=0.2, upto=upto)
            hook_bar(song, tb, HOOK_B[j % 4], "marimba", 0, 0.5, pan=-0.2, upto=upto)
            arp_bar(song, tb, ch, 0.7, upto=ups)
        else:
            hook_bar(song, tb, HOOK_A[j % 4], "lead", 0, 1.0, upto=upto)
            hook_bar(song, tb, HOOK_A[j % 4], "steel", 1, 0.8, pan=0.2, upto=upto)
            hook_bar(song, tb, HOOK_A[j % 4], "marimba", 0, 0.6, pan=-0.2, upto=upto)
            hook_bar(song, tb, HOOK_A[j % 4], "brass", -1, 0.55, upto=upto)
            arp_bar(song, tb, ch, 0.85, upto=ups)
        # subidas al final de cada grupo de 4 compases
        if grp_end and not last:
            song.riser(tb + 2 * BEAT, tb + BAR, 0.8, f_lo=400.0, f_hi=10000.0, whistle=False)
            song.roll([(tb + 3 * BEAT, tb + BAR, 0.1)], 0.3, 0.8)
    # relleno del último compás: redoble a 16avos y luego 32avos, hueco seco
    tb = tend - BAR
    song.riser(tb + 2 * BEAT, tend - GAP, 1.0)
    song.roll([(tb + 2 * BEAT, tb + 3 * BEAT, 0.1), (tb + 3 * BEAT, tend - GAP, 0.05)], 0.45, 1.0)


def stab(song, t, ch, kind, vel=1.0):
    """Golpe seco de S5: acorde corto de sierras + marimba + sub, sin colas."""
    dur = {"imp": 0.24, "on": 0.14, "off": 0.10}[kind]
    for m in CH[ch]["stab"]:
        song.note("stab", t, m + (12 if kind == "off" else 0), dur, vel)
    for m in CH[ch]["stab"][1:4]:
        song.marimba(t, m + 12, 0.8 * vel, ring=dur + 0.06)
    song.note("bass", t, CH[ch]["bass"] + 12, dur * 0.9, vel)
    if kind == "off":
        song.clap(t, 0.85 * vel)
        song.hat(t, 0.5)
    else:
        song.kick(t, 1.0)
        if kind == "imp":
            song.boom(t, 1.0)
            song.crash(t, 0.7)
            song.clap(t, 0.9)
            song.duck(t, 1.0, 0.3)
        else:
            song.snare(t, 0.5)


def sec_cuts(song, t0, bars):
    """S5 / V3: patrón entrecortado de golpes secos; `bars` indica qué compases del patrón se usan."""
    for i, k in enumerate(bars):
        pat = CUT_BARS[k]
        tb = t0 + i * BAR
        for e in pat["hits"]:
            t = tb + e * 0.2
            if e == 0:
                kind = "imp" if (i == 0 or k == 4) else "on"
            else:
                kind = "on" if e % 2 == 0 else "off"
            song.dry_mode = True
            stab(song, t, pat["ch"][e], kind)
            song.dry_mode = False
            song.hit(t, "impact" if kind == "imp" else "cut")
        if pat.get("build"):
            tend = tb + BAR
            song.riser(tb + 2 * BEAT, tend - GAP, 1.0, f_lo=500.0, f_hi=11000.0)
            song.roll([(tb + 2 * BEAT, tb + 3 * BEAT, 0.1), (tb + 3 * BEAT, tend - GAP, 0.05)], 0.4, 1.0)


def sting(song, t0):
    """Golpe del logo: acorde de sol con novena, sub, platillo y destellos; cola hasta el final de la pista."""
    song.hit(t0, "sting")
    song.kick(t0, 1.0, deep=True)
    song.boom(t0, 1.0)
    song.crash(t0, 1.0)
    song.duck(t0, 1.0, 0.45)
    for k, m in enumerate([55, 62, 67, 71, 74, 79, 83]):
        song.marimba(t0 + 0.004 * k, m, 0.85, pan=(k - 3) * 0.13)
    for m, p in ((67, -0.3), (74, 0.3), (79, 0.0)):
        song.steel(t0, m, 0.9, pan=p)
    for m in (55, 62, 67, 71, 74):
        song.note("brass", t0, m, 1.0, 0.9)
    song.pad_lp += [(t0, 8000.0), (t0 + 1.2, 3500.0), (song.duration, 700.0)]
    song.pad(t0, song.duration - t0 - 0.05, [55, 59, 62, 67, 71, 74, 81], gain=1.0, att=0.02, rel=0.5, decay=1.7, sl=0.55)
    for i, m in enumerate((91, 95, 98, 103)):
        song.glock(t0 + 0.03 + 0.09 * i, m, 0.75, pan=(-0.4, 0.4, -0.2, 0.2)[i])
    song.sub_tone(t0, 31, 2.6, 1.0, tau=1.1)


def sec_final(song, t0, chorus_bars, dead_beat):
    """S6: estribillo (G | Em | C-D), pulso muerto con swell inverso y sting."""
    tstop = t0 + chorus_bars * BAR - (BEAT if dead_beat else 0.0)     # 55,6
    chords = ["G", "Em", "C"]
    song.pad_lp += [(t0, 8500.0), (tstop, 8500.0)]
    for j in range(chorus_bars):
        tb = t0 + j * BAR
        ch = chords[j]
        last = j == chorus_bars - 1
        ups = 12 if last else 16                        # el último compás muere en el pulso 3 (55,6)
        if j == 0:
            song.impact(tb, 1.0, "drop", crash=True)
        else:
            song.impact(tb, 0.7, "accent", crash=True)
        for b in range(4):
            if b * 4 < ups:
                song.kick(tb + b * BEAT, 1.0 if b == 0 else 0.92)
        for s in (4, 12):
            if s < ups:
                song.clap(tb + s * SLOT, 1.0)
                song.snare(tb + s * SLOT, 0.8)
        hats16(song, tb, 1.05, True, upto=ups)
        for s in (0, 3, 6, 8, 11, 14):
            if s < ups:
                song.perc("cowbell", tb + s * SLOT, 0.5, pan=0.4)
        if last:
            bar_bass(song, tb, BASS_ROLL, "C", 1.0, upto=ups, ch2="D", split=8)
            song.pad(tb, 0.8, CH["C"]["pad"], gain=1.0, sl=0.3)
            song.pad(tb + 0.8, BEAT, CH["D"]["pad"], gain=1.0, sl=0.3)
            arp_bar(song, tb, "C", 0.9, upto=8)
            arp_bar(song, tb, "D", 0.9, upto=ups, start=8)
            end_lead = HOOK_END
        else:
            bar_bass(song, tb, BASS_ROLL, ch, 1.0)
            song.pad(tb, BAR + 0.02, CH[ch]["pad"], gain=1.05, sl=0.3)
            arp_bar(song, tb, ch, 0.9)
            end_lead = HOOK_A[0 if ch == "G" else 2]
        hook_bar(song, tb, end_lead, "lead", 0, 1.0)
        hook_bar(song, tb, end_lead, "steel", 1, 0.85, pan=0.2)
        hook_bar(song, tb, end_lead, "marimba", 0, 0.65, pan=-0.2)
        hook_bar(song, tb, end_lead, "brass", -1, 0.65)
    if dead_beat:
        song.swell(t0 + chorus_bars * BAR, BEAT + 0.6, 0.9)
    return tstop


# ═══════════════════════════════════════════════════════════════════════════
# 8. Composición de cada versión
# ═══════════════════════════════════════════════════════════════════════════
SECTIONS_60 = [("S1", 0.0, 6.4, "Gancho"), ("S2", 6.4, 19.2, "Cooperativo"), ("S3", 19.2, 22.4, "Corte"),
               ("S4", 22.4, 41.6, "Carrera"), ("S5", 41.6, 51.2, "Características"), ("S6", 51.2, 60.0, "Final")]
SECTIONS_30 = [("V1", 0.0, 3.2, "Gancho"), ("V2", 3.2, 19.2, "Carrera"), ("V3", 19.2, 25.6, "Características"),
               ("V4", 25.6, 30.0, "Logo")]


def compose_60():
    cuts = [6.4 - GAP, 6.4, 19.2, 22.4 - GAP, 22.4, 41.6 - GAP, 41.6, 51.2 - GAP, 51.2, 55.6, 56.0]
    s = Song("trailer_60", 60.0, cuts)
    s.energy = [(0.0, -6.0), (6.32, -4.5), (6.4, -5.0), (19.15, -2.5), (19.2, -4.5), (22.3, -4.5), (22.4, 0.0),
                (41.52, 0.0), (41.6, -1.5), (51.12, -1.0), (51.2, 0.5), (55.6, 0.5), (56.0, 0.0), (60.0, 0.0)]
    sec_intro(s, 0.0, 4, [1, 2, 3], [1.0, 1.25, 1.5], GAP)
    sec_coop(s, 6.4)
    sec_tension(s, 19.2)
    sec_drop(s, 22.4, 12)
    sec_cuts(s, 41.6, [0, 1, 2, 3, 4, 5])
    sec_final(s, 51.2, 3, True)
    sting(s, 56.0)
    return s, SECTIONS_60


def compose_30():
    cuts = [3.2 - GAP, 3.2, 19.2 - GAP, 19.2, 25.6 - GAP, 25.6]
    s = Song("trailer_30", 30.0, cuts)
    s.energy = [(0.0, -3.0), (3.12, -2.0), (3.2, 0.0), (19.15, 0.0), (19.2, -1.5), (25.52, -1.0), (25.6, 0.0), (30.0, 0.0)]
    sec_intro(s, 0.0, 2, [0, 1], [1.6, 1.3], GAP)
    sec_drop(s, 3.2, 10)
    sec_cuts(s, 19.2, [0, 1, 4, 5])
    sting(s, 25.6)
    return s, SECTIONS_30


# ═══════════════════════════════════════════════════════════════════════════
# 9. Masterización y medidas
# ═══════════════════════════════════════════════════════════════════════════
_K1B = np.array([1.53512485958697, -2.69169618940638, 1.19839281085285])
_K1A = np.array([1.0, -1.69065929318241, 0.73248077421585])
_K2B = np.array([1.0, -2.0, 1.0])
_K2A = np.array([1.0, -1.99004745483398, 0.99007225036621])


def _freqz(b, a, w):
    z = np.exp(-1j * w)
    return (b[0] + b[1] * z + b[2] * z ** 2) / (a[0] + a[1] * z + a[2] * z ** 2)


def k_weight(x):
    """Ponderación K de la BS.1770 (coeficientes de 48 kHz) aplicada por FFT."""
    n = x.shape[1]
    nf = next_fast(n + SR)
    w = 2.0 * np.pi * np.fft.rfftfreq(nf, 1.0 / SR) / SR
    H = _freqz(_K1B, _K1A, w) * _freqz(_K2B, _K2A, w)
    return np.fft.irfft(np.fft.rfft(x, nf, axis=1) * H, nf, axis=1)[:, :n]


def lufs_integrated(x):
    y = k_weight(x)
    blk, hop = int(0.4 * SR), int(0.1 * SR)
    cs = np.concatenate([np.zeros((2, 1)), np.cumsum(y ** 2, axis=1)], axis=1)
    idx = np.arange(0, x.shape[1] - blk + 1, hop)
    ms = ((cs[:, idx + blk] - cs[:, idx]) / blk).sum(axis=0)
    l = -0.691 + 10.0 * np.log10(ms + 1e-20)
    m1 = l > -70.0
    if not m1.any():
        return -70.0
    rel = -0.691 + 10.0 * np.log10(ms[m1].mean()) - 10.0
    m2 = m1 & (l > rel)
    return float(-0.691 + 10.0 * np.log10(ms[m2].mean()))


def lufs_plain(x):
    """Sonoridad sin puertas de un trozo (para informar por tramos)."""
    y = k_weight(x)
    return float(-0.691 + 10.0 * np.log10((y ** 2).mean(axis=1).sum() + 1e-20))


def true_peak_db(x):
    pk = 0.0
    for c in range(x.shape[0]):
        n = x.shape[1]
        X = np.fft.rfft(x[c])
        Y = np.zeros(4 * n // 2 + 1, complex)
        Y[:len(X)] = X
        pk = max(pk, float(np.abs(np.fft.irfft(Y, 4 * n)).max() * 4.0))
    return 20.0 * np.log10(pk + 1e-12)


def bus_comp(x, thr_db=-15.0, ratio=2.0, att=0.010, rel=0.15, knee=6.0):
    """Compresor de bus ligero (detector RMS de 5 ms, ganancia interpolada)."""
    hop = 240
    nb = x.shape[1] // hop
    seg = x[:, :nb * hop].reshape(2, nb, hop)
    lvl = 10.0 * np.log10((seg ** 2).mean(axis=(0, 2)) + 1e-12)
    over = lvl - thr_db
    gr = np.where(over < -knee / 2, 0.0,
                  np.where(over > knee / 2, (1.0 / ratio - 1.0) * over,
                           (1.0 / ratio - 1.0) * (over + knee / 2) ** 2 / (2.0 * knee)))
    ca = np.exp(-1.0 / (att * SR / hop))
    cr = np.exp(-1.0 / (rel * SR / hop))
    g = np.empty(nb)
    s = 0.0
    for i in range(nb):
        c = ca if gr[i] < s else cr
        s = c * s + (1.0 - c) * gr[i]
        g[i] = s
    centers = (np.arange(nb) + 0.5) * hop
    gain = 10.0 ** (np.interp(np.arange(x.shape[1]), centers, g) / 20.0)
    return x * gain[None, :]


def softclip(x, thr=0.55):
    a = np.abs(x)
    return np.where(a <= thr, x, np.sign(x) * (thr + (1.0 - thr) * np.tanh((a - thr) / (1.0 - thr))))


def _sliding_min_fwd(r, w):
    """m[n] = min(r[n : n + w])."""
    m = r.copy()
    span = 1
    while span * 2 <= w:
        m[:-span] = np.minimum(m[:-span], m[span:])
        span *= 2
    if span < w:
        sh = w - span
        m[:-sh] = np.minimum(m[:-sh], m[sh:])
    return m


def limiter(x, ceiling_db=CEILING_DB, look=144, rel=0.07):
    """Limitador de anticipación: mínimo deslizante + media móvil + liberación exponencial."""
    c = 10.0 ** (ceiling_db / 20.0)
    peak = np.abs(x).max(axis=0)
    r = np.minimum(1.0, c / np.maximum(peak, 1e-9))
    gm = _sliding_min_fwd(r, look)
    cs = np.concatenate([[0.0], np.cumsum(gm)])
    avg = np.empty_like(gm)
    avg[look:] = (cs[look + 1:] - cs[1:len(gm) - look + 1]) / look
    avg[:look] = gm[:look]
    gr = 1.0 - avg
    dec = np.exp(-1.0 / (rel * SR))
    B = 2048
    out = np.empty_like(gr)
    carry = 0.0
    w = dec ** np.arange(B)
    for i in range(0, len(gr), B):
        seg = gr[i:i + B].copy()
        seg[0] = max(seg[0], carry * dec)
        ww = w[:len(seg)]
        y = ww * np.maximum.accumulate(seg / ww)
        out[i:i + B] = y
        carry = y[-1]
    return x * (1.0 - out)[None, :]


def master(mix, label=""):
    """Cadena: filtro de graves -> ganancia -> compresor de bus -> saturador suave -> limitador -> calibración."""
    x = mix - mix.mean(axis=1, keepdims=True)
    x = hp_causal(x, 30.0)
    g_db = TARGET_LUFS - lufs_integrated(x)
    ceil = CEILING_DB
    y = x
    for it in range(10):
        y = limiter(softclip(bus_comp(x * 10.0 ** (g_db / 20.0)), 0.62), ceil)
        L = lufs_integrated(y)
        tp = true_peak_db(y)
        err = TARGET_LUFS - L
        print("    [%s] iter %d: %.2f LUFS, TP %.2f dBTP (ganancia %.2f dB, techo %.2f)" % (label, it, L, tp, g_db, ceil))
        if tp > TP_MAX_DB:
            ceil -= (tp - TP_MAX_DB) + 0.05
            continue
        if abs(err) < 0.06:
            break
        g_db += err
    return y


def write_wav(path, x):
    pcm = np.clip(np.round(x.T * 32767.0), -32768, 32767).astype("<i2")
    with wave.open(str(path), "wb") as w:
        w.setnchannels(2)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes(pcm.tobytes())


def read_wav(path):
    with wave.open(str(path), "rb") as w:
        ch, sw, sr, n = w.getnchannels(), w.getsampwidth(), w.getframerate(), w.getnframes()
        raw = w.readframes(n)
    a = np.frombuffer(raw, dtype="<i2").astype(np.float64) / 32768.0
    return a.reshape(-1, ch).T, sr, sw


def make_beatmap(song, sections, duration):
    beats = [round(k * BEAT, 4) for k in range(int(round(duration / BEAT)))]
    downs = [round(k * BAR, 4) for k in range(int(np.ceil(duration / BAR - 1e-9)))]
    return {
        "bpm": BPM,
        "duration": duration,
        "sample_rate": SR,
        "key": "sol mayor (mi menor en los tramos misteriosos)",
        "beats": beats,
        "downbeats": downs,
        "sections": [{"name": n, "start": a, "end": b, "label": lb} for n, a, b, lb in sections],
        "hits": song.hits(),
        "kinds": {
            "impact": "golpe grave grande (sub + platillo)",
            "drop": "inicio de tramo a toda energía",
            "accent": "golpe de compás dentro de una caída",
            "cut": "golpe seco o acento cada 2 pulsos, pensado para cortes y rótulos",
            "count": "pitido de la cuenta 3-2-1",
            "scratch": "rayado de disco (la música se corta en seco)",
            "sting": "golpe del logo",
        },
    }


def build_track(kind):
    t0 = time.time()
    song, sections = compose_60() if kind == "60" else compose_30()
    print("  [%s] composición: %.1f s" % (kind, time.time() - t0))
    t1 = time.time()
    mix = song.mixdown()
    print("  [%s] mezcla: %.1f s" % (kind, time.time() - t1))
    t2 = time.time()
    y = master(mix, kind)
    y = y.copy()
    fade_out(y, smp(0.6))                                   # cierra la cola exactamente en cero
    print("  [%s] masterización: %.1f s" % (kind, time.time() - t2))
    dur = song.duration
    write_wav(OUT_MUSIC / ("trailer_%s.wav" % kind), y)
    bm = make_beatmap(song, sections, dur)
    (OUT_MUSIC / ("beatmap_%s.json" % kind)).write_text(json.dumps(bm, ensure_ascii=False, indent=1), encoding="utf-8")
    return y, bm


# ═══════════════════════════════════════════════════════════════════════════
# 10. Efectos de sonido
# ═══════════════════════════════════════════════════════════════════════════
def finish_sfx(y, peak_db=-3.0, fade_in_ms=0.4, fade_out_ms=8.0):
    y = np.asarray(y, np.float64)
    if y.ndim == 1:
        y = np.stack([y, y])
    y = y - y.mean(axis=1, keepdims=True)
    y = hp_causal(y, 25.0, 0.25)
    fade_in(y, max(2, int(fade_in_ms * SR / 1000)))
    fade_out(y, int(fade_out_ms * SR / 1000))
    return y * (10.0 ** (peak_db / 20.0) / (np.abs(y).max() + 1e-12))


def widen(mono, rng, amount=0.25):
    """Estéreo barato: un retardo Haas corto y muy pequeño distinto en cada canal."""
    d = int(rng.integers(20, 90))
    a = mono
    b = np.concatenate([np.zeros(d), mono[:-d]])
    return np.stack([a * (1 - amount) + b * amount, b * (1 - amount) + a * amount])


def sfx_whoosh(name, dur, f0, f1, f2, sigma=1.1):
    rng = rng_for(name)
    n = smp(dur)
    t = tt(n)
    u = t / dur
    fc = np.where(u < 0.5, f0 * (f1 / f0) ** (u / 0.5), f1 * (f2 / f1) ** ((u - 0.5) / 0.5))
    stereo = []
    for c in range(2):
        nz = rng.standard_normal(n)
        y = stft_apply(nz, lambda f, tc: bp_mask(f, np.interp(tc, t, fc), sigma))
        stereo.append(y / (y.std() + 1e-9))
    amp = np.sin(np.pi * u) ** 1.7
    p = 0.5 - 0.45 * np.cos(np.pi * u)                        # de izquierda a derecha
    a = p * np.pi / 2.0
    y = np.stack([stereo[0] * amp * np.cos(a) * 1.2, stereo[1] * amp * np.sin(a) * 1.2])
    return finish_sfx(y, -4.0, 2.0, 25.0)


def sfx_impact_big():
    b = bank()
    rng = rng_for("impact_big")
    dur = 3.2
    n = smp(dur)
    x = np.zeros(n)
    x[:len(b.boom_big)] += b.boom_big * 0.95
    x[:len(b.crash)] += b.crash * 0.42
    x[:smp(0.05)] += rng.standard_normal(smp(0.05)) * 0.3
    wet = reverb_mono(x, "long")
    y = np.stack([x, x]) * 0.8 + wet * 0.55
    return finish_sfx(y, -2.5, 0.2, 200.0)


def sfx_impact_small():
    b = bank()
    dur = 0.7
    n = smp(dur)
    x = np.zeros(n)
    x[:len(b.boom_small)] += b.boom_small
    x[:len(b.snare[0])] += b.snare[0] * 0.25
    return finish_sfx(widen(x, rng_for("impact_small"), 0.15), -3.0, 0.2, 30.0)


def sfx_riser_2s():
    y = np.stack([make_riser(2.0, "riser_2s_L", 220.0, 9500.0, 1.35), make_riser(2.0, "riser_2s_R", 240.0, 9500.0, 1.35)])
    return finish_sfx(y, -4.0, 2.0, 3.0)


def sfx_record_scratch():
    x = make_scratch(rng_for("record_scratch"), 0.9)
    wet = reverb_mono(x, "short")
    y = np.stack([x, x]) * 0.9 + wet * 0.25
    return finish_sfx(y, -3.0, 0.2, 25.0)


def sfx_boing():
    rng = rng_for("boing")
    n = smp(0.85)
    t = tt(n)
    base = 240.0 * (1.0 + 0.85 * np.exp(-t / 0.33))
    f = base * (1.0 + 0.32 * np.exp(-t / 0.30) * np.sin(2 * np.pi * 12.0 * t))
    ph = 2.0 * np.pi * np.cumsum(f) / SR
    y = np.sin(ph) + 0.35 * np.sin(2 * ph) + 0.12 * np.sin(3.7 * ph)
    y = y * (1.0 - np.exp(-t / 0.003)) * np.exp(-t / 0.30)
    y += band_noise(rng, n, 1500, 5000, 2) * np.exp(-t / 0.004) * 0.15
    return finish_sfx(widen(y, rng, 0.2), -4.0, 0.3, 60.0)


def sfx_splash():
    rng = rng_for("splash")
    n = smp(1.0)
    t = tt(n)
    fc = 3500.0 * (700.0 / 3500.0) ** np.minimum(1.0, t / 0.6)
    body = stft_apply(rng.standard_normal(n), lambda f, tc: bp_mask(f, np.interp(tc, t, fc), 1.4))
    body = body / body.std() * (1.0 - np.exp(-t / 0.004)) * (0.6 * np.exp(-t / 0.10) + 0.4 * np.exp(-t / 0.32))
    thump = np.sin(2 * np.pi * np.cumsum(55.0 + 90.0 * np.exp(-t / 0.05)) / SR) * np.exp(-t / 0.13) * 0.9
    L = body * 0.9 + thump
    R = body * 0.9 + thump
    bub = np.zeros((2, n))
    for _ in range(16):
        t0 = rng.uniform(0.06, 0.75)
        d = rng.uniform(0.03, 0.09)
        k = smp(d)
        tv = tt(k)
        f0 = rng.uniform(500.0, 1500.0)
        b = np.sin(2 * np.pi * np.cumsum(f0 * (1.0 + 1.8 * tv / d)) / SR) * np.exp(-tv / (d * 0.4)) * rng.uniform(0.08, 0.22)
        i = smp(t0)
        p = rng.uniform(-1.0, 1.0)
        a = (p + 1.0) * np.pi / 4.0
        bub[0, i:i + k] += b[:n - i] * np.cos(a)
        bub[1, i:i + k] += b[:n - i] * np.sin(a)
    y = np.stack([L, R]) + bub
    return finish_sfx(y, -3.0, 0.4, 120.0)


def sfx_explosion_toy():
    b = bank()
    rng = rng_for("explosion_toy")
    n = smp(1.2)
    t = tt(n)
    fc = 7000.0 * (250.0 / 7000.0) ** np.minimum(1.0, t / 0.7)
    nz = stft_apply(rng.standard_normal(n), lambda f, tc: 1.0 / np.sqrt(1.0 + (f[None, :] / np.interp(tc, t, fc)[:, None]) ** 4))
    nz = nz / nz.std() * (1.0 - np.exp(-t / 0.002)) * (0.5 * np.exp(-t / 0.08) + 0.5 * np.exp(-t / 0.28))
    x = np.zeros(n)
    x[:len(b.boom_small)] += b.boom_small * 0.9
    wob = np.sin(2 * np.pi * np.cumsum(500.0 + 700.0 * np.sin(np.pi * np.minimum(t, 0.3) / 0.3)) / SR) * np.exp(-t / 0.10) * 0.25
    y = nz * 0.9 + x + wob
    for m, dly in ((100, 0.06), (104, 0.11), (108, 0.17), (96, 0.24)):
        g = make_glock(m, 0.5) * 0.18
        i = smp(dly)
        y[i:i + len(g)] += g[:n - i]
    return finish_sfx(widen(y, rng, 0.3), -3.0, 0.3, 90.0)


def sfx_gull_screech():
    rng = rng_for("gull")
    n = smp(1.0)
    y = np.zeros(n)
    for t0, dur, f_hi, f_lo in ((0.0, 0.40, 2700.0, 1500.0), (0.44, 0.34, 2900.0, 1750.0)):
        k = smp(dur)
        t = tt(k)
        u = t / dur
        f = f_lo * 0.72 + (f_hi - f_lo * 0.72) * np.minimum(1.0, u / 0.16) ** 0.6
        f = np.where(u > 0.16, f_hi - (f_hi - f_lo) * (np.maximum(u - 0.16, 0.0) / 0.84) ** 0.8, f)
        f = f * (1.0 + 0.035 * np.sin(2 * np.pi * 38.0 * t))
        dt = f / SR
        s = osc_saw(np.cumsum(dt) % 1.0, dt)
        s = fft_filter(s, hp=900, lp=6500, order=2)
        breath = band_noise(rng, k, 2500, 6500, 2) * 0.22
        env = np.minimum(1.0, t / 0.02) * np.exp(-np.maximum(t - dur * 0.55, 0.0) / (dur * 0.2))
        v = (s / (s.std() + 1e-9) + breath) * env
        i = smp(t0)
        y[i:i + k] += v[:n - i]
    return finish_sfx(widen(y, rng, 0.25), -4.0, 0.5, 60.0)


def sfx_crab_clack():
    rng = rng_for("crab")
    n = smp(0.4)
    y = np.zeros(n)
    for t0, v, fa in ((0.0, 1.0, 1.0), (0.085, 0.7, 1.06), (0.165, 0.95, 0.96)):
        k = smp(0.12)
        t = tt(k)
        c = (np.sin(2 * np.pi * 1100 * fa * t) * np.exp(-t / 0.02) + 0.6 * np.sin(2 * np.pi * 2350 * fa * t) * np.exp(-t / 0.011)
             + band_noise(rng, k, 2500, 10000, 2) * np.exp(-t / 0.0015) * 0.7)
        c *= (1.0 - np.exp(-t / 0.0002))
        i = smp(t0)
        y[i:i + k] += c * v
    return finish_sfx(widen(y, rng, 0.2), -3.5, 0.1, 30.0)


def sfx_worm_chomp():
    rng = rng_for("worm")
    n = smp(0.9)
    t = tt(n)
    open_ = fft_filter(rng.standard_normal(n), lp=700) * np.exp(-((t - 0.10) / 0.09) ** 2) * 0.6
    munch = band_noise(rng, n, 150, 1800, 2)
    am = 0.5 + 0.5 * np.sin(2 * np.pi * 26.0 * t)
    crunch = munch * (t > 0.2) * np.exp(-np.maximum(t - 0.2, 0.0) / 0.10) * (0.45 + 0.55 * am)
    thump = np.sin(2 * np.pi * np.cumsum(120.0 * np.exp(-np.maximum(t - 0.2, 0.0) / 0.12) + 40.0) / SR) * (t > 0.2) * np.exp(-np.maximum(t - 0.2, 0.0) / 0.15)
    gulp = np.sin(2 * np.pi * np.cumsum(180.0 + 320.0 * np.clip((t - 0.45) / 0.10, 0.0, 1.0)) / SR) * (t > 0.45) * np.exp(-np.maximum(t - 0.45, 0.0) / 0.10) * 0.5
    squelch = np.sin(2 * np.pi * 140 * t + 4.0 * np.sin(2 * np.pi * 37.0 * t)) * (t > 0.3) * np.exp(-np.maximum(t - 0.3, 0.0) / 0.2) * 0.35
    y = open_ + crunch * 1.4 + thump * 1.1 + gulp + squelch
    return finish_sfx(widen(y, rng, 0.2), -3.0, 1.0, 80.0)


def sfx_egg_crack():
    rng = rng_for("egg")
    n = smp(0.5)
    t = tt(n)
    y = np.zeros(n)
    for t0, v in ((0.0, 1.0), (0.033, 0.7), (0.071, 0.55), (0.128, 0.45)):
        k = smp(0.05)
        tv = tt(k)
        c = band_noise(rng, k, 3500, 13000, 2) * np.exp(-tv / 0.003) + 0.5 * np.sin(2 * np.pi * rng.uniform(2800, 3800) * tv) * np.exp(-tv / 0.006)
        i = smp(t0)
        y[i:i + k] += c * v
    y += np.sin(2 * np.pi * np.cumsum(380.0 - 140.0 * np.minimum(t / 0.05, 1.0)) / SR) * np.exp(-t / 0.035) * 0.7
    k = smp(0.08)
    tv = tt(k)
    i = smp(0.17)
    y[i:i + k] += np.sin(2 * np.pi * np.cumsum(250.0 + 500.0 * tv / 0.08) / SR) * np.exp(-tv / 0.03) * 0.5
    return finish_sfx(widen(y, rng, 0.15), -3.5, 0.1, 60.0)


def sfx_coin_pop():
    rng = rng_for("coin")
    n = smp(0.7)
    t = tt(n)
    pop = np.sin(2 * np.pi * np.cumsum(420.0 + 900.0 * np.minimum(t / 0.03, 1.0)) / SR) * np.exp(-t / 0.014) * 0.8
    pop += band_noise(rng, n, 2000, 8000, 2) * np.exp(-t / 0.004) * 0.25
    y = pop
    for dly, m in ((0.04, 88), (0.11, 93)):
        g = make_glock(m, 0.55) * 0.55
        g = g * np.exp(-tt(len(g)) / 0.16)
        i = smp(dly)
        y[i:i + len(g)] += g[:n - i]
    return finish_sfx(widen(y, rng, 0.25), -4.0, 0.3, 90.0)


def sfx_crowd_cheer():
    rng = rng_for("crowd")
    n = smp(1.5)
    t = tt(n)
    vow = [(700.0, 0.35, 1.0), (1150.0, 0.35, 0.7), (2600.0, 0.4, 0.4)]
    voices = np.zeros((2, n))
    for _ in range(18):
        t0 = rng.uniform(0.0, 0.25)
        f0 = rng.uniform(150.0, 340.0)
        i = smp(t0)
        k = n - i
        tv = tt(k)
        f = f0 * (1.0 + 0.10 * np.clip(tv / 0.4, 0, 1)) * (1.0 + 0.012 * np.sin(2 * np.pi * rng.uniform(4.5, 6.5) * tv + rng.uniform(0, 6)))
        dt = f / SR
        s = osc_saw(np.cumsum(dt) % 1.0, dt)
        p = rng.uniform(-0.9, 0.9)
        a = (p + 1.0) * np.pi / 4.0
        voices[0, i:] += s * np.cos(a)
        voices[1, i:] += s * np.sin(a)
    fr = np.fft.rfftfreq(next_fast(2 * n), 1.0 / SR)
    m = np.zeros_like(fr)
    for F, sg, a in vow:
        m += a * np.exp(-0.5 * (np.log2(np.maximum(fr, 1.0) / F) / sg) ** 2)
    nf = next_fast(2 * n)
    voices = np.fft.irfft(np.fft.rfft(voices, nf, axis=1) * m, nf, axis=1)[:, :n]
    voices /= voices.std() + 1e-9
    roar = np.stack([band_noise(rng, n, 900, 4500, 2), band_noise(rng, n, 900, 4500, 2)])
    roar /= roar.std()
    claps = np.zeros((2, n))
    for _ in range(46):
        i = smp(rng.uniform(0.12, 1.1))
        k = smp(0.02)
        c = band_noise(rng, k, 1000, 5000, 2) * np.exp(-tt(k) / 0.004) * rng.uniform(0.4, 1.0)
        claps[int(rng.integers(2)), i:i + k] += c[:n - i]
    env = np.minimum(1.0, t / 0.14) * np.where(t < 0.75, 1.0, np.exp(-(t - 0.75) / 0.22))
    y = (voices * 0.9 + roar * 0.35 + claps * 0.8) * env[None, :]
    return finish_sfx(y, -3.5, 5.0, 90.0)


def sfx_ui_tick():
    rng = rng_for("ui")
    n = smp(0.12)
    t = tt(n)
    y = np.sin(2 * np.pi * 2400 * t) * np.exp(-t / 0.014) + 0.4 * np.sin(2 * np.pi * 4700 * t) * np.exp(-t / 0.006)
    y += band_noise(rng, n, 3000, 12000, 2) * np.exp(-t / 0.0015) * 0.4
    return finish_sfx(y, -8.0, 0.1, 25.0)


def sfx_logo_sting():
    s = Song("logo_sting", 4.2, [])
    sting(s, 0.05)
    s.hits_raw.clear()
    y = s.mixdown()
    y = hp_causal(y, 28.0)
    y = limiter(softclip(y * 1.0, 0.7), -1.5)
    return finish_sfx(y, -2.5, 0.1, 500.0)


def build_sfx():
    OUT_SFX.mkdir(parents=True, exist_ok=True)
    table = [
        ("whoosh_short", lambda: sfx_whoosh("whoosh_short", 0.38, 500.0, 3800.0, 1200.0)),
        ("whoosh_long", lambda: sfx_whoosh("whoosh_long", 1.1, 260.0, 2600.0, 500.0, 1.3)),
        ("impact_big", sfx_impact_big),
        ("impact_small", sfx_impact_small),
        ("riser_2s", sfx_riser_2s),
        ("record_scratch", sfx_record_scratch),
        ("boing", sfx_boing),
        ("splash", sfx_splash),
        ("explosion_toy", sfx_explosion_toy),
        ("gull_screech", sfx_gull_screech),
        ("crab_clack", sfx_crab_clack),
        ("worm_chomp", sfx_worm_chomp),
        ("egg_crack", sfx_egg_crack),
        ("coin_pop", sfx_coin_pop),
        ("crowd_cheer_short", sfx_crowd_cheer),
        ("ui_tick", sfx_ui_tick),
        ("logo_sting", sfx_logo_sting),
    ]
    index = {}
    for name, fn in table:
        t0 = time.time()
        y = fn()
        write_wav(OUT_SFX / (name + ".wav"), y)
        index[name] = {"duration": round(y.shape[1] / SR, 3), "peak_db": round(20 * np.log10(np.abs(y).max()), 2),
                       "sample_rate": SR, "channels": 2}
        print("  sfx %-18s %.2f s  pico %.1f dBFS  (%.1f s)" % (name, index[name]["duration"], index[name]["peak_db"], time.time() - t0))
    (OUT_SFX / "index.json").write_text(json.dumps(index, indent=1), encoding="utf-8")
    return index


# ═══════════════════════════════════════════════════════════════════════════
# 11. Comprobación final y espectrograma
# ═══════════════════════════════════════════════════════════════════════════
def step_jumps(x, win, hop=96, floor_db=-60.0):
    """Salto de energía (dB) entre la ventana posterior y la anterior a cada posición (detector de escalón)."""
    cs = np.concatenate([[0.0], np.cumsum(x ** 2)])
    pos = np.arange(win, len(x) - win, hop)
    fwd = (cs[pos + win] - cs[pos]) / win
    bwd = (cs[pos] - cs[pos - win]) / win
    fl = 10.0 ** (floor_db / 10.0)
    jump = 10.0 * np.log10((fwd + fl) / (bwd + fl))
    full = np.zeros(len(x) // hop + 1)
    full[pos // hop] = jump
    return full


def check_hits(y, hits):
    """Localiza el ataque real de cada golpe: el mayor salto de energía de la señal (ventana de 10 ms) o de su
    derivada, que realza el ataque (ventana de 3 ms), dentro de ±20 ms. Devuelve la desviación en ms y la fuerza del salto en dB."""
    mono = y.mean(axis=0)
    diff = np.append(mono[0], mono[1:] - 0.95 * mono[:-1])                # paso alto causal (sin pre-eco)
    jl = step_jumps(mono, 480)
    jh = step_jumps(diff, 144)
    hop = 96
    rows = []
    for h in hits:
        t = h["t"]
        c = int(round(t * SR / hop))
        a, b = max(6, c - 10), c + 11                                  # ±20 ms
        if t < 0.005:                                                  # golpe en la muestra 0: debe sonar de inmediato
            lvl = 10.0 * np.log10((mono[:96] ** 2).mean() + 1e-12)
            rows.append({"t": t, "kind": h["kind"], "estim": 0.0, "dt_ms": 0.0 if lvl > -35 else 99.0, "fuerza": round(lvl + 60, 1)})
            continue
        if h["kind"] == "scratch":                                     # corte: la música cae en seco y entra el rayado
            sc = np.maximum(np.abs(jl[a:b]), np.abs(jh[a:b]))
        else:
            sc = np.maximum(jl[a:b], jh[a:b])
        j = a + int(np.argmax(sc))
        est = j * hop / SR
        rows.append({"t": t, "kind": h["kind"], "estim": round(est, 4), "dt_ms": round((est - t) * 1000, 2),
                     "fuerza": round(float(sc.max()), 1)})
    return rows


COLORS = [(0.0, (0, 0, 4)), (0.15, (30, 10, 80)), (0.35, (110, 20, 110)), (0.55, (190, 55, 80)),
          (0.75, (240, 120, 30)), (0.9, (252, 200, 60)), (1.0, (252, 255, 190))]


def colormap(v):
    xs = [c[0] for c in COLORS]
    return np.stack([np.interp(v, xs, [c[1][k] for c in COLORS]) for k in range(3)], axis=-1).astype(np.uint8)


def spectrogram_png(y, bm, path):
    import cv2
    from PIL import Image, ImageDraw, ImageFont
    x = y.mean(axis=0)
    n_fft, hop = 4096, 1024
    nfr = (len(x) - n_fft) // hop
    win = np.hanning(n_fft)
    idx = np.arange(nfr)[:, None] * hop + np.arange(n_fft)[None, :]
    mag = np.abs(np.fft.rfft(x[idx] * win, axis=1)).T / (n_fft / 4)
    f = np.fft.rfftfreq(n_fft, 1.0 / SR)
    rows = np.geomspace(30.0, 20000.0, 340)[::-1]
    sel = np.clip(np.searchsorted(f, rows), 0, len(f) - 1)
    db = 20 * np.log10(mag[sel] + 1e-7)
    vmax = np.percentile(db, 99.8)
    img = colormap(np.clip((db - (vmax - 78)) / 78, 0, 1))
    W = 2000
    img = cv2.resize(img, (W, 680), interpolation=cv2.INTER_AREA)
    L, T, R, B = 70, 60, 20, 40
    canvas = Image.new("RGB", (W + L + R, 680 + T + B), (14, 14, 18))
    canvas.paste(Image.fromarray(img), (L, T))
    d = ImageDraw.Draw(canvas)
    try:
        font = ImageFont.truetype("C:/Windows/Fonts/bahnschrift.ttf", 15)
    except Exception:
        font = ImageFont.load_default()
    dur = bm["duration"]
    X = lambda t: L + t / dur * W
    for fr in (50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000):
        yy = T + 680 * (1 - (np.log(fr) - np.log(30.0)) / (np.log(20000.0) - np.log(30.0)))
        d.line([(L - 5, yy), (L, yy)], fill=(200, 200, 200))
        d.text((6, yy - 8), ("%dk" % (fr // 1000)) if fr >= 1000 else str(fr), fill=(200, 200, 200), font=font)
    for k in range(0, int(dur) + 1, 5):
        d.line([(X(k), T + 680), (X(k), T + 686)], fill=(200, 200, 200))
        d.text((X(k) - 8, T + 690), "%ds" % k, fill=(200, 200, 200), font=font)
    for b in bm["beats"]:
        d.line([(X(b), T + 672), (X(b), T + 680)], fill=(90, 90, 100))
    for s in bm["sections"]:
        d.line([(X(s["start"]), T), (X(s["start"]), T + 680)], fill=(255, 255, 255), width=1)
        d.text((X(s["start"]) + 4, 8), "%s %s" % (s["name"], s["label"]), fill=(255, 255, 255), font=font)
    col = {"impact": (0, 220, 255), "drop": (255, 60, 200), "accent": (120, 255, 120), "cut": (255, 255, 90),
           "count": (255, 160, 60), "scratch": (255, 90, 90), "sting": (255, 255, 255)}
    for h in bm["hits"]:
        x0 = X(h["t"])
        d.polygon([(x0 - 4, T - 12), (x0 + 4, T - 12), (x0, T - 3)], fill=col.get(h["kind"], (200, 200, 200)))
    canvas.save(path)


def sfx_overview(path):
    """Mosaico de forma de onda + espectrograma de todos los efectos, para revisarlos de un vistazo."""
    import cv2
    from PIL import Image, ImageDraw, ImageFont
    files = sorted(OUT_SFX.glob("*.wav"))
    cols, cw, ch_w, ch_s = 3, 640, 70, 150
    rows = (len(files) + cols - 1) // cols
    canvas = Image.new("RGB", (cols * cw, rows * (ch_w + ch_s + 26)), (14, 14, 18))
    d = ImageDraw.Draw(canvas)
    try:
        font = ImageFont.truetype("C:/Windows/Fonts/bahnschrift.ttf", 14)
    except Exception:
        font = ImageFont.load_default()
    for k, f in enumerate(files):
        y, sr, sw = read_wav(f)
        x = y.mean(axis=0)
        ox, oy = (k % cols) * cw, (k // cols) * (ch_w + ch_s + 26)
        d.text((ox + 6, oy + 4), "%s  %.2f s  pico %.1f dBFS" % (f.stem, len(x) / sr, 20 * np.log10(np.abs(y).max() + 1e-12)),
               fill=(230, 230, 230), font=font)
        w = cw - 12
        env = np.abs(x)[:len(x) // w * w].reshape(w, -1).max(axis=1)
        img = np.zeros((ch_w, w, 3), np.uint8) + 20
        for i, v in enumerate(env):
            h = int(min(1.0, v) * (ch_w / 2 - 2))
            img[ch_w // 2 - h:ch_w // 2 + h + 1, i] = (90, 200, 255)
        canvas.paste(Image.fromarray(img), (ox + 6, oy + 24))
        n_fft, hop = 1024, max(64, len(x) // 400)
        nfr = max(1, (len(x) - n_fft) // hop)
        idx = np.arange(nfr)[:, None] * hop + np.arange(n_fft)[None, :]
        mag = np.abs(np.fft.rfft(x[idx] * np.hanning(n_fft), axis=1)).T
        fr = np.fft.rfftfreq(n_fft, 1.0 / sr)
        sel = np.clip(np.searchsorted(fr, np.geomspace(60.0, 20000.0, 120)[::-1]), 0, len(fr) - 1)
        db = 20 * np.log10(mag[sel] + 1e-6)
        sp = colormap(np.clip((db - (db.max() - 70)) / 70, 0, 1))
        sp = cv2.resize(sp, (w, ch_s), interpolation=cv2.INTER_AREA)
        canvas.paste(Image.fromarray(sp), (ox + 6, oy + 24 + ch_w))
    canvas.save(path)


def band_report(y, sections):
    edges = [(20, 60), (60, 200), (200, 800), (800, 3000), (3000, 8000), (8000, 20000)]
    out = []
    for n, a, b, lb in sections:
        seg = y[:, smp(a):smp(b)]
        sp = np.abs(np.fft.rfft(seg.mean(axis=0) * np.hanning(seg.shape[1]))) ** 2
        f = np.fft.rfftfreq(seg.shape[1], 1.0 / SR)
        tot = sp.sum() + 1e-20
        bands = [10 * np.log10(sp[(f >= lo) & (f < hi)].sum() / tot + 1e-12) for lo, hi in edges]
        rms = 20 * np.log10(np.sqrt((seg ** 2).mean()) + 1e-12)
        out.append({"tramo": n, "rms_dbfs": round(rms, 1), "lufs_tramo": round(lufs_plain(seg), 1),
                    "bandas_db": [round(float(v), 1) for v in bands], "pico": round(20 * np.log10(np.abs(seg).max() + 1e-12), 1)})
    return out


def verify(kind):
    y, sr, sw = read_wav(OUT_MUSIC / ("trailer_%s.wav" % kind))
    bm = json.loads((OUT_MUSIC / ("beatmap_%s.json" % kind)).read_text(encoding="utf-8"))
    info = {
        "archivo": "trailer_%s.wav" % kind, "duracion_s": round(y.shape[1] / sr, 4), "sr": sr, "canales": y.shape[0],
        "bits": sw * 8, "pico_muestra_dbfs": round(20 * np.log10(np.abs(y).max()), 2),
        "pico_verdadero_dbtp": round(true_peak_db(y), 2), "lufs_integrado": round(lufs_integrated(y), 2),
        "muestras_a_fondo_de_escala": int((np.abs(y) >= 0.9999).sum()),
        "salto_max": round(float(np.abs(np.diff(y, axis=1)).max()), 3),
    }
    rows = check_hits(y, bm["hits"])
    bad = [r for r in rows if abs(r["dt_ms"]) > 4.5 or r["fuerza"] < 4.0]
    info["golpes"] = len(rows)
    info["golpes_fuera"] = bad
    info["desviacion_media_ms"] = round(float(np.mean([abs(r["dt_ms"]) for r in rows])), 2)
    info["tramos"] = band_report(y, [(s["name"], s["start"], s["end"], s["label"]) for s in bm["sections"]])
    spectrogram_png(y, bm, OUT_MUSIC / ("espectrograma_%s.png" % kind))
    print("  %s: %.3f s, %d Hz, %d canales, %d bits | pico %.2f dBFS, %.2f dBTP | %.2f LUFS | clip %d | salto máx %.3f"
          % (info["archivo"], info["duracion_s"], sr, info["canales"], info["bits"], info["pico_muestra_dbfs"],
             info["pico_verdadero_dbtp"], info["lufs_integrado"], info["muestras_a_fondo_de_escala"], info["salto_max"]))
    print("    golpes: %d, desviación media %.2f ms, fuera de tolerancia: %d" % (len(rows), info["desviacion_media_ms"], len(bad)))
    for r in bad:
        print("      !! t=%.3f %-7s estimado %.4f (%.2f ms) fuerza %.1f" % (r["t"], r["kind"], r["estim"], r["dt_ms"], r["fuerza"]))
    for tr in info["tramos"]:
        print("    %s  rms %6.1f  lufs %6.1f  pico %6.1f  bandas(20-60,60-200,200-800,0.8-3k,3-8k,8-20k) %s"
              % (tr["tramo"], tr["rms_dbfs"], tr["lufs_tramo"], tr["pico"], tr["bandas_db"]))
    return info


# ═══════════════════════════════════════════════════════════════════════════
def main():
    try:
        sys.stdout.reconfigure(encoding="utf-8")
    except Exception:
        pass
    args = set(sys.argv[1:])
    everything = not args
    OUT_MUSIC.mkdir(parents=True, exist_ok=True)
    OUT_SFX.mkdir(parents=True, exist_ok=True)
    t0 = time.time()
    report = {}
    for kind in ("60", "30"):
        if everything or kind in args:
            print("== pista de %s s ==" % kind)
            build_track(kind)
    if everything or "sfx" in args:
        print("== efectos ==")
        report["sfx"] = build_sfx()
    if everything or "verify" in args or "60" in args or "30" in args:
        print("== comprobación ==")
        for kind in ("60", "30"):
            if (OUT_MUSIC / ("trailer_%s.wav" % kind)).exists() and (everything or "verify" in args or kind in args):
                report["musica_" + kind] = verify(kind)
        if any(OUT_SFX.glob("*.wav")):
            sfx_overview(OUT_SFX / "resumen_efectos.png")
        (OUT_MUSIC / "informe.json").write_text(json.dumps(report, ensure_ascii=False, indent=1), encoding="utf-8")
    print("Total: %.1f s" % (time.time() - t0))


if __name__ == "__main__":
    main()
