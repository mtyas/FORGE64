#pragma once

static int testSamplerSourceSelection()
{
    int failures = 0;
    auto check = [&](bool ok, const char* message)
    {
        if (!ok) { ++failures; std::cout << "FAILED: " << message << std::endl; }
    };
    constexpr int blockSize = 512;
    const auto file = juce::File::getCurrentWorkingDirectory().getChildFile("build_win/sampler-regression.wav");
    file.getParentDirectory().createDirectory();
    file.deleteFile();
    juce::AudioBuffer<float> wave(2, 4800);
    for (int sample = 0; sample < wave.getNumSamples(); ++sample)
    {
        wave.setSample(0, sample, .15f * std::sin(juce::MathConstants<double>::twoPi * 997. * sample / 48000.));
        wave.setSample(1, sample, .12f * std::sin(juce::MathConstants<double>::twoPi * 1543. * sample / 48000.));
    }
    juce::WavAudioFormat format;
    auto stream = file.createOutputStream();
    std::unique_ptr<juce::AudioFormatWriter> writer(stream ? format.createWriterFor(stream.get(), 48000., 2, 24, {}, 0) : nullptr);
    if (!writer) { std::cout << "FAILED: create sampler WAV fixture" << std::endl; return 1; }
    stream.release();
    check(writer->writeFromAudioSampleBuffer(wave, 0, wave.getNumSamples()), "write sampler WAV fixture");
    writer.reset();

    auto actual = std::make_unique<f64::Forge64Processor>();
    auto reference = std::make_unique<f64::Forge64Processor>();
    const juce::String script = "function process() for i=0,n-1 do outL(i,0.25) outR(i,0.125) end end";
    auto set = [](f64::Forge64Processor& processor, const char* key, float value)
    {
        auto* parameter = processor.getAPVTS().getParameter(f64::padParamId(0, key));
        parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
    };
    for (auto* processor : {actual.get(), reference.get()})
    {
        processor->setPlayConfigDetails(0,2,48000.,blockSize);
        processor->prepareToPlay(48000.,blockSize);
        set(*processor,"src",f64::SRC_SAMPLE);
        set(*processor,"cthr",0.f); set(*processor,"crat",1.f);
        set(*processor,"drv",0.f); set(*processor,"vcft",0.f);
        set(*processor,"eqlg",0.f); set(*processor,"eqmg",0.f); set(*processor,"eqhg",0.f);
        set(*processor,"snda",0.f); set(*processor,"sndb",0.f); set(*processor,"sndc",0.f); set(*processor,"sndd",0.f);
        auto& master = processor->getAuxManager().masterParams;
        master.compOn = master.eqOn = master.driveOn = master.limiterOn = false;
        for (auto& aux : processor->getAuxManager().auxParams) aux.enabled = false;
        processor->grid().setSampleFile(0,file);
        const bool enabled = processor == actual.get();
        auto state = processor->grid().padState(0);
        state.setProperty("script",script,nullptr); state.setProperty("scriptOn",enabled,nullptr);
        processor->grid().runtime(0).scriptOn.store(enabled);
        processor->lua().setScript(0,script,enabled);
    }
    juce::AudioBuffer<float> output(2,blockSize), expected(2,blockSize);
    juce::MidiBuffer midi;
    int playbackCase = 0;
    auto comparePlayback = [&](f64::Forge64Processor& sampler)
    {
        sampler.triggerAudition(0); reference->triggerAudition(0);
        double difference = 0.; float peak = 0.f;
        for (int block = 0; block < 40; ++block)
        {
            sampler.processBlock(output,midi); reference->processBlock(expected,midi);
            for (int channel = 0; channel < 2; ++channel)
                for (int sample = 0; sample < blockSize; ++sample)
                {
                    difference += std::abs(output.getSample(channel,sample)-expected.getSample(channel,sample));
                    peak = std::max(peak,std::abs(output.getSample(channel,sample)));
                }
        }
        ++playbackCase;
        check(peak < .2f && output.getMagnitude(0,blockSize) < .0001f, "one-shot sample finishes without reading beyond its endpoint");
        const double meanDifference = difference / (40 * 2 * blockSize);
        if (!(peak > .01f && meanDifference < .00001)) std::cout << "Sampler case " << playbackCase << ": peak=" << peak << ", mean difference=" << meanDifference << std::endl;
        check(peak > .01f && meanDifference < .00001, "sampler plays the loaded stereo PCM even with a stored enabled Lua script");
    };
    comparePlayback(*actual);

    juce::MemoryBlock saved;
    actual->getStateInformation(saved);
    auto restored = std::make_unique<f64::Forge64Processor>();
    restored->setPlayConfigDetails(0,2,48000.,blockSize); restored->prepareToPlay(48000.,blockSize);
    restored->setStateInformation(saved.getData(),(int)saved.getSize());
    comparePlayback(*restored);

    auto services = std::make_unique<f64::Forge64Editor>(*actual);
    f64::PadEditor padEditor(*actual,*services,0,[] {});
    std::vector<juce::Component*> pending {&padEditor};
    juce::ComboBox* engine = nullptr; bool filenameVisible = false;
    while (!pending.empty())
    {
        auto* component = pending.back(); pending.pop_back();
        if (auto* combo = dynamic_cast<juce::ComboBox*>(component); combo && combo->getNumItems() == 2
            && combo->getItemText(0) == "Sample Playback") engine = combo;
        if (auto* label = dynamic_cast<juce::Label*>(component); label && label->getText() == file.getFileName()) filenameVisible = label->isVisible();
        for (auto* child : component->getChildren()) pending.push_back(child);
    }
    check(filenameVisible,"sampler label displays the loaded filename");
    check(engine != nullptr,"pad editor exposes the engine selector");
    if (engine)
    {
        engine->setSelectedId(2,juce::sendNotificationSync);
        // Match the reference's DSP history so the engine-switch comparison
        // includes the same DC-blocker tail from the constant Lua fixture.
        set(*reference,"src",f64::SRC_LUA);
        reference->grid().runtime(0).scriptOn.store(true);
        reference->lua().setScript(0,script,true);
        actual->triggerAudition(0); reference->triggerAudition(0);
        actual->processBlock(output,midi); reference->processBlock(expected,midi);
        check(output.getSample(0,blockSize-1) > .05f && output.getSample(1,blockSize-1) > .02f,
              "switching back to Lua plays the preserved script");
        for (int block = 0; block < 120; ++block)
        { actual->processBlock(output,midi); reference->processBlock(expected,midi); }
        engine->setSelectedId(1,juce::sendNotificationSync);
        set(*reference,"src",f64::SRC_SAMPLE);
        reference->grid().runtime(0).scriptOn.store(false);
        reference->lua().setScript(0,script,false);
        for (int block = 0; block < 40; ++block)
        { actual->processBlock(output,midi); reference->processBlock(expected,midi); }
        comparePlayback(*actual);
        check(actual->grid().padState(0).getProperty("script").toString() == script,
              "switching engines preserves the pad's script");
    }
    file.deleteFile();
    std::cout << "Sampler source regression tests: " << failures << " failures." << std::endl;
    return failures ? 1 : 0;
}
