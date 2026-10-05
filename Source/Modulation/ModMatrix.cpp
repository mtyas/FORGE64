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
    for (int i = 0; i < kNumMidiSrc + kNumMacros; ++i)
    {
        juce::ValueTree state(i < kNumMidiSrc ? "MIDI" : "MACRO");
        state.setProperty("enabled", true, nullptr);
        srcOut.appendChild(state, nullptr);
    }
    for (auto state : srcOut)
        state.setProperty("offset", 0.f, nullptr);
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

void ModMatrix::renderSources(int n, const std::array<float, kNumMacros>* performanceValues)
{
    cacheLive = cacheSnapshot();

    for (int i = 0; i < kNumMacros; ++i)
        if (macroParams[(size_t) i] != nullptr)
            static_cast<MacroSource*>(sources[(size_t) slotMacro(i)].get())
                ->setValue(performanceValues != nullptr && (*performanceValues)[(size_t)i] >= 0.f
                    ? (*performanceValues)[(size_t)i] : macroParams[(size_t) i]->load());

    for (int k = 0; k < kNumMidiSrc; ++k)
        static_cast<MidiSource*>(sources[(size_t) slotMidi(k)].get())
            ->setValue(midiLatch[(size_t) k].load());

    for (int s = 0; s < kNumSlots; ++s)
    {
        auto& buf = srcBuf[(size_t) s];
        if ((int) buf.size() < n)
            buf.resize((size_t) n, 0.f);

        sources[(size_t) s]->render(buf.data(), n);

        const auto& source = *sources[(size_t) s];
        const bool active = source.enabled.load();
        if (! active) std::fill_n(buf.data(), n, 0.f);
        const float shift = active ? source.outputOffset.load() : 0.f;

        float sum = 0.f;
        for (int i = 0; i < n; ++i)
        {
            buf[(size_t) i] += shift;
            sum += buf[(size_t) i];
        }
        srcAvg[(size_t) s] = n > 0 ? sum / (float) n : 0.f;
        if (s >= slotSeq(0) && s < slotSeq(kNumSeq) && n > 0
            && static_cast<SeqSource*>(sources[(size_t)s].get())->quantize.load())
            srcAvg[(size_t)s] = buf[(size_t)n - 1];
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

bool ModMatrix::melodicPitchConnection(int slot, const std::string& dest) const
{
    const int index = slot - slotSeq(0);
    return index >= 0 && index < kNumSeq && static_cast<const SeqSource*>(sources[(size_t)slot].get())->quantize.load()
        && (dest == idVoicePitch || (dest.size() >= 5 && dest.compare(dest.size() - 5, 5, "_tune") == 0));
}

float ModMatrix::effectiveConnectionAmount(int slot, const std::string& dest, float amount) const
{
    if (!melodicPitchConnection(slot, dest)) return amount;
    const float semitones = 12.f * juce::jlimit(1, 4, static_cast<const SeqSource*>(sources[(size_t)slot].get())->octaves.load());
    return dest == idVoicePitch ? semitones : semitones / 48.f;
}

void ModMatrix::computeOffsets()
{
    destOffsets.clear();
    if (! cacheLive)
        return;

    for (const auto& c : cacheLive->conns)
    {
        const bool melodic = melodicPitchConnection(c.slot, c.dest);
        if (c.muted || (!melodic && std::abs(c.amount) < 0.005f))
            continue;
        const int envIndex = c.slot - slotEnv(0);
        if (envIndex >= 0 && envIndex < kNumEnv
            && (c.dest == idVoiceAmp || c.dest == idVoicePitch || c.dest == idVoicePan))
            continue; // Voice envelopes are evaluated per voice, never added twice.
        float v = srcAvg[(size_t) clampRange(c.slot, 0, kNumSlots - 1)].load();
        if (melodic)
        {
            auto* seq = static_cast<SeqSource*>(sources[(size_t)c.slot].get());
            if (!seq->enabled.load()) continue;
            v = quantizeValue(v, (uint16_t)seq->noteMask.load(), seq->octaves.load());
        }
        else v = shapeCurve(v, c.curve);
        if (c.invert)
            v = -v;
        destOffsets[c.dest] += effectiveConnectionAmount(c.slot, c.dest, c.amount) * v;
    }
}

float ModMatrix::displayOffsetFor(const std::string& dest) const
{
    const auto snapshot = cacheSnapshot();
    if (!snapshot) return 0.f;
    float result = 0.f;
    for (const auto& connection : snapshot->conns)
    {
        if (connection.muted || connection.dest != dest) continue;
        const bool melodic = melodicPitchConnection(connection.slot, dest);
        if (!melodic && std::abs(connection.amount) < .005f) continue;
        float value = srcAvg[(size_t)connection.slot].load();
        if (melodic)
        {
            const auto* seq = static_cast<const SeqSource*>(sources[(size_t)connection.slot].get());
            if (!seq->enabled.load()) continue;
            value = quantizeValue(value, (uint16_t)seq->noteMask.load(), seq->octaves.load());
        }
        else value = shapeCurve(value, connection.curve);
        result += effectiveConnectionAmount(connection.slot, dest, connection.amount) * (connection.invert ? -value : value);
    }
    return result;
}

float ModMatrix::offsetFor(const std::string& dest) const
{
    auto it = destOffsets.find(dest);
    return it != destOffsets.end() ? it->second : 0.f;
}

void ModMatrix::resetSourcesForPad(int pad, int sampleOffset)
{
    for (int slot = 0; slot < kNumLFO + kNumRnd; ++slot)
    {
        auto* source = static_cast<PadResetSource*>(sources[(size_t)slot].get());
        if (source->enabled.load() && source->resetOnPad.load()
            && (source->triggerPad.load() == -1 || source->triggerPad.load() == pad))
            source->scheduleReset(sampleOffset);
    }
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
        const int instIdx = voiceId; // Each active voice owns its envelope instance.
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
        const float v = env->instanceValue(ref.inst) + (env->enabled.load() ? env->outputOffset.load() : 0.f);
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
            if (destIdx >= 0 && ! conn.muted && std::abs(conn.amount) >= 0.005f)
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
    // Older kits contain only the generated sources. Add state for MIDI and
    // macros too, so every source can store its offset and open an editor.
    while (srcTree.getNumChildren() < kNumSlots)
    {
        const int slot = srcTree.getNumChildren();
        juce::ValueTree state(slotClassOf(slot) == SC_MIDI ? "MIDI" : "MACRO");
        state.setProperty("enabled", true, nullptr);
        state.setProperty("offset", 0.f, nullptr);
        srcTree.appendChild(state, nullptr);
    }
    for (int slot = 0; slot < kNumSlots; ++slot)
        sources[(size_t) slot]->state = srcTree.getChild(slot);
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
        r.amount = effectiveConnectionAmount(conn.slot, conn.dest, conn.amount) * (conn.invert ? -1.f : 1.f);
        r.now = srcAvg[(size_t) conn.slot].load();
        r.curve = melodicPitchConnection(conn.slot, conn.dest) ? 0.f : conn.curve;
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
