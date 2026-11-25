#include "Engine.h"
#include <algorithm>

namespace eup
{
namespace
{
constexpr double kPi = 3.14159265358979323846;

float lerp (std::initializer_list<std::pair<float, float>> pts, float k)
{
    auto it = pts.begin();
    auto prev = *it;
    if (k <= prev.first) return prev.second;
    for (++it; it != pts.end(); ++it)
    {
        const auto p = *it;
        if (k <= p.first) return prev.second + (p.second - prev.second) * ((k - prev.first) / (p.first - prev.first));
        prev = p;
    }
    return prev.second;
}

inline float logCosh (float u)
{
    const float a = std::abs (u);
    return a + std::log1p (std::exp (-2.0f * a)) - 0.69314718f;
}

inline float dbToGain (float db) { return std::pow (10.0f, db * 0.05f); }

void rbj (Biquad& q, double b0, double b1, double b2, double a0, double a1, double a2)
{
    q.b0 = (float) (b0 / a0); q.b1 = (float) (b1 / a0); q.b2 = (float) (b2 / a0);
    q.a1 = (float) (a1 / a0); q.a2 = (float) (a2 / a0);
}
}

// ------------------------------------------------------------------ mapping
namespace
{
/** Level correction when the Spellbook moves width, drive or OTT away from the default spell
    (measured on the supersaw lead in tests/PluginCheck.cpp; reverb needs none). */
float trimCorrectionDb (float k, float tw, float drive0, float drive1, float ott0, float ott1)
{
    const float dW = tw - 1.0f, dD = drive1 - drive0, dO = ott1 - ott0;
    const float widthPresence = k < 0.3f ? k / 0.3f : 1.0f;
    return -widthPresence * (dW < 0 ? 1.5f : 2.0f) * dW
           + (dD < 0 ? 0.125f : 0.31f) * dD
           + (dO < 0 ? 5.3f : 6.9f) * dO;
}
}
Mapping Mapping::forAmount (float k, bool bright, const Trims& t)
{
    Mapping m;
    m.width      = lerp ({ { 0.0f, 100.0f }, { 0.3f, 130.0f }, { 0.7f, 140.0f }, { 1.0f, 150.0f } }, k);
    m.chorusMix  = lerp ({ { 0.0f, 0.0f }, { 0.3f, 0.25f }, { 1.0f, 0.25f } }, k);
    m.driveDb    = lerp ({ { 0.0f, 0.0f }, { 0.3f, 3.0f }, { 0.7f, 6.0f }, { 1.0f, 9.0f } }, k);
    m.reverbMix  = lerp ({ { 0.0f, 0.0f }, { 0.3f, 0.0f }, { 0.7f, 0.25f }, { 1.0f, 0.40f } }, k);
    m.decaySec   = lerp ({ { 0.0f, 1.5f }, { 0.3f, 1.5f }, { 0.7f, 4.0f }, { 1.0f, 6.0f } }, k);
    m.preDelayMs = lerp ({ { 0.0f, 20.0f }, { 0.3f, 20.0f }, { 0.7f, 35.0f }, { 1.0f, 40.0f } }, k);
    m.duckDb     = lerp ({ { 0.0f, 0.0f }, { 0.3f, 0.0f }, { 0.7f, 6.0f }, { 1.0f, 9.0f } }, k);
    m.ottDepth   = lerp ({ { 0.0f, 0.0f }, { 0.7f, 0.0f }, { 1.0f, 0.40f } }, k);
    m.shelfDb    = lerp ({ { 0.0f, 0.0f }, { 0.7f, 0.0f }, { 1.0f, bright ? 4.0f : 1.0f } }, k);
    m.doublerMix = lerp ({ { 0.0f, 0.0f }, { 0.7f, 0.0f }, { 1.0f, 0.30f } }, k);
    // measured with tests/PluginCheck.cpp (supersaw lead) so the level stays put over the whole knob range
    m.autoGainDb = bright ? lerp ({ { 0.0f, 0.0f }, { 0.15f, -0.15f }, { 0.3f, -0.75f }, { 0.5f, -0.8f }, { 0.7f, -0.85f }, { 0.85f, 0.25f }, { 1.0f, 1.3f } }, k)
                          : lerp ({ { 0.0f, 0.0f }, { 0.15f, 0.2f }, { 0.3f, -0.3f }, { 0.5f, -0.2f }, { 0.7f, 0.0f }, { 0.85f, 1.2f }, { 1.0f, 2.3f } }, k);
    m.dampingHz  = bright ? 12000.0f : 5000.0f;

    // Spellbook trims (1 = default). Auto gain is calibrated for the default spell; the trims add a rough correction.
    const auto clampT = [] (float v) { return v < 0.0f ? 0.0f : (v > 2.0f ? 2.0f : v); };
    const float tw = clampT (t.width), tr = clampT (t.reverb), td = clampT (t.drive), to = clampT (t.ott);
    const float baseVerb = m.reverbMix;
    const float baseDrive = m.driveDb, baseOtt = m.ottDepth;
    m.width = 100.0f + (m.width - 100.0f) * tw;
    m.chorusMix *= tw;
    m.reverbMix = std::min (0.8f, baseVerb * tr);
    m.driveDb = baseDrive * td;
    m.ottDepth = std::min (0.8f, baseOtt * to);
    m.autoGainDb += trimCorrectionDb (k, tw, baseDrive, m.driveDb, baseOtt, m.ottDepth);
    return m;
}

// ------------------------------------------------------------------ biquads
void Biquad::setHighShelf (double s, double f, double gainDb, double q)
{
    const double A = std::pow (10.0, gainDb / 40.0), w = 2 * kPi * f / s, c = std::cos (w), al = std::sin (w) / (2 * q), sa = 2 * std::sqrt (A) * al;
    rbj (*this, A * ((A + 1) + (A - 1) * c + sa), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sa),
         (A + 1) - (A - 1) * c + sa, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sa);
}
void Biquad::setLowPass (double s, double f, double q)
{
    const double w = 2 * kPi * f / s, c = std::cos (w), al = std::sin (w) / (2 * q);
    rbj (*this, (1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al);
}
void Biquad::setHighPass (double s, double f, double q)
{
    const double w = 2 * kPi * f / s, c = std::cos (w), al = std::sin (w) / (2 * q);
    rbj (*this, (1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
}
void Biquad::setAllPass (double s, double f, double q)
{
    const double w = 2 * kPi * f / s, c = std::cos (w), al = std::sin (w) / (2 * q);
    rbj (*this, 1 - al, -2 * c, 1 + al, 1 + al, -2 * c, 1 - al);
}

// ------------------------------------------------------------------ engine
void Engine::prepare (double sampleRate, int)
{
    sr = sampleRate > 0 ? sampleRate : 48000.0;
    const auto ms = [this] (double m) { return (int) std::ceil (m * 0.001 * sr); };
    for (int c = 0; c < 2; ++c) { chorusDl[c].setMax (ms (14.0) + 8); dblDl[c].setMax (ms (24.0) + 8); }
    pre.setMax (ms (45.0) + 8);
    static constexpr float base[N] = { 31.1f, 37.3f, 41.7f, 47.9f, 53.3f, 59.9f, 67.1f, 73.7f };
    for (int i = 0; i < N; ++i)
    {
        lineLen[(size_t) i] = (float) (base[i] * 1.45 * 0.001 * sr);
        lines[(size_t) i].setMax ((int) lineLen[(size_t) i] + ms (1.0) + 8);
    }
    verbHp.setHighPass (sr, 300.0);
    xLow.set (sr, 120.0);
    xHigh.set (sr, 2500.0);
    apLow.set (sr, 2500.0);
    sideHp1.setHighPass (sr, 150.0); sideHp2.setHighPass (sr, 150.0);
    ampCoef = (float) (1.0 - std::exp (-1.0 / (0.020 * sr)));
    wetCoef = (float) (1.0 - std::exp (-1.0 / (0.010 * sr)));
    duckCoef = (float) (1.0 - std::exp (-1.0 / (0.004 * sr)));
    gainCoef = (float) (1.0 - std::exp (-1.0 / (0.030 * sr)));
    envAtt = (float) (1.0 - std::exp (-1.0 / (0.004 * sr)));
    envRel = (float) (1.0 - std::exp (-1.0 / (0.090 * sr)));
    reset();
}

void Engine::reset()
{
    amount = targetAmount;
    wetMix = bypassed ? 0.0f : 1.0f;
    for (int c = 0; c < 2; ++c) { chorusDl[c].clear(); dblDl[c].clear(); satPrevU[c] = satPrevY[c] = 0.0f; }
    pre.clear();
    for (auto& l : lines) l.clear();
    lineLp.fill (0.0f);
    verbHp.reset(); xLow.reset(); xHigh.reset(); apLow.reset(); shelf.reset();
    sideHp1.reset(); sideHp2.reset();
    env[0] = env[1] = env[2] = 0.0f;
    lfoChorus = lfoDbl = lfoVerb = beatPhase = 0.0;
    duckGain = 1.0f;
    lastShelf = -100.0f; lastDamping = -1.0f; lastDecay = -1.0f;
    updateMapping();
    outGain = dbToGain (now.autoGainDb);
}

void Engine::setTempo (double b, double ppq, bool playing)
{
    if (b > 20.0 && b < 400.0) bpm = b;
    hostPpq = ppq;
    hostPlaying = playing && ppq >= 0.0;
}

void Engine::updateMapping()
{
    now = Mapping::forAmount (amount, bright, trims);
    if (std::abs (now.shelfDb - lastShelf) > 0.02f) { shelf.setHighShelf (sr, 10000.0, now.shelfDb); lastShelf = now.shelfDb; }
    if (now.dampingHz != lastDamping)
    {
        dampCoef = (float) (1.0 - std::exp (-2 * kPi * now.dampingHz / sr));
        lastDamping = now.dampingHz;
    }
    if (std::abs (now.decaySec - lastDecay) > 0.005f) { lastDecay = now.decaySec; updateReverbGains(); }
}

void Engine::updateReverbGains()
{
    for (int i = 0; i < N; ++i)
        lineGain[(size_t) i] = (float) std::pow (10.0, -3.0 * lineLen[(size_t) i] / (now.decaySec * sr));
}

void Engine::process (float* L, float* R, int n)
{
    const double twoPi = 2 * kPi;
    const double incChorus = twoPi * 0.4 / sr, incDbl = twoPi * 0.3 / sr, incVerb = twoPi * 0.7 / sr;
    const float chorusBase = (float) (0.008 * sr), chorusDepth = (float) (0.003 * sr);
    const float dblBase = (float) (0.015 * sr), dblDepth = (float) (0.0046 * sr);
    const float verbMod = (float) (0.0003 * sr);
    const double beatInc = bpm / 60.0 / sr;
    const float warmBias = bright ? 0.0f : 0.18f;

    for (int start = 0; start < n; start += 32)
    {
        const int end = std::min (n, start + 32);
        // knob smoothing (20 ms) at sub-block rate
        amount += (targetAmount - amount) * (1.0f - std::pow (1.0f - ampCoef, (float) (end - start)));
        if (std::abs (targetAmount - amount) < 1.0e-5f) amount = targetAmount;
        if (bright != lastBright) { lastBright = bright; lastShelf = -100.0f; }
        updateMapping();
        const Mapping m = now;

        const float g = dbToGain (m.driveDb);
        const float satWet = std::min (1.0f, m.driveDb);
        const float biasOut = std::tanh (g * warmBias) / g;
        const float widthGain = m.width * 0.01f;
        const float monoAmt = std::min (1.0f, amount / 0.05f);
        const int preSamples = std::max (1, (int) (m.preDelayMs * 0.001f * (float) sr));
        const float targetGain = dbToGain (m.autoGainDb);

        for (int i = start; i < end; ++i)
        {
            const float dryL = L[i], dryR = R[i];
            float x[2] = { dryL, dryR };

            // 1. saturation: first-order antiderivative anti-aliased tanh, normalised to unity small-signal gain
            if (satWet > 0.0f)
                for (int c = 0; c < 2; ++c)
                {
                    const float u = g * (x[c] + warmBias);
                    const float du = u - satPrevU[c];
                    float y = std::abs (du) > 1.0e-4f ? (logCosh (u) - logCosh (satPrevU[c])) / du : std::tanh (0.5f * (u + satPrevU[c]));
                    satPrevU[c] = u;
                    y = y / g - biasOut;
                    x[c] += satWet * (y - x[c]);
                }

            // 2. chorus (0.4 Hz, 8 +- 3 ms, quadrature L/R)
            const float cS = (float) std::sin (lfoChorus), cC = (float) std::cos (lfoChorus);
            lfoChorus += incChorus; if (lfoChorus > twoPi) lfoChorus -= twoPi;
            chorusDl[0].push (x[0]); chorusDl[1].push (x[1]);
            if (m.chorusMix > 0.0f)
            {
                const float wl = chorusDl[0].read (chorusBase + chorusDepth * cS);
                const float wr = chorusDl[1].read (chorusBase + chorusDepth * cC);
                x[0] += m.chorusMix * wl;
                x[1] += m.chorusMix * wr;
            }

            // 3. unison doubler: two voices at +-15 cents (moving delay), panned L/R
            const float dS = (float) std::sin (lfoDbl);
            lfoDbl += incDbl; if (lfoDbl > twoPi) lfoDbl -= twoPi;
            dblDl[0].push (x[0]); dblDl[1].push (x[1]);
            if (m.doublerMix > 0.0f)
            {
                x[0] += m.doublerMix * dblDl[0].read (dblBase + dblDepth * dS);
                x[1] += m.doublerMix * dblDl[1].read (dblBase - dblDepth * dS);
            }

            // 4. width, with the side signal removed below 150 Hz
            {
                const float mid = 0.5f * (x[0] + x[1]);
                float side = 0.5f * (x[0] - x[1]);
                const float sideHi = sideHp2.process (sideHp1.process (side, 0), 0);
                side = (side + monoAmt * (sideHi - side)) * widthGain;
                x[0] = mid + side;
                x[1] = mid - side;
            }

            // 5. FDN hall reverb with pre-delay, 300 Hz low cut and tempo-synced ducking
            {
                pre.push (0.5f * (x[0] + x[1]));
                const float in = verbHp.process (pre.readInt (preSamples), 0);
                const float vm = (float) std::sin (lfoVerb);
                lfoVerb += incVerb; if (lfoVerb > twoPi) lfoVerb -= twoPi;
                float f[N];
                float sum = 0.0f;
                for (int k = 0; k < N; ++k)
                {
                    const float len = lineLen[(size_t) k] + (k == 0 ? verbMod * vm : (k == 5 ? -verbMod * vm : 0.0f));
                    const float o = lines[(size_t) k].read (len);
                    lineLp[(size_t) k] += dampCoef * (o - lineLp[(size_t) k]);
                    f[k] = lineLp[(size_t) k];
                    sum += f[k];
                }
                sum *= 2.0f / (float) N;
                for (int k = 0; k < N; ++k)
                    lines[(size_t) k].push ((f[k] - sum) * lineGain[(size_t) k] + ((k & 1) ? -0.35f : 0.35f) * in);
                const float wl = 0.5f * (f[0] - f[2] + f[4] - f[6]);
                const float wr = 0.5f * (f[1] - f[3] + f[5] - f[7]);

                double phase;
                if (hostPlaying) { const double p = hostPpq + (double) i * beatInc; phase = p - std::floor (p); }
                else { beatPhase += beatInc; if (beatPhase >= 1.0) beatPhase -= 1.0; phase = beatPhase; }
                const float r = (float) (1.0 - phase);
                const float duckTarget = dbToGain (-m.duckDb * r * r);
                duckGain += duckCoef * (duckTarget - duckGain);
                const float wetAmt = m.reverbMix * duckGain;
                x[0] += wetAmt * wl;
                x[1] += wetAmt * wr;
            }

            // 6. OTT-style 3-band upward + downward compression (parallel, phase-aligned bands)
            {
                float band[3][2];
                for (int c = 0; c < 2; ++c)
                {
                    float lo, rest, mid, hi;
                    xLow.split (x[c], c, lo, rest);
                    xHigh.split (rest, c, mid, hi);
                    band[0][c] = apLow.process (lo, c);
                    band[1][c] = mid;
                    band[2][c] = hi;
                }
                if (m.ottDepth > 0.0f)
                {
                    float y[2] = { 0, 0 }, dry[2] = { 0, 0 };
                    for (int b = 0; b < 3; ++b)
                    {
                        const float pk = std::max (std::abs (band[b][0]), std::abs (band[b][1]));
                        env[b] += (pk > env[b] ? envAtt : envRel) * (pk - env[b]);
                        const float lvl = 20.0f * std::log10 (env[b] + 1.0e-9f);
                        float gdb = 0.0f;
                        if (lvl > -24.0f) gdb = -(lvl + 24.0f) * (2.0f / 3.0f);
                        else if (lvl < -42.0f && lvl > -75.0f) gdb = std::min (12.0f, (-42.0f - lvl) * (2.0f / 3.0f));
                        const float gl = dbToGain (gdb);
                        for (int c = 0; c < 2; ++c) { y[c] += band[b][c] * gl; dry[c] += band[b][c]; }
                    }
                    for (int c = 0; c < 2; ++c) x[c] = dry[c] + m.ottDepth * (y[c] - dry[c]);
                }
                else
                {
                    for (int c = 0; c < 2; ++c) x[c] = band[0][c] + band[1][c] + band[2][c];
                    env[0] = env[1] = env[2] = 0.0f;
                }
            }

            // 7. high shelf at 10 kHz, 8. auto gain
            outGain += gainCoef * (targetGain - outGain);
            const float wl = shelf.process (x[0], 0) * outGain;
            const float wr = shelf.process (x[1], 1) * outGain;

            // bypass crossfade (10 ms)
            wetMix += wetCoef * ((bypassed ? 0.0f : 1.0f) - wetMix);
            L[i] = dryL + wetMix * (wl - dryL);
            R[i] = dryR + wetMix * (wr - dryR);
        }
    }
}
}
