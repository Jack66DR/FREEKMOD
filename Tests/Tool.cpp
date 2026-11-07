// FREEKMOD_Tool
//   snapshot <out.png> [look][2x]   plays a chord into the plugin and renders the editor to a PNG
//   demo <out.wav>                  plays a short tour of the factory presets into a WAV
//   presets                         factory load, undo, save, load back, step, init, randomise
//
// Looks: chord (the default), quiet, menu, load, save, size, or any factory preset name with
// underscores (e_piano_02, fm_hellscape ...).

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Factory.h"

static void setParam (FreekmodProcessor& p, const char* id, float value)
{
    if (auto* prm = p.apvts.getParameter (id))
        prm->setValueNotifyingHost (prm->convertTo0to1 (value));
}

static int snapshot (const juce::File& out, const juce::String& look)
{
    juce::ScopedJuceInitialiser_GUI gui;
    FreekmodProcessor proc;
    constexpr double sr = 48000.0;
    proc.setPlayConfigDetails (0, 2, sr, 512);
    proc.prepareToPlay (sr, 512);

    for (int i = 0; i < (int) proc.presets->factory().size(); ++i)
        if (look.startsWith (proc.presets->factory()[(size_t) i].name.replaceCharacter (' ', '_').toLowerCase()))
            proc.presets->loadFactory (i);

    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    ed->setSize (manners::kBaseW, manners::kBaseH);
    const float pixelScale = look.endsWith ("2x") ? 2.0f : 1.0f;
    auto* editor = dynamic_cast<FreekmodEditor*> (ed.get());

    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer midi;
    if (! look.startsWith ("quiet"))
    {
        int k = 0;
        for (int n : { 48, 55, 60, 64, 67 })
            midi.addEvent (juce::MidiMessage::noteOn (1, n, (juce::uint8) (70 + 10 * k++)), 0);
    }
    for (int b = 0; b < 40; ++b)
    {
        proc.processBlock (buf, midi);
        midi.clear();
        if (b == 30)   // let one go, so its lamp is fading
            midi.addEvent (juce::MidiMessage::noteOff (1, 67), 0);
        if (editor != nullptr && b % 2 == 0)
            editor->pump();
    }

    if (editor != nullptr)
    {
        using Page = manners::MenuCard::Page;
        if (look.startsWith ("menu")) editor->openMenu (Page::main);
        if (look.startsWith ("load")) editor->openMenu (Page::load);
        if (look.startsWith ("save")) editor->openMenu (Page::save);
        if (look.startsWith ("size")) editor->openMenu (Page::size);
    }

    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, pixelScale);
    out.deleteFile();
    juce::FileOutputStream os (out);
    juce::PNGImageFormat().writeImageToStream (img, os);
    std::printf ("wrote %s\n", out.getFullPathName().toRawUTF8());
    ed.reset();
    return 0;
}

// a phrase per preset, so the demo shows each one off
static int demo (const juce::File& out)
{
    juce::ScopedJuceInitialiser_GUI gui;
    FreekmodProcessor proc;
    constexpr double sr = 48000.0;
    constexpr int block = 256;
    proc.setPlayConfigDetails (0, 2, sr, block);
    proc.prepareToPlay (sr, block);

    struct Ev { double t; int note; bool on; };
    std::vector<float> L, R;
    float peak = 0.0f;
    bool clean = true;

    const auto& fac = proc.presets->factory();
    for (int pi = 0; pi < (int) fac.size(); ++pi)
    {
        proc.presets->loadFactory (pi);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (20);

        std::vector<Ev> evs;
        const auto& nm = fac[(size_t) pi].name;
        if (nm.contains ("BASS"))
        {
            const int line[] = { 36, 36, 48, 36, 39, 41, 43, 34 };
            for (int i = 0; i < 8; ++i) { evs.push_back ({ i * 0.25, line[i], true }); evs.push_back ({ i * 0.25 + 0.2, line[i], false }); }
        }
        else if (nm.contains ("MALLET") || nm.contains ("WOOD") || nm.contains ("TRAY") || nm.contains ("CLAV") || nm.contains ("LEAD")
                 || nm.contains ("WHERE"))
        {
            const int line[] = { 60, 64, 67, 72, 71, 67, 64, 62 };
            for (int i = 0; i < 8; ++i) { evs.push_back ({ i * 0.25, line[i], true }); evs.push_back ({ i * 0.25 + 0.2, line[i], false }); }
        }
        else
        {
            for (int n : { 48, 55, 60, 64, 67 }) { evs.push_back ({ 0.0, n, true }); evs.push_back ({ 1.4, n, false }); }
            for (int n : { 50, 57, 62, 65, 69 }) { evs.push_back ({ 1.6, n, true }); evs.push_back ({ 2.6, n, false }); }
        }

        const double len = 3.2;
        const int total = (int) (len * sr);
        juce::AudioBuffer<float> buf (2, block);
        for (int s = 0; s < total; s += block)
        {
            juce::MidiBuffer midi;
            for (auto& e : evs)
            {
                const int at = (int) (e.t * sr);
                if (at >= s && at < s + block)
                    midi.addEvent (e.on ? juce::MidiMessage::noteOn (1, e.note, (juce::uint8) 100) : juce::MidiMessage::noteOff (1, e.note), at - s);
            }
            proc.processBlock (buf, midi);
            for (int i = 0; i < block; ++i)
            {
                const float l = buf.getSample (0, i), r = buf.getSample (1, i);
                if (! std::isfinite (l) || ! std::isfinite (r)) clean = false;
                peak = juce::jmax (peak, std::abs (l), std::abs (r));
                L.push_back (l);
                R.push_back (r);
            }
        }
        // silence between
        juce::MidiBuffer off;
        off.addEvent (juce::MidiMessage::allNotesOff (1), 0);
        proc.processBlock (buf, off);
        std::printf ("  %-22s\n", nm.toRawUTF8());
    }

    juce::AudioBuffer<float> all (2, (int) L.size());
    for (size_t i = 0; i < L.size(); ++i) { all.setSample (0, (int) i, L[i]); all.setSample (1, (int) i, R[i]); }
    out.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (out), sr, 2, 24, {}, 0));
    if (w == nullptr) return 1;
    w->writeFromAudioSampleBuffer (all, 0, all.getNumSamples());
    w.reset();
    std::printf ("wrote %s, peak %.3f, %s\n", out.getFullPathName().toRawUTF8(), peak, clean ? "no NaNs" : "NaNs FOUND");
    return clean && peak < 1.0f ? 0 : 1;
}

static int presetCheck()
{
    juce::ScopedJuceInitialiser_GUI gui;
    FreekmodProcessor proc;
    auto& ps = *proc.presets;
    namespace P = ParamIDs;
    auto real = [&proc] (const char* id) { auto* p = proc.apvts.getParameter (id); return p->convertFrom0to1 (p->getValue()); };
    auto settle = [] { juce::MessageManager::getInstance()->runDispatchLoopUntil (300); };
    int fails = 0;
    auto check = [&fails] (bool ok, const char* what) { std::printf ("  [%s] %s\n", ok ? "PASS" : "FAIL", what); fails += ok ? 0 : 1; };

    settle();
    ps.loadFactory (1);   // LATE NIGHT BASS
    settle();
    check (real (P::alg) == 3.0f && ps.currentName() == "LATE NIGHT BASS", "factory preset loads");
    ps.undo.undo();
    settle();
    check (real (P::alg) == 4.0f, "undo puts it back");
    ps.undo.redo();
    settle();
    check (real (P::alg) == 3.0f, "redo");

    const juce::String name = "TOOL CHECK " + juce::String (juce::Random::getSystemRandom().nextInt (100000));
    check (ps.save (name), "save to the preset folder");
    ps.init();
    settle();
    check (real (P::alg) == 4.0f && ps.currentName() == "INIT", "init");
    const auto file = ps.folder().getChildFile (name + ".manners");
    check (ps.loadFile (file) && real (P::alg) == 3.0f, "load it back");
    ps.step (1);
    check (ps.currentName() != name, "next preset moves on");
    ps.randomise();
    check (ps.currentName() == "RANDOMISED", "randomise");
    const float seedBefore = real (P::seed);
    proc.throwSpanner();
    settle();
    check (real (P::seed) != seedBefore, "THROW picks a new seed");
    ps.undo.undo();
    settle();
    check (real (P::seed) == seedBefore, "and undo takes it back");
    file.deleteFile();

    // every factory preset makes a sound and stays clean
    constexpr double sr = 48000.0;
    proc.setPlayConfigDetails (0, 2, sr, 512);
    proc.prepareToPlay (sr, 512);
    bool allSound = true;
    for (int i = 0; i < (int) ps.factory().size(); ++i)
    {
        ps.loadFactory (i);
        juce::AudioBuffer<float> buf (2, 512);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::allNotesOff (1), 0);
        proc.processBlock (buf, midi);
        midi.clear();
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
        float pk = 0.0f;
        for (int b = 0; b < 200; ++b)
        {
            proc.processBlock (buf, midi);
            midi.clear();
            pk = juce::jmax (pk, buf.getMagnitude (0, 0, 512));
        }
        const bool ok = pk > 0.01f && pk < 1.0f;
        if (! ok) std::printf ("    %s: peak %.4f\n", ps.factory()[(size_t) i].name.toRawUTF8(), pk);
        allSound = allSound && ok;
    }
    check (allSound, "every factory preset sounds, under 0 dBFS");
    std::printf (fails == 0 ? "ALL PASS\n" : "%d FAILED\n", fails);
    return fails == 0 ? 0 : 1;
}

int main (int argc, char** argv)
{
    if (argc >= 2 && juce::String (argv[1]) == "presets")
        return presetCheck();

    if (argc < 3)
    {
        std::printf ("usage: FREEKMOD_Tool snapshot <out.png> [chord|quiet|menu|load|save|size|<preset>][2x]\n"
                     "       FREEKMOD_Tool demo <out.wav>\n"
                     "       FREEKMOD_Tool presets\n");
        return 2;
    }
    const juce::String mode (argv[1]);
    const juce::File out (juce::File::getCurrentWorkingDirectory().getChildFile (juce::String (argv[2])));
    if (mode == "snapshot")
        return snapshot (out, argc > 3 ? juce::String (argv[3]) : juce::String ("chord"));
    if (mode == "demo")
        return demo (out);
    return 2;
}
