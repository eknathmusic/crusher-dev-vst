#pragma once
#include <cmath>
#include <algorithm>

// Port of compressor_prototype.py's CamelStyleCompressor, with ONE
// deliberate change: the Python version used match_loudness=True, which
// scales output by the RATIO OF WHOLE-BUFFER RMS values -- that's an
// offline trick (it needs to see the entire signal, including "future"
// samples, before it can compute an RMS to match against). A real-time
// plugin only ever sees samples up to "now", so that approach isn't
// available here.
//
// Replacement: a standard causal "auto makeup gain" heuristic -- compute
// how much gain reduction a signal sitting at 0dBFS would receive at the
// current threshold/ratio, and compensate by half of that. This is a
// common real-world approximation (full 0dBFS-referenced compensation
// tends to overshoot on real program material, since peaks don't sit at
// 0dBFS constantly) and, critically, it only depends on the current
// threshold/ratio settings, not on future samples.
class CamelStyleCompressor
{
public:
    void prepare (double newSampleRate) noexcept
    {
        sampleRate = newSampleRate;
        attackCoef  = onePoleCoef (attackMs);
        releaseCoef = onePoleCoef (releaseMs);
        reset();
    }

    void reset() noexcept { envDb = -60.0f; }

    void setParams (float amount01, bool phatOn) noexcept
    {
        amount01 = std::clamp (amount01, 0.0f, 1.0f);
        thresholdDb = -6.0f - amount01 * 24.0f;
        ratio = 1.5f + amount01 * 8.5f;
        if (phatOn)
        {
            thresholdDb -= 6.0f;
            ratio = std::min (ratio * 1.8f, 20.0f);
        }
        phat = phatOn;

        const float overAtZero = -thresholdDb;
        const float grAtZero = (overAtZero <= 0.0f) ? 0.0f
                                                      : overAtZero * (1.0f - 1.0f / ratio);
        makeupGainLin = std::pow (10.0f, (grAtZero * 0.5f) / 20.0f);
    }

    float processSample (float x) noexcept
    {
        const float levelDb = 20.0f * std::log10 (std::abs (x) + 1.0e-9f);
        const float coef = (levelDb > envDb) ? attackCoef : releaseCoef;
        envDb = coef * envDb + (1.0f - coef) * levelDb;

        const float over = envDb - thresholdDb;
        const float grDb = (over <= 0.0f) ? 0.0f : over * (1.0f - 1.0f / ratio);
        const float gain = std::pow (10.0f, -grDb / 20.0f);

        float y = x * gain * makeupGainLin;
        if (phat)
            y = 0.8f * y + 0.2f * std::tanh (y * 2.5f);
        return y;
    }

private:
    double sampleRate = 44100.0;
    float attackMs = 8.0f;
    float releaseMs = 120.0f;
    float attackCoef = 0.0f, releaseCoef = 0.0f;
    float envDb = -60.0f;
    float thresholdDb = -6.0f, ratio = 1.5f;
    float makeupGainLin = 1.0f;
    bool phat = false;

    float onePoleCoef (float timeMs) const noexcept
    {
        return std::exp (-1.0f / (0.001f * timeMs * (float) sampleRate));
    }
};
