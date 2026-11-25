#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "../dsp/Engine.h"

class EuphoriaProcessor : public juce::AudioProcessor
{
public:
    EuphoriaProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Euphoria"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 6.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;
    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParam; }

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    /** For the UI (message thread). */
    float amount01() const { return amountParam->load() * 0.01f; }
    bool isBright() const { return toneParam->load() > 0.5f; }
    bool isBypassed() const { return bypassParam->get(); }
    bool isUnlocked() const { return unlockedCache.load(); }
    eup::Trims trims() const
    {
        eup::Trims t;
        t.width = trimParams[0]->load() * 0.01f; t.reverb = trimParams[1]->load() * 0.01f;
        t.drive = trimParams[2]->load() * 0.01f; t.ott = trimParams[3]->load() * 0.01f;
        return t;
    }
    eup::Mapping mapping (float amount) const { return eup::Mapping::forAmount (amount, isBright(), trims()); }

    /** Output level (0..1, peak with decay) so Ephy's orb can pulse with the audio. */
    std::atomic<float> outputLevel { 0.0f };

private:
    eup::Engine engine;
    std::atomic<float>* amountParam = nullptr;
    std::atomic<float>* toneParam = nullptr;
    juce::AudioParameterBool* bypassParam = nullptr;
    std::atomic<float>* trimParams[4] {};
    std::atomic<bool> unlockedCache { true };
    int licenseTick = 0;
    std::vector<float> monoScratch;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EuphoriaProcessor)
};
