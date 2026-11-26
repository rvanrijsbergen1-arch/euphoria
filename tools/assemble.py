"""Build the 40 s input stems for the promo from the user's own stems (150 BPM, bar = 1.6 s).
Comparisons use one fixed 2-bar excerpt (source bars 0-1) so every stage plays exactly the same notes and chords."""
import numpy as np
from scipy.io import wavfile
class sf:
    @staticmethod
    def read(f):
        sr, x = wavfile.read(f); return x.astype(np.float64), sr
    @staticmethod
    def write(f, x, sr, subtype=None):
        wavfile.write(f, sr, x)
SR = 48000; BAR = 1.6; N = int(40 * SR)
lead, _ = sf.read('src_lead.wav'); chords, _ = sf.read('src_chords.wav'); claps, _ = sf.read('src_claps.wav')
def bars(x, b0, nb):
    s = int(round(b0 * BAR * SR)); return x[s:s + int(round(nb * BAR * SR))]
# video bar -> source bar (2-bar blocks)
plan = { 0: 0, 2: 0, 4: 0, 6: 0, 8: 0, 10: 2, 12: 0, 14: 0, 16: 0, 18: 0, 20: 0, 22: 14 }
L = np.zeros((N, 2)); C = np.zeros((N, 2)); K = np.zeros((N, 2))
for vb, sb in plan.items():
    s = int(round(vb * BAR * SR))
    seg_l, seg_c = bars(lead, sb, 2), bars(chords, sb, 2)
    L[s:s + len(seg_l)] += seg_l
    C[s:s + len(seg_c)] += seg_c
# claps loop under everything until the end of bar 24 (then only the hall rings)
for s in range(0, int(24 * BAR * SR), len(claps)):
    seg = claps[: min(len(claps), int(24 * BAR * SR) - s)]
    K[s:s + len(seg)] += seg
for name, x in (('in_lead.wav', L), ('in_chords.wav', C), ('in_claps.wav', K)):
    sf.write(name, x.astype(np.float32), SR, subtype='FLOAT')
print('ok', N / SR)
