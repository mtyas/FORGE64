#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Scripting/ScriptPresetManager.h"
#include "Presets/SequencePresetManager.h"
#include <cmath>

namespace f64 {

static_assert(kNumPadParams == 45, "PadParams field mapping below must match kPadParams order");

// ---------------------------------------------------------------------------
// Buses: main stereo + 15 additional stereo pairs (16 total), each pad
// assignable to any of them.
// ---------------------------------------------------------------------------
juce::AudioProcessor::BusesProperties Forge64Processor::makeBuses()
{
    BusesProperties b;
    b = b.withOutput("Main", juce::AudioChannelSet::stereo(), true);
    for (int i = 1; i < kNumBuses; ++i)
        b = b.withOutput("Out " + juce::String(i + 1), juce::AudioChannelSet::stereo(), false);
    return b;
}

// ---------------------------------------------------------------------------
// Parameters: globals + 44 per pad x 64 pads.
// ---------------------------------------------------------------------------
juce::AudioProcessorValueTreeState::ParameterLayout Forge64Processor::makeParams()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    auto addF = [&params](juce::StringRef id, juce::StringRef name,
                          juce::NormalisableRange<float> range, float def)
    {
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { id, 1 }, juce::String(name), range, def));
    };
    auto addI = [&params](juce::StringRef id, juce::StringRef name, int mn, int mx, int def)
    {
        params.push_back(std::make_unique<juce::AudioParameterInt>(
            juce::ParameterID { id, 1 }, juce::String(name), mn, mx, def));
    };

    addF("master",  "Master Level",    { 0.f, 1.f, 0.001f },              0.8f);
    addF("revsize", "Reverb Size",     { 0.f, 1.f, 0.001f },              0.65f);
    addF("revdamp", "Reverb Damp",     { 0.f, 1.f, 0.001f },              0.5f);
    addF("dlytime", "Delay Time",      { 5.f, 1500.f, 0.1f, 0.25f },      375.f);
    addF("dlyfb",   "Delay Feedback",  { 0.f, 0.92f, 0.001f },            0.35f);
    for (int i = 0; i < kNumMacros; ++i)
        addF("m" + juce::String(i), "Macro " + juce::String(i + 1), { 0.f, 1.f, 0.001f }, 0.5f);

    for (int p = 0; p < kNumPads; ++p)
    {
        const juce::String n = "P" + juce::String(p + 1).paddedLeft('0', 2) + " ";
        auto id = [p](const char* base) { return padParamId(p, base); };

        const auto d = PadGrid::getDefaultPadPreset(p);
        const float defTune = d.tune;
        const float defDec  = d.decay;
        const float defDrv  = d.drive;
        const float defP1   = d.p1;
        const float defP2   = d.p2;
        const float defP3   = d.p3;
        const float defP4   = d.p4;
        const float defP5   = d.p5;
        const int defChoke  = d.choke;
        const int defVcfType = d.vcfType;
        const float defVcfCut = d.vcfCut;
        const float defVcfRes = d.vcfRes;
        const float defVcfEnv = d.vcfEnv;

        addF(id("lvl"),  n + "Level",     { 0.f, 1.f, 0.001f },              0.55f);
        addF(id("pan"),  n + "Pan",       { -1.f, 1.f, 0.001f },             0.f);
        addF(id("tune"), n + "Tune",      { -24.f, 24.f, 0.01f },            defTune);
        addF(id("dec"),  n + "Decay",     { 0.01f, 6.f, 0.001f, 0.45f },     defDec);
        addF(id("eqlf"), n + "EQ Lo Hz",  { 20.f, 2000.f, 0.1f, 0.3f },      200.f);
        addF(id("eqlg"), n + "EQ Lo dB",  { -18.f, 18.f, 0.01f },            0.f);
        addF(id("eqmf"), n + "EQ Mid Hz", { 100.f, 8000.f, 0.1f, 0.3f },     1000.f);
        addF(id("eqmg"), n + "EQ Mid dB", { -18.f, 18.f, 0.01f },            0.f);
        addF(id("eqhf"), n + "EQ Hi Hz",  { 2000.f, 16000.f, 0.1f, 0.3f },   8000.f);
        addF(id("eqhg"), n + "EQ Hi dB",  { -18.f, 18.f, 0.01f },            0.f);
        addF(id("cthr"), n + "Comp Thr",  { -60.f, 0.f, 0.1f },              0.f);
        addF(id("crat"), n + "Comp Ratio",{ 1.f, 20.f, 0.01f, 0.3f },        1.f);
        addF(id("catk"), n + "Comp Atk",  { 0.1f, 100.f, 0.01f, 0.3f },      5.f);
        addF(id("crel"), n + "Comp Rel",  { 10.f, 1000.f, 0.1f, 0.3f },      100.f);
        addF(id("drv"),  n + "Drive",     { 0.f, 1.f, 0.001f },              defDrv);
        addI(id("fx"),   n + "FX Type",   0, 4, 0);
        addF(id("fx1"),  n + "FX P1",     { 0.f, 1.f, 0.001f },              defP1);
        addF(id("fx2"),  n + "FX P2",     { 0.f, 1.f, 0.001f },              defP2);
        addF(id("fx3"),  n + "FX P3",     { 0.f, 1.f, 0.001f },              defP3);
        addF(id("fx4"),  n + "FX P4",     { 0.f, 1.f, 0.001f },              defP4);
        addF(id("fx5"),  n + "FX P5",     { 0.f, 1.f, 0.001f },              defP5);

        addF(id("snda"), n + "Send A",    { 0.f, 1.f, 0.001f },              0.f);
        addF(id("sndb"), n + "Send B",    { 0.f, 1.f, 0.001f },              0.f);
        addF(id("sndc"), n + "Send C",    { 0.f, 1.f, 0.001f },              0.f);
        addF(id("sndd"), n + "Send D",    { 0.f, 1.f, 0.001f },              0.f);
        addI(id("chok"), n + "Choke",     0, kNumChokes, defChoke);
        addI(id("obus"), n + "Out Bus",   1, kNumBuses, 1);
        addI(id("pmode"), n + "Mode",     0, 1, 0);
        addI(id("mchan"), n + "MIDI Ch",  0, 16, 0);
        addI(id("mnote"), n + "MIDI Note", 0, 127, juce::jmin(127, 36 + p));
        addI(id("src"),  n + "Source",    0, (int) SRC_COUNT - 1, (int) SRC_LUA);

        addF(id("satk"), n + "SMPL Atk",  { 0.0005f, 5.0f, 0.001f, 0.3f },  0.001f);
        addF(id("sdec"), n + "SMPL Dec",  { 0.005f, 10.0f, 0.001f, 0.3f },  1.0f);
        addF(id("ssus"), n + "SMPL Sus",  { 0.f, 1.f, 0.001f },              1.0f);
        addF(id("srel"), n + "SMPL Rel",  { 0.005f, 10.0f, 0.001f, 0.3f },  0.1f);
        addI(id("ifx"),  n + "Insert FX", 0, 21, 0);
        addF(id("ifx1"), n + "IFX P1",    { 0.f, 1.f, 0.001f },              0.5f);
        addF(id("ifx2"), n + "IFX P2",    { 0.f, 1.f, 0.001f },              0.5f);
        addF(id("ifx3"), n + "IFX P3",    { 0.f, 1.f, 0.001f },              0.5f);
        addF(id("ifx4"), n + "IFX P4",    { 0.f, 1.f, 0.001f },              0.5f);

        addI(id("vcft"), n + "VCF Type",  0, 4,                              defVcfType);
        addF(id("vcfc"), n + "VCF Cut",   { 20.f, 20000.f, 0.1f, 0.25f },   defVcfCut);
        addF(id("vcfr"), n + "VCF Res",   { 0.1f, 10.f, 0.01f, 0.35f },     defVcfRes);
        addF(id("vcfe"), n + "VCF Env",   { -1.f, 1.f, 0.001f },            defVcfEnv);
        addF(id("cmg"),  n + "Comp Gain", { 0.f, 24.f, 0.1f },              0.f);
    }

    return { params.begin(), params.end() };
}

// ---------------------------------------------------------------------------
Forge64Processor::Forge64Processor()
    : AudioProcessor(makeBuses()),
      apvts(*this, &undoManager, "PARAMS", makeParams())
{
    ScriptPresetManager::initializePresetsOnDisk();
    SequencePresetManager::initializePresetsOnDisk();
    PresetManager::initializePresetsOnDisk();

    kitRoot = juce::ValueTree("FORGE64KIT");
    kitRoot.appendChild(apvts.state, nullptr);
    kitRoot.appendChild(PadGrid::makeDefaultTree(), nullptr);

    juce::ValueTree srcT, matT;
    ModMatrix::fillDefaultTrees(srcT, matT);
    kitRoot.appendChild(srcT, nullptr);
    kitRoot.appendChild(matT, nullptr);

    gridPtr = std::make_unique<PadGrid>(kitRoot.getChildWithName("PADS"), sampleManager);
    gridPtr->bindParams(&apvts);
    modPtr = std::make_unique<ModMatrix>(kitRoot.getChildWithName("MODSRC"),
                                         kitRoot.getChildWithName("MODMAT"), apvts);
    presetPtr = std::make_unique<PresetManager>(kitRoot, apvts);
    presetPtr->onLoaded = [this] { onPresetLoaded(); };
    presetPtr->onBeforeSave = [this]
    {
        kitRoot.removeChild(kitRoot.getChildWithName("MIDI_LEARN"), nullptr);
        kitRoot.appendChild(midiLearn.serialize(), nullptr);
        kitRoot.removeChild(kitRoot.getChildWithName("SEQUENCER"), nullptr);
        kitRoot.appendChild(sequencer.serialize(), nullptr);
        kitRoot.removeChild(kitRoot.getChildWithName("AUX_MASTER_FX"), nullptr);
        kitRoot.appendChild(auxManager.serialize(), nullptr);
    };

    voices.setDeps(gridPtr.get(), modPtr.get());

    for (int p = 0; p < kNumPads; ++p)
    {
        const auto st = gridPtr->padState(p);
        const auto script = st.getProperty("script", "").toString();
        const bool scriptOn = bool(st.getProperty("scriptOn", false));
        if (script.isNotEmpty())
            luaEngine.setScript(p, script, scriptOn);
    }

    for (int p = 0; p < kNumPads; ++p)
        for (int k = 0; k < kNumPadParams; ++k)
        {
            const auto id = padParamId(p, kPadParams[k].base);
            padIds[(size_t) p][(size_t) k] = id.toStdString();
            padPtrs[(size_t) p][(size_t) k] =
                dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter(id));
        }

    static const char* gids[GI_Count] = { "master", "revsize", "revdamp", "dlytime", "dlyfb" };
    for (int i = 0; i < GI_Count; ++i)
        globalPtrs[(size_t) i] = dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter(gids[i]));

    // Initialize all 64 pads with tailored sound presets
    for (int p = 0; p < kNumPads; ++p)
    {
        const auto pre = PadGrid::getDefaultPadPreset(p);
        auto setP = [this, p](const char* base, float val)
        {
            if (auto* par = dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter(padParamId(p, base))))
                par->setValueNotifyingHost(par->convertTo0to1(val));
        };
        setP("tune", pre.tune);
        setP("dec",  pre.decay);
        setP("drv",  pre.drive);
        setP("fx1",  pre.p1);
        setP("fx2",  pre.p2);
        setP("fx3",  pre.p3);
        setP("fx4",  pre.p4);
        setP("fx5",  pre.p5);
        setP("vcft", (float) pre.vcfType);
        setP("vcfc", pre.vcfCut);
        setP("vcfr", pre.vcfRes);
        setP("vcfe", pre.vcfEnv);
    }

    sequencer.clearCurrentPattern();
}

Forge64Processor::~Forge64Processor() = default;

void Forge64Processor::triggerAudition(int pad, float velocity)
{
    if (pad < 0 || pad >= kNumPads)
        return;
    lastTriggeredPad.store(pad);
    const juce::SpinLock::ScopedLockType sl(auditionLock);
    auditionQueue.push_back({ pad, velocity, false });
}

void Forge64Processor::triggerStepAudition(int pad, float velocity, const StepData& stepData)
{
    if (pad < 0 || pad >= kNumPads)
        return;
    lastTriggeredPad.store(pad);
    const juce::SpinLock::ScopedLockType sl(auditionLock);
    AuditionTrigger tr;
    tr.pad = pad;
    tr.vel = velocity;
    tr.hasLocks = stepData.hasLocks;
    tr.lockMask = stepData.lockMask;
    if (stepData.hasLocks)
    {
        tr.pitch = stepData.pLockPitch;
        tr.decay = stepData.pLockDecay;
        tr.drive = stepData.pLockDrive;
        tr.tone  = stepData.pLockTone;
        tr.p2    = stepData.pLockP2;
        tr.p3    = stepData.pLockP3;
        tr.p4    = stepData.pLockP4;
        tr.p5    = stepData.pLockP5;
        tr.sendA = stepData.pLockSendA;
        tr.sendB = stepData.pLockSendB;
        tr.level = stepData.pLockLevel;
        tr.pan   = stepData.pLockPan;
        tr.modAmt = stepData.pLockModAmt;
        tr.vcfType = stepData.pLockVcfType;
        tr.vcfCut  = stepData.pLockVcfCut;
        tr.vcfRes  = stepData.pLockVcfRes;
        tr.vcfEnv  = stepData.pLockVcfEnv;
        tr.eqLF    = stepData.pLockEqLF;
        tr.eqLG    = stepData.pLockEqLG;
        tr.eqMF    = stepData.pLockEqMF;
        tr.eqMG    = stepData.pLockEqMG;
        tr.eqHF    = stepData.pLockEqHF;
        tr.eqHG    = stepData.pLockEqHG;
        tr.cThr    = stepData.pLockCThr;
        tr.cRat    = stepData.pLockCRat;
        tr.cAtk    = stepData.pLockCAtk;
        tr.cRel    = stepData.pLockCRel;
        tr.cMg     = stepData.pLockCMg;
        tr.ifxType = stepData.pLockIfxType;
        tr.ifx1    = stepData.pLockIfx1;
        tr.ifx2    = stepData.pLockIfx2;
        tr.ifx3    = stepData.pLockIfx3;
        tr.ifx4    = stepData.pLockIfx4;
        tr.sendC   = stepData.pLockSendC;
        tr.sendD   = stepData.pLockSendD;
    }
    auditionQueue.push_back(tr);
}

bool Forge64Processor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    for (int i = 1; i < kNumBuses; ++i)
    {
        const auto set = layouts.getChannelSet(false, i);
        if (! set.isDisabled() && set != juce::AudioChannelSet::stereo())
            return false;
    }
    return true;
}

void Forge64Processor::prepareToPlay(double sr, int maxBlock)
{
    for (int i = 0; i < kNumPads; ++i)
        chains[(size_t) i].prepare(sr, maxBlock);
    gfx.prepare(sr, maxBlock);
    auxManager.prepare(sr, maxBlock);
    modPtr->prepare(sr, maxBlock);
    voices.prepare(sr, maxBlock);
    sequencer.prepare(sr);
    padTailHold.fill(0);
    dynamicSummingGain = 1.0f;

    busScratch.setSize(2 * kNumBuses, maxBlock, false, false, true);
    auxA.setSize(2, maxBlock, false, false, true);
    auxB.setSize(2, maxBlock, false, false, true);
    auxC.setSize(2, maxBlock, false, false, true);
    auxD.setSize(2, maxBlock, false, false, true);
    scratchL.assign((size_t) maxBlock, 0.f);
    scratchR.assign((size_t) maxBlock, 0.f);

    padBufStride = juce::jmax(1, maxBlock);
    padBuf.assign((size_t) kNumPads * 2 * (size_t) padBufStride, 0.f);

    {
        const int cores = juce::SystemStats::getNumCpus();
        int wanted = juce::jlimit(0, 7, cores - 1);
        const auto envThreads = juce::SystemStats::getEnvironmentVariable("FORGE64_THREADS", {});
        if (envThreads.isNotEmpty())
            wanted = juce::jlimit(0, 15, envThreads.getIntValue());
        if (workerPool.numWorkers() != wanted)
            workerPool.start(wanted);
    }

    busOffset.fill(0);
    busActive.fill(false);
    int off = 0;
    for (int b = 0; b < kNumBuses; ++b)
    {
        const auto set = getChannelLayoutOfBus(false, b);
        const int ch = set.isDisabled() ? 0 : set.size();
        busActive[(size_t) b] = ch > 0;
        busOffset[(size_t) b] = off;
        off += ch;
    }
}

void Forge64Processor::releaseResources()
{
    voices.allNotesOff();
}

float Forge64Processor::globalEff(int idx) const
{
    if (idx < 0 || idx >= GI_Count)
        return 0.f;
    auto* par = globalPtrs[(size_t) idx];
    if (par == nullptr)
        return 0.f;
    static const char* gids[GI_Count] = { "master", "revsize", "revdamp", "dlytime", "dlyfb" };
    const float v = par->getValue() + modPtr->offsetFor(gids[idx]);
    return par->convertFrom0to1(clampRange(v, 0.f, 1.f));
}

void Forge64Processor::fillPadParams(int pad, PadParams& out, float modScale)
{
    auto& ptrs = padPtrs[(size_t) pad];
    auto& ids = padIds[(size_t) pad];

    auto getF = [&](int k) -> float
    {
        auto* par = ptrs[(size_t) k];
        if (par == nullptr)
            return 0.f;
        const float v = par->getValue() + modPtr->offsetFor(ids[(size_t) k]) * modScale;
        return par->convertFrom0to1(clampRange(v, 0.f, 1.f));
    };
    auto getI = [&](int k) -> int { return (int) std::lround((double) getF(k)); };

    out.level = getF(0);  out.pan = getF(1);  out.tune = getF(2);  out.decay = getF(3);
    out.eqLF = getF(4);   out.eqLG = getF(5); out.eqMF = getF(6);  out.eqMG = getF(7);
    out.eqHF = getF(8);   out.eqHG = getF(9);
    out.cThr = getF(10);  out.cRat = getF(11); out.cAtk = getF(12); out.cRel = getF(13);
    out.drive = getF(14);
    out.fxType = getI(15);
    out.fx1 = getF(16);   out.fx2 = getF(17); out.fx3 = getF(18);  out.fx4 = getF(19);
    out.fx5 = getF(20);
    out.sendA = getF(21); out.sendB = getF(22);
    out.sendC = getF(23); out.sendD = getF(24);
    out.choke = getI(25); out.outBus = getI(26);
    out.mode = getI(27);  out.mchan = getI(28); out.mnote = getI(29); out.srcType = getI(30);
    out.smplAtk = getF(31); out.smplDec = getF(32); out.smplSus = getF(33); out.smplRel = getF(34);
    out.ifxType = getI(35);
    out.ifx1 = getF(36);  out.ifx2 = getF(37); out.ifx3 = getF(38);  out.ifx4 = getF(39);
    out.vcfType = getI(40);
    out.vcfCut  = getF(41);
    out.vcfRes  = getF(42);
    out.vcfEnv  = getF(43);
    out.cMg     = getF(44);
}

// ---------------------------------------------------------------------------
void Forge64Processor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (n <= 0)
    {
        buffer.clear();
        return;
    }

    const int64_t clockNow = clock.load() + n;
    clock.store(clockNow);
    voices.setHitClock(clockNow);

    double bpm = 120.0;
    bool playing = false;
    bool hostJustStarted = false;
    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            if (auto t = pos->getBpm())
                bpm = *t;
            playing = pos->getIsPlaying();
            if (auto ppq = pos->getPpqPosition())
            {
                if (*ppq < lastPpqPosition - 0.05) // DAW loop or rewind / seek back
                    hostJustStarted = true;
                lastPpqPosition = *ppq;
            }
        }
    }

    if (playing && ! wasHostPlaying)
        hostJustStarted = true;
    else if (! playing && wasHostPlaying)
        sequencer.resetPlayback();

    if (hostJustStarted)
        sequencer.resetPlayback();

    wasHostPlaying = playing;
    modPtr->setTempo(bpm, playing);

    busScratch.setSize(2 * kNumBuses, n, false, false, true);
    busScratch.clear();
    auxA.setSize(2, n, false, false, true);
    auxA.clear();
    auxB.setSize(2, n, false, false, true);
    auxB.clear();
    auxC.setSize(2, n, false, false, true);
    auxC.clear();
    auxD.setSize(2, n, false, false, true);
    auxD.clear();
    if ((int) scratchL.size() < n)
    {
        scratchL.resize((size_t) n);
        scratchR.resize((size_t) n);
    }
    if (padBufStride < n)
    {
        padBufStride = n;
        padBuf.assign((size_t) kNumPads * 2 * (size_t) padBufStride, 0.f);
    }

    // 1) Latch MIDI modulation sources and collect timed note events.
    rawEvents.clear();
    for (const auto& meta : midi)
    {
        const auto& m = meta.getMessage();
        modPtr->handleMidiMessage(m);

        if (m.isController())
        {
            const int chan = m.getChannel();
            const int cc = m.getControllerNumber();
            const float normVal = (float) m.getControllerValue() / 127.0f;
            if (midiLearn.isLearning())
            {
                auto target = midiLearn.currentLearningTarget();
                if (! target.isEmpty())
                {
                    midiLearn.bind(chan, cc, target);
                    midiLearn.stopLearning();
                }
            }
            else
            {
                auto bound = midiLearn.getBoundParam(chan, cc);
                if (! bound.isEmpty())
                {
                    if (auto* par = dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter(bound)))
                        par->setValueNotifyingHost(normVal);
                }
            }
        }

        if (m.isProgramChange())
            currentBank.store(m.getProgramChangeNumber() % kNumBanks);
        if (m.isAllNotesOff() || m.isAllSoundOff())
            voices.allNotesOff();

        if (m.isNoteOn() || m.isNoteOff())
        {
            RawMidiEv e;
            e.pos = clampRange(meta.samplePosition, 0, n - 1);
            e.vel = m.getFloatVelocity();
            e.note = m.getNoteNumber();
            e.chan = m.getChannel();
            e.off = m.isNoteOff();
            rawEvents.push_back(e);
        }
    }

    auto retriggerEnvsForPad = [&](int padIndex, int noteNum)
    {
        lastTriggeredPad.store(padIndex);
        for (int i = 0; i < kNumEnv; ++i)
        {
            if (auto* env = dynamic_cast<EnvSource*>(modPtr->sourceAt(slotEnv(i))))
            {
                const int tp = env->triggerPad.load();
                const int tn = env->triggerNote.load();
                if ((tp == -1 && tn == -1) || tp == padIndex || (tn >= 0 && tn == noteNum))
                    env->retrigger();
            }
        }
    };

    // 2) Modulation sources -> per-destination block offsets.
    modPtr->renderSources(n);
    modPtr->computeOffsets();

    // 3) Effective (modulated) pad parameters.
    for (int p = 0; p < kNumPads; ++p)
        fillPadParams(p, eff[(size_t) p]);

    // 4) Note routing: per-note pad map + chromatic channel map.
    for (auto& v : noteMap)
        v.clear();
    chromMap.fill(-1);
    for (int p = 0; p < kNumPads; ++p)
    {
        const auto& e = eff[(size_t) p];
        if (e.mode == 0)
            noteMap[(size_t) clampRange(e.mnote, 0, 127)].push_back(p);
        else if (e.mchan > 0)
            chromMap[(size_t) e.mchan] = p;
    }

    for (int p = 0; p < kNumPads; ++p)
        padEvents[(size_t) p].clear();

    for (const auto& re : rawEvents)
    {
        if (! re.off)
        {
            const int cp = chromMap[(size_t) clampRange(re.chan, 0, 16)];
            if (cp >= 0)
            {
                auto& rt = gridPtr->runtime(cp);
                rt.hasLocks.store(false);
                rt.isNewTrigger.store(true);
                rt.lastVel.store(re.vel);
                rt.lastNote.store(re.note);
                rt.lastHitStamp.store(clockNow);
                padEvents[(size_t) cp].push_back({ re.pos, { cp, re.vel, re.note, re.chan, false } });
                retriggerEnvsForPad(cp, re.note);
            }
            else
            {
                for (int p : noteMap[(size_t) clampRange(re.note, 0, 127)])
                {
                    if (eff[(size_t) p].mchan == 0 || eff[(size_t) p].mchan == re.chan)
                    {
                        auto& rt = gridPtr->runtime(p);
                        rt.hasLocks.store(false);
                        rt.isNewTrigger.store(true);
                        rt.lastVel.store(re.vel);
                        rt.lastNote.store(re.note);
                        rt.lastHitStamp.store(clockNow);
                        padEvents[(size_t) p].push_back({ re.pos, { p, re.vel, re.note, re.chan, false } });
                        retriggerEnvsForPad(p, re.note);
                    }
                }
            }
        }
        else
        {
            for (int p = 0; p < kNumPads; ++p)
            {
                const auto& e = eff[(size_t) p];
                if ((e.mode == 1 || e.srcType == SRC_SAMPLE) && (e.mchan == 0 || e.mchan == re.chan))
                    padEvents[(size_t) p].push_back({ re.pos, { p, 0.f, re.note, re.chan, true } });
            }
        }
    }

    // Drain UI mouse audition triggers into padEvents
    {
        const juce::SpinLock::ScopedLockType sl(auditionLock);
        for (const auto& a : auditionQueue)
        {
            if (a.pad >= 0 && a.pad < kNumPads)
            {
                const int nte = eff[(size_t) a.pad].mnote;
                padEvents[(size_t) a.pad].push_back({ 0, { a.pad, a.vel, nte, 1, false } });
                retriggerEnvsForPad(a.pad, nte);

                auto& rt = gridPtr->runtime(a.pad);
                rt.lastVel.store(a.vel);
                rt.lastNote.store(nte);
                rt.lastHitStamp.store(clockNow);
                rt.hasLocks.store(a.hasLocks);
                rt.lockMask.store(a.hasLocks ? a.lockMask : 0);
                if (a.hasLocks)
                {
                    rt.latchedTune.store(a.pitch);
                    rt.latchedDecay.store(a.decay);
                    rt.latchedTone.store(a.tone);
                    rt.latchedDrive.store(a.drive);
                    rt.latchedP2.store(a.p2);
                    rt.latchedP3.store(a.p3);
                    rt.latchedP4.store(a.p4);
                    rt.latchedP5.store(a.p5);
                    rt.latchedSendA.store(a.sendA);
                    rt.latchedSendB.store(a.sendB);
                    rt.latchedLevel.store(a.level);
                    rt.latchedPan.store(a.pan);
                    rt.latchedModAmt.store(a.modAmt);

                    rt.latchedVcfType.store(a.vcfType);
                    rt.latchedVcfCut.store(a.vcfCut);
                    rt.latchedVcfRes.store(a.vcfRes);
                    rt.latchedVcfEnv.store(a.vcfEnv);

                    rt.latchedEqLF.store(a.eqLF);
                    rt.latchedEqLG.store(a.eqLG);
                    rt.latchedEqMF.store(a.eqMF);
                    rt.latchedEqMG.store(a.eqMG);
                    rt.latchedEqHF.store(a.eqHF);
                    rt.latchedEqHG.store(a.eqHG);

                    rt.latchedCThr.store(a.cThr);
                    rt.latchedCRat.store(a.cRat);
                    rt.latchedCAtk.store(a.cAtk);
                    rt.latchedCRel.store(a.cRel);
                    rt.latchedCMg.store(a.cMg);

                    rt.latchedIfxType.store(a.ifxType);
                    rt.latchedIfx1.store(a.ifx1);
                    rt.latchedIfx2.store(a.ifx2);
                    rt.latchedIfx3.store(a.ifx3);
                    rt.latchedIfx4.store(a.ifx4);

                    rt.latchedSendC.store(a.sendC);
                    rt.latchedSendD.store(a.sendD);
                }
                rt.isNewTrigger.store(true);
            }
        }
        auditionQueue.clear();
    }

    // 4.5) Process Step Sequencer
    std::vector<StepSequencer::TriggerEvent> seqTriggers;
    sequencer.process(n, bpm, playing, seqTriggers);
    for (const auto& st : seqTriggers)
    {
        if (st.pad >= 0 && st.pad < kNumPads)
        {
            padEvents[(size_t) st.pad].push_back({ st.pos, { st.pad, st.vel, 60, 1, false } });
            retriggerEnvsForPad(st.pad, 60);

            auto& rt = gridPtr->runtime(st.pad);
            rt.lastVel.store(st.vel);
            rt.lastNote.store(60);
            rt.lastHitStamp.store(clockNow);
            rt.hasLocks.store(st.hasLocks);
            rt.lockMask.store(st.hasLocks ? st.lockMask : 0);
            if (st.hasLocks)
            {
                rt.latchedTune.store(st.pitch);
                rt.latchedDecay.store(st.decay);
                rt.latchedTone.store(st.tone);
                rt.latchedDrive.store(st.drive);
                rt.latchedP2.store(st.p2);
                rt.latchedP3.store(st.p3);
                rt.latchedP4.store(st.p4);
                rt.latchedP5.store(st.p5);
                rt.latchedSendA.store(st.sendA);
                rt.latchedSendB.store(st.sendB);
                rt.latchedLevel.store(st.level);
                rt.latchedPan.store(st.pan);
                rt.latchedModAmt.store(st.modAmt);

                rt.latchedVcfType.store(st.vcfType);
                rt.latchedVcfCut.store(st.vcfCut);
                rt.latchedVcfRes.store(st.vcfRes);
                rt.latchedVcfEnv.store(st.vcfEnv);

                rt.latchedEqLF.store(st.eqLF);
                rt.latchedEqLG.store(st.eqLG);
                rt.latchedEqMF.store(st.eqMF);
                rt.latchedEqMG.store(st.eqMG);
                rt.latchedEqHF.store(st.eqHF);
                rt.latchedEqHG.store(st.eqHG);

                rt.latchedCThr.store(st.cThr);
                rt.latchedCRat.store(st.cRat);
                rt.latchedCAtk.store(st.cAtk);
                rt.latchedCRel.store(st.cRel);
                rt.latchedCMg.store(st.cMg);

                rt.latchedIfxType.store(st.ifxType);
                rt.latchedIfx1.store(st.ifx1);
                rt.latchedIfx2.store(st.ifx2);
                rt.latchedIfx3.store(st.ifx3);
                rt.latchedIfx4.store(st.ifx4);

                rt.latchedSendC.store(st.sendC);
                rt.latchedSendD.store(st.sendD);
            }
            rt.isNewTrigger.store(true);
        }
    }

    // 5) Render pads: voices -> optional Lua -> pad chain -> bus/aux routing.
    const double sr = getSampleRate();
    const float master = globalEff(GI_Master);

    bool anyPadSolo = false;
    for (int p = 0; p < kNumPads; ++p)
    {
        if (gridPtr->runtime(p).isSolo.load())
        {
            anyPadSolo = true;
            break;
        }
    }

    for (int p = 0; p < kNumPads; ++p)
    {
        if (padEvents[(size_t) p].size() > 1)
        {
            std::sort(padEvents[(size_t) p].begin(), padEvents[(size_t) p].end(),
                      [](const VoicePool::TimedEvent& a, const VoicePool::TimedEvent& b) { return a.pos < b.pos; });
        }
    }

    // Polyphony Guard & Simultaneous Burst Limiter:
    // If a burst of pad triggers arrives in a single block (e.g. hitting all pads simultaneously),
    // prioritize the most musically significant pads up to kMaxPolyphony so the audio engine
    // doesn't attempt to start 64 simultaneous voices, avoiding CPU underrun and chaotic voice stealing.
    int totalNewPadHits = 0;
    for (int p = 0; p < kNumPads; ++p)
    {
        for (const auto& te : padEvents[(size_t) p])
        {
            if (! te.ev.isOff)
            {
                ++totalNewPadHits;
                break;
            }
        }
    }

    if (totalNewPadHits > VoicePool::kMaxPolyphony)
    {
        struct TriggerCandidate
        {
            int pad;
            float priority;
        };
        std::vector<TriggerCandidate> candidates;
        candidates.reserve((size_t) totalNewPadHits);

        for (int p = 0; p < kNumPads; ++p)
        {
            float maxVel = 0.f;
            bool hasTrig = false;
            for (const auto& te : padEvents[(size_t) p])
            {
                if (! te.ev.isOff)
                {
                    hasTrig = true;
                    if (te.ev.vel > maxVel) maxVel = te.ev.vel;
                }
            }
            if (hasTrig)
            {
                // Score: velocity + core drum hierarchy (Bank A: core acoustic/electronic, Bank B: heavy drums, Bank C: perc)
                float prio = maxVel * 10.f;
                if (p < 16)      prio += 25.f; // Bank A: Kick, Snare, Hats, Toms, Cymbals
                else if (p < 32) prio += 15.f; // Bank B: 909, Hardstyle, Claps
                else if (p < 48) prio += 5.f;  // Bank C: World percussion
                candidates.push_back({ p, prio });
            }
        }

        std::sort(candidates.begin(), candidates.end(), [](const TriggerCandidate& a, const TriggerCandidate& b) {
            return a.priority > b.priority;
        });

        std::array<bool, kNumPads> keepPad {};
        keepPad.fill(false);
        const int keepCount = juce::jmin((int) candidates.size(), VoicePool::kMaxPolyphony);
        for (int i = 0; i < keepCount; ++i)
            keepPad[(size_t) candidates[(size_t) i].pad] = true;

        for (int p = 0; p < kNumPads; ++p)
        {
            if (! keepPad[(size_t) p])
            {
                auto& evs = padEvents[(size_t) p];
                evs.erase(std::remove_if(evs.begin(), evs.end(), [](const VoicePool::TimedEvent& te) {
                    return ! te.ev.isOff;
                }), evs.end());
            }
        }
    }

    // Dynamic Equal-Power Summing Scaler:
    // When multiple pads sound simultaneously, scale headroom smoothly using the acoustic
    // power-sum law (1/sqrt(N)) so summing 8, 16, or 24 pads never blows through the ceiling (+25 dBFS)
    // into harsh square-wave clipping, while single drum hits maintain 100% full punch and volume.
    int activeSoundingPads = 0;
    for (int p = 0; p < kNumPads; ++p)
    {
        if (voices.padActive(p) || ! padEvents[(size_t) p].empty() || padTailHold[(size_t) p] > 0)
            ++activeSoundingPads;
    }

    const float targetSumGain = (activeSoundingPads <= 1) ? 1.0f
        : (1.0f / std::sqrt(1.0f + (float) (activeSoundingPads - 1) * 0.38f));

    const float sumSmoothRate = (targetSumGain < dynamicSummingGain) ? 0.30f : 0.08f;
    dynamicSummingGain += (targetSumGain - dynamicSummingGain) * sumSmoothRate;

    numPadJobs = 0;
    jobBlockSize = n;
    jobSampleRate = getSampleRate();

    for (int p = 0; p < kNumPads; ++p)
    {
        const bool hasEvents = ! padEvents[(size_t) p].empty();
        const bool vActive   = voices.padActive(p);
        if (! hasEvents && ! vActive && padTailHold[(size_t) p] <= 0)
            continue;

        if (vActive || hasEvents)
            padTailHold[(size_t) p] = 20; // Maintain ~200ms ringdown for filter/FX tails and clean fadeout
        else if (padTailHold[(size_t) p] > 0)
            --padTailHold[(size_t) p];

        const bool isLastTailBlock = (! vActive && ! hasEvents && padTailHold[(size_t) p] == 0);

        float* const pL = padBufL(p);
        float* const pR = padBufR(p);
        std::fill(pL, pL + n, 0.f);
        std::fill(pR, pR + n, 0.f);

        auto& rt = gridPtr->runtime(p);
        auto pp = eff[(size_t) p];
        const uint64_t mask = rt.lockMask.load();
        const bool hasLocks = rt.hasLocks.load() && mask != 0;
        const bool isPLockPreview = rt.isPLockPreviewActive.load();

        if (isPLockPreview || hasLocks)
        {
            auto& ptrs = padPtrs[(size_t) p];
            auto& ids  = padIds[(size_t) p];
            const float modScale = (hasLocks && (mask & LOCK_FLAG_MODAMT)) ? rt.latchedModAmt.load() : 1.0f;

            auto applyMod = [&](int k, float baseVal) -> float
            {
                auto* par = ptrs[(size_t) k];
                if (par == nullptr)
                    return baseVal;
                const float off = modPtr->offsetFor(ids[(size_t) k]) * modScale;
                if (std::abs(off) < 0.0001f)
                    return baseVal;
                const float norm = par->convertTo0to1(baseVal) + off;
                return par->convertFrom0to1(clampRange(norm, 0.f, 1.f));
            };

            const auto& bp = padBaseParams[(size_t) p];

            // 1. Level & Pan
            if (hasLocks && (mask & LOCK_FLAG_LEVEL)) pp.level = applyMod(0, rt.latchedLevel.load());
            else if (isPLockPreview)                  pp.level = applyMod(0, bp.level);

            if (hasLocks && (mask & LOCK_FLAG_PAN))   pp.pan   = applyMod(1, rt.latchedPan.load());
            else if (isPLockPreview)                  pp.pan   = applyMod(1, bp.pan);

            // 2. Tune & Decay
            if (hasLocks && (mask & LOCK_FLAG_PITCH)) pp.tune  = applyMod(2, rt.latchedTune.load());
            else if (isPLockPreview)                  pp.tune  = applyMod(2, bp.tune);

            if (hasLocks && (mask & LOCK_FLAG_DECAY))
            {
                const float decFactor = rt.latchedDecay.load();
                const float baseDecay = isPLockPreview ? applyMod(3, bp.decay) : pp.decay;
                pp.decay = juce::jlimit(0.01f, 6.0f, baseDecay * decFactor);
            }
            else if (isPLockPreview)
            {
                pp.decay = applyMod(3, bp.decay);
            }

            // 3. EQ
            if (hasLocks && (mask & LOCK_FLAG_EQ_LF)) pp.eqLF = applyMod(4, rt.latchedEqLF.load());
            else if (isPLockPreview)                  pp.eqLF = applyMod(4, bp.eqLF);

            if (hasLocks && (mask & LOCK_FLAG_EQ_LG)) pp.eqLG = applyMod(5, rt.latchedEqLG.load());
            else if (isPLockPreview)                  pp.eqLG = applyMod(5, bp.eqLG);

            if (hasLocks && (mask & LOCK_FLAG_EQ_MF)) pp.eqMF = applyMod(6, rt.latchedEqMF.load());
            else if (isPLockPreview)                  pp.eqMF = applyMod(6, bp.eqMF);

            if (hasLocks && (mask & LOCK_FLAG_EQ_MG)) pp.eqMG = applyMod(7, rt.latchedEqMG.load());
            else if (isPLockPreview)                  pp.eqMG = applyMod(7, bp.eqMG);

            if (hasLocks && (mask & LOCK_FLAG_EQ_HF)) pp.eqHF = applyMod(8, rt.latchedEqHF.load());
            else if (isPLockPreview)                  pp.eqHF = applyMod(8, bp.eqHF);

            if (hasLocks && (mask & LOCK_FLAG_EQ_HG)) pp.eqHG = applyMod(9, rt.latchedEqHG.load());
            else if (isPLockPreview)                  pp.eqHG = applyMod(9, bp.eqHG);

            // 4. Compressor
            if (hasLocks && (mask & LOCK_FLAG_COMP_THR)) pp.cThr = applyMod(10, rt.latchedCThr.load());
            else if (isPLockPreview)                     pp.cThr = applyMod(10, bp.cThr);

            if (hasLocks && (mask & LOCK_FLAG_COMP_RAT)) pp.cRat = applyMod(11, rt.latchedCRat.load());
            else if (isPLockPreview)                     pp.cRat = applyMod(11, bp.cRat);

            if (hasLocks && (mask & LOCK_FLAG_COMP_ATK)) pp.cAtk = applyMod(12, rt.latchedCAtk.load());
            else if (isPLockPreview)                     pp.cAtk = applyMod(12, bp.cAtk);

            if (hasLocks && (mask & LOCK_FLAG_COMP_REL)) pp.cRel = applyMod(13, rt.latchedCRel.load());
            else if (isPLockPreview)                     pp.cRel = applyMod(13, bp.cRel);

            if (hasLocks && (mask & LOCK_FLAG_COMP_MG))  pp.cMg  = applyMod(44, rt.latchedCMg.load());
            else if (isPLockPreview)                     pp.cMg  = applyMod(44, bp.cMg);

            // 5. Drive & Macro FX P1..P5
            if (hasLocks && (mask & LOCK_FLAG_DRIVE)) pp.drive = applyMod(14, rt.latchedDrive.load());
            else if (isPLockPreview)                  pp.drive = applyMod(14, bp.drive);

            if (hasLocks && (mask & LOCK_FLAG_TONE))  pp.fx1   = applyMod(16, rt.latchedTone.load());
            else if (isPLockPreview)                  pp.fx1   = applyMod(16, bp.fx1);

            if (hasLocks && (mask & LOCK_FLAG_P2))    pp.fx2   = applyMod(17, rt.latchedP2.load());
            else if (isPLockPreview)                  pp.fx2   = applyMod(17, bp.fx2);

            if (hasLocks && (mask & LOCK_FLAG_P3))    pp.fx3   = applyMod(18, rt.latchedP3.load());
            else if (isPLockPreview)                  pp.fx3   = applyMod(18, bp.fx3);

            if (hasLocks && (mask & LOCK_FLAG_P4))    pp.fx4   = applyMod(19, rt.latchedP4.load());
            else if (isPLockPreview)                  pp.fx4   = applyMod(19, bp.fx4);

            if (hasLocks && (mask & LOCK_FLAG_P5))    pp.fx5   = applyMod(20, rt.latchedP5.load());
            else if (isPLockPreview)                  pp.fx5   = applyMod(20, bp.fx5);

            // 6. Aux Sends
            if (hasLocks && (mask & LOCK_FLAG_SENDA)) pp.sendA = applyMod(21, rt.latchedSendA.load());
            else if (isPLockPreview)                  pp.sendA = applyMod(21, bp.sendA);

            if (hasLocks && (mask & LOCK_FLAG_SENDB)) pp.sendB = applyMod(22, rt.latchedSendB.load());
            else if (isPLockPreview)                  pp.sendB = applyMod(22, bp.sendB);

            if (hasLocks && (mask & LOCK_FLAG_SENDC)) pp.sendC = applyMod(23, rt.latchedSendC.load());
            else if (isPLockPreview)                  pp.sendC = applyMod(23, bp.sendC);

            if (hasLocks && (mask & LOCK_FLAG_SENDD)) pp.sendD = applyMod(24, rt.latchedSendD.load());
            else if (isPLockPreview)                  pp.sendD = applyMod(24, bp.sendD);

            // 7. Insert Multi-FX
            if (hasLocks && (mask & LOCK_FLAG_IFX_TYPE)) pp.ifxType = rt.latchedIfxType.load();
            else if (isPLockPreview)                     pp.ifxType = bp.ifxType;

            if (hasLocks && (mask & LOCK_FLAG_IFX1))     pp.ifx1    = applyMod(36, rt.latchedIfx1.load());
            else if (isPLockPreview)                     pp.ifx1    = applyMod(36, bp.ifx1);

            if (hasLocks && (mask & LOCK_FLAG_IFX2))     pp.ifx2    = applyMod(37, rt.latchedIfx2.load());
            else if (isPLockPreview)                     pp.ifx2    = applyMod(37, bp.ifx2);

            if (hasLocks && (mask & LOCK_FLAG_IFX3))     pp.ifx3    = applyMod(38, rt.latchedIfx3.load());
            else if (isPLockPreview)                     pp.ifx3    = applyMod(38, bp.ifx3);

            if (hasLocks && (mask & LOCK_FLAG_IFX4))     pp.ifx4    = applyMod(39, rt.latchedIfx4.load());
            else if (isPLockPreview)                     pp.ifx4    = applyMod(39, bp.ifx4);

            // 8. VCF
            if (hasLocks && (mask & LOCK_FLAG_VCF_TYPE)) pp.vcfType = rt.latchedVcfType.load();
            else if (isPLockPreview)                     pp.vcfType = bp.vcfType;

            if (hasLocks && (mask & LOCK_FLAG_VCF_CUT))  pp.vcfCut  = applyMod(41, rt.latchedVcfCut.load());
            else if (isPLockPreview)                     pp.vcfCut  = applyMod(41, bp.vcfCut);

            if (hasLocks && (mask & LOCK_FLAG_VCF_RES))  pp.vcfRes  = applyMod(42, rt.latchedVcfRes.load());
            else if (isPLockPreview)                     pp.vcfRes  = applyMod(42, bp.vcfRes);

            if (hasLocks && (mask & LOCK_FLAG_VCF_ENV))  pp.vcfEnv  = applyMod(43, rt.latchedVcfEnv.load());
            else if (isPLockPreview)                     pp.vcfEnv  = applyMod(43, bp.vcfEnv);
        }

        const bool isSilenced = (anyPadSolo && ! rt.isSolo.load()) || rt.isMuted.load();
        if (isSilenced)
        {
            // Drain voice events silently to keep voice states consistent
            voices.renderPad(p, scratchL.data(), scratchR.data(), n, sr, pp, padEvents[(size_t) p]);
            continue;
        }

        bool isNewTrig = rt.isNewTrigger.exchange(false);
        for (const auto& ev : padEvents[(size_t) p])
        {
            if (! ev.ev.isOff)
            {
                isNewTrig = true;
                rt.lastVel.store(ev.ev.vel);
                rt.lastNote.store(ev.ev.note);
                rt.lastHitStamp.store(clockNow);
            }
        }
        if (isNewTrig)
            rt.lastHitStamp.store(clockNow);

        voices.renderPad(p, pL, pR, n, sr, pp, padEvents[(size_t) p]);

        const bool padIsSounding = voices.padActive(p);

        if (! padIsSounding && ! hasEvents)
        {
            // If the voice was stolen or decayed away, quickly finish tail
            padTailHold[(size_t) p] = juce::jmin(padTailHold[(size_t) p], 4);
        }

        // Queue the heavy per-pad DSP (Lua script + insert chain) for parallel execution.
        auto& job = padJobs[(size_t) numPadJobs++];
        job.pad = p;
        job.pp = pp;
        job.lastTail = isLastTailBlock;
        job.newTrig = isNewTrig;
        job.runLua = padIsSounding && (rt.scriptOn.load() || pp.srcType == SRC_LUA);
        if (job.runLua)
        {
            const int64_t hit = rt.lastHitStamp.load();
            job.age = juce::jmax(0.0, hit > 0 ? (double) (clockNow - hit) / sr : 0.0);
            job.vel = rt.lastVel.load();
            job.note = rt.lastNote.load();
            // Chromatic mode: transpose by incoming MIDI note relative to base note
            job.luaTuneOffset = (pp.mode == 1) ? (float) (job.note - pp.mnote) : 0.f;
        }
    }

    // 5b) Parallel: every queued pad is independent (own Lua VM, own PadChain, own buffer).
    workerPool.run(numPadJobs, &Forge64Processor::runPadJob, this);

    // 5c) Serial mix of the rendered pad buffers into buses and aux sends.
    for (int j = 0; j < numPadJobs; ++j)
    {
        const auto& job = padJobs[(size_t) j];
        const int p = job.pad;
        const auto& pp = job.pp;
        const float* pL = padBufL(p);
        const float* pR = padBufR(p);

        const float gt = gridPtr->runtime(p).gainTrim.load();
        const float lvl = pp.level * master * gt * 0.75f * dynamicSummingGain; // Equal-power dynamic summing headroom
        const float pan = clampRange(pp.pan, -1.0f, 1.0f);
        const float panAngle = (pan + 1.0f) * 0.25f * 3.14159265358979323846f;
        const float panL = std::cos(panAngle) * 1.41421356f;
        const float panR = std::sin(panAngle) * 1.41421356f;

        const int bus = clampRange(pp.outBus, 1, kNumBuses) - 1;
        float* bL = busScratch.getWritePointer(bus * 2);
        float* bR = busScratch.getWritePointer(bus * 2 + 1);
        float* aL = auxA.getWritePointer(0);
        float* aR = auxA.getWritePointer(1);
        float* bL_aux = auxB.getWritePointer(0);
        float* bR_aux = auxB.getWritePointer(1);
        float* cL = auxC.getWritePointer(0);
        float* cR = auxC.getWritePointer(1);
        float* dL = auxD.getWritePointer(0);
        float* dR = auxD.getWritePointer(1);
        const float sa = pp.sendA, sb = pp.sendB, sc = pp.sendC, sd = pp.sendD;

        for (int i = 0; i < n; ++i)
        {
            const float l = pL[i] * lvl * panL;
            const float r = pR[i] * lvl * panR;
            bL[i] += l;
            bR[i] += r;
            if (sa > 0.f) { aL[i] += l * sa; aR[i] += r * sa; }
            if (sb > 0.f) { bL_aux[i] += l * sb; bR_aux[i] += r * sb; }
            if (sc > 0.f) { cL[i] += l * sc; cR[i] += r * sc; }
            if (sd > 0.f) { dL[i] += l * sd; dR[i] += r * sd; }
        }
    }

    // 6) 4 Aux Send Buses processed through AuxBusManager
    auxManager.setBpm(bpm);
    auxManager.processAux(0, auxA.getWritePointer(0), auxA.getWritePointer(1), n, auxManager.auxParams[0]);
    auxManager.processAux(1, auxB.getWritePointer(0), auxB.getWritePointer(1), n, auxManager.auxParams[1]);
    auxManager.processAux(2, auxC.getWritePointer(0), auxC.getWritePointer(1), n, auxManager.auxParams[2]);
    auxManager.processAux(3, auxD.getWritePointer(0), auxD.getWritePointer(1), n, auxManager.auxParams[3]);

    // Sum Aux returns into main bus (bus 0)
    {
        float* mL = busScratch.getWritePointer(0);
        float* mR = busScratch.getWritePointer(1);
        const float* aL = auxA.getReadPointer(0);
        const float* aR = auxA.getReadPointer(1);
        const float* bL_in = auxB.getReadPointer(0);
        const float* bR_in = auxB.getReadPointer(1);
        const float* cL = auxC.getReadPointer(0);
        const float* cR = auxC.getReadPointer(1);
        const float* dL = auxD.getReadPointer(0);
        const float* dR = auxD.getReadPointer(1);
        for (int i = 0; i < n; ++i)
        {
            mL[i] += aL[i] + bL_in[i] + cL[i] + dL[i];
            mR[i] += aR[i] + bR_in[i] + cR[i] + dR[i];
        }
    }

    // Process Master Bus Chain (VCA Glue Compressor, 4-Band Mastering EQ, Tape Drive & Limiter)
    auxManager.processMasterChain(busScratch.getWritePointer(0), busScratch.getWritePointer(1), n, auxManager.masterParams);

    // Calculate master output peak levels for UI metering
    {
        const float* pL = busScratch.getReadPointer(0);
        const float* pR = busScratch.getReadPointer(1);
        float peakL = 0.f, peakR = 0.f;
        for (int i = 0; i < n; ++i)
        {
            peakL = juce::jmax(peakL, std::abs(pL[i]));
            peakR = juce::jmax(peakR, std::abs(pR[i]));
        }
        const float prevL = masterPeakL.load();
        const float prevR = masterPeakR.load();
        masterPeakL.store(juce::jmax(peakL, prevL * 0.92f));
        masterPeakR.store(juce::jmax(peakR, prevR * 0.92f));
    }

    // 7) Copy internal buses to host outputs; pads on disabled buses fall
    //    back to the main bus so nothing goes silent.
    buffer.clear();
    const int totalCh = buffer.getNumChannels();
    for (int b = 0; b < kNumBuses; ++b)
    {
        const int off = busOffset[(size_t) b];
        if (busActive[(size_t) b] && off + 1 < totalCh)
        {
            buffer.copyFrom(off, 0, busScratch, b * 2, 0, n);
            buffer.copyFrom(off + 1, 0, busScratch, b * 2 + 1, 0, n);
        }
        else if (b > 0 && busActive[0] && busOffset[0] + 1 < totalCh)
        {
            buffer.addFrom(busOffset[0], 0, busScratch, b * 2, 0, n);
            buffer.addFrom(busOffset[0] + 1, 0, busScratch, b * 2 + 1, 0, n);
        }
    }
}

// ---------------------------------------------------------------------------
// Runs on the audio thread or a DSP worker thread. Touches only this pad's
// buffer, Lua VM and PadChain, so pads can be processed concurrently.
void Forge64Processor::runPadJob(void* self, int jobIndex)
{
    auto& proc = *static_cast<Forge64Processor*>(self);
    const auto& job = proc.padJobs[(size_t) jobIndex];
    const int p = job.pad;
    const int n = proc.jobBlockSize;
    float* L = proc.padBufL(p);
    float* R = proc.padBufR(p);

    if (job.runLua)
    {
        PadParams ppLua = job.pp;
        ppLua.tune += job.luaTuneOffset;
        proc.luaEngine.process(p, L, R, n, proc.jobSampleRate, job.vel, job.age, ppLua,
                               job.newTrig, job.note);
    }

    proc.chains[(size_t) p].process(L, R, n, job.pp);

    if (job.lastTail)
    {
        const int fadeLen = juce::jmin(n, 64);
        for (int i = 0; i < fadeLen; ++i)
        {
            const float g = 0.5f * (1.0f + std::cos((float) i * juce::MathConstants<float>::pi / (float) fadeLen));
            L[i] *= g;
            R[i] *= g;
        }
        for (int i = fadeLen; i < n; ++i)
        {
            L[i] = 0.f;
            R[i] = 0.f;
        }
    }
}

// ---------------------------------------------------------------------------
void Forge64Processor::onPresetLoaded()
{
    gridPtr->syncAllAtomics();
    gridPtr->reloadSamples();
    modPtr->syncAllFromTrees();

    auto seq = kitRoot.getChildWithName("SEQUENCER");
    if (seq.isValid())
        sequencer.deserialize(seq);

    auto aux = kitRoot.getChildWithName("AUX_MASTER_FX");
    if (aux.isValid())
        auxManager.deserialize(aux);

    for (int p = 0; p < kNumPads; ++p)
    {
        const auto st = gridPtr->padState(p);
        luaEngine.setScript(p,
                            st.getProperty("script", "").toString(),
                            bool(st.getProperty("scriptOn", false)));
    }
    voices.allNotesOff();
}

juce::StringArray Forge64Processor::getFactoryKitNames()
{
    return {
        "01 Clean Electronic (Default)",
        "02 Acoustic Jazz Club",
        "03 Garage Punk 77",
        "04 1960s Experimental Lab",
        "05 1990s Modular Drum Machine",
        "06 Ambient Space Dub"
    };
}

void Forge64Processor::loadFactoryKit(int kitIndex)
{
    auto setAP = [this](int p, const char* base, float val)
    {
        if (auto* par = dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter(padParamId(p, base))))
            par->setValueNotifyingHost(par->convertTo0to1(val));
    };

    // First reset all 64 pads to their default core presets cleanly
    for (int p = 0; p < kNumPads; ++p)
    {
        const auto pre = PadGrid::getDefaultPadPreset(p);
        setAP(p, "tune", pre.tune);
        setAP(p, "dec",  pre.decay);
        setAP(p, "drv",  pre.drive);
        setAP(p, "fx1",  pre.p1);
        setAP(p, "fx2",  pre.p2);
        setAP(p, "fx3",  pre.p3);
        setAP(p, "fx4",  pre.p4);
        setAP(p, "fx5",  pre.p5);
        setAP(p, "snda", 0.0f);
        setAP(p, "sndb", 0.0f);
        setAP(p, "sndc", 0.0f);
        setAP(p, "sndd", 0.0f);
        setAP(p, "lvl",  0.55f);
        setAP(p, "pan",  0.0f);
        setAP(p, "vcft", (float) pre.vcfType);
        setAP(p, "vcfc", pre.vcfCut);
        setAP(p, "vcfr", pre.vcfRes);
        setAP(p, "vcfe", pre.vcfEnv);
        setAP(p, "eqlf", 200.f);  setAP(p, "eqlg", 0.f);
        setAP(p, "eqmf", 1000.f); setAP(p, "eqmg", 0.f);
        setAP(p, "eqhf", 8000.f); setAP(p, "eqhg", 0.f);
        setAP(p, "cthr", 0.f);    setAP(p, "crat", 1.f);
        setAP(p, "catk", 5.f);    setAP(p, "crel", 100.f); setAP(p, "cmg", 0.f);
        setAP(p, "ifx",  0.f);
    }

    // Default Aux FX setup
    auxManager.auxParams[0].fxType = AUX_FX_REVERB;
    auxManager.auxParams[0].p1 = 0.45f;
    auxManager.auxParams[0].p2 = 0.50f;
    auxManager.auxParams[0].p3 = 0.10f;
    auxManager.auxParams[0].p4 = 0.80f;
    auxManager.auxParams[0].returnLevel = 0.50f;
    auxManager.auxParams[0].returnPan = 0.0f;
    auxManager.auxParams[0].enabled = true;

    auxManager.auxParams[1].fxType = AUX_FX_DELAY;
    auxManager.auxParams[1].p1 = 0.17f; // ~250ms
    auxManager.auxParams[1].p2 = 0.25f;
    auxManager.auxParams[1].p3 = 0.50f;
    auxManager.auxParams[1].p4 = 0.0f;  // Free
    auxManager.auxParams[1].returnLevel = 0.35f;
    auxManager.auxParams[1].returnPan = 0.0f;
    auxManager.auxParams[1].enabled = true;

    auxManager.auxParams[2].fxType = AUX_FX_DRIVE;
    auxManager.auxParams[2].p1 = 0.30f;
    auxManager.auxParams[2].p2 = 0.50f;
    auxManager.auxParams[2].p3 = 0.50f;
    auxManager.auxParams[2].p4 = 0.50f;
    auxManager.auxParams[2].returnLevel = 0.0f;
    auxManager.auxParams[2].returnPan = 0.0f;
    auxManager.auxParams[2].enabled = false;

    auxManager.auxParams[3].fxType = AUX_FX_CHORUS;
    auxManager.auxParams[3].p1 = 0.30f;
    auxManager.auxParams[3].p2 = 0.50f;
    auxManager.auxParams[3].p3 = 0.20f;
    auxManager.auxParams[3].p4 = 0.80f;
    auxManager.auxParams[3].returnLevel = 0.0f;
    auxManager.auxParams[3].returnPan = 0.0f;
    auxManager.auxParams[3].enabled = false;

    // Reset Master FX
    auxManager.masterParams.compOn = false;
    auxManager.masterParams.driveOn = false;
    auxManager.masterParams.drive = 0.0f;
    auxManager.masterParams.compThresh = -10.f;
    auxManager.masterParams.compRatio = 2.5f;
    auxManager.masterParams.compAtk = 25.f;
    auxManager.masterParams.compRel = 120.f;
    auxManager.masterParams.compMakeup = 0.f;
    auxManager.masterParams.eqOn = true;
    auxManager.masterParams.eqLowGain = 0.f;
    auxManager.masterParams.eqLowMidGain = 0.f;
    auxManager.masterParams.eqHiMidGain = 0.f;
    auxManager.masterParams.eqHighGain = 0.f;
    auxManager.masterParams.limiterOn = true;
    auxManager.masterParams.ceiling = -0.3f;

    auto setPad = [&](int p, const char* name, float tune, float dec, float drv,
                      float p1, float p2, float p3, float p4, float p5,
                      int vcft = 0, float vcfc = 20000.f, float vcfr = 0.707f, float vcfe = 0.f,
                      float eqlg = 0.f, float eqmg = 0.f, float eqhg = 0.f,
                      float snda = 0.f, float sndb = 0.f, float lvl = 0.55f)
    {
        if (p >= 0 && p < kNumPads && gridPtr != nullptr)
            gridPtr->padState(p).setProperty("name", name, nullptr);

        setAP(p, "tune", tune);
        setAP(p, "dec",  dec);
        setAP(p, "drv",  drv);
        setAP(p, "fx1",  p1);
        setAP(p, "fx2",  p2);
        setAP(p, "fx3",  p3);
        setAP(p, "fx4",  p4);
        setAP(p, "fx5",  p5);
        setAP(p, "vcft", (float) vcft);
        setAP(p, "vcfc", vcfc);
        setAP(p, "vcfr", vcfr);
        setAP(p, "vcfe", vcfe);
        setAP(p, "eqlg", eqlg);
        setAP(p, "eqmg", eqmg);
        setAP(p, "eqhg", eqhg);
        setAP(p, "snda", snda);
        setAP(p, "sndb", sndb);
        setAP(p, "lvl",  lvl);
    };

    switch (kitIndex)
    {
        case 0: // 01 Clean Electronic
        {
            // Crisp, dynamic, uncompressed, crystal clear
            auxManager.masterParams.compOn = false;
            auxManager.masterParams.driveOn = false;
            auxManager.auxParams[0].p1 = 0.38f;
            auxManager.auxParams[0].returnLevel = 0.40f;
            auxManager.auxParams[1].p1 = 0.14f; // ~200 ms
            auxManager.auxParams[1].p2 = 0.20f;
            auxManager.auxParams[1].returnLevel = 0.25f;

            setPad(0,  "808 Sub Kick",        0.0f,  0.55f, 0.00f, 0.65f, 0.35f, 0.70f, 0.30f, 0.20f, 0, 20000.f, 0.707f, 0.f,  1.5f, 0.f, 0.f, 0.00f, 0.00f);
            setPad(1,  "808 Snare",           0.0f,  0.28f, 0.00f, 0.60f, 0.65f, 0.50f, 0.40f, 0.30f, 0, 20000.f, 0.707f, 0.f,  0.0f, 0.f, 0.f, 0.18f, 0.00f);
            setPad(2,  "Closed Hat",          0.0f,  0.055f,0.00f, 0.75f, 0.50f, 0.50f, 0.50f, 0.50f, 0, 16000.f, 0.707f, 0.f,  0.0f, 0.f, 1.0f, 0.00f, 0.00f);
            setPad(3,  "Open Hat",            0.0f,  0.38f, 0.00f, 0.70f, 0.50f, 0.50f, 0.50f, 0.50f, 0, 15000.f, 0.707f, 0.f,  0.0f, 0.f, 1.0f, 0.00f, 0.12f);
            setPad(4,  "808 Handclap",        0.0f,  0.25f, 0.00f, 0.70f, 0.50f, 0.50f, 0.50f, 0.50f, 0, 20000.f, 0.707f, 0.f,  0.0f, 0.f, 0.f, 0.22f, 0.00f);
            setPad(5,  "Low Tom",            -2.0f,  0.35f, 0.00f, 0.50f, 0.50f, 0.50f, 0.50f, 0.50f, 0, 20000.f, 0.707f, 0.f,  1.0f, 0.f, 0.f, 0.05f, 0.00f);
            setPad(6,  "Mid Tom",             0.0f,  0.30f, 0.00f, 0.55f, 0.50f, 0.50f, 0.50f, 0.50f, 0, 20000.f, 0.707f, 0.f,  0.0f, 0.f, 0.f, 0.05f, 0.00f);
            setPad(7,  "Hi Tom",              3.0f,  0.25f, 0.00f, 0.60f, 0.50f, 0.50f, 0.50f, 0.50f, 0, 20000.f, 0.707f, 0.f,  0.0f, 0.f, 0.f, 0.05f, 0.00f);
            setPad(8,  "Maple Rimshot",       0.0f,  0.12f, 0.00f, 0.50f, 0.50f, 0.50f, 0.50f, 0.50f, 0, 20000.f, 0.707f, 0.f,  0.0f, 0.f, 0.f, 0.08f, 0.00f);
            setPad(9,  "808 Cowbell",         0.0f,  0.32f, 0.00f, 0.50f, 0.50f, 0.50f, 0.50f, 0.50f, 0, 20000.f, 0.707f, 0.f,  0.0f, 0.f, 0.f, 0.10f, 0.05f);
            setPad(10, "Sizzle Shaker",       0.0f,  0.09f, 0.00f, 0.80f, 0.50f, 0.50f, 0.50f, 0.50f, 0, 18000.f, 0.707f, 0.f,  0.0f, 0.f, 1.5f, 0.05f, 0.00f);
            setPad(11, "Modal Crash",         0.0f,  1.20f, 0.00f, 0.50f, 0.50f, 0.50f, 0.50f, 0.50f, 0, 20000.f, 0.707f, 0.f,  0.0f, 0.f, 0.f, 0.28f, 0.00f);
            setPad(12, "Ride Bell",           0.0f,  1.50f, 0.00f, 0.65f, 0.50f, 0.50f, 0.50f, 0.50f, 0, 20000.f, 0.707f, 0.f,  0.0f, 0.f, 0.f, 0.15f, 0.00f);
            setPad(13, "Latin Conga",         1.0f,  0.28f, 0.00f, 0.50f, 0.50f, 0.50f, 0.50f, 0.50f, 0, 20000.f, 0.707f, 0.f,  0.0f, 0.f, 0.f, 0.08f, 0.00f);
            setPad(14, "Laser Zap",           0.0f,  0.18f, 0.00f, 0.50f, 0.50f, 0.50f, 0.50f, 0.50f, 0, 20000.f, 0.707f, 0.f,  0.0f, 0.f, 0.f, 0.12f, 0.00f);
            setPad(15, "909 Punch Kick",      1.0f,  0.38f, 0.00f, 0.80f, 0.40f, 0.50f, 0.30f, 0.20f, 0, 20000.f, 0.707f, 0.f,  2.0f, 0.f, 0.f, 0.00f, 0.00f);
            break;
        }

        case 1: // 02 Acoustic Jazz Club
        {
            // Natural room acoustics, warm dynamics, subtle VCA glue
            auxManager.masterParams.compOn = true;
            auxManager.masterParams.compThresh = -14.f;
            auxManager.masterParams.compRatio = 1.8f;
            auxManager.masterParams.compAtk = 30.f;
            auxManager.masterParams.compRel = 180.f;
            auxManager.masterParams.compMakeup = 1.5f;
            auxManager.masterParams.driveOn = false;

            // Wooden club room acoustic curve
            auxManager.masterParams.eqLowGain = 1.5f;
            auxManager.masterParams.eqLowMidGain = -1.2f;
            auxManager.masterParams.eqHiMidGain = 0.8f;
            auxManager.masterParams.eqHighGain = -1.5f;

            auxManager.auxParams[0].p1 = 0.65f;
            auxManager.auxParams[0].p2 = 0.45f;
            auxManager.auxParams[0].p3 = 0.15f;
            auxManager.auxParams[0].returnLevel = 0.42f;

            auxManager.auxParams[1].p1 = 0.05f; // ~85 ms slap
            auxManager.auxParams[1].p2 = 0.15f;
            auxManager.auxParams[1].returnLevel = 0.18f;

            setPad(0,  "Woody Bebop Kick",   -3.5f,  0.32f, 0.00f, 0.40f, 0.20f, 0.30f, 0.60f, 0.20f, 1,  4500.f, 0.707f, 0.f,  2.5f, 0.f,-1.0f, 0.05f, 0.00f);
            setPad(1,  "Coated Snare (Brush)", 1.0f, 0.24f, 0.00f, 0.42f, 0.55f, 0.40f, 0.50f, 0.40f, 1,  9500.f, 0.707f, 0.f,  0.0f, 1.2f, 0.f, 0.30f, 0.00f);
            setPad(2,  "Jazz Hat (Closed)",   -2.0f,  0.06f, 0.00f, 0.45f, 0.40f, 0.40f, 0.40f, 0.40f, 1, 11000.f, 0.707f, 0.f,  0.0f, 0.f,-1.5f, 0.18f, 0.00f);
            setPad(3,  "Sizzle Hat (Half)",   -1.5f,  0.32f, 0.00f, 0.50f, 0.45f, 0.45f, 0.45f, 0.45f, 1, 10500.f, 0.707f, 0.f,  0.0f, 0.f,-1.0f, 0.25f, 0.00f);
            setPad(4,  "Finger Snap / Clap",   2.0f,  0.18f, 0.00f, 0.45f, 0.40f, 0.40f, 0.40f, 0.40f, 0, 14000.f, 0.707f, 0.f,  0.0f, 0.f, 0.f, 0.28f, 0.00f);
            setPad(5,  "16\" Floor Tom",      -4.0f,  0.55f, 0.00f, 0.45f, 0.35f, 0.40f, 0.50f, 0.30f, 1,  6000.f, 0.707f, 0.f,  3.0f, 0.f,-1.0f, 0.35f, 0.00f);
            setPad(6,  "12\" Rack Tom",       -1.0f,  0.45f, 0.00f, 0.50f, 0.40f, 0.45f, 0.50f, 0.35f, 1,  7500.f, 0.707f, 0.f,  1.5f, 0.f,-0.5f, 0.32f, 0.00f);
            setPad(7,  "10\" High Tom",        2.5f,  0.38f, 0.00f, 0.55f, 0.45f, 0.50f, 0.50f, 0.40f, 1,  9000.f, 0.707f, 0.f,  0.0f, 0.f, 0.f, 0.30f, 0.00f);
            setPad(8,  "Cross-Stick Rim",      1.5f,  0.15f, 0.00f, 0.40f, 0.35f, 0.40f, 0.40f, 0.35f, 0, 12000.f, 0.707f, 0.f,  0.0f, 1.5f, 0.f, 0.25f, 0.00f);
            setPad(9,  "Mambo Cowbell",       -1.0f,  0.22f, 0.00f, 0.38f, 0.40f, 0.40f, 0.40f, 0.40f, 0, 10000.f, 0.707f, 0.f,  0.0f, 0.f,-1.0f, 0.15f, 0.00f);
            setPad(10, "Cabasa Shaker",       -1.0f,  0.08f, 0.00f, 0.48f, 0.40f, 0.40f, 0.40f, 0.40f, 0, 13000.f, 0.707f, 0.f,  0.0f, 0.f, 0.f, 0.15f, 0.00f);
            setPad(11, "Dark Melodic Crash",  -2.0f,  1.60f, 0.00f, 0.45f, 0.40f, 0.40f, 0.40f, 0.40f, 1, 12000.f, 0.707f, 0.f,  0.0f, 0.f,-1.5f, 0.45f, 0.00f);
            setPad(12, "Dry Constantinople Ride",-0.5f, 2.40f, 0.00f, 0.55f, 0.45f, 0.45f, 0.45f, 0.45f, 0, 14000.f, 0.707f, 0.f, 0.0f, 0.f, 0.f, 0.42f, 0.12f);
            setPad(13, "Low Tumba Conga",     -2.0f,  0.35f, 0.00f, 0.40f, 0.40f, 0.40f, 0.40f, 0.40f, 0,  8500.f, 0.707f, 0.f,  2.0f, 0.f, 0.f, 0.20f, 0.00f);
            setPad(14, "High Quinto Slap",     3.0f,  0.16f, 0.00f, 0.65f, 0.50f, 0.50f, 0.50f, 0.50f, 0, 11000.f, 0.707f, 0.f,  0.0f, 2.0f, 0.f, 0.22f, 0.00f);
            setPad(15, "Felt Beater Kick",    -4.5f,  0.28f, 0.00f, 0.30f, 0.15f, 0.30f, 0.50f, 0.20f, 1,  3800.f, 0.707f, 0.f,  3.0f, 0.f,-2.0f, 0.05f, 0.00f);
            break;
        }

        case 2: // 03 Garage Punk 77
        {
            // Raw, loud, distorted tape saturation and high-energy compressor pumping
            auxManager.masterParams.driveOn = true;
            auxManager.masterParams.drive = 0.38f;

            auxManager.masterParams.compOn = true;
            auxManager.masterParams.compThresh = -18.f;
            auxManager.masterParams.compRatio = 4.5f;
            auxManager.masterParams.compAtk = 8.f;
            auxManager.masterParams.compRel = 75.f;
            auxManager.masterParams.compMakeup = 3.5f;

            auxManager.masterParams.eqLowGain = 2.5f;
            auxManager.masterParams.eqLowMidGain = -2.0f;
            auxManager.masterParams.eqHiMidGain = 3.2f;
            auxManager.masterParams.eqHighGain = 1.0f;

            auxManager.auxParams[0].fxType = AUX_FX_SPRING;
            auxManager.auxParams[0].p1 = 0.55f;
            auxManager.auxParams[0].p2 = 0.30f;
            auxManager.auxParams[0].returnLevel = 0.28f;

            auxManager.auxParams[1].p1 = 0.08f; // ~120 ms
            auxManager.auxParams[1].p2 = 0.35f;
            auxManager.auxParams[1].returnLevel = 0.22f;

            setPad(0,  "Trash Can Kick",      -1.0f,  0.42f, 0.28f, 0.85f, 0.55f, 0.60f, 0.45f, 0.30f, 0, 20000.f, 0.707f, 0.f,  3.5f, 0.f, 0.f, 0.08f, 0.00f);
            setPad(1,  "Cranked Snare Crack",  2.5f,  0.35f, 0.34f, 0.75f, 0.80f, 0.60f, 0.50f, 0.40f, 0, 20000.f, 0.707f, 0.f,  0.0f, 3.0f, 1.5f, 0.22f, 0.00f);
            setPad(2,  "Chipped Iron Hat",     1.5f,  0.05f, 0.20f, 0.85f, 0.60f, 0.60f, 0.60f, 0.60f, 0, 20000.f, 0.707f, 0.f,  0.0f, 0.f, 2.0f, 0.05f, 0.00f);
            setPad(3,  "Loose Trash Hat",      0.5f,  0.35f, 0.25f, 0.80f, 0.60f, 0.60f, 0.60f, 0.60f, 0, 20000.f, 0.707f, 0.f,  0.0f, 0.f, 1.5f, 0.10f, 0.08f);
            setPad(4,  "Stomp Clatter Clap",  -1.0f,  0.30f, 0.30f, 0.80f, 0.60f, 0.60f, 0.60f, 0.60f, 0, 20000.f, 0.707f, 0.f,  0.0f, 2.0f, 0.f, 0.30f, 0.00f);
            setPad(5,  "Dirty Floor Thud",    -3.0f,  0.48f, 0.22f, 0.65f, 0.50f, 0.55f, 0.55f, 0.40f, 0, 20000.f, 0.707f, 0.f,  3.0f, 0.f, 0.f, 0.15f, 0.00f);
            setPad(6,  "Rattly Mid Tom",       0.0f,  0.38f, 0.20f, 0.65f, 0.50f, 0.55f, 0.55f, 0.40f, 0, 20000.f, 0.707f, 0.f,  1.0f, 1.0f, 0.f, 0.15f, 0.00f);
            setPad(7,  "Ringy High Tom",       3.0f,  0.28f, 0.20f, 0.70f, 0.55f, 0.60f, 0.60f, 0.45f, 0, 20000.f, 0.707f, 0.f,  0.0f, 1.5f, 0.f, 0.15f, 0.00f);
            setPad(8,  "Metal Rimshot Clack",  2.0f,  0.12f, 0.25f, 0.70f, 0.50f, 0.50f, 0.50f, 0.50f, 0, 20000.f, 0.707f, 0.f,  0.0f, 2.0f, 0.f, 0.18f, 0.00f);
            setPad(9,  "Battered Steel Cowbell",1.0f, 0.25f, 0.22f, 0.60f, 0.50f, 0.50f, 0.50f, 0.50f, 0, 20000.f, 0.707f, 0.f,  0.0f, 2.5f, 0.f, 0.15f, 0.00f);
            setPad(10, "Sandpaper Scrape",     0.0f,  0.10f, 0.25f, 0.80f, 0.50f, 0.50f, 0.50f, 0.50f, 0, 20000.f, 0.707f, 0.f,  0.0f, 0.f, 2.0f, 0.10f, 0.00f);
            setPad(11, "Trash China Bash",     1.0f,  0.95f, 0.30f, 0.70f, 0.60f, 0.60f, 0.60f, 0.60f, 0, 20000.f, 0.707f, 0.f,  0.0f, 0.f, 2.0f, 0.32f, 0.00f);
            setPad(12, "Rivet Trash Ride",     0.0f,  1.40f, 0.22f, 0.70f, 0.55f, 0.55f, 0.55f, 0.55f, 0, 20000.f, 0.707f, 0.f,  0.0f, 0.f, 1.0f, 0.20f, 0.00f);
            setPad(13, "Junk Metal Can",      -2.0f,  0.28f, 0.30f, 0.65f, 0.60f, 0.60f, 0.60f, 0.60f, 0, 20000.f, 0.707f, 0.f,  0.0f, 2.0f, 0.f, 0.18f, 0.00f);
            setPad(14, "Screaming Feedback Zap",4.0f, 0.20f, 0.35f, 0.70f, 0.70f, 0.70f, 0.70f, 0.70f, 0, 20000.f, 0.707f, 0.f,  0.0f, 3.0f, 2.0f, 0.22f, 0.25f);
            setPad(15, "Mud Overdrive Kick",  -2.5f,  0.50f, 0.42f, 0.85f, 0.60f, 0.65f, 0.65f, 0.50f, 0, 20000.f, 0.707f, 0.f,  4.0f, 0.f, 0.f, 0.10f, 0.00f);
            break;
        }

        case 3: // 04 1960s Experimental Lab
        {
            // Early electronic / tape radiophonic sound: reel-to-reel echoes, hollow spring reverb, ringing filters
            auxManager.masterParams.driveOn = true;
            auxManager.masterParams.drive = 0.26f;

            auxManager.masterParams.compOn = true;
            auxManager.masterParams.compThresh = -12.f;
            auxManager.masterParams.compRatio = 2.2f;
            auxManager.masterParams.compAtk = 40.f;
            auxManager.masterParams.compRel = 250.f;
            auxManager.masterParams.compMakeup = 1.2f;

            auxManager.masterParams.eqLowGain = -2.0f;
            auxManager.masterParams.eqLowMidGain = 2.2f;
            auxManager.masterParams.eqHiMidGain = 1.5f;
            auxManager.masterParams.eqHighGain = -3.5f;

            auxManager.auxParams[0].fxType = AUX_FX_SPRING;
            auxManager.auxParams[0].p1 = 0.82f;
            auxManager.auxParams[0].p2 = 0.25f;
            auxManager.auxParams[0].returnLevel = 0.45f;

            auxManager.auxParams[1].p1 = 0.25f; // ~360 ms tape delay
            auxManager.auxParams[1].p2 = 0.62f;
            auxManager.auxParams[1].p3 = 0.40f;
            auxManager.auxParams[1].returnLevel = 0.40f;

            setPad(0,  "Radiophonic Tape Thump",-5.0f,0.45f, 0.00f, 0.20f, 0.10f, 0.30f, 0.80f, 0.20f, 1,  1200.f, 2.50f, 0.f, -1.0f, 2.0f,-2.0f, 0.10f, 0.00f);
            setPad(1,  "White Noise Burst",   0.0f,  0.18f, 0.00f, 0.50f, 0.60f, 0.50f, 0.50f, 0.50f, 2,  2400.f, 3.20f, 0.f,  0.0f, 2.5f, 0.f, 0.35f, 0.25f);
            setPad(2,  "Filtered Metallic Blip",7.0f, 0.04f, 0.00f, 0.60f, 0.50f, 0.50f, 0.50f, 0.50f, 2,  4200.f, 4.00f, 0.f,  0.0f, 0.f, 0.f, 0.10f, 0.20f);
            setPad(3,  "Resonant Ringing Hat", 5.0f, 0.25f, 0.00f, 0.55f, 0.50f, 0.50f, 0.50f, 0.50f, 2,  3500.f, 5.00f, 0.f,  0.0f, 0.f, 0.f, 0.15f, 0.35f);
            setPad(4,  "Hollow Spring Clap",   0.0f,  0.22f, 0.00f, 0.50f, 0.50f, 0.50f, 0.50f, 0.50f, 0, 16000.f, 0.707f, 0.f,  0.0f, 0.f, 0.f, 0.50f, 0.15f);
            setPad(5,  "Sub-Harmonic Drone",  -6.0f,  0.75f, 0.00f, 0.30f, 0.20f, 0.30f, 0.60f, 0.40f, 1,   800.f, 3.00f, 0.f,  2.0f, 0.f,-2.0f, 0.20f, 0.25f);
            setPad(6,  "Tape Flutter Tom",    -2.0f,  0.50f, 0.00f, 0.45f, 0.35f, 0.45f, 0.55f, 0.35f, 2,  1400.f, 3.50f, 0.f,  0.0f, 1.5f,-1.0f, 0.20f, 0.30f);
            setPad(7,  "Pitch-Drop Oscillator",4.0f,  0.35f, 0.00f, 0.60f, 0.45f, 0.50f, 0.50f, 0.50f, 1,  2200.f, 4.00f, 0.f,  0.0f, 1.0f, 0.f, 0.20f, 0.35f);
            setPad(8,  "Glass Tube Ting",      9.0f,  0.14f, 0.00f, 0.50f, 0.50f, 0.50f, 0.50f, 0.50f, 2,  5500.f, 4.50f, 0.f,  0.0f, 0.f, 0.f, 0.25f, 0.35f);
            setPad(9,  "Ring-Mod Bell Strike", 2.0f,  0.45f, 0.00f, 0.50f, 0.50f, 0.50f, 0.50f, 0.50f, 3,  1800.f, 4.50f, 0.f,  0.0f, 2.0f, 0.f, 0.20f, 0.45f);
            setPad(10, "Radio Static Burst",   0.0f,  0.12f, 0.00f, 0.65f, 0.50f, 0.50f, 0.50f, 0.50f, 2,  1800.f, 3.80f, 0.f,  0.0f, 1.5f, 0.f, 0.15f, 0.35f);
            setPad(11, "Gong Splash Plate",   -3.0f,  2.20f, 0.00f, 0.45f, 0.40f, 0.40f, 0.40f, 0.40f, 1,  8000.f, 2.50f, 0.f,  0.0f, 0.f,-2.0f, 0.55f, 0.35f);
            setPad(12, "Sawtooth Ping Cymbal", 3.0f,  1.80f, 0.00f, 0.55f, 0.45f, 0.45f, 0.45f, 0.45f, 1,  6500.f, 2.00f, 0.f,  0.0f, 0.f,-1.0f, 0.30f, 0.40f);
            setPad(13, "Wooden Log Drum",     -1.0f,  0.38f, 0.00f, 0.40f, 0.25f, 0.35f, 0.50f, 0.30f, 1,  1100.f, 3.00f, 0.f,  1.5f, 0.f,-2.0f, 0.20f, 0.20f);
            setPad(14, "Theremin Laser Zap",   5.0f,  0.28f, 0.00f, 0.60f, 0.60f, 0.60f, 0.60f, 0.60f, 1,  2200.f, 4.20f, 0.f,  0.0f, 2.0f, 0.f, 0.25f, 0.50f);
            setPad(15, "Impulse Click Kick",  -7.0f,  0.30f, 0.00f, 0.90f, 0.80f, 0.50f, 0.40f, 0.20f, 1,  2800.f, 1.50f, 0.f,  3.0f, 0.f, 0.f, 0.10f, 0.00f);
            break;
        }

        case 4: // 05 1990s Modular Drum Machine
        {
            // West Coast & Eurorack modular: snappy envelopes, punchy VCA, stereo delay, sub impact
            auxManager.masterParams.compOn = true;
            auxManager.masterParams.compThresh = -13.f;
            auxManager.masterParams.compRatio = 3.2f;
            auxManager.masterParams.compAtk = 15.f;
            auxManager.masterParams.compRel = 95.f;
            auxManager.masterParams.compMakeup = 2.0f;

            auxManager.masterParams.driveOn = true;
            auxManager.masterParams.drive = 0.18f;

            auxManager.masterParams.eqLowGain = 3.0f;
            auxManager.masterParams.eqLowMidGain = -1.0f;
            auxManager.masterParams.eqHiMidGain = 1.8f;
            auxManager.masterParams.eqHighGain = 2.2f;

            auxManager.auxParams[0].p1 = 0.72f;
            auxManager.auxParams[0].p2 = 0.40f;
            auxManager.auxParams[0].returnLevel = 0.30f;

            auxManager.auxParams[1].p1 = 0.13f; // ~187 ms
            auxManager.auxParams[1].p2 = 0.45f;
            auxManager.auxParams[1].returnLevel = 0.28f;

            setPad(0,  "Eurorack VCF Sub Kick",2.0f,  0.48f, 0.12f, 0.88f, 0.45f, 0.75f, 0.40f, 0.20f, 0, 20000.f, 0.707f, 0.f,  4.0f, 0.f, 0.f, 0.00f, 0.00f);
            setPad(1,  "Analog Noise Snare",  -1.0f,  0.22f, 0.15f, 0.65f, 0.75f, 0.55f, 0.45f, 0.30f, 0, 20000.f, 0.707f, 0.f,  0.0f, 1.5f, 1.0f, 0.20f, 0.00f);
            setPad(2,  "Clocked Linear FM Hat",3.0f,  0.045f,0.05f, 0.85f, 0.55f, 0.55f, 0.55f, 0.55f, 0, 15000.f, 0.707f, 0.f,  0.0f, 0.f, 1.5f, 0.05f, 0.00f);
            setPad(3,  "VCA Ringing Open Hat", 2.0f,  0.32f, 0.08f, 0.78f, 0.55f, 0.55f, 0.55f, 0.55f, 0, 16000.f, 0.707f, 0.f,  0.0f, 0.f, 1.0f, 0.08f, 0.25f);
            setPad(4,  "Dual Impulse Clap",    1.0f,  0.20f, 0.10f, 0.80f, 0.55f, 0.55f, 0.55f, 0.55f, 0, 20000.f, 0.707f, 0.f,  0.0f, 1.5f, 0.f, 0.22f, 0.00f);
            setPad(5,  "Pinged Ladder Tom Low",-2.0f, 0.42f, 0.10f, 0.55f, 0.45f, 0.50f, 0.50f, 0.35f, 1,  3500.f, 2.50f, 0.f,  2.0f, 0.f, 0.f, 0.10f, 0.30f);
            setPad(6,  "Pinged Ladder Tom Mid",1.0f,  0.34f, 0.10f, 0.60f, 0.50f, 0.55f, 0.55f, 0.40f, 1,  4200.f, 2.50f, 0.f,  1.0f, 0.f, 0.f, 0.10f, 0.15f);
            setPad(7,  "Pinged Ladder Tom Hi", 4.0f,  0.26f, 0.10f, 0.65f, 0.55f, 0.60f, 0.60f, 0.45f, 1,  5500.f, 2.50f, 0.f,  0.0f, 0.f, 0.f, 0.10f, 0.15f);
            setPad(8,  "Resonance Rim Click",  5.0f,  0.08f, 0.12f, 0.65f, 0.50f, 0.50f, 0.50f, 0.50f, 2,  4800.f, 4.00f, 0.f,  0.0f, 2.0f, 0.f, 0.12f, 0.00f);
            setPad(9,  "Bipolar FM Bell",      2.0f,  0.38f, 0.15f, 0.60f, 0.50f, 0.50f, 0.50f, 0.50f, 0, 18000.f, 0.707f, 0.f,  0.0f, 1.5f, 1.0f, 0.15f, 0.35f);
            setPad(10, "Stochastic Noise Shaker",0.0f,0.07f, 0.10f, 0.75f, 0.50f, 0.50f, 0.50f, 0.50f, 0, 18000.f, 0.707f, 0.f,  0.0f, 0.f, 1.5f, 0.05f, 0.10f);
            setPad(11, "Metallic Phase Crash", 0.0f,  1.10f, 0.15f, 0.55f, 0.50f, 0.50f, 0.50f, 0.50f, 0, 20000.f, 0.707f, 0.f,  0.0f, 0.f, 1.0f, 0.35f, 0.15f);
            setPad(12, "West Coast Folded Ride",1.5f, 1.60f, 0.14f, 0.70f, 0.50f, 0.50f, 0.50f, 0.50f, 0, 20000.f, 0.707f, 0.f,  0.0f, 0.f, 1.0f, 0.22f, 0.20f);
            setPad(13, "Pinged Resonator Perc", 3.0f, 0.25f, 0.15f, 0.55f, 0.50f, 0.50f, 0.50f, 0.50f, 0, 12000.f, 3.50f, 0.f,  0.0f, 1.5f, 0.f, 0.18f, 0.25f);
            setPad(14, "VCO FM Laser Zap",     3.0f,  0.18f, 0.20f, 0.65f, 0.60f, 0.60f, 0.60f, 0.60f, 0, 20000.f, 0.707f, 0.f,  0.0f, 2.0f, 1.5f, 0.20f, 0.35f);
            setPad(15, "S&H Glitch Impact Kick",4.0f, 0.35f, 0.25f, 0.95f, 0.60f, 0.65f, 0.50f, 0.30f, 0, 20000.f, 0.707f, 0.f,  3.5f, 0.f, 0.f, 0.00f, 0.00f);
            break;
        }

        case 5: // 06 Ambient Space Dub
        {
            // Deep cavernous abyss reverb, long regenerative ping-pong delay, seismic sub-bass
            auxManager.masterParams.compOn = true;
            auxManager.masterParams.compThresh = -16.f;
            auxManager.masterParams.compRatio = 2.4f;
            auxManager.masterParams.compAtk = 35.f;
            auxManager.masterParams.compRel = 300.f;
            auxManager.masterParams.compMakeup = 2.5f;

            auxManager.masterParams.driveOn = true;
            auxManager.masterParams.drive = 0.22f;

            auxManager.masterParams.eqLowGain = 4.0f;
            auxManager.masterParams.eqLowMidGain = -2.5f;
            auxManager.masterParams.eqHiMidGain = 0.0f;
            auxManager.masterParams.eqHighGain = 1.5f;

            auxManager.auxParams[0].fxType = AUX_FX_PLATE;
            auxManager.auxParams[0].p1 = 0.85f;
            auxManager.auxParams[0].p2 = 0.60f;
            auxManager.auxParams[0].p3 = 0.40f;
            auxManager.auxParams[0].p4 = 0.75f;
            auxManager.auxParams[0].returnLevel = 0.52f;

            auxManager.auxParams[1].fxType = AUX_FX_PINGPONG;
            auxManager.auxParams[1].p1 = 0.30f; // ~428 ms
            auxManager.auxParams[1].p2 = 0.78f;
            auxManager.auxParams[1].p3 = 0.40f;
            auxManager.auxParams[1].p4 = 0.0f;
            auxManager.auxParams[1].returnLevel = 0.48f;

            setPad(0,  "Deep Sub Abyss Kick", -4.0f,  0.95f, 0.00f, 0.45f, 0.25f, 0.90f, 0.60f, 0.25f, 0, 20000.f, 0.707f, 0.f,  5.0f, 0.f,-2.0f, 0.08f, 0.00f);
            setPad(1,  "Echo Chamber Snare",   0.0f,  0.35f, 0.00f, 0.55f, 0.65f, 0.50f, 0.40f, 0.30f, 0, 20000.f, 0.707f, 0.f,  0.0f, 1.5f, 0.f, 0.45f, 0.60f);
            setPad(2,  "Ticked Delay Hat",     1.0f,  0.05f, 0.00f, 0.70f, 0.50f, 0.50f, 0.50f, 0.50f, 0, 15000.f, 0.707f, 0.f,  0.0f, 0.f, 0.f, 0.15f, 0.40f);
            setPad(3,  "Sustained Airy Hat",   0.0f,  0.45f, 0.00f, 0.65f, 0.50f, 0.50f, 0.50f, 0.50f, 0, 16000.f, 0.707f, 0.f,  0.0f, 0.f, 1.0f, 0.50f, 0.45f);
            setPad(4,  "Reverberant Hall Clap",-1.0f, 0.35f, 0.00f, 0.65f, 0.50f, 0.50f, 0.50f, 0.50f, 0, 20000.f, 0.707f, 0.f,  0.0f, 0.f, 0.f, 0.55f, 0.30f);
            setPad(5,  "Cavernous Floor Tom", -3.0f,  0.65f, 0.00f, 0.45f, 0.35f, 0.50f, 0.55f, 0.35f, 0, 20000.f, 0.707f, 0.f,  4.0f, 0.f, 0.f, 0.45f, 0.25f);
            setPad(6,  "Low Reso Dub Tom",    -1.0f,  0.50f, 0.00f, 0.50f, 0.40f, 0.50f, 0.55f, 0.35f, 0, 20000.f, 0.707f, 0.f,  2.0f, 0.f, 0.f, 0.40f, 0.35f);
            setPad(7,  "Echoing Mid Tom",      2.0f,  0.40f, 0.00f, 0.55f, 0.45f, 0.55f, 0.55f, 0.40f, 0, 20000.f, 0.707f, 0.f,  1.0f, 0.f, 0.f, 0.40f, 0.45f);
            setPad(8,  "Spring Dub Rimshot",   0.0f,  0.14f, 0.00f, 0.45f, 0.40f, 0.40f, 0.40f, 0.40f, 0, 16000.f, 0.707f, 0.f,  0.0f, 2.0f, 0.f, 0.40f, 0.55f);
            setPad(9,  "Dub Delay Cowbell",   -1.0f,  0.35f, 0.00f, 0.45f, 0.40f, 0.40f, 0.40f, 0.40f, 0, 14000.f, 0.707f, 0.f,  0.0f, 1.5f, 0.f, 0.50f, 0.70f);
            setPad(10, "Space Shaker Swarm",   0.0f,  0.12f, 0.00f, 0.70f, 0.45f, 0.45f, 0.45f, 0.45f, 0, 15000.f, 0.707f, 0.f,  0.0f, 0.f, 1.5f, 0.35f, 0.30f);
            setPad(11, "Cloud Wash Crash",    -1.0f,  2.50f, 0.00f, 0.50f, 0.45f, 0.45f, 0.45f, 0.45f, 0, 16000.f, 0.707f, 0.f,  0.0f, 0.f, 0.f, 0.70f, 0.40f);
            setPad(12, "Endless Tape Ride",    0.0f,  2.80f, 0.00f, 0.60f, 0.50f, 0.50f, 0.50f, 0.50f, 0, 18000.f, 0.707f, 0.f,  0.0f, 0.f, 0.f, 0.50f, 0.45f);
            setPad(13, "Deep Bongo Hit",       1.0f,  0.30f, 0.00f, 0.45f, 0.40f, 0.40f, 0.40f, 0.40f, 0, 12000.f, 0.707f, 0.f,  2.0f, 0.f, 0.f, 0.35f, 0.35f);
            setPad(14, "Filter Sweep Zap",    -2.0f,  0.45f, 0.00f, 0.55f, 0.50f, 0.50f, 0.50f, 0.50f, 1,  2800.f, 3.00f, 0.f,  0.0f, 2.0f, 0.f, 0.30f, 0.60f);
            setPad(15, "Infra-Sub Boom",      -6.0f,  1.20f, 0.00f, 0.30f, 0.15f, 0.95f, 0.70f, 0.30f, 0, 20000.f, 0.707f, 0.f,  6.0f, 0.f,-3.0f, 0.50f, 0.20f);
            break;
        }

        default:
            break;
    }

    onPresetLoaded();
}

void Forge64Processor::resetPadMidiNote(int pad)
{
    if (pad >= 0 && pad < kNumPads)
    {
        if (auto* param = apvts.getParameter(padParamId(pad, "mnote")))
        {
            const float defNote = (float) juce::jmin(127, 36 + pad);
            param->setValueNotifyingHost(param->convertTo0to1(defNote));
        }
    }
}

void Forge64Processor::resetBankMidiNotes(int bankIndex)
{
    if (bankIndex >= 0 && bankIndex < kNumBanks)
    {
        const int start = bankIndex * kPadsPerBank;
        for (int p = start; p < start + kPadsPerBank; ++p)
            resetPadMidiNote(p);
    }
}

void Forge64Processor::resetAllMidiNotes()
{
    for (int p = 0; p < kNumPads; ++p)
        resetPadMidiNote(p);
}

void Forge64Processor::getStateInformation(juce::MemoryBlock& destData)
{
    // Save UI dimensions
    kitRoot.setProperty("uiWidth", lastUIWidth, nullptr);
    kitRoot.setProperty("uiHeight", lastUIHeight, nullptr);

    // Save midi learn, sequencer and aux FX into kitRoot before serializing
    kitRoot.removeChild(kitRoot.getChildWithName("MIDI_LEARN"), nullptr);
    kitRoot.appendChild(midiLearn.serialize(), nullptr);
    kitRoot.removeChild(kitRoot.getChildWithName("SEQUENCER"), nullptr);
    kitRoot.appendChild(sequencer.serialize(), nullptr);
    kitRoot.removeChild(kitRoot.getChildWithName("AUX_MASTER_FX"), nullptr);
    kitRoot.appendChild(auxManager.serialize(), nullptr);

    if (auto xml = kitRoot.createXml())
    {
        juce::MemoryOutputStream os(destData, false);
        xml->writeTo(os, {});
    }
}

void Forge64Processor::setStateInformation(const void* data, int sizeInBytes)
{
    auto xml = juce::XmlDocument::parse(juce::String::fromUTF8((const char*) data, (size_t) sizeInBytes));
    if (! xml || xml->getTagName() != kitRoot.getType().toString())
        return;

    auto incoming = juce::ValueTree::fromXml(*xml);
    if (! incoming.isValid())
        return;

    if (incoming.hasProperty("uiWidth"))
        lastUIWidth = (int) incoming.getProperty("uiWidth", 1200);
    if (incoming.hasProperty("uiHeight"))
        lastUIHeight = (int) incoming.getProperty("uiHeight", 800);

    auto params = incoming.getChildWithName("PARAMS");
    if (params.isValid())
        apvts.replaceState(params);

    for (const char* name : { "PADS", "MODSRC", "MODMAT" })
    {
        auto src = incoming.getChildWithName(name);
        auto dst = kitRoot.getChildWithName(name);
        if (src.isValid() && dst.isValid())
            PresetManager::copyTreeInPlace(dst, src);
    }

    auto ml = incoming.getChildWithName("MIDI_LEARN");
    if (ml.isValid())
        midiLearn.deserialize(ml);

    auto seq = incoming.getChildWithName("SEQUENCER");
    if (seq.isValid())
    {
        sequencer.deserialize(seq);
        kitRoot.removeChild(kitRoot.getChildWithName("SEQUENCER"), nullptr);
        kitRoot.appendChild(seq.createCopy(), nullptr);
    }

    auto aux = incoming.getChildWithName("AUX_MASTER_FX");
    if (aux.isValid())
    {
        auxManager.deserialize(aux);
        kitRoot.removeChild(kitRoot.getChildWithName("AUX_MASTER_FX"), nullptr);
        kitRoot.appendChild(aux.createCopy(), nullptr);
    }

    onPresetLoaded();
}

juce::AudioProcessorEditor* Forge64Processor::createEditor()
{
    return new Forge64Editor(*this);
}

} // namespace f64

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new f64::Forge64Processor();
}
