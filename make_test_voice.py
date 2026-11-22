"""Synthetic test vocal (formant synthesis): rap section + sung phrase in C minor, plus a 95 BPM beat.
Not a real voice - only a repeatable signal for judging the plugin's character."""
import sys, numpy as np
from scipy.signal import lfilter, butter
from scipy.io import wavfile

FS = 48000
BPM = 95.0
BEAT = 60.0 / BPM
rng = np.random.default_rng(3)

VOWELS = {  # F1..F5 (Hz), male
    'a': [730, 1090, 2440, 3400, 4500], 'o': [570, 840, 2410, 3300, 4400], 'u': [300, 870, 2240, 3300, 4400],
    'e': [530, 1840, 2480, 3450, 4500], 'i': [300, 2200, 2950, 3500, 4600], 'y': [400, 1550, 2450, 3400, 4500],
    'm': [250, 1100, 2300, 3300, 4400],
}
BW = [70, 90, 130, 180, 250]

def glottal(f0, amp):
    n = len(f0)
    ph = np.cumsum(f0 / FS) % 1.0
    Tp, Tn = 0.42, 0.16
    g = np.where(ph < Tp, 0.5 * (1 - np.cos(np.pi * ph / Tp)),
         np.where(ph < Tp + Tn, np.cos(np.pi * (ph - Tp) / (2 * Tn)), 0.0))
    dg = np.diff(g, prepend=0.0) * FS / 300.0
    asp = rng.normal(0, 1, n) * (g > 0.05) * 0.04
    return (dg + asp) * amp

def formant_filter(x, ftrack):
    y = x.copy()
    block = 32
    for k in range(5):
        out = np.zeros_like(y); z1 = z2 = 0.0
        for s in range(0, len(y), block):
            F = ftrack[s, k]; r = np.exp(-np.pi * BW[k] / FS); th = 2 * np.pi * F / FS
            a1, a2 = -2 * r * np.cos(th), r * r; g = 1 + a1 + a2
            seg = y[s:s + block]
            o, zf = lfilter([g], [1, a1, a2], seg, zi=[z1, z2] if s else lfilter_zi_zero())
            z1, z2 = zf; out[s:s + block] = o
        y = out
    return y

def lfilter_zi_zero():
    return [0.0, 0.0]

def noise_band(n, lo, hi):
    b, a = butter(2, [lo / (FS / 2), min(hi / (FS / 2), 0.99)], btype='band')
    return lfilter(b, a, rng.normal(0, 1, n))

def env_adsr(n, a, r):
    e = np.ones(n); na = max(1, int(a * FS)); nr = max(1, int(r * FS))
    e[:na] = np.linspace(0, 1, na); e[-nr:] *= np.linspace(1, 0, nr); return e

def build():
    events = []  # (start_s, dur_s, cons, vowel, f0_start, f0_end, amp, vibrato)
    t = 0.25
    six = BEAT / 4
    sylls = ["so", "ma", "la", "ta", "ki", "sha", "na", "du", "vo", "ry", "te", "ma", "shu", "ka", "lo", "mi"]
    for ph in range(4):
        base = 138 - ph * 3
        for i in range(8):
            s = sylls[(ph * 8 + i) % len(sylls)]
            cons, vow = (s[:-1], s[-1]) if len(s) > 1 else ('', s)
            prog = i / 7.0
            f0 = base * (1.0 - 0.22 * prog) * (1.08 if i in (0, 4) else 1.0) * (1 + rng.normal(0, 0.02))
            events.append((t, six * 0.92, cons, vow, f0, f0 * 0.96, 0.9 if i in (0, 4) else 0.7, 0.0))
            t += six
        t += 2 * six
    t += BEAT * 0.5
    melody = [(196.00, 2, 'la'), (207.65, 1, 'mo'), (196.00, 1, 'a'), (174.61, 2, 've'), (155.56, 2, 'o'),
              (146.83, 1, 'na'), (155.56, 1, 'e'), (130.81, 4, 'a')]
    for f, beats, s in melody:
        cons, vow = (s[:-1], s[-1]) if len(s) > 1 else ('', s)
        events.append((t, beats * BEAT * 0.97, cons, vow, f, f, 0.85, 1.0))
        t += beats * BEAT
    total = int((t + 0.8) * FS)

    f0 = np.full(total, 120.0); amp = np.zeros(total); ftr = np.tile(np.array(VOWELS['a'], float), (total, 1))
    noise = np.zeros(total)
    prev_f = None
    for (st, du, cons, vow, fa, fb, a, vib) in events:
        s0 = int(st * FS); n = int(du * FS); s1 = min(total, s0 + n)
        idx = np.arange(s1 - s0); tt = idx / FS
        cdur = {'': 0, 's': 0.07, 'sh': 0.08, 't': 0.03, 'k': 0.035, 'm': 0.05, 'n': 0.045, 'l': 0.03,
                'd': 0.025, 'v': 0.04, 'r': 0.03}.get(cons, 0.03)
        ncon = int(cdur * FS)
        track = np.linspace(fa, fb, len(idx))
        if prev_f is not None and vib > 0:
            glide = np.exp(-tt / 0.05); track = track * (1 - glide) + prev_f * glide
        if vib > 0:
            depth = np.clip((tt - 0.3) / 0.4, 0, 1) * 0.017
            track = track * (1 + depth * np.sin(2 * np.pi * 5.5 * tt))
        f0[s0:s1] = track
        e = env_adsr(len(idx), 0.012, 0.05 if vib == 0 else 0.12) * a
        if cons in ('s', 'sh', 't', 'k', 'd'):
            e[:ncon] *= np.linspace(0.0, 0.6, ncon) if cons in ('s', 'sh') else 0.15
        amp[s0:s1] = np.maximum(amp[s0:s1], e)
        # formants: from consonant colour into the vowel
        tgt = np.array(VOWELS[vow], float)
        src = np.array(VOWELS['m' if cons in ('m', 'n') else 'y'], float)
        mix = np.clip(idx / max(1, ncon + int(0.03 * FS)), 0, 1)[:, None]
        ftr[s0:s1] = src * (1 - mix) + tgt * mix
        if cons in ('s', 'sh', 't', 'k', 'd', 'v'):
            band = {'s': (4500, 9500), 'sh': (2200, 5500), 't': (3000, 8000), 'k': (1500, 3500), 'd': (2500, 6000), 'v': (2000, 7000)}[cons]
            nn = noise_band(ncon + int(0.02 * FS), *band)
            ne = np.concatenate([np.linspace(0.2, 1, ncon // 2), np.linspace(1, 0, len(nn) - ncon // 2)])
            g = {'s': 0.22, 'sh': 0.2, 't': 0.25, 'k': 0.22, 'd': 0.12, 'v': 0.06}[cons]
            end = min(total, s0 + len(nn)); noise[s0:end] += (nn * ne * g)[:end - s0]
        prev_f = track[-1]
    jitter = 1 + np.cumsum(rng.normal(0, 0.0004, total)); jitter = jitter / np.convolve(jitter, np.ones(4800) / 4800, mode='same')
    src = glottal(f0 * jitter, amp * (1 + 0.04 * rng.normal(0, 1, total)))
    voice = formant_filter(src, ftr) + noise
    b, a = butter(2, 70 / (FS / 2), btype='high'); voice = lfilter(b, a, voice)
    voice *= 0.35 / np.max(np.abs(voice))   # about -9 dBFS peaks
    return voice.astype(np.float32), t + 0.8

def beat(length):
    n = int(length * FS); L = np.zeros(n); R = np.zeros(n)
    def add(sig, at, pan=0.0):
        s = int(at * FS); e = min(n, s + len(sig)); L[s:e] += sig[:e - s] * (1 - pan) ; R[s:e] += sig[:e - s] * (1 + pan)
    tk = np.arange(int(0.35 * FS)) / FS
    kick = np.sin(2 * np.pi * (45 * tk + 75 * 0.04 * (1 - np.exp(-tk / 0.04)))) * np.exp(-tk / 0.18) * 0.6
    sn_t = np.arange(int(0.2 * FS)) / FS
    snare = (noise_band(len(sn_t), 1500, 8000) * 0.5 + np.sin(2 * np.pi * 190 * sn_t) * 0.3) * np.exp(-sn_t / 0.06) * 0.35
    hat = noise_band(int(0.04 * FS), 7000, 15000) * np.exp(-np.arange(int(0.04 * FS)) / FS / 0.01) * 0.12
    bars = int(length / (4 * BEAT)) + 1
    for b in range(bars):
        t0 = 0.25 + b * 4 * BEAT
        for k in (0, 2.5): add(kick, t0 + k * BEAT)
        for s in (1, 3): add(snare, t0 + s * BEAT)
        for h in range(8): add(hat, t0 + h * BEAT / 2, pan=0.3)
    # pad: Cm - Ab - Bb - Cm  (C minor, matches the harp default)
    chords = [[130.81, 155.56, 196.00, 233.08], [103.83, 130.81, 155.56, 207.65], [116.54, 146.83, 174.61, 233.08], [130.81, 155.56, 196.00, 261.63]]
    pad = np.zeros(n)
    for b in range(bars):
        ch = chords[b % 4]; s = int((0.25 + b * 4 * BEAT) * FS); e = min(n, s + int(4 * BEAT * FS)); tt = np.arange(e - s) / FS
        for f in ch: pad[s:e] += (2 * ((f * tt) % 1.0) - 1) * 0.035
        pad[s:e] *= np.minimum(1, tt / 0.3) * np.minimum(1, (4 * BEAT - tt) / 0.3)
    bb, aa = butter(2, 900 / (FS / 2)); pad = lfilter(bb, aa, pad)
    bass = np.zeros(n)
    for b in range(bars):
        f = [65.41, 51.91, 58.27, 65.41][b % 4]; s = int((0.25 + b * 4 * BEAT) * FS); e = min(n, s + int(4 * BEAT * FS)); tt = np.arange(e - s) / FS
        bass[s:e] = np.sin(2 * np.pi * f * tt) * 0.25 * np.minimum(1, tt / 0.02) * np.exp(-tt / 1.5)
    L += pad + bass; R += pad + bass
    return np.stack([L, R], 1).astype(np.float32)

if __name__ == '__main__':
    out = sys.argv[1] if len(sys.argv) > 1 else '.'
    v, length = build()
    wavfile.write(f'{out}/voice_dry.wav', FS, v)
    wavfile.write(f'{out}/beat.wav', FS, beat(len(v) / FS + 2.5))
    print('voice', len(v) / FS, 's')
