#include "PadGrid.h"

namespace f64 {

juce::ValueTree PadGrid::makeDefaultTree()
{
    juce::ValueTree t("PADS");
    static const int bankCols[kNumBanks] = { 0xFF3A6EA5, 0xFF3E8E5A, 0xFFC2703A, 0xFF8E5BC7 };

    for (int i = 0; i < kNumPads; ++i)
    {
        juce::ValueTree p("pad");
        p.setProperty("name", "", nullptr);
        p.setProperty("sample", "", nullptr);
        p.setProperty("colour", bankCols[i / kPadsPerBank], nullptr);
        p.setProperty("scriptOn", false, nullptr);
        p.setProperty("script", "", nullptr);
        p.setProperty("gainTrim", 1.0, nullptr);
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
