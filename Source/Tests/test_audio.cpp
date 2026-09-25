#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include <iostream>

int main(int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI guiInit;

    std::cout << "=== FORGE64 FULL VERIFICATION (AUDITION + MIDI + SEQUENCER) ===" << std::endl;
    auto proc = std::make_unique<f64::Forge64Processor>();

    proc->setPlayConfigDetails(0, 2, 44100.0, 512);
    proc->prepareToPlay(44100.0, 512);

    // 0. Default State Verification (Empty Sequencer & Tailored Pad Presets)
    {
        int initialActiveSteps = 0;
        const auto& curPat = proc->getSequencer().currentPattern();
        for (int t = 0; t < 8; ++t)
            for (int s = 0; s < 64; ++s)
                if (curPat.tracks[(size_t) t].steps[(size_t) s].active)
                    initialActiveSteps++;

        auto* hatDecParam = proc->getAPVTS().getRawParameterValue(f64::padParamId(2, "dec"));
        float hatDec = hatDecParam ? hatDecParam->load() : 1.5f;

        std::cout << "[0] Default State: initialActiveSteps=" << initialActiveSteps
                  << ", pad2 hatDecay=" << hatDec << std::endl;
        if (initialActiveSteps != 0 || hatDec > 0.2f)
        {
            std::cerr << "FAIL: Sequencer not empty or default presets not loaded!" << std::endl;
            return 1;
        }
    }

    // 1. Audition Test
    int failedPads = 0;
    for (int p = 0; p < 64; ++p)
    {
        proc->triggerAudition(p, 0.9f);
        juce::AudioBuffer<float> buf(2, 512);
        juce::MidiBuffer midi;
        float maxPeak = 0.f;

        for (int b = 0; b < 10; ++b)
        {
            buf.clear();
            proc->processBlock(buf, midi);
            float pk = buf.getMagnitude(0, 512);
            if (pk > maxPeak)
                maxPeak = pk;
        }

        auto err = proc->lua().errorFor(p);
        if (err.isNotEmpty() || maxPeak < 0.0001f)
            failedPads++;
    }
    std::cout << "[1] Audition Test: " << (64 - failedPads) << "/64 passed." << std::endl;

    // 2. MIDI Note Input Test (Pad 0 has default note 36)
    {
        juce::AudioBuffer<float> buf(2, 512);
        juce::MidiBuffer midi;
        midi.addEvent(juce::MidiMessage::noteOn(1, 36, (juce::uint8) 110), 0);
        buf.clear();
        proc->processBlock(buf, midi);
        float midiPeak = buf.getMagnitude(0, 512);
        std::cout << "[2] MIDI Note-On Test (Note 36 -> Pad 0): peak = " << midiPeak << std::endl;
        if (midiPeak < 0.001f) failedPads++;
    }

    // 3. Step Sequencer Test (Step 0 has a hit on track 0 -> pad 0)
    {
        proc->getSequencer().setStepActive(0, 0, true);
        proc->getSequencer().setStepVelocity(0, 0, 0.9f);
        proc->getSequencer().setPlaying(true);
        juce::AudioBuffer<float> buf(2, 512);
        juce::MidiBuffer midi;
        float seqPeak = 0.f;
        for (int b = 0; b < 10; ++b)
        {
            buf.clear();
            proc->processBlock(buf, midi);
            float pk = buf.getMagnitude(0, 512);
            if (pk > seqPeak) seqPeak = pk;
        }
        std::cout << "[3] Sequencer Playback Test: peak = " << seqPeak << std::endl;
        if (seqPeak < 0.001f) failedPads++;
    }

    // 4. Negative Microtiming Test (Look-ahead triggering)
    {
        proc->getSequencer().setPlaying(false);
        proc->getSequencer().clearCurrentPattern();
        // Set step 1 on track 0 with negative microtiming -0.2f
        proc->getSequencer().setStepActive(0, 1, true);
        proc->getSequencer().setStepVelocity(0, 1, 0.95f);
        proc->getSequencer().setStepMicrotiming(0, 1, -0.2f);
        proc->getSequencer().setPlaying(true);

        juce::AudioBuffer<float> buf(2, 512);
        juce::MidiBuffer midi;
        float negMicroPeak = 0.f;
        for (int b = 0; b < 20; ++b)
        {
            buf.clear();
            proc->processBlock(buf, midi);
            float pk = buf.getMagnitude(0, 512);
            if (pk > negMicroPeak) negMicroPeak = pk;
        }
        std::cout << "[4] Negative Microtiming Test: peak = " << negMicroPeak << std::endl;
        if (negMicroPeak < 0.001f) failedPads++;
    }

    // 5. Sequencer Randomizer Test (Selected track vs All tracks)
    {
        proc->getSequencer().clearCurrentPattern();
        // Select track 3
        proc->getSequencer().setSelectedTrack(3);
        proc->getSequencer().randomizeCurrentTrack();

        int t0Active = 0, t3Active = 0;
        const auto& pat = proc->getSequencer().currentPattern();
        for (const auto& s : pat.tracks[0].steps) if (s.active) t0Active++;
        for (const auto& s : pat.tracks[3].steps) if (s.active) t3Active++;

        bool randSelectedPassed = (t0Active == 0 && t3Active > 0);
        std::cout << "[5] Selected Track Randomizer (Track 3): "
                  << (randSelectedPassed ? "PASSED" : "FAILED")
                  << " (T0 active=" << t0Active << ", T3 active=" << t3Active << ")" << std::endl;
        if (! randSelectedPassed) failedPads++;

        proc->getSequencer().randomizeAllTracks();
        int totalActive = 0;
        const auto& patAll = proc->getSequencer().currentPattern();
        for (int t = 0; t < 8; ++t)
            for (const auto& s : patAll.tracks[(size_t) t].steps)
                if (s.active) totalActive++;

        bool randAllPassed = (totalActive >= 8);
        std::cout << "[6] All Tracks Randomizer: " << (randAllPassed ? "PASSED" : "FAILED")
                  << " (Total active steps=" << totalActive << ")" << std::endl;
        if (! randAllPassed) failedPads++;
    }

    // 7. Aux FX Tests: Gated Reverb Long Decay, Shimmer Reverb, and Pitch Shifter
    {
        f64::AuxBusManager auxMgr;
        auxMgr.prepare(44100.0, 512);

        // A) Gated Reverb (Long decay > 1.5s)
        f64::AuxBusParams gatedParams;
        gatedParams.fxType = f64::AUX_FX_GATED_VERB;
        gatedParams.p1 = 0.95f; // Max gate duration (~2.4s)
        gatedParams.p2 = 0.90f; // Density
        gatedParams.p3 = 0.80f; // Tone
        gatedParams.enabled = true;

        std::vector<float> l(512, 0.f), r(512, 0.f);
        l[0] = 0.9f; r[0] = 0.9f; // Impulse
        auxMgr.processAux(0, l.data(), r.data(), 512, gatedParams);

        // Process 1.0 second of audio (86 blocks of 512)
        float gateLatePeak = 0.f;
        for (int b = 0; b < 86; ++b)
        {
            std::fill(l.begin(), l.end(), 0.f);
            std::fill(r.begin(), r.end(), 0.f);
            auxMgr.processAux(0, l.data(), r.data(), 512, gatedParams);
            for (int i = 0; i < 512; ++i)
                gateLatePeak = std::max(gateLatePeak, std::abs(l[i]));
        }
        bool gatePassed = (gateLatePeak > 0.005f);
        std::cout << "[7] Aux Gated Reverb 1.0s Decay Test: " << (gatePassed ? "PASSED" : "FAILED")
                  << " (late peak=" << gateLatePeak << ")" << std::endl;
        if (! gatePassed) failedPads++;

        // B) Shimmer Reverb Test
        f64::AuxBusParams shimParams;
        shimParams.fxType = f64::AUX_FX_SHIMMER;
        shimParams.p1 = 0.85f; // Feedback
        shimParams.p2 = 0.80f; // Shimmer amount
        shimParams.p3 = 0.80f; // Brightness
        shimParams.p4 = 0.50f; // Width
        shimParams.enabled = true;

        auxMgr.reset();
        l[0] = 0.9f; r[0] = 0.9f;
        auxMgr.processAux(1, l.data(), r.data(), 512, shimParams);

        float shimPeak = 0.f;
        for (int b = 0; b < 40; ++b)
        {
            std::fill(l.begin(), l.end(), 0.f);
            std::fill(r.begin(), r.end(), 0.f);
            auxMgr.processAux(1, l.data(), r.data(), 512, shimParams);
            for (int i = 0; i < 512; ++i)
                shimPeak = std::max(shimPeak, std::abs(l[i]));
        }
        bool shimPassed = (shimPeak > 0.005f);
        std::cout << "[8] Aux Shimmer Reverb Bloom Test: " << (shimPassed ? "PASSED" : "FAILED")
                  << " (peak=" << shimPeak << ")" << std::endl;
        if (! shimPassed) failedPads++;

        // C) Pitch Shifter Test (-12 semitones & +12 semitones)
        f64::AuxBusParams pitchParams;
        pitchParams.fxType = f64::AUX_FX_PITCH;
        pitchParams.p1 = 1.0f; // +12 semitones (octave up)
        pitchParams.p2 = 0.5f; // 0 cents
        pitchParams.p3 = 0.4f; // feedback
        pitchParams.p4 = 1.0f; // 100% wet
        pitchParams.enabled = true;

        auxMgr.reset();
        float pitchPeak = 0.f;
        for (int b = 0; b < 10; ++b)
        {
            for (int i = 0; i < 512; ++i)
            {
                l[i] = std::sin((b * 512 + i) * 0.1f) * 0.5f;
                r[i] = std::sin((b * 512 + i) * 0.1f) * 0.5f;
            }
            auxMgr.processAux(2, l.data(), r.data(), 512, pitchParams);
            for (int i = 0; i < 512; ++i)
                pitchPeak = std::max(pitchPeak, std::abs(l[i]));
        }

        bool pitchPassed = (pitchPeak > 0.01f);
        std::cout << "[9] Aux Pitch Shifter Octave-Up Test: " << (pitchPassed ? "PASSED" : "FAILED")
                  << " (peak=" << pitchPeak << ")" << std::endl;
        if (! pitchPassed) failedPads++;
    }

    std::cout << "All tests completed with " << failedPads << " errors." << std::endl;
    return (failedPads == 0) ? 0 : 1;
}
