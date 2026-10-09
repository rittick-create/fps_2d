"""Regenerate Grid Breach's original synthesized PCM effects (stdlib only)."""
import math
from pathlib import Path
import random
import struct
import wave

RATE = 22050
OUT = Path(__file__).resolve().parent
TAU = math.tau


def write(name, duration, synth, peak):
    rng = random.Random(name)
    frames = []
    low = 0.0
    count = int(duration * RATE)
    for i in range(count):
        t = i / RATE
        noise = rng.uniform(-1, 1)
        low += 0.15 * (noise - low)
        # Short fade-in and fade-out prevent discontinuity clicks.
        fade = min(1.0, t / 0.0015, (duration - t) / 0.014)
        frames.append(synth(t, noise, low) * max(0.0, fade))
    scale = peak / max(0.001, max(abs(value) for value in frames))
    payload = b"".join(struct.pack("<h", round(value * scale * 32767)) for value in frames)
    with wave.open(str(OUT / (name + ".wav")), "wb") as wav:
        wav.setnchannels(1)
        wav.setsampwidth(2)
        wav.setframerate(RATE)
        wav.writeframes(payload)


def note(t, start, frequency, length, amplitude=1):
    t -= start
    if not 0 <= t < length:
        return 0.0
    envelope = min(1, t / 0.008) * (1 - t / length) ** 2
    return amplitude * envelope * (math.sin(TAU * frequency * t)
                                   + 0.16 * math.sin(TAU * frequency * 2 * t))


write("rifle", 0.16, lambda t, n, l:
      0.75 * n * math.exp(-t * 70)
      + 0.5 * math.sin(TAU * (145 * t - 140 * t * t)) * math.exp(-t * 27), 0.48)
write("shotgun", 0.32, lambda t, n, l:
      n * math.exp(-t * 27) + 1.2 * l * math.exp(-t * 13)
      + 0.45 * math.sin(TAU * (83 * t - 65 * t * t)) * math.exp(-t * 12), 0.65)
write("hit", 0.08, lambda t, n, l:
      (0.45 * n + 0.7 * math.sin(TAU * 970 * t)) * math.exp(-t * 68), 0.23)
write("explosion", 0.58, lambda t, n, l:
      1.9 * l * math.exp(-t * 7) + 0.5 * n * math.exp(-t * 27)
      + 0.52 * math.sin(TAU * (69 * t - 30 * t * t)) * math.exp(-t * 8), 0.67)
write("hurt", 0.23, lambda t, n, l:
      (math.sin(TAU * 155 * t) + 0.5 * math.sin(TAU * 173 * t)
       + 0.4 * l) * math.exp(-t * 16), 0.32)
write("pickup", 0.40, lambda t, n, l:
      note(t, 0, 660, 0.24) + note(t, 0.10, 880, 0.28, 0.8), 0.28)
write("reload", 0.44, lambda t, n, l:
      sum((0.55 * n + 0.2 * math.sin(TAU * f * t)) * math.exp(-(t - s) * 65)
          for s, f in ((0, 820), (0.23, 510), (0.32, 1050)) if t >= s), 0.30)
write("wave", 0.66, lambda t, n, l:
      note(t, 0, 220, 0.30) + note(t, 0.13, 330, 0.32)
      + note(t, 0.26, 440, 0.38), 0.32)
write("victory", 1.45, lambda t, n, l:
      sum(note(t, s, f, length, a) for s, f, length, a in
          ((0, 330, 0.30, 1), (0.16, 440, 0.30, 1), (0.32, 550, 0.30, 1),
           (0.50, 660, 0.85, 0.8), (0.50, 440, 0.85, 0.5),
           (0.50, 550, 0.85, 0.5))), 0.36)
write("defeat", 0.95, lambda t, n, l:
      note(t, 0, 330, 0.4) + note(t, 0.21, 277.18, 0.42)
      + note(t, 0.42, 220, 0.5), 0.32)
