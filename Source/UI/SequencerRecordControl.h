#pragma once
#include "../PluginProcessor.h"

namespace f64 {
inline void setupSequencerRecordButton(juce::TextButton& button, Forge64Processor& processor)
{
    button.setComponentID("sequencerRecord");
    button.setTooltip("Overdub mouse or MIDI pad hits onto the selected track. A new hit replaces its step; press again to stop.");
    button.onClick = [&button, &processor]
    {
        auto& seq = processor.getSequencer();
        if (seq.isPadRecording()) { seq.stopPadRecording(); return; }
        seq.startPadRecording(seq.selectedTrackIndex(), seq.selectedPatternIndex());
    };
}
}
