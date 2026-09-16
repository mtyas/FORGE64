#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

namespace f64 {

static_assert(kNumPadParams == 28, "PadParams field mapping below must match kPadParams order");

// ---------------------------------------------------------------------------
// Buses: main stereo + 15 additional stereo pairs (16 total), each pad
// assignable to any of them.
// ---------------------------------------------------------------------------
juce::AudioProcessor::BusesProperties Forge64Processor::makeBuses()
{
    BusesProperties b;
    b.withOutput("Main", juce::AudioChannelSet::stereo(), true);
    for (int i = 1; i < kNumBuses; ++i)
        b.withOutput("Out " + juce::String(i + 1), juce::AudioChannelSet::stereo(), false);
    return b;
}

// ---------------------------------------------------------------------------
// Parameters: globals + 28 per pad x 64 pads.
// ---------------------------------------------------------------------------
juce::AudioProcessorParameters Forge64Processor::makeParams()
{
    juce::AudioProcessorParameters params;

    auto addF = [&params](juce::StringRef id, juce::StringRef name,
                          juce::NormalisableRange<float> range, float def)
    {
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::AudioParameterFloat::Attributes { juce::ParameterID { id, 1 } }.withName(name),
            range, def));
    };
    auto addI = [&params](juce::StringRef id, juce::StringRef name, int mn, int mx, int def)
    {
        params.push_back(std::make_unique<juce::AudioParameterInt>(
            juce::AudioParameterInt::Attributes { juce::ParameterID { id, 1 } }.withName(name),
            mn, mx, def));
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

        addF(id("lvl"),  n + "Level",     { 0.f, 1.f, 0.001f },              0.8f);
        addF(id("pan"),  n + "Pan",       { -1.f, 1.f, 0.001f },             0.f);
        addF(id("tune"), n + "Tune",      { -24.f, 24.f, 0.01f },            0.f);
        addF(id("dec"),  n + "Decay",     { 0.005f, 8.f, 0.001f, 0.25f },    1.f);
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
        addF(id("drv"),  n + "Drive",     { 0.f, 1.f, 0.001f },              0.f);
        addI(id("fx"),   n + "FX Type",   0, 4, 0);
        addF(id("fx1"),  n + "FX P1",     { 0.f, 1.f, 0.001f },              0.5f);
        addF(id("fx2"),  n + "FX P2",     { 0.f, 1.f, 0.001f },              0.5f);
        addF(id("fx3"),  n + "FX P3",     { 0.f, 1.f, 0.001f },              0.5f);
        addF(id("fx4"),  n + "FX P4",     { 0.f, 1.f, 0.001f },              0.5f);
        addF(id("snda"), n + "Send A",    { 0.f, 1.f, 0.001f },              0.f);
        addF(id("sndb"), n + "Send B",    { 0.f, 1.f, 0.001f },              0.f);
        addI(id("chok"), n + "Choke",     0, kNumChokes, 0);
        addI(id("obus"), n + "Out Bus",   1, kNumBuses, 1);
        addI(id("pmode"), n + "Mode",     0, 1, 0);
        addI(id("mchan"), n + "MIDI Ch",  0, 16, 0);
        addI(id("mnote"), n + "MIDI Note", 0, 127, juce::jmin(127, 36 + p));
        addI(id("src"),  n + "Source",    0, 1, 0);
    }

    return params;
}

// ---------------------------------------------------------------------------
Forge64Processor::Forge64Processor()
    : AudioProcessor(makeBuses()),
      apvts(*this, nullptr, "PARAMS", makeParams())
{
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

    voices.setDeps(gridPtr.get(), modPtr.get());

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
}

Forge64Processor::~Forge64Processor() = default;

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
    modPtr->prepare(sr, maxBlock);
    voices.prepare(sr, maxBlock);

    busScratch.setSize(2 * kNumBuses, maxBlock, false, false, true);
    auxA.setSize(2, maxBlock, false, false, true);
    auxB.setSize(2, maxBlock, false, false, true);
    scratchL.assign((size_t) maxBlock, 0.f);
    scratchR.assign((size_t) maxBlock, 0.f);

    busOffset.fill(0);
    busActive.fill(false);
    int off = 0;
    const auto& lay = getActiveLayout();
    for (int b = 0; b < kNumBuses; ++b)
    {
        const int ch = lay.getNumChannelsForBus(false, b);
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
    auto* par = globalPtrs[(size_t) idx];
    if (par == nullptr)
        return 0.f;
    static const std::string ids[GI_Count] = { "master", "revsize", "revdamp", "dlytime", "dlyfb" };
    const float v = par->getValue() + modPtr->offsetFor(ids[(size_t) idx]);
    return par->convertFrom0to1(juce::limitRange(v, 0.f, 1.f));
}

void Forge64Processor::fillPadParams(int pad, PadParams& out)
{
    auto& ptrs = padPtrs[(size_t) pad];
    auto& ids = padIds[(size_t) pad];

    auto getF = [&](int k) -> float
    {
        auto* par = ptrs[(size_t) k];
        if (par == nullptr)
            return 0.f;
        const float v = par->getValue() + modPtr->offsetFor(ids[(size_t) k]);
        return par->convertFrom0to1(juce::limitRange(v, 0.f, 1.f));
    };
    auto getI = [&](int k) -> int { return (int) std::lround((double) getF(k)); };

    out.level = getF(0);  out.pan = getF(1);  out.tune = getF(2);  out.decay = getF(3);
    out.eqLF = getF(4);   out.eqLG = getF(5); out.eqMF = getF(6);  out.eqMG = getF(7);
    out.eqHF = getF(8);   out.eqHG = getF(9);
    out.cThr = getF(10);  out.cRat = getF(11); out.cAtk = getF(12); out.cRel = getF(13);
    out.drive = getF(14);
    out.fxType = getI(15);
    out.fx1 = getF(16);   out.fx2 = getF(17); out.fx3 = getF(18);  out.fx4 = getF(19);
    out.sendA = getF(20); out.sendB = getF(21);
    out.choke = getI(22); out.outBus = getI(23);
    out.mode = getI(24);  out.mchan = getI(25); out.mnote = getI(26); out.srcType = getI(27);
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
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto t = pos->getTempo())     bpm = *t;
            if (auto p = pos->getIsPlaying()) playing = *p;
        }
    modPtr->setTempo(bpm, playing);

    busScratch.setSize(2 * kNumBuses, n, false, false, true);
    busScratch.clear();
    auxA.setSize(2, n, false, false, true);
    auxA.clear();
    auxB.setSize(2, n, false, false, true);
    auxB.clear();
    if ((int) scratchL.size() < n)
    {
        scratchL.resize((size_t) n);
        scratchR.resize((size_t) n);
    }

    // 1) Latch MIDI modulation sources and collect timed note events.
    rawEvents.clear();
    for (const auto& meta : midi)
    {
        const auto& m = meta.getMessage();
        modPtr->handleMidiMessage(m);

        if (m.isProgramChange())
            currentBank.store(m.getProgramChangeNumber() % kNumBanks);
        if (m.isAllNotesOff() || m.isAllSoundOff())
            voices.allNotesOff();

        if (m.isNoteOn() || m.isNoteOff())
        {
            RawMidiEv e;
            e.pos = juce::limitRange(meta.samplePosition, 0, n - 1);
            e.vel = m.getFloatVelocity();
            e.note = m.getNoteNumber();
            e.chan = m.getChannel();
            e.off = m.isNoteOff();
            rawEvents.push_back(e);
        }
    }

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
            noteMap[(size_t) juce::limitRange(e.mnote, 0, 127)].push_back(p);
        else if (e.mchan > 0)
            chromMap[(size_t) e.mchan] = p;
    }

    for (int p = 0; p < kNumPads; ++p)
        padEvents[(size_t) p].clear();

    for (const auto& re : rawEvents)
    {
        if (! re.off)
        {
            const int cp = chromMap[(size_t) juce::limitRange(re.chan, 0, 16)];
            if (cp >= 0)
            {
                padEvents[(size_t) cp].push_back({ re.pos, { cp, re.vel, re.note, re.chan, false } });
            }
            else
            {
                for (int p : noteMap[(size_t) juce::limitRange(re.note, 0, 127)])
                    if (eff[(size_t) p].mchan == 0 || eff[(size_t) p].mchan == re.chan)
                        padEvents[(size_t) p].push_back({ re.pos, { p, re.vel, re.note, re.chan, false } });
            }
        }
        else
        {
            for (int p = 0; p < kNumPads; ++p)
            {
                const auto& e = eff[(size_t) p];
                if (e.mode == 1 && (e.mchan == 0 || e.mchan == re.chan))
                    padEvents[(size_t) p].push_back({ re.pos, { p, 0.f, re.note, re.chan, true } });
            }
        }
    }

    // 5) Render pads: voices -> optional Lua -> pad chain -> bus/aux routing.
    const double sr = getSampleRate();
    const float master = globalEff(GI_Master);

    for (int p = 0; p < kNumPads; ++p)
    {
        const bool hasEvents = ! padEvents[(size_t) p].empty();
        if (! hasEvents && ! voices.padActive(p))
            continue;

        std::fill(scratchL.begin(), scratchL.begin() + n, 0.f);
        std::fill(scratchR.begin(), scratchR.begin() + n, 0.f);

        const auto& pp = eff[(size_t) p];
        voices.renderPad(p, scratchL.data(), scratchR.data(), n, sr, pp, padEvents[(size_t) p]);

        auto& rt = gridPtr->runtime(p);
        if (rt.scriptOn.load())
        {
            const int64_t hit = rt.lastHitStamp.load();
            const double age = hit > 0 ? (double) (clockNow - hit) / sr : 0.0;
            luaEngine.process(p, scratchL.data(), scratchR.data(), n, sr,
                              rt.lastVel.load(), juce::jmax(0.0, age), pp);
        }

        const float gt = rt.gainTrim.load();
        chains[(size_t) p].process(scratchL.data(), scratchR.data(), n, pp);

        const float lvl = pp.level * master * gt;
        const int bus = juce::limitRange(pp.outBus, 1, kNumBuses) - 1;
        float* bL = busScratch.getWritePointer(bus * 2);
        float* bR = busScratch.getWritePointer(bus * 2 + 1);
        float* aL = auxA.getWritePointer(0);
        float* aR = auxA.getWritePointer(1);
        float* dL = auxB.getWritePointer(0);
        float* dR = auxB.getWritePointer(1);
        const float sa = pp.sendA, sb = pp.sendB;

        for (int i = 0; i < n; ++i)
        {
            const float l = scratchL[(size_t) i] * lvl;
            const float r = scratchR[(size_t) i] * lvl;
            bL[i] += l;
            bR[i] += r;
            if (sa > 0.f) { aL[i] += l * sa; aR[i] += r * sa; }
            if (sb > 0.f) { dL[i] += l * sb; dR[i] += r * sb; }
        }
    }

    // 6) Global FX buses (fully wet) return into the main bus.
    gfx.processReverb(auxA.getWritePointer(0), auxA.getWritePointer(1), n,
                      globalEff(GI_RevSize), globalEff(GI_RevDamp));
    gfx.processDelay(auxB.getWritePointer(0), auxB.getWritePointer(1), n,
                     globalEff(GI_DlyTime), globalEff(GI_DlyFb));
    {
        float* mL = busScratch.getWritePointer(0);
        float* mR = busScratch.getWritePointer(1);
        const float* aL = auxA.getReadPointer(0);
        const float* aR = auxA.getReadPointer(1);
        const float* dL = auxB.getReadPointer(0);
        const float* dR = auxB.getReadPointer(1);
        for (int i = 0; i < n; ++i)
        {
            mL[i] += aL[i] + dL[i];
            mR[i] += aR[i] + dR[i];
        }
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
void Forge64Processor::onPresetLoaded()
{
    gridPtr->syncAllAtomics();
    gridPtr->reloadSamples();
    modPtr->syncAllFromTrees();

    for (int p = 0; p < kNumPads; ++p)
    {
        const auto st = gridPtr->padState(p);
        luaEngine.setScript(p,
                            st.getProperty("script", "").toString(),
                            bool(st.getProperty("scriptOn", false)));
    }
    voices.allNotesOff();
}

void Forge64Processor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto xml = juce::createXmlFromValueTree(kitRoot))
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

    auto incoming = juce::parseXmlRecursively(*xml);
    if (! incoming.isValid())
        return;

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
