// Renders the EUPHORIA promo videos with the real plugin: the real editor is filmed while the knob follows a script,
// and the soundtrack is a lead processed by the real engine with the same automation.
// Output: story (1080x1920) and feed (1080x1350) silent videos + soundtrack.wav, muxed afterwards with ffmpeg.
// Usage: eup_promo <outdir>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "../Source/plugin/PluginProcessor.h"
#include "../Source/plugin/PluginEditor.h"
#include <cstdio>

namespace
{
constexpr int kFps = 30;
constexpr double kDur = 40.0, kSr = 48000.0, kBpm = 150.0;

double smooth (double x) { x = juce::jlimit (0.0, 1.0, x); return x * x * (3 - 2 * x); }
double ramp (double t, double t0, double t1, double a, double b) { return a + (b - a) * smooth ((t - t0) / (t1 - t0)); }

float amountAt (double t)
{
    if (t < 3.0) return 0.0f;
    if (t < 8.0) return (float) ramp (t, 3.0, 7.5, 0.0, 0.30);
    if (t < 16.0) return (float) ramp (t, 8.0, 15.0, 0.30, 0.70);
    if (t < 22.0) return (float) ramp (t, 16.0, 20.0, 0.70, 1.00);
    if (t < 34.0) return 1.0f;
    return (float) ramp (t, 34.0, 35.5, 1.0, 0.85);
}
bool bypassAt (double t) { return (t >= 23.0 && t < 24.6) || (t >= 25.8 && t < 27.2); }
bool darkAt (double t) { return t >= 29.0 && t < 34.0; }
bool bookAt (double t) { return t >= 31.3 && t < 34.0; }

struct Caption { double from, to; const char* big; const char* small; };
const Caption captions[] = {
    { 0.0, 3.0, "Your lead sounds flat?", "" },
    { 3.0, 8.0, "CHARM", "width + shimmer" },
    { 8.0, 16.0, "CONJURE", "a hall that breathes with your kick" },
    { 16.0, 22.0, "ASCEND", "OTT + sparkle" },
    { 22.0, 28.0, "Same volume.", "Just better.  (bypass A/B)" },
    { 28.0, 34.0, "Dark or Bright.", "Or write your own spell." },
    { 34.0, 40.0, "One knob. One wizard.", "EUPHORIA by EPH SAMPLES" },
};

// ------------------------------------------------------------------ soundtrack (original progression and melody)
struct Song
{
    juce::AudioBuffer<float> lead, kick;
};

Song makeSong()
{
    const int n = (int) (kDur * kSr);
    Song s;
    s.lead.setSize (2, n); s.lead.clear();
    s.kick.setSize (2, n); s.kick.clear();
    const double beat = 60.0 / kBpm, bar = 4 * beat;
    auto midi = [] (int m) { return 440.0 * std::pow (2.0, (m - 69) / 12.0); };
    // four chords, one bar each: F minor, Db major, Ab major, Eb major (root notes and the pattern written for this video)
    const int roots[4] = { 65, 61, 68, 63 };
    const int third[4] = { 3, 4, 4, 4 };
    const int pattern[8] = { 0, 2, 1, 2, 3, 2, 1, 2 };   // index into chord tones per 8th note
    const double det[7] = { 0.0, -0.11, 0.11, -0.23, 0.23, -0.37, 0.37 };   // semitones
    double ph[7][2] {};
    for (int i = 0; i < n; ++i)
    {
        const double t = i / kSr;
        const int barIdx = (int) (t / bar);
        const int chord = barIdx % 4;
        const double inBar = std::fmod (t, bar);
        const int step = (int) (inBar / (beat * 0.5));
        const double inStep = std::fmod (inBar, beat * 0.5);
        const int tones[4] = { 0, third[chord], 7, 12 };
        const int note = roots[chord] + tones[pattern[step]] + (step == 7 && chord == 3 ? 2 : 0);
        const double f = midi (note);
        const double env = std::min (1.0, inStep / 0.004) * std::exp (-inStep * 2.2) * (inStep < beat * 0.45 ? 1.0 : std::max (0.0, 1.0 - (inStep - beat * 0.45) / 0.01));
        for (int c = 0; c < 2; ++c)
        {
            double v = 0;
            for (int k = 0; k < 7; ++k)
            {
                const double fk = f * std::pow (2.0, (det[k] + (c ? 0.02 * k : -0.02 * k)) / 12.0);
                ph[k][c] += fk / kSr; ph[k][c] -= std::floor (ph[k][c]);
                v += 2.0 * ph[k][c] - 1.0;
            }
            s.lead.setSample (c, i, (float) (v / 7.0 * env * 0.32));
        }
        // kick on every beat (unprocessed, so you hear the reverb breathing around it)
        const double tb = std::fmod (t, beat);
        const double kf = 45.0 + 160.0 * std::exp (-tb * 30.0);
        static double kph = 0.0;
        if (tb < 1.0 / kSr) kph = 0.0;
        kph += kf / kSr;
        const double kv = std::sin (2 * juce::MathConstants<double>::pi * kph) * std::exp (-tb * 6.0) * (t > 0.0 ? 1.0 : 0.0);
        const float kk = (float) std::tanh (kv * 1.6) * 0.42f;
        s.kick.setSample (0, i, kk); s.kick.setSample (1, i, kk);
    }
    return s;
}

// ------------------------------------------------------------------ frame composition
juce::Font font (float h, bool bold) { return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain)); }

float captionAlpha (const Caption& c, double t)
{
    if (t < c.from || t >= c.to) return 0.0f;
    const double a = juce::jmin (1.0, (t - c.from) / 0.3, (c.to - t) / 0.3);
    return (float) (c.to >= kDur ? juce::jmin (1.0, (t - c.from) / 0.3) : a);
}

void drawFrame (juce::Graphics& g, int W, int H, const juce::Image& ui, int uiY, double t, juce::Colour accent, bool story)
{
    juce::ColourGradient bg (juce::Colour (0xff1a1235), W * 0.5f, H * 0.45f, juce::Colour (0xff07060f), W * 0.5f, H * 1.05f, true);
    g.setGradientFill (bg);
    g.fillAll();

    {
        const auto r = juce::Rectangle<float> ((float) (W - ui.getWidth()) / 2, (float) uiY, (float) ui.getWidth(), (float) ui.getHeight());
        const bool fullWidth = ui.getWidth() >= W;
        const float radius = fullWidth ? 0.0f : 26.0f;
        g.saveState();
        juce::Path clip; clip.addRoundedRectangle (r, radius);
        g.reduceClipRegion (clip);
        g.drawImageAt (ui, (int) r.getX(), uiY);
        g.restoreState();
        g.setColour (juce::Colour (0xff2a2348));
        if (fullWidth) { g.fillRect (r.withHeight (2.0f)); g.fillRect (r.withY (r.getBottom() - 2).withHeight (2.0f)); }
        else g.drawRoundedRectangle (r.reduced (0.5f), radius, 2.0f);
    }

    const float scale = story ? 1.0f : 0.8f;
    const int capTop = story ? 230 : 46;
    const int capH = story ? 260 : 220;

    g.setColour (juce::Colour (0xffc9a646));
    g.setFont (font (26 * scale, true).withExtraKerningFactor (0.3f));
    g.drawText ("EPH SAMPLES  |  EUPHORIA", 0, story ? 150 : 22, W, 34, juce::Justification::centred);

    for (auto& c : captions)
    {
        const float a = captionAlpha (c, t);
        if (a <= 0.0f) continue;
        const float rise = (1.0f - a) * 16.0f;
        auto area = juce::Rectangle<float> (40.0f, capTop + rise, W - 80.0f, (float) capH);
        g.setColour (juce::Colours::white.withAlpha (a));
        g.setFont (font ((story ? 96.0f : 76.0f) * (juce::String (c.big).length() > 14 ? 0.78f : 1.0f), true));
        g.drawFittedText (c.big, area.removeFromTop (capH * 0.55f).toNearestInt(), juce::Justification::centredBottom, 2);
        g.setColour (accent.withAlpha (a));
        g.setFont (font (story ? 46.0f : 38.0f, false));
        g.drawFittedText (c.small, area.toNearestInt(), juce::Justification::centredTop, 2);
    }

    // call to action
    const float cta = t >= 34.0 ? (float) juce::jmin (1.0, 0.55 + (t - 34.0) / 0.5) : 0.55f;
    if (cta > 0.0f)
    {
        const int y = story ? H - 330 : uiY + ui.getHeight() + 44;
        auto pill = juce::Rectangle<float> (W * 0.5f - 300, (float) y, 600, story ? 110.0f : 96.0f);
        g.setColour (accent.withAlpha (cta));
        g.fillRoundedRectangle (pill, pill.getHeight() * 0.5f);
        g.setColour (juce::Colour (0xff07060f).withAlpha (cta));
        g.setFont (font (story ? 46.0f : 40.0f, true));
        g.drawText ("ephoric.nl/shop", pill.toNearestInt(), juce::Justification::centred);
        if (story && t >= 34.0)
        {
            g.setColour (juce::Colours::white.withAlpha (cta * 0.8f));
            g.setFont (font (34.0f, false));
            g.drawText ("VST3 / AU  |  Windows + Mac", 0, y + 140, W, 44, juce::Justification::centred);
        }
    }
}

struct Pipe
{
    FILE* f = nullptr;
    int W, H;
    Pipe (int w, int h, const juce::String& file) : W (w), H (h)
    {
        const auto cmd = juce::String::formatted ("ffmpeg -y -loglevel error -f rawvideo -pix_fmt bgra -s %dx%d -r %d -i - -c:v libx264 -crf 18 -preset veryfast -pix_fmt yuv420p -movflags +faststart \"%s\"",
                                                  w, h, kFps, file.toRawUTF8());
        f = popen (cmd.toRawUTF8(), "w");
    }
    void write (const juce::Image& img)
    {
        juce::Image::BitmapData bd (img, juce::Image::BitmapData::readOnly);
        for (int y = 0; y < H; ++y) fwrite (bd.getLinePointer (y), 4, (size_t) W, f);
    }
    int close() { return pclose (f); }
};
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File out (argc > 1 ? juce::String (argv[1]) : juce::File::getCurrentWorkingDirectory().getFullPathName());
    const bool preview = argc > 2 && juce::String (argv[2]) == "preview";
    out.createDirectory();

    EuphoriaProcessor p;
    p.setPlayConfigDetails (2, 2, kSr, 1600);
    p.prepareToPlay (kSr, 1600);
    auto* amount = p.apvts.getParameter ("amount");
    auto* tone = p.apvts.getParameter ("tone");
    auto* bypass = p.apvts.getParameter ("bypass");

    std::unique_ptr<EuphoriaEditor> ed (dynamic_cast<EuphoriaEditor*> (p.createEditor()));
    if (ed == nullptr) return 1;

    auto song = makeSong();
    juce::AudioBuffer<float> mix (2, song.lead.getNumSamples());
    mix.clear();

    const int SW = 1080, SH = 1920, FW = 1080, FH = 1350;
    Pipe story (SW, SH, out.getChildFile (preview ? "p1.mp4" : "story_silent.tmp.mp4").getFullPathName());
    Pipe feed (FW, FH, out.getChildFile (preview ? "p2.mp4" : "feed_silent.tmp.mp4").getFullPathName());
    juce::Image storyImg (juce::Image::ARGB, SW, SH, true), feedImg (juce::Image::ARGB, FW, FH, true);

    const int frames = (int) (kDur * kFps), hop = (int) (kSr / kFps);
    juce::AudioBuffer<float> blk (2, hop);
    juce::MidiBuffer midi;
    bool book = false;
    for (int i = 0; i < frames; ++i)
    {
        const double t = (double) i / kFps;
        amount->setValueNotifyingHost (amountAt (t));
        tone->setValueNotifyingHost (darkAt (t) ? 0.0f : 1.0f);
        bypass->setValueNotifyingHost (bypassAt (t) ? 1.0f : 0.0f);

        const int start = i * hop;
        for (int c = 0; c < 2; ++c) blk.copyFrom (c, 0, song.lead, c, start, hop);
        p.processBlock (blk, midi);
        for (int c = 0; c < 2; ++c) { mix.copyFrom (c, start, blk, c, 0, hop); mix.addFrom (c, start, song.kick, c, start, hop); }

        if (bookAt (t) != book) { book = bookAt (t); ed->showSpellbook (book); }
        ed->renderAt (t);
        const auto accent = p.isBright() ? juce::Colour (0xff5ee7ff) : juce::Colour (0xff9b7bff);

        const auto uiS = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1080.0f / 980.0f);
        { juce::Graphics g (storyImg); drawFrame (g, SW, SH, uiS, 600, t, accent, true); }
        const auto uiF = uiS.rescaled (1000, 714, juce::Graphics::highResamplingQuality);
        { juce::Graphics g (feedImg); drawFrame (g, FW, FH, uiF, 270, t, accent, false); }
        if (preview)
        {
            for (double pt : { 1.0, 12.0, 19.5, 24.0, 32.5, 37.0 })
                if (i == (int) (pt * kFps))
                    for (auto [img, nm] : { std::pair<juce::Image*, const char*> { &storyImg, "story" }, { &feedImg, "feed" } })
                    {
                        auto f = out.getChildFile (juce::String (nm) + "_" + juce::String (pt, 1) + ".jpg");
                        f.deleteFile();
                        juce::FileOutputStream os (f);
                        juce::JPEGImageFormat jf; jf.setQuality (0.9f);
                        jf.writeImageToStream (*img, os);
                    }
            if (t > 37.1) break;
            continue;
        }
        story.write (storyImg);
        feed.write (feedImg);
        if (i % 150 == 0) std::printf ("frame %d / %d\n", i, frames);
    }
    const int rs = story.close(), rf = feed.close();

    // soundtrack: peak to -1 dBFS, short fade out
    const float pk = mix.getMagnitude (0, mix.getNumSamples());
    if (pk > 0) mix.applyGain (0.89f / pk);
    mix.applyGainRamp (mix.getNumSamples() - (int) kSr, (int) kSr, 1.0f, 0.0f);
    auto wavFile = out.getChildFile ("soundtrack.wav");
    wavFile.deleteFile();
    juce::WavAudioFormat wav;
    if (auto os = std::unique_ptr<juce::FileOutputStream> (wavFile.createOutputStream()))
        if (auto w = std::unique_ptr<juce::AudioFormatWriter> (wav.createWriterFor (os.get(), kSr, 2, 24, {}, 0)))
        {
            os.release();
            w->writeFromAudioSampleBuffer (mix, 0, mix.getNumSamples());
        }
    std::printf ("done: story %d, feed %d\n", rs, rf);
    return rs == 0 && rf == 0 ? 0 : 1;
}
