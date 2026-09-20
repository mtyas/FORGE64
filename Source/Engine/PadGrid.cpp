#include "PadGrid.h"
#include "../Presets/ModulePresetManager.h"

namespace f64 {

juce::ValueTree PadGrid::makeDefaultTree()
{
    juce::ValueTree t("PADS");
    static const int bankCols[kNumBanks] = { (int) 0xFF3A6EA5, (int) 0xFF3E8E5A, (int) 0xFFC2703A, (int) 0xFF8E5BC7 };

    static const char* defaultNames[kNumPads] = {
        // Bank A: Core Electronic & Acoustic (0..15)
        "808 Sub Kick", "808 Snare", "Closed Hat", "Open Hat",
        "808 Clap", "Acoustic Low Tom", "Acoustic Mid Tom", "Acoustic Hi Tom",
        "Maple Rimshot", "808 Cowbell", "Sizzle Shaker", "Modal Crash",
        "Ride Bell", "Latin Conga", "Laser Zap", "Rock Kick",
        // Bank B: Heavy / Electro / Industrial Club (16..31)
        "909 Punch Kick", "909 Dance Snare", "Linear FM Hat", "FM Cyber Bell",
        "Stereo Room Clap", "Simmons Low Tom", "Simmons Mid Tom", "Simmons Hi Tom",
        "Hardstyle Kick", "Trash Gated Clap", "Rock Noise Snare", "Trash China Splash",
        "Electro 7-Oct Kick", "Cross-Mod Noise", "303 Acid Stab", "Deep Sub FM Kick",
        // Bank C: World & Acoustic Percussion (32..47)
        "Floor Tom Sub", "Studio Wood Rim", "Agogo High Bell", "Agogo Low Bell",
        "Conga Slap", "Conga Low Mute", "Hardwood Block Hi", "Hardwood Block Lo",
        "High Disco Cowbell", "Low Latin Cha-Cha", "Pure Ride Ping", "China Choke",
        "Fast Splash Wash", "Air Noise Shaker", "Acoustic Snare Rim", "24-Inch Deep Bass",
        // Bank D: Melodic Synths, Acid, Plucks & Cyber FX (48..63)
        "Karplus Nylon", "Karplus Steel Wire", "Karplus Bass Pluck", "303 Screaming Reso",
        "303 Square Bass", "Acid Rave Lead", "Space Invader Zap", "Downer Laser Drop",
        "Laser Chirp Stereo", "FM Chaos Static", "Metallic FM Drone", "Dubstep Low Rattle",
        "Brutal Folded Raw", "Electro Air Crunch", "Ambient Hall Clap", "Plate Shimmer"
    };

    static const char* defaultModules[kNumPads] = {
        // Bank A: Core Electronic & Acoustic (0..15)
        "kick_808", "snare_808", "hat_closed", "hat_open",
        "clap_808", "tom_dual", "tom_dual", "tom_dual",
        "snare_rimshot", "perc_cowbell", "hat_noise", "cymbal_crash",
        "cymbal_ride", "perc_conga", "synth_zap", "kick_rock",
        // Bank B: Heavy / Electro / Industrial Club (16..31)
        "kick_909", "snare_909", "hat_fm", "hat_fm",
        "clap_room", "tom_simmons", "tom_simmons", "tom_simmons",
        "kick_hardstyle", "clap_trash", "snare_rock", "cymbal_china",
        "kick_electro", "synth_noise", "synth_acid", "kick_sub_fm",
        // Bank C: World & Acoustic Percussion (32..47)
        "tom_floor", "perc_rimshot", "perc_agogo", "perc_agogo",
        "perc_conga", "perc_conga", "perc_rimshot", "perc_rimshot",
        "perc_cowbell", "perc_cowbell", "cymbal_ride", "cymbal_china",
        "cymbal_crash", "hat_noise", "snare_rock", "kick_rock",
        // Bank D: Melodic Synths, Acid, Plucks & Cyber FX (48..63)
        "synth_karplus", "synth_karplus", "synth_karplus", "synth_acid",
        "synth_acid", "synth_acid", "synth_zap", "synth_zap",
        "synth_zap", "synth_noise", "synth_noise", "kick_sub_fm",
        "kick_hardstyle", "kick_electro", "clap_room", "cymbal_crash"
    };

    for (int i = 0; i < kNumPads; ++i)
    {
        juce::ValueTree p("pad");
        p.setProperty("name", defaultNames[i], nullptr);
        p.setProperty("moduleId", defaultModules[i], nullptr);
        p.setProperty("sample", "", nullptr);
        p.setProperty("colour", bankCols[i / kPadsPerBank], nullptr);

        auto mod = ModulePresetManager::getModuleById(defaultModules[i]);
        p.setProperty("scriptOn", true, nullptr);
        p.setProperty("script", mod.scriptCode, nullptr);

        p.setProperty("gainTrim", 1.0, nullptr);
        p.setProperty("sampleStart", 0.0, nullptr);
        p.setProperty("sampleEnd", 1.0, nullptr);
        p.setProperty("loopStart", 0.0, nullptr);
        p.setProperty("loopEnd", 1.0, nullptr);
        p.setProperty("loopOn", false, nullptr);
        p.setProperty("reverseOn", false, nullptr);
        p.setProperty("mute", false, nullptr);
        p.setProperty("solo", false, nullptr);
        t.appendChild(p, nullptr);
    }
    return t;
}

PadGrid::PadGrid(juce::ValueTree padsTree, SampleManager& manager)
    : tree(padsTree), sm(manager)
{
    tree.addListener(this);
    syncAllAtomics();
    reloadSamples();
}

PadGrid::~PadGrid()
{
    tree.removeListener(this);
}

void PadGrid::syncAtomics(int pad)
{
    if (pad < 0 || pad >= kNumPads)
        return;
    const auto st = tree.getChild(pad);
    if (! st.isValid())
        return;
    rt[(size_t) pad].scriptOn = bool(st.getProperty("scriptOn", false));
    rt[(size_t) pad].gainTrim = (float) (double) st.getProperty("gainTrim", 1.0);
    rt[(size_t) pad].sampleStart = (float) (double) st.getProperty("sampleStart", 0.0);
    rt[(size_t) pad].sampleEnd   = (float) (double) st.getProperty("sampleEnd", 1.0);
    rt[(size_t) pad].loopStart   = (float) (double) st.getProperty("loopStart", 0.0);
    rt[(size_t) pad].loopEnd     = (float) (double) st.getProperty("loopEnd", 1.0);
    rt[(size_t) pad].loopOn      = bool(st.getProperty("loopOn", false));
    rt[(size_t) pad].reverseOn   = bool(st.getProperty("reverseOn", false));
    rt[(size_t) pad].isMuted     = bool(st.getProperty("mute", false));
    rt[(size_t) pad].isSolo      = bool(st.getProperty("solo", false));
}

void PadGrid::syncAllAtomics()
{
    for (int p = 0; p < kNumPads; ++p)
        syncAtomics(p);
}

void PadGrid::valueTreePropertyChanged(juce::ValueTree& t, const juce::Identifier&)
{
    if (t.getParent() == tree)
        syncAtomics(tree.indexOf(t));
}

void PadGrid::reloadSamples()
{
    for (int p = 0; p < kNumPads; ++p)
    {
        const auto path = tree.getChild(p).getProperty("sample", "").toString();
        SampleManager::Ptr smp;
        if (path.isNotEmpty())
        {
            smp = sm.findCached(path);
            if (! smp)
            {
                const juce::File f(path);
                if (f.existsAsFile())
                    smp = sm.loadFile(f);
            }
        }
        const juce::ScopedLock sl(rt[(size_t) p].lock);
        rt[(size_t) p].sample = smp;
    }
}

void PadGrid::setSampleFile(int pad, const juce::File& f)
{
    if (pad < 0 || pad >= kNumPads)
        return;

    auto st = tree.getChild(pad);
    SampleManager::Ptr smp = sm.loadFile(f);
    st.setProperty("sample", smp ? f.getFullPathName() : juce::String(), nullptr);

    if (smp && st.getProperty("name", "").toString().isEmpty())
        st.setProperty("name", f.getFileNameWithoutExtension(), nullptr);

    {
        const juce::ScopedLock sl(rt[(size_t) pad].lock);
        rt[(size_t) pad].sample = smp;
    }
}

void PadGrid::clearSample(int pad)
{
    if (pad < 0 || pad >= kNumPads)
        return;
    tree.getChild(pad).setProperty("sample", "", nullptr);
    const juce::ScopedLock sl(rt[(size_t) pad].lock);
    rt[(size_t) pad].sample = nullptr;
}

void PadGrid::copyPad(int from, int to)
{
    if (from == to || from < 0 || from >= kNumPads || to < 0 || to >= kNumPads)
        return;

    tree.getChild(to).copyPropertiesFrom(tree.getChild(from), nullptr);

    {
        const juce::ScopedLock slFrom(rt[(size_t) from].lock);
        const juce::ScopedLock slTo(rt[(size_t) to].lock);
        rt[(size_t) to].sample = rt[(size_t) from].sample;
    }

    if (apvts != nullptr)
    {
        for (int k = 0; k < kNumPadParams; ++k)
        {
            auto* sp = apvts->getParameter(padParamId(from, kPadParams[k].base));
            auto* dp = apvts->getParameter(padParamId(to, kPadParams[k].base));
            if (sp != nullptr && dp != nullptr)
                dp->setValueNotifyingHost(sp->getValue());
        }
        // Avoid duplicate note mappings: give the copy its default note back.
        if (auto* mp = dynamic_cast<juce::RangedAudioParameter*>(
                apvts->getParameter(padParamId(to, "mnote"))))
            mp->setValueNotifyingHost(mp->convertTo0to1((float) juce::jmin(127, 36 + to)));
    }

    syncAtomics(to);
}

void PadGrid::clearPad(int pad)
{
    if (pad < 0 || pad >= kNumPads)
        return;
    auto st = tree.getChild(pad);
    st.setProperty("name", "", nullptr);
    st.setProperty("sample", "", nullptr);
    st.setProperty("scriptOn", false, nullptr);
    st.setProperty("script", "", nullptr);
    st.setProperty("gainTrim", 1.0, nullptr);
    {
        const juce::ScopedLock sl(rt[(size_t) pad].lock);
        rt[(size_t) pad].sample = nullptr;
    }
    syncAtomics(pad);
}

} // namespace f64
