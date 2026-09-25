#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Scripting/ScriptPresetManager.h"
#include <cmath>

namespace f64 {

static_assert(kNumPadParams == 44, "PadParams field mapping below must match kPadParams order");

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

        struct PadInitialDefaults
        {
            float tune, dec, drv, p1, p2, p3, p4, p5;
            int choke;
        };

        static const PadInitialDefaults kPadDefaults[kNumPads] = {
            // Bank A: Core Electronic & Acoustic (0..15)
            {   0.f, 0.65f, 0.12f, 0.60f, 0.40f, 0.50f, 0.40f, 0.30f, 0 }, // 00: 808 Sub Kick
            {   0.f, 0.28f, 0.08f, 0.65f, 0.50f, 0.45f, 0.55f, 0.50f, 0 }, // 01: 808 Snare
            {   0.f, 0.08f, 0.05f, 0.50f, 0.60f, 0.45f, 0.50f, 0.60f, 1 }, // 02: Closed Hat
            {   0.f, 0.55f, 0.06f, 0.60f, 0.65f, 0.55f, 0.50f, 0.45f, 1 }, // 03: Open Hat
            {   0.f, 0.35f, 0.06f, 0.55f, 0.50f, 0.50f, 0.50f, 0.60f, 0 }, // 04: 808 Clap
            {  -7.f, 0.55f, 0.08f, 0.55f, 0.45f, 0.50f, 0.50f, 0.45f, 0 }, // 05: Acoustic Low Tom
            {   0.f, 0.48f, 0.08f, 0.55f, 0.45f, 0.50f, 0.50f, 0.45f, 0 }, // 06: Acoustic Mid Tom
            {  +7.f, 0.42f, 0.08f, 0.55f, 0.45f, 0.50f, 0.50f, 0.45f, 0 }, // 07: Acoustic Hi Tom
            {  +2.f, 0.12f, 0.08f, 0.65f, 0.60f, 0.50f, 0.50f, 0.55f, 0 }, // 08: Maple Rimshot
            {   0.f, 0.30f, 0.08f, 0.55f, 0.50f, 0.50f, 0.50f, 0.50f, 0 }, // 09: 808 Cowbell
            {  +3.f, 0.12f, 0.04f, 0.65f, 0.45f, 0.55f, 0.50f, 0.50f, 0 }, // 10: Sizzle Shaker
            {   0.f, 1.20f, 0.08f, 0.65f, 0.60f, 0.50f, 0.30f, 0.55f, 0 }, // 11: Modal Crash
            {   0.f, 0.85f, 0.05f, 0.75f, 0.55f, 0.60f, 0.50f, 0.45f, 0 }, // 12: Ride Bell
            {   0.f, 0.35f, 0.05f, 0.55f, 0.50f, 0.60f, 0.50f, 0.50f, 0 }, // 13: Latin Conga
            {  +4.f, 0.22f, 0.15f, 0.80f, 0.65f, 0.40f, 0.50f, 0.45f, 0 }, // 14: Laser Zap
            {  -3.f, 0.42f, 0.14f, 0.65f, 0.50f, 0.50f, 0.50f, 0.45f, 0 }, // 15: Rock Kick

            // Bank B: Heavy / Electro / Industrial Club (16..31)
            {   0.f, 0.36f, 0.15f, 0.65f, 0.65f, 0.50f, 0.50f, 0.45f, 0 }, // 16: 909 Punch Kick
            {   0.f, 0.25f, 0.10f, 0.65f, 0.60f, 0.50f, 0.50f, 0.55f, 0 }, // 17: 909 Dance Snare
            {  +3.f, 0.10f, 0.05f, 0.65f, 0.55f, 0.60f, 0.45f, 0.55f, 2 }, // 18: Linear FM Hat
            {  +3.f, 0.45f, 0.06f, 0.85f, 0.75f, 0.70f, 0.40f, 0.80f, 2 }, // 19: FM Cyber Bell
            {   0.f, 0.45f, 0.08f, 0.60f, 0.70f, 0.55f, 0.50f, 0.55f, 0 }, // 20: Stereo Room Clap
            {  -6.f, 0.45f, 0.12f, 0.75f, 0.65f, 0.55f, 0.50f, 0.45f, 0 }, // 21: Simmons Low Tom
            {   0.f, 0.38f, 0.12f, 0.75f, 0.65f, 0.55f, 0.50f, 0.45f, 0 }, // 22: Simmons Mid Tom
            {  +6.f, 0.32f, 0.12f, 0.75f, 0.65f, 0.55f, 0.50f, 0.45f, 0 }, // 23: Simmons Hi Tom
            {  +1.f, 0.48f, 0.35f, 0.85f, 0.65f, 0.60f, 0.60f, 0.50f, 0 }, // 24: Hardstyle Kick
            {   0.f, 0.28f, 0.25f, 0.70f, 0.60f, 0.65f, 0.55f, 0.55f, 0 }, // 25: Trash Gated Clap
            {   0.f, 0.32f, 0.12f, 0.65f, 0.55f, 0.50f, 0.60f, 0.45f, 0 }, // 26: Rock Noise Snare
            {   0.f, 0.38f, 0.18f, 0.75f, 0.70f, 0.60f, 0.55f, 0.50f, 0 }, // 27: Trash China Splash
            {   0.f, 0.35f, 0.22f, 0.70f, 0.50f, 0.50f, 0.40f, 0.30f, 0 }, // 28: Electro 7-Oct Kick
            {   0.f, 0.35f, 0.20f, 0.60f, 0.55f, 0.65f, 0.55f, 0.45f, 0 }, // 29: Cross-Mod Noise
            { -12.f, 0.42f, 0.25f, 0.45f, 0.80f, 0.70f, 0.35f, 0.60f, 0 }, // 30: 303 Acid Stab
            {  -3.f, 0.60f, 0.12f, 0.50f, 0.50f, 0.60f, 0.40f, 0.40f, 0 }, // 31: Deep Sub FM Kick

            // Bank C: World & Acoustic Percussion (32..47)
            {  -4.f, 0.65f, 0.10f, 0.75f, 0.60f, 0.50f, 0.45f, 0.40f, 0 }, // 32: Floor Tom Sub
            {   0.f, 0.10f, 0.06f, 0.60f, 0.55f, 0.50f, 0.50f, 0.50f, 0 }, // 33: Studio Wood Rim
            {  +5.f, 0.38f, 0.04f, 0.65f, 0.60f, 0.50f, 0.50f, 0.45f, 0 }, // 34: Agogo High Bell
            {  -2.f, 0.48f, 0.04f, 0.35f, 0.55f, 0.50f, 0.50f, 0.45f, 0 }, // 35: Agogo Low Bell
            {  +4.f, 0.16f, 0.10f, 0.85f, 0.70f, 0.40f, 0.65f, 0.60f, 0 }, // 36: Conga Slap
            {  -5.f, 0.45f, 0.04f, 0.30f, 0.40f, 0.80f, 0.35f, 0.40f, 0 }, // 37: Conga Low Mute
            {  +7.f, 0.08f, 0.08f, 0.80f, 0.70f, 0.60f, 0.60f, 0.65f, 0 }, // 38: Hardwood Block Hi
            {  -3.f, 0.14f, 0.06f, 0.45f, 0.50f, 0.75f, 0.45f, 0.45f, 0 }, // 39: Hardwood Block Lo
            {  +5.f, 0.20f, 0.10f, 0.75f, 0.65f, 0.65f, 0.65f, 0.55f, 0 }, // 40: High Disco Cowbell
            {  -4.f, 0.38f, 0.05f, 0.40f, 0.45f, 0.35f, 0.40f, 0.40f, 0 }, // 41: Low Latin Cha-Cha
            {  +4.f, 0.50f, 0.03f, 0.95f, 0.30f, 0.80f, 0.40f, 0.35f, 0 }, // 42: Pure Ride Ping
            {  +3.f, 0.25f, 0.24f, 0.85f, 0.85f, 0.75f, 0.65f, 0.60f, 0 }, // 43: China Choke
            {  +3.f, 0.55f, 0.12f, 0.85f, 0.80f, 0.75f, 0.60f, 0.70f, 0 }, // 44: Fast Splash Wash
            {  +2.f, 0.09f, 0.02f, 0.70f, 0.50f, 0.60f, 0.50f, 0.45f, 0 }, // 45: Air Noise Shaker
            {  +3.f, 0.20f, 0.16f, 0.80f, 0.70f, 0.90f, 0.70f, 0.60f, 0 }, // 46: Acoustic Snare Rim
            {  -4.f, 0.58f, 0.15f, 0.45f, 0.35f, 0.75f, 0.70f, 0.40f, 0 }, // 47: 24-Inch Deep Bass

            // Bank D: Melodic Synths, Acid, Plucks & Cyber FX (48..63)
            {   0.f, 0.65f, 0.08f, 0.50f, 0.60f, 0.50f, 0.50f, 0.50f, 0 }, // 48: Karplus Nylon
            {  +7.f, 0.35f, 0.12f, 0.75f, 0.80f, 0.35f, 0.75f, 0.70f, 0 }, // 49: Karplus Steel Wire
            { -12.f, 0.85f, 0.15f, 0.35f, 0.45f, 0.85f, 0.35f, 0.30f, 0 }, // 50: Karplus Bass Pluck
            {  -2.f, 0.30f, 0.45f, 0.30f, 0.95f, 0.85f, 0.15f, 0.75f, 0 }, // 51: 303 Screaming Reso
            { -12.f, 0.45f, 0.20f, 0.50f, 0.65f, 0.55f, 0.80f, 0.40f, 0 }, // 52: 303 Square Bass
            {   0.f, 0.28f, 0.35f, 0.60f, 0.85f, 0.75f, 0.20f, 0.65f, 0 }, // 53: Acid Rave Lead
            {  +6.f, 0.12f, 0.22f, 0.90f, 0.85f, 0.30f, 0.60f, 0.70f, 0 }, // 54: Space Invader Zap
            {  -4.f, 0.40f, 0.18f, 0.70f, 0.45f, 0.65f, 0.40f, 0.30f, 0 }, // 55: Downer Laser Drop
            { +12.f, 0.10f, 0.12f, 0.95f, 0.90f, 0.25f, 0.65f, 0.80f, 0 }, // 56: Laser Chirp Stereo
            {  +6.f, 0.15f, 0.32f, 0.85f, 0.75f, 0.80f, 0.65f, 0.70f, 0 }, // 57: FM Chaos Static
            {  -6.f, 0.75f, 0.25f, 0.45f, 0.80f, 0.50f, 0.70f, 0.30f, 0 }, // 58: Metallic FM Drone
            {  -4.f, 0.80f, 0.28f, 0.75f, 0.70f, 0.80f, 0.60f, 0.55f, 0 }, // 59: Dubstep Low Rattle
            {  +1.f, 0.55f, 0.65f, 0.95f, 0.85f, 0.80f, 0.80f, 0.75f, 0 }, // 60: Brutal Folded Raw
            {  +3.f, 0.22f, 0.35f, 0.90f, 0.80f, 0.30f, 0.85f, 0.70f, 0 }, // 61: Electro Air Crunch
            {  -2.f, 0.65f, 0.12f, 0.80f, 0.95f, 0.65f, 0.40f, 0.65f, 0 }, // 62: Ambient Hall Clap
            {  -2.f, 1.40f, 0.08f, 0.65f, 0.70f, 0.60f, 0.25f, 0.70f, 0 }  // 63: Plate Shimmer
        };

        const auto& d = kPadDefaults[juce::jlimit(0, kNumPads - 1, p)];
        const float defTune = d.tune;
        const float defDec  = d.dec;
        const float defDrv  = d.drv;
        const float defP1   = d.p1;
        const float defP2   = d.p2;
        const float defP3   = d.p3;
        const float defP4   = d.p4;
        const float defP5   = d.p5;
        const int defChoke  = d.choke;

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

        addI(id("vcft"), n + "VCF Type",  0, 4, 0);
        addF(id("vcfc"), n + "VCF Cut",   { 20.f, 20000.f, 0.1f, 0.25f },   20000.f);
        addF(id("vcfr"), n + "VCF Res",   { 0.1f, 10.f, 0.01f, 0.35f },     0.707f);
        addF(id("vcfe"), n + "VCF Env",   { -1.f, 1.f, 0.001f },            0.0f);
    }

    return { params.begin(), params.end() };
}

// ---------------------------------------------------------------------------
Forge64Processor::Forge64Processor()
    : AudioProcessor(makeBuses()),
      apvts(*this, &undoManager, "PARAMS", makeParams())
{
    ScriptPresetManager::initializePresetsOnDisk();

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

    busScratch.setSize(2 * kNumBuses, maxBlock, false, false, true);
    auxA.setSize(2, maxBlock, false, false, true);
    auxB.setSize(2, maxBlock, false, false, true);
    auxC.setSize(2, maxBlock, false, false, true);
    auxD.setSize(2, maxBlock, false, false, true);
    scratchL.assign((size_t) maxBlock, 0.f);
    scratchR.assign((size_t) maxBlock, 0.f);

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
            if (auto t = pos->getBpm())
                bpm = *t;
            playing = pos->getIsPlaying();
        }
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
        const bool hasEvents = ! padEvents[(size_t) p].empty();
        const bool vActive   = voices.padActive(p);
        if (! hasEvents && ! vActive && padTailHold[(size_t) p] <= 0)
            continue;

        if (vActive || hasEvents)
            padTailHold[(size_t) p] = 20; // Maintain ~200ms ringdown for filter/FX tails and clean fadeout
        else if (padTailHold[(size_t) p] > 0)
            --padTailHold[(size_t) p];

        const bool isLastTailBlock = (! vActive && ! hasEvents && padTailHold[(size_t) p] == 0);

        std::fill(scratchL.begin(), scratchL.begin() + n, 0.f);
        std::fill(scratchR.begin(), scratchR.begin() + n, 0.f);

        auto& rt = gridPtr->runtime(p);
        auto pp = eff[(size_t) p];
        if (rt.hasLocks.load())
        {
            const float modScale = rt.latchedModAmt.load();
            auto& ptrs = padPtrs[(size_t) p];
            auto& ids  = padIds[(size_t) p];

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

            pp.level = applyMod(0,  rt.latchedLevel.load());
            pp.pan   = applyMod(1,  rt.latchedPan.load());
            pp.tune  = applyMod(2,  rt.latchedTune.load());
            pp.decay = applyMod(3,  rt.latchedDecay.load());

            // EQ
            pp.eqLF = applyMod(4, rt.latchedEqLF.load());
            pp.eqLG = applyMod(5, rt.latchedEqLG.load());
            pp.eqMF = applyMod(6, rt.latchedEqMF.load());
            pp.eqMG = applyMod(7, rt.latchedEqMG.load());
            pp.eqHF = applyMod(8, rt.latchedEqHF.load());
            pp.eqHG = applyMod(9, rt.latchedEqHG.load());

            // Compressor
            pp.cThr = applyMod(10, rt.latchedCThr.load());
            pp.cRat = applyMod(11, rt.latchedCRat.load());
            pp.cAtk = applyMod(12, rt.latchedCAtk.load());
            pp.cRel = applyMod(13, rt.latchedCRel.load());

            pp.drive = applyMod(14, rt.latchedDrive.load());

            pp.fx1   = applyMod(16, rt.latchedTone.load());
            pp.fx2   = applyMod(17, rt.latchedP2.load());
            pp.fx3   = applyMod(18, rt.latchedP3.load());
            pp.fx4   = applyMod(19, rt.latchedP4.load());
            pp.fx5   = applyMod(20, rt.latchedP5.load());

            pp.sendA = applyMod(21, rt.latchedSendA.load());
            pp.sendB = applyMod(22, rt.latchedSendB.load());
            pp.sendC = applyMod(23, rt.latchedSendC.load());
            pp.sendD = applyMod(24, rt.latchedSendD.load());

            // Insert Multi-FX
            pp.ifxType = rt.latchedIfxType.load();
            pp.ifx1    = applyMod(36, rt.latchedIfx1.load());
            pp.ifx2    = applyMod(37, rt.latchedIfx2.load());
            pp.ifx3    = applyMod(38, rt.latchedIfx3.load());
            pp.ifx4    = applyMod(39, rt.latchedIfx4.load());

            // VCF
            pp.vcfType = rt.latchedVcfType.load();
            pp.vcfCut  = applyMod(41, rt.latchedVcfCut.load());
            pp.vcfRes  = applyMod(42, rt.latchedVcfRes.load());
            pp.vcfEnv  = applyMod(43, rt.latchedVcfEnv.load());
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

        voices.renderPad(p, scratchL.data(), scratchR.data(), n, sr, pp, padEvents[(size_t) p]);

        if (rt.scriptOn.load() || pp.srcType != SRC_SAMPLE)
        {
            const int64_t hit = rt.lastHitStamp.load();
            const double age = hit > 0 ? (double) (clockNow - hit) / sr : 0.0;
            PadParams ppLua = pp;
            if (pp.mode == 1) // Chromatic mode: transpose by incoming MIDI note relative to base note
            {
                ppLua.tune += (float) (rt.lastNote.load() - pp.mnote);
            }
            luaEngine.process(p, scratchL.data(), scratchR.data(), n, sr,
                              rt.lastVel.load(), juce::jmax(0.0, age), ppLua, isNewTrig,
                              rt.lastNote.load());
        }

        const float gt = rt.gainTrim.load();
        chains[(size_t) p].process(scratchL.data(), scratchR.data(), n, pp);

        if (isLastTailBlock)
        {
            const int fadeLen = juce::jmin(n, 64);
            for (int i = 0; i < fadeLen; ++i)
            {
                const float g = 0.5f * (1.0f + std::cos((float) i * juce::MathConstants<float>::pi / (float) fadeLen));
                scratchL[(size_t) i] *= g;
                scratchR[(size_t) i] *= g;
            }
            for (int i = fadeLen; i < n; ++i)
            {
                scratchL[(size_t) i] = 0.f;
                scratchR[(size_t) i] = 0.f;
            }
        }

        const float lvl = pp.level * master * gt * 0.75f; // Nominal -2.5 dB summing headroom
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
            const float l = scratchL[(size_t) i] * lvl * panL;
            const float r = scratchR[(size_t) i] * lvl * panR;
            bL[i] += l;
            bR[i] += r;
            if (sa > 0.f) { aL[i] += l * sa; aR[i] += r * sa; }
            if (sb > 0.f) { bL_aux[i] += l * sb; bR_aux[i] += r * sb; }
            if (sc > 0.f) { cL[i] += l * sc; cR[i] += r * sc; }
            if (sd > 0.f) { dL[i] += l * sd; dR[i] += r * sd; }
        }
    }

    // 6) 4 Aux Send Buses processed through AuxBusManager
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
    // Save midi learn and sequencer into kitRoot before serializing
    kitRoot.removeChild(kitRoot.getChildWithName("MIDI_LEARN"), nullptr);
    kitRoot.appendChild(midiLearn.serialize(), nullptr);
    kitRoot.removeChild(kitRoot.getChildWithName("SEQUENCER"), nullptr);
    kitRoot.appendChild(sequencer.serialize(), nullptr);

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
        sequencer.deserialize(seq);

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
