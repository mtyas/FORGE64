#include "SequencePresetManager.h"

namespace f64 {

juce::File SequencePresetManager::getPresetsDirectory()
{
    auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                   .getChildFile("Forge64")
                   .getChildFile("Sequences");
    if (! dir.exists())
        dir.createDirectory();
    return dir;
}

juce::StringArray SequencePresetManager::getUserPresetNames()
{
    juce::StringArray names;
    auto dir = getPresetsDirectory();
    if (dir.isDirectory())
    {
        auto files = dir.findChildFiles(juce::File::findFiles, false, "*.f64seq");
        for (const auto& f : files)
            names.add(f.getFileNameWithoutExtension());
    }
    names.sort(true);
    return names;
}

bool SequencePresetManager::savePreset(const juce::String& name, const StepSequencer& seq, int patternIndex)
{
    if (name.trim().isEmpty())
        return false;

    auto dir = getPresetsDirectory();
    auto file = dir.getChildFile(juce::File::createLegalFileName(name.trim()) + ".f64seq");
    return exportPresetToFile(file, seq, patternIndex);
}

bool SequencePresetManager::loadPreset(const juce::String& name, StepSequencer& seq, int patternIndex)
{
    auto dir = getPresetsDirectory();
    auto file = dir.getChildFile(name.trim() + ".f64seq");
    if (! file.existsAsFile())
        return false;
    return loadPresetFromFile(file, seq, patternIndex);
}

bool SequencePresetManager::loadPresetFromFile(const juce::File& file, StepSequencer& seq, int patternIndex)
{
    if (! file.existsAsFile())
        return false;

    auto xml = juce::parseXML(file);
    if (xml == nullptr)
        return false;

    auto tree = juce::ValueTree::fromXml(*xml);
    if (! tree.isValid())
        return false;

    if (tree.hasType("PATTERN"))
    {
        seq.deserializePattern(patternIndex, tree);
        return true;
    }
    else if (tree.hasType("SEQUENCER"))
    {
        auto pats = tree.getChildWithName("PATTERNS");
        if (pats.isValid() && pats.getNumChildren() > 0)
        {
            seq.deserializePattern(patternIndex, pats.getChild(0));
            return true;
        }
    }
    return false;
}

bool SequencePresetManager::exportPresetToFile(const juce::File& file, const StepSequencer& seq, int patternIndex)
{
    auto tree = seq.serializePattern(patternIndex);
    if (! tree.isValid())
        return false;

    tree.setProperty("name", file.getFileNameWithoutExtension(), nullptr);
    auto xml = tree.createXml();
    if (xml == nullptr)
        return false;

    return xml->writeTo(file);
}

static juce::Array<juce::File> getCandidatePresetDirs(const juce::String& subFolderName)
{
    juce::Array<juce::File> candidates;
    auto exe = juce::File::getSpecialLocation(juce::File::currentExecutableFile);
    auto appDir = exe.getParentDirectory();

    candidates.add(juce::File("F:/forge64").getChildFile(subFolderName));
    candidates.add(appDir.getChildFile(subFolderName));
    candidates.add(appDir.getParentDirectory().getChildFile(subFolderName));
    candidates.add(appDir.getParentDirectory().getParentDirectory().getChildFile(subFolderName));
    candidates.add(appDir.getParentDirectory().getParentDirectory().getParentDirectory().getChildFile(subFolderName));
    candidates.add(juce::File::getCurrentWorkingDirectory().getChildFile(subFolderName));

    return candidates;
}

void SequencePresetManager::initializePresetsOnDisk()
{
    auto targetDir = getPresetsDirectory();

    for (const auto& cand : getCandidatePresetDirs("sequencer presets"))
    {
        if (cand.isDirectory())
        {
            auto sourceFiles = cand.findChildFiles(juce::File::findFiles, false, "*.f64seq");
            for (const auto& sf : sourceFiles)
            {
                auto destFile = targetDir.getChildFile(sf.getFileName());
                if (! destFile.existsAsFile())
                    sf.copyFileTo(destFile);
            }
        }
    }

    if (targetDir.findChildFiles(juce::File::findFiles, false, "*.f64seq").isEmpty())
    {
        auto tempSeq = std::make_unique<StepSequencer>();
        tempSeq->loadFactoryPreset(0);
        savePreset("4-on-the-Floor Reference", *tempSeq, 0);
    }
}

} // namespace f64
