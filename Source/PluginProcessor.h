#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Engine.h"
#include "manners/MannersPresets.h"

namespace ParamIDs
{
    inline constexpr const char* alg      = "alg";
    inline constexpr const char* ratio[4] = { "ratio1", "ratio2", "ratio3", "ratio4" };
    inline constexpr const char* level[4] = { "level1", "level2", "level3", "level4" };
    inline constexpr const char* wave[4]  = { "wave1", "wave2", "wave3", "wave4" };
    // TIMBRE
    inline constexpr const char* bright   = "bright";
    inline constexpr const char* bite     = "bite";
    inline constexpr const char* tail     = "tail";
    inline constexpr const char* feedback = "feedback";
    inline constexpr const char* touch    = "touch";
    // CHARACTER
    inline constexpr const char* detune   = "detune";
    inline constexpr const char* grit     = "grit";
    inline constexpr const char* vibrato  = "vibrato";
    // SPANNER
    inline constexpr const char* spanner  = "spanner";
    inline constexpr const char* seed     = "seed";
    inline constexpr const char* loose    = "loose";
    // AMPLIFIER
    inline constexpr const char* attack   = "attack";
    inline constexpr const char* decay    = "decay";
    inline constexpr const char* sustain  = "sustain";
    inline constexpr const char* release  = "release";
    inline constexpr const char* ampVel   = "ampVel";
    inline constexpr const char* volume   = "volume";
}

class FreekmodProcessor : public juce::AudioProcessor
{
public:
    FreekmodProcessor();
    ~FreekmodProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "FREEKMOD"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 10.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    // THROW: a new seed for the SPANNER, as one undoable step
    void throwSpanner();

    juce::UndoManager undo;                    // declared before apvts: apvts records into it
    juce::AudioProcessorValueTreeState apvts;
    std::unique_ptr<manners::Presets> presets;

    fm::Synth synth;                           // the face reads its voice meters directly

    // the algorithm window's chord: the face asks, the audio thread plays it
    enum FaceChord { none = 0, press, letGo };
    std::atomic<int> faceChord { none };
    static constexpr int kChord[] = { 48, 55, 60, 64, 67 };

private:
    void pullParams();
    void handle (const juce::MidiMessage&);
    float get (const char* id) const { return apvts.getRawParameterValue (id)->load(); }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FreekmodProcessor)
};
