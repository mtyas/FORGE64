#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include "../Presets/ModulePresetManager.h"
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

        // Clear pattern while playing test
        proc->getSequencer().loadFactoryPreset(0);
        proc->getSequencer().setPlaying(true);
        juce::AudioBuffer<float> playBuf(2, 512);
        juce::MidiBuffer playMidi;
        for (int b = 0; b < 10; ++b)
        {
            playBuf.clear();
            proc->processBlock(playBuf, playMidi);
        }
        const int patIdx = proc->getSequencer().selectedPatternIndex();
        auto before = proc->getSequencer().getPatternCopy(patIdx);
        auto after = std::make_unique<f64::PatternData>(*before);
        for (auto& t : after->tracks)
            for (auto& s : t.steps)
                s.resetStep();
        proc->getUndoManager().perform(new f64::SequencerPatternAction(proc->getSequencer(), patIdx, std::move(before), std::move(after)));
        for (int b = 0; b < 10; ++b)
        {
            playBuf.clear();
            proc->processBlock(playBuf, playMidi);
        }
        proc->getUndoManager().undo();
        for (int b = 0; b < 10; ++b)
        {
            playBuf.clear();
            proc->processBlock(playBuf, playMidi);
        }
        proc->getSequencer().setPlaying(false);
        std::cout << "[6.5] Clear Pattern While Playing Test: PASSED" << std::endl;
    }

    // 6.6 Step Parameter Locks Clearing and Step Reset Verification
    {
        f64::StepData s;
        s.active = true;
        s.velocity = 0.9f;
        s.padOverride = 5;
        s.pLockPitch = 7.0f;
        s.pLockDecay = 0.4f;
        s.hasLocks = true;
        s.lockMask = f64::StepLockFlags::LOCK_FLAG_PITCH | f64::StepLockFlags::LOCK_FLAG_DECAY | f64::StepLockFlags::LOCK_FLAG_PAD_OVERRIDE;

        // Clear locks must reset hasLocks and lockMask while preserving padOverride
        s.clearLocks();
        bool clearPassed = (! s.hasLocks && s.lockMask == 0 && s.padOverride == 5);
        if (! clearPassed) failedPads++;

        // Reset step must erase everything: inactive, default velocity, padOverride = -1, hasLocks = false
        s.resetStep();
        bool resetPassed = (! s.active && s.padOverride == -1 && ! s.hasLocks && s.lockMask == 0);
        if (! resetPassed) failedPads++;

        std::cout << "[6.6] Step Clear Locks and Reset Test: "
                  << (clearPassed && resetPassed ? "PASSED" : "FAILED") << std::endl;
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

        // B) Studio Plate Reverb Test
        f64::AuxBusParams plateParams;
        plateParams.fxType = f64::AUX_FX_PLATE;
        plateParams.p1 = 0.85f; // Decay
        plateParams.p2 = 0.60f; // Size
        plateParams.p3 = 0.40f; // Damp
        plateParams.p4 = 0.75f; // Diffusion
        plateParams.enabled = true;

        auxMgr.reset();
        l[0] = 0.9f; r[0] = 0.9f;
        auxMgr.processAux(1, l.data(), r.data(), 512, plateParams);

        float platePeak = 0.f;
        for (int b = 0; b < 40; ++b)
        {
            std::fill(l.begin(), l.end(), 0.f);
            std::fill(r.begin(), r.end(), 0.f);
            auxMgr.processAux(1, l.data(), r.data(), 512, plateParams);
            for (int i = 0; i < 512; ++i)
                platePeak = std::max(platePeak, std::abs(l[i]));
        }
        bool platePassed = (platePeak > 0.005f);
        std::cout << "[8] Aux Studio Plate Reverb Test: " << (platePassed ? "PASSED" : "FAILED")
                  << " (peak=" << platePeak << ")" << std::endl;
        if (! platePassed) failedPads++;

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

    // 10. Simultaneous All-64-Pads Strike & Clean Summing Verification
    {
        proc->allNotesOff();
        juce::MidiBuffer chordMidi;
        for (int p = 0; p < 64; ++p)
        {
            // All 64 notes triggered simultaneously on sample 0
            chordMidi.addEvent(juce::MidiMessage::noteOn(1, 36 + p, (juce::uint8) 120), 0);
        }

        juce::AudioBuffer<float> buf(2, 512);
        buf.clear();
        proc->processBlock(buf, chordMidi);

        float maxPeak = 0.f;
        bool hasNaN = false;
        for (int ch = 0; ch < 2; ++ch)
        {
            const float* rd = buf.getReadPointer(ch);
            for (int i = 0; i < 512; ++i)
            {
                if (std::isnan(rd[i]) || std::isinf(rd[i]))
                    hasNaN = true;
                maxPeak = std::max(maxPeak, std::abs(rd[i]));
            }
        }

        // Process another 10 blocks to verify decay and limiter stability
        for (int b = 0; b < 10; ++b)
        {
            buf.clear();
            juce::MidiBuffer emptyMidi;
            proc->processBlock(buf, emptyMidi);
            for (int ch = 0; ch < 2; ++ch)
            {
                const float* rd = buf.getReadPointer(ch);
                for (int i = 0; i < 512; ++i)
                {
                    if (std::isnan(rd[i]) || std::isinf(rd[i]))
                        hasNaN = true;
                    maxPeak = std::max(maxPeak, std::abs(rd[i]));
                }
            }
        }

        // Must be clean: non-zero, healthy audio, no NaNs, and cleanly contained within ceiling (<= 1.05)
        bool allPadsPassed = (! hasNaN && maxPeak > 0.05f && maxPeak <= 1.05f);
        std::cout << "[10] All-64-Pads Simultaneous Strike Test: "
                  << (allPadsPassed ? "PASSED" : "FAILED")
                  << " (peak=" << maxPeak << ", hasNaN=" << (hasNaN ? "YES" : "NO") << ")" << std::endl;
        if (! allPadsPassed) failedPads++;
    }

    // 11. Custom Lua Preset & Category Persistence Test
    {
        bool p11Passed = true;
        const juce::String testScript1 = "-- Test Custom Lua Algorithm 1\nfunction process()\nend\n";
        const juce::String testScript2 = "-- Test Custom Lua Algorithm 2\nfunction process()\nend\n";

        // Test 1: Save module & preset to "Custom" category
        f64::ModuleInfo m1;
        m1.id = "user_test_lua_custom";
        m1.name = "Test Custom Sound";
        m1.category = "Custom";
        m1.scriptCode = testScript1;
        m1.p1Label = "CUSTOM1";
        if (! f64::ModulePresetManager::saveUserModule(m1))
            p11Passed = false;

        f64::SoundPreset sp1;
        sp1.name = "Test Custom Sound";
        sp1.moduleId = "user_test_lua_custom";
        sp1.category = "Custom";
        sp1.scriptCode = testScript1;
        sp1.p1Label = "CUSTOM1";
        if (! f64::ModulePresetManager::saveSoundPreset(sp1))
            p11Passed = false;

        // Test 2: Save module & preset to "Kicks" category
        f64::ModuleInfo m2;
        m2.id = "user_test_lua_kick";
        m2.name = "Test Lua Kick";
        m2.category = "Kicks";
        m2.scriptCode = testScript2;
        m2.p1Label = "KICK_P1";
        if (! f64::ModulePresetManager::saveUserModule(m2))
            p11Passed = false;

        f64::SoundPreset sp2;
        sp2.name = "Test Lua Kick";
        sp2.moduleId = "user_test_lua_kick";
        sp2.category = "Kicks";
        sp2.scriptCode = testScript2;
        sp2.p1Label = "KICK_P1";
        if (! f64::ModulePresetManager::saveSoundPreset(sp2))
            p11Passed = false;

        // Verify retrieval in "Custom" category (should contain BOTH m1 and m2)
        auto customSounds = f64::ModulePresetManager::getSoundsForCategory("Custom");
        bool foundM1InCustom = false;
        bool foundM2InCustom = false;
        for (const auto& cs : customSounds)
        {
            if (cs.moduleId == "user_test_lua_custom" && cs.preset.scriptCode == testScript1)
                foundM1InCustom = true;
            if (cs.moduleId == "user_test_lua_kick" && cs.preset.scriptCode == testScript2)
                foundM2InCustom = true;
        }

        // Verify retrieval in "Kicks" category (should contain m2)
        auto kickSounds = f64::ModulePresetManager::getSoundsForCategory("Kicks");
        bool foundM2InKicks = false;
        for (const auto& ks : kickSounds)
        {
            if (ks.moduleId == "user_test_lua_kick" && ks.preset.scriptCode == testScript2)
                foundM2InKicks = true;
        }

        if (! foundM1InCustom || ! foundM2InCustom || ! foundM2InKicks)
            p11Passed = false;

        // Clean up test files
        f64::ModulePresetManager::deleteSoundPreset("user_test_lua_custom", "Test Custom Sound");
        f64::ModulePresetManager::deleteSoundPreset("user_test_lua_kick", "Test Lua Kick");

        std::cout << "[11] Custom Lua Preset & Category Persistence Test: "
                  << (p11Passed ? "PASSED" : "FAILED")
                  << " (foundM1Custom=" << foundM1InCustom
                  << ", foundM2Custom=" << foundM2InCustom
                  << ", foundM2Kicks=" << foundM2InKicks << ")" << std::endl;
        if (! p11Passed) failedPads++;
    }

    // 12. Dynamic Macro Labels Extraction Test
    {
        juce::String testScript =
            "-- Custom Snare DSP\n"
            "-- @p1: SNAP\n"
            "-- @p2: RATIO\n"
            "-- @p3: NOISE COLOR\n"
            "-- @p4: TENSION\n"
            "-- @p5: BLEED\n\n"
            "function process() outL(0, 0) end\n";

        auto labels = f64::parseMacroLabelsFromScript(testScript);
        bool p12Passed = (labels.p1 == "SNAP" &&
                          labels.p2 == "RATIO" &&
                          labels.p3 == "NOISE COLOR" &&
                          labels.p4 == "TENSION" &&
                          labels.p5 == "BLEED");

        // Test alternative @labels format
        juce::String altScript = "-- @labels: ATTACK, BODY, CLICK, WARMTH, TAIL\n";
        auto altLabels = f64::parseMacroLabelsFromScript(altScript);
        if (altLabels.p1 != "ATTACK" || altLabels.p2 != "BODY" || altLabels.p3 != "CLICK" ||
            altLabels.p4 != "WARMTH" || altLabels.p5 != "TAIL")
            p12Passed = false;

        std::cout << "[12] Dynamic Script Macro Labels Test: "
                  << (p12Passed ? "PASSED" : "FAILED")
                  << " (p1=" << labels.p1 << ", p2=" << labels.p2 << ", p3=" << labels.p3
                  << ", p4=" << labels.p4 << ", p5=" << labels.p5 << ")" << std::endl;
        if (! p12Passed) failedPads++;
    }

    // 13. Modulation Step Sequencer Arbitrary Steps (1-32) & Melodic Quantization Test
    {
        bool p13Passed = true;
        // Test quantizeValue helper: C Major Pentatonic (C=0, D=2, E=4, G=7, A=9)
        const uint16_t pentatonicMask = (1 << 0) | (1 << 2) | (1 << 4) | (1 << 7) | (1 << 9);
        const int octaves = 1; // 12 semitones

        // If continuous pitch is at semitone 1 (C#), closest allowed pentatonic note is either 0 (C) or 2 (D)
        float vCsharp = 1.0f / 12.0f;
        float qVal = f64::quantizeValue(vCsharp, pentatonicMask, octaves);
        int quantizedSemitone = (int) std::round(qVal * 12.0f);
        if (quantizedSemitone != 0 && quantizedSemitone != 2)
            p13Passed = false;

        // Test noteNameForValue
        auto noteNameC = f64::noteNameForValue(0.0f, octaves);
        auto noteNameTop = f64::noteNameForValue(1.0f, octaves);
        if (noteNameC != "C1" || noteNameTop != "C2")
            p13Passed = false;

        // Test SeqSource with 7 steps and rendering
        f64::SeqSource seq;
        seq.enabled = true;
        seq.numSteps = 7;
        seq.quantize = true;
        seq.noteMask = pentatonicMask;
        seq.octaves = 2; // 24 semitones
        seq.rate = 100.0f; // fast rate
        seq.sync = false;
        seq.gate = 1.0f;
        seq.slew = 0.0f;
        for (int i = 0; i < 7; ++i)
            seq.steps[(size_t) i] = (float) i / 7.0f;

        float outBuf[128];
        seq.prepare(44100.0, 128);
        seq.render(outBuf, 128);

        // Check that rendered values strictly adhere to pentatonic notes
        for (int i = 0; i < 128; ++i)
        {
            float v = juce::jlimit(0.f, 1.f, outBuf[i]);
            int semi = (int) std::round(v * 24.0f);
            int noteInOctave = semi % 12;
            if ((pentatonicMask & (1 << noteInOctave)) == 0)
            {
                p13Passed = false;
                break;
            }
        }

        std::cout << "[13] Mod Seq 1-32 Steps & Melodic Quantization Test: "
                  << (p13Passed ? "PASSED" : "FAILED")
                  << " (C1=" << noteNameC << ", Top=" << noteNameTop << ", qSemi=" << quantizedSemitone << ")" << std::endl;
        if (! p13Passed) failedPads++;
    }

    std::cout << "All tests completed with " << failedPads << " errors." << std::endl;
    return (failedPads == 0) ? 0 : 1;
}
