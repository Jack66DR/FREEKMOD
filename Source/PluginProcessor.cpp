#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Factory.h"

namespace
{
    juce::String pctText (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)); }

    juce::String secondsText (float v, int)
    {
        if (v < 0.01f) return juce::String (v * 1000.0f, 1) + " ms";
        if (v < 1.0f) return juce::String (juce::roundToInt (v * 1000.0f)) + " ms";
        return juce::String (v, 2) + " s";
    }

    // a 0.1 ms grid: a value that comes back from undo maps to exactly the same value again, so the
    // host's round trip doesn't log a phantom change and wipe the redo
    juce::NormalisableRange<float> centred (float lo, float hi, float centre)
    {
        juce::NormalisableRange<float> r (lo, hi, 0.0001f);
        r.setSkewForCentre (centre);
        return r;
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout FreekmodProcessor::createLayout()
{
    using namespace juce;
    using F = AudioParameterFloat;
    using I = AudioParameterInt;
    using B = AudioParameterBool;
    using C = AudioParameterChoice;
    using A = AudioParameterFloatAttributes;
    namespace P = ParamIDs;
    AudioProcessorValueTreeState::ParameterLayout l;

    auto pct = [] (const char* id, const char* name, float def)
    {
        return std::make_unique<F> (ParameterID { id, 1 }, name, NormalisableRange<float> (0.0f, 1.0f), def,
                                    A().withStringFromValueFunction (pctText));
    };
    auto seconds = [] (const char* id, const char* name, float lo, float hi, float centre, float def)
    {
        return std::make_unique<F> (ParameterID { id, 1 }, name, centred (lo, hi, centre), def,
                                    A().withStringFromValueFunction (secondsText));
    };

    StringArray algs, ratios, waves;
    for (int i = 0; i < fm::kAlgs; ++i) algs.add (String (i + 1));
    for (int i = 0; i < fm::kRatioCount; ++i)
    {
        const double r = fm::ratioValue (i);
        ratios.add (r == std::floor (r) ? String ((int) r) : String (r, 2).trimCharactersAtEnd ("0"));
    }
    for (int i = 0; i < fm::kWaves; ++i) waves.add (fm::waveName (i));

    const fm::Params d;
    l.add (std::make_unique<C> (ParameterID { P::alg, 1 }, "Algorithm", algs, d.alg));
    for (int k = 0; k < 4; ++k)
    {
        const String op = "Op " + String (k + 1) + " ";
        l.add (std::make_unique<C> (ParameterID { P::ratio[k], 1 }, op + "Ratio", ratios, d.ratio[(size_t) k]));
        l.add (pct (P::level[k], (op + "Level").toRawUTF8(), d.level[(size_t) k]));
        l.add (std::make_unique<C> (ParameterID { P::wave[k], 1 }, op + "Wave", waves, d.wave[(size_t) k]));
    }

    // TIMBRE
    l.add (pct (P::bright, "Bright", d.bright));
    l.add (seconds (P::bite, "Bite", 0.02f, 6.0f, 0.5f, d.bite));
    l.add (pct (P::tail, "Tail", d.tail));
    l.add (pct (P::feedback, "Feedback", d.feedback));
    l.add (pct (P::touch, "Touch", d.touch));
    // CHARACTER
    l.add (pct (P::detune, "Detune", d.detune));
    l.add (pct (P::grit, "Grit", d.grit));
    l.add (pct (P::vibrato, "Vibrato", d.vibrato));
    // SPANNER
    l.add (pct (P::spanner, "Spanner", d.spanner));
    l.add (std::make_unique<I> (ParameterID { P::seed, 1 }, "Spanner Seed", 0, 9999, d.seed));
    l.add (std::make_unique<B> (ParameterID { P::loose, 1 }, "Spanner Loose", d.loose));
    // AMPLIFIER
    l.add (seconds (P::attack, "Attack", 0.001f, 5.0f, 0.2f, d.attack));
    l.add (seconds (P::decay, "Decay", 0.01f, 20.0f, 1.5f, d.decay));
    l.add (pct (P::sustain, "Sustain", d.sustain));
    l.add (seconds (P::release, "Release", 0.005f, 10.0f, 0.6f, d.release));
    l.add (pct (P::ampVel, "Velocity", d.ampVel));
    l.add (pct (P::volume, "Volume", d.volume));
    return l;
}

FreekmodProcessor::FreekmodProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, &undo, "PARAMS", createLayout())
{
    presets = std::make_unique<manners::Presets> (apvts, undo, fm::presetSetup());
}

bool FreekmodProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void FreekmodProcessor::prepareToPlay (double sampleRate, int)
{
    synth.prepare (sampleRate);
}

void FreekmodProcessor::throwSpanner()
{
    if (auto* prm = apvts.getParameter (ParamIDs::seed))
    {
        presets->beginGesture ("throw");
        const int now = juce::roundToInt (prm->convertFrom0to1 (prm->getValue()));
        int next = now;
        while (next == now)
            next = juce::Random::getSystemRandom().nextInt (10000);
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 ((float) next));
        prm->endChangeGesture();
    }
}

void FreekmodProcessor::pullParams()
{
    namespace P = ParamIDs;
    auto& p = synth.p;
    auto idx = [this] (const char* id) { return juce::roundToInt (get (id)); };

    p.alg = juce::jlimit (0, fm::kAlgs - 1, idx (P::alg));
    for (int k = 0; k < 4; ++k)
    {
        p.ratio[(size_t) k] = idx (P::ratio[k]);
        p.level[(size_t) k] = get (P::level[k]);
        p.wave[(size_t) k] = juce::jlimit (0, fm::kWaves - 1, idx (P::wave[k]));
    }
    p.bright = get (P::bright);
    p.bite = get (P::bite);
    p.tail = get (P::tail);
    p.feedback = get (P::feedback);
    p.touch = get (P::touch);
    p.detune = get (P::detune);
    p.grit = get (P::grit);
    p.vibrato = get (P::vibrato);
    p.spanner = get (P::spanner);
    p.seed = idx (P::seed);
    p.loose = get (P::loose) > 0.5f;
    p.attack = get (P::attack);
    p.decay = get (P::decay);
    p.sustain = get (P::sustain);
    p.release = get (P::release);
    p.ampVel = get (P::ampVel);
    p.volume = get (P::volume);
}

void FreekmodProcessor::handle (const juce::MidiMessage& m)
{
    if (m.isNoteOn())
        synth.noteOn (m.getNoteNumber(), m.getFloatVelocity());
    else if (m.isNoteOff())
        synth.noteOff (m.getNoteNumber());
    else if (m.isPitchWheel())
        synth.setBend ((float) (m.getPitchWheelValue() - 8192) / 8192.0f * 2.0f);
    else if (m.isController())
    {
        const int cc = m.getControllerNumber(), v = m.getControllerValue();
        if (cc == 1) synth.setWheel ((float) v / 127.0f);
        else if (cc == 64) synth.setSustain (v >= 64);
        else if (cc == 120) synth.allOff (true);
        else if (cc == 123) synth.allOff (false);
    }
    else if (m.isAllNotesOff())
        synth.allOff (false);
}

void FreekmodProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();
    if (n == 0)
        return;

    pullParams();
    switch (faceChord.exchange (none))
    {
        case press:  for (int k : kChord) synth.noteOn (k, 0.8f); break;
        case letGo:  for (int k : kChord) synth.noteOff (k); break;
        default: break;
    }

    float* L = buffer.getWritePointer (0);
    float* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;

    auto renderRange = [&] (int from, int to)
    {
        if (R != nullptr)
        {
            synth.render (L + from, R + from, to - from);
            return;
        }
        float tmp[256];   // mono host: fold the two sides down, a chunk at a time
        for (int i = from; i < to; i += 256)
        {
            const int m = juce::jmin (256, to - i);
            synth.render (L + i, tmp, m);
            for (int k = 0; k < m; ++k) L[i + k] = 0.5f * (L[i + k] + tmp[k]);
        }
    };

    int pos = 0;
    for (const auto meta : midi)
    {
        const int at = juce::jlimit (0, n, meta.samplePosition);
        if (at > pos)
        {
            renderRange (pos, at);
            pos = at;
        }
        handle (meta.getMessage());
    }
    if (pos < n)
        renderRange (pos, n);
}

void FreekmodProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void FreekmodProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* FreekmodProcessor::createEditor()
{
    return new FreekmodEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FreekmodProcessor();
}
