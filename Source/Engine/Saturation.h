#pragma once
#include <cmath>
#include <algorithm>

namespace f64 {

// Keep drive from acting as a large input boost. Ramp RMS compensation to
// avoid abrupt gain changes at block boundaries.
inline void saturateStereo(float* left, float* right, int count, float amount,
                           float maxGain, float& makeup)
{
    amount = std::clamp(amount, 0.f, 1.f);
    if (count <= 0) return;
    if (amount <= 0.001f) { makeup = 1.f; return; }
    const float gain = 1.f + amount * amount * (maxGain - 1.f);
    double dryEnergy = 0.0, wetEnergy = 0.0;
    for (int i = 0; i < count; ++i)
    {
        dryEnergy += left[i] * left[i] + right[i] * right[i];
        left[i] = std::tanh(left[i] * gain) / gain;
        right[i] = std::tanh(right[i] * gain) / gain;
        wetEnergy += left[i] * left[i] + right[i] * right[i];
    }
    const float target = wetEnergy > 1.e-12 ? (float) std::sqrt(dryEnergy / wetEnergy) : 1.f;
    // A loud block followed by a quiet one must not inherit a large makeup
    // gain. Bound the ramp around the current block's measured target.
    const float start = std::clamp(makeup, target * 0.8f, target * 1.25f);
    for (int i = 0; i < count; ++i)
    {
        const float compensation = start + (target - start) * (float) (i + 1) / (float) count;
        left[i] *= compensation;
        right[i] *= compensation;
    }
    makeup = target;
}

} // namespace f64
