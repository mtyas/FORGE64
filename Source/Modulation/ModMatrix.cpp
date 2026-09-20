#include "ModMatrix.h"

namespace f64 {

namespace {
    const std::string idVoiceAmp   = kDestVoiceAmp;
    const std::string idVoicePitch = kDestVoicePitch;
    const std::string idVoicePan   = kDestVoicePan;
}

ModMatrix::ModMatrix(juce::ValueTree srcTreeIn, juce::ValueTree matTreeIn,
                     juce::AudioProcessorValueTreeState& apvtsIn)
    : srcTree(srcTreeIn), matTree(matTreeIn), apvts(apvtsIn)
{
    sources.resize((size_t) kNumSlots);

    for (int i = 0; i < kNumLFO; ++i)
    {
        auto s = std::make_unique<LFOSource>();
        s->state = srcTree.getChild(slotLFO(i));
        sources[(size_t) slotLFO(i)] = std::move(s);
    }
    for (int i = 0; i < kNumRnd; ++i)
    {
        auto s = std::make_unique<RandomSource>();
        s->state = srcTree.getChild(slotRnd(i));
        sources[(size_t) slotRnd(i)] = std::move(s);
    }
    for (int i = 0; i < kNumEnv; ++i)
    {
        auto s = std::make_unique<EnvSource>();
        s->state = srcTree.getChild(slotEnv(i));
        sources[(size_t) slotEnv(i)] = std::move(s);
    }
    for (int i = 0; i < kNumSeq; ++i)
    {
        auto s = std::make_unique<SeqSource>();
        s->state = srcTree.getChild(slotSeq(i));
        sources[(size_t) slotSeq(i)] = std::move(s);
    }
    for (int k = 0; k < kNumMidiSrc; ++k)
        sources[(size_t) slotMidi(k)] = std::make_unique<MidiSource>(k);
    for (int i = 0; i < kNumMacros; ++i)
    {
        sources[(size_t) slotMacro(i)] = std::make_unique<MacroSource>(i);
        macroParams[(size_t) i] = apvts.getRawParameterValue("m" + juce::String(i));
    }

    srcBuf.resize((size_t) kNumSlots);
    syncAllFromTrees();

    srcTree.addListener(this);
    matTree.addListener(this);
    rebuildCache();
}

ModMatrix::~ModMatrix()
{
    srcTree.removeListener(this);
    matTree.removeListener(this);
}

void ModMatrix::fillDefaultTrees(juce::ValueTree& srcOut, juce::ValueTree& matOut)
{
    srcOut = juce::ValueTree("MODSRC");
    for (int i = 0; i < kNumLFO; ++i) srcOut.appendChild(LFOSource::makeDefault(), nullptr);
    for (int i = 0; i < kNumRnd; ++i) srcOut.appendChild(RandomSource::makeDefault(), nullptr);
    for (int i = 0; i < kNumEnv; ++i) srcOut.appendChild(EnvSource::makeDefault(), nullptr);
    for (int i = 0; i < kNumSeq; ++i) srcOut.appendChild(SeqSource::makeDefault(), nullptr);
    matOut = juce::ValueTree("MODMAT");
}

// ---------------------------------------------------------------------------
// Audio thread
// ---------------------------------------------------------------------------
void ModMatrix::prepare(double sr, int maxBlock)
{
    for (int s = 0; s < kNumSlots; ++s)
    {
        sources[(size_t) s]->prepare(sr, maxBlock);
        srcBuf[(size_t) s].assign((size_t) maxBlock, 0.f);
    }
}

void ModMatrix::setTempo(double bpm, bool isPlaying)
{
    tempo = juce::jlimit(20.0, 400.0, bpm);
    transportPlaying = isPlaying;
    for (auto& s : sources)
        s->setTempo(tempo);
}

void ModMatrix::handleMidiMessage(const juce::MidiMessage& m)
{
    if (m.isNoteOn())
        midiLatch[0] = m.getFloatVelocity();
    else if (m.isController() && m.getControllerNumber() == 1)
        midiLatch[1] = (float) m.getControllerValue() / 127.f;
    else if (m.isPitchWheel())
        midiLatch[2] = (float) (m.getPitchWheelValue() - 8192) / 8192.f;
    else if (m.isChannelPressure())
        midiLatch[3] = (float) m.getChannelPressureValue() / 127.f;
    else if (m.isAftertouch())
        midiLatch[4] = (float) m.getAfterTouchValue() / 127.f;
}

void ModMatrix::renderSources(int n)
{
    cacheLive = cacheSnapshot();

    for (int i = 0; i < kNumMacros; ++i)
        if (macroParams[(size_t) i] != nullptr)
            static_cast<MacroSource*>(sources[(size_t) slotMacro(i)].get())
                ->setValue(macroParams[(size_t) i]->load());

    for (int k = 0; k < kNumMidiSrc; ++k)
        static_cast<MidiSource*>(sources[(size_t) slotMidi(k)].get())
            ->setValue(midiLatch[(size_t) k].load());

    for (int s = 0; s < kNumSlots; ++s)
    {
        auto& buf = srcBuf[(size_t) s];
        if ((int) buf.size() < n)
            buf.resize((size_t) n, 0.f);

        sources[(size_t) s]->render(buf.data(), n);

        float sum = 0.f;
        for (int i = 0; i < n; ++i)
            sum += buf[(size_t) i];
        srcAvg[(size_t) s] = n > 0 ? sum / (float) n : 0.f;
    }
}

float ModMatrix::shapeCurve(float v, float c)
{
    if (std::abs(c) < 0.01f || v == 0.f)
        return v;
    const float e = c > 0.f ? (1.f + 3.f * c) : (1.f / (1.f + 3.f * -c));
    const float av = std::pow(std::abs(v), e);
    return v < 0.f ? -av : av;
}

void ModMatrix::computeOffsets()
{
    destOffsets.clear();
    if (! cacheLive)
        return;

    for (const auto& c : cacheLive->conns)
    {
        if (c.muted || std::abs(c.amount) < 0.005f)
            continue;
        float v = srcAvg[(size_t) clampRange(c.slot, 0, kNumSlots - 1)].load();
        v = shapeCurve(v, c.curve);
        if (c.invert)
            v = -v;
        destOffsets[c.dest] += c.amount * v;
    }
}

float ModMatrix::offsetFor(const std::string& dest) const
{
    auto it = destOffsets.find(dest);
    return it != destOffsets.end() ? it->second : 0.f;
}

void ModMatrix::triggerVoice(int voiceId)
{
    if (voiceId < 0 || voiceId >= kMaxVoices)
        return;

    auto c = cacheLive ? cacheLive : cacheSnapshot();
    if (! c)
        return;

    auto& refs = voiceEnvs[(size_t) voiceId];
    refs.clear();

    for (int e = 0; e < kNumEnv; ++e)
    {
        if (c->envVoice[(size_t) e].empty())
            continue;
        auto* env = static_cast<EnvSource*>(sources[(size_t) slotEnv(e)].get());
        const int instIdx = (rrInst[(size_t) e]++) % kEnvInstances;
        env->triggerInstance(instIdx);
        refs.push_back({ slotEnv(e), instIdx });
    }
}

void ModMatrix::renderVoice(int voiceId, VoiceMods& out, int n)
{
    out.amp   = offsetFor(idVoiceAmp);
    out.pitch = offsetFor(idVoicePitch);
    out.pan   = offsetFor(idVoicePan);

    if (voiceId < 0 || voiceId >= kMaxVoices || ! cacheLive)
        return;

    for (const auto& ref : voiceEnvs[(size_t) voiceId])
    {
        auto* env = static_cast<EnvSource*>(sources[(size_t) ref.srcIdx].get());
        env->renderInstance(ref.inst, n);
        const float v = env->instanceValue(ref.inst);
        const int envIdx = ref.srcIdx - slotEnv(0);

        for (const auto& conn : cacheLive->envVoice[(size_t) envIdx])
        {
            float x = shapeCurve(v, conn.curve);
            if (conn.invert)
                x = -x;
            x *= conn.amount;
            if (conn.destIdx == 0)      out.amp += x;
            else if (conn.destIdx == 1) out.pitch += x;
            else                        out.pan += x;
        }
    }
}

// ---------------------------------------------------------------------------
// Message thread / UI
// ---------------------------------------------------------------------------
std::shared_ptr<const ModMatrix::Cache> ModMatrix::cacheSnapshot() const
{
    const juce::SpinLock::ScopedLockType sl(cacheLock);
    return cache;
}

void ModMatrix::rebuildCache()
{
    auto c = std::make_shared<Cache>();

    for (int i = 0; i < matTree.getNumChildren(); ++i)
    {
        const auto child = matTree.getChild(i);
        Connection conn;
        conn.id     = (int) child.getProperty("id", 0);
        conn.slot   = clampRange((int) child.getProperty("slot", 0), 0, kNumSlots - 1);
        conn.dest   = child.getProperty("dest", "").toString().toStdString();
        conn.amount = (float) (double) child.getProperty("amount", 0.5);
        conn.invert = bool(child.getProperty("invert", false));
        conn.muted  = bool(child.getProperty("muted", false));
        conn.curve  = (float) (double) child.getProperty("curve", 0.0);
        c->conns.push_back(conn);

        const int envIdx = conn.slot - slotEnv(0);
        if (envIdx >= 0 && envIdx < kNumEnv)
        {
            int destIdx = -1;
            if (conn.dest == idVoiceAmp)   destIdx = 0;
            if (conn.dest == idVoicePitch) destIdx = 1;
            if (conn.dest == idVoicePan)   destIdx = 2;
            if (destIdx >= 0)
                c->envVoice[(size_t) envIdx].push_back({ destIdx, conn.amount, conn.invert, conn.curve });
        }
    }

    bool any = false;
    for (const auto& s : sources)
        any = any || s->enabled.load();
    anyEnabled = any;

    {
        const juce::SpinLock::ScopedLockType sl(cacheLock);
        cache = c;
    }
}

int ModMatrix::addConnection(int slot, juce::StringRef dest, float amount)
{
    // Auto-enable source when a connection is patched so modulation is active immediately
    setSourceParam(slot, "enabled", true);

    const int safeSlot = clampRange(slot, 0, kNumSlots - 1);
    const juce::String destStr(dest);

    // Prevent duplicate connection if this source is already patched to this destination
    for (int i = 0; i < matTree.getNumChildren(); ++i)
    {
        auto child = matTree.getChild(i);
        if ((int) child.getProperty("slot", -1) == safeSlot && child.getProperty("dest", "").toString() == destStr)
        {
            child.setProperty("muted", false, nullptr);
            if (std::abs(amount - 0.5f) > 0.001f)
                child.setProperty("amount", (double) amount, nullptr);
            return (int) child.getProperty("id", 0);
        }
    }

    int maxId = 0;
    for (int i = 0; i < matTree.getNumChildren(); ++i)
        maxId = juce::jmax(maxId, (int) matTree.getChild(i).getProperty("id", 0));

    juce::ValueTree c("conn");
    c.setProperty("id", maxId + 1, nullptr);
    c.setProperty("slot", safeSlot, nullptr);
    c.setProperty("dest", destStr, nullptr);
    c.setProperty("amount", (double) amount, nullptr);
    c.setProperty("invert", false, nullptr);
    c.setProperty("muted", false, nullptr);
    c.setProperty("curve", 0.0, nullptr);
    matTree.appendChild(c, nullptr); // listener rebuilds the audio cache
    return maxId + 1;
}

void ModMatrix::removeConnection(int id)
{
    for (int i = 0; i < matTree.getNumChildren(); ++i)
    {
        auto child = matTree.getChild(i);
        if ((int) child.getProperty("id", -1) == id)
        {
            matTree.removeChild(child, nullptr);
            return;
        }
    }
}

juce::ValueTree ModMatrix::connectionById(int id) const
{
    for (int i = 0; i < matTree.getNumChildren(); ++i)
    {
        auto child = matTree.getChild(i);
        if ((int) child.getProperty("id", -1) == id)
            return child;
    }
    return {};
}

void ModMatrix::setSourceParam(int slot, juce::StringRef key, juce::var value)
{
    if (slot < 0 || slot >= kNumSlots)
        return;
    auto st = sources[(size_t) slot]->state;
    if (st.isValid())
        st.setProperty(juce::Identifier(juce::String(key)), value, nullptr); // listener syncs the source
}

void ModMatrix::setSeqStep(int slot, int step, float v)
{
    if (slotClassOf(slot) == SC_SEQ)
        static_cast<SeqSource*>(sources[(size_t) slot].get())->setStep(step, v);
}

void ModMatrix::retriggerSource(int slot)
{
    if (slot >= 0 && slot < kNumSlots)
        sources[(size_t) slot]->retrigger();
}

void ModMatrix::syncAllFromTrees()
{
    for (auto& s : sources)
        s->syncFromState();
    rebuildCache();
}

std::vector<ModMatrix::Ring> ModMatrix::ringsFor(juce::StringRef dest) const
{
    std::vector<Ring> out;
    auto c = cacheSnapshot();
    if (! c)
        return out;

    const std::string d = juce::String(dest).toStdString();
    for (const auto& conn : c->conns)
    {
        if (conn.muted || conn.dest != d)
            continue;
        Ring r;
        r.slot = conn.slot;
        r.id = conn.id;
        r.amount = conn.invert ? -conn.amount : conn.amount;
        r.now = srcAvg[(size_t) conn.slot].load();
        r.curve = conn.curve;
        out.push_back(r);
    }
    return out;
}

void ModMatrix::valueTreeChildAdded(juce::ValueTree& parent, juce::ValueTree&)
{
    if (parent == matTree)
        rebuildCache();
}

void ModMatrix::valueTreeChildRemoved(juce::ValueTree& parent, juce::ValueTree&, int)
{
    if (parent == matTree)
        rebuildCache();
}

void ModMatrix::valueTreePropertyChanged(juce::ValueTree& tree, const juce::Identifier& prop)
{
    const auto parent = tree.getParent();
    if (parent == matTree)
    {
        rebuildCache();
    }
    else if (parent == srcTree)
    {
        const int idx = srcTree.indexOf(tree);
        if (idx >= 0 && idx < kNumSlots)
        {
            sources[(size_t) idx]->syncFromState();
            if (prop.toString() == "enabled")
                rebuildCache();
        }
    }
}

} // namespace f64
