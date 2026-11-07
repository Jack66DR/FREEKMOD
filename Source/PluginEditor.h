#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "manners/MannersFace.h"
#include "manners/MannersMenu.h"

namespace freekmod::ui
{
    // The positions of a switch-like knob, printed round it, the current one lit.
    class Legend : public juce::Component
    {
    public:
        Legend (FreekmodProcessor&, const char* paramId, juce::StringArray words, juce::Point<float> centre, float radius);
        void update();
        void paint (juce::Graphics&) override;

    private:
        juce::RangedAudioParameter* param = nullptr;
        juce::StringArray words;
        juce::Point<float> c;
        float r;
        int shown = -1;
    };

    // The LCD: the current algorithm drawn as boxes and wires, carriers along the bottom. When the
    // SPANNER gets going, the stray wires it adds show up dotted. Click to step to the next algorithm.
    class AlgDisplay : public juce::Component
    {
    public:
        explicit AlgDisplay (FreekmodProcessor&);
        void update();
        void paint (juce::Graphics&) override;
        void mouseUp (const juce::MouseEvent&) override;

    private:
        FreekmodProcessor& proc;
        int alg = -1, seed = -1;
        float spanner = -1.0f, lit = 0.0f;
        bool loose = false;
    };

    // Under each operator: its wave, drawn, and whether this algorithm makes it a CARRIER or a MOD.
    class OpBadge : public juce::Component
    {
    public:
        OpBadge (FreekmodProcessor&, int op);
        void update();
        void paint (juce::Graphics&) override;

    private:
        FreekmodProcessor& proc;
        int op, wave = -1, alg = -1;
    };

    // The SPANNER's own little LCD: how wrecked it is, and the seed. Hold it to hear a chord.
    class Mood : public juce::Component
    {
    public:
        explicit Mood (FreekmodProcessor&);
        void update();
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;

    private:
        FreekmodProcessor& proc;
        float spanner = -1.0f, lit = 0.0f;
        int seed = -1;
        bool loose = false, sounding = false;
    };
}

class FreekmodEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit FreekmodEditor (FreekmodProcessor&);
    ~FreekmodEditor() override;

    void paint (juce::Graphics&) override {}
    void resized() override;

    // for the offline snapshot
    void pump() { timerCallback(); }
    void openMenu (manners::MenuCard::Page);

private:
    void timerCallback() override;
    void setScale (float);
    float scale() const { return (float) getWidth() / (float) manners::kBaseW; }

    FreekmodProcessor& proc;
    manners::Face face;
    freekmod::ui::AlgDisplay algDisplay;
    freekmod::ui::Mood mood;
    std::vector<std::unique_ptr<freekmod::ui::OpBadge>> badges;

    std::vector<std::unique_ptr<manners::Dial>> dials;
    std::vector<std::unique_ptr<manners::Lamp>> lamps;
    std::vector<std::unique_ptr<freekmod::ui::Legend>> legends;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> buttonAttachments;
    manners::NameStrip strip;
    manners::MenuIcon menuIcon;
    manners::MenuCard menu;
    juce::String shownPreset;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FreekmodEditor)
};
