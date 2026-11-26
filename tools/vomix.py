"""Place the 6 voice-over lines, duck the music under them, make the voice prominent. Writes soundtrack_v5.wav + subs.txt."""
import numpy as np
from scipy.io import wavfile
from scipy.signal import lfilter
SR = 48000
_, vo = wavfile.read('vo_src.wav'); vo = vo.astype(np.float64)
_, mus = wavfile.read('soundtrack_v4.wav')
mus = mus.astype(np.float64) / (2**31 if mus.dtype == np.int32 else 1.0)
if mus.dtype != np.float64 or np.max(np.abs(mus)) > 4: mus = mus / np.max(np.abs(mus))
# the 6 lines in the ElevenLabs file (split on the longer pauses) -> where they start in the video (150 BPM, bar 1.6 s)
lines = [  # src start, src end, video start, subtitle
    (0.18, 2.70, 0.25, "Ready for a little... euphoria?"),
    (3.18, 5.62, 6.45, "Make it wide. Give it warmth."),
    (6.00, 9.24, 9.65, "A huge hall... that breathes with your track."),
    (9.52, 13.26, 13.25, "Add punch. Add sparkle. Ascend!"),
    (13.64, 17.34, 32.15, "Bright... or dark. Shape your own magic."),
    (17.70, 20.11, 36.20, "EUPHORIA. Turn the knob... cast the spell."),
]
KB1, KA1 = [1.53512485958697, -2.69169618940638, 1.19839281085285], [1.0, -1.69065929318241, 0.73248077421585]
KB2, KA2 = [1.0, -2.0, 1.0], [1.0, -1.99004745483398, 0.99007225036621]
kw = lambda x: lfilter(KB2, KA2, lfilter(KB1, KA1, x, axis=0), axis=0)
loud = lambda x: -0.691 + 10 * np.log10(np.mean(np.atleast_2d(kw(x).T).T ** 2, axis=0).sum() + 1e-20)

N = len(mus)
voice = np.zeros(N); mask = np.zeros(N)
fade = int(0.015 * SR)
subs = []
for s0, s1, v0, text in lines:
    seg = vo[int(s0 * SR) - int(0.05 * SR):int(s1 * SR) + int(0.12 * SR)].copy()
    seg[:fade] *= np.linspace(0, 1, fade); seg[-fade:] *= np.linspace(1, 0, fade)
    # gentle compression: even out syllables (RMS envelope, 3:1 above -24 dBFS)
    env = np.sqrt(np.convolve(seg**2, np.ones(int(0.01 * SR)) / int(0.01 * SR), mode='same')) + 1e-9
    lvl = 20 * np.log10(env); gr = np.where(lvl > -24, -(lvl + 24) * (1 - 1 / 3), 0)
    gr = np.convolve(gr, np.ones(int(0.005 * SR)) / int(0.005 * SR), mode='same')
    seg *= 10 ** (gr / 20)
    seg *= 10 ** ((-11.0 - loud(seg)) / 20)          # voice at about -11 LUFS: clearly above the music
    a = int((v0 - 0.05) * SR); voice[a:a + len(seg)] += seg; mask[a:a + len(seg)] = 1
    subs.append((v0, v0 + (s1 - s0) + 0.25, text))
# duck the music 7 dB while she talks (80 ms down, 350 ms back up)
duck = np.ones(N); g = 1.0
att, rel = 1 - np.exp(-1 / (0.08 * SR)), 1 - np.exp(-1 / (0.35 * SR))
tgt = np.where(mask > 0, 10 ** (-7 / 20), 1.0)
for i in range(N):
    g += (att if tgt[i] < g else rel) * (tgt[i] - g); duck[i] = g
out = mus * duck[:, None] + voice[:, None] * np.array([1.0, 1.0])
out *= 10 ** (-1.0 / 20) / np.max(np.abs(out))
wavfile.write('soundtrack_v5.wav', SR, out.astype(np.float32))
with open('subs.txt', 'w') as f:
    for a, b, t in subs: f.write(f"{a:.2f}\t{b:.2f}\t{t}\n")
print('music loudness', round(loud(mus), 1), '| voice lines placed:')
for a, b, t in subs: print(f"  {a:5.2f}-{b:5.2f}  {t}")
bar = 1.6
print('A/B (19.2-32.0 s) untouched:', not mask[int(19.2 * SR):int(32.0 * SR)].any(), '| DRY ref (3.2-6.4 s) untouched:', not mask[int(3.2 * SR):int(6.4 * SR)].any())
