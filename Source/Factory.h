#pragma once

// FREEKMOD's factory presets and where RANDOMISE may wander. Values are real units; choices by index:
//   ALG     algorithm number minus one (ALG 5 is 4)
//   RATIO   0.5 0, 0.71 1, 1 2, 1.41 3, 1.5 4, 2 5, 2.5 6, 3 7, 3.5 8, 4 9, 5 10, 6 11, 7 12, 8 13,
//           9 14, 10 15, 11 16, 12 17, 13 18, 14 19, 15 20, 16 21
//   WAVE    SINE 0, PEAK 1, HALF 2, CAMEL 3, SINE 2X 4, PEAK 2X 5, CAMEL 2X 6, QUARTER 7
// All original patches written for FREEKMOD, none copied from Yamaha's.

#include "manners/MannersPresets.h"
#include "PluginProcessor.h"

namespace fm
{
    inline manners::Presets::Setup presetSetup()
    {
        namespace P = ParamIDs;
        manners::Presets::Setup s;
        s.plugin = "FREEKMOD";

        s.factory = {
            // the defaults: two pairs, a body and a tine that dies fast. Safe as houses.
            { "E PIANO 02", {} },

            // a late night bass: half-ratio carrier, a peaky wave, a little feedback for the bark
            { "LATE NIGHT BASS", { { P::alg, 3.0f }, { P::ratio[0], 0.0f }, { P::ratio[1], 0.0f }, { P::ratio[2], 2.0f }, { P::ratio[3], 2.0f },
                                   { P::level[0], 1.0f }, { P::level[1], 0.7f }, { P::level[2], 0.45f }, { P::level[3], 0.5f },
                                   { P::wave[0], 1.0f }, { P::feedback, 0.3f }, { P::bright, 0.55f }, { P::bite, 0.25f },
                                   { P::tail, 0.2f }, { P::sustain, 0.6f }, { P::decay, 0.8f }, { P::release, 0.12f },
                                   { P::detune, 0.0f }, { P::touch, 0.7f }, { P::volume, 0.85f } } },

            // bells: a 3.5 and a 1.41 never quite agree with the note, which is the point
            { "GLASS HOUSE", { { P::ratio[1], 8.0f }, { P::ratio[3], 3.0f }, { P::level[1], 0.5f }, { P::level[3], 0.45f },
                               { P::bright, 0.5f }, { P::bite, 1.2f }, { P::tail, 0.05f }, { P::decay, 4.0f },
                               { P::release, 2.5f }, { P::detune, 0.4f } } },

            // brass: three in a row, the brightness held up, a slow-ish blat on the way in
            { "BRASS 87", { { P::alg, 1.0f }, { P::ratio[0], 2.0f }, { P::ratio[1], 2.0f }, { P::ratio[2], 2.0f }, { P::ratio[3], 2.0f },
                            { P::level[0], 1.0f }, { P::level[1], 0.6f }, { P::level[2], 0.4f }, { P::level[3], 0.35f },
                            { P::bright, 0.55f }, { P::attack, 0.06f }, { P::bite, 0.5f }, { P::tail, 0.55f },
                            { P::sustain, 0.8f }, { P::decay, 1.0f }, { P::release, 0.3f }, { P::feedback, 0.35f },
                            { P::touch, 0.7f }, { P::vibrato, 0.1f } } },

            // a mallet on wood: a high modulator that's gone almost before it started
            { "MARIMBA MALLET", { { P::ratio[1], 9.0f }, { P::ratio[3], 15.0f }, { P::level[1], 0.45f }, { P::level[3], 0.4f },
                                  { P::bright, 0.45f }, { P::bite, 0.12f }, { P::tail, 0.0f }, { P::decay, 0.6f },
                                  { P::release, 0.3f }, { P::detune, 0.1f }, { P::volume, 0.95f } } },

            // four carriers, an octave apart and more: drawbars
            { "ORGAN STOP", { { P::alg, 7.0f }, { P::ratio[0], 0.0f }, { P::ratio[1], 2.0f }, { P::ratio[2], 5.0f }, { P::ratio[3], 9.0f },
                              { P::level[0], 0.8f }, { P::level[1], 0.9f }, { P::level[2], 0.6f }, { P::level[3], 0.4f },
                              { P::attack, 0.005f }, { P::sustain, 1.0f }, { P::release, 0.1f }, { P::detune, 0.35f },
                              { P::vibrato, 0.25f }, { P::ampVel, 0.0f } } },

            // a clavinet-ish snap: a half wave at the bottom of the stack, bright and quick
            { "CLAVINETTISH", { { P::alg, 0.0f }, { P::ratio[0], 2.0f }, { P::ratio[1], 7.0f }, { P::ratio[2], 2.0f }, { P::ratio[3], 11.0f },
                                { P::level[0], 1.0f }, { P::level[1], 0.5f }, { P::level[2], 0.3f }, { P::level[3], 0.2f },
                                { P::wave[0], 2.0f }, { P::bright, 0.6f }, { P::bite, 0.2f }, { P::tail, 0.2f },
                                { P::decay, 0.5f }, { P::sustain, 0.3f }, { P::touch, 0.8f }, { P::release, 0.08f },
                                { P::detune, 0.0f }, { P::volume, 1.0f } } },

            // one modulator into three carriers, slow everything: a pad that thaws
            { "FROZEN PAD", { { P::alg, 5.0f }, { P::ratio[0], 2.0f }, { P::ratio[1], 5.0f }, { P::ratio[2], 0.0f }, { P::ratio[3], 3.0f },
                              { P::level[0], 0.9f }, { P::level[1], 0.6f }, { P::level[2], 0.7f }, { P::level[3], 0.4f },
                              { P::attack, 1.2f }, { P::decay, 3.0f }, { P::sustain, 0.8f }, { P::release, 3.0f },
                              { P::bright, 0.3f }, { P::bite, 3.0f }, { P::tail, 0.6f }, { P::detune, 0.6f },
                              { P::vibrato, 0.1f }, { P::touch, 0.3f } } },

            // a knock: everything over in a fifth of a second
            { "WOODBLOCK", { { P::alg, 0.0f }, { P::ratio[0], 2.0f }, { P::ratio[1], 3.0f }, { P::ratio[2], 12.0f }, { P::ratio[3], 2.0f },
                             { P::level[0], 1.0f }, { P::level[1], 0.7f }, { P::level[2], 0.4f }, { P::level[3], 0.3f },
                             { P::bright, 0.7f }, { P::bite, 0.03f }, { P::tail, 0.0f }, { P::decay, 0.15f },
                             { P::release, 0.1f }, { P::detune, 0.0f }, { P::volume, 1.0f } } },

            // feedback doing the work of a saw, held bright: a lead
            { "FEEDBACK LEAD", { { P::alg, 1.0f }, { P::ratio[0], 2.0f }, { P::ratio[1], 5.0f }, { P::ratio[2], 2.0f }, { P::ratio[3], 2.0f },
                                 { P::level[0], 1.0f }, { P::level[1], 0.45f }, { P::level[2], 0.3f }, { P::level[3], 0.5f },
                                 { P::feedback, 0.7f }, { P::bright, 0.5f }, { P::tail, 0.6f }, { P::sustain, 0.9f },
                                 { P::vibrato, 0.2f }, { P::attack, 0.01f }, { P::release, 0.25f }, { P::detune, 0.1f }, { P::volume, 0.85f } } },

            // the E PIANO with a spanner in it: a bit out of tune with itself, things where they shouldn't be
            { "SPANNER IN THE WORKS", { { P::spanner, 0.35f }, { P::seed, 4404.0f } } },

            // the mallet patch, dropped down the stairs
            { "DROPPED THE TRAY", { { P::alg, 5.0f }, { P::ratio[1], 9.0f }, { P::ratio[3], 15.0f }, { P::level[1], 0.45f },
                                    { P::level[3], 0.4f }, { P::bright, 0.45f }, { P::bite, 0.12f }, { P::tail, 0.0f },
                                    { P::decay, 0.9f }, { P::release, 0.4f }, { P::spanner, 0.55f }, { P::seed, 77.0f }, { P::volume, 0.9f } } },

            // all the way over
            { "FM HELLSCAPE", { { P::spanner, 0.95f }, { P::seed, 666.0f }, { P::feedback, 0.5f }, { P::grit, 0.3f },
                                { P::sustain, 0.5f }, { P::decay, 3.0f }, { P::release, 1.0f } } },

            // the E PIANO through a converter that was cheap in 1987 and hasn't improved
            { "RUBBISH DAY", { { P::grit, 0.7f }, { P::detune, 0.4f }, { P::vibrato, 0.08f } } },

            // LOOSE: every note goes wrong in its own way
            { "WHERE DID THAT COME FROM", { { P::spanner, 0.6f }, { P::seed, 1234.0f }, { P::loose, 1.0f }, { P::alg, 6.0f },
                                            { P::ratio[3], 7.0f }, { P::level[3], 0.5f }, { P::bright, 0.5f }, { P::bite, 0.4f },
                                            { P::decay, 1.5f }, { P::release, 0.8f }, { P::volume, 1.0f } } },
        };

        s.randomise = {
            { P::alg, 0.0f, 7.0f },
            { P::ratio[0], 0.0f, 5.0f }, { P::ratio[1], 0.0f, 21.0f }, { P::ratio[2], 0.0f, 9.0f }, { P::ratio[3], 0.0f, 21.0f },
            { P::level[0], 0.5f, 1.0f }, { P::level[1], 0.1f, 0.8f }, { P::level[2], 0.1f, 0.9f }, { P::level[3], 0.1f, 0.7f },
            { P::wave[0], 0.0f, 3.0f }, { P::wave[1], 0.0f, 7.0f }, { P::wave[2], 0.0f, 3.0f }, { P::wave[3], 0.0f, 7.0f },
            { P::bright, 0.15f, 0.75f }, { P::bite, 0.05f, 2.0f }, { P::tail, 0.0f, 0.6f }, { P::feedback, 0.0f, 0.6f },
            { P::touch, 0.2f, 0.9f }, { P::detune, 0.0f, 0.6f },
            { P::attack, 0.001f, 0.3f }, { P::decay, 0.2f, 5.0f }, { P::sustain, 0.0f, 0.9f }, { P::release, 0.05f, 2.0f },
        };
        return s;
    }
}
