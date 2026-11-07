// Offline checks for the FREEKMOD engine. Build with -DFREEKMOD_BUILD_TESTS=ON (or just g++ -O2 -std=c++17 -I Source).

#include "Engine.h"

#include <chrono>
#include <complex>
#include <cstdio>
#include <vector>

static int failures = 0;

static void check (bool ok, const char* what)
{
    std::printf ("  [%s] %s\n", ok ? "PASS" : "FAIL", what);
    if (! ok) ++failures;
}

static constexpr double kSr = 48000.0;

struct Out { std::vector<float> L, R; };

static Out render (fm::Synth& s, double secs)
{
    Out o;
    const int n = (int) (secs * kSr);
    o.L.resize ((size_t) n);
    o.R.resize ((size_t) n);
    for (int i = 0; i < n; i += 256)
        s.render (o.L.data() + i, o.R.data() + i, std::min (256, n - i));
    return o;
}

static float peak (const std::vector<float>& x)
{
    float p = 0.0f;
    for (float v : x) p = std::max (p, std::abs (v));
    return p;
}

static bool finite (const std::vector<float>& x)
{
    for (float v : x) if (! std::isfinite (v)) return false;
    return true;
}

static double pitchOf (const std::vector<float>& x)
{
    double first = -1.0, last = -1.0;
    int count = 0;
    for (size_t i = x.size() / 2; i + 1 < x.size(); ++i)
        if (x[i] < 0.0f && x[i + 1] >= 0.0f)
        {
            const double t = (double) i + (double) (-x[i]) / (double) (x[i + 1] - x[i]);
            if (first < 0.0) first = t; else ++count;
            last = t;
        }
    return count > 0 ? kSr * count / (last - first) : 0.0;
}

static double dft (const std::vector<float>& x, size_t from, size_t N, double f)
{
    std::complex<double> acc;
    for (size_t i = 0; i < N; ++i)
    {
        const double w = 0.5 - 0.5 * std::cos (2.0 * fm::kPi * (double) i / (double) N);
        acc += w * (double) x[from + i] * std::polar (1.0, -2.0 * fm::kPi * f * (double) i / kSr);
    }
    return std::abs (acc);
}

// energy away from the harmonics of f, as a share of all of it
static double inharmonicShare (const std::vector<float>& x, size_t from, double f)
{
    const size_t N = 8192;
    double total = 0.0, off = 0.0;
    for (double bin = 50.0; bin < 20000.0; bin += 25.0)
    {
        const double m = dft (x, from, N, bin);
        total += m * m;
        const double h = bin / f;
        if (std::abs (h - std::round (h)) * f > 60.0) off += m * m;
    }
    return off / std::max (1.0e-12, total);
}

// energy in harmonics 2..12 against the fundamental: how bright it is
static double brightness (const std::vector<float>& x, size_t from, double f)
{
    const size_t N = 4096;
    const double f1 = dft (x, from, N, f);
    double h = 0.0;
    for (int k = 2; k <= 12; ++k) { const double m = dft (x, from, N, f * k); h += m * m; }
    return std::sqrt (h) / std::max (1.0e-12, f1);
}

static fm::Params sineOnly()
{
    fm::Params p;
    p.alg = 7;   // all carriers
    p.level = { 1.0f, 0.0f, 0.0f, 0.0f };
    p.ratio = { 2, 2, 2, 2 };
    p.detune = 0.0f;
    p.attack = 0.001f; p.sustain = 1.0f; p.decay = 1.0f;
    return p;
}

int main()
{
    std::printf ("FREEKMOD offline test\n");

    {
        fm::Synth s; s.p = sineOnly(); s.prepare (kSr);
        s.noteOn (69, 1.0f);
        check (std::abs (pitchOf (render (s, 0.4).L) - 440.0) < 0.5, "a carrier at ratio 1 plays A4 at 440 Hz");
        s.p.ratio[0] = 5;   // 2.0
        check (std::abs (pitchOf (render (s, 0.4).L) - 880.0) < 1.0, "ratio 2 plays an octave up");
        s.p.ratio[0] = 0;   // 0.5
        check (std::abs (pitchOf (render (s, 0.4).L) - 220.0) < 0.5, "ratio 0.5 plays an octave down");
    }
    {
        fm::Synth s; s.p = sineOnly(); s.p.alg = 0; s.p.level = { 1.0f, 0.7f, 0.0f, 0.0f }; s.p.bright = 0.0f;
        s.p.tail = 1.0f; s.prepare (kSr);
        s.noteOn (57, 1.0f);
        const auto dull = render (s, 0.5).L;
        s.p.bright = 0.6f;
        s.allOff (true); s.noteOn (57, 1.0f);
        const auto rich = render (s, 0.5).L;
        const double b0 = brightness (dull, 15000, 220.0), b1 = brightness (rich, 15000, 220.0);
        std::printf ("    brightness: BRIGHT 0 %.4f, BRIGHT 0.6 %.3f\n", b0, b1);
        check (b0 < 0.01 && b1 > 0.3, "BRIGHT turns a sine into harmonics");
        check (inharmonicShare (rich, 15000, 220.0) < 0.01, "and with whole-number ratios they're harmonic");
    }
    {
        fm::Synth s; s.p = sineOnly(); s.p.alg = 0; s.p.level = { 1.0f, 0.8f, 0.0f, 0.0f }; s.p.bright = 0.6f;
        s.p.bite = 0.15f; s.p.tail = 0.0f; s.prepare (kSr);
        s.noteOn (57, 1.0f);
        const auto x = render (s, 1.2).L;
        const double early = brightness (x, 2000, 220.0), late = brightness (x, 48000, 220.0);
        std::printf ("    BITE: brightness %.3f at the strike, %.4f a second later\n", early, late);
        check (early > 5.0 * late, "BITE lets the brightness die away");
    }
    {
        // the default patch is an electric piano; it should be clean, and harmonic
        fm::Synth s; s.prepare (kSr);
        for (int n : { 48, 55, 60, 64, 67, 71 }) s.noteOn (n, 0.8f);
        const auto x = render (s, 1.5);
        std::printf ("    default chord: peak %.3f\n", peak (x.L));
        check (finite (x.L) && peak (x.L) < 1.0f && peak (x.L) > 0.05f, "the default patch, a six note chord: loud, clean, under 0 dBFS");
        fm::Synth one; one.p.detune = 0.0f; one.prepare (kSr);
        one.noteOn (57, 0.8f);
        const auto y = render (one, 0.6).L;
        const double share = inharmonicShare (y, 15000, 220.0);
        std::printf ("    default single note inharmonic share %.4f\n", share);
        check (share < 0.02, "SPANNER at zero: the piano stays in tune with itself");
    }
    {
        fm::Synth s; s.p.detune = 0.0f; s.p.spanner = 1.0f; s.prepare (kSr);
        s.noteOn (57, 0.8f);
        const auto x = render (s, 0.8);
        const double share = inharmonicShare (x.L, 20000, 220.0);
        std::printf ("    SPANNER at full: inharmonic share %.3f, peak %.3f\n", share, peak (x.L));
        check (share > 0.3, "SPANNER at full wrecks it");
        check (finite (x.L) && finite (x.R) && peak (x.L) < 1.0f, "and the wreckage is finite and under 0 dBFS");
        bool ok = true; float pk = 0.0f;
        for (int seed = 0; seed < 40; ++seed)
        {
            s.p.seed = seed * 37 + 1; s.p.feedback = 1.0f; s.p.bright = 1.0f; s.p.alg = seed % 8; s.p.grit = (seed % 3) * 0.5f;
            for (int n : { 24, 60, 96 }) s.noteOn (n, 1.0f);
            const auto y = render (s, 0.25);
            ok = ok && finite (y.L) && finite (y.R);
            pk = std::max ({ pk, peak (y.L), peak (y.R) });
            s.allOff (false);
        }
        std::printf ("    forty seeds, every algorithm, flat out: peak %.3f\n", pk);
        check (ok && pk < 1.0f, "forty different spanners, all finite, all under 0 dBFS");
    }
    {
        fm::Synth a; a.p.spanner = 0.6f; a.prepare (kSr);
        fm::Synth b; b.p.spanner = 0.6f; b.prepare (kSr);
        a.noteOn (60, 0.8f); b.noteOn (60, 0.8f);
        const auto x = render (a, 0.3).L, y = render (b, 0.3).L;
        double d = 0.0; for (size_t i = 0; i < x.size(); ++i) d += std::abs ((double) x[i] - y[i]);
        check (d < 1.0e-6, "LOOSE off: the same key with the same seed fails the same way");
        a.allOff (true); a.noteOn (60, 0.8f);
        const auto x2 = render (a, 0.3).L;
        d = 0.0; for (size_t i = 0; i < x.size(); ++i) d += std::abs ((double) x[i] - x2[i]);
        check (d / (double) x.size() < 1.0e-4, "and again on the next press");
        a.p.loose = true; a.allOff (true); a.noteOn (60, 0.8f);
        const auto x3 = render (a, 0.3).L;
        d = 0.0; for (size_t i = 0; i < x.size(); ++i) d += std::abs ((double) x[i] - x3[i]);
        check (d > 1.0, "LOOSE on: every press fails differently");
        a.p.loose = false; a.p.seed = 4242; a.allOff (true); a.noteOn (60, 0.8f);
        const auto x4 = render (a, 0.3).L;
        d = 0.0; for (size_t i = 0; i < x.size(); ++i) d += std::abs ((double) x[i] - x4[i]);
        check (d > 1.0, "a new THROW (seed) fails differently");
    }
    {
        fm::Synth s; s.prepare (kSr);
        for (int k = 0; k < 8; ++k) s.noteOn (48 + k * 2, 0.8f);
        render (s, 0.05);
        check (s.activeVoices() == 8, "eight voices");
        s.noteOn (80, 0.8f);
        render (s, 0.05);
        check (s.activeVoices() == 8, "a ninth note takes one of them");
    }
    {
        fm::Synth s; s.p.detune = 0.0f; s.prepare (kSr);
        s.noteOn (57, 0.8f);
        const auto clean = render (s, 0.3).L;
        s.p.grit = 1.0f; s.allOff (true); s.noteOn (57, 0.8f);
        const auto gritty = render (s, 0.3).L;
        double d = 0.0; for (size_t i = 2000; i < clean.size(); ++i) d += std::abs ((double) clean[i] - gritty[i]);
        check (d / (double) clean.size() > 0.002, "GRIT changes the sound");
    }
    {
        fm::Synth s; s.p.feedback = 0.6f; s.p.spanner = 0.4f; s.prepare (kSr);
        for (int k = 0; k < 8; ++k) s.noteOn (40 + k * 5, 0.9f);
        s.p.sustain = 1.0f;
        const auto t0 = std::chrono::steady_clock::now();
        render (s, 10.0);
        const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        std::printf ("    eight voices, ten seconds rendered in %.2f s (%.0fx real time)\n", secs, 10.0 / secs);
        check (secs < 2.5, "eight voices run comfortably faster than real time");
    }

    std::printf (failures == 0 ? "ALL PASS\n" : "%d FAILED\n", failures);
    return failures == 0 ? 0 : 1;
}
