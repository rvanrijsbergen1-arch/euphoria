#pragma once
// EUPHORIA audio engine: one knob (amount 0..1) drives eleven parameters.
// Chain: saturation (ADAA tanh) -> chorus -> unison doubler -> stereo width (mono below 150 Hz)
//        -> FDN hall reverb (pre-delay, 300 Hz low cut, tempo-synced ducking) -> 3-band OTT -> 10 kHz high shelf -> auto gain.
// Tone switch: Bright (damping 12 kHz, shelf max +4 dB) or Dark (damping 5 kHz, shelf max +1 dB, warmer saturation).
// No JUCE dependency, no allocation in process().
#include <vector>
#include <array>
#include <cmath>

namespace eup
{
struct Mapping
{
    float width = 100.0f;     // %
    float chorusMix = 0.0f;   // 0..1
    float driveDb = 0.0f;
    float reverbMix = 0.0f;   // 0..1
    float decaySec = 1.5f;
    float preDelayMs = 20.0f;
    float duckDb = 0.0f;
    float ottDepth = 0.0f;    // 0..1
    float shelfDb = 0.0f;
    float doublerMix = 0.0f;  // 0..1
    float autoGainDb = 0.0f;
    float dampingHz = 12000.0f;

    static Mapping forAmount (float amount, bool bright);
};

struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    float z1[2] {}, z2[2] {};
    void reset() { z1[0] = z1[1] = z2[0] = z2[1] = 0; }
    void setHighShelf (double sr, double f, double gainDb, double q = 0.707);
    void setLowPass (double sr, double f, double q = 0.707);
    void setHighPass (double sr, double f, double q = 0.707);
    void setAllPass (double sr, double f, double q = 0.707);
    inline float process (float x, int ch)
    {
        const float y = b0 * x + z1[ch];
        z1[ch] = b1 * x - a1 * y + z2[ch];
        z2[ch] = b2 * x - a2 * y;
        return y;
    }
};

/** Linkwitz-Riley 4th order split (two cascaded Butterworth biquads per side). */
struct LR4
{
    Biquad lp1, lp2, hp1, hp2;
    void set (double sr, double f) { lp1.setLowPass (sr, f); lp2.setLowPass (sr, f); hp1.setHighPass (sr, f); hp2.setHighPass (sr, f); }
    void reset() { lp1.reset(); lp2.reset(); hp1.reset(); hp2.reset(); }
    inline void split (float x, int ch, float& lo, float& hi)
    {
        lo = lp2.process (lp1.process (x, ch), ch);
        hi = hp2.process (hp1.process (x, ch), ch);
    }
};

/** LR4 allpass at the same crossover (lo + hi of an LR4 split), to keep bands phase-aligned. */
struct LR4AllPass
{
    Biquad ap;
    void set (double sr, double f) { ap.setAllPass (sr, f, 0.5); }
    void reset() { ap.reset(); }
    inline float process (float x, int ch) { return ap.process (x, ch); }
};

class DelayLine
{
public:
    void setMax (int n) { buf.assign ((size_t) n + 4, 0.0f); w = 0; }
    void clear() { std::fill (buf.begin(), buf.end(), 0.0f); w = 0; }
    inline void push (float x) { buf[(size_t) w] = x; if (++w >= (int) buf.size()) w = 0; }
    /** delay in samples (>= 1), linear interpolation */
    inline float read (float d) const
    {
        const int n = (int) buf.size();
        float pos = (float) w - d;
        while (pos < 0) pos += (float) n;
        const int i0 = (int) pos;
        const float fr = pos - (float) i0;
        const int i1 = (i0 + 1) % n;
        return buf[(size_t) (i0 % n)] * (1.0f - fr) + buf[(size_t) i1] * fr;
    }
    inline float readInt (int d) const
    {
        int i = w - d;
        const int n = (int) buf.size();
        while (i < 0) i += n;
        return buf[(size_t) i];
    }
private:
    std::vector<float> buf;
    int w = 0;
};

class Engine
{
public:
    void prepare (double sampleRate, int maxBlock);
    void reset();

    void setAmount (float a01) { targetAmount = a01 < 0 ? 0 : (a01 > 1 ? 1 : a01); }
    void setBright (bool b) { bright = b; }
    void setBypassed (bool b) { bypassed = b; }
    /** Host tempo for reverb ducking. ppq < 0 = not playing (the engine runs its own beat clock). */
    void setTempo (double bpm, double ppqAtBlockStart, bool playing);

    void process (float* left, float* right, int numSamples);

    Mapping current() const { return now; }
    float currentAmount() const { return amount; }

private:
    void updateMapping();
    void updateReverbGains();

    double sr = 48000.0;
    float targetAmount = 0.45f, amount = 0.45f, ampCoef = 0.0f;
    bool bright = true, bypassed = false;
    float wetMix = 1.0f, wetCoef = 0.0f;
    Mapping now;
    bool lastBright = true;
    float lastShelf = -100.0f, lastDamping = -1.0f, lastDecay = -1.0f;

    // saturation (ADAA)
    float satPrevU[2] {};
    float satPrevY[2] {};

    // chorus + doubler
    DelayLine chorusDl[2], dblDl[2];
    double lfoChorus = 0.0, lfoDbl = 0.0;

    // width
    Biquad sideHp1, sideHp2;

    // reverb (8-line FDN)
    static constexpr int N = 8;
    DelayLine pre;
    std::array<DelayLine, N> lines;
    std::array<float, N> lineLen {}, lineGain {}, lineLp {};
    float dampCoef = 0.0f;
    Biquad verbHp;
    double lfoVerb = 0.0;

    // ducking
    double bpm = 150.0, beatPhase = 0.0;
    double hostPpq = -1.0;
    bool hostPlaying = false;
    float duckGain = 1.0f, duckCoef = 0.0f;

    // OTT
    LR4 xLow, xHigh;
    LR4AllPass apLow;
    float env[3] {};
    float envAtt = 0.0f, envRel = 0.0f;

    // shelf + gain
    Biquad shelf;
    float outGain = 1.0f, gainCoef = 0.0f;
};
}
