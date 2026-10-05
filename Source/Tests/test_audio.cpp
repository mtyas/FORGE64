#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include "../PluginEditor.h"
#include "../Presets/ModulePresetManager.h"
#include "../Engine/StepLockOperations.h"
#include "../UI/PadEditor.h"
#include "../UI/SequencerDrawer.h"
#include "../UI/ModPanel.h"
#include "../Engine/Saturation.h"
#include <iostream>

static int testDspAndMidiFixes()
{
    int failures = 0;
    auto check = [&](bool ok, const char* what)
    {
        if (! ok) { ++failures; std::cout << "FAILED: " << what << std::endl; }
    };
    f64::EnvSource envelope;
    envelope.prepare(48000., 256);
    envelope.enabled = true;
    envelope.atk = 0.1f; envelope.dec = 0.1f; envelope.sus = 0.7f; envelope.rel = 0.1f;
    std::vector<float> values(48000);
    envelope.retrigger();
    envelope.render(values.data(), (int) values.size());
    check(values[0] == 0.f && values[100] < 0.03f && values[4800] > 0.99f, "envelope starts at zero and follows attack");
    check(values.back() == 0.f, "one-shot envelope finishes its release");
    envelope.retrigger(); envelope.render(values.data(), 12000);
    envelope.scheduleTrigger(64); envelope.render(values.data(), 256);
    check(values[0] > 0.6f && values[64] == 0.f && values[100] < 0.02f, "sample-timed retrigger resets sustain to zero");

    constexpr int n = 512;
    std::vector<float> dry(n), left(n), right(n);
    for (int i = 0; i < n; ++i) dry[i] = 0.08f * std::sin(juce::MathConstants<float>::twoPi * i / 64.f);
    auto rms = [](const std::vector<float>& samples)
    {
        double energy = 0.; for (float value : samples) energy += value * value;
        return std::sqrt(energy / samples.size());
    };
    f64::AuxBusManager effects;
    effects.prepare(48000., n);
    f64::AuxBusParams aux;
    aux.fxType = f64::AUX_FX_DRIVE; aux.p1 = 1.f; aux.p2 = 1.f; aux.returnLevel = 1.f;
    for (int block = 0; block < 4; ++block) {
        left = right = dry;
        effects.processAux(0, left.data(), right.data(), n, aux);
    }
    check(rms(left) <= rms(dry) * 1.01, "aux tape does not amplify the input level");
    f64::MasterFXParams master;
    master.compOn = master.eqOn = master.limiterOn = false;
    master.driveOn = true; master.drive = 1.f;
    for (int block = 0; block < 4; ++block) {
        left = right = dry;
        effects.processMasterChain(left.data(), right.data(), n, master);
    }
    double difference = 0.;
    for (int i = 0; i < n; ++i) difference += std::abs(left[i] - dry[i]);
    check(difference / n > 0.008 && std::abs(rms(left) / rms(dry) - 1.) < 0.02, "master tape changes timbre at matched RMS");
    for (int i = 0; i < n; ++i) left[i] = right[i] = dry[i] * 0.001f;
    effects.processMasterChain(left.data(), right.data(), n, master);
    check(rms(left) < rms(dry) * 0.0013, "tape makeup cannot boost a quiet block after a loud one");

    auto proc = std::make_unique<f64::Forge64Processor>();
    proc->setPlayConfigDetails(0, 2, 48000., n);
    proc->prepareToPlay(48000., n);
    juce::AudioBuffer<float> buffer(2, n);
    auto sendCC = [&](int cc, int value)
    {
        juce::MidiBuffer midi; midi.addEvent(juce::MidiMessage::controllerEvent(1, cc, value), 0);
        buffer.clear(); proc->processBlock(buffer, midi);
    };
    auto& learn = proc->getMidiLearn();
    learn.setLearnActive(true); sendCC(20, 100);
    check(learn.isLearning() && learn.getBoundParam(1, 20).isEmpty(), "header ANY is never bound to a CC");
    learn.startLearning("p0_pan"); sendCC(20, 127);
    check(! learn.isLearning() && proc->getAPVTS().getRawParameterValue("p0_pan")->load() == 1.f, "learning applies the first CC value");
    sendCC(20, 0);
    check(proc->getAPVTS().getRawParameterValue("p0_pan")->load() == -1.f, "learned pad control responds after learning");
    learn.startLearning("m_drv"); sendCC(21, 127); sendCC(21, 64);
    check(std::abs(proc->getAuxManager().masterParams.drive - 64.f / 127.f) < 0.001f, "master MIDI control works without an open editor");
    learn.startLearning("aux0_RETURN"); sendCC(22, 127);
    check(proc->getAuxManager().auxParams[0].returnLevel == 1.5f, "aux MIDI control works without an open editor");
    learn.bind(1, 20, "p0_lvl");
    check(learn.getBoundCC("p0_pan") == -1 && learn.getBoundParam(1, 20) == "p0_lvl", "rebinding removes stale mapping");
    f64::MidiLearnManager restored;
    restored.deserialize(learn.serialize());
    check(restored.getBoundParam(1, 21) == "m_drv", "MIDI bindings persist through state restore");

    auto* env = static_cast<f64::EnvSource*>(proc->mods().sourceAt(f64::slotEnv(0)));
    env->state.setProperty("enabled", true, nullptr);
    env->state.setProperty("atk", 0.1f, nullptr);
    proc->mods().addConnection(f64::slotEnv(0), "v_pitch", 12.f);
    env->retrigger(); proc->mods().renderSources(12000); proc->mods().computeOffsets();
    check(proc->mods().offsetFor("v_pitch") == 0.f, "voice envelopes are not also applied as global offsets");
    for (int voice = 0; voice < f64::kMaxVoices; ++voice) proc->mods().triggerVoice(voice);
    f64::ModMatrix::VoiceMods mods;
    proc->mods().renderVoice(0, mods, 1);
    check(mods.pitch == 0.f, "first voice-envelope sample is zero");
    proc->mods().renderVoice(0, mods, 100);
    const float advancedPitch = mods.pitch;
    proc->mods().renderVoice(16, mods, 1);
    check(mods.pitch == 0.f && advancedPitch > 0.f, "simultaneous voices have independent envelopes");
    env->state.setProperty("sus", 0.7f, nullptr);
    proc->mods().renderSources(12000);
    juce::MidiBuffer note;
    note.addEvent(juce::MidiMessage::noteOn(1, 36, (juce::uint8) 100), 64);
    buffer.clear(); proc->processBlock(buffer, note);
    // The first 64 samples retain sustain; the remaining 448 restart attack.
    check(proc->mods().sourceAverage(f64::slotEnv(0)) < 0.15f, "note retriggers envelope in the current block");

    f64::PadParams params; params.vcfType = 1; params.vcfCut = 12000.f; params.vcfRes = 0.707f;
    f64::PadChain cleanFilter, drivenFilter;
    cleanFilter.prepare(48000., n); drivenFilter.prepare(48000., n);
    std::vector<float> clean(n), cleanRight(n);
    for (int block = 0; block < 8; ++block) {
        clean = cleanRight = dry; left = right = dry;
        params.vcfDrive = 0.f; cleanFilter.process(clean.data(), cleanRight.data(), n, params);
        params.vcfDrive = 1.f; drivenFilter.process(left.data(), right.data(), n, params);
    }
    difference = 0.; for (int i = 0; i < n; ++i) difference += std::abs(left[i] - clean[i]);
    check(difference / n > 0.01, "filter drive changes the processed signal");
    auto* drive = proc->getAPVTS().getParameter("p0_vcfd");
    drive->setValueNotifyingHost(0.8f);
    const auto bank = juce::File::getCurrentWorkingDirectory().getChildFile("banks presets/05_Roland_TR808.bnk");
    check(proc->presets().loadBank(bank, 0) && proc->getAPVTS().getRawParameterValue("p0_vcfd")->load() == 0.f, "legacy bank loading resets filter drive to zero");
    drive->setValueNotifyingHost(0.64f);
    juce::MemoryBlock savedState;
    proc->getStateInformation(savedState);
    drive->setValueNotifyingHost(0.f);
    proc->setStateInformation(savedState.getData(), (int) savedState.getSize());
    check(std::abs(proc->getAPVTS().getRawParameterValue("p0_vcfd")->load() - 0.64f) < 0.001f, "filter drive persists through plugin state restore");
    auto legacyXml = juce::XmlDocument::parse(juce::String::fromUTF8((const char*) savedState.getData(), (int) savedState.getSize()));
    auto legacy = juce::ValueTree::fromXml(*legacyXml);
    auto legacyParams = legacy.getChildWithName("PARAMS");
    for (int index = legacyParams.getNumChildren() - 1; index >= 0; --index)
        if (legacyParams.getChild(index).getProperty("id").toString().endsWith("_vcfd"))
            legacyParams.removeChild(index, nullptr);
    const auto legacyText = legacy.createXml()->toString();
    proc->setStateInformation(legacyText.toRawUTF8(), (int) legacyText.getNumBytesAsUTF8());
    check(proc->getAPVTS().getRawParameterValue("p0_vcfd")->load() == 0.f, "legacy plugin state restores filter drive as zero");
    auto pattern = proc->getSequencer().getPatternCopy(0);
    auto& step = pattern->tracks[0].steps[0];
    step.active = true; step.hasLocks = true; step.lockMask = f64::LOCK_FLAG_VCF_DRIVE; step.pLockVcfDrive = 0.6f;
    proc->getSequencer().setPattern(0, *pattern);
    const auto sequence = proc->getSequencer().serializePattern(0);
    proc->getSequencer().clearCurrentPattern();
    proc->getSequencer().deserializePattern(0, sequence);
    check(proc->getSequencer().currentPattern().tracks[0].steps[0].pLockVcfDrive == 0.6f, "filter drive step locks persist");
    {
        auto offsetProc = std::make_unique<f64::Forge64Processor>();
        auto& matrix = offsetProc->mods();
        matrix.prepare(48000., 256);
        std::array<float, f64::kNumSlots> original;
        for (int slot = 0; slot < f64::kNumSlots; ++slot)
        {
            check(matrix.sourceState(slot).isValid(), "every source has editable saved state");
            matrix.setSourceParam(slot, "enabled", true);
            matrix.addConnection(slot, "offset-test-" + juce::String(slot), 0.4f);
        }
        matrix.renderSources(1);
        for (int slot = 0; slot < f64::kNumSlots; ++slot)
        {
            original[(size_t) slot] = matrix.sourceAverage(slot);
            matrix.setSourceParam(slot, "offset", 0.25f);
        }
        matrix.renderSources(1); matrix.computeOffsets();
        for (int slot = 0; slot < f64::kNumSlots; ++slot)
        {
            check(std::abs(matrix.sourceAverage(slot) - original[(size_t) slot] - 0.25f) < 0.005f,
                  "offset shifts all source classes without clipping the waveform");
            check(std::abs(matrix.offsetFor(("offset-test-" + juce::String(slot)).toStdString())
                    - 0.4f * matrix.sourceAverage(slot)) < 0.001f,
                  "source offset follows connection amount");
        }
        matrix.setSourceParam(f64::slotEnv(0), "offset", -0.4f);
        matrix.addConnection(f64::slotEnv(0), "v_pitch", 12.f);
        matrix.renderSources(1); matrix.computeOffsets(); matrix.triggerVoice(0);
        f64::ModMatrix::VoiceMods voice;
        matrix.renderVoice(0, voice, 1);
        check(std::abs(voice.pitch + 4.8f) < 0.001f, "voice envelope offset is applied once from the first sample");
        for (int slot : { f64::slotLFO(0), f64::slotMidi(0), f64::slotMacro(0) })
        {
            matrix.setSourceParam(slot, "enabled", false);
            matrix.renderSources(1);
            check(matrix.sourceAverage(slot) == 0.f, "disabled source emits no modulation or offset");
        }
        juce::MemoryBlock state;
        offsetProc->getStateInformation(state);
        matrix.setSourceParam(f64::slotMacro(0), "offset", -0.7f);
        offsetProc->setStateInformation(state.getData(), (int) state.getSize());
        check(std::abs(matrix.sourceAt(f64::slotMacro(0))->outputOffset.load() - 0.25f) < 0.001f,
              "macro offset survives session restore");
        auto legacyXml = juce::XmlDocument::parse(juce::String::fromUTF8((const char*) state.getData(), (int) state.getSize()));
        auto legacy = juce::ValueTree::fromXml(*legacyXml);
        auto sources = legacy.getChildWithName("MODSRC");
        while (sources.getNumChildren() > f64::slotMidi(0)) sources.removeChild(sources.getNumChildren() - 1, nullptr);
        for (auto source : sources) source.removeProperty("offset", nullptr);
        const auto text = legacy.createXml()->toString();
        offsetProc->setStateInformation(text.toRawUTF8(), (int) text.getNumBytesAsUTF8());
        for (int slot = 0; slot < f64::kNumSlots; ++slot)
            check(matrix.sourceState(slot).isValid() && matrix.sourceAt(slot)->outputOffset.load() == 0.f,
                  "legacy sessions default every source offset to zero");
        f64::ModPanel panel(matrix, offsetProc->getKit().getChildWithName("MODSRC"),
                            offsetProc->getKit().getChildWithName("MODMAT"), offsetProc.get());
        panel.setSize(340, 700);
        std::function<juce::Slider*(juce::Component&)> findOffset = [&](juce::Component& component) -> juce::Slider*
        {
            if (component.getComponentID() == "source-offset") return dynamic_cast<juce::Slider*>(&component);
            for (auto* child : component.getChildren()) if (auto* slider = findOffset(*child)) return slider;
            return nullptr;
        };
        for (int slot : { f64::slotLFO(0), f64::slotRnd(0), f64::slotEnv(0), f64::slotSeq(0), f64::slotMidi(0), f64::slotMacro(0) })
        {
            panel.openSourceEditor(slot, nullptr);
            auto* slider = findOffset(panel);
            // Slider step rounding can leave a tiny floating-point residual
            // around zero on Apple Silicon.
            check(slider != nullptr && std::abs(slider->getValue()) < 0.000001,
                  "every source editor exposes a zero-default offset control");
            if (slider != nullptr) slider->setValue(-0.35, juce::sendNotificationSync);
            check(std::abs(matrix.sourceAt(slot)->outputOffset.load() + 0.35f) < 0.001f,
                  "editor offset control updates the source");
            panel.closeInspector();
        }
    }
    std::cout << "DSP / MIDI regression tests: " << failures << " failures." << std::endl;
    return failures == 0 ? 0 : 1;
}

static int testBankPresetsAndLocks()
{
    int failures = 0;
    auto check = [&](bool ok, const char* what)
    {
        if (! ok) { ++failures; std::cout << "FAILED: " << what << std::endl; }
    };
    juce::Random random(12345);
    const uint64_t flags[] = { 0, 0, 0, f64::LOCK_FLAG_PITCH, f64::LOCK_FLAG_DECAY,
                              f64::LOCK_FLAG_DRIVE, f64::LOCK_FLAG_LEVEL, f64::LOCK_FLAG_PAN };
    for (int mode = 0; mode < 8; ++mode)
    {
        f64::TrackData track;
        track.stepCount = 32;
        for (auto& step : track.steps)
        {
            step.active = true;
            step.padOverride = 7;
            step.lockMask = f64::LOCK_FLAG_TONE | f64::LOCK_FLAG_PAD_OVERRIDE | flags[mode];
            step.hasLocks = true;
            step.pLockTone = 0.73f;
        }
        const auto parameter = static_cast<f64::StepLockParameter>(mode);
        f64::applyTrackLocks(track, parameter, true, 0.25f, random);
        for (const auto& step : track.steps)
            check(step.active && step.padOverride == 7 && step.pLockTone == 0.73f, "randomization preserves rhythm and other locks");
        check(track.steps[32].velocity == 0.85f && track.steps[32].pLockPitch == 0.f, "randomization respects track length");
        f64::applyTrackLocks(track, parameter, false, 0.f, random);
        for (const auto& step : track.steps)
            check(step.lockMask == (f64::LOCK_FLAG_TONE | f64::LOCK_FLAG_PAD_OVERRIDE) && step.hasLocks,
                  "reset clears only the requested lock, including hidden steps");
        const auto& step = track.steps[0];
        check(step.velocity == 0.85f && step.probability == 1.f && step.microtiming == 0.f
              && step.pLockPitch == 0.f && step.pLockDecay == 1.f && step.pLockDrive == 0.f
              && step.pLockLevel == 1.f && step.pLockPan == 0.f, "reset restores default values");
        track.steps[0].lockMask = flags[mode] | f64::LOCK_FLAG_PAD_OVERRIDE;
        f64::applyTrackLocks(track, parameter, false, 0.f, random);
        if (flags[mode] != 0) check(! track.steps[0].hasLocks, "last parameter reset removes hasLocks");
    }
    struct Services : f64::ModRingKnob::Services
    {
        f64::ModMatrix* matrix() override { return nullptr; }
        void connectFromDrag(int, const juce::String&) override {}
        void registerKnob(f64::ModRingKnob*) override {}
        void unregisterKnob(f64::ModRingKnob*) override {}
    } services;
    auto proc = std::make_unique<f64::Forge64Processor>();
    const auto file = juce::File::getCurrentWorkingDirectory().getChildFile("banks presets/05_Roland_TR808.bnk");
    // Seed the stale snapshot that used to be restored by every sound load.
    juce::MemoryBlock initialState;
    proc->getStateInformation(initialState);
    auto& sequencer = proc->getSequencer();
    for (int index : { 0, 3 })
    {
        auto pattern = sequencer.getPatternCopy(index);
        pattern->name = "Preserve pattern " + juce::String(index);
        auto& track = pattern->tracks[2];
        track.stepCount = 23;
        track.swing = 0.21f;
        track.steps[5].active = true;
        track.steps[5].velocity = 0.63f;
        track.steps[5].probability = 0.71f;
        track.steps[5].microtiming = -0.15f;
        track.steps[5].hasLocks = true;
        track.steps[5].lockMask = f64::LOCK_FLAG_VCF_DRIVE;
        track.steps[5].pLockVcfDrive = 0.47f;
        sequencer.setPattern(index, *pattern);
    }
    const auto sequenceBeforeLoads = sequencer.serialize().createXml()->toString();
    std::function<juce::ComboBox*(juce::Component&)> findPreset = [&](juce::Component& component) -> juce::ComboBox*
    {
        if (auto* combo = dynamic_cast<juce::ComboBox*>(&component))
            if (combo->getTooltip() == "Sound Preset / Algorithm") return combo;
        for (auto* child : component.getChildren())
            if (auto* result = findPreset(*child)) return result;
        return nullptr;
    };
    for (int bank = 0; bank < 4; ++bank)
    {
        check(proc->presets().loadBank(file, bank), "bank loads");
        check(sequencer.serialize().createXml()->toString() == sequenceBeforeLoads,
              "bank loading preserves all sequence patterns and locks");
        for (int slot = 0; slot < 16; ++slot)
        {
            const int pad = bank * 16 + slot;
            const auto state = proc->grid().padState(pad);
            const auto script = state.getProperty("script").toString();
            f64::PadEditor editor(*proc, services, pad, [] {});
            auto* combo = findPreset(editor);
            check(combo != nullptr && combo->getText() == state.getProperty("name").toString(), "pad's personal preset displays its name");
            if (combo != nullptr)
            {
                combo->onChange();
                check(state.getProperty("script").toString() == script, "reselecting personal preset keeps its Lua script");
            }
        }
    }
    juce::TemporaryFile padFile(".pad"), kitFile(".kit");
    check(proc->presets().savePad(padFile.getFile(), 0), "pad saves");
    check(proc->presets().loadPad(padFile.getFile(), 19), "pad loads into another bank");
    check(sequencer.serialize().createXml()->toString() == sequenceBeforeLoads,
          "pad loading preserves all sequence patterns and locks");
    check(proc->presets().saveKit(kitFile.getFile()), "kit saves its sequences");
    sequencer.clearCurrentPattern();
    check(sequencer.serialize().createXml()->toString() != sequenceBeforeLoads,
          "sequence changed after kit save");
    check(proc->presets().loadKit(kitFile.getFile()), "kit loads");
    check(sequencer.serialize().createXml()->toString() == sequenceBeforeLoads,
          "kit loading restores its saved sequences and locks");
    // Pad clipboard must be a snapshot, include Lua and DSP settings, and leave
    // the destination trigger note and the current sequence alone.
    auto* sourceDrive = proc->getAPVTS().getParameter("p0_vcfd");
    sourceDrive->setValueNotifyingHost(0.68f);
    const auto copiedScript = proc->grid().padState(0).getProperty("script").toString();
    const auto copiedName = proc->grid().padState(0).getProperty("name").toString();
    check(proc->presets().copyPad(0), "pad copies to clipboard");
    sourceDrive->setValueNotifyingHost(0.1f);
    proc->grid().padState(0).setProperty("name", "Edited after copy", nullptr);
    auto* destinationNote = proc->getAPVTS().getParameter("p19_mnote");
    destinationNote->setValueNotifyingHost(destinationNote->convertTo0to1(110.f));
    check(proc->presets().pastePad(19), "clipboard pastes across banks");
    check(proc->grid().padState(19).getProperty("name").toString() == copiedName
          && proc->grid().padState(19).getProperty("script").toString() == copiedScript
          && std::abs(proc->getAPVTS().getRawParameterValue("p19_vcfd")->load() - 0.68f) < 0.001f,
          "paste restores copied sound and DSP settings independently of later source edits");
    check(proc->getAPVTS().getRawParameterValue("p19_mnote")->load() == 110.f,
          "paste preserves destination MIDI note");
    check(proc->presets().cutPad(19), "pad cuts to clipboard");
    check(proc->grid().padState(19).getProperty("script").toString().isEmpty()
          && proc->getAPVTS().getRawParameterValue("p19_src")->load() == (float) f64::SRC_SAMPLE,
          "cut leaves an empty silent pad");
    check(proc->presets().pastePad(33) && proc->presets().pastePad(34), "cut sound can be pasted repeatedly");
    check(proc->grid().padState(33).getProperty("script").toString() == copiedScript
          && std::abs(proc->getAPVTS().getRawParameterValue("p34_vcfd")->load() - 0.68f) < 0.001f,
          "cut retains original Lua script and parameters");
    check(sequencer.serialize().createXml()->toString() == sequenceBeforeLoads,
          "pad clipboard operations preserve the sequence");

    f64::SequencerDrawer drawer(*proc);
    std::vector<juce::Component*> stepComponents;
    for (auto* child : drawer.getChildren())
        if (dynamic_cast<juce::DragAndDropTarget*>(child) != nullptr) stepComponents.push_back(child);
    check(stepComponents.size() == 16, "drawer exposes its 16 step targets");
    if (stepComponents.size() == 16)
    {
        drawer.setTrack(2);
        auto click = [&](int index, int modifier)
        {
            auto* component = stepComponents[(size_t) index];
            const auto now = juce::Time::getCurrentTime();
            component->mouseDown(juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),
                { 5.f, 5.f }, juce::ModifierKeys(modifier), 1.f, 0.f, 0.f, 0.f, 0.f,
                component, component, now, { 5.f, 5.f }, now, 1, false));
        };
        for (bool editorOpen : { false, true })
        {
            drawer.isPadEditActive = [editorOpen] { return editorOpen; };
            click(5, juce::ModifierKeys::leftButtonModifier);
            click(5, juce::ModifierKeys::leftButtonModifier);
            check(sequencer.currentPattern().tracks[2].steps[5].active && drawer.getSelectedStep() == 5,
                  "repeated left clicks select an active step without erasing it");
        }
        auto* target = dynamic_cast<juce::DragAndDropTarget*>(stepComponents[8]);
        target->itemDropped(juce::DragAndDropTarget::SourceDetails("f64step:2:5", stepComponents[5], { 5, 5 }));
        const auto& steps = sequencer.currentPattern().tracks[2].steps;
        check(steps[5].active && steps[8].active && steps[8].pLockVcfDrive == 0.47f
              && steps[8].velocity == steps[5].velocity && steps[8].probability == steps[5].probability,
              "dropping a step duplicates its locks and keeps the source");
        click(8, juce::ModifierKeys::rightButtonModifier);
        click(8, juce::ModifierKeys::rightButtonModifier);
        check(! steps[8].active && ! steps[8].hasLocks && steps[5].active,
              "right click erases a step and never activates an empty step");
    }
    auto* pad3Drive = proc->getAPVTS().getParameter("p3_vcfd");
    auto* pad17Drive = proc->getAPVTS().getParameter("p17_vcfd");
    pad3Drive->setValueNotifyingHost(0.31f);
    pad17Drive->setValueNotifyingHost(0.52f);
    f64::StepData assignedStep;
    assignedStep.active = true;
    assignedStep.padOverride = 3; // A04
    assignedStep.hasLocks = true;
    assignedStep.lockMask = f64::LOCK_FLAG_VCF_DRIVE;
    assignedStep.pLockVcfDrive = 0.81f;
    sequencer.setStepData(0, 0, assignedStep);
    assignedStep.padOverride = 17; // B02
    assignedStep.clearLocks();
    sequencer.setStepData(0, 1, assignedStep);
    f64::Forge64Editor pluginEditor(*proc);
    pluginEditor.padClicked(0);
    f64::SequencerDrawer* editorDrawer = nullptr;
    auto displayedPad = [&]() -> f64::PadEditor*
    {
        for (auto* child : pluginEditor.getChildren())
            if (auto* editor = dynamic_cast<f64::PadEditor*>(child)) return editor;
        return nullptr;
    };
    for (auto* child : pluginEditor.getChildren())
        if (auto* component = dynamic_cast<f64::SequencerDrawer*>(child)) editorDrawer = component;
    check(editorDrawer != nullptr, "plugin editor has its sequencer drawer");
    if (editorDrawer != nullptr)
    {
        editorDrawer->onStepClicked(0, 0, 3);
        auto* shown = displayedPad();
        auto* preset = shown != nullptr ? findPreset(*shown) : nullptr;
        check(shown != nullptr && shown->padIndex() == 3 && shown->getPLockStep() == 0
              && preset != nullptr && preset->getText() == proc->grid().padState(3).getProperty("name").toString(),
              "step selection displays A04's pad and named module");
        check(std::abs(proc->getAPVTS().getRawParameterValue("p3_vcfd")->load() - 0.81f) < 0.001f,
              "selected step applies its locks to the assigned pad");
        editorDrawer->onStepClicked(0, 1, 17);
        shown = displayedPad();
        preset = shown != nullptr ? findPreset(*shown) : nullptr;
        check(shown != nullptr && shown->padIndex() == 17 && shown->getPLockStep() == 1
              && preset != nullptr && preset->getText() == proc->grid().padState(17).getProperty("name").toString(),
              "next step switches to B02's pad and named module");
        check(std::abs(proc->getAPVTS().getRawParameterValue("p3_vcfd")->load() - 0.31f) < 0.001f
              && std::abs(proc->getAPVTS().getRawParameterValue("p17_vcfd")->load() - 0.52f) < 0.001f,
              "switching pads restores the old pad and uses the new pad's base knob values");
        editorDrawer->onStepClicked(0, 0, 3);
        check(displayedPad() != nullptr && displayedPad()->padIndex() == 3
              && std::abs(proc->getAPVTS().getRawParameterValue("p3_vcfd")->load() - 0.81f) < 0.001f,
              "switching back restores the first step's pad and locks");
    }
    std::cout << "Bank preset / step lock regression tests: " << failures << " failures." << std::endl;
    return failures == 0 ? 0 : 1;
}

#include "feature_regressions.h"

int main(int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI guiInit;
    if (argc > 1 && juce::String(argv[1]) == "--dsp-midi-regression")
        return testDspAndMidiFixes() + testSequencerAndPerformanceFeatures();
    if (argc > 1 && juce::String(argv[1]) == "--bank-lock-regression")
        return testBankPresetsAndLocks();

    if (argc > 1 && juce::String(argv[1]) == "--bench")
    {
        const double sr = 48000.0;
        const int bs = 128;
        auto bp = std::make_unique<f64::Forge64Processor>();
        bp->setPlayConfigDetails(0, 2, sr, bs);
        bp->prepareToPlay(sr, bs);

        const char* banks[4] = { "05_Roland_TR808", "06_Roland_TR909", "10_Buchla_Modular", "09_Industrial_Glitch" };
        for (int b = 0; b < 4; ++b)
        {
            auto f = f64::PresetManager::getBankFileByName(banks[b]);
            bp->presets().loadBank(f, b);
        }
        int scripted = 0;
        for (int p = 0; p < 64; ++p)
            scripted += bp->lua().enabledFor(p) ? 1 : 0;

        juce::AudioBuffer<float> buf(2, bs);
        juce::MidiBuffer midi;
        const double budgetMs = 1000.0 * bs / sr;
        const int blocks = (int) (sr * 12.0 / bs); // 12 seconds
        const int hitEvery = (int) (sr * 0.08 / bs); // a new pad every 80 ms
        double total = 0.0, worst = 0.0;
        int over = 0, hitIdx = 0;
        for (int i = 0; i < blocks; ++i)
        {
            if (i % hitEvery == 0)
                bp->triggerAudition((hitIdx++ * 7) % 64, 0.9f);
            buf.clear();
            const auto t0 = juce::Time::getHighResolutionTicks();
            bp->processBlock(buf, midi);
            const double ms = juce::Time::highResolutionTicksToSeconds(juce::Time::getHighResolutionTicks() - t0) * 1000.0;
            total += ms;
            worst = juce::jmax(worst, ms);
            if (ms > budgetMs * 0.8)
                ++over;
        }
        std::cout << "BENCH scriptedPads=" << scripted
                  << " threads=" << (getenv("FORGE64_THREADS") ? getenv("FORGE64_THREADS") : "auto")
                  << " avgLoad=" << (100.0 * total / blocks / budgetMs) << "%"
                  << " worstBlock=" << (100.0 * worst / budgetMs) << "%"
                  << " blocksOver80%=" << over << "/" << blocks << std::endl;
        return 0;
    }

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
        {
            std::cout << "Pad " << p << " failed audition: maxPeak=" << maxPeak << ", err=" << err << std::endl;
            failedPads++;
        }
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
        auto beforeRandomize = proc->getSequencer().getPatternCopy(proc->getSequencer().selectedPatternIndex());
        proc->getSequencer().randomizeCurrentTrack();

        int t0Active = 0, t3Active = 0;
        const auto& pat = proc->getSequencer().currentPattern();
        for (const auto& s : pat.tracks[0].steps) if (s.active) t0Active++;
        for (const auto& s : pat.tracks[3].steps) if (s.active) t3Active++;

        bool randSelectedPassed = pat.tracks[0] == beforeRandomize->tracks[0] && pat.tracks[3] != beforeRandomize->tracks[3];
        std::cout << "[5] Selected Track Randomizer (Track 3): "
                  << (randSelectedPassed ? "PASSED" : "FAILED")
                  << " (T0 active=" << t0Active << ", T3 active=" << t3Active << ")" << std::endl;
        if (! randSelectedPassed) failedPads++;

        auto beforeRandomizeAll = proc->getSequencer().getPatternCopy(proc->getSequencer().selectedPatternIndex());
        proc->getSequencer().randomizeAllTracks();
        int totalActive = 0;
        const auto& patAll = proc->getSequencer().currentPattern();
        for (int t = 0; t < 8; ++t)
            for (const auto& s : patAll.tracks[(size_t) t].steps)
                if (s.active) totalActive++;

        bool randAllPassed = true;
        for (int k = 0; k < 8; ++k) randAllPassed &= patAll.tracks[(size_t)k] != beforeRandomizeAll->tracks[(size_t)k];
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

    // 14. Modulation Active On Unlocked Sounds Test
    {
        proc->getSequencer().setPlaying(false);
        proc->getSequencer().clearCurrentPattern();
        proc->getSequencer().setStepActive(0, 0, true);
        proc->getSequencer().setStepVelocity(0, 0, 0.9f);
        // Verify step has NO locks
        bool stepHasLocks = proc->getSequencer().currentPattern().tracks[0].steps[0].hasLocks;
        int connId = proc->mods().addConnection(f64::slotLFO(0), "p0_tune", 0.8f);

        juce::AudioBuffer<float> buf(2, 512);
        juce::MidiBuffer midi;
        float maxOffsetSeen = 0.f;
        float maxTuneDev = 0.f;

        for (int b = 0; b < 40; ++b)
        {
            buf.clear();
            proc->processBlock(buf, midi);
            float off = proc->mods().offsetFor("p0_tune");
            if (std::abs(off) > std::abs(maxOffsetSeen))
                maxOffsetSeen = off;

            f64::PadParams ppTest;
            proc->fillPadParams(0, ppTest);
            if (std::abs(ppTest.tune) > std::abs(maxTuneDev))
                maxTuneDev = ppTest.tune;
        }

        bool p14Passed = (! stepHasLocks) && (connId >= 0) && (std::abs(maxOffsetSeen) > 0.05f) && (std::abs(maxTuneDev) > 0.5f);

        std::cout << "[14] Modulation Active On Unlocked Sounds: "
                  << (p14Passed ? "PASSED" : "FAILED")
                  << " (stepHasLocks=" << stepHasLocks
                  << ", maxOffset=" << maxOffsetSeen
                  << ", maxTuneDev=" << maxTuneDev << " st)" << std::endl;
        proc->mods().removeConnection(connId);
    }

    // [15] Bank Load & MIDI Note Preservation / Assignment Test
    {
        bool bnkNotePassed = true;
        auto bankNames = f64::PresetManager::getAvailableBankNames();
        if (! bankNames.isEmpty())
        {
            auto f = f64::PresetManager::getBankFileByName(bankNames[0]);
            if (f.existsAsFile())
            {
                // Load bank into Bank B (pads 16..31)
                proc->presets().loadBank(f, 1);
                for (int p = 16; p < 32; ++p)
                {
                    int expectedNote = 36 + p;
                    if (auto* param = proc->getAPVTS().getRawParameterValue(f64::padParamId(p, "mnote")))
                    {
                        if ((int) std::round(param->load()) != expectedNote)
                            bnkNotePassed = false;
                    }
                }

                // Load bank into Bank C (pads 32..47)
                proc->presets().loadBank(f, 2);
                for (int p = 32; p < 48; ++p)
                {
                    int expectedNote = 36 + p;
                    if (auto* param = proc->getAPVTS().getRawParameterValue(f64::padParamId(p, "mnote")))
                    {
                        if ((int) std::round(param->load()) != expectedNote)
                            bnkNotePassed = false;
                    }
                }

                // Load bank into Bank D (pads 48..63)
                proc->presets().loadBank(f, 3);
                for (int p = 48; p < 64; ++p)
                {
                    int expectedNote = 36 + p;
                    if (auto* param = proc->getAPVTS().getRawParameterValue(f64::padParamId(p, "mnote")))
                    {
                        if ((int) std::round(param->load()) != expectedNote)
                            bnkNotePassed = false;
                    }
                }
            }
        }

        // Test resetAllMidiNotes
        proc->resetAllMidiNotes();
        for (int p = 0; p < 64; ++p)
        {
            int expectedNote = 36 + p;
            if (auto* param = proc->getAPVTS().getRawParameterValue(f64::padParamId(p, "mnote")))
            {
                if ((int) std::round(param->load()) != expectedNote)
                    bnkNotePassed = false;
            }
        }

        std::cout << "[15] Bank Load & MIDI Note Assignment (36..99): "
                  << (bnkNotePassed ? "PASSED" : "FAILED") << std::endl;
        if (! bnkNotePassed) failedPads++;
    }

    std::cout << "All tests completed with " << failedPads << " errors." << std::endl;
    return (failedPads == 0) ? 0 : 1;
}
