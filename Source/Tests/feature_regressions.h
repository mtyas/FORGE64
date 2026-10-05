#pragma once
#include "../UI/PerformancePage.h"
#include "../UI/MixerFXPage.h"
#include "../UI/SequencerPage.h"
#include <algorithm>

static int testSequencerAndPerformanceFeatures()
{
    int failures = 0;
    auto check = [&](bool ok, const char* text) { if (!ok) { ++failures; std::cout << "FAILED: " << text << std::endl; } };
    // Every LFO/random shape must restart at the event sample, never early.
    for (int family = 0; family < 2; ++family)
    {
        for (int shape = 0; shape < (family == 0 ? 6 : 5); ++shape)
        {
            std::unique_ptr<f64::PadResetSource> timed, reference;
            if (family == 0)
            {
                auto a = std::make_unique<f64::LFOSource>(), b = std::make_unique<f64::LFOSource>();
                a->shape = b->shape = shape; a->rate = b->rate = 8.f;
                timed = std::move(a); reference = std::move(b);
            }
            else
            {
                auto a = std::make_unique<f64::RandomSource>(), b = std::make_unique<f64::RandomSource>();
                a->kind = b->kind = shape; a->sync = b->sync = false;
                a->p1 = b->p1 = 1.f;
                timed = std::move(a); reference = std::move(b);
            }
            timed->prepare(48000.,256); reference->prepare(48000.,256);
            timed->enabled = reference->enabled = true;
            timed->rnd.setSeed(123); reference->rnd.setSeed(123);
            std::vector<float> actual(256), expected(256);
            for (int block = 0; block < 20; ++block) { timed->render(actual.data(),256); reference->render(expected.data(),256); }
            timed->scheduleReset(64); timed->render(actual.data(),256);
            reference->render(expected.data(),64);
            check(std::equal(actual.begin(),actual.begin()+64,expected.begin()), "pad reset preserves modulation before the event sample");
            reference->retrigger(); reference->render(expected.data(),192);
            check(std::equal(actual.begin()+64,actual.end(),expected.begin()), "every LFO/random shape restarts exactly at the event sample");
        }
    }
    auto triggerProc = std::make_unique<f64::Forge64Processor>();
    triggerProc->setPlayConfigDetails(0,2,48000.,256); triggerProc->prepareToPlay(48000.,256);
    auto& triggerMatrix = triggerProc->mods();
    const int resetSlot = f64::slotLFO(0);
    triggerMatrix.setSourceParam(resetSlot,"enabled",true);
    triggerMatrix.setSourceParam(resetSlot,"shape",2);
    triggerMatrix.setSourceParam(resetSlot,"rate",10.f);
    triggerMatrix.setSourceParam(resetSlot,"resetOnPad",true);
    triggerMatrix.setSourceParam(resetSlot,"trigPad",5);
    juce::AudioBuffer<float> triggerAudio(2,256); juce::MidiBuffer triggerMidi;
    for (int block = 0; block < 4; ++block) triggerProc->processBlock(triggerAudio,triggerMidi);
    const auto triggerBefore = triggerProc->getPadTriggerSerial();
    triggerProc->triggerAudition(5); triggerProc->processBlock(triggerAudio,triggerMidi);
    check(triggerProc->getPadTriggerSerial() > triggerBefore, "pad learning detects a new hit even when its pad number repeats");
    const float restarted = triggerMatrix.sourceAverage(resetSlot);
    check(restarted < -.94f, "mouse pad hits reset the selected LFO");
    triggerProc->triggerAudition(6); triggerProc->processBlock(triggerAudio,triggerMidi);
    check(triggerMatrix.sourceAverage(resetSlot) > restarted + .08f, "other pads do not reset a specifically assigned LFO");
    triggerMidi.addEvent(juce::MidiMessage::noteOn(1,41,.8f),0);
    triggerProc->processBlock(triggerAudio,triggerMidi); triggerMidi.clear();
    check(std::abs(triggerMatrix.sourceAverage(resetSlot)-restarted) < .0001f, "MIDI pad hits reset the LFO");
    triggerProc->getSequencer().setStepActiveWithPad(0,0,true,5); triggerProc->getSequencer().setPlaying(true);
    triggerProc->processBlock(triggerAudio,triggerMidi);
    check(std::abs(triggerMatrix.sourceAverage(resetSlot)-restarted) < .0001f, "sequencer pad hits reset the LFO");
    triggerProc->getSequencer().setPlaying(false);
    triggerMatrix.setSourceParam(resetSlot,"resetOnPad",false);
    triggerProc->triggerAudition(5); triggerProc->processBlock(triggerAudio,triggerMidi);
    check(triggerMatrix.sourceAverage(resetSlot) > restarted + .08f, "disabled pad-reset option leaves modulation free running");
    triggerMatrix.setSourceParam(resetSlot,"resetOnPad",true); triggerMatrix.setSourceParam(resetSlot,"trigPad",-1);
    triggerProc->triggerAudition(6); triggerProc->processBlock(triggerAudio,triggerMidi);
    check(std::abs(triggerMatrix.sourceAverage(resetSlot)-restarted) < .0001f, "all-pads reset mode accepts another pad");
    triggerMatrix.setSourceParam(f64::slotRnd(0),"resetOnPad",true);
    triggerMatrix.setSourceParam(f64::slotRnd(0),"trigPad",12);
    juce::MemoryBlock triggerState; triggerProc->getStateInformation(triggerState);
    triggerMatrix.setSourceParam(resetSlot,"resetOnPad",false);
    triggerMatrix.setSourceParam(f64::slotRnd(0),"trigPad",-1);
    triggerProc->setStateInformation(triggerState.getData(),(int)triggerState.getSize());
    check(static_cast<f64::LFOSource*>(triggerMatrix.sourceAt(resetSlot))->resetOnPad.load()
          && static_cast<f64::RandomSource*>(triggerMatrix.sourceAt(f64::slotRnd(0)))->triggerPad.load() == 12,
          "LFO and random pad-reset settings survive project state restore");
    auto oldLfoState = f64::LFOSource::makeDefault(); oldLfoState.removeProperty("resetOnPad",nullptr);
    f64::LFOSource legacyLfo; legacyLfo.state = oldLfoState; legacyLfo.syncFromState();
    check(!legacyLfo.resetOnPad.load(), "older LFO states remain free running");
    triggerProc.reset();

    juce::UndoManager undo;
    auto sequence = std::make_unique<f64::StepSequencer>();
    auto& seq = *sequence;
    seq.bindUndoManager(&undo);
    seq.setStepActiveWithPad(0, 0, true, 19);
    seq.beginEditGesture();
    seq.setStepVelocity(0, 0, .3f);
    seq.setStepVelocity(0, 0, .4f);
    seq.setStepRatchet(0, 0, 3);
    seq.endEditGesture();
    check(undo.undo() && seq.currentPattern().tracks[0].steps[0].ratchet == 1 && seq.currentPattern().tracks[0].steps[0].velocity == .85f,
          "undo restores whole drag gesture");
    check(undo.redo() && seq.currentPattern().tracks[0].steps[0].ratchet == 3, "redo restores ratchet and velocity");
    seq.setSelectedPattern(4);
    undo.undo();
    check(seq.selectedPatternIndex() == 4 && seq.pattern(0).tracks[0].steps[0].ratchet == 1, "undo does not navigate patterns");
    seq.setSelectedPattern(0);
    seq.setStepRatchet(0, 0, 8);
    seq.setTrackLength(0, 31);
    seq.setTrackSpeed(0, 1.5f);
    seq.setTrackSwing(0, .2f);
    seq.copyTrack(0);
    seq.setSelectedPattern(1);
    seq.pasteTrack(3);
    const auto& pasted = seq.currentPattern().tracks[3];
    check(pasted.steps[0].ratchet == 8 && pasted.steps[0].padOverride == 19 && pasted.stepCount == 31 && pasted.speedMultiplier == 1.5f && pasted.swing == .2f,
          "track clipboard copies all steps and track settings across patterns");
    check(!seq.currentPattern().tracks[2].steps[0].active, "track paste leaves other tracks intact");
    undo.undo();
    check(!seq.currentPattern().tracks[3].steps[0].active && seq.currentPattern().tracks[3].stepCount == 16, "track paste undo");
    seq.setSelectedPattern(0);
    seq.setTrackSpeed(0, 1.f); seq.setTrackSwing(0, 0.f);
    seq.prepare(48000.); seq.setPlaying(true);
    std::vector<f64::StepSequencer::TriggerEvent> triggers;
    std::vector<int> positions;
    for (int offset = 0; offset < 6000; offset += 128)
    {
        const int length = std::min(128, 6000 - offset);
        triggers.clear(); seq.process(length, 120., false, triggers);
        for (const auto& event : triggers) if (event.pad == 19) positions.push_back(offset + event.pos);
    }
    check(positions == std::vector<int>({0,750,1500,2250,3000,3750,4500,5250}), "eight ratchets are sample aligned across block boundaries");
    auto monitor = std::make_unique<f64::StepSequencer>();
    monitor->prepare(48000.);
    monitor->setStepActiveWithPad(2,0,true,11);
    monitor->startPadRecording(2,0);
    triggers.clear(); monitor->process(256,120.,false,triggers);
    check(std::any_of(triggers.begin(),triggers.end(),[](const auto& event) { return event.pad == 11; }),
          "existing track steps remain audible during overdub");
    monitor->setPlaying(false); monitor->setTrackMute(2,true); monitor->startPadRecording(2,0);
    triggers.clear(); monitor->process(256,120.,false,triggers);
    check(triggers.empty(), "recording still respects an explicitly muted track");
    monitor->stopPadRecording();

    auto state = seq.serialize();
    auto restored = std::make_unique<f64::StepSequencer>(); restored->deserialize(state);
    check(restored->pattern(0).tracks[0].steps[0].ratchet == 8, "ratchet state round trip");
    seq.addSongBlock(3, 2);
    seq.setSongSequence({{3, 5}});
    undo.undo();
    check(seq.getSongSequence().size() == 1 && seq.getSongSequence()[0].repeats == 2, "song edit undo restores repeats");
    undo.undo();
    check(seq.getSongSequence().empty(), "song block addition undo");
    juce::Random random(12);
    auto track = std::make_unique<f64::TrackData>(seq.currentPattern().tracks[0]);
    f64::applyTrackLocks(*track, f64::StepLockParameter::Ratchet, true, 1.f, random);
    bool inRange = true;
    for (int k = 0; k < track->stepCount; ++k) inRange &= track->steps[(size_t)k].ratchet >= 1 && track->steps[(size_t)k].ratchet <= 8;
    check(inRange, "random ratchets stay in range");
    f64::applyTrackLocks(*track, f64::StepLockParameter::Ratchet, false, 0.f, random);
    check(track->steps[0].ratchet == 1, "ratchet reset returns to one trigger");

    auto looper = std::make_unique<f64::XYMotionLooper>();
    looper->start(0.f, 1.f); looper->append(1.f, 0.f); looper->finish();
    looper->process(800, 48000.); looper->process(800, 48000.);
    check(std::abs(looper->x.load() - .5f) < .001f, "XY playback interpolates movement");
    looper->process(1600, 48000.); looper->process(1, 48000.);
    check(std::abs(looper->x.load()) < .001f, "XY playback loops on audio clock");
    looper->speed.store(2.f); looper->finish();
    looper->process(800, 48000.); looper->process(1, 48000.);
    check(std::abs(looper->x.load() - 1.f) < .001f, "XY playback speed doubles");
    auto loopCopy = std::make_unique<f64::XYMotionLooper>(); loopCopy->deserialize(looper->serialize());
    check(loopCopy->size() == 2 && loopCopy->speed.load() == 2.f && loopCopy->playing.load(), "XY loop state round trip");
    loopCopy->clear(); check(!loopCopy->playing.load() && loopCopy->size() == 0, "XY clear stops playback");

    auto effects = std::make_unique<f64::AuxBusManager>(); effects->prepare(48000., 256); effects->setBpm(120.);
    f64::AuxBusParams delay;
    delay.fxType = f64::AUX_FX_DELAY; delay.p1 = 5.5f / 12.f; delay.p2 = 0.f; delay.p4 = 1.f; delay.returnLevel = 1.f;
    auto echoes = [&](int mode)
    {
        effects->reset(); delay.delayMode = mode;
        std::array<int,2> first {-1,-1};
        for (int offset = 0; offset < 30000; offset += 256)
        {
            std::vector<float> left(256),right(256);
            if (offset == 0) left[0] = right[0] = .5f;
            effects->processAux(0,left.data(),right.data(),256,delay);
            for (int k = 0; k < 256; ++k) {
                if (first[0] < 0 && std::abs(left[(size_t)k]) > .001f) first[0] = offset+k;
                if (first[1] < 0 && std::abs(right[(size_t)k]) > .001f) first[1] = offset+k;
                check(std::isfinite(left[(size_t)k]) && std::isfinite(right[(size_t)k]), "delay output is finite");
            }
        }
        return first;
    };
    check(echoes(0) == std::array<int,2>{12000,16000}, "synced delay retains 3:4 stereo ratio");
    check(echoes(4) == std::array<int,2>{12000,24000}, "synced delay 1:2 ratio");
    delay.p2 = .5f;
    check(echoes(1) == std::array<int,2>{12000,24000}, "ping-pong alternates mono input between channels");
    delay.p2 = 0.f;
    const auto multi = echoes(2);
    check(multi[0] == 6000 && multi[1] == 3000, "multitap has independent early stereo taps");
    effects->masterParams.eqFrequency = {120.f, 700.f, 3100.f, 14000.f};
    effects->masterParams.eqQ = {2.f,3.f,4.f,5.f}; effects->auxParams[0].delayMode = 6;
    effects->masterParams.driveColour = 3; effects->masterParams.eqPreComp = true;
    auto fxCopy = std::make_unique<f64::AuxBusManager>(); fxCopy->deserialize(effects->serialize());
    check(fxCopy->masterParams.eqFrequency == effects->masterParams.eqFrequency && fxCopy->masterParams.eqQ == effects->masterParams.eqQ && fxCopy->auxParams[0].delayMode == 6 && fxCopy->masterParams.driveColour == 3 && fxCopy->masterParams.eqPreComp,
          "parametric EQ and delay modes survive state round trip");
    f64::MasterFXParams eq;
    eq.compOn = eq.driveOn = eq.limiterOn = false;
    eq.eqLowMidGain = 12.f; eq.eqFrequency[1] = 1000.f; eq.eqQ[1] = 8.f;
    auto measure = [&](float frequency)
    {
        effects->prepare(48000.,256);
        double energy = 0.;
        for (int block = 0; block < 100; ++block)
        {
            std::vector<float> left(256), right(256);
            for (int k = 0; k < 256; ++k) left[(size_t)k] = right[(size_t)k] = .01f * std::sin(juce::MathConstants<double>::twoPi * frequency * (block*256+k) / 48000.);
            effects->processMasterChain(left.data(),right.data(),256,eq);
            if (block >= 80) for (float value : left) energy += value * value;
        }
        return std::sqrt(energy / (20 * 256));
    };
    const double center = measure(1000.f), offCenter = measure(2000.f);
    check(center > offCenter * 3., "master bell peaks at selected frequency");
    const double narrow = measure(1200.f); eq.eqQ[1] = .7f;
    check(measure(1200.f) > narrow * 2., "master Q controls bandwidth");
    eq.eqFrequency[1] = 2000.f;
    check(measure(2000.f) > center * .95, "master frequency moves the EQ peak");
    auto proc = std::make_unique<f64::Forge64Processor>();
    proc->setPlayConfigDetails(0,2,48000.,256); proc->prepareToPlay(48000.,256);
    proc->getXYLooper(0).start(.2f,.8f); proc->getXYLooper(0).append(.6f,.4f); proc->getXYLooper(0).finish();
    juce::AudioBuffer<float> buffer(2,256); juce::MidiBuffer midi; proc->processBlock(buffer,midi);
    check(std::abs(proc->mods().sourceAverage(f64::slotMacro(0)) - .2f) < .002f, "XY loops drive macros without editor");
    proc->getUndoManager().clearUndoHistory();
    proc->getSequencer().setStepActive(0,0,true);
    for (int k = 0; k < 30; ++k) proc->processBlock(buffer,midi);
    proc->getAPVTS().copyState(); // flush pending parameter updates
    check(proc->getUndoManager().undo() && !proc->getSequencer().currentPattern().tracks[0].steps[0].active,
          "XY playback does not replace a sequencer edit in undo history");
    juce::MemoryBlock project; proc->getStateInformation(project);
    proc->getXYLooper(0).clear(); proc->setStateInformation(project.getData(),(int)project.getSize());
    check(proc->getXYLooper(0).size() == 2 && proc->getXYLooper(0).playing.load(), "project state restores XY loops");
    proc->getMidiLearn().startLearning("m_eqf0");
    midi.addEvent(juce::MidiMessage::controllerEvent(1,23,127),0); proc->processBlock(buffer,midi); midi.clear();
    check(std::abs(proc->getAuxManager().masterParams.eqFrequency[0] - 20000.f) < 1.f, "frequency MIDI learn uses knob range");
    proc->getMidiLearn().startLearning("m_eqq0");
    midi.addEvent(juce::MidiMessage::controllerEvent(1,24,127),0); proc->processBlock(buffer,midi); midi.clear();
    check(proc->getAuxManager().masterParams.eqQ[0] == 12.f, "Q MIDI learn uses knob range");
    auto& live = proc->getSequencer();
    live.setPlaying(false);
    live.setStepActiveWithPad(2,4,true,11);
    live.startPadRecording(2,0);
    check(live.currentPattern().tracks[2].steps[4].padOverride == 11, "pad overdub preserves existing steps");
    proc->triggerAudition(5,.6f,true);
    proc->triggerAudition(8,.9f,false);
    proc->processBlock(buffer,midi);
    check(live.currentPattern().tracks[2].steps[0].padOverride == 5, "mouse pad recording excludes editor audition previews");
    for (int k = 0; k < 23; ++k) proc->processBlock(buffer,midi);
    midi.addEvent(juce::MidiMessage::noteOn(1,43,.7f),0); proc->processBlock(buffer,midi); midi.clear();
    check(live.currentPattern().tracks[2].steps[1].padOverride == 7, "MIDI pad recording aligns to the track clock");
    live.recordPadHit(9,.4f,0,120.);
    check(live.currentPattern().tracks[2].steps[1].padOverride == 9, "monophonic recording replaces the hit in the same step");
    live.setStepActiveWithPad(4,5,true,12);
    live.stopPadRecording();
    proc->getUndoManager().undo();
    check(live.currentPattern().tracks[2].steps[4].padOverride == 11 && !live.currentPattern().tracks[2].steps[0].active
        && live.currentPattern().tracks[4].steps[5].active, "one recording undo restores original track and preserves other track edits");
    live.setPlaying(false);

    const int melodicSlot = f64::slotSeq(0);
    auto* melody = static_cast<f64::SeqSource*>(proc->mods().sourceAt(melodicSlot));
    const int pitchConnection = proc->mods().addConnection(melodicSlot,"p0_tune",.13f);
    const int voiceConnection = proc->mods().addConnection(melodicSlot,"v_pitch",.01f);
    proc->mods().connectionById(pitchConnection).setProperty("curve",1.f,nullptr);
    melody->enabled = true; melody->quantize = true; melody->uni = false;
    melody->gate = 1.f; melody->slew = 1.f; melody->outputOffset = 0.f;
    for (int octaves = 1; octaves <= 4; ++octaves)
    {
        melody->octaves = octaves;
        for (int k = 0; k < 32; ++k) melody->steps[(size_t)k] = 7.f / (12.f * octaves);
        melody->retrigger(); proc->mods().renderSources(256); proc->mods().computeOffsets();
        check(std::abs(proc->mods().offsetFor("p0_tune") * 48.f - 7.f) < .002f, "melodic pad pitch is seven semitones regardless of amount, curve, slew, polarity or octave range");
        check(std::abs(proc->mods().offsetFor("v_pitch") - 7.f) < .002f, "melodic voice pitch uses semitone units");
    }
    melody->quantize = false; melody->uni = true;
    proc->mods().connectionById(pitchConnection).setProperty("curve",0.f,nullptr);
    proc->mods().renderSources(256); proc->mods().computeOffsets();
    check(proc->mods().effectiveConnectionAmount(melodicSlot,"p0_tune",.13f) == .13f, "disabling quantization restores freely adjustable amount");
    proc->mods().removeConnection(pitchConnection); proc->mods().removeConnection(voiceConnection);

    // Gradual gain mapping and distinct mastering colour curves at matched level.
    std::vector<float> colourInput(512), colourLeft(512), colourRight(512);
    for (int k = 0; k < 512; ++k) colourInput[(size_t)k] = .08f * std::sin(juce::MathConstants<float>::twoPi * k / 64.f);
    auto distortion = [&](float amount, int colour)
    {
        colourLeft = colourRight = colourInput;
        float makeup = 1.f;
        for (int k = 0; k < 4; ++k) { colourLeft = colourRight = colourInput; f64::saturateStereo(colourLeft.data(),colourRight.data(),512,amount,256.f,makeup,colour); }
        double difference = 0.; for (int k = 0; k < 512; ++k) difference += std::abs(colourLeft[(size_t)k] - colourInput[(size_t)k]);
        return difference / 512.;
    };
    check(distortion(.25f,0) < .001 && distortion(.5f,0) < distortion(1.f,0), "master drive builds gradually through the range");
    distortion(.7f,0); const auto tapeCurve = colourLeft;
    for (int colour = 1; colour < 4; ++colour)
    {
        distortion(.7f,colour);
        double difference = 0.; for (int k = 0; k < 512; ++k) difference += std::abs(colourLeft[(size_t)k] - tapeCurve[(size_t)k]);
        check(difference / 512. > .0001, "master colour has its own saturation curve");
    }

    // Visual values use the same normalized mapping as the actual DSP.
    proc->getAuxManager().masterParams.eqFrequency[0] = 1000.f;
    const int masterMod = proc->mods().addConnection(f64::slotMacro(0), "m_eqf0", .2f);
    const int padMod = proc->mods().addConnection(f64::slotMacro(0), "p0_eqmf", .2f);
    proc->mods().renderSources(256); proc->mods().computeOffsets();
    check(proc->modulatedMasterParams().eqFrequency[0] > 1000.f &&
          std::abs(proc->modulatedMasterParams().eqFrequency[0] - proc->modulatedMasterParams(false).eqFrequency[0]) < .01f,
          "master graph and audio modulation use identical frequency mapping");
    auto* eqParameter = proc->getAPVTS().getParameter("p0_eqmf");
    const float expected = eqParameter->convertFrom0to1(juce::jlimit(0.f,1.f,eqParameter->getValue()+proc->mods().offsetFor("p0_eqmf")));
    check(std::abs(proc->displayPadValue(0,"eqmf",0.f)-expected) < .01f, "pad graphs follow actual normalized modulation");
    proc->mods().removeConnection(masterMod); proc->mods().removeConnection(padMod);
    auto compressionForOrder = [&](bool pre)
    {
        effects->prepare(48000.,256);
        f64::MasterFXParams params;
        params.eqPreComp = pre; params.compOn = true; params.driveOn = params.limiterOn = false;
        params.compThresh = -24.f; params.compRatio = 8.f; params.compAtk = .1f;
        params.eqLowMidGain = 12.f; params.eqFrequency[1] = 1000.f;
        for (int block = 0; block < 40; ++block)
        {
            std::vector<float> left(256),right(256);
            for (int k = 0; k < 256; ++k) left[(size_t)k] = right[(size_t)k] = .08f * std::sin(juce::MathConstants<float>::twoPi * (block*256+k) / 48.f);
            effects->processMasterChain(left.data(),right.data(),256,params);
        }
        return effects->getMasterCompGR();
    };
    const float postGR = compressionForOrder(false), preGR = compressionForOrder(true);
    check(preGR > postGR + 6.f, "pre-compressor EQ affects compression detector while post EQ does not");

    // Render actual product pages for layout review.
    auto editor = std::make_unique<f64::Forge64Editor>(*proc);
    juce::TextButton* headerRecord = nullptr;
    for (auto* child : editor->getChildren()) if (child->getComponentID() == "sequencerRecord") headerRecord = dynamic_cast<juce::TextButton*>(child);
    check(headerRecord && headerRecord->getWidth() == 26 && headerRecord->getHeight() == 26 && headerRecord->getX() == 252,
          "square header record button follows the bank buttons");
    if (headerRecord)
    {
        headerRecord->onClick(); check(live.isPadRecording() && live.currentPattern().tracks[2].steps[4].active, "header starts overdub without clearing existing steps");
        juce::Label* status = nullptr;
        for (auto* child : editor->getChildren()) if (child->getComponentID() == "recordingStatus") status = dynamic_cast<juce::Label*>(child);
        check(status && status->isVisible() && status->getText().contains("TRACK 3"), "recording has a visible track banner");
        juce::Component* frame = nullptr;
        for (auto* child : editor->getChildren()) if (child->getComponentID() == "recordingPadFrame") frame = child;
        check(frame && frame->isVisible() && !frame->hitTest(0,0), "recording pad frame is visible without blocking input");
        auto recordingImage = editor->createComponentSnapshot(editor->getLocalBounds());
        auto recordingFile = juce::File::getCurrentWorkingDirectory().getChildFile("build_win/feature-previews/recording.png");
        recordingFile.getParentDirectory().createDirectory();
        juce::FileOutputStream recordingStream(recordingFile); recordingStream.setPosition(0); recordingStream.truncate();
        juce::PNGImageFormat().writeImageToStream(recordingImage,recordingStream);
        headerRecord->onClick(); check(!live.isPadRecording(), "same header button stops overdub");
        check(status && !status->isVisible(), "recording banner hides when the take stops");
    }

    f64::ModPanel* resetPanel = nullptr;
    for (auto* child : editor->getChildren()) if (auto* panel = dynamic_cast<f64::ModPanel*>(child)) resetPanel = panel;
    if (resetPanel)
    {
        for (const int sourceSlot : {f64::slotLFO(0),f64::slotRnd(0)})
        {
            resetPanel->openSourceEditor(sourceSlot,nullptr);
            std::vector<juce::Component*> pending {resetPanel};
            juce::ToggleButton* resetToggle = nullptr;
            while (!pending.empty())
            {
                auto* component = pending.back(); pending.pop_back();
                if (auto* toggle = dynamic_cast<juce::ToggleButton*>(component); toggle && toggle->getButtonText() == "Reset on pad trigger") resetToggle = toggle;
                for (auto* child : component->getChildren()) pending.push_back(child);
            }
            check(resetToggle != nullptr,"LFO/random editor offers pad reset switch");
            if (resetToggle)
            {
                resetToggle->setToggleState(true,juce::dontSendNotification); resetToggle->onClick();
                check(static_cast<f64::PadResetSource*>(proc->mods().sourceAt(sourceSlot))->resetOnPad.load(), "reset editor switch updates the audio source");
                const auto previewFile = juce::File::getCurrentWorkingDirectory().getChildFile(sourceSlot == f64::slotLFO(0) ? "build_win/feature-previews/lfo-reset.png" : "build_win/feature-previews/random-reset.png");
                previewFile.getParentDirectory().createDirectory();
                juce::FileOutputStream preview(previewFile); preview.setPosition(0); preview.truncate();
                juce::PNGImageFormat().writeImageToStream(resetPanel->createComponentSnapshot(resetPanel->getLocalBounds()),preview);
                resetToggle->setToggleState(false,juce::dontSendNotification); resetToggle->onClick();
            }
        }
        resetPanel->closeInspector();
    }
    f64::PerformancePage performance(*proc,*editor); performance.setSize(850,540);
    f64::MixerFXPage mixer(*proc,*editor); mixer.setSize(900,840);
    f64::SequencerPage page(*proc); page.setSize(850,600);
    auto event = [](juce::Component& component, juce::Point<float> position)
    {
        const auto now = juce::Time::getCurrentTime();
        return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),position,
            juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier),1.f,0.f,0.f,0.f,0.f,&component,&component,now,position,now,1,false);
    };
    f64::MasterEQGraphView graph(*proc); graph.setSize(400,150);
    proc->getAuxManager().masterParams.eqFrequency[0] = 80.f;
    proc->getAuxManager().masterParams.eqLowGain = 0.f;
    const float firstNode = 8.f + std::log10(80.f / 20.f) / 3.f * 384.f;
    graph.mouseDown(event(graph,{firstNode,75.f}));
    graph.mouseDrag(event(graph,{200.f,45.f})); graph.mouseUp(event(graph,{200.f,45.f}));
    check(std::abs(proc->getAuxManager().masterParams.eqFrequency[0] - 632.455f) < 1.f && proc->getAuxManager().masterParams.eqLowGain > 0.f,
          "EQ graph drag changes frequency and gain");
    f64::XYPadComponent xy(*proc,"TEST",0,1); xy.setSize(420,360);
    juce::TextButton* arm = nullptr;
    for (auto* child : xy.getChildren()) if (auto* button = dynamic_cast<juce::TextButton*>(child); button && button->getButtonText() == "RECORD") arm = button;
    check(arm != nullptr,"XY gesture record arm exists");
    if (arm)
    {
        arm->setToggleState(true,juce::dontSendNotification);
        xy.mouseDown(event(xy,{210.f,230.f}));
        check(proc->getXYLooper(0).recording.load(), "XY recording starts on mouse press");
        xy.mouseDrag(event(xy,{320.f,170.f}));
        xy.mouseUp(event(xy,{320.f,170.f}));
        check(!proc->getXYLooper(0).recording.load() && proc->getXYLooper(0).playing.load(), "XY release finishes recording and starts looping");
    }
    auto folder = juce::File::getCurrentWorkingDirectory().getChildFile("build_win/feature-previews"); folder.createDirectory();
    for (auto entry : {std::pair<juce::Component*,const char*>{&performance,"performance"},{&mixer,"mixer"},{&page,"sequencer"},{editor.get(),"editor"}})
    {
        auto image = entry.first->createComponentSnapshot(entry.first->getLocalBounds());
        juce::FileOutputStream stream(folder.getChildFile(juce::String(entry.second)+".png"));
        stream.setPosition(0); stream.truncate();
        juce::PNGImageFormat().writeImageToStream(image,stream);
    }
    std::cout << "Sequencer / performance feature tests: " << failures << " failures." << std::endl;
    return failures ? 1 : 0;
}
