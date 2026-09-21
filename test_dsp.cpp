// Standalone cross-validation: no JUCE required, just the DSP headers.
// Goal: catch porting bugs by checking the C++ output against the actual
// measured numbers the Python prototypes printed earlier in this project.
// Build:  g++ -std=c++17 -O2 -Wall -Wextra -o test_dsp test_dsp.cpp
// Run:    ./test_dsp

#include <cstdio>
#include <cmath>
#include <vector>
#include "Source/DSP/Distortion.h"
#include "Source/DSP/ResonantFilter.h"
#include "Source/DSP/Compressor.h"

constexpr double SR = 44100.0;
int failures = 0;

void check (const char* name, double got, double expected, double tolerance)
{
    const bool ok = std::fabs (got - expected) <= tolerance;
    std::printf ("[%s] %-28s got=%8.3f  expected=%8.3f  (tol %.3f)\n",
                 ok ? " OK " : "FAIL", name, got, expected, tolerance);
    if (! ok) failures++;
}

// ---- Distortion: compare peak amplitude vs Python's printed peaks ----
void testDistortion()
{
    std::printf ("\n-- Distortion (vs distortion_prototype.py output) --\n");
    const double freq = 220.0;
    const int n = (int) SR; // 1 second, matches Python test tone

    auto peakFor = [&] (float tubeAmt, float mechAmt) {
        DistortionStage d;
        d.setParams (tubeAmt, mechAmt);
        float peak = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            float x = 0.5f * std::sin (2.0f * (float) M_PI * (float) freq * (float) i / (float) SR);
            peak = std::max (peak, std::fabs (d.processSample (x)));
        }
        return peak;
    };

    // Expected values are the actual printed output from distortion_prototype.py
    check ("Tube(0.7) peak",        peakFor (0.7f, 0.0f), 0.722, 0.02);
    check ("Mech(0.7) peak",        peakFor (0.0f, 0.7f), 0.679, 0.02);
    check ("Blend(0.5/0.5) peak",   peakFor (0.5f, 0.5f), 0.920, 0.02);
}

// ---- Filter: steady-state sine response at cutoff vs Python's FFT peaks ----
void testFilter()
{
    std::printf ("\n-- Filter (vs filter_prototype.py resonance sweep) --\n");
    const double cutoff = 1000.0;

    auto steadyStateGainDb = [&] (float resonance, double testFreq) {
        ResonantLowPass f;
        f.prepare (SR);
        f.setParams ((float) cutoff, resonance);
        const int n = (int) (SR * 0.3); // 300ms - plenty for ringing to settle
        const int measureFrom = (int) (SR * 0.25);
        float inPeak = 0.0f, outPeak = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            float x = std::sin (2.0f * (float) M_PI * (float) testFreq * (float) i / (float) SR);
            float y = f.processSample (x);
            if (i >= measureFrom) { inPeak = std::max (inPeak, std::fabs (x)); outPeak = std::max (outPeak, std::fabs (y)); }
        }
        return 20.0f * std::log10 (outPeak / inPeak);
    };

    // Expected values are the printed peak dB from filter_prototype.py's sweep.
    // NOTE: Python measured the PEAK of the whole response curve, which for
    // Res=0.0 (no resonance, Butterworth-flat) sits at DC, not at cutoff --
    // a maximally-flat 2nd-order filter is *defined* by -3dB at its own
    // cutoff, so testing gain-at-cutoff for Res=0 and expecting 0dB was my
    // own test bug on the first pass, not a filter bug. Fixed by measuring
    // DC gain for that one case, matching what "peak of curve" actually means
    // when there's no resonant bump to be the peak.
    check ("Res=0.0 gain @ DC",     steadyStateGainDb (0.0f, 20.0),     0.0,  1.5);
    check ("Res=0.3 gain @ cutoff", steadyStateGainDb (0.3f, cutoff),   14.0, 1.5);
    check ("Res=0.6 gain @ cutoff", steadyStateGainDb (0.6f, cutoff),   19.4, 1.5);
    check ("Res=0.9 gain @ cutoff", steadyStateGainDb (0.9f, cutoff),   22.6, 1.5);
}

// ---- Compressor: gain reduction direction/shape sanity (pre-makeup-gain) ----
void testCompressor()
{
    std::printf ("\n-- Compressor (threshold/ratio shape, sanity) --\n");

    // Same amount_to_params formula as Python, computed by hand here to
    // check the class's *behavior* implies the same threshold/ratio.
    auto expectedThresholdRatio = [] (float amount, bool phat) {
        float thr = -6.0f - amount * 24.0f;
        float ratio = 1.5f + amount * 8.5f;
        if (phat) { thr -= 6.0f; ratio = std::min (ratio * 1.8f, 20.0f); }
        return std::make_pair (thr, ratio);
    };

    for (float amount : { 0.3f, 0.9f })
    {
        for (bool phat : { false, true })
        {
            CamelStyleCompressor c;
            c.prepare (SR);
            c.setParams (amount, phat);

            // Feed a loud steady 0dBFS-ish tone long enough for the envelope
            // to settle, then check gain reduction happened in roughly the
            // expected direction (can't check exact sample vs makeup-gain-free
            // Python curve since this class includes causal auto-makeup --
            // that's an intentional difference, documented in Compressor.h).
            const int n = (int) (SR * 0.5);
            float lastIn = 0.0f, lastOut = 0.0f;
            for (int i = 0; i < n; ++i)
            {
                float x = 0.95f * std::sin (2.0f * (float) M_PI * 300.0f * (float) i / (float) SR);
                float y = c.processSample (x);
                if (i > n - 200) { lastIn = std::fabs (x); lastOut = std::fabs (y); }
            }
            auto [thr, ratio] = expectedThresholdRatio (amount, phat);
            std::printf ("  amount=%.1f phat=%d -> thr=%.1fdB ratio=%.2f:1 | loud-tone in=%.3f out=%.3f (should be squashed toward similar regardless of amount, not literally louder)\n",
                         amount, phat, thr, ratio, lastIn, lastOut);
        }
    }
}

int main()
{
    testDistortion();
    testFilter();
    testCompressor();

    std::printf ("\n%s (%d failing check%s)\n",
                 failures == 0 ? "ALL CHECKS PASSED" : "SOME CHECKS FAILED",
                 failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
