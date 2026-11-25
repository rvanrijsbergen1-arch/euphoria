#include "PluginEditor.h"
#include "Licensing.h"
#include "EphLogo.h"

using namespace eupui;

namespace
{
constexpr int kW = 980, kH = 700;

void drawStar (juce::Graphics& g, juce::Point<float> c, float outer, float inner, float rotRad, bool fill, float stroke)
{
    juce::Path p;
    for (int i = 0; i < 8; ++i)
    {
        const float r = (i % 2 == 0) ? outer : inner;
        const float a = rotRad + (float) i * juce::MathConstants<float>::pi / 4.0f - juce::MathConstants<float>::halfPi;
        const auto pt = c + juce::Point<float> (std::cos (a), std::sin (a)) * r;
        if (i == 0) p.startNewSubPath (pt); else p.lineTo (pt);
    }
    p.closeSubPath();
    if (fill) g.fillPath (p); else g.strokePath (p, juce::PathStrokeType (stroke, juce::PathStrokeType::mitered));
}
}

// ------------------------------------------------------------------ knob
MagicKnob::MagicKnob()
{
    setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    setRange (0.0, 100.0, 0.1);
    setDoubleClickReturnValue (true, 0.0);
    setMouseDragSensitivity (300);
    setVelocityModeParameters (0.4, 1, 0.0, true, juce::ModifierKeys::shiftModifier);   // Shift-drag = fine control
    setScrollWheelEnabled (true);
    setMouseCursor (juce::MouseCursor::UpDownLeftRightResizeCursor);
    setTitle ("Euphoria amount");
}

void MagicKnob::paint (juce::Graphics& g)
{
    const float k = (float) (getValue() / 100.0);
    const float s = juce::jmin (getWidth(), getHeight()) / 320.0f;
    const juce::Point<float> c (getWidth() * 0.5f, getHeight() * 0.5f);
    const auto col = asleep ? off : accent;
    const float power = asleep ? 0.0f : k;
    const float fillDeg = k * 270.0f;

    // outer glow band
    {
        juce::ColourGradient grad (accent.withAlpha (0.0f), c, accent.withAlpha (0.0f), c.translated (160 * s, 0), true);
        grad.addColour (0.55, accent.withAlpha (0.0f));
        grad.addColour (0.75, accent.withAlpha (0.35f * (0.15f + 0.85f * power)));
        g.setGradientFill (grad);
        g.fillEllipse (juce::Rectangle<float> (320 * s, 320 * s).withCentre (c));
    }

    // 37 rune dots along the 270 degree sweep
    for (int i = 0; i <= 36; ++i)
    {
        const float deg = (float) i * 7.5f;
        const float a = juce::degreesToRadians (deg - 225.0f);
        const float r = (i % 6 == 0 ? 4.0f : 2.6f) * s;
        const auto p = c + juce::Point<float> (std::cos (a), std::sin (a)) * 146.0f * s;
        g.setColour (! asleep && deg <= fillDeg + 0.01f ? col : juce::Colour (0xff2e2752));
        g.fillEllipse (juce::Rectangle<float> (2 * r, 2 * r).withCentre (p));
    }

    // rotating rune ring with four diamonds (spins faster with more power)
    {
        const double period = 14.0 - power * 11.0;
        const float rot = (float) std::fmod (time / period * juce::MathConstants<double>::twoPi, juce::MathConstants<double>::twoPi);
        juce::Path ring, dashed;
        ring.addCentredArc (c.x, c.y, 128 * s, 128 * s, 0, 0, juce::MathConstants<float>::twoPi, true);
        const float dashes[] = { 2 * s, 10 * s, 14 * s, 10 * s };
        juce::PathStrokeType (1.5f * s).createDashedStroke (dashed, ring, dashes, 4);
        g.setColour (col.withAlpha (0.2f + 0.8f * power));
        g.fillPath (dashed, juce::AffineTransform::rotation (rot, c.x, c.y));
        for (int q = 0; q < 4; ++q)
        {
            const float a = rot + (float) q * juce::MathConstants<float>::halfPi - juce::MathConstants<float>::halfPi;
            const auto p = c + juce::Point<float> (std::cos (a), std::sin (a)) * 125.0f * s;
            juce::Path d;
            d.addQuadrilateral (p.x, p.y - 9 * s, p.x + 5 * s, p.y, p.x, p.y + 9 * s, p.x - 5 * s, p.y);
            g.fillPath (d, juce::AffineTransform::rotation (a + juce::MathConstants<float>::halfPi, p.x, p.y));
        }
        // counter-rotating gold dotted ring
        juce::Path inner, dots;
        inner.addCentredArc (c.x, c.y, 116 * s, 116 * s, 0, 0, juce::MathConstants<float>::twoPi, true);
        const float dd[] = { 1 * s, 7 * s };
        juce::PathStrokeType (1.2f * s).createDashedStroke (dots, inner, dd, 2);
        g.setColour (gold.withAlpha (0.8f));
        g.fillPath (dots, juce::AffineTransform::rotation (-rot * 0.66f, c.x, c.y));
    }

    // knob body
    {
        const auto body = juce::Rectangle<float> (208 * s, 208 * s).withCentre (c);
        juce::ColourGradient grad (juce::Colour (0xff3b2e78), body.getRelativePoint (0.4f, 0.35f), juce::Colour (0xff0c0920), body.getBottomRight(), true);
        grad.addColour (0.7, juce::Colour (0xff151030));
        g.setGradientFill (grad);
        g.fillEllipse (body);
        g.setColour (juce::Colour (0xff4a3d8f));
        g.drawEllipse (body, 2.0f * s);
    }

    // star turns and fills with the amount
    {
        const float rot = juce::degreesToRadians (fillDeg * 0.5f);
        g.setColour (col.withAlpha (0.3f + 0.7f * power));
        drawStar (g, c, 68 * s, 17 * s, rot, false, 1.5f * s);
        g.setColour (col.withAlpha (0.35f * power));
        drawStar (g, c, 44 * s, 9 * s, rot, true, 0);
    }

    // gem pointer
    {
        const float a = juce::degreesToRadians (fillDeg - 225.0f);
        const auto p = c + juce::Point<float> (std::cos (a), std::sin (a)) * 90.0f * s;
        g.setColour (juce::Colours::white);
        g.fillEllipse (juce::Rectangle<float> (16 * s, 16 * s).withCentre (p));
        g.setColour (col);
        g.drawEllipse (juce::Rectangle<float> (16 * s, 16 * s).withCentre (p), 3.0f * s);
    }

    // value
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.setFont (font (46 * s, true));
    const auto txt = juce::String (juce::roundToInt (getValue()));
    g.drawText (txt, getLocalBounds().translated (0, 2), juce::Justification::centred);
    g.setColour (text);
    g.drawText (txt, getLocalBounds(), juce::Justification::centred);
}

// ------------------------------------------------------------------ pill button
void PillButton::paintButton (juce::Graphics& g, bool over, bool)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    const bool on = getToggleState();
    if (subtle)
    {
        g.setColour (over ? eupui::text : textColour);
        g.setFont (font (12.0f));
        g.drawFittedText (getButtonText(), getLocalBounds(), juce::Justification::centred, 1);
        return;
    }
    if (invertWhenOn)
    {
        g.setColour (on ? eupui::text : juce::Colours::transparentBlack);
        g.fillRoundedRectangle (r, 10.0f);
        g.setColour (on ? eupui::text : (over ? dim : juce::Colour (0xff3a3266)));
        g.drawRoundedRectangle (r, 10.0f, 1.0f);
        g.setColour (on ? bg : juce::Colour (0xffcfc9e8));
    }
    else
    {
        if (on) { g.setColour (accent); g.fillRoundedRectangle (r, 9.0f); }
        g.setColour (on ? bg : (over ? eupui::text : dim));
    }
    g.setFont (font (12.0f, true));
    g.drawText (getButtonText(), getLocalBounds(), juce::Justification::centred);
}

// ------------------------------------------------------------------ license panel
LicensePanel::LicensePanel()
{
    key.setTextToShowWhenEmpty ("EUP-XXXX-XXXX-XXXX-XXXX", dim);
    key.setFont (font (16.0f));
    key.setJustification (juce::Justification::centred);
    for (auto* c : std::initializer_list<juce::Component*> { &key, &activate, &buy, &deactivate, &close }) addAndMakeVisible (c);
    buy.onClick = [] { juce::URL (lic::LicenseConfig::buyUrl).launchInDefaultBrowser(); };
    activate.onClick = [this]
    {
        message = "Activating...";
        repaint();
        const auto k = key.getText();
        juce::Component::SafePointer<LicensePanel> safe (this);
        juce::Thread::launch ([safe, k]
        {
            const auto err = lic::sharedLicense().activate (k);
            juce::MessageManager::callAsync ([safe, err] { if (safe) { safe->message = err.isEmpty() ? "Activated - thank you!" : err; safe->refresh(); } });
        });
    };
    deactivate.onClick = [this]
    {
        message = "Deactivating...";
        repaint();
        juce::Component::SafePointer<LicensePanel> safe (this);
        juce::Thread::launch ([safe]
        {
            const auto err = lic::sharedLicense().deactivate();
            juce::MessageManager::callAsync ([safe, err] { if (safe) { safe->message = err.isEmpty() ? "This computer is deactivated." : err; safe->refresh(); } });
        });
    };
}

void LicensePanel::refresh()
{
    const bool licensed = lic::sharedLicense().getState() == lic::LicenseManager::State::Licensed;
    key.setVisible (! licensed); activate.setVisible (! licensed); deactivate.setVisible (licensed);
    repaint();
}

void LicensePanel::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (0.75f));
    auto box = getLocalBounds().withSizeKeepingCentre (460, 300);
    g.setColour (panel);
    g.fillRoundedRectangle (box.toFloat(), 12.0f);
    g.setColour (border);
    g.drawRoundedRectangle (box.toFloat().reduced (0.5f), 12.0f, 1.0f);
    auto r = box.reduced (24);
    g.setColour (text); g.setFont (font (18.0f, true));
    g.drawText ("Euphoria license", r.removeFromTop (28), juce::Justification::centred);
    g.setColour (dim); g.setFont (font (13.0f));
    auto& L = lic::sharedLicense();
    g.drawText (L.statusText(), r.removeFromTop (22), juce::Justification::centred);
    if (L.getState() == lic::LicenseManager::State::Licensed)
        g.drawText ("Serial " + L.maskedKey() + "  |  version " + lic::currentVersion(), r.removeFromTop (20), juce::Justification::centred);
    g.setColour (message.startsWith ("Activated") ? juce::Colour (0xff3ddc84) : orange);
    g.drawFittedText (message, box.reduced (24).removeFromBottom (78).removeFromTop (34), juce::Justification::centred, 2);
}

void LicensePanel::resized()
{
    auto box = getLocalBounds().withSizeKeepingCentre (460, 300).reduced (24);
    auto b = box.withTrimmedTop (80);
    key.setBounds (b.removeFromTop (34));
    b.removeFromTop (10);
    auto row = b.removeFromTop (34);
    activate.setBounds (row.removeFromLeft (row.getWidth() / 2 - 5));
    row.removeFromLeft (10);
    buy.setBounds (row);
    deactivate.setBounds (box.withTrimmedTop (80).removeFromTop (34));
    close.setBounds (box.removeFromBottom (30).withSizeKeepingCentre (120, 30));
}

// ------------------------------------------------------------------ editor
EuphoriaEditor::EuphoriaEditor (EuphoriaProcessor& p) : juce::AudioProcessorEditor (p), proc (p)
{
    setSize (kW, kH);

    addAndMakeVisible (ephy);
    addAndMakeVisible (knob);
    knobAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "amount", knob);

    for (auto* b : { &dark, &bright }) { b->setClickingTogglesState (false); addAndMakeVisible (b); }
    dark.onClick = [this] { if (auto* t = proc.apvts.getParameter ("tone")) t->setValueNotifyingHost (0.0f); };
    bright.onClick = [this] { if (auto* t = proc.apvts.getParameter ("tone")) t->setValueNotifyingHost (1.0f); };
    dark.setTitle ("Dark tone"); bright.setTitle ("Bright tone");

    bypass.invertWhenOn = true;
    bypass.setClickingTogglesState (true);
    addAndMakeVisible (bypass);
    bypassAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "bypass", bypass);

    licenseButton.subtle = true;
    licenseButton.onClick = [this] { licensePanel.refresh(); licensePanel.setVisible (true); };
    addAndMakeVisible (licenseButton);

    licensePanel.close.onClick = [this] { licensePanel.setVisible (false); };
    addChildComponent (licensePanel);

    if (auto xml = juce::XmlDocument::parse (EphLogo::svg)) logo = juce::Drawable::createFromSVG (*xml);

    timerCallback();
    startTimerHz (30);
}

EuphoriaEditor::~EuphoriaEditor() { stopTimer(); }

juce::Colour EuphoriaEditor::accentColour() const { return proc.isBright() ? cyan : violet; }

void EuphoriaEditor::timerCallback()
{
    const double t = (juce::Time::getMillisecondCounterHiRes() - startMs) * 0.001;
    const auto accent = accentColour();
    const bool asleep = proc.isBypassed() || ! proc.isUnlocked();
    const float k = proc.amount01();

    knob.accent = accent;
    knob.asleep = asleep;
    knob.time = t;
    knob.repaint();

    dark.accent = bright.accent = accent;
    dark.setToggleState (! proc.isBright(), juce::dontSendNotification);
    bright.setToggleState (proc.isBright(), juce::dontSendNotification);
    bypass.setButtonText (proc.isBypassed() ? "BYPASSED" : "BYPASS");

    auto& L = lic::sharedLicense();
    const auto upd = L.availableUpdate();
    licenseButton.setButtonText (upd.isNotEmpty() ? "Update " + upd + " available" : L.statusText());
    licenseButton.textColour = L.isUnlocked() ? dim : orange;
    if (upd.isNotEmpty()) licenseButton.onClick = [] { juce::URL (lic::LicenseConfig::accountUrl).launchInDefaultBrowser(); };
    licenseButton.repaint();

    ephy.setState (k, accent, asleep, proc.outputLevel.load(), t);
    repaint (stagesArea.getUnion (readoutArea));
    repaint (rankArea);
}

void EuphoriaEditor::paint (juce::Graphics& g)
{
    const auto accent = accentColour();
    const bool asleep = proc.isBypassed() || ! proc.isUnlocked();
    const float k = asleep ? 0.0f : proc.amount01();

    // background
    {
        juce::ColourGradient grad (juce::Colour (0xff1a1235), (float) kW * 0.3f, (float) kH * 0.6f, bg, (float) kW * 0.95f, (float) kH * 1.1f, true);
        g.setGradientFill (grad);
        g.fillAll();
    }

    // header
    if (logo)
    {
        const float h = 18.0f;
        logo->drawWithin (g, { 36.0f, 22.0f, h * EphLogo::aspect, h }, juce::RectanglePlacement::xLeft, 1.0f);
    }
    else
    {
        g.setColour (dim); g.setFont (font (11.0f, true));
        g.drawText ("EPH SAMPLES", 36, 28, 200, 14, juce::Justification::left);
    }
    g.setColour (text);
    g.setFont (font (34.0f, true).withExtraKerningFactor (0.25f));
    g.drawText ("EUPHORIA", 36, 46, 340, 40, juce::Justification::left);

    // Ephy's name, rank and spell
    g.setColour (gold); g.setFont (font (11.0f, true).withExtraKerningFactor (0.25f));
    g.drawText ("EPHY THE WIZARD", rankArea.withHeight (14), juce::Justification::centred);
    g.setColour (k > 0.0f ? accent : juce::Colour (0xff6e6894));
    g.setFont (font (22.0f, true).withExtraKerningFactor (0.15f));
    g.drawText (EphyView::rankFor (k, asleep), rankArea.withTrimmedTop (16).withHeight (28), juce::Justification::centred);
    g.setColour (dim); g.setFont (font (12.0f));
    g.drawText (EphyView::spellFor (k, asleep), rankArea.withTrimmedTop (46).withHeight (16), juce::Justification::centred);

    // hint under the knob
    g.setColour (dim); g.setFont (font (12.0f));
    g.drawText ("Drag to cast  |  double-click resets  |  Shift = fine", knob.getBounds().withY (knob.getBottom() + 6).withHeight (16).expanded (60, 0),
                juce::Justification::centred);

    // three spell stages
    {
        struct S { const char* range; const char* name; const char* desc; float at; };
        const S stages[] = { { "0-30%", "Charm", "width  |  shimmer", 0.0005f }, { "30-70%", "Conjure", "hall  |  breathing", 0.3f }, { "70-100%", "Ascend", "OTT  |  sparkle", 0.7f } };
        auto r = stagesArea;
        const int w = (r.getWidth() - 20) / 3;
        for (int i = 0; i < 3; ++i)
        {
            auto b = r.removeFromLeft (w).toFloat();
            r.removeFromLeft (10);
            const bool lit = k >= stages[i].at && k > 0.0f;
            if (lit)
            {
                g.setColour (accent.withAlpha (0.12f)); g.fillRoundedRectangle (b, 12.0f);
                g.setColour (accent); g.drawRoundedRectangle (b.reduced (0.5f), 12.0f, 1.0f);
            }
            else
            {
                g.setColour (panel); g.fillRoundedRectangle (b, 12.0f);
                g.setColour (border); g.drawRoundedRectangle (b.reduced (0.5f), 12.0f, 1.0f);
            }
            auto tr = b.reduced (12, 9).toNearestInt();
            g.setColour (lit ? text : juce::Colour (0xff8e88b0));
            g.setFont (font (11.0f, true)); g.drawText (stages[i].range, tr.removeFromTop (14), juce::Justification::left);
            g.setFont (font (17.0f, true)); g.drawText (stages[i].name, tr.removeFromTop (22), juce::Justification::left);
            g.setFont (font (11.0f)); g.drawText (stages[i].desc, tr.removeFromTop (14), juce::Justification::left);
        }
    }

    // readouts
    {
        const auto m = eup::Mapping::forAmount (k, proc.isBright());
        auto b = readoutArea.toFloat();
        g.setColour (panel); g.fillRoundedRectangle (b, 12.0f);
        g.setColour (border); g.drawRoundedRectangle (b.reduced (0.5f), 12.0f, 1.0f);
        const juce::String labels[] = { "WIDTH", "REVERB", "DRIVE", "OTT" };
        const juce::String values[] = { juce::String (juce::roundToInt (m.width)) + "%", juce::String (juce::roundToInt (m.reverbMix * 100)) + "%",
                                        juce::String (m.driveDb, 1) + " dB", juce::String (juce::roundToInt (m.ottDepth * 100)) + "%" };
        auto r = readoutArea.reduced (12, 10);
        const int w = r.getWidth() / 4;
        for (int i = 0; i < 4; ++i)
        {
            auto c = r.removeFromLeft (w);
            g.setColour (dim); g.setFont (font (11.0f)); g.drawText (labels[i], c.removeFromTop (16), juce::Justification::centred);
            g.setColour (text); g.setFont (font (16.0f, true)); g.drawText (values[i], c, juce::Justification::centred);
        }
    }
}

void EuphoriaEditor::resized()
{
    // header
    bypass.setBounds (kW - 36 - 112, 34, 112, 40);
    bright.setBounds (bypass.getX() - 12 - 80, 37, 80, 34);
    dark.setBounds (bright.getX() - 80, 37, 80, 34);
    licenseButton.setBounds (dark.getX() - 16 - 260, 37, 260, 34);

    // left: Ephy
    rankArea = { 36, 100, 400, 64 };
    ephy.setBounds (0, 150, 472, kH - 150);   // 400 x 520 figure, with room for the aura

    // right: knob, stages, readouts
    const int rx = 36 + 400 + 28, rw = kW - 36 - rx;
    knob.setBounds (rx + (rw - 320) / 2, 104, 320, 320);
    stagesArea = { rx, 456, rw, 76 };
    readoutArea = { rx, 546, rw, 62 };

    licensePanel.setBounds (getLocalBounds());
}
