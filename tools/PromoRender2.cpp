// EUPHORIA promo v2 (9:16, 40 s): original 152 BPM euphoric track, lead processed by the real plugin,
// real editor filmed with a virtual camera (close-ups cut on bar lines), captions in social safe zones.
// Writes promo_v2_silent.mp4, music.wav (full mix) and lead_dry.wav / lead_euphoria.wav (for checks).
// Usage: eup_promo2 <outdir> [preview]
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "../Source/plugin/PluginProcessor.h"
#include "../Source/plugin/PluginEditor.h"
#include "../Source/plugin/EphLogo.h"
#include <cstdio>
#include <random>

namespace
{
constexpr int kFps = 30, W = 1080, H = 1920;
constexpr double kDur = 40.0, kSr = 48000.0, kBpm = 152.0;
constexpr double kBeat = 60.0 / kBpm, kBar = 4.0 * kBeat, kStep = kBeat / 4.0;
double barT (double b) { return b * kBar; }

double smooth (double x) { x = juce::jlimit (0.0, 1.0, x); return x * x * (3 - 2 * x); }
double ramp (double t, double t0, double t1, double a, double b) { return a + (b - a) * smooth ((t - t0) / (t1 - t0)); }

// ------------------------------------------------------------------ script (bar-aligned)
// bar 0 dry | bar 1 processed preview | bars 2-4 Charm | bar 5 kick enters | bars 6-9 Conjure | bars 10-13 Ascend
// bars 14-17 A/B | bars 18-21 Dark + Spellbook | bars 22-25 hero + tail
float amountAt (double t)
{
    if (t < barT (1)) return 0.0f;
    if (t < barT (2) - 0.12) return (float) ramp (t, barT (1), barT (1) + 0.25, 0.0, 1.0);
    if (t < barT (2)) return (float) ramp (t, barT (2) - 0.12, barT (2), 1.0, 0.0);
    if (t < barT (5)) return (float) ramp (t, barT (2), barT (4), 0.0, 0.30);
    if (t < barT (10)) return (float) ramp (t, barT (6), barT (8), 0.30, 0.70);
    return (float) ramp (t, barT (10), barT (12), 0.70, 1.00);
}
bool bypassAt (double t) { return (t >= barT (14) && t < barT (15)) || (t >= barT (16) && t < barT (17)); }
bool darkAt (double t) { return t >= barT (18) + kBeat && t < barT (22); }
bool bookAt (double t) { return t >= barT (19) && t < barT (22); }
float reverbTrimAt (double t)
{
    if (t < barT (20)) return 1.0f;
    if (t < barT (21)) return (float) ramp (t, barT (20), barT (20) + kBar * 0.6, 1.0, 1.7);
    return (float) ramp (t, barT (21), barT (21) + kBar * 0.6, 1.7, 1.0);
}
bool kickOn (double t) { return t >= barT (5) && t < barT (24); }

// ------------------------------------------------------------------ music (original composition for this video)
struct Note { int bar; double step, len; int midi; };

std::vector<Note> melody()
{
    // 4-bar phrase over Fm - Db - Ab - Eb, 16th-note grid
    const std::vector<std::vector<std::array<double, 3>>> phrase = {
        { { 0, 3, 72 }, { 3, 3, 75 }, { 6, 2, 77 }, { 8, 4, 80 }, { 12, 2, 79 }, { 14, 2, 77 } },
        { { 0, 3, 77 }, { 3, 3, 80 }, { 6, 2, 77 }, { 8, 6, 73 }, { 14, 2, 72 } },
        { { 0, 3, 72 }, { 3, 3, 75 }, { 6, 2, 80 }, { 8, 4, 84 }, { 12, 2, 82 }, { 14, 2, 80 } },
        { { 0, 3, 79 }, { 3, 3, 82 }, { 6, 4, 79 }, { 10, 4, 75 } },
    };
    std::vector<Note> notes;
    for (int bar = 0; bar < 24; ++bar)
    {
        if (bar == 23) { notes.push_back ({ bar, 0, 12, 77 }); continue; }   // last note, then the tail rings out
        if (bar == 22) { notes.push_back ({ bar, 0, 3, 72 }); notes.push_back ({ bar, 3, 3, 75 }); notes.push_back ({ bar, 6, 2, 77 }); notes.push_back ({ bar, 8, 8, 80 }); continue; }
        for (auto& n : phrase[(size_t) (bar % 4)])
        {
            if (bar == 9 && n[0] >= 8) continue;   // rest: expose the reverb tail between phrases
            notes.push_back ({ bar, n[0], n[1], (int) n[2] });
        }
    }
    return notes;
}

double midiHz (double m) { return 440.0 * std::pow (2.0, (m - 69.0) / 12.0); }

/** Dry supersaw lead: 9 detuned saws + sub-octave, low-pass with a short envelope, mono (EUPHORIA adds the width). */
juce::AudioBuffer<float> renderLead (int n)
{
    juce::AudioBuffer<float> b (2, n);
    b.clear();
    const double det[9] = { 0.0, -0.08, 0.08, -0.17, 0.17, -0.27, 0.27, -0.38, 0.38 };
    std::mt19937 rng (7);
    std::uniform_real_distribution<double> u (0.0, 1.0);
    for (auto& nt : melody())
    {
        const int s0 = (int) ((barT (nt.bar) + nt.step * kStep) * kSr);
        const int len = (int) (nt.len * kStep * kSr);
        const int rel = (int) (0.07 * kSr);
        double ph[10];
        for (auto& p : ph) p = u (rng);
        double lp1 = 0, lp2 = 0;
        for (int i = 0; i < len + rel && s0 + i < n; ++i)
        {
            const double t = i / kSr;
            const double f = midiHz (nt.midi);
            double v = 0;
            for (int k = 0; k < 9; ++k)
            {
                ph[k] += f * std::pow (2.0, det[k] / 12.0) / kSr;
                ph[k] -= std::floor (ph[k]);
                v += (2.0 * ph[k] - 1.0) * (k == 0 ? 1.0 : 0.8);
            }
            ph[9] += f * 0.5 / kSr; ph[9] -= std::floor (ph[9]);
            v = v / 7.4 + 0.35 * (2.0 * ph[9] - 1.0);
            // filter: bright attack settling to ~5 kHz
            const double fc = 5000.0 + 7000.0 * std::exp (-t * 18.0);
            const double a = 1.0 - std::exp (-2.0 * juce::MathConstants<double>::pi * fc / kSr);
            lp1 += a * (v - lp1); lp2 += a * (lp1 - lp2);
            double env = std::min (1.0, t / 0.004) * (0.82 + 0.18 * std::exp (-t * 9.0));
            if (i >= len) env *= std::max (0.0, 1.0 - (double) (i - len) / rel);
            const float s = (float) (lp2 * env * 0.30);
            b.addSample (0, s0 + i, s);
            b.addSample (1, s0 + i, s);
        }
    }
    return b;
}

/** Backing: soft chord pad (unprocessed, identical in every comparison), pumps with the kick once it is in. */
juce::AudioBuffer<float> renderPad (int n)
{
    juce::AudioBuffer<float> b (2, n);
    b.clear();
    const int chords[4][4] = { { 53, 60, 65, 68 }, { 49, 56, 61, 65 }, { 56, 60, 63, 68 }, { 51, 58, 63, 67 } };
    double ph[4][2] {}, lp[2] {};
    for (int i = 0; i < n; ++i)
    {
        const double t = i / kSr;
        const int bar = (int) (t / kBar);
        if (bar >= 24) break;
        const auto& c = chords[bar % 4];
        double v[2] = { 0, 0 };
        for (int k = 0; k < 4; ++k)
            for (int ch = 0; ch < 2; ++ch)
            {
                ph[k][ch] += midiHz (c[k] + (ch ? 0.06 : -0.06)) / kSr; ph[k][ch] -= std::floor (ph[k][ch]);
                v[ch] += 2.0 * ph[k][ch] - 1.0;
            }
        const double a = 1.0 - std::exp (-2.0 * juce::MathConstants<double>::pi * 1400.0 / kSr);
        const double inBar = std::fmod (t, kBar);
        const double fade = std::min (1.0, inBar / 0.02) * std::min (1.0, (kBar - inBar) / 0.02);
        double pump = 1.0;
        if (kickOn (t)) { const double tb = std::fmod (t, kBeat); pump = 0.35 + 0.65 * smooth (tb / (kBeat * 0.7)); }
        for (int ch = 0; ch < 2; ++ch)
        {
            lp[ch] += a * (v[ch] - lp[ch]);
            b.setSample (ch, i, (float) (lp[ch] * 0.05 * fade * pump));
        }
    }
    return b;
}

/** Hardstyle-style kick: pitch-swept sine, click, distortion and a tuned tail. Enters separately at bar 5. */
juce::AudioBuffer<float> renderKick (int n)
{
    juce::AudioBuffer<float> b (2, n);
    b.clear();
    for (int beat = 0; ; ++beat)
    {
        const double t0 = beat * kBeat;
        if (t0 >= kDur) break;
        if (! kickOn (t0 + 1e-6)) continue;
        const int s0 = (int) (t0 * kSr), len = (int) (0.34 * kSr);
        double ph = 0, lp = 0;
        for (int i = 0; i < len && s0 + i < n; ++i)
        {
            const double t = i / kSr;
            const double f = 43.65 + 900.0 * std::exp (-t * 55.0) + 90.0 * std::exp (-t * 9.0);
            ph += f / kSr;
            double v = std::sin (2 * juce::MathConstants<double>::pi * ph);
            v = std::tanh (v * (2.5 + 5.0 * std::exp (-t * 6.0)));
            const double a = 1.0 - std::exp (-2.0 * juce::MathConstants<double>::pi * 6000.0 / kSr);
            lp += a * (v - lp);
            const double env = std::min (1.0, t / 0.0015) * (t < 0.26 ? 1.0 : std::max (0.0, 1.0 - (t - 0.26) / 0.08));
            const float s = (float) (lp * env * 0.36);
            b.addSample (0, s0 + i, s);
            b.addSample (1, s0 + i, s);
        }
    }
    return b;
}

// ------------------------------------------------------------------ camera
struct Cam { float cx, cy, w; };
constexpr float kAspect = 1080.0f / 1000.0f;
const Cam FULL { 490, 350, 1010 }, KNOB { 704, 322, 520 }, EPHY { 236, 430, 480 }, EPHY_HEAD { 262, 330, 360 },
          RIGHT { 704, 430, 560 }, HEADER { 760, 150, 470 }, BOOK { 712, 322, 520 };

struct Shot { double from; Cam cam; };
const Shot shots[] = {
    { 0.0, FULL }, { barT (2), KNOB }, { barT (4), EPHY }, { barT (5), FULL }, { barT (6), RIGHT }, { barT (8), EPHY },
    { barT (10), KNOB }, { barT (12), EPHY_HEAD }, { barT (13), FULL }, { barT (18), HEADER }, { barT (19), BOOK }, { barT (22), FULL },
};

Cam lerpCam (const Cam& a, const Cam& b, float x) { return { a.cx + (b.cx - a.cx) * x, a.cy + (b.cy - a.cy) * x, a.w + (b.w - a.w) * x }; }

Cam cameraAt (double t)
{
    int idx = 0;
    for (int i = 0; i < (int) std::size (shots); ++i) if (t >= shots[i].from) idx = i;
    Cam c = shots[idx].cam;
    if (idx > 0)
    {
        const double x = (t - shots[idx].from) / 0.22;   // quick whip into the new shot
        if (x < 1.0) c = lerpCam (shots[idx - 1].cam, c, (float) smooth (x));
    }
    // slow push-in within a shot
    const double next = idx + 1 < (int) std::size (shots) ? shots[idx + 1].from : kDur;
    const float push = (float) ((t - shots[idx].from) / juce::jmax (0.5, next - shots[idx].from));
    c.w *= 1.0f - 0.04f * push;
    // bump on every kick
    if (kickOn (t)) { const double tb = std::fmod (t, kBeat); c.w *= 1.0f - 0.012f * (float) std::exp (-tb * 14.0); }
    return c;
}

double flashAt (double t)
{
    double f = 0;
    for (double b : { 1.0, 2.0, 5.0, 10.0, 12.0, 14.0, 22.0 })
    {
        const double d = t - barT (b);
        if (d >= 0 && d < 0.35) f = juce::jmax (f, 0.22 * (1.0 - d / 0.35));
    }
    return f;
}

// ------------------------------------------------------------------ captions
juce::Font font (float h, bool bold) { return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain)); }

struct Caption { double from, to; const char* big; const char* small; };
const Caption captions[] = {
    { 0.0, barT (2), "ONE KNOB. ONE WIZARD.", "" },
    { barT (2), barT (5), "CHARM", "WIDTH  \xc2\xb7  SHIMMER  \xc2\xb7  WARMTH" },
    { barT (5), barT (10), "CONJURE", "TEMPO-SYNCED REVERB DUCKING" },
    { barT (10), barT (14), "ASCEND", "OTT  \xc2\xb7  SPARKLE  \xc2\xb7  UNISON" },
    { barT (18), barT (22), "DARK / BRIGHT", "+ SPELLBOOK" },
};

float fadeIn (double t, double from, double to, double fade = 0.25)
{
    if (t < from || t >= to) return 0.0f;
    return (float) juce::jmin (1.0, (t - from) / fade, (to - t) / fade);
}

void drawCaption (juce::Graphics& g, const juce::String& big, const juce::String& small, float a, juce::Colour accent, float bigSize = 86.0f)
{
    if (a <= 0) return;
    const float rise = (1.0f - a) * 18.0f;
    g.setColour (juce::Colours::white.withAlpha (a));
    g.setFont (font (bigSize, true).withExtraKerningFactor (0.04f));
    g.drawFittedText (big, juce::Rectangle<int> (60, (int) (300 + rise), W - 120, 110), juce::Justification::centredBottom, 1);
    if (small.isNotEmpty())
    {
        g.setColour (accent.withAlpha (a));
        g.setFont (font (40.0f, true).withExtraKerningFactor (0.08f));
        g.drawFittedText (small, juce::Rectangle<int> (60, (int) (418 + rise), W - 120, 50), juce::Justification::centredTop, 1);
    }
}

struct Pipe
{
    FILE* f = nullptr;
    Pipe (const juce::String& file)
    {
        const auto cmd = juce::String::formatted ("ffmpeg -y -loglevel error -f rawvideo -pix_fmt bgra -s %dx%d -r %d -i - -c:v libx264 -crf 17 -preset veryfast -pix_fmt yuv420p -movflags +faststart \"%s\"",
                                                  W, H, kFps, file.toRawUTF8());
        f = popen (cmd.toRawUTF8(), "w");
    }
    void write (const juce::Image& img)
    {
        juce::Image::BitmapData bd (img, juce::Image::BitmapData::readOnly);
        for (int y = 0; y < H; ++y) fwrite (bd.getLinePointer (y), 4, (size_t) W, f);
    }
    int close() { return pclose (f); }
};

void writeWav (const juce::File& f, const juce::AudioBuffer<float>& b)
{
    f.deleteFile();
    juce::WavAudioFormat wav;
    if (auto os = std::unique_ptr<juce::FileOutputStream> (f.createOutputStream()))
        if (auto w = std::unique_ptr<juce::AudioFormatWriter> (wav.createWriterFor (os.get(), kSr, 2, 24, {}, 0)))
        {
            os.release();
            w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
        }
}

double rms (const juce::AudioBuffer<float>& b, double t0, double t1)
{
    const int a = (int) (t0 * kSr), z = (int) (t1 * kSr);
    double s = 0;
    for (int c = 0; c < 2; ++c) for (int i = a; i < z; ++i) s += (double) b.getSample (c, i) * b.getSample (c, i);
    return 10.0 * std::log10 (s / (2.0 * (z - a)) + 1e-20);
}
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File out (argc > 1 ? juce::String (argv[1]) : juce::File::getCurrentWorkingDirectory().getFullPathName());
    out.createDirectory();
    const bool preview = argc > 2 && juce::String (argv[2]) == "preview";

    EuphoriaProcessor p;
    const int hop = (int) (kSr / kFps);
    p.setPlayConfigDetails (2, 2, kSr, hop);
    p.prepareToPlay (kSr, hop);
    auto* amount = p.apvts.getParameter ("amount");
    auto* tone = p.apvts.getParameter ("tone");
    auto* bypass = p.apvts.getParameter ("bypass");
    auto* trimReverb = p.apvts.getParameter ("trimReverb");

    std::unique_ptr<EuphoriaEditor> ed (dynamic_cast<EuphoriaEditor*> (p.createEditor()));
    if (ed == nullptr) return 1;
    std::unique_ptr<juce::Drawable> logo;
    if (auto xml = juce::XmlDocument::parse (EphLogo::svg)) logo = juce::Drawable::createFromSVG (*xml);

    const int n = (int) (kDur * kSr);
    const auto lead = renderLead (n), pad = renderPad (n), kick = renderKick (n);
    juce::AudioBuffer<float> leadOut (2, n), mix (2, n);
    leadOut.clear();

    // host tempo for the reverb ducking: a fake play head in sync with the track
    struct Head : juce::AudioPlayHead
    {
        double ppq = 0;
        juce::Optional<PositionInfo> getPosition() const override
        {
            PositionInfo pi; pi.setBpm (kBpm); pi.setPpqPosition (ppq); pi.setIsPlaying (true); return pi;
        }
    } head;
    p.setPlayHead (&head);

    Pipe* pipe = preview ? nullptr : new Pipe (out.getChildFile ("promo_v2_silent.tmp.mp4").getFullPathName());
    juce::Image frame (juce::Image::ARGB, W, H, true);
    const juce::Rectangle<float> V (0, 580, (float) W, 1000);   // viewport

    const int frames = (int) (kDur * kFps);
    juce::AudioBuffer<float> blk (2, hop);
    juce::MidiBuffer midi;
    bool book = false;
    const double previews[] = { 1.0, 2.6, 6.0, 12.0, 14.0, 19.6, 23.0, 24.5, 29.5, 32.5, 37.0 };

    for (int i = 0; i < frames; ++i)
    {
        const double t = (double) i / kFps;
        amount->setValueNotifyingHost (amountAt (t));
        tone->setValueNotifyingHost (darkAt (t) ? 0.0f : 1.0f);
        bypass->setValueNotifyingHost (bypassAt (t) ? 1.0f : 0.0f);
        trimReverb->setValueNotifyingHost (trimReverb->convertTo0to1 (reverbTrimAt (t) * 100.0f));

        const int s0 = i * hop;
        head.ppq = t / kBeat;
        for (int c = 0; c < 2; ++c) blk.copyFrom (c, 0, lead, c, s0, hop);
        p.processBlock (blk, midi);
        for (int c = 0; c < 2; ++c) leadOut.copyFrom (c, s0, blk, c, 0, hop);

        bool wantFrame = ! preview;
        if (preview) for (double pt : previews) if (i == (int) (pt * kFps)) wantFrame = true;
        if (bookAt (t) != book) { book = bookAt (t); ed->showSpellbook (book); }
        if (! wantFrame) continue;

        ed->renderAt (t);
        const auto accent = p.isBright() ? juce::Colour (0xff5ee7ff) : juce::Colour (0xff9b7bff);
        const auto ui = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);

        juce::Graphics g (frame);
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        juce::ColourGradient bg (juce::Colour (0xff1c1340), W * 0.5f, H * 0.55f, juce::Colour (0xff06050d), W * 0.5f, H * 1.15f, true);
        g.setGradientFill (bg);
        g.fillAll();

        // viewport with the real plugin, filmed through the camera
        const Cam cam = cameraAt (t);
        const float camH = cam.w / kAspect;
        const float scale = V.getWidth() / cam.w;
        const float camX = cam.cx - cam.w * 0.5f, camY = cam.cy - camH * 0.5f;
        auto uiToScreen = [&] (juce::Rectangle<float> r) { return juce::Rectangle<float> (V.getX() + (r.getX() - camX) * scale, V.getY() + (r.getY() - camY) * scale, r.getWidth() * scale, r.getHeight() * scale); };
        {
            g.saveState();
            g.reduceClipRegion (V.toNearestInt());
            const auto panelR = uiToScreen ({ 0.0f, 0.0f, 980.0f, 700.0f });
            juce::Path panelClip; panelClip.addRoundedRectangle (panelR, 22.0f * scale);
            g.saveState();
            g.reduceClipRegion (panelClip);
            g.drawImageTransformed (ui, juce::AffineTransform::scale (0.5f).translated (-camX, -camY).scaled (scale).translated (V.getX(), V.getY()), false);
            g.restoreState();
            g.setColour (juce::Colour (0xff3a3266));
            g.drawRoundedRectangle (panelR, 22.0f * scale, 2.0f);

            // Spellbook: highlight each control as it is mentioned
            if (book)
            {
                const int row = t < barT (20) - 0.4 ? 0 : (t < barT (21) + kBar * 0.65 ? 1 : (t < barT (21) + kBar * 0.65 + 0.75 ? 2 : 3));
                const float a = 0.55f + 0.45f * (float) std::sin (t * 9.0);
                const auto r = uiToScreen ({ 482.0f, 204.0f + row * 50.0f, 444.0f, 50.0f }).expanded (6);
                for (int k = 3; k >= 1; --k) { g.setColour (accent.withAlpha (0.12f * a)); g.drawRoundedRectangle (r.expanded ((float) k * 4), 14.0f, 4.0f); }
                g.setColour (accent.withAlpha (a)); g.drawRoundedRectangle (r, 12.0f, 3.0f);
            }
            // Dark/Bright switch: ring on the Dark button when it flips
            if (t >= barT (18) && t < barT (19))
            {
                const float a = fadeIn (t, barT (18) + kBeat * 0.6, barT (19), 0.15);
                const auto r = uiToScreen ({ 656.0f, 33.0f, 168.0f, 42.0f }).expanded (6);
                g.setColour (accent.withAlpha (a)); g.drawRoundedRectangle (r, 14.0f, 3.0f);
            }
            const float fl = (float) flashAt (t);
            if (fl > 0) { g.setColour (juce::Colours::white.withAlpha (fl)); g.fillRect (V); }
            g.restoreState();
        }
        // neon frame, pulsing with the kick
        {
            float pulse = 0.35f;
            if (kickOn (t)) pulse += 0.5f * (float) std::exp (-std::fmod (t, kBeat) * 10.0);
            for (int k = 4; k >= 1; --k) { g.setColour (accent.withAlpha (0.07f * pulse * (5 - k))); g.fillRect (V.withY (V.getY() - k * 3.0f).withHeight (2.0f)); g.fillRect (V.withY (V.getBottom() + k * 3.0f - 2).withHeight (2.0f)); }
            g.setColour (accent.withAlpha (0.4f + 0.6f * pulse)); g.fillRect (V.withHeight (2.0f)); g.fillRect (V.withY (V.getBottom() - 2).withHeight (2.0f));
        }

        // brand line (inside the safe zone)
        const bool hero = t >= barT (22);
        if (! hero)
        {
            g.setColour (juce::Colour (0xffc9a646));
            g.setFont (font (28.0f, true).withExtraKerningFactor (0.3f));
            g.drawText ("EPH SAMPLES  |  EUPHORIA", 0, 252, W, 36, juce::Justification::centred);
        }

        for (auto& c : captions) drawCaption (g, juce::String (juce::CharPointer_UTF8 (c.big)), juce::String (juce::CharPointer_UTF8 (c.small)), fadeIn (t, c.from, c.to), accent);

        // A/B: big state label + level-matched tag
        if (t >= barT (14) && t < barT (18))
        {
            const bool dry = bypassAt (t);
            const double since = t - barT ((double) (int) (t / kBar));
            const float pop = (float) (1.0 + 0.08 * std::exp (-since * 12.0));
            g.setColour ((dry ? juce::Colour (0xffd8d6e6) : accent).withAlpha (fadeIn (t, barT (14), barT (18), 0.15)));
            g.setFont (font (110.0f * pop, true).withExtraKerningFactor (0.06f));
            g.drawText (dry ? "DRY" : "EUPHORIA", juce::Rectangle<int> (40, 300, W - 80, 120), juce::Justification::centredBottom);
            g.setColour (juce::Colours::white.withAlpha (0.85f));
            g.setFont (font (36.0f, true).withExtraKerningFactor (0.15f));
            g.drawText ("LEVEL-MATCHED A/B", juce::Rectangle<int> (40, 430, W - 80, 44), juce::Justification::centred);
        }

        // hero end card: logo, title, URL
        if (hero)
        {
            const float a = fadeIn (t, barT (22), kDur + 1.0, 0.4);
            if (logo)
            {
                g.setOpacity (a);
                logo->drawWithin (g, { W * 0.5f - 120.0f, 232.0f, 240.0f, 240.0f / EphLogo::aspect }, juce::RectanglePlacement::centred, a);
            }
            g.setColour (juce::Colours::white.withAlpha (a));
            g.setFont (font (64.0f, true).withExtraKerningFactor (0.06f));
            g.drawText ("EUPHORIA BY EPH SAMPLES", juce::Rectangle<int> (40, 330, W - 80, 76), juce::Justification::centred);
            g.setColour (accent.withAlpha (a));
            g.setFont (font (38.0f, false));
            g.drawText ("Discover EUPHORIA", juce::Rectangle<int> (40, 408, W - 80, 46), juce::Justification::centred);
            const float pa = fadeIn (t, barT (22) + 0.5, kDur + 1.0, 0.4);
            auto pill = juce::Rectangle<float> (W * 0.5f - 250.0f, 466.0f, 500.0f, 84.0f);
            g.setColour (accent.withAlpha (pa)); g.fillRoundedRectangle (pill, 42.0f);
            g.setColour (juce::Colour (0xff07060f).withAlpha (pa));
            g.setFont (font (42.0f, true));
            g.drawText ("ephoric.nl/shop", pill.toNearestInt(), juce::Justification::centred);
        }

        if (pipe) pipe->write (frame);
        else
        {
            auto f = out.getChildFile ("v2_" + juce::String (t, 1) + ".jpg");
            f.deleteFile();
            juce::FileOutputStream os (f);
            juce::JPEGImageFormat jf; jf.setQuality (0.9f);
            jf.writeImageToStream (frame, os);
        }
        if (i % 150 == 0) std::printf ("frame %d / %d\n", i, frames);
    }
    const int rv = pipe ? pipe->close() : 0;
    delete pipe;

    // mix: processed lead + pad + kick, gentle bus saturation, peak -1 dBFS, fade at the very end
    for (int c = 0; c < 2; ++c)
        for (int i = 0; i < n; ++i)
        {
            const float v = leadOut.getSample (c, i) + pad.getSample (c, i) + kick.getSample (c, i);
            mix.setSample (c, i, std::tanh (v * 1.15f) / 1.15f);
        }
    const float pk = mix.getMagnitude (0, n);
    if (pk > 0) mix.applyGain (0.89f / pk);
    mix.applyGainRamp (n - (int) (0.6 * kSr), (int) (0.6 * kSr), 1.0f, 0.0f);
    writeWav (out.getChildFile ("music.wav"), mix);
    writeWav (out.getChildFile ("lead_euphoria.wav"), leadOut);
    writeWav (out.getChildFile ("lead_dry.wav"), lead);

    // loudness report: dry vs processed lead in the A/B bars (only the lead, same notes)
    std::printf ("A/B lead RMS: dry %.2f / euphoria %.2f / dry %.2f / euphoria %.2f dB\n",
                 rms (leadOut, barT (14) + 0.05, barT (15)), rms (leadOut, barT (15) + 0.05, barT (16)),
                 rms (leadOut, barT (16) + 0.05, barT (17)), rms (leadOut, barT (17) + 0.05, barT (18)));
    std::printf ("hook: dry bar %.2f dB, preview bar %.2f dB\n", rms (leadOut, 0.05, barT (1)), rms (leadOut, barT (1) + 0.05, barT (2) - 0.15));
    std::printf ("done %d\n", rv);
    return rv == 0 ? 0 : 1;
}
