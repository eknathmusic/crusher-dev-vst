#pragma once
#include <cmath>
#include <algorithm>

// Direct port of filter_prototype.py's ResonantLowPass (TPT / "Cytomic"
// zero-delay-feedback State Variable Filter, Andrew Simper's design --
// generic published DSP topology, same math as the validated Python version.
class ResonantLowPass
{
public:
    void prepare (double newSampleRate) noexcept
    {
        sampleRate = newSampleRate;
        reset();
    }

    void setParams (float cutoffHz, float resonance01) noexcept
    {
        resonance01 = std::clamp (resonance01, 0.0f, 1.0f);
        const float q = 0.707f + resonance01 * 14.3f;   // 0.707 .. ~15 (near self-osc)
        k = 1.0f / q;
        const float g = std::tan (pi * cutoffHz / (float) sampleRate);
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    void reset() noexcept
    {
        ic1eq = 0.0f;
        ic2eq = 0.0f;
    }

    float processSample (float x) noexcept
    {
        const float v3 = x - ic2eq;
        const float v1 = a1 * ic1eq + a2 * v3;
        const float v2 = ic2eq + a2 * ic1eq + a3 * v3;
        ic1eq = 2.0f * v1 - ic1eq;
        ic2eq = 2.0f * v2 - ic2eq;
        return v2;   // lowpass output
    }

private:
    static constexpr float pi = 3.14159265358979323846f;

    double sampleRate = 44100.0;
    float a1 = 0.0f, a2 = 0.0f, a3 = 0.0f, k = 1.0f;
    float ic1eq = 0.0f, ic2eq = 0.0f;
};
