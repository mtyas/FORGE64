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

    std::cout << "All tests completed with " << failedPads << " errors." << std::endl;
    return (failedPads == 0) ? 0 : 1;
}
