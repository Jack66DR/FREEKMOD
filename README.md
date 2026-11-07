# FREEKMOD

**manners! DSP** four-operator FM synth after the idea of Yamaha's 1987 TX81Z, made playable with a few big knobs, and with a SPANNER to throw in the works. AU and VST3, macOS universal (Apple Silicon and Intel).

The TX81Z was four operators, eight algorithms and eight waveforms in a 1U rack, programmed one parameter at a time through a tiny display. FREEKMOD keeps the four operators, the eight algorithms and eight waves, and replaces the menu diving with knobs that do what they say. Then it adds the thing FM was always secretly good at: noises nobody asked for.

FREEKMOD is an independent instrument for personal use. It is not affiliated with or endorsed by Yamaha, and its presets are original. Its algorithms and waves follow the TX81Z's idea, not its exact data.

## Build

GitHub Actions builds it on every push to `main` (Actions tab, "Build FREEKMOD"). It also runs the offline engine test and Apple's `auval` check.
Download the `FREEKMOD-macOS` artifact from the finished run. Inside are `FREEKMOD-AU.zip` and `FREEKMOD-VST3.zip`.

## Install

Unzip `FREEKMOD-AU.zip`, then in Terminal:

```sh
sudo cp -R ~/Downloads/FREEKMOD.component /Library/Audio/Plug-Ins/Components/
sudo xattr -dr com.apple.quarantine /Library/Audio/Plug-Ins/Components/FREEKMOD.component
```

Then in Logic: AU Instruments > manners! DSP > FREEKMOD. If it doesn't appear, open Plug-in Manager, find manners! DSP > FREEKMOD, and Reset & Rescan Selection.
The build is ad hoc signed, not notarised, which is why the `xattr` line is needed.

## FM in one paragraph

An operator is a wave played at some ratio of the note's pitch. A **carrier** is an operator you hear. A **modulator** is one you don't: it wobbles the phase of the operator below it so fast that the wobble becomes new harmonics. Whole-number ratios give harmonics that belong to the note (pianos, organs, basses); odd ratios give ones that don't (bells, metal, trouble). The **algorithm** is the wiring: which operators modulate which. The LCD draws it, carriers solid along the bottom.

## Controls

| Section | Control | What it does |
|---|---|---|
| **ALGORITHM** | the LCD | The wiring, drawn. Click it for the next algorithm (shift-click for the previous). |
| | ALG | The same, as a knob: 1 to 8, from a stack of four to four carriers side by side. |
| **OP 1 to OP 4** | RATIO | The operator's pitch against the note: 0.5, 0.71, 1, 1.41, 1.5, 2, 2.5, 3, 3.5, then whole numbers to 16. It snaps, so you can't land on mud by accident. |
| | LEVEL | A carrier's volume, or a modulator's depth. |
| | WAVE | SINE, PEAK, HALF, CAMEL, SINE 2X, PEAK 2X, CAMEL 2X, QUARTER. The picture beside it draws one cycle, and says whether this algorithm makes the operator a CARRIER or a MODULATOR. |
| **TIMBRE** | BRIGHT | How hard every modulator works: the amount of FM. Gentler up the keyboard, as it should be. |
| | BITE | How quickly the brightness dies after the strike. Higher ratios die faster than lower ones and higher notes faster than low, so a ratio-14 modulator makes a tine without being told to. |
| | TAIL | How much brightness is left once it has. |
| | FEEDBACK | OP 4 modulating itself: from a sine to a saw-like edge to noise. |
| | TOUCH | How much harder playing means brighter. This is what makes an FM piano feel like a piano. |
| **CHARACTER** | DETUNE | Pulls the operators a few cents apart and spreads the carriers across the stereo field. |
| | GRIT | Fewer bits and a slower clock, like a converter from 1987 having a bad day. |
| **SPANNER** | SPANNER | See below. |
| | THROW | A new seed: the SPANNER wrecks things differently. Undoable. |
| | LOOSE | Off: the same key always fails the same way, so a good accident can be kept (it's saved with the patch). On: every note fails afresh. |
| | the little LCD | How wrecked it is (BEHAVING, ITCHY, SPANNERED, UNHINGED, HELLSCAPE) and the seed. Hold it to hear a chord. |
| **AMPLIFIER** | ATTACK, DECAY, SUSTAIN, RELEASE | The carriers' envelope. |
| | VEL | Soft notes quieter. |
| **OUTPUT** | VIBRATO | Vibrato. The mod wheel adds it too. |
| | VOLUME | Output level. |

## The SPANNER

One knob from a safe E PIANO 02 to an FM hellscape, in stages:

- **A little:** the ratios slip off their whole numbers. Beating, bells, a bit of wrongness.
- **A bit more:** stray modulation paths appear between operators the algorithm never wired. They show up on the LCD, dotted.
- **Halfway:** the output loops back into OP 4, and some modulators swell as the note dies instead of fading.
- **Most of the way:** the ratios slide towards irrational ones, the operators swap waves, the feedback climbs.
- **All of it:** every note starts with a pitch lurch.

What gets rewired is decided by the seed, so turning the knob is repeatable; THROW changes the seed, LOOSE changes it on every note. Its level is made up as it goes, so it changes what you hear, not how loud it is.

## Notes

- Eight voices, as the TX81Z had. A ninth note takes the quietest released voice, then the oldest.
- The voice runs at twice the host's sample rate and is filtered back down, so ordinary patches stay clean. The hellscape aliases anyway. That's allowed.
- A note that starts from silence starts its operators in phase, so attacks are consistent, the way FM players expect.
- **MIDI**: notes, pitch bend (2 semitones), mod wheel (vibrato), sustain pedal, all notes off.
- Factory presets, all original: E PIANO 02, LATE NIGHT BASS, GLASS HOUSE, BRASS 87, MARIMBA MALLET, ORGAN STOP, CLAVINETTISH, FROZEN PAD, WOODBLOCK, FEEDBACK LEAD, SPANNER IN THE WORKS, DROPPED THE TRAY, FM HELLSCAPE, RUBBISH DAY, WHERE DID THAT COME FROM.
- The face is the manners! card inverted: black print on brushed silver (`Resources/plate.jpg`, generated from the house card's wear so the grime matches the family), with green LCDs.
- Font: Barlow Semi Condensed, SIL Open Font License (see `Resources/BarlowSemiCondensed-OFL.txt`).
- The shared face code lives in `Source/manners/`, copied from JX-PP unchanged.

## Local test build (optional)

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release -DFREEKMOD_BUILD_TESTS=ON
cmake --build build --target FREEKMOD_Test FREEKMOD_Tool
./build/FREEKMOD_Test_artefacts/Release/FREEKMOD_Test
./build/FREEKMOD_Tool_artefacts/Release/FREEKMOD_Tool presets
./build/FREEKMOD_Tool_artefacts/Release/FREEKMOD_Tool demo tour.wav
./build/FREEKMOD_Tool_artefacts/Release/FREEKMOD_Tool snapshot face.png fm_hellscape
```
