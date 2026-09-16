#include "PresetManager.h"
#include "../Engine/PadDefs.h"

namespace f64 {

PresetManager::PresetManager(juce::ValueTree& kitRoot, juce::AudioProcessorValueTreeState& apvtsIn)
    : kit(kitRoot), apvts(apvtsIn) {}

void PresetManager::copyTreeInPlace(juce::ValueTree dst, const juce::ValueTree& src)
{
    dst.copyPropertiesFrom(src, nullptr);
    while (dst.getNumChildren() > src.getNumChildren())
        dst.removeChild(dst.getNumChildren() - 1, nullptr);
    for (int i = 0; i < src.getNumChildren(); ++i)
    {
        if (i < dst.getNumChildren())
            copyTreeInPlace(dst.getChild(i), src.getChild(i));
        else
            dst.appendChild(src.getChild(i).createCopy(), nullptr);
    }
}

int PresetManager::padIndexOfParamId(juce::StringRef id)
{
    if (! id.startsWithChar('p') || ! id.contains("_"))
        return -1;
    const int pad = id.substring(1, id.indexOf("_")).getIntValue();
    return (pad >= 0 && pad < kNumPads) ? pad : -1;
}

// ---------------------------------------------------------------------------
// .kit - complete state
// ---------------------------------------------------------------------------
bool PresetManager::saveKit(const juce::File& f)
{
    auto xml = juce::createXmlFromValueTree(kit);
    if (! xml)
    {
        err = "serialization failed";
        return false;
    }
    xml->setAttribute("type", "kit");
    xml->setAttribute("version", 1);
    if (! xml->writeTo(f, {}))
    {
        err = "could not write " + f.getFullPathName();
        return false;
    }
    err = {};
    return true;
}

bool PresetManager::loadKit(const juce::File& f)
{
    auto xml = juce::XmlDocument::parse(f);
    if (! xml || xml->getTagName() != kit.getType().toString())
    {
        err = "not a FORGE64 kit file";
        return false;
    }
    auto incoming = juce::parseXmlRecursively(*xml);
    if (! incoming.isValid())
    {
        err = "corrupt kit file";
        return false;
    }

    auto params = incoming.getChildWithName("PARAMS");
    if (params.isValid())
        apvts.replaceState(params);

    for (const char* name : { "PADS", "MODSRC", "MODMAT" })
    {
        auto src = incoming.getChildWithName(name);
        auto dst = kit.getChildWithName(name);
        if (src.isValid() && dst.isValid())
            copyTreeInPlace(dst, src);
    }

    err = {};
    if (onLoaded)
        onLoaded();
    return true;
}

// ---------------------------------------------------------------------------
// .bnk / .pad - subsets. File pad indices are mapped modulo padCount onto the
// destination range, so a bank saved from C loads cleanly into A, etc.
// ---------------------------------------------------------------------------
bool PresetManager::saveParamSubset(const juce::File& f, const char* tag,
                                    int firstPad, int padCount, int bankOrPad)
{
    juce::XmlElement root(tag);
    root.setAttribute("version", 1);
    root.setAttribute("index", bankOrPad);

    auto pads = kit.getChildWithName("PADS");
    for (int i = 0; i < padCount; ++i)
    {
        auto padTree = pads.getChild(firstPad + i);
        if (padTree.isValid())
            if (auto* x = juce::createXmlFromValueTree(padTree).release())
                root.addChildElement(x);
    }

    if (auto fullParams = juce::createXmlFromValueTree(apvts.state))
    {
        auto* paramsEl = new juce::XmlElement("PARAMS");
        for (auto* child : fullParams->getChildIterator())
        {
            const int pad = padIndexOfParamId(child->getStringAttribute("id"));
            if (pad >= firstPad && pad < firstPad + padCount)
                paramsEl->addChildElement(new juce::XmlElement(*child));
        }
        root.addChildElement(paramsEl);
    }

    if (! root.writeTo(f, {}))
    {
        err = "could not write " + f.getFullPathName();
        return false;
    }
    err = {};
    return true;
}

bool PresetManager::loadParamSubset(const juce::File& f, const char* tag,
                                    int destFirstPad, int padCount)
{
    auto xml = juce::XmlDocument::parse(f);
    if (! xml || xml->getTagName() != juce::String(tag))
    {
        err = juce::String("not a FORGE64 ") + tag + " file";
        return false;
    }

    // pad state children
    auto pads = kit.getChildWithName("PADS");
    int i = 0;
    for (auto* child : xml->getChildIterator())
    {
        if (child->hasTagName("pad") && i < padCount)
        {
            auto incoming = juce::parseXmlRecursively(*child);
            auto dst = pads.getChild(destFirstPad + i);
            if (incoming.isValid() && dst.isValid())
                copyTreeInPlace(dst, incoming);
            ++i;
        }
    }

    // parameter subset: remap pad index into destination range
    if (auto* paramsEl = xml->getChildByName("PARAMS"))
    {
        for (auto* child : paramsEl->getChildIterator())
        {
            const auto id = child->getStringAttribute("id");
            const int pad = padIndexOfParamId(id);
            if (pad < 0)
                continue;

            const int mappedPad = destFirstPad + (pad % padCount);
            const auto base = id.substring(id.indexOf("_") + 1);
            if (auto* param = apvts.getParameter(padParamId(mappedPad, base)))
                param->setValueNotifyingHost((float) child->getDoubleAttribute("value", 0.0));
        }
    }

    err = {};
    if (onLoaded)
        onLoaded();
    return true;
}

bool PresetManager::saveBank(const juce::File& f, int bank)
{
    return saveParamSubset(f, "FORGE64BANK", bank * kPadsPerBank, kPadsPerBank, bank);
}

bool PresetManager::loadBank(const juce::File& f, int bank)
{
    return loadParamSubset(f, "FORGE64BANK", bank * kPadsPerBank, kPadsPerBank);
}

bool PresetManager::savePad(const juce::File& f, int pad)
{
    return saveParamSubset(f, "FORGE64PAD", pad, 1, pad);
}

bool PresetManager::loadPad(const juce::File& f, int pad)
{
    return loadParamSubset(f, "FORGE64PAD", pad, 1);
}

} // namespace f64
