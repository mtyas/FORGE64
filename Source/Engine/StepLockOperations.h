#pragma once
#include "StepSequencer.h"

namespace f64 {

// Order matches the sequencer's lock view buttons.
enum class StepLockParameter { Velocity, Probability, Microtiming, Pitch, Decay, Drive, Level, Pan, Ratchet };

inline void applyTrackLocks(TrackData& track, StepLockParameter parameter,
                            bool randomize, float amount, juce::Random& random)
{
    amount = juce::jlimit(0.0f, 1.0f, amount);
    const StepData defaults;
    for (auto& step : track.steps)
    {
        // Reset hidden steps too, so extending the track cannot revive old locks.
        // Randomize only steps within the track length, keeping activation intact.
        if (randomize && &step >= track.steps.data() + track.stepCount)
            break;
        uint64_t flag = 0;
        auto value = [&](float def, float min, float max)
        {
            return randomize ? def + amount * (min + random.nextFloat() * (max - min) - def) : def;
        };
        switch (parameter)
        {
            case StepLockParameter::Ratchet: step.ratchet = juce::jlimit(1, 8, (int)std::round(value(1.f, 1.f, 8.f))); break;
            case StepLockParameter::Velocity: step.velocity = value(defaults.velocity, 0.05f, 1.f); break;
            case StepLockParameter::Probability: step.probability = value(defaults.probability, 0.f, 1.f); break;
            case StepLockParameter::Microtiming: step.microtiming = value(defaults.microtiming, -0.5f, 0.5f); break;
            case StepLockParameter::Pitch: step.pLockPitch = value(defaults.pLockPitch, -24.f, 24.f); flag = LOCK_FLAG_PITCH; break;
            case StepLockParameter::Decay: step.pLockDecay = value(defaults.pLockDecay, 0.05f, 5.f); flag = LOCK_FLAG_DECAY; break;
            case StepLockParameter::Drive: step.pLockDrive = value(defaults.pLockDrive, 0.f, 1.f); flag = LOCK_FLAG_DRIVE; break;
            case StepLockParameter::Level: step.pLockLevel = value(defaults.pLockLevel, 0.f, 1.5f); flag = LOCK_FLAG_LEVEL; break;
            case StepLockParameter::Pan: step.pLockPan = value(defaults.pLockPan, -1.f, 1.f); flag = LOCK_FLAG_PAN; break;
        }
        if (parameter == StepLockParameter::Ratchet)
            step.hasLocks = (step.lockMask & ~LOCK_FLAG_PAD_OVERRIDE) != 0 || step.ratchet > 1;
        if (flag != 0)
        {
            if (randomize) step.lockMask |= flag;
            else step.lockMask &= ~flag;
            step.hasLocks = (step.lockMask & ~LOCK_FLAG_PAD_OVERRIDE) != 0 || step.ratchet > 1;
        }
    }
}

} // namespace f64
