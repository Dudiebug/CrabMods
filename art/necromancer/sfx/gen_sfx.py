#!/usr/bin/env python3
"""Procedural sound effects for the CrabMods necromancer kit.

Every sound is synthesized from scratch with numpy (no samples, no downloads).
Deterministic: each cue variation uses a fixed seed, so re-running produces
bit-identical WAVs. Output: 44.1 kHz, 16-bit, stereo WAVs written next to this
script, named A_<Thing>_<Event>_NN.wav (one UE random cue per name group).

    python3 gen_sfx.py
"""
import os
import wave

import numpy as np

SR = 44100
OUT = os.path.dirname(os.path.abspath(__file__))

# Loudness tiers: (true-peak ceiling dBTP, momentary-loudness target LUFS, max drive dB).
# Game impacts peak ~0 dBFS and reach ~-6 to -4 LUFS momentary; UI sits 6-8 dB under.
IMPACT = (-0.5, -5.5, 20.0)
SOFT = (-2.0, -11.0, 18.0)
PICKUP = (-4.0, -14.0, 8.0)
UI = (-6.0, -13.5, 8.0)
QUIET = (-8.0, -18.0, 6.0)
CRACK = (-0.5, -12.0, 3.0)  # keep the crack transient on top rather than squashed

CRACK_T = 16 / 30  # 0.5333 s: whip montage frame 17 (1-based) at 30 fps


# --------------------------------------------------------------------------- basics
def N(d):
    return int(round(d * SR))


def tt(d):
    return np.arange(N(d)) / SR


def db(x):
    return 10 ** (x / 20)


def st(x):
    x = np.asarray(x, float)
    return np.vstack([x, x]) if x.ndim == 1 else x


def buf(d):
    return np.zeros((2, N(d)))


def place(b, x, t0, gain=1.0, pan=0.0):
    """Add x (mono or stereo) into stereo buffer b at time t0; pan in [-1, 1]."""
    x = np.asarray(x, float)
    if x.ndim == 1:
        a = (pan + 1) * np.pi / 4
        x = np.vstack([x * np.cos(a), x * np.sin(a)]) * np.sqrt(2)
    i = N(t0)
    n = min(x.shape[1], b.shape[1] - i)
    if n > 0:
        b[:, i:i + n] += gain * x[:, :n]


def envp(t, pts, curve=1.0):
    """Piecewise-linear envelope through (time, value) points, optional power curve."""
    ts, vs = zip(*pts)
    e = np.interp(t, ts, vs)
    return e ** curve if curve != 1.0 else e


def npow2(n):
    return 1 << int(np.ceil(np.log2(max(n, 2))))


# --------------------------------------------------------------------------- filters
def ffilt(x, H, pad=8192):
    """Zero-phase FFT filter along last axis; H(f) -> gain."""
    x = np.asarray(x, float)
    n = x.shape[-1]
    nf = npow2(n + 2 * pad)
    xp = np.zeros(x.shape[:-1] + (nf,))
    xp[..., pad:pad + n] = x
    f = np.fft.rfftfreq(nf, 1 / SR)
    y = np.fft.irfft(np.fft.rfft(xp) * H(f), nf)
    return y[..., pad:pad + n]


def _ratio(f, fc):
    return np.maximum(f, 1e-3) / fc


def lp(x, fc, order=2):
    return ffilt(x, lambda f: 1 / np.sqrt(1 + _ratio(f, fc) ** (2 * order)))


def hp(x, fc, order=2):
    return ffilt(x, lambda f: 1 / np.sqrt(1 + _ratio(f, fc) ** (-2 * order)))


def bp(x, lo, hi, order=2):
    return lp(hp(x, lo, order), hi, order)


def gband(f, fc, oct_bw):
    return np.exp(-0.5 * (np.log2(np.maximum(f, 1.0) / fc) / oct_bw) ** 2)


def formant_H(forms, floor=0.02):
    """forms: list of (freq, bandwidth Hz, gain)."""
    def H(f):
        h = np.full_like(f, floor)
        for fc, bw, g in forms:
            h += g * np.exp(-0.5 * ((f - fc) / bw) ** 2)
        return h
    return H


def tv_filter(x, maskfn, nfft=1024, hop=256):
    """Time-varying filter via STFT masking. maskfn(t[:,None], f[None,:]) -> gain."""
    n = len(x)
    pad = nfft
    xp = np.concatenate([np.zeros(pad), x, np.zeros(pad + nfft)])
    nfr = (len(xp) - nfft) // hop + 1
    idx = np.arange(nfr)[:, None] * hop + np.arange(nfft)[None, :]
    win = 0.5 - 0.5 * np.cos(2 * np.pi * np.arange(nfft) / nfft)
    X = np.fft.rfft(xp[idx] * win, axis=1)
    tc = (np.arange(nfr) * hop + nfft / 2 - pad) / SR
    f = np.fft.rfftfreq(nfft, 1 / SR)
    y = np.fft.irfft(X * maskfn(tc[:, None], f[None, :]), n=nfft, axis=1) * win
    out = np.zeros(len(xp))
    for i in range(nfr):
        out[i * hop:i * hop + nfft] += y[i]
    out /= (win ** 2).sum() / hop
    return out[pad:pad + n]


def fftconv(a, b):
    n = len(a) + len(b) - 1
    nf = npow2(n)
    return np.fft.irfft(np.fft.rfft(a, nf) * np.fft.rfft(b, nf), nf)[:n]


def reverb(x, rt=0.6, wet=0.2, pre=0.012, seed=0, bright=6000.0, width=0.6):
    """Stereo room: dry + decorrelated exponentially decaying noise IRs."""
    x = st(x)
    rng = np.random.default_rng(seed + 9001)
    t = tt(rt * 1.3)
    out = np.zeros((2, x.shape[1] + len(t) + N(pre)))
    out[:, :x.shape[1]] += x
    wets = []
    for c in range(2):
        nz = rng.standard_normal(len(t))
        ir = lp(nz, bright, 1) * np.exp(-6.91 * t / (rt * 0.7))
        ir += 0.8 * lp(rng.standard_normal(len(t)), 1800, 1) * np.exp(-6.91 * t / rt)
        ir *= np.minimum(1, t / 0.004)  # no hard onset
        ir = hp(ir, 180, 2)  # keep lows dry: no muddy sub tails
        ir /= np.sqrt((ir ** 2).sum())
        wets.append(fftconv(x[c], ir) * wet)
    mid, side = (wets[0] + wets[1]) / 2, (wets[0] - wets[1]) / 2 * width  # narrowed for mono safety
    for c, w in enumerate((mid + side, mid - side)):
        out[c, N(pre):N(pre) + len(w)] += w[:out.shape[1] - N(pre)]
    return out


# --------------------------------------------------------------------------- sources
def phase_of(freq, n):
    f = np.broadcast_to(np.asarray(freq, float), (n,))
    return 2 * np.pi * np.cumsum(f) / SR


def sine(freq, d, ph0=0.0):
    return np.sin(phase_of(freq, N(d)) + ph0)


def saw_bl(freq, d, fmax=5000.0, ph0=0.0):
    """Band-limited additive sawtooth (harmonics up to fmax, gently rolled off)."""
    n = N(d)
    ph = phase_of(freq, n) + ph0
    top = float(np.max(freq))
    out = np.zeros(n)
    for k in range(1, int(fmax / top) + 1):
        out += np.sin(k * ph) / k * (1 - 0.5 * k * top / fmax)
    return out


def damped(f, tau, d, ph=0.0):
    t = tt(d)
    return np.sin(2 * np.pi * f * t + ph) * np.exp(-t / tau)


def noise(rng, d):
    return rng.standard_normal(N(d))


def swept_noise(rng, d, fc_fn, bw_oct, amp_fn):
    """Noise through a time-varying log-Gaussian band-pass centred on fc_fn(t)."""
    x = noise(rng, d)
    y = tv_filter(x, lambda t, f: gband(f, fc_fn(t), bw_oct))
    return y * amp_fn(tt(d))


def modal_click(rng, freqs, taus, amps, d=None, snap=0.3):
    """Resonant click: a few damped modes excited by a tiny noise burst."""
    d = d or max(taus) * 6
    out = np.zeros(N(d))
    for f, tau, a in zip(freqs, taus, amps):
        out += a * damped(f, tau, d, rng.uniform(0, 2 * np.pi))
    burst = rng.standard_normal(N(0.0015)) * np.exp(-np.arange(N(0.0015)) / N(0.0004))
    out[:len(burst)] += snap * hp(burst, 1500)
    out[:N(0.001)] *= np.linspace(0, 1, N(0.001))
    return out


def bone_click(rng, lo=1200, hi=4800, scale=1.0):
    f0 = rng.uniform(lo, hi)
    freqs = [f0, f0 * rng.uniform(1.45, 1.8), f0 * rng.uniform(2.3, 2.9)]
    taus = [rng.uniform(0.004, 0.012) * scale, rng.uniform(0.002, 0.006) * scale, 0.002]
    return modal_click(rng, freqs, taus, [1.0, 0.5, 0.25])


def heavy_bone_crack(rng):
    """Big splintering crack: low modal body + broadband snap + splinter micro-clicks."""
    d = 0.18
    out = np.zeros(N(d))
    f0 = rng.uniform(350, 650)
    for f, tau, a in [(f0, 0.03, 0.8), (f0 * 2.31, 0.015, 0.5), (f0 * 4.1, 0.008, 0.3)]:
        out += a * damped(f, tau, d, rng.uniform(0, 6.28))
    sn = noise(rng, d) * np.exp(-tt(d) / 0.006)
    out += 1.2 * hp(sn, 900)
    for _ in range(rng.integers(3, 7)):
        c = bone_click(rng, 1500, 5000)
        i = N(rng.uniform(0.002, 0.03))
        out[i:i + len(c)] += rng.uniform(0.3, 0.7) * c[:len(out) - i]
    out[:N(0.0008)] *= np.linspace(0, 1, N(0.0008))
    return out


def skitter(rng, b, t0, t1, rate_fn, amp=0.4, lo=1500, hi=5000, width=0.5):
    """Poisson stream of small bone clicks with time-varying density."""
    t = t0
    while True:
        r = max(rate_fn(t), 1.0)
        t += rng.exponential(1 / r)
        if t >= t1:
            break
        place(b, bone_click(rng, lo, hi), t, amp * rng.uniform(0.3, 1.0) * min(1, rate_fn(t) / 20 + 0.3),
              pan=rng.uniform(-width, width))


def whoosh(rng, d, f0, f1, peak, bw=0.7, curve=1.0, flutter=0.0, fl_rate=20.0):
    def fc(t):
        u = np.clip(t / d, 0, 1) ** curve
        return f0 * (f1 / f0) ** u
    tp = peak * d

    def amp(t):
        a = np.where(t < tp, (t / tp) ** 2, np.exp(-(t - tp) / (0.25 * (d - tp) + 1e-3)))
        return a * (1 - flutter + flutter * (0.5 + 0.5 * np.sin(2 * np.pi * fl_rate * t)))
    l = swept_noise(rng, d, fc, bw, amp)
    r = swept_noise(rng, d, lambda t: fc(t) * 1.06, bw, amp)
    return np.vstack([0.8 * l + 0.2 * r, 0.2 * l + 0.8 * r])


def low_thump(rng, f_hi, f_lo, tau, d=None):
    d = d or tau * 7
    t = tt(d)
    f = f_lo + (f_hi - f_lo) * np.exp(-t / 0.03)
    s = np.sin(phase_of(f, len(t))) * np.exp(-t / tau)
    s[:N(0.002)] *= np.linspace(0, 1, N(0.002))
    return s


def rumble(rng, d, amp_fn, cutoff=110):
    x = bp(noise(rng, d), 32, cutoff, 4) * 3
    t = tt(d)
    sub = np.sin(phase_of(46 + 4 * np.sin(2 * np.pi * 0.7 * t), len(t)))
    am = 0.7 + 0.3 * lp(rng.standard_normal(len(t)), 6, 2) / 0.05
    return (x + 0.5 * sub * np.clip(am, 0.2, 1.3)) * amp_fn(t)


def choir(rng, notes, d, amp_fn, voices=3, detune=14.0, vowel=((730, 90, 1.0), (1090, 110, 0.5), (2440, 160, 0.22))):
    """Dark 'ahh' pad: detuned band-limited saws through vowel formants."""
    t = tt(d)
    out = np.zeros((2, len(t)))
    for f0 in notes:
        for v in range(voices):
            cents = rng.uniform(-detune, detune)
            vib = 1 + 0.005 * np.sin(2 * np.pi * rng.uniform(4.2, 5.6) * t + rng.uniform(0, 6.28))
            drift = 1 + 0.002 * np.sin(2 * np.pi * rng.uniform(0.2, 0.5) * t + rng.uniform(0, 6.28))
            s = saw_bl(f0 * 2 ** (cents / 1200) * vib * drift, d, fmax=4500, ph0=rng.uniform(0, 6.28))
            place(out, s, 0, 1 / np.sqrt(len(notes) * voices), pan=rng.uniform(-0.7, 0.7))
    out += 0.04 * np.vstack([noise(rng, d), noise(rng, d)])  # breath
    out = ffilt(out, formant_H(vowel, 0.03))
    out = lp(out, 3500, 2)
    return out * amp_fn(t)


def chime(rng, f, d, tau=0.12, ratios=(1, 2.0, 2.76, 5.4), amps=(1, 0.2, 0.3, 0.1), det_c=1.0):
    """Glassy struck chime; L/R share phase with a tiny detune (wide but mono-safe)."""
    t = tt(d)
    out = np.zeros((2, len(t)))
    for k, (r, a) in enumerate(zip(ratios, amps)):
        tk = tau / (1 + 0.7 * k)
        ph = rng.uniform(0, 6.28)
        for c, sgn in enumerate((-1, 1)):
            fr = f * r * 2 ** (sgn * det_c / 1200)
            out[c] += a * np.sin(2 * np.pi * fr * t + ph) * np.exp(-t / tk)
    out[:, :N(0.0015)] *= np.linspace(0, 1, N(0.0015))
    return out


# --------------------------------------------------------------------------- mastering
def true_peak(x):
    x = st(x)
    n = x.shape[1]
    X = np.fft.rfft(x, axis=1)
    Xu = np.zeros((2, 2 * n + 1), complex)
    Xu[:, :X.shape[1]] = X
    return np.abs(np.fft.irfft(Xu, 4 * n, axis=1) * 4).max()


def limiter(x, ceil=0.9, win=0.012):
    """Look-ahead brickwall: sliding min of needed gain, then centred Hann smoothing."""
    a = np.abs(x).max(0)
    g = np.minimum(1.0, ceil / np.maximum(a, 1e-12))
    w = N(win)
    m = g.copy()
    s = 1
    while s < w:  # sliding min over width ~w (centred)
        sh = s // 2 + 1
        m = np.minimum(m, np.minimum(np.concatenate([m[sh:], np.ones(sh)]),
                                     np.concatenate([np.ones(sh), m[:-sh]])))
        s = s * 2 + 1
    k = np.hanning(w // 2 * 2 + 1)
    k /= k.sum()
    gs = np.convolve(m, k, mode="same")
    return x * gs


def kweight(x):
    """Approximate BS.1770 K-weighting (38 Hz high-pass + ~+4 dB high shelf)."""
    def H(f):
        f2 = np.maximum(f, 1e-3) ** 2
        return (f2 / (f2 + 38.0 ** 2)) * np.sqrt((1 + db(8) * f2 / 1500.0 ** 2) / (1 + f2 / 1500.0 ** 2))
    return ffilt(x, H, pad=2048)


def momentary_max(x):
    """Max BS.1770 momentary loudness (400 ms window), LUFS, approximately."""
    z = (kweight(np.pad(x, ((0, 0), (0, N(0.4))))) ** 2).sum(0)
    c = np.concatenate([[0], np.cumsum(z)])
    w = N(0.4)
    ms = (c[w:] - c[:-w]) / w
    return -0.691 + 10 * np.log10(ms.max() + 1e-20)


def compressor(x, thr_db=-16.0, ratio=3.0, win=0.025):
    """RMS feed-forward compressor with smoothed (Hann) gain; gain never exceeds 1."""
    e = np.convolve((x ** 2).mean(0), np.ones(N(0.01)) / N(0.01), mode="same")
    lvl = 10 * np.log10(e + 1e-20)
    gdb = np.minimum(0, (thr_db - lvl) * (1 - 1 / ratio))
    k = np.hanning(N(win) | 1)
    k /= k.sum()
    return x * db(np.convolve(gdb, k, mode="same"))


def master(x, drive):
    if drive > 0:
        x = compressor(x * db(drive * 0.5))
        x = x / np.abs(x).max()
        x = limiter(x * db(drive * 0.6), 0.9)
    return x


def finalize(x, tier, keep_lead=False, maxdur=None, fade_out=0.02):
    """Clean (DC, trim, fades), then raise density until momentary loudness hits the tier
    target (bounded drive), then normalise to the tier's true-peak ceiling."""
    tp_db, m_target, max_drive = tier
    x = st(x)
    x = x - x.mean(axis=1, keepdims=True)
    x = hp(x, 30, 2)
    x = x / (np.abs(x).max() + 1e-12)
    a = np.abs(x).max(0)
    end = int(np.nonzero(a > db(-62))[0][-1]) + N(0.01)
    start = 0
    if not keep_lead:
        start = max(0, int(np.nonzero(a > db(-50))[0][0]) - N(0.002))
    x = x[:, start:min(end, x.shape[1])]
    if maxdur and x.shape[1] > N(maxdur):
        x = x[:, :N(maxdur)]
        fade_out = max(fade_out, min(0.18, 0.12 * maxdur))
    fi = N(0.005)
    x[:, :fi] *= 0.5 - 0.5 * np.cos(np.linspace(0, np.pi, fi))
    fo = N(fade_out)
    x[:, -fo:] *= 0.5 + 0.5 * np.cos(np.linspace(0, np.pi, fo))

    def render(drive):
        y = master(x, drive)
        y[:, 0] = 0
        y[:, -1] = 0
        return y * (db(tp_db - 0.05) / true_peak(y))

    lo, hi = 0.0, max_drive
    y = render(0.0)
    if momentary_max(y) < m_target:
        y = render(hi)
        if momentary_max(y) < m_target:
            return y
        for _ in range(7):  # bisection on drive
            mid = 0.5 * (lo + hi)
            if momentary_max(render(mid)) < m_target:
                lo = mid
            else:
                hi = mid
        y = render(hi)
    return y


def write(name, x):
    pcm = np.clip(np.round(x.T * 32767), -32768, 32767).astype("<i2")
    with wave.open(os.path.join(OUT, name + ".wav"), "wb") as w:
        w.setnchannels(2)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes(pcm.tobytes())
    print(f"{name:32s} {x.shape[1] / SR:5.3f}s")


# --------------------------------------------------------------------------- cues
def soul_whip_crack(v):
    rng = np.random.default_rng(1000 + v)
    P = [dict(f0=280, f1=3400, nw=0.0011, body=2200, pop=170, vowel=0),
         dict(f0=320, f1=4200, nw=0.0008, body=2600, pop=190, vowel=1),
         dict(f0=240, f1=3000, nw=0.0014, body=1900, pop=150, vowel=0),
         dict(f0=300, f1=3800, nw=0.0010, body=2400, pop=210, vowel=1)][v]
    b = buf(1.4)
    # 1. building swish (0 -> crack): rising band-pass sweep, growing louder, slight whirr
    sd = CRACK_T + 0.004
    sw = whoosh(rng, sd, P["f0"], P["f1"], peak=0.97, bw=0.55, curve=1.4,
                flutter=0.35, fl_rate=rng.uniform(16, 24))
    sw *= envp(tt(sd), [(0, 0), (0.45, 0.75), (sd - 0.02, 1.0), (sd, 0.2)])
    place(b, sw, 0, 0.5)
    # 2. CRACK at exactly CRACK_T: N-wave + click + HP noise snap + body + pop + slap
    nw = np.linspace(1.0, -1.0, max(3, N(P["nw"])))
    c = np.zeros(N(0.08))
    c[:len(nw)] += nw
    c[1] += 0.6  # extra sharp click on the leading edge
    snap = hp(noise(rng, 0.03) * np.exp(-tt(0.03) / 0.004), 2500, 2)
    c[:len(snap)] += 0.9 * snap / np.abs(snap).max()
    c = np.tanh(5.0 * c) / np.tanh(5.0)  # saturate the transient: keeps sharp edges, adds density
    c = lp(c, 15000, 4)  # tame Gibbs/inter-sample overshoot of the hard edges
    c += 0.35 * damped(P["body"], 0.005, 0.08, 0.0)
    c += 0.35 * low_thump(rng, P["pop"] * 1.6, P["pop"], 0.02, 0.08)
    c += 0.3 * lp(hp(noise(rng, 0.08), 400) * np.exp(-tt(0.08) / 0.012), 3000)  # meaty thwack
    cs = st(c)
    cs[1, :N(0.03)] += 0.25 * hp(noise(rng, 0.03) * np.exp(-tt(0.03) / 0.002), 3000)  # width
    place(b, cs, CRACK_T, 1.0)
    place(b, lp(c, 6000), CRACK_T + rng.uniform(0.009, 0.014), 0.22, pan=rng.uniform(-0.3, 0.3))
    # 3. ghostly whisper tail: formant-morphing noise, tremolo
    wd = 0.55
    va = [(850, 120, 1.0), (1220, 150, 0.6), (2810, 250, 0.35)]
    vu = [(350, 90, 1.0), (870, 120, 0.5), (2250, 220, 0.25)]
    if P["vowel"]:
        va, vu = [(500, 100, 1.0), (1700, 180, 0.6), (2600, 250, 0.4)], va

    def wmask(t, f):
        u = np.clip(t / wd, 0, 1)
        h = 0.02
        for (fa, ba, ga), (fu, bu, gu) in zip(va, vu):
            fc = fa + (fu - fa) * u
            bw = ba + (bu - ba) * u
            h = h + (ga + (gu - ga) * u) * np.exp(-0.5 * ((f - fc) / bw) ** 2)
        return h
    t = tt(wd)
    wenv = envp(t, [(0, 0), (0.04, 1), (wd, 0)], 1.6) * (0.65 + 0.35 * np.sin(2 * np.pi * 7 * t))
    wl = tv_filter(noise(rng, wd), wmask) * wenv
    wr = tv_filter(noise(rng, wd), wmask) * wenv
    w = np.vstack([wl, wr])
    place(b, w / np.abs(w).max(), CRACK_T + 0.02, 0.16)
    out = reverb(b, rt=0.45, wet=0.18, seed=v, bright=7000)
    return finalize(out, CRACK, keep_lead=True, maxdur=0.95, fade_out=0.06)


def soul_whip_equip(v):
    rng = np.random.default_rng(2000 + v)
    b = buf(0.8)
    # leather creak: jittered stick-slip impulse train through leather body resonances
    imp = np.zeros(N(0.3))
    t, end = 0.01, 0.2 + 0.03 * v
    while t < end:
        u = t / end
        rate = 55 + 110 * u if v == 0 else 140 - 70 * u
        imp[N(t)] += np.sin(np.pi * u) ** 0.7 * rng.uniform(0.5, 1.0)
        t += rng.uniform(0.8, 1.25) / rate
    cr = ffilt(imp, formant_H([(520 + 60 * v, 60, 1.0), (1150, 110, 0.6), (2400, 200, 0.3)], 0.05))
    place(b, cr / np.abs(cr).max(), 0, 0.55, pan=-0.15)
    # soft lash flick
    ft = 0.24 + 0.02 * v
    place(b, whoosh(rng, 0.09, 900, 4500, peak=0.85, bw=0.6), ft - 0.08, 0.6)
    nw = np.linspace(1, -1, N(0.0007))
    sn = np.zeros(N(0.04))
    sn[:len(nw)] = nw
    sn += 0.5 * hp(noise(rng, 0.04) * np.exp(-tt(0.04) / 0.002), 2000)
    place(b, lp(sn, 7000), ft, 0.45, pan=0.1)
    # soul shimmer: inharmonic high partials, tremolo, fade in/out
    sd = 0.45
    t = tt(sd)
    sh = np.zeros((2, len(t)))
    for k in range(5):
        f = rng.uniform(1800, 4200)
        ph, trem, tph = rng.uniform(0, 6.28), rng.uniform(8, 13), rng.uniform(0, 6.28)
        for c in range(2):
            sh[c] += np.sin(2 * np.pi * f * (1 + (c - 0.5) * 0.0002) * t + ph) * \
                (0.6 + 0.4 * np.sin(2 * np.pi * trem * t + tph + 0.6 * c))
    sh *= envp(t, [(0, 0), (0.15, 1), (sd, 0)], 1.5)
    place(b, sh / np.abs(sh).max(), 0.1, 0.12)
    out = reverb(b, rt=0.35, wet=0.15, seed=20 + v)
    return finalize(out, SOFT, maxdur=0.55, fade_out=0.05)


def soul_whip_tag(v):
    rng = np.random.default_rng(3000 + v)
    b = buf(1.2)
    # lash slap + dull thud
    place(b, low_thump(rng, 140 - 10 * v, 60 + 5 * v, 0.06), 0, 1.0)
    dust = lp(noise(rng, 0.12) * np.exp(-tt(0.12) / 0.03), 900, 2)
    place(b, dust / np.abs(dust).max(), 0, 0.45)
    sl = hp(noise(rng, 0.02) * np.exp(-tt(0.02) / 0.003), 1200)
    place(b, sl / np.abs(sl).max(), 0, 0.35)
    # eerie inharmonic bell: partials 500-900 Hz, slow chorus detune, slight downward bend
    f1 = [540, 600, 570][v]
    ratios = [(1.0, 1.0), ([1.34, 1.29, 1.37][v], 0.7), ([1.59, 1.52, 1.63][v], 0.55),
              (2.76, 0.12), (4.07, 0.05)]
    d = 0.9
    t = tt(d)
    bell = np.zeros((2, len(t)))
    for k, (r, a) in enumerate(ratios):
        tau = 0.32 / (1 + 0.6 * k)
        for sgn in (-1, 1):  # detuned pair -> slow chorus beating; L/R differ only in LFO phase
            cb, lr, lph, ph = rng.uniform(3, 7), rng.uniform(0.3, 0.8), rng.uniform(0, 6.28), rng.uniform(0, 6.28)
            for c in range(2):
                cents = sgn * cb * (1 + 0.5 * np.sin(2 * np.pi * lr * t + lph + 0.9 * c))
                bend = 2 ** (-18 * (1 - np.exp(-t / 0.4)) / 1200)
                bell[c] += a * 0.5 * np.sin(phase_of(f1 * r * 2 ** (cents / 1200) * bend, len(t))
                                            + ph) * np.exp(-t / tau)
    bell *= envp(t, [(0, 0), (0.004, 1), (d, 1)])
    place(b, bell / np.abs(bell).max(), 0.018, 0.55)
    out = reverb(b, rt=0.7, wet=0.22, seed=30 + v, bright=5000)
    return finalize(out, IMPACT, maxdur=0.85, fade_out=0.08)


PENTA = [0, 2, 4, 7, 9, 12, 14, 16]  # major pentatonic steps


def soul_pickup(i):
    rng = np.random.default_rng(4000 + i)
    f = 784.0 * 2 ** (PENTA[i] / 12)  # G5 ... B6
    b = buf(0.5)
    place(b, chime(rng, f, 0.3, tau=0.10), 0, 1.0)
    place(b, chime(rng, f * 1.5, 0.25, tau=0.06, amps=(1, 0.1, 0.15, 0.0)), 0.022, 0.28)
    tick = hp(noise(rng, 0.01) * np.exp(-tt(0.01) / 0.0012), 3000)
    place(b, tick / np.abs(tick).max(), 0, 0.12)
    b = lp(b, 9000, 2)
    out = reverb(b, rt=0.35, wet=0.2, seed=40 + i, bright=8000)
    return finalize(out, PICKUP, maxdur=0.25, fade_out=0.04)


def ground_crack(rng, b, t0, g=1.0):
    place(b, low_thump(rng, 110, 48, 0.05), t0, 0.8 * g)
    d = lp(noise(rng, 0.25) * np.exp(-tt(0.25) / 0.06), 2000, 2)
    place(b, np.vstack([d, lp(noise(rng, 0.25) * np.exp(-tt(0.25) / 0.06), 2000, 2)]) / np.abs(d).max(),
          t0, 0.35 * g)
    for _ in range(14):
        place(b, bone_click(rng, 700, 3000, 0.7), t0 + rng.exponential(0.02), 0.3 * g * rng.uniform(0.4, 1),
              pan=rng.uniform(-0.4, 0.4))


def boom(rng, d=1.0):
    t = tt(d)
    f = 46 + 120 * np.exp(-t / 0.045)
    k = np.tanh(2.2 * np.sin(phase_of(f, len(t))) * np.exp(-t / 0.35)) * np.minimum(1, t / 0.002)
    mid = bp(noise(rng, d), 120, 500) * np.exp(-t / 0.09)
    cr = hp(noise(rng, d), 1500) * np.exp(-t / 0.012)
    return k + 0.8 * mid / np.abs(mid).max() + 0.5 * cr / np.abs(cr).max()


def necro_raise(tier, v):
    rng = np.random.default_rng(5000 + 10 * tier + v)
    dur = [0.8, 1.2, 1.6, 2.4][tier]
    b = buf(dur + 1.2)
    semis = [0, -1.5][v] if tier < 3 else [0, -1][v]
    # all tiers: small ground crack, skittering bones, soft whoosh
    ground_crack(rng, b, 0.0, 1.0 if tier == 0 else 1.2)
    sk_end = [0.6, 0.9, 1.2, 1.7][tier]
    peak_t = 0.12 + 0.05 * v
    skitter(rng, b, 0.02, sk_end,
            lambda t: 15 + 110 * np.exp(-((t - peak_t) / (0.18 + 0.12 * tier)) ** 2) * (1 + 0.5 * tier),
            amp=0.45, lo=1400 * 2 ** (semis / 12), hi=4800)
    place(b, whoosh(rng, 0.55, 500 + 80 * v, 1600, peak=0.5, bw=0.8), 0.04, 0.30)
    if tier >= 1:  # rising whoosh + low rumble
        rd = [0, 1.0, 1.3, 1.45][tier]
        place(b, whoosh(rng, rd, 220, 4000 + 600 * v, peak=0.85, bw=0.6, curve=1.3), 0.08, 0.45)
        rl = [0, 1.1, 1.5, 2.3][tier]
        r = rumble(rng, rl, lambda t: envp(t, [(0, 0), (0.25 * rl, 1), (0.7 * rl, 0.8), (rl, 0)]))
        place(b, r / np.abs(r).max(), 0, [0, 0.45, 0.55, 0.75][tier])
    if tier >= 2:  # heavy bone cracks + dark 'ahh' choir
        hits = [0.0, 0.35, 0.62] if tier == 2 else [0.0, 0.3, 0.55, 0.85, 1.1]
        for h in hits:
            place(b, heavy_bone_crack(rng), h + rng.uniform(0, 0.04), 0.6, pan=rng.uniform(-0.35, 0.35))
        root = 73.42 * 2 ** (semis / 12)  # D2
        notes = [root, root * 1.5, root * 2, root * 2.378]
        cd = dur - 0.05
        if tier == 3:
            notes += [root * 3, root * 4]
            cenv = lambda t: envp(t, [(0, 0), (1.3, 1), (1.4, 1), (cd, 0)], 1.4)
        else:
            cenv = lambda t: envp(t, [(0, 0), (0.6, 1), (1.0, 0.8), (cd, 0)], 1.3)
        ch = choir(rng, notes, cd, cenv, voices=3 if tier == 2 else 4)
        place(b, ch / np.abs(ch).max(), 0.1, [0, 0, 0.32, 0.45][tier])
    if tier == 3:  # final impact boom with long tail
        bt = 1.38 + 0.04 * v
        place(b, boom(rng, 1.0), bt, 1.3)
        ground_crack(rng, b, bt, 1.0)
    out = reverb(b, rt=[0.45, 0.6, 0.8, 1.3][tier], wet=[0.18, 0.2, 0.22, 0.28][tier], seed=50 + tier * 3 + v,
                 bright=5000)
    return finalize(out, IMPACT, maxdur=dur, fade_out=0.08)


def raise_denied():
    rng = np.random.default_rng(6000)
    d = 0.2
    t = tt(d)
    f = 250 * (1 - 0.18 * np.minimum(t / 0.12, 1))
    ph = phase_of(f, len(t))
    tone = np.sin(ph) + 0.3 * np.sin(2 * ph) + 0.12 * np.sin(3 * ph)
    tone *= envp(t, [(0, 0), (0.006, 1), (0.04, 0.6), (0.17, 0)], 1.5)
    tone = lp(tone, 900, 2)
    knock = lp(noise(rng, d) * np.exp(-t / 0.008), 1200, 2)
    x = tone / np.abs(tone).max() + 0.25 * knock / np.abs(knock).max()
    out = reverb(x, rt=0.2, wet=0.08, seed=60)
    return finalize(out, UI, maxdur=0.2, fade_out=0.03)


def soulbar_full():
    rng = np.random.default_rng(7000)
    d = 0.7
    b = buf(1.3)
    for k, s in enumerate([0, 4, 7, 12, 16, 19]):  # rising arpeggio
        place(b, chime(rng, 523.25 * 2 ** (s / 12), 0.5, tau=0.18, amps=(1, 0.15, 0.2, 0.05)),
              0.03 + 0.06 * k, 0.35 + 0.08 * k)
    t = tt(d)
    pad = np.zeros((2, len(t)))
    for f in [1046.5, 1318.5, 1568.0, 2093.0]:
        gl = 2 ** (np.minimum(t / 0.5, 1) * 2 / 12)  # glide up 2 semitones
        ph = rng.uniform(0, 6.28)
        for c in range(2):
            pad[c] += np.sin(phase_of(f * gl * (1 + (c - 0.5) * 0.0006), len(t)) + ph)
    pad *= (0.7 + 0.3 * np.sin(2 * np.pi * 11 * t)) * envp(t, [(0, 0), (0.45, 1), (d, 0)], 1.8)
    place(b, pad / np.abs(pad).max(), 0, 0.35)
    sp = hp(np.vstack([noise(rng, d), noise(rng, d)]), 6000, 2) * envp(t, [(0, 0), (0.45, 1), (d, 0)], 2)
    place(b, sp / np.abs(sp).max(), 0, 0.12)
    b = lp(b, 10000, 2)
    out = reverb(b, rt=0.8, wet=0.25, seed=70, bright=8000)
    return finalize(out, UI, maxdur=0.7, fade_out=0.12)


def undead_crumble(v):
    rng = np.random.default_rng(8000 + v)
    b = buf(1.5)
    place(b, low_thump(rng, 120, 55, 0.04), 0, 0.6)
    # initial collapse burst
    for _ in range(30 + 6 * v):
        place(b, bone_click(rng, 900, 5000), rng.exponential(0.035), rng.uniform(0.3, 0.9),
              pan=rng.uniform(-0.5, 0.5))
    # individual bones bouncing and settling (geometric bounce intervals, consistent pitch per bone)
    for _ in range(7 + v):
        seed = int(rng.integers(1 << 30))
        t, T, a = rng.uniform(0.02, 0.18), rng.uniform(0.07, 0.14), rng.uniform(0.5, 0.9)
        r, pan = rng.uniform(0.55, 0.72), rng.uniform(-0.6, 0.6)
        lo = rng.uniform(1000, 3500)
        while a > 0.04 and t < 0.8:
            place(b, bone_click(np.random.default_rng(seed), lo, lo * 1.15), t, a, pan=pan)
            t += T
            T *= r
            a *= rng.uniform(0.5, 0.7)
    skitter(rng, b, 0.1, 0.75, lambda t: 60 * np.exp(-t / 0.2), amp=0.25)
    # dusty exhale (formant noise, darkening)
    ed = 0.7
    t = tt(ed)
    ex = tv_filter(noise(rng, ed), lambda tc, f: (formant_H([(900, 250, 1), (2200, 400, 0.6)], 0.05)(f)
                                                  / np.sqrt(1 + (f / (5000 * np.exp(-tc / 0.35) + 1200)) ** 4)))
    ex *= envp(t, [(0, 0), (0.08, 1), (ed, 0)], 1.5)
    place(b, np.vstack([ex, np.roll(ex, N(0.011))]) / np.abs(ex).max(), 0.03, 0.22)
    # faint soul-release whoosh
    place(b, whoosh(rng, 0.5, 700, 3500 + 500 * v, peak=0.6, bw=0.6), 0.18, 0.12)
    out = reverb(b, rt=0.5, wet=0.2, seed=80 + v)
    return finalize(out, IMPACT, maxdur=0.9, fade_out=0.08)


def tag_mark(v):
    rng = np.random.default_rng(9000 + v)
    b = buf(0.4)
    tick = hp(noise(rng, 0.01) * np.exp(-tt(0.01) / 0.0012), 3000)
    place(b, tick / np.abs(tick).max(), 0, 0.4)
    place(b, chime(rng, [1660, 1480][v], 0.2, tau=0.06, ratios=(1, 2.43), amps=(1, 0.25)), 0.002, 0.8)
    b = lp(b, 9000, 2)
    out = reverb(b, rt=0.2, wet=0.12, seed=90 + v)
    return finalize(out, QUIET, maxdur=0.15, fade_out=0.03)


def main():
    for v in range(4):
        write(f"A_SoulWhip_Crack_{v + 1:02d}", soul_whip_crack(v))
    for v in range(2):
        write(f"A_SoulWhip_Equip_{v + 1:02d}", soul_whip_equip(v))
    for v in range(3):
        write(f"A_SoulWhip_Tag_{v + 1:02d}", soul_whip_tag(v))
    for i in range(8):
        write(f"A_Soul_Pickup_{i + 1:02d}", soul_pickup(i))
    for tier, nm in enumerate(["Fodder", "Medium", "Hard", "Elite"]):
        for v in range(2):
            write(f"A_Necro_Raise_{nm}_{v + 1:02d}", necro_raise(tier, v))
    write("A_Necro_Raise_Denied_01", raise_denied())
    write("A_Necro_SoulBar_Full_01", soulbar_full())
    for v in range(3):
        write(f"A_Undead_Crumble_{v + 1:02d}", undead_crumble(v))
    for v in range(2):
        write(f"A_Necro_TagMark_{v + 1:02d}", tag_mark(v))


if __name__ == "__main__":
    main()
