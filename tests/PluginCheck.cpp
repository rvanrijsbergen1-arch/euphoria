// Headless end-to-end check of Euphoria: real processor + engine.
// Levels (auto gain), transparency at 0 %, mono bass, bypass, stability, state, editor snapshot.
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "../Source/plugin/PluginProcessor.h"
#include "../Source/dsp/Engine.h"

namespace
{
int failures = 0, passes = 0;
void check (bool ok, const juce::String& what)
{
    std::printf ("%s  %s\n", ok ? "PASS" : "FAIL", what.toRawUTF8());
    (ok ? passes : failures)++;
}

constexpr double kSr = 48000.0;

/** Supersaw-ish lead: three detuned saws per side, gated 8th notes, about -12 dBFS. */
juce::AudioBuffer<float> makeLead (int seconds)
{
    const int n = (int) (kSr * seconds);
    juce::AudioBuffer<float> b (2, n);
    const double notes[] = { 440.0, 523.25, 659.25, 587.33 };
    double ph[2][3] {};
    for (int i = 0; i < n; ++i)
    {
        const double tsec = i / kSr;
        const int step = (int) (tsec * 5.0);   // 150 bpm 8ths
        const double f = notes[step % 4];
        const double gate = std::fmod (tsec * 5.0, 1.0) < 0.8 ? 1.0 : 0.0;
        for (int c = 0; c < 2; ++c)
        {
            double s = 0;
            const double det[3] = { 1.0, 1.006 + 0.002 * c, 0.994 - 0.002 * c };
            for (int v = 0; v < 3; ++v)
            {
                ph[c][v] += f * det[v] / kSr;
                ph[c][v] -= std::floor (ph[c][v]);
                s += 2.0 * ph[c][v] - 1.0;
            }
            b.setSample (c, i, (float) (0.25 * 0.33 * s * gate * 3.0 * 0.33));
        }
    }
    b.applyGain (0.25f / juce::jmax (1.0e-6f, b.getRMSLevel (0, 0, n)) * 0.5f);
    return b;
}

double rmsDb (const juce::AudioBuffer<float>& b, int from, int to)
{
    double s = 0;
    for (int c = 0; c < b.getNumChannels(); ++c)
        for (int i = from; i < to; ++i) s += (double) b.getSample (c, i) * b.getSample (c, i);
    return 10.0 * std::log10 (s / (double) (b.getNumChannels() * (to - from)) + 1e-20);
}

juce::AudioBuffer<float> runEngine (const juce::AudioBuffer<float>& in, float amount, bool bright, bool bypassed = false)
{
    eup::Engine e;
    e.setAmount (amount);
    e.setBright (bright);
    e.setBypassed (bypassed);
    e.prepare (kSr, 512);
    e.setTempo (150.0, -1.0, false);
    auto out = in;
    for (int i = 0; i < out.getNumSamples(); i += 512)
    {
        const int n = juce::jmin (512, out.getNumSamples() - i);
        e.process (out.getWritePointer (0, i), out.getWritePointer (1, i), n);
    }
    return out;
}

bool allFinite (const juce::AudioBuffer<float>& b, float& peak)
{
    peak = 0;
    for (int c = 0; c < b.getNumChannels(); ++c)
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            const float v = b.getSample (c, i);
            if (! std::isfinite (v)) return false;
            peak = juce::jmax (peak, std::abs (v));
        }
    return true;
}
}

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;

    const auto lead = makeLead (6);
    const int from = (int) (2 * kSr), to = lead.getNumSamples();
    const double inDb = rmsDb (lead, from, to);
    std::printf ("input lead RMS %.2f dBFS\n", inDb);

    // 1. loudness stays about the same over the whole knob range (auto gain)
    for (bool bright : { true, false })
        for (float a : { 0.0f, 0.15f, 0.3f, 0.5f, 0.7f, 0.85f, 1.0f })
        {
            const auto out = runEngine (lead, a, bright);
            float peak = 0;
            const bool finite = allFinite (out, peak);
            const double d = rmsDb (out, from, to) - inDb;
            std::printf ("     amount %3d%% %-6s  level %+.2f dB  peak %.2f\n", (int) (a * 100), bright ? "bright" : "dark", d, peak);
            check (finite && peak < 4.0f, juce::String::formatted ("finite output, sane peak at %d%% %s", (int) (a * 100), bright ? "bright" : "dark"));
            check (std::abs (d) < 2.0, juce::String::formatted ("loudness within 2 dB of the input at %d%% %s (%+.2f dB)", (int) (a * 100), bright ? "bright" : "dark", d));
        }

    // 2. transparent at 0 %: same level, nearly no stereo change
    {
        const auto out = runEngine (lead, 0.0f, true);
        check (std::abs (rmsDb (out, from, to) - inDb) < 0.2, "0 % keeps the level (within 0.2 dB)");
    }

    // 3. mono below 150 Hz: a pure side signal at 60 Hz is removed
    {
        juce::AudioBuffer<float> side (2, (int) (2 * kSr));
        for (int i = 0; i < side.getNumSamples(); ++i)
        {
            const float v = 0.3f * (float) std::sin (2 * juce::MathConstants<double>::pi * 60.0 * i / kSr);
            side.setSample (0, i, v); side.setSample (1, i, -v);
        }
        const auto out = runEngine (side, 0.5f, true);
        double s = 0, m = 0;
        for (int i = (int) kSr; i < out.getNumSamples(); ++i)
        {
            const double l = out.getSample (0, i), r = out.getSample (1, i);
            s += (l - r) * (l - r) * 0.25; m += (l + r) * (l + r) * 0.25;
        }
        const double inSide = 0.3 * 0.3 * 0.5 * kSr;
        const double reductionDb = 10.0 * std::log10 (inSide / (s + 1e-20));
        std::printf ("     60 Hz side reduced by %.1f dB\n", reductionDb);
        check (reductionDb > 12.0, "stereo below 150 Hz is folded to mono (side -12 dB or more)");
        // a 3 kHz side signal stays (and gets wider)
        juce::AudioBuffer<float> hi (2, (int) kSr);
        for (int i = 0; i < hi.getNumSamples(); ++i)
        {
            const float v = 0.1f * (float) std::sin (2 * juce::MathConstants<double>::pi * 3000.0 * i / kSr);
            hi.setSample (0, i, v); hi.setSample (1, i, -v);
        }
        const auto o2 = runEngine (hi, 0.3f, true);
        double s2 = 0;
        for (int i = (int) (0.5 * kSr); i < o2.getNumSamples(); ++i) { const double d = o2.getSample (0, i) - o2.getSample (1, i); s2 += d * d * 0.25; }
        const double inS2 = 0.01 * 0.5 * 0.5 * kSr;
        check (10.0 * std::log10 (s2 / inS2) > -1.0, "high-frequency stereo is kept");
    }

    // 4. bypass: output equals input
    {
        const auto out = runEngine (lead, 1.0f, true, true);
        float maxDiff = 0;
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < out.getNumSamples(); ++i) maxDiff = juce::jmax (maxDiff, std::abs (out.getSample (c, i) - lead.getSample (c, i)));
        check (maxDiff < 1.0e-6f, "bypass passes the input unchanged");
    }

    // 5. reverb tail: grows with the knob and dies out
    {
        juce::AudioBuffer<float> imp (2, (int) (10 * kSr));
        imp.clear();
        for (int i = 0; i < 2400; ++i) { imp.setSample (0, i, 0.5f * (float) std::sin (i * 0.05)); imp.setSample (1, i, 0.5f * (float) std::sin (i * 0.05)); }
        const auto low = runEngine (imp, 0.3f, true), high = runEngine (imp, 1.0f, true);
        const int a = (int) (0.6 * kSr), b = (int) (1.6 * kSr);
        const double tLow = rmsDb (low, a, b), tHigh = rmsDb (high, a, b);
        std::printf ("     tail 0.6-1.6 s: 30%% %.1f dB, 100%% %.1f dB\n", tLow, tHigh);
        check (tHigh > tLow + 20.0, "a big reverb tail at 100 %, none at 30 %");
        const double end = rmsDb (high, (int) (9 * kSr), (int) (10 * kSr));
        check (end < -70.0, juce::String::formatted ("tail decays (last second %.1f dB)", end));
    }

    // 6. Dark is darker than Bright at 100 %
    {
        const auto b = runEngine (lead, 1.0f, true), d = runEngine (lead, 1.0f, false);
        auto hf = [] (const juce::AudioBuffer<float>& x)
        {
            double s = 0, prev = 0;
            for (int i = (int) (2 * kSr); i < x.getNumSamples(); ++i) { const double v = x.getSample (0, i); s += (v - prev) * (v - prev); prev = v; }
            return s;
        };
        check (hf (b) > hf (d) * 1.15, "Bright has more top end than Dark");
    }

    // 7. the real processor: parameters, tempo-less playback, state round trip, editor
    {
        EuphoriaProcessor p;
        p.setPlayConfigDetails (2, 2, kSr, 512);
        p.prepareToPlay (kSr, 512);
        p.apvts.getParameter ("amount")->setValueNotifyingHost (0.8f);
        p.apvts.getParameter ("tone")->setValueNotifyingHost (0.0f);
        juce::AudioBuffer<float> buf (2, 512);
        juce::MidiBuffer midi;
        bool finite = true;
        for (int blk = 0; blk < 200; ++blk)
        {
            for (int c = 0; c < 2; ++c) buf.copyFrom (c, 0, lead, c, (blk * 512) % (lead.getNumSamples() - 512), 512);
            p.processBlock (buf, midi);
            float pk; finite = finite && allFinite (buf, pk);
        }
        check (finite, "processor output is finite");
        check (std::abs (p.amount01() - 0.8f) < 0.002f && ! p.isBright(), "parameters reach the processor");

        juce::MemoryBlock state;
        p.getStateInformation (state);
        EuphoriaProcessor q;
        q.setStateInformation (state.getData(), (int) state.getSize());
        check (std::abs (q.amount01() - 0.8f) < 0.002f && ! q.isBright(), "state is saved and restored");

        // mono track
        EuphoriaProcessor mono;
        mono.setPlayConfigDetails (1, 1, kSr, 512);
        mono.prepareToPlay (kSr, 512);
        juce::AudioBuffer<float> mb (1, 512);
        bool monoOk = true;
        for (int blk = 0; blk < 50; ++blk) { mb.copyFrom (0, 0, lead, 0, blk * 512, 512); mono.processBlock (mb, midi); float pk; monoOk = monoOk && allFinite (mb, pk); }
        check (monoOk, "mono tracks work");

        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        check (ed != nullptr && ed->getWidth() == 980 && ed->getHeight() == 700, "editor opens at 980 x 700");
        if (ed)
        {
            for (float a : { 0.0f, 0.45f, 1.0f })
            {
                p.apvts.getParameter ("amount")->setValueNotifyingHost (a);
                p.apvts.getParameter ("tone")->setValueNotifyingHost (a < 0.9f ? 1.0f : 0.0f);
                juce::MessageManager::getInstance()->runDispatchLoopUntil (120);
                auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
                auto f = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("eup_editor_" + juce::String (juce::roundToInt (a * 100)) + ".png");
                f.deleteFile();
                juce::FileOutputStream os (f);
                juce::PNGImageFormat().writeImageToStream (img, os);
                std::printf ("     editor snapshot: %s\n", f.getFullPathName().toRawUTF8());
            }
        }
    }

    std::printf ("\n%d passed, %d failed\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
