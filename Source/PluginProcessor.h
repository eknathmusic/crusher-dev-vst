#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "DSP/Distortion.h"
#include "DSP/ResonantFilter.h"
#include "DSP/Compressor.h"

class CamelCloneAudioProcessor : public juce::AudioProcessor
{
public:
    CamelCloneAudioProcessor();
    ~CamelCloneAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Public + APVTS so a future custom editor (or the generic one used for
    // now) can bind directly to it. Parameter IDs match the module knobs
    // confirmed by research: Tube/Mech (distortion), Cutoff/Resonance
    // (filter), Comp Amount/Phat (compressor), Volume/Mix (master).
    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Distortion is stateless (pure function of input + current params),
    // so one shared instance is enough. Filter and compressor both hold
    // per-sample state (ic1eq/ic2eq, envDb) and MUST be one-per-channel or
    // channels will bleed into each other's envelopes/ringing.
    DistortionStage distortion;
    std::vector<ResonantLowPass> filter;
    std::vector<CamelStyleCompressor> compressor;

    std::atomic<float>* tubeParam = nullptr;
    std::atomic<float>* mechParam = nullptr;
    std::atomic<float>* cutoffParam = nullptr;
    std::atomic<float>* resonanceParam = nullptr;
    std::atomic<float>* compAmountParam = nullptr;
    std::atomic<float>* compPhatParam = nullptr;
    std::atomic<float>* volumeParam = nullptr;
    std::atomic<float>* mixParam = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CamelCloneAudioProcessor)
};
