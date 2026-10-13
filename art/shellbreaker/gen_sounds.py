#!/usr/bin/env python3
"""Shellbreaker's sounds, synthesized (no samples): writes sfx/*.wav, mono 16-bit 32 kHz, which bake_pack.py puts in
shell.pack as raw PCM (.pcm, the mixer's own format).

    uv run --no-project --with numpy python gen_sounds.py

  A_Shell_Swing_0N    a swing: a blade whoosh (band-passed noise sweeping up then down) over a chain rattle
  A_Shell_Circle_01   the overhead circle: a longer, rounder whoosh that pans by itself
  A_Shell_Throw_0N    the hook thrown: chain paying out fast (a run of link clicks, speeding up)
  A_Shell_Bite_0N     the hook catching: a metallic thunk with a ring
  A_Shell_Equip_01    picking it up: a short chain jingle and a blade ring
"""
import os
import wave

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "sfx")
RATE = 32000


def write(name, x):
    x = np.asarray(x, np.float64)
    peak = np.max(np.abs(x)) or 1.0
    x = x / peak * 0.85
    fade = min(len(x), 160)
    x[-fade:] *= np.linspace(1, 0, fade)
    os.makedirs(OUT, exist_ok=True)
    with wave.open(os.path.join(OUT, name + ".wav"), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes((np.clip(x, -1, 1) * 32767).astype("<i2").tobytes())


def t_of(seconds):
    return np.arange(int(seconds * RATE)) / RATE


def bandpass(x, centre, q):
    """A resonant band-pass (state-variable filter) whose centre frequency may change per sample."""
    centre = np.broadcast_to(np.asarray(centre, np.float64), x.shape)
    low = band = 0.0
    out = np.empty_like(x)
    damp = 1.0 / q
    for i, s in enumerate(x):
        f = 2 * np.sin(np.pi * min(centre[i], RATE / 6) / RATE)
        low += f * band
        high = s - low - damp * band
        band += f * high
        out[i] = band
    return out


def click(rng, length=0.012, pitch=None):
    """One link knocking on the next: a short burst of metallic partials."""
    t = t_of(length)
    pitch = pitch or rng.uniform(2600, 4200)
    partials = sum(np.sin(2 * np.pi * pitch * r * t + rng.uniform(0, 6)) * a
                   for r, a in ((1, 1), (2.76, 0.6), (5.4, 0.35)))
    return partials * np.exp(-t * rng.uniform(260, 420))


def place(out, sound, at, gain=1.0):
    i = int(at * RATE)
    n = min(len(sound), len(out) - i)
    if n > 0:
        out[i:i + n] += sound[:n] * gain


def swing(rng, seconds=0.3):
    t = t_of(seconds)
    u = t / seconds
    env = np.sin(np.pi * np.clip(u, 0, 1)) ** 1.6
    centre = 500 + 2600 * np.sin(np.pi * u) ** 2 * rng.uniform(0.85, 1.15)
    whoosh = bandpass(rng.standard_normal(len(t)), centre, 2.2) * env
    rattle = np.zeros(len(t))
    for k in range(rng.integers(5, 9)):
        place(rattle, click(rng), rng.uniform(0.0, seconds * 0.8), rng.uniform(0.15, 0.35))
    return whoosh + rattle


def circle(rng, seconds=0.55):
    t = t_of(seconds)
    u = t / seconds
    env = np.sin(np.pi * u) ** 1.2 * (0.75 + 0.25 * np.sin(2 * np.pi * 2 * u))
    centre = 700 + 1500 * np.sin(np.pi * u) + 500 * np.sin(2 * np.pi * 2 * u)
    return bandpass(rng.standard_normal(len(t)), centre, 1.8) * env


def throw(rng, seconds=0.45):
    out = np.zeros(int(seconds * RATE))
    at, gap = 0.0, 0.03
    while at < seconds - 0.02:
        place(out, click(rng, pitch=rng.uniform(2000, 3400)), at, rng.uniform(0.5, 1.0) * (1 - at / seconds * 0.6))
        at += gap * rng.uniform(0.7, 1.3)
        gap = max(0.009, gap * 0.9)  # the chain pays out faster and faster
    t = t_of(seconds)
    out += bandpass(rng.standard_normal(len(t)), 1800, 3) * np.exp(-t * 4) * 0.3
    return out


def bite(rng, seconds=0.5):
    t = t_of(seconds)
    thunk = np.sin(2 * np.pi * (140 * t - 60 * t * t)) * np.exp(-t * 28)
    knock = bandpass(rng.standard_normal(len(t)), 900, 4) * np.exp(-t * 60) * 0.8
    base = rng.uniform(1700, 2100)
    ring = sum(np.sin(2 * np.pi * base * r * t) * a * np.exp(-t * d)
               for r, a, d in ((1, 0.35, 9), (2.32, 0.2, 13), (4.25, 0.12, 18)))
    return thunk + knock + ring


def equip(rng, seconds=0.6):
    out = np.zeros(int(seconds * RATE))
    for k in range(9):
        place(out, click(rng, 0.02), 0.02 + k * 0.03 + rng.uniform(0, 0.015), 0.5)
    t = t_of(seconds)
    ring = sum(np.sin(2 * np.pi * 2900 * r * t) * a for r, a in ((1, 0.3), (2.1, 0.15))) * np.exp(-(t - 0.3).clip(0) * 7)
    ring[t < 0.3] = 0
    return out + ring


def main():
    rng = np.random.default_rng(7)
    for i in range(4):
        write(f"A_Shell_Swing_{i + 1:02d}", swing(rng, rng.uniform(0.26, 0.34)))
    write("A_Shell_Circle_01", circle(rng))
    for i in range(2):
        write(f"A_Shell_Throw_{i + 1:02d}", throw(rng))
        write(f"A_Shell_Bite_{i + 1:02d}", bite(rng))
    write("A_Shell_Equip_01", equip(rng))
    print("wrote", len(os.listdir(OUT)), "sounds to", os.path.relpath(OUT))


if __name__ == "__main__":
    main()
