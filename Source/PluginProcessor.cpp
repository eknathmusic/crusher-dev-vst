#include "PluginProcessor.h"

CamelCloneAudioProcessor::CamelCloneAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    tubeParam       = apvts.getRawParameterValue ("tube");
    mechParam       = apvts.getRawParameterValue ("mech");
    cutoffParam     = apvts.getRawParameterValue ("cutoff");
    resonanceParam  = apvts.getRawParameterValue ("resonance");
    compAmountParam = apvts.getRawParameterValue ("compAmount");
    compPhatParam   = apvts.getRawParameterValue ("compPhat");
    volumeParam     = apvts.getRawParameterValue ("volume");
    mixParam        = apvts.getRawParameterValue ("mix");
}

juce::AudioProcessorValueTreeState::ParameterLayout CamelCloneAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "tube", 1 }, "Tube",
        juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "mech", 1 }, "Mech",
        juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f));

    // Skewed range so the knob spends more travel in the musically useful
    // low/mid frequencies rather than half the knob living above 5kHz.
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "cutoff", 1 }, "Cutoff",
        juce::NormalisableRange<float> (20.0f, 20000.0f, 0.0f, 0.3f), 20000.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "resonance", 1 }, "Resonance",
        juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "compAmount", 1 }, "Comp Amount",
        juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "compPhat", 1 }, "Phat", false));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "volume", 1 }, "Volume",
        juce::NormalisableRange<float> (-24.0f, 24.0f), 0.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "mix", 1 }, "Mix",
        juce::NormalisableRange<float> (0.0f, 1.0f), 1.0f));

    return { params.begin(), params.end() };
}

void CamelCloneAudioProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    const int numChannels = juce::jmax (getTotalNumInputChannels(), getTotalNumOutputChannels(), 1);

    filter.assign ((size_t) numChannels, ResonantLowPass{});
    compressor.assign ((size_t) numChannels, CamelStyleCompressor{});

    for (auto& f : filter)      f.prepare (sampleRate);
    for (auto& c : compressor)  c.prepare (sampleRate);
}

bool CamelCloneAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto set = layouts.getMainOutputChannelSet();
    if (set != juce::AudioChannelSet::mono() && set != juce::AudioChannelSet::stereo())
        return false;
    return set == layouts.getMainInputChannelSet();
}

void CamelCloneAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    const float tube        = tubeParam->load();
    const float mech        = mechParam->load();
    const float cutoff      = cutoffParam->load();
    const float resonance   = resonanceParam->load();
    const float compAmount  = compAmountParam->load();
    const bool  phat        = compPhatParam->load() > 0.5f;
    const float volumeDb    = volumeParam->load();
    const float mix         = mixParam->load();

    // NOTE: params updated once per block, not per-sample -- fine for a
    // skeleton, but will zipper-click on fast automation. Swap in
    // juce::SmoothedValue<float> per parameter before shipping.
    distortion.setParams (tube, mech);

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto& f = filter[(size_t) ch];
        auto& c = compressor[(size_t) ch];
        f.setParams (cutoff, resonance);
        c.setParams (compAmount, phat);

        auto* data = buffer.getWritePointer (ch);
        for (int i = 0; i < numSamples; ++i)
        {
            const float dry = data[i];

            float wet = distortion.processSample (dry);
            wet = f.processSample (wet);
            wet = c.processSample (wet);

            const float blended = mix * wet + (1.0f - mix) * dry;
            const float withVolume = blended * juce::Decibels::decibelsToGain (volumeDb);

            // Master safety stage -- same fix full_chain_prototype.py needed:
            // the filter's resonant peak can amplify whatever distortion
            // harmonic lands near the cutoff well past unity. Without this,
            // that combination hard-clips instead of saturating gracefully.
            data[i] = std::tanh (withVolume);
        }
    }
}

juce::AudioProcessorEditor* CamelCloneAudioProcessor::createEditor()
{
    // Generic editor for now -- every APVTS parameter gets an auto slider.
    // Deliberately not building custom UI yet per the DSP-first plan; swap
    // this for a real PluginEditor once the sound is locked in.
    return new juce::GenericAudioProcessorEditor (*this);
}

void CamelCloneAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void CamelCloneAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

// This function must exist in exactly one file linked into the plugin --
// JUCE's plugin wrapper code calls it to obtain the processor instance.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CamelCloneAudioProcessor();
}
