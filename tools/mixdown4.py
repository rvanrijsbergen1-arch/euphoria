"""Mix the EUPHORIA promo v3 soundtrack from the real stems and check it.

lead_euphoria.wav = the dry lead after the real EUPHORIA processor (bypass, knob, tone and Spellbook automated as in the video).
Per stage, one constant gain on the lead bus matches its perceived loudness (BS.1770 K-weighted) to the DRY bars,
so the comparison is level-matched. Kick and pad are identical everywhere and sit below the lead.
Usage: python3 mixdown.py <dir>
"""
import sys, wave
import numpy as np

D = sys.argv[1]
SR = 48000
BAR = 1.6


def rd(name):
    w = wave.open(f"{D}/{name}")
    n = w.getnframes()
    a = np.frombuffer(w.readframes(n), dtype=np.uint8).reshape(-1, 3)
    x = a[:, 0].astype(np.int32) | (a[:, 1].astype(np.int32) << 8) | (a[:, 2].astype(np.int32) << 16)
    x = np.where(x >= 2**23, x - 2**24, x).astype(np.float64) / 2**23
    return x.reshape(-1, 2)


def wr(name, x):
    y = np.clip(np.round(x * (2**23 - 1)), -(2**23), 2**23 - 1).astype(np.int32)
    b = np.zeros((y.size, 3), dtype=np.uint8)
    f = y.reshape(-1)
    b[:, 0] = f & 255
    b[:, 1] = (f >> 8) & 255
    b[:, 2] = (f >> 16) & 255
    w = wave.open(f"{D}/{name}", "wb")
    w.setnchannels(2)
    w.setsampwidth(3)
    w.setframerate(SR)
    w.writeframes(b.tobytes())
    w.close()


def biquad(x, b, a):
    y = np.zeros_like(x)
    z1 = np.zeros(x.shape[1]); z2 = np.zeros(x.shape[1])
    for i in range(len(x)):
        xi = x[i]
        yi = b[0] * xi + z1
        z1 = b[1] * xi - a[1] * yi + z2
        z2 = b[2] * xi - a[2] * yi
        y[i] = yi
    return y


# BS.1770 K-weighting at 48 kHz
KB1, KA1 = [1.53512485958697, -2.69169618940638, 1.19839281085285], [1.0, -1.69065929318241, 0.73248077421585]
KB2, KA2 = [1.0, -2.0, 1.0], [1.0, -1.99004745483398, 0.99007225036621]


def kweight(x):
    try:
        from scipy.signal import lfilter
        return lfilter(KB2, KA2, lfilter(KB1, KA1, x, axis=0), axis=0)
    except ImportError:
        return biquad(biquad(x, KB1, KA1), KB2, KA2)


def loud(xk, t0, t1):
    s = xk[int(t0 * SR):int(t1 * SR)]
    return -0.691 + 10 * np.log10(np.sum(np.mean(s**2, axis=0)) + 1e-20)


lead, dry, pad, kick = rd("lead_euphoria.wav"), rd("lead_dry.wav"), rd("pad.wav"), rd("kick.wav")
# balance: chords about 6 dB and claps about 7 dB under the dry lead (perceived), so the lead and its processing are in front
_b = lambda n: n * BAR
_ref = loud(kweight(dry), _b(2) + 0.35, _b(4))
CH = 10 ** ((_ref - 6.0 - loud(kweight(pad), _b(2) + 0.35, _b(4))) / 20)
CL = 10 ** ((_ref - 7.0 - loud(kweight(kick), _b(2) + 0.35, _b(4))) / 20)
print(f"chords gain {20*np.log10(CH):+.1f} dB, claps gain {20*np.log10(CL):+.1f} dB")
lk = kweight(lead)
b = lambda n: n * BAR

ref = loud(lk, b(2) + 0.35, b(4))          # DRY bars (bypassed)
stages = {                                  # segment -> where its loudness is measured
    "hook 100%": ((b(1), b(2)), (b(8) + 0.35, b(10))),
    "30%": ((b(4), b(6)), (b(4) + 0.35, b(6))),
    "70%": ((b(6), b(8)), (b(6) + 0.35, b(8))),
    "100%": ((b(8), b(12)), (b(8) + 0.35, b(10))),
    "A/B EUPH 1": ((b(14), b(16)), (b(8) + 0.35, b(10))),
    "A/B EUPH 2": ((b(18), b(20)), (b(8) + 0.35, b(10))),
    "dark+book": ((b(20), b(22)), (b(20) + 0.35, b(22))),
    "hero": ((b(22), 40.0), (b(8) + 0.35, b(10))),
}
gain = np.ones(len(lead))
print(f"reference (DRY) lead loudness {ref:.2f} LUFS-weighted")
for name, ((s0, s1), (m0, m1)) in stages.items():
    g_db = ref - loud(lk, m0, m1)
    gain[int(s0 * SR):int(s1 * SR)] = 10 ** (g_db / 20)
    print(f"  {name:12s} correction {g_db:+.2f} dB")
# 20 ms ramps between segments (same moment as the plugin's own bypass crossfade)
k = int(0.02 * SR)
gain = np.convolve(np.pad(gain, (k, k), mode="edge"), np.ones(k) / k, mode="same")[k:-k]
lead_m = lead * gain[:, None]

mix = 1.0 * lead_m + CH * pad + CL * kick
mix *= 10 ** (-1.0 / 20) / np.max(np.abs(mix))
fade = int(0.6 * SR)
mix[-fade:] *= np.linspace(1, 0, fade)[:, None]
wr("soundtrack_v4.wav", mix)

# report: lead vs whole mix, matched loudness per state, stereo / mono
mk = kweight(mix)
print("\nsegment            lead-in-mix  mix loud   side/mid   mono loss  corr")
for name, (t0, t1) in {"DRY": (b(2), b(4)), "30%": (b(4), b(6)), "70%": (b(6), b(8)), "100%": (b(8), b(10)),
                       "A/B DRY": (b(12), b(14)), "A/B EUPH": (b(14), b(16)), "A/B DRY 2": (b(16), b(18)), "A/B EUPH 2": (b(18), b(20))}.items():
    t0 += 0.35
    s = mix[int(t0 * SR):int(t1 * SR)]
    M, S = (s[:, 0] + s[:, 1]) / 2, (s[:, 0] - s[:, 1]) / 2
    e = lambda v: 10 * np.log10(np.mean(v**2) + 1e-20)
    corr = np.corrcoef(s[:, 0], s[:, 1])[0, 1]
    print(f"{name:18s} {loud(kweight(lead_m), t0, t1) - loud(mk, t0, t1):+8.2f} dB {loud(mk, t0, t1):8.2f} {e(S) - e(M):9.1f} dB {e(M) - e(s.reshape(-1)):8.2f} dB {corr:6.2f}")
print(f"\nintegrated (ungated) {loud(mk, 0, 40):.1f} LUFS, peak {20*np.log10(np.max(np.abs(mix))):.1f} dBFS")
