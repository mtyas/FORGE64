#pragma once
#include <JuceHeader.h>
#include <unordered_map>
#include <atomic>
#include <mutex>

namespace f64 {

class MidiLearnManager
{
public:
    MidiLearnManager() = default;

    void startLearning(const juce::String& paramId)
    {
        std::lock_guard<std::mutex> lock(mutex);
        learningTarget = paramId;
        isLearningActive.store(true);
    }

    void stopLearning()
    {
        std::lock_guard<std::mutex> lock(mutex);
        learningTarget.clear();
        isLearningActive.store(false);
    }

    void setLearnActive(bool active)
    {
        if (active)
            startLearning("ANY");
        else
            stopLearning();
    }

    bool isLearning() const { return isLearningActive.load(); }
    juce::String currentLearningTarget() const
    {
        std::lock_guard<std::mutex> lock(mutex);
        return learningTarget;
    }

    bool isParamLearning(const juce::String& paramId) const
    {
        if (! isLearningActive.load()) return false;
        std::lock_guard<std::mutex> lock(mutex);
        return learningTarget == paramId;
    }

    void bind(int channel, int cc, const juce::String& paramId)
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (paramId.isEmpty() || paramId == "ANY") return;
        // Remove any previous binding for this param
        for (auto it = ccToParam.begin(); it != ccToParam.end(); )
        {
            if (it->second == paramId)
                it = ccToParam.erase(it);
            else
                ++it;
        }
        const uint32_t key = makeKey(channel, cc);
        if (auto previous = ccToParam.find(key); previous != ccToParam.end())
            paramToKey.erase(previous->second);
        ccToParam[key] = paramId;
        paramToKey[paramId] = key;
    }

    void unbind(const juce::String& paramId)
    {
        std::lock_guard<std::mutex> lock(mutex);
        auto it = paramToKey.find(paramId);
        if (it != paramToKey.end())
        {
            ccToParam.erase(it->second);
            paramToKey.erase(it);
        }
    }

    juce::String getBoundParam(int channel, int cc) const
    {
        std::lock_guard<std::mutex> lock(mutex);
        // First try channel-specific, then omni (channel 0)
        auto it = ccToParam.find(makeKey(channel, cc));
        if (it != ccToParam.end())
            return it->second;
        it = ccToParam.find(makeKey(0, cc));
        if (it != ccToParam.end())
            return it->second;
        return {};
    }

    int getBoundCC(const juce::String& paramId) const
    {
        std::lock_guard<std::mutex> lock(mutex);
        auto it = paramToKey.find(paramId);
        if (it != paramToKey.end())
            return (int) (it->second & 0xFFFF);
        return -1;
    }

    void queueControlValue(const juce::String& paramId, float value)
    {
        std::lock_guard<std::mutex> lock(mutex);
        pendingControlValues[paramId] = value;
    }

    bool consumeControlValue(const juce::String& paramId, float& value)
    {
        std::lock_guard<std::mutex> lock(mutex);
        auto it = pendingControlValues.find(paramId);
        if (it == pendingControlValues.end()) return false;
        value = it->second;
        pendingControlValues.erase(it);
        return true;
    }

    juce::ValueTree serialize() const
    {
        std::lock_guard<std::mutex> lock(mutex);
        juce::ValueTree tree("MIDI_LEARN");
        for (const auto& pair : ccToParam)
        {
            juce::ValueTree entry("BINDING");
            const int chan = (int) (pair.first >> 16);
            const int cc   = (int) (pair.first & 0xFFFF);
            entry.setProperty("chan", chan, nullptr);
            entry.setProperty("cc", cc, nullptr);
            entry.setProperty("param", pair.second, nullptr);
            tree.appendChild(entry, nullptr);
        }
        return tree;
    }

    void deserialize(const juce::ValueTree& tree)
    {
        std::lock_guard<std::mutex> lock(mutex);
        ccToParam.clear();
        paramToKey.clear();
        pendingControlValues.clear();
        if (! tree.isValid()) return;

        for (int i = 0; i < tree.getNumChildren(); ++i)
        {
            const auto child = tree.getChild(i);
            if (child.hasType("BINDING"))
            {
                const int chan = child.getProperty("chan", 0);
                const int cc   = child.getProperty("cc", -1);
                const juce::String param = child.getProperty("param", "").toString();
                if (cc >= 0 && cc <= 127 && chan >= 0 && chan <= 16 && param.isNotEmpty() && param != "ANY")
                {
                    const uint32_t key = makeKey(chan, cc);
                    ccToParam[key] = param;
                    paramToKey[param] = key;
                }
            }
        }
    }

private:
    static uint32_t makeKey(int chan, int cc)
    {
        return ((uint32_t) (chan & 0xFFFF) << 16) | (uint32_t) (cc & 0xFFFF);
    }

    mutable std::mutex mutex;
    std::atomic<bool> isLearningActive { false };
    juce::String learningTarget;
    std::unordered_map<uint32_t, juce::String> ccToParam;
    std::unordered_map<juce::String, uint32_t> paramToKey;
    std::unordered_map<juce::String, float> pendingControlValues;
};

} // namespace f64
