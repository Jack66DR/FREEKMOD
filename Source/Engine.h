#pragma once

// FREEKMOD engine
// ---------------
// An eight voice, four-operator FM synth after the idea of Yamaha's 1987 TX81Z, made playable with
// a handful of big knobs, and with a SPANNER to throw in the works. Header only and free of JUCE,
// so the offline test can build it with nothing but a compiler.
//
// One voice: four operators, each a wave read at a ratio of the note's pitch, wired by one of eight
// algorithms. Modulators bend the phase of the operators below them; carriers are what you hear.
// OP 4 can feed back into itself.
//
// The knobs a programmer used to set per operator (an envelope with five rates, level scaling,
// rate scaling) are folded into four:
//   BRIGHT    how hard every modulator works: the amount of FM
//   BITE      how quickly that brightness dies away after the strike. Higher ratios die faster,
//             and so do higher notes, the way real struck things behave, so a tine sounds like a tine.
//   TAIL      how much brightness is left once it has
//   TOUCH     how much harder playing means brighter
//
// SPANNER, from nothing to everything:
//   a little    ratios slip off their whole numbers: beating, bells, a bit of wrongness
//   a bit more  stray modulation paths appear between operators that the algorithm never wired
//   halfway     the output loops back into OP 4, and some modulators swell instead of dying
//   most        ratios slide towards irrational ones, the waves swap, the feedback climbs
//   all         each note starts with a pitch lurch. The FM hellscape.
// What gets rewired is decided by a seed (THROW picks a new one). With LOOSE off the same key always
// fails the same way, so a good accident can be kept; with LOOSE on every note fails afresh.
//
// The voice runs at twice the host's sample rate and is filtered back down, so ordinary patches stay
// clean. The hellscape aliases anyway. That's allowed.

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <vector>

namespace fm
{
    constexpr double kPi = 3.14159265358979323846;
    constexpr double kTwoPi = 2.0 * kPi;
    constexpr int kVoices = 8, kOps = 4, kWaves = 8, kAlgs = 8;

    // Musical ratios to snap to: whole numbers, halves, and the two square roots that ring like bells.
    constexpr int kRatioCount = 22;
    inline double ratioValue (int i)
    {
        static const double r[kRatioCount] = { 0.5, 0.71, 1.0, 1.41, 1.5, 2.0, 2.5, 3.0, 3.5, 4.0, 5.0,
                                               6.0, 7.0, 8.0, 9.0, 10.0, 11.0, 12.0, 13.0, 14.0, 15.0, 16.0 };
        return r[std::clamp (i, 0, kRatioCount - 1)];
    }

    // Algorithms. mod[i] is the set of operators that modulate operator i (bit 0 is OP 1 ... bit 3 is
    // OP 4); carriers is the set you hear. Every algorithm only lets a higher operator modulate a
    // lower one, so working from OP 4 down, a modulator is always ready before it's needed.
    struct Alg { uint8_t mod[kOps]; uint8_t carriers; };
    inline const Alg& alg (int i)
    {
        static const Alg a[kAlgs] = {
            { { 1 << 1, 1 << 2, 1 << 3, 0 }, 0b0001 },                  // 1: 4 > 3 > 2 > 1
            { { 1 << 1, (1 << 2) | (1 << 3), 0, 0 }, 0b0001 },          // 2: 3 and 4 > 2 > 1
            { { (1 << 1) | (1 << 3), 1 << 2, 0, 0 }, 0b0001 },          // 3: 3 > 2 > 1, and 4 > 1
            { { (1 << 1) | (1 << 2), 0, 1 << 3, 0 }, 0b0001 },          // 4: 4 > 3 > 1, and 2 > 1
            { { 1 << 1, 0, 1 << 3, 0 }, 0b0101 },                       // 5: 2 > 1, 4 > 3: two pairs
            { { 1 << 3, 1 << 3, 1 << 3, 0 }, 0b0111 },                  // 6: 4 > 1, 2 and 3
            { { 0, 0, 1 << 3, 0 }, 0b0111 },                            // 7: 4 > 3, with 1 and 2 on their own
            { { 0, 0, 0, 0 }, 0b1111 },                                 // 8: four carriers, organ style
        };
        return a[std::clamp (i, 0, kAlgs - 1)];
    }
    inline bool isCarrier (int algIndex, int op) { return (alg (algIndex).carriers >> op) & 1; }

    // Eight waves in the TX81Z's spirit: a sine, and sines squared, halved, folded and doubled.
    inline const char* waveName (int w)
    {
        static const char* n[kWaves] = { "SINE", "PEAK", "HALF", "CAMEL", "SINE 2X", "PEAK 2X", "CAMEL 2X", "QUARTER" };
        return n[std::clamp (w, 0, kWaves - 1)];
    }

    inline double waveShape (int w, double p)   // p 0..1
    {
        const double s1 = std::sin (kTwoPi * p);
        const double s2 = std::sin (2.0 * kTwoPi * p);
        switch (w)
        {
            case 0: return s1;
            case 1: return s1 * std::abs (s1);
            case 2: return p < 0.5 ? s1 : 0.0;
            case 3: return std::abs (s1);
            case 4: return p < 0.5 ? s2 : 0.0;
            case 5: return p < 0.5 ? s2 * std::abs (s2) : 0.0;
            case 6: return p < 0.5 ? std::abs (s2) : 0.0;
            case 7: return std::fmod (p, 0.5) < 0.25 ? std::abs (s1) : 0.0;
            default: return s1;
        }
    }

    struct Tables
    {
        static constexpr int kSize = 4096;
        std::array<std::array<float, kSize + 1>, kWaves> t {};

        Tables()
        {
            for (int w = 0; w < kWaves; ++w)
                for (int i = 0; i <= kSize; ++i)
                    t[(size_t) w][(size_t) i] = (float) waveShape (w, (double) (i % kSize) / kSize);
        }

        float read (int w, double p) const   // p 0..1
        {
            const double x = p * kSize;
            const int i = (int) x;
            const float f = (float) (x - i);
            const auto& tb = t[(size_t) w];
            return tb[(size_t) i] + (tb[(size_t) i + 1] - tb[(size_t) i]) * f;
        }

        static const Tables& get() { static const Tables tables; return tables; }
    };

    struct Params
    {
        int alg = 4;                                    // index: algorithm 5
        std::array<int, kOps> ratio { 2, 2, 2, 19 };    // indices into ratioValue: 1, 1, 1, 14
        std::array<float, kOps> level { 0.9f, 0.55f, 0.8f, 0.35f };
        std::array<int, kOps> wave { 0, 0, 0, 0 };

        float bright = 0.45f, bite = 0.6f, tail = 0.15f, feedback = 0.0f, touch = 0.6f;
        float detune = 0.2f, vibrato = 0.0f, grit = 0.0f;
        float spanner = 0.0f;
        int seed = 1987;
        bool loose = false;

        float attack = 0.002f, decay = 2.5f, sustain = 0.0f, release = 0.4f;   // seconds, sustain 0..1
        float ampVel = 0.5f;
        float volume = 0.75f;
    };

    //==========================================================================
    inline float fastTanh (float x)
    {
        x = std::clamp (x, -4.5f, 4.5f);
        const float x2 = x * x;
        return x * (27.0f + x2) / (27.0f + 9.0f * x2);
    }

    inline float smooth (float a, float b, float x)
    {
        const float t = std::clamp ((x - a) / (b - a), 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    }

    struct Rng
    {
        uint32_t s;
        explicit Rng (uint32_t seed) : s (seed ? seed : 0x9E3779B9u) {}
        float next()   // -1..1
        {
            s ^= s << 13; s ^= s >> 17; s ^= s << 5;
            return (float) ((double) s / 2147483648.0 - 1.0);
        }
        float unit() { return 0.5f * (next() + 1.0f); }   // 0..1
    };

    // What the SPANNER does to one note: fixed when the note starts, scaled by the knob as it plays.
    struct Chaos
    {
        std::array<float, kOps> ratioJitter {};                  // -1..1
        std::array<float, kOps> weird {};                        // an irrational ratio each operator slides towards
        std::array<int, kOps> wave {};                           // the wave it swaps to
        std::array<std::array<float, kOps>, kOps> extra {};     // extra[i][j]: OP j into OP i, where the algorithm never wired it
        std::array<float, kOps> invert {};                       // 1: this modulator swells instead of dying
        float lurch = 0.0f;                                      // -1..1, the pitch lurch at the start

        static Chaos make (uint32_t seed)
        {
            static const float odd[] = { 3.14159f, 2.23607f, 0.293f, 7.13f, 1.059f, 11.3f, 0.77f, 4.66f, 1.73f, 5.41f };
            Rng r (seed * 2654435761u + 12345u);
            for (int k = 0; k < 4; ++k) r.next();   // stir
            Chaos c;
            for (int i = 0; i < kOps; ++i)
            {
                c.ratioJitter[(size_t) i] = r.next();
                c.weird[(size_t) i] = odd[(int) (r.unit() * 9.999f)];
                c.wave[(size_t) i] = (int) (r.unit() * 7.999f);
                c.invert[(size_t) i] = r.unit() < 0.4f ? 1.0f : 0.0f;
                for (int j = 0; j < kOps; ++j)
                    c.extra[(size_t) i][(size_t) j] = (i != j && r.unit() < 0.45f) ? r.next() : 0.0f;
            }
            c.lurch = r.next();
            return c;
        }
    };

    inline uint32_t chaosSeed (int seed, bool loose, int note, uint32_t count)
    {
        return (uint32_t) seed * 7919u + (loose ? count * 104729u + 7u : (uint32_t) note * 97u);
    }

    //==========================================================================
    // Envelopes shaped like analogue ones: the attack aims past the top, the rest fall exponentially.
    struct Env
    {
        enum Stage { idle, attack, decay, release } stage = idle;
        float level = 0.0f;

        void on() { stage = attack; }
        void off() { if (stage != idle) stage = release; }

        static float coef (float seconds, float sr, float shape)
        {
            return 1.0f - std::exp (-1.0f / std::max (1.0f, seconds * sr / shape));
        }

        float next (float a, float d, float s, float r)
        {
            switch (stage)
            {
                case attack:
                    level += (1.25f - level) * a;
                    if (level >= 1.0f) { level = 1.0f; stage = decay; }
                    break;
                case decay:   level += (s - level) * d; break;
                case release:
                    level += (0.0f - level) * r;
                    if (level < 1.0e-5f) { level = 0.0f; stage = idle; }
                    break;
                default: break;
            }
            return level;
        }
    };

    // Halving the sample rate: a windowed-sinc low-pass at the host's Nyquist, then every other sample.
    class Decimator
    {
    public:
        Decimator()
        {
            const int c = kTaps / 2;
            double sum = 0.0;
            for (int i = 0; i < kTaps; ++i)
            {
                const double n = i - c;
                const double sinc = n == 0 ? 0.5 : std::sin (kPi * 0.5 * n) / (kPi * n);
                const double w = 0.42 - 0.5 * std::cos (kTwoPi * i / (kTaps - 1)) + 0.08 * std::cos (2.0 * kTwoPi * i / (kTaps - 1));
                h[(size_t) i] = sinc * w;
                sum += h[(size_t) i];
            }
            for (auto& v : h) v /= sum;
        }

        float process (float a, float b)   // two samples at the high rate in, one out
        {
            push (a);
            push (b);
            double y = 0.0;
            for (int i = 0; i < kTaps; ++i)
                y += h[(size_t) i] * buf[(size_t) ((pos - 1 - i + 2 * kTaps) % kTaps)];
            return (float) y;
        }

        void reset() { buf.fill (0.0f); }

    private:
        void push (float x) { buf[(size_t) pos] = x; pos = (pos + 1) % kTaps; }
        static constexpr int kTaps = 63;
        std::array<double, kTaps> h {};
        std::array<float, kTaps> buf {};
        int pos = 0;
    };

    //==========================================================================
    struct Voice
    {
        int note = -1;
        float velocity = 1.0f;
        bool held = false, sustained = false;
        uint64_t age = 0;

        std::array<double, kOps> phase {};
        std::array<float, kOps> out {};
        float prev4 = 0.0f, lastCarrier = 0.0f;
        Env amp;
        std::array<Env, kOps> mod;
        Chaos chaos;
        float lurchEnv = 0.0f;
        float jitter = 0.0f;   // a few cents of its own, so a chord doesn't beat in lockstep

        bool active() const { return amp.stage != Env::idle; }
    };

    //==========================================================================
    class Synth
    {
    public:
        Params p;

        // for the face
        std::array<std::atomic<float>, kVoices> meterLevel {};
        std::array<std::atomic<int>, kVoices> meterNote {};

        Synth()
        {
            for (int i = 0; i < kVoices; ++i)
                meterNote[(size_t) i].store (-1);
        }

        void prepare (double sampleRate)
        {
            sr = sampleRate;
            sr2 = 2.0 * sampleRate;
            decL.reset();
            decR.reset();
            allOff (true);
        }

        //------------------------------------------------------------------ MIDI
        void noteOn (int note, float velocity)
        {
            ++clock;
            ++noteCount;
            Voice* v = nullptr;
            for (auto& vc : voices)
                if (vc.note == note && vc.active()) { v = &vc; break; }
            if (v == nullptr)
                for (auto& vc : voices)
                    if (! vc.active() && (v == nullptr || vc.age < v->age)) v = &vc;
            if (v == nullptr)
                for (auto& vc : voices)
                    if (! vc.held && ! vc.sustained && (v == nullptr || vc.amp.level < v->amp.level)) v = &vc;
            if (v == nullptr)
                for (auto& vc : voices)
                    if (v == nullptr || vc.age < v->age) v = &vc;

            const bool fresh = ! v->active();
            v->note = note;
            v->velocity = std::clamp (velocity, 0.0f, 1.0f);
            v->held = true;
            v->sustained = false;
            v->age = clock;
            v->chaos = Chaos::make (chaosSeed (p.seed, p.loose, note, noteCount));
            v->lurchEnv = 1.0f;
            v->jitter = Rng ((uint32_t) note * 2246822519u + 3u).next();   // each key its own few cents, the same every time
            if (fresh)
            {
                // FM wants its operators to start together, or every note's attack is different
                v->phase.fill (0.0);
                v->out.fill (0.0f);
                v->prev4 = v->lastCarrier = 0.0f;
            }
            v->amp.on();
            for (auto& m : v->mod) m.on();
        }

        void noteOff (int note)
        {
            ++clock;
            for (auto& v : voices)
                if (v.note == note && v.held)
                {
                    v.held = false;
                    if (pedal) v.sustained = true;
                    else release (v);
                }
        }

        void setSustain (bool down)
        {
            pedal = down;
            if (! down)
                for (auto& v : voices)
                    if (v.sustained) { v.sustained = false; release (v); }
        }

        void setBend (float semitones) { bend = semitones; }
        void setWheel (float amount) { wheel = std::clamp (amount, 0.0f, 1.0f); }

        void allOff (bool hard)
        {
            pedal = false;
            if (hard)
            {
                decL.reset(); decR.reset();
                dcL = {}; dcR = {};
                holdCount = heldL = heldR = 0.0f;
            }
            for (auto& v : voices)
            {
                v.held = v.sustained = false;
                release (v);
                if (hard)
                {
                    v.amp = {};
                    for (auto& m : v.mod) m = {};
                    v.note = -1;
                }
            }
        }

        int activeVoices() const
        {
            int n = 0;
            for (auto& v : voices) n += v.active() ? 1 : 0;
            return n;
        }

        //------------------------------------------------------------------ audio
        void render (float* L, float* R, int n)
        {
            const float f2 = (float) sr2;
            const float aC = Env::coef (p.attack, f2, 1.609f), dC = Env::coef (p.decay, f2, 4.0f), rC = Env::coef (p.release, f2, 4.0f);
            const float sus = std::clamp (p.sustain, 0.0f, 1.0f);
            const auto& A = alg (p.alg);
            // a wrecked patch spreads its energy into places a carrier's level doesn't reach: make it up, so
            // the SPANNER changes what you hear, not how loud it is
            const float s0 = std::clamp (p.spanner, 0.0f, 1.0f);
            const float vol = p.volume * p.volume * (1.0f + 0.3f * smooth (0.0f, 0.4f, s0) + 0.75f * smooth (0.6f, 0.9f, s0));

            // the SPANNER, unpacked
            const float s = std::clamp (p.spanner, 0.0f, 1.0f);
            const float jit = 0.35f * std::pow (s, 1.5f);
            const float weirdMix = smooth (0.55f, 0.92f, s);
            const float extraAmt = smooth (0.22f, 0.75f, s) * 3.0f;
            const float loopAmt = smooth (0.45f, 0.85f, s) * 2.5f;
            const float invAmt = smooth (0.5f, 0.8f, s);
            const float boost = 1.0f + 5.0f * s * s * s;
            const bool swapWaves = s > 0.72f;
            const float lurchSemis = smooth (0.65f, 1.0f, s) * 24.0f;
            const float lurchDecay = std::exp (-1.0f / (0.045f * f2));

            const float fb = (float) (std::pow (p.feedback, 1.5f) * 3.5 + s * s * 3.0);
            const float twoPi = (float) kTwoPi;

            // carrier mix: fewer carriers each get more, and with DETUNE they spread across the stereo field
            int nCar = 0;
            for (int i = 0; i < kOps; ++i) nCar += (A.carriers >> i) & 1;
            const float carGain = 1.0f / (0.55f + 0.45f * (float) nCar);
            std::array<float, kOps> gL {}, gR {};
            {
                int k = 0;
                for (int i = 0; i < kOps; ++i)
                {
                    if (! ((A.carriers >> i) & 1)) continue;
                    const float pos = nCar > 1 ? -1.0f + 2.0f * (float) k / (float) (nCar - 1) : 0.0f;
                    const float pan = 0.8f * p.detune * pos * (k % 2 == 0 ? 1.0f : -1.0f);
                    const float a = (pan + 1.0f) * 0.25f * (float) kPi;
                    gL[(size_t) i] = std::cos (a) * 1.41421f * p.level[(size_t) i] * carGain;
                    gR[(size_t) i] = std::sin (a) * 1.41421f * p.level[(size_t) i] * carGain;
                    ++k;
                }
            }
            static const float detuneCents[kOps] = { 0.0f, 5.0f, -7.0f, 10.0f };
            std::array<double, kOps> baseRatio {};
            for (int i = 0; i < kOps; ++i)
                baseRatio[(size_t) i] = ratioValue (p.ratio[(size_t) i]) * std::exp2 (p.detune * detuneCents[i] / 1200.0);

            // per voice, once a block: how quickly each modulator's brightness dies, and how bright high notes are
            for (int vi = 0; vi < kVoices; ++vi)
            {
                auto& v = voices[(size_t) vi];
                if (! v.active()) continue;
                const float noteFast = (float) std::exp2 (-(v.note - 60) / 24.0);
                for (int op = 0; op < kOps; ++op)
                {
                    const double ratio = ratioValue (p.ratio[(size_t) op]);
                    biteC[(size_t) vi][(size_t) op] = Env::coef (p.bite / (float) std::sqrt (std::max (1.0, ratio)) * noteFast, f2, 4.0f);
                }
                keyScale[(size_t) vi] = std::clamp ((float) std::exp2 (-(v.note - 60) / 36.0), 0.35f, 1.6f);
            }

            const auto& tables = Tables::get();
            const double vibStep = 5.5 / sr2;
            const float holdEvery = 1.0f + std::floor (p.grit * 3.0f);
            const float steps = std::pow (2.0f, 15.0f - 9.0f * p.grit);

            for (int i = 0; i < n; ++i)
            {
                float hiL[2] = { 0.0f, 0.0f }, hiR[2] = { 0.0f, 0.0f };
                for (int sub = 0; sub < 2; ++sub)
                {
                    vibPhase += vibStep;
                    if (vibPhase >= 1.0) vibPhase -= 1.0;
                    const double vib = std::max (p.vibrato, wheel) * 0.45 * std::sin (kTwoPi * vibPhase);

                    for (int vi = 0; vi < kVoices; ++vi)
                    {
                        auto& v = voices[(size_t) vi];
                        if (! v.active())
                            continue;
                        const float a = v.amp.next (aC, dC, sus, rC);
                        const auto& c = v.chaos;

                        // brightness for this note: the knob, how hard it was played, gentler up the keyboard
                        const float brightV = p.bright * (1.0f - p.touch * (1.0f - v.velocity)) * boost * keyScale[(size_t) vi];

                        v.lurchEnv *= lurchDecay;
                        const double semis = (double) v.note + bend + vib + v.jitter * p.detune * 0.03
                                           + lurchSemis * c.lurch * v.lurchEnv;
                        const double hz = 440.0 * std::exp2 ((semis - 69.0) / 12.0);

                        std::array<float, kOps> cur {};
                        float carrier = 0.0f, outL = 0.0f, outR = 0.0f;
                        for (int op = kOps - 1; op >= 0; --op)
                        {
                            const size_t o = (size_t) op;
                            const bool car = (A.carriers >> op) & 1;

                            // its envelope: carriers follow the amplifier, modulators the brightness
                            float env;
                            if (car)
                                env = a;
                            else
                            {
                                env = v.mod[o].next (aC, biteC[(size_t) vi][o], p.tail, rC);
                                if (invAmt > 0.0f && c.invert[o] > 0.0f)
                                    env += (a * (1.0f - env) - env) * invAmt;   // swells as the note dies
                            }

                            // phase modulation from everything feeding it
                            float pm = 0.0f;
                            for (int j = op + 1; j < kOps; ++j)
                                if ((A.mod[o] >> j) & 1)
                                    pm += p.level[(size_t) j] * brightV * twoPi * cur[(size_t) j];
                            if (extraAmt > 0.0f)
                                for (int j = 0; j < kOps; ++j)
                                    if (c.extra[o][(size_t) j] != 0.0f)
                                        pm += extraAmt * c.extra[o][(size_t) j] * (j > op ? cur[(size_t) j] : v.out[(size_t) j]);
                            if (op == kOps - 1)
                                pm += fb * 0.5f * (v.out[o] + v.prev4) + loopAmt * v.lastCarrier;

                            double ratio = baseRatio[o] * (1.0 + jit * c.ratioJitter[o]);
                            if (weirdMix > 0.0f) ratio += (c.weird[o] - ratio) * weirdMix;
                            v.phase[o] += std::min (0.45, hz * ratio / sr2);
                            v.phase[o] -= std::floor (v.phase[o]);
                            double ph = v.phase[o] + pm / kTwoPi;
                            ph -= std::floor (ph);

                            const int w = swapWaves ? c.wave[o] : p.wave[o];
                            cur[o] = tables.read (w, ph) * env;
                            if (car)
                            {
                                carrier += cur[o];
                                outL += cur[o] * gL[o];
                                outR += cur[o] * gR[o];
                            }
                        }
                        v.prev4 = v.out[(size_t) (kOps - 1)];
                        v.out = cur;
                        v.lastCarrier = carrier;

                        const float velAmp = 1.0f - p.ampVel * (1.0f - v.velocity);
                        hiL[sub] += outL * velAmp;
                        hiR[sub] += outR * velAmp;
                    }
                }

                float l = decL.process (hiL[0], hiL[1]) * 0.25f;
                float r = decR.process (hiR[0], hiR[1]) * 0.25f;

                // GRIT: fewer bits and a slower clock, like a converter from 1987 having a bad day
                if (p.grit > 0.001f)
                {
                    if (++holdCount >= holdEvery) { holdCount = 0.0f; heldL = l; heldR = r; }
                    l = std::round (heldL * steps) / steps;
                    r = std::round (heldR * steps) / steps;
                }

                l = dcL.process (l, sr) * vol;
                r = dcR.process (r, sr) * vol;
                L[i] = ceiling (l);
                R[i] = ceiling (r);
            }

            for (int vi = 0; vi < kVoices; ++vi)
            {
                auto& v = voices[(size_t) vi];
                meterLevel[(size_t) vi].store (v.active() ? v.amp.level : 0.0f);
                meterNote[(size_t) vi].store (v.active() ? v.note : -1);
            }
        }

        Voice& voice (int i) { return voices[(size_t) i]; }

    private:
        struct DcBlock
        {
            float x1 = 0.0f, y1 = 0.0f;
            float process (float x, double rate)
            {
                const float r = 1.0f - (float) (kTwoPi * 8.0 / rate);
                const float y = x - x1 + r * y1;
                x1 = x;
                y1 = y;
                return y;
            }
        };

        static float ceiling (float o)   // a soft ceiling, so nothing leaves above 0 dBFS
        {
            const float a = std::abs (o);
            return a > 0.8f ? std::copysign (0.8f + 0.19f * std::tanh ((a - 0.8f) / 0.19f), o) : o;
        }

        void release (Voice& v)
        {
            v.amp.off();
            for (auto& m : v.mod) m.off();
            v.age = clock;
        }

        std::array<Voice, kVoices> voices;
        std::array<std::array<float, kOps>, kVoices> biteC {};
        std::array<float, kVoices> keyScale {};
        Decimator decL, decR;
        DcBlock dcL, dcR;
        double sr = 48000.0, sr2 = 96000.0, vibPhase = 0.0;
        float bend = 0.0f, wheel = 0.0f;
        float holdCount = 0.0f, heldL = 0.0f, heldR = 0.0f;
        bool pedal = false;
        uint64_t clock = 0;
        uint32_t noteCount = 0;
    };
}
