#pragma once
// Ephy the Wizard: the EPH SAMPLES mascot. His power follows the EUPHORIA knob:
// aura, magic circle, staff orb, light rays, sparks and glowing eyes all grow with the amount.
#include <juce_gui_basics/juce_gui_basics.h>

class EphyView : public juce::Component
{
public:
    EphyView();
    void setState (float power01, juce::Colour accent, bool asleep, float audioLevel, double timeSec);
    void paint (juce::Graphics&) override;

    static juce::String rankFor (float power01, bool asleep);
    static juce::String spellFor (float power01, bool asleep);

private:
    void rebuildBody();
    std::unique_ptr<juce::Drawable> body;
    float power = 0.45f, level = 0.0f;
    bool sleeping = false;
    double t = 0.0;
    juce::Colour accent { 0xff5ee7ff };
};
