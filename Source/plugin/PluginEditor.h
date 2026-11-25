#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"
#include "Ephy.h"

namespace eupui
{
inline const juce::Colour bg { 0xff07060f }, panel { 0xff110d22 }, border { 0xff2a2348 }, text { 0xfff2f2f7 }, dim { 0xffa39cc4 },
                          gold { 0xffc9a646 }, cyan { 0xff5ee7ff }, violet { 0xff9b7bff }, off { 0xff3a3a55 }, orange { 0xffffb547 };

inline juce::Font font (float h, bool bold = false)
{
    return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain));
}

/** The big magic knob. */
class MagicKnob : public juce::Slider
{
public:
    MagicKnob();
    void paint (juce::Graphics&) override;
    juce::Colour accent { cyan };
    bool asleep = false;
    double time = 0.0;
};

/** Small pill button (Dark / Bright / Bypass / license). */
class PillButton : public juce::Button
{
public:
    explicit PillButton (const juce::String& name) : juce::Button (name) {}
    void paintButton (juce::Graphics&, bool over, bool down) override;
    juce::Colour accent { cyan };
    bool invertWhenOn = false;   // bypass: white when on
    bool subtle = false;         // license/status button
    juce::Colour textColour { dim };
};

class LicensePanel : public juce::Component
{
public:
    LicensePanel();
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();
    juce::TextEditor key;
    juce::TextButton activate { "Activate" }, buy { "Buy" }, deactivate { "Deactivate this computer" }, close { "Close" };
    juce::String message;
};
}

class EuphoriaEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit EuphoriaEditor (EuphoriaProcessor&);
    ~EuphoriaEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    juce::Colour accentColour() const;

    EuphoriaProcessor& proc;
    eupui::MagicKnob knob;
    eupui::PillButton dark { "DARK" }, bright { "BRIGHT" }, bypass { "BYPASS" }, licenseButton { "license" };
    EphyView ephy;
    eupui::LicensePanel licensePanel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> knobAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttach;
    std::unique_ptr<juce::Drawable> logo;
    double startMs = juce::Time::getMillisecondCounterHiRes();
    juce::Rectangle<int> stagesArea, readoutArea, rankArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EuphoriaEditor)
};
