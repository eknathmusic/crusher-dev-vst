#pragma once
#include <cmath>
#include <algorithm>

// Direct port of distortion_prototype.py (tube_saturate + mech_distort +
// distortion_stage). Same math, same constants -- verified against the
// Python version's measured outputs in test_dsp.cpp before this ever
// touches the plugin wrapper.
class DistortionStage
{
public:
    void setParams (float tubeAmount, float mechAmount) noexcept
    {
        tubeAmt = std::clamp (tubeAmount, 0.0f, 1.0f);
        mechAmt = std::clamp (mechAmount, 0.0f, 1.0f);
    }

    float processSample (float x) const noexcept
    {
        const float tubeOut = (tubeAmt > 0.0f) ? tubeSaturate (x, tubeAmt) : 0.0f;
        const float mechOut = (mechAmt > 0.0f) ? mechDistort (x, mechAmt) : 0.0f;
        const float combined = tubeOut + mechOut;
        const float norm = 1.0f / (1.0f + 0.3f * (tubeAmt + mechAmt));
        return std::tanh (combined * norm);   // master safety stage, same as Python
    }

private:
    float tubeAmt = 0.0f;
    float mechAmt = 0.0f;

    static float tubeSaturate (float x, float amount) noexcept
    {
        const float drive = 1.0f + amount * 9.0f;
        const float bias  = 0.15f * amount;
        const float driven = x * drive + bias;
        return std::tanh (driven) - std::tanh (bias);
    }

    static float mechDistort (float x, float amount) noexcept
    {
        const float drive  = 1.0f + amount * 20.0f;
        const float driven = x * drive;
        const float hard   = std::clamp (driven, -1.0f, 1.0f);
        const float fold   = std::sin (driven * (1.0f + amount));
        const float foldMix = 0.3f * amount;
        return (1.0f - foldMix) * hard + foldMix * fold;
    }
};
