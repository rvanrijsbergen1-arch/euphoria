#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Licensing.h"

juce::AudioProcessorValueTreeState::ParameterLayout EuphoriaProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    l.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "amount", 1 }, "Euphoria",
                                                        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 45.0f,
                                                        juce::AudioParameterFloatAttributes().withLabel ("%")
                                                            .withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v)) + " %"; })));
    l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "tone", 1 }, "Tone", juce::StringArray { "Dark", "Bright" }, 1));
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "bypass", 1 }, "Bypass", false));
    return l;
}

EuphoriaProcessor::EuphoriaProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "EUPHORIA", createLayout())
{
    amountParam = apvts.getRawParameterValue ("amount");
    toneParam = apvts.getRawParameterValue ("tone");
    bypassParam = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter ("bypass"));
    unlockedCache = lic::sharedLicense().isUnlocked();
}

bool EuphoriaProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono()) return false;
    return layouts.getMainInputChannelSet() == out;
}

void EuphoriaProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.setAmount (amount01());
    engine.setBright (isBright());
    engine.setBypassed (isBypassed() || ! isUnlocked());
    engine.prepare (sampleRate, samplesPerBlock);
    monoScratch.assign ((size_t) juce::jmax (1, samplesPerBlock) * 2, 0.0f);
}

void EuphoriaProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c) buffer.clear (c, 0, n);

    if (++licenseTick % 200 == 0) unlockedCache = lic::sharedLicense().isUnlocked();

    engine.setAmount (amount01());
    engine.setBright (isBright());
    engine.setBypassed (isBypassed() || ! isUnlocked());

    double bpm = 150.0, ppq = -1.0;
    bool playing = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            if (auto p = pos->getPpqPosition()) ppq = *p;
            playing = pos->getIsPlaying();
        }
    engine.setTempo (bpm, ppq, playing);

    float* l = buffer.getWritePointer (0);
    if (buffer.getNumChannels() >= 2)
        engine.process (l, buffer.getWritePointer (1), n);
    else
    {
        // mono track: run in stereo and fold back
        if ((int) monoScratch.size() < n) monoScratch.resize ((size_t) n);   // only if the host exceeds its announced block size
        float* r = monoScratch.data();
        std::copy (l, l + n, r);
        engine.process (l, r, n);
        for (int i = 0; i < n; ++i) l[i] = 0.5f * (l[i] + r[i]);
    }

    const float pk = buffer.getMagnitude (0, n);
    outputLevel = juce::jmax (pk, outputLevel.load() * 0.85f);
}

juce::AudioProcessorEditor* EuphoriaProcessor::createEditor() { return new EuphoriaEditor (*this); }

void EuphoriaProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
}

void EuphoriaProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType())) apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new EuphoriaProcessor(); }
