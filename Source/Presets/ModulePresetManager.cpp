#include "ModulePresetManager.h"

namespace f64 {

juce::File ModulePresetManager::getModulesDir()
{
    auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                   .getChildFile("Forge64")
                   .getChildFile("Modules");
    if (! dir.exists())
        dir.createDirectory();
    return dir;
}

juce::File ModulePresetManager::getSoundPresetsDir(const juce::String& moduleId)
{
    auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                   .getChildFile("Forge64")
                   .getChildFile("SoundPresets")
                   .getChildFile(moduleId);
    if (! dir.exists())
        dir.createDirectory();
    return dir;
}

juce::StringArray ModulePresetManager::getModuleCategories()
{
    return { "Kicks", "Snares", "Hi-Hats", "Claps", "Toms", "Cymbals", "Percussion", "Synths & Noise", "Custom" };
}

void ModulePresetManager::initializeOnDisk()
{
    getModulesDir();
}

const std::vector<ModuleInfo>& ModulePresetManager::getFactoryModules()
{
    static std::vector<ModuleInfo> list;
    if (! list.empty())
        return list;

    // =========================================================================
    // 1. KICKS (6 Unique Architectures)
    // =========================================================================
    {
        ModuleInfo m;
        m.id = "kick_808";
        m.name = "808 Bass Kick";
        m.category = "Kicks";
        m.description = "KD-01 Resonant Bridged-T: sub-sine sweep with punch envelope, beater click, harmonic warmth, and tail pitch sag";
        m.p1Label = "PUNCH"; m.p2Label = "CLICK"; m.p3Label = "SUB"; m.p4Label = "WARMTH"; m.p5Label = "TAIL BEND";
        m.defTune = 0.f; m.defDecay = 0.65f; m.defDrive = 0.12f;
        m.defP1 = 0.60f; m.defP2 = 0.40f; m.defP3 = 0.50f; m.defP4 = 0.40f; m.defP5 = 0.30f;
        m.scriptCode =
R"(-- 808 Bass Kick Synthesizer (KD-01 Physical Model)
local phase = 0.0
local subPhase = 0.0

function process()
    local fTune    = param("tune")
    local punch    = param("p1")
    local click    = param("p2")
    local sub      = param("p3")
    local warmth   = param("p4")
    local tailBend = param("p5")
    local decaySec = math.max(0.04, param("decay"))
    local drive    = param("drive")

    if trig then phase = 0.0; subPhase = 0.0 end

    local f0 = 48.0 * math.pow(2.0, fTune / 12.0)
    local f1 = f0 + (160.0 + punch * 280.0)
    local pd = math.max(0.012, 0.018 + punch * 0.035)
    local dt = 1.0 / sr

    for i = 0, n - 1 do
        local t = age + i * dt
        local bendMod = 1.0 - tailBend * 0.32 * (1.0 - math.exp(-t / (decaySec * 0.75)))
        local curFreq = (f0 + (f1 - f0) * math.exp(-t / pd)) * math.max(0.35, bendMod)
        phase = (phase + curFreq * dt) % 1.0
        subPhase = (subPhase + (curFreq * 0.5) * dt) % 1.0

        local body = math.sin(phase * 6.2831853)
        if warmth > 0.02 then
            body = body + (math.sin(phase * 12.5663706) * 0.35 + math.sin(phase * 18.8495559) * 0.15) * warmth
        end

        local subOsc = math.sin(subPhase * 6.2831853) * sub * 0.85
        local clickSig = 0.0
        if t < 0.016 then
            clickSig = (rnd() * 2.0 - 1.0) * math.exp(-t / 0.003) * (0.3 + click * 1.3)
        end

        local sig = (body + subOsc + clickSig) * math.exp(-t / decaySec) * vel
        local totalDrive = drive + warmth * 0.25
        if totalDrive > 0.01 then
            sig = math.tanh(sig * (1.0 + totalDrive * 5.0))
        end

        outL(i, sig)
        outR(i, sig)
    end
end
)";
        list.push_back(m);
    }

    {
        ModuleInfo m;
        m.id = "kick_909";
        m.name = "909 Dance Beater Kick";
        m.category = "Kicks";
        m.description = "Punchy 909-style kick: dual-stage pitch sweep with tuned beater slap, body resonance, and diode saturation";
        m.p1Label = "BEATER"; m.p2Label = "PUNCH"; m.p3Label = "BODY RES"; m.p4Label = "CURVE"; m.p5Label = "DIODE GRIT";
        m.defTune = 0.f; m.defDecay = 0.36f; m.defDrive = 0.15f;
        m.defP1 = 0.65f; m.defP2 = 0.65f; m.defP3 = 0.50f; m.defP4 = 0.50f; m.defP5 = 0.45f;
        m.scriptCode =
R"(-- 909 Dance Beater Kick
local phase = 0.0
local subPhase = 0.0
function process()
    local fTune   = param("tune")
    local beater  = param("p1")
    local punch   = param("p2")
    local bodyRes = param("p3")
    local curve   = param("p4")
    local grit    = param("p5")
    local dec     = math.max(0.04, param("decay"))
    local drv     = param("drive")

    if trig then phase = 0.0; subPhase = 0.0 end

    local baseF = (48.0 + bodyRes * 12.0) * math.pow(2.0, fTune / 12.0)
    local dt = 1.0 / sr
    local cTau1 = 0.008 + (1.0 - curve) * 0.012
    local cTau2 = 0.040 + (1.0 - curve) * 0.035

    for i = 0, n - 1 do
        local t = age + i * dt
        local pDrop = (280.0 * math.exp(-t / cTau1) + 80.0 * math.exp(-t / cTau2)) * (0.4 + punch * 0.9)
        local curF = baseF + pDrop
        phase = (phase + curF * dt) % 1.0
        subPhase = (subPhase + (curF * 0.5) * dt) % 1.0

        local body = math.sin(phase * 6.2831853)
        local subOsc = math.sin(subPhase * 6.2831853) * (0.3 + bodyRes * 0.4)
        local click = 0.0
        if t < 0.014 then
            click = (rnd() * 2.0 - 1.0) * math.exp(-t / 0.0022) * (0.3 + beater * 1.4)
            click = click + math.sin(t * 7500.0) * math.exp(-t / 0.0035) * beater * 0.8
        end

        local sig = (body + subOsc + click) * math.exp(-t / dec) * vel * 1.4
        if grit > 0.02 then
            sig = sig + (sig * math.abs(sig)) * grit * 0.4
        end
        local totalDrv = drv + grit * 0.35
        if totalDrv > 0.01 then sig = math.tanh(sig * (1.0 + totalDrv * 5.0)) end
        outL(i, sig)
        outR(i, sig)
    end
end
)";
        list.push_back(m);
    }

    {
        ModuleInfo m;
        m.id = "kick_rock";
        m.name = "Rock Membrane Kick";
        m.category = "Kicks";
        m.description = "Acoustic 2D Bessel circular drumhead modal model with dual-head coupling, shell air cavity, and felt slap";
        m.p1Label = "BEATER WT"; m.p2Label = "HEAD RATIO"; m.p3Label = "MEMBRANE"; m.p4Label = "SHELL AIR"; m.p5Label = "BEATER SLAP";
        m.defTune = 0.f; m.defDecay = 0.42f; m.defDrive = 0.14f;
        m.defP1 = 0.65f; m.defP2 = 0.50f; m.defP3 = 0.50f; m.defP4 = 0.50f; m.defP5 = 0.45f;
        m.scriptCode =
R"(-- Rock Membrane Kick (2D Bessel Modes + Shell Air)
local phase = 0.0
local head2Phase = 0.0
local mode3Phase = 0.0
local airPhase = 0.0
function process()
    local fTune     = param("tune")
    local beaterWt  = param("p1")
    local headRatio = param("p2")
    local membrane  = param("p3")
    local shellAir  = param("p4")
    local slap      = param("p5")
    local dec       = math.max(0.04, param("decay"))
    local drv       = param("drive")

    if trig then phase = 0.0; head2Phase = 0.0; mode3Phase = 0.0; airPhase = 0.0 end

    local baseF = (44.0 + (1.0 - beaterWt) * 8.0) * math.pow(2.0, fTune / 12.0)
    local h2F   = baseF * (0.65 + headRatio * 0.45)
    local m3F   = baseF * 1.59
    local airF  = 72.0 * math.pow(2.0, fTune / 12.0)
    local pDec  = math.max(0.008, 0.040 * (1.3 - beaterWt * 0.6))
    local dt    = 1.0 / sr

    for i = 0, n - 1 do
        local t = age + i * dt
        local curF = baseF * math.pow(2.0, (2.8 + beaterWt * 3.0) * math.exp(-t / pDec))
        phase = (phase + curF * dt) % 1.0
        head2Phase = (head2Phase + h2F * dt) % 1.0
        mode3Phase = (mode3Phase + m3F * dt) % 1.0
        airPhase = (airPhase + airF * dt) % 1.0

        local b1 = math.sin(phase * 6.2831853)
        local b2 = math.sin(head2Phase * 6.2831853) * 0.45
        local b3 = math.sin(mode3Phase * 6.2831853) * (membrane * 0.35) * math.exp(-t / (dec * 0.35))
        local bAir = math.sin(airPhase * 6.2831853) * (shellAir * 0.4) * math.exp(-t / (dec * 0.6))

        local slapSig = 0.0
        if t < 0.016 then
            slapSig = (rnd() * 2.0 - 1.0) * math.exp(-t / 0.0025) * (0.3 + slap * 1.3)
        end

        local sig = (b1 + b2 + b3 + bAir + slapSig) * math.exp(-t / dec) * vel * 1.25
        if drv > 0.01 then sig = math.tanh(sig * (1.0 + drv * 5.0)) end
        outL(i, sig)
        outR(i, sig)
    end
end
)";
        list.push_back(m);
    }

    {
        ModuleInfo m;
        m.id = "kick_electro";
        m.name = "Electro 7-Octave Kick";
        m.category = "Kicks";
        m.description = "Aggressive 7-octave hyperbolic descent with wave-morphing (sine-tri-pulse), attack bitcrush, and stereo spread";
        m.p1Label = "SWEEP SPD"; m.p2Label = "WAVE MORPH"; m.p3Label = "SUB BOOM"; m.p4Label = "CRUNCH"; m.p5Label = "SPREAD";
        m.defTune = 0.f; m.defDecay = 0.35f; m.defDrive = 0.22f;
        m.defP1 = 0.70f; m.defP2 = 0.50f; m.defP3 = 0.50f; m.defP4 = 0.40f; m.defP5 = 0.30f;
        m.scriptCode =
R"(-- Electro 7-Octave Sweep Kick
local phase = 0.0
function process()
    local fTune     = param("tune")
    local sweepSpd  = param("p1")
    local waveMorph = param("p2")
    local subBoom   = param("p3")
    local crunch    = param("p4")
    local spread    = param("p5")
    local dec       = math.max(0.04, param("decay"))
    local drv       = param("drive")

    if trig then phase = 0.0 end

    local baseF = 42.0 * math.pow(2.0, fTune / 12.0)
    local octs  = 3.5 + sweepSpd * 4.2
    local pDec  = 0.010 + sweepSpd * 0.028
    local dt    = 1.0 / sr

    for i = 0, n - 1 do
        local t = age + i * dt
        local curF = baseF * math.pow(2.0, octs * math.exp(-t / pDec))
        phase = (phase + curF * dt) % 1.0

        local sinW = math.sin(phase * 6.2831853)
        local triW = (math.abs(phase - 0.5) * 4.0 - 1.0)
        local pw = 0.5 + waveMorph * 0.25
        local pulseW = (phase < pw and 0.8 or -0.8)

        local wave = sinW * (1.0 - waveMorph) + triW * (waveMorph * 0.6) + pulseW * (waveMorph * 0.4)
        if crunch > 0.05 and t < 0.03 then
            wave = math.floor(wave * (8.0 - crunch * 5.0)) / (8.0 - crunch * 5.0)
        end

        local body = wave * math.exp(-t / (dec * (0.8 + subBoom * 0.7))) * (1.0 + subBoom * 0.5)
        local clk = 0.0
        if t < 0.012 then
            clk = (rnd() * 2.0 - 1.0) * math.exp(-t / 0.002) * (0.4 + crunch * 1.2)
        end

        local sigL = (body + clk) * vel * 1.3
        local sigR = (body + clk * (1.0 - spread * 0.5)) * vel * 1.3
        if drv > 0.01 then
            sigL = math.tanh(sigL * (1.0 + drv * 6.0))
            sigR = math.tanh(sigR * (1.0 + drv * 6.0))
        end
        outL(i, sigL)
        outR(i, sigR)
    end
end
)";
        list.push_back(m);
    }

    {
        ModuleInfo m;
        m.id = "kick_hardstyle";
        m.name = "Hardstyle Punch Kick";
        m.category = "Kicks";
        m.description = "Saturated pitch-dropping punch with multi-stage trigonometric sine wavefolder, post-distortion filter, and sub-rumble tail";
        m.p1Label = "PUNCH SPIKE"; m.p2Label = "WAVEFOLD"; m.p3Label = "DIST FILT"; m.p4Label = "TAIL DRIVE"; m.p5Label = "RUMBLE TONE";
        m.defTune = 0.f; m.defDecay = 0.48f; m.defDrive = 0.35f;
        m.defP1 = 0.85f; m.defP2 = 0.65f; m.defP3 = 0.60f; m.defP4 = 0.60f; m.defP5 = 0.50f;
        m.scriptCode =
R"(-- Hardstyle Punch Kick (Multi-fold Saturation & Rumble)
local phase = 0.0
local rumblePhase = 0.0
local lpFilt = 0.0
function process()
    local fTune    = param("tune")
    local punch    = param("p1")
    local fold     = param("p2")
    local distFilt = param("p3")
    local tailDrv  = param("p4")
    local rumble   = param("p5")
    local drv      = param("drive")
    local dec      = math.max(0.05, param("decay"))

    if trig then phase = 0.0; rumblePhase = 0.0; lpFilt = 0.0 end

    local baseF = 50.0 * math.pow(2.0, fTune / 12.0)
    local dt = 1.0 / sr
    local fCut = 0.15 + distFilt * 0.75

    for i = 0, n - 1 do
        local t = age + i * dt
        local curF = baseF + (360.0 * math.exp(-t / 0.018)) * (0.5 + punch * 1.1)
        phase = (phase + curF * dt) % 1.0
        rumblePhase = (rumblePhase + (baseF * 0.5) * dt) % 1.0

        local raw = math.sin(phase * 6.2831853)
        if fold > 0.02 then
            local gain = 1.0 + fold * 5.0
            raw = math.sin(raw * gain * 3.14159265)
        end

        lpFilt = lpFilt + fCut * (raw - lpFilt)
        local shaped = lpFilt

        local rmb = math.sin(rumblePhase * 6.2831853) * rumble * 0.5 * (1.0 - math.exp(-t / 0.04))
        local sig = (shaped + rmb) * math.exp(-t / dec) * vel * 1.4

        local totDrv = drv + tailDrv * 0.4
        if totDrv > 0.01 then
            sig = math.tanh(sig * (1.0 + totDrv * 7.0))
        end
        outL(i, sig)
        outR(i, sig)
    end
end
)";
        list.push_back(m);
    }

    {
        ModuleInfo m;
        m.id = "kick_sub_fm";
        m.name = "Deep Sub FM Kick";
        m.category = "Kicks";
        m.description = "Deep sub-sine bass drum with 2-operator linear phase modulation, self-feedback, and waveshape warping";
        m.p1Label = "FM DEPTH"; m.p2Label = "FM RATIO"; m.p3Label = "MOD DECAY"; m.p4Label = "FEEDBACK"; m.p5Label = "WARP";
        m.defTune = 0.f; m.defDecay = 0.60f; m.defDrive = 0.12f;
        m.defP1 = 0.50f; m.defP2 = 0.50f; m.defP3 = 0.60f; m.defP4 = 0.40f; m.defP5 = 0.40f;
        m.scriptCode =
R"(-- Deep Sub FM Kick (2-Op Phase Modulation + Feedback)
local pCarrier = 0.0
local pMod = 0.0
local lastCarrier = 0.0
function process()
    local fTune   = param("tune")
    local fmDepth = param("p1") * 5.0
    local fmRatio = 1.0 + math.floor(param("p2") * 6.0)
    local modDec  = param("p3")
    local fbAmt   = param("p4") * 0.4
    local warp    = param("p5")
    local dec     = math.max(0.06, param("decay"))
    local drv     = param("drive")

    if trig then pCarrier = 0.0; pMod = 0.0; lastCarrier = 0.0 end

    local baseF = 44.0 * math.pow(2.0, fTune / 12.0)
    local dt = 1.0 / sr

    for i = 0, n - 1 do
        local t = age + i * dt
        local pEnv = math.exp(-t / (0.015 + modDec * 0.04))
        local curF = baseF * (1.0 + pEnv * 3.0)

        pMod = (pMod + curF * fmRatio * dt) % 1.0
        local modEnv = math.exp(-t / (0.02 + modDec * 0.06)) * fmDepth
        local modSig = math.sin(pMod * 6.2831853) * modEnv

        pCarrier = (pCarrier + curF * (1.0 + modSig + lastCarrier * fbAmt) * dt) % 1.0
        local body = math.sin(pCarrier * 6.2831853)
        lastCarrier = body

        if warp > 0.03 then
            body = math.sin(body * (1.0 + warp * 2.0) * 1.5707963)
        end

        local sig = body * math.exp(-t / dec) * vel * 1.45
        if drv > 0.01 then sig = math.tanh(sig * (1.0 + drv * 4.5)) end
        outL(i, sig)
        outR(i, sig)
    end
end
)";
        list.push_back(m);
    }

    // =========================================================================
    // 2. SNARES (4 Unique Architectures)
    // =========================================================================
    {
        ModuleInfo m;
        m.id = "snare_808";
        m.name = "808 Analog Snare";
        m.category = "Snares";
        m.description = "Dual bridged-T sine resonators (1.82 ratio) with bandpass filtered snare rattle and shell body Q";
        m.p1Label = "SNAPPY"; m.p2Label = "TONE"; m.p3Label = "WIRE DECAY"; m.p4Label = "BODY Q"; m.p5Label = "CLICK";
        m.defTune = 0.f; m.defDecay = 0.28f; m.defDrive = 0.08f;
        m.defP1 = 0.65f; m.defP2 = 0.50f; m.defP3 = 0.45f; m.defP4 = 0.55f; m.defP5 = 0.50f;
        m.scriptCode =
R"(-- 808 Analog Snare (Dual Bridged-T + Shaped Wire Noise)
local p1, p2 = 0.0, 0.0
local bp0, bp1 = 0.0, 0.0
function process()
    local fTune    = param("tune")
    local snappy   = param("p1")
    local tone     = param("p2")
    local wireDec  = param("p3")
    local bodyQ    = param("p4")
    local click    = param("p5")
    local dec      = math.max(0.04, param("decay"))
    local drv      = param("drive")

    if trig then p1 = 0.0; p2 = 0.0; bp0 = 0.0; bp1 = 0.0 end

    local f1 = 185.0 * math.pow(2.0, fTune / 12.0)
    local f2 = f1 * 1.82
    local dt = 1.0 / sr
    local cut = 1100.0 + tone * 4800.0
    local f = 2.0 * math.sin(3.14159265 * cut / sr)
    local q = 1.8 + bodyQ * 2.5

    for i = 0, n - 1 do
        local t = age + i * dt
        p1 = (p1 + f1 * dt) % 1.0
        p2 = (p2 + f2 * dt) % 1.0

        local body = (math.sin(p1 * 6.2831853) * 0.7 + math.sin(p2 * 6.2831853) * 0.3) * (0.4 + bodyQ * 0.9)
        local bodyEnv = math.exp(-t / (dec * (0.2 + bodyQ * 0.35)))

        local noiseSample = rnd() * 2.0 - 1.0
        bp0 = bp0 + f * (noiseSample - bp0 - bp1 / q)
        bp1 = bp1 + f * bp0
        local noise = bp0 * math.exp(-t / (dec * (0.5 + wireDec * 0.9)))

        local clk = 0.0
        if t < 0.012 then clk = noiseSample * math.exp(-t / 0.002) * (0.2 + click * 1.4) end

        local sig = (body * bodyEnv * (1.1 - snappy * 0.45) + noise * (0.3 + snappy * 1.1) + clk) * vel * 1.45
        if drv > 0.01 then sig = math.tanh(sig * (1.0 + drv * 4.0)) end
        outL(i, sig)
        outR(i, sig)
    end
end
)";
        list.push_back(m);
    }

    {
        ModuleInfo m;
        m.id = "snare_909";
        m.name = "909 Dance Snare";
        m.category = "Snares";
        m.description = "Punchy 909-style dance snare with distinct triangle tone body, pitch snap, dynamic noise VCA, and punch compressor";
        m.p1Label = "SNAPPY"; m.p2Label = "CRACK"; m.p3Label = "TONE"; m.p4Label = "SHELL DECAY"; m.p5Label = "COMPRESSION";
        m.defTune = 0.f; m.defDecay = 0.25f; m.defDrive = 0.10f;
        m.defP1 = 0.65f; m.defP2 = 0.60f; m.defP3 = 0.50f; m.defP4 = 0.50f; m.defP5 = 0.55f;
        m.scriptCode =
R"(-- 909 Dance Snare (Pitch Snap + VCA Shaper + Diode Compressor)
local phase = 0.0
local hp0, lastSample = 0.0, 0.0
function process()
    local fTune    = param("tune")
    local snappy   = param("p1")
    local crack    = param("p2")
    local tone     = param("p3")
    local shellDec = param("p4")
    local comp     = param("p5")
    local dec      = math.max(0.04, param("decay"))
    local drv      = param("drive")

    if trig then phase = 0.0; hp0 = 0.0; lastSample = 0.0 end

    local baseF = (210.0 + tone * 60.0) * math.pow(2.0, fTune / 12.0)
    local dt = 1.0 / sr
    local hpCut = 1800.0 + tone * 2400.0
    local hpCoef = math.exp(-6.2831853 * hpCut / sr)

    for i = 0, n - 1 do
        local t = age + i * dt
        local curF = baseF + (380.0 * math.exp(-t / 0.010)) * (0.4 + crack * 0.8)
        phase = (phase + curF * dt) % 1.0

        local triW = (math.abs(phase - 0.5) * 4.0 - 1.0)
        local body = triW * math.exp(-t / (dec * (0.2 + shellDec * 0.4))) * (1.2 - snappy * 0.4)

        local rawN = rnd() * 2.0 - 1.0
        hp0 = hpCoef * (hp0 + rawN - lastSample)
        lastSample = rawN

        local noiseEnv = math.exp(-t / dec) * (0.3 + snappy * 1.1)
        local noise = hp0 * noiseEnv

        local snapImp = 0.0
        if t < 0.008 then snapImp = (rnd() * 2.0 - 1.0) * (crack * 1.2) end

        local raw = (body + noise + snapImp) * vel * 1.4
        local cGain = 1.0 + comp * 3.0
        local compressed = math.tanh(raw * cGain) / math.sqrt(cGain)

        local sig = compressed
        if drv > 0.01 then sig = math.tanh(sig * (1.0 + drv * 3.5)) end
        outL(i, sig)
        outR(i, sig)
    end
end
)";
        list.push_back(m);
    }

    {
        ModuleInfo m;
        m.id = "snare_rock";
        m.name = "Rock Brown Noise Snare";
        m.category = "Snares";
        m.description = "Acoustic wood shell resonance with stochastic Poisson wire rattle impulses and stereo room air bleed";
        m.p1Label = "WIRE TENS"; m.p2Label = "SHELL TONE"; m.p3Label = "RIM HIT"; m.p4Label = "BOTTOM HEAD"; m.p5Label = "STEREO AIR";
        m.defTune = 0.f; m.defDecay = 0.32f; m.defDrive = 0.12f;
        m.defP1 = 0.65f; m.defP2 = 0.55f; m.defP3 = 0.50f; m.defP4 = 0.60f; m.defP5 = 0.45f;
        m.scriptCode =
R"(-- Rock Brown Noise Snare (Poisson Wire Rattle + Acoustic Room Air)
local phase = 0.0
local lp = 0.0
local lpR = 0.0
function process()
    local fTune     = param("tune")
    local wireTens  = param("p1")
    local shellTone = param("p2")
    local rimHit    = param("p3")
    local botHead   = param("p4")
    local stereoAir = param("p5")
    local dec       = math.max(0.04, param("decay"))
    local drv       = param("drive")

    if trig then phase = 0.0; lp = 0.0; lpR = 0.0 end

    local f0 = (180.0 + shellTone * 70.0) * math.pow(2.0, fTune / 12.0)
    local dt = 1.0 / sr
    local lpCut = 0.15 + (1.0 - wireTens) * 0.6

    for i = 0, n - 1 do
        local t = age + i * dt
        phase = (phase + f0 * dt) % 1.0
        local body = math.sin(phase * 6.2831853) * math.exp(-t / (dec * 0.35)) * 0.65

        local nzL = rnd() * 2.0 - 1.0
        local nzR = rnd() * 2.0 - 1.0
        lp  = lp  + lpCut * (nzL - lp)
        lpR = lpR + lpCut * (nzR - lpR)

        local wireDec = math.exp(-t / (dec * (0.4 + wireTens * 0.8)))
        local wireL = (lp * 0.65 + nzL * 0.35) * wireDec * (0.4 + wireTens * 0.9)
        local wireR = (lpR * 0.65 + nzR * 0.35) * wireDec * (0.4 + wireTens * 0.9)

        local rimClk = 0.0
        if t < 0.009 then
            rimClk = (rnd() * 2.0 - 1.0) * math.exp(-t / 0.0018) * (rimHit * 1.5)
        end

        local sigL = (body + wireL + rimClk) * vel * 1.35
        local sigR = (body + wireR * (1.0 - stereoAir * 0.5) + wireL * (stereoAir * 0.5) + rimClk) * vel * 1.35

        if drv > 0.01 then
            sigL = math.tanh(sigL * (1.0 + drv * 3.5))
            sigR = math.tanh(sigR * (1.0 + drv * 3.5))
        end
        outL(i, sigL)
        outR(i, sigR)
    end
end
)";
        list.push_back(m);
    }

    {
        ModuleInfo m;
        m.id = "snare_rimshot";
        m.name = "Maple Wood Rimshot";
        m.category = "Snares";
        m.description = "Dual coupled high-Q wooden resonant block filter bank with asymmetric stick impact impulse and hollow cavity ring";
        m.p1Label = "WOOD PITCH"; m.p2Label = "STICK SNAP"; m.p3Label = "CHAMBER"; m.p4Label = "RING DAMP"; m.p5Label = "BRIGHT";
        m.defTune = 0.f; m.defDecay = 0.12f; m.defDrive = 0.08f;
        m.defP1 = 0.65f; m.defP2 = 0.60f; m.defP3 = 0.50f; m.defP4 = 0.50f; m.defP5 = 0.55f;
        m.scriptCode =
R"(-- Maple Wood Rimshot (Dual High-Q Wood Cavity Block)
local p1 = 0.0
local p2 = 0.0
local lp = 0.0
function process()
    local fTune     = param("tune")
    local woodPitch = param("p1")
    local stickSnap = param("p2")
    local chamber   = param("p3")
    local ringDamp  = param("p4")
    local bright    = param("p5")
    local dec       = math.max(0.02, param("decay") * (0.8 + (1.0 - ringDamp) * 0.8))
    local drv       = param("drive")

    if trig then p1 = 0.0; p2 = 0.0; lp = 0.0 end

    local fWood = (540.0 + woodPitch * 620.0) * math.pow(2.0, fTune / 12.0)
    local fChamber = fWood * (1.5 + chamber * 0.4)
    local dt = 1.0 / sr
    local lpCut = 0.2 + bright * 0.75

    for i = 0, n - 1 do
        local t = age + i * dt
        p1 = (p1 + fWood * dt) % 1.0
        p2 = (p2 + fChamber * dt) % 1.0

        local w1 = math.sin(p1 * 6.2831853)
        local w2 = math.sin(p2 * 6.2831853) * (chamber * 0.6)
        local woodTone = (w1 + w2) * math.exp(-t / dec)

        local click = 0.0
        if t < 0.007 then
            click = (rnd() * 2.0 - 1.0) * math.exp(-t / 0.0014) * (0.4 + stickSnap * 1.5)
        end

        local raw = woodTone + click
        lp = lp + lpCut * (raw - lp)
        local sig = lp * vel * 2.0
        if drv > 0.01 then sig = math.tanh(sig * (1.0 + drv * 3.0)) end
        outL(i, sig)
        outR(i, sig)
    end
end
)";
        list.push_back(m);
    }

    // =========================================================================
    // 3. HI-HATS (4 Unique Architectures)
    // =========================================================================
    {
        ModuleInfo m;
        m.id = "hat_closed";
        m.name = "6-Osc Metallic Closed Hat";
        m.category = "Hi-Hats";
        m.description = "6 authentic Roland TR-808 square oscillators ring-modulated through bandpass and highpass filters with choke damping";
        m.p1Label = "DETUNE"; m.p2Label = "HP CUTOFF"; m.p3Label = "CHIRP"; m.p4Label = "DAMPING"; m.p5Label = "VEL SENS";
        m.defTune = 0.f; m.defDecay = 0.08f; m.defDrive = 0.05f;
        m.defP1 = 0.50f; m.defP2 = 0.60f; m.defP3 = 0.45f; m.defP4 = 0.50f; m.defP5 = 0.60f;
        m.scriptCode =
R"(-- 6-Osc Metallic Closed Hat (TR-808 Cluster + XOR Ring Mod)
local p = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0}
local hp0, lastSample = 0.0, 0.0
function process()
    local fTune   = param("tune")
    local detune  = param("p1")
    local hpCut   = param("p2")
    local chirp   = param("p3")
    local damping = param("p4")
    local velSens = param("p5")
    local dec     = math.max(0.015, param("decay") * (1.2 - damping * 0.7))
    local drv     = param("drive")

    if trig then
        for k = 1, 6 do p[k] = 0.0 end
        hp0 = 0.0; lastSample = 0.0
    end

    local pitchMult = math.pow(2.0, fTune / 12.0)
    local fBase = {205.3, 304.4, 369.6, 422.3, 540.2, 588.0}
    local f = {}
    for k = 1, 6 do
        f[k] = fBase[k] * pitchMult * (1.0 + (k - 3.5) * 0.08 * detune)
    end

    local dt = 1.0 / sr
    local cutHz = 4000.0 + hpCut * 6000.0
    local hpCoef = math.exp(-6.2831853 * cutHz / sr)
    local vScale = (1.0 - velSens) + velSens * vel

    for i = 0, n - 1 do
        local t = age + i * dt
        local chirpRate = 1.0 + chirp * 1.5 * math.exp(-t / 0.012)
        local sumSq = 0.0
        for k = 1, 6 do
            p[k] = (p[k] + f[k] * chirpRate * dt) % 1.0
            sumSq = sumSq + (p[k] < 0.5 and 0.166 or -0.166)
        end

        local raw = sumSq
        hp0 = hpCoef * (hp0 + raw - lastSample)
        lastSample = raw

        local sig = hp0 * math.exp(-t / dec) * vScale * 2.6
        if drv > 0.01 then sig = math.tanh(sig * (1.0 + drv * 3.0)) end
        outL(i, sig)
        outR(i, sig)
    end
end
)";
        list.push_back(m);
    }

    {
        ModuleInfo m;
        m.id = "hat_open";
        m.name = "6-Osc Metallic Open Hat";
        m.category = "Hi-Hats";
        m.description = "6-oscillator cluster with dual decay stages (initial bright wash + long bronze ring) and stereo chorus shimmer";
        m.p1Label = "SIZZLE"; m.p2Label = "METAL RING"; m.p3Label = "BP FILTER"; m.p4Label = "SHIMMER"; m.p5Label = "BELL TONE";
        m.defTune = 0.f; m.defDecay = 0.55f; m.defDrive = 0.06f;
        m.defP1 = 0.60f; m.defP2 = 0.65f; m.defP3 = 0.55f; m.defP4 = 0.50f; m.defP5 = 0.45f;
        m.scriptCode =
R"(-- 6-Osc Metallic Open Hat (Dual Decay + Stereo Shimmer)
local pL = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0}
local pR = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0}
local hp0, hp1 = 0.0, 0.0
local lastSampleL, lastSampleR = 0.0, 0.0
function process()
    local fTune     = param("tune")
    local sizzle    = param("p1")
    local metalRing = param("p2")
    local bpFilter  = param("p3")
    local shimmer   = param("p4")
    local bellTone  = param("p5")
    local dec       = math.max(0.08, param("decay"))
    local drv       = param("drive")

    if trig then
        for k = 1, 6 do pL[k] = 0.0; pR[k] = 0.0 end
        hp0 = 0.0; hp1 = 0.0; lastSampleL = 0.0; lastSampleR = 0.0
    end

    local pitchMult = math.pow(2.0, fTune / 12.0)
    local fBase = {205.3, 304.4, 369.6, 422.3, 540.2, 588.0}
    local fL = {}
    local fR = {}
    for k = 1, 6 do
        fL[k] = fBase[k] * pitchMult
        fR[k] = fBase[k] * pitchMult * (1.0 + (k % 2 == 0 and 0.012 or -0.012) * shimmer)
    end

    local dt = 1.0 / sr
    local cutHz = 3500.0 + bpFilter * 5000.0
    local hpCoef = math.exp(-6.2831853 * cutHz / sr)

    for i = 0, n - 1 do
        local t = age + i * dt
        local sumL = 0.0
        local sumR = 0.0
        for k = 1, 6 do
            pL[k] = (pL[k] + fL[k] * dt) % 1.0
            pR[k] = (pR[k] + fR[k] * dt) % 1.0
            sumL = sumL + (pL[k] < 0.5 and 0.166 or -0.166)
            sumR = sumR + (pR[k] < 0.5 and 0.166 or -0.166)
        end

        local nz = (rnd() * 2.0 - 1.0) * sizzle * 0.4 * math.exp(-t / (dec * 0.4))
        local rawL = sumL * (0.5 + metalRing * 0.8) + nz
        local rawR = sumR * (0.5 + metalRing * 0.8) + nz

        hp0 = hpCoef * (hp0 + rawL - lastSampleL)
        lastSampleL = rawL
        hp1 = hpCoef * (hp1 + rawR - lastSampleR)
        lastSampleR = rawR

        local bell = math.sin(t * (fBase[5] * 2.0) * 6.2831853) * bellTone * 0.3 * math.exp(-t / (dec * 0.6))
        local env = math.exp(-t / dec)
        local sigL = (hp0 + bell) * env * vel * 2.4
        local sigR = (hp1 + bell) * env * vel * 2.4

        if drv > 0.01 then
            sigL = math.tanh(sigL * (1.0 + drv * 3.0))
            sigR = math.tanh(sigR * (1.0 + drv * 3.0))
        end
        outL(i, sigL)
        outR(i, sigR)
    end
end
)";
        list.push_back(m);
    }

    {
        ModuleInfo m;
        m.id = "hat_fm";
        m.name = "Linear FM Metal Hat";
        m.category = "Hi-Hats";
        m.description = "3-operator inharmonic FM metal synthesis with microtonal sidebands and tuned bell overtones";
        m.p1Label = "FM RATIO"; m.p2Label = "FM DEPTH"; m.p3Label = "HP CUT"; m.p4Label = "NOISE MIX"; m.p5Label = "METALLIC BELL";
        m.defTune = 0.f; m.defDecay = 0.10f; m.defDrive = 0.05f;
        m.defP1 = 0.65f; m.defP2 = 0.55f; m.defP3 = 0.60f; m.defP4 = 0.45f; m.defP5 = 0.55f;
        m.scriptCode =
R"(-- Linear FM Metal Hat (3-Op Microtonal FM)
local p1, p2, p3 = 0.0, 0.0, 0.0
local hp0, lastSample = 0.0, 0.0
function process()
    local fTune    = param("tune")
    local fmRatio  = param("p1")
    local fmDepth  = param("p2") * 6.0
    local hpCut    = param("p3")
    local noiseMix = param("p4")
    local bell     = param("p5")
    local dec      = math.max(0.02, param("decay"))
    local drv      = param("drive")

    if trig then p1 = 0.0; p2 = 0.0; p3 = 0.0; hp0 = 0.0; lastSample = 0.0 end

    local baseF = 340.0 * math.pow(2.0, fTune / 12.0)
    local r1 = 1.0 + fmRatio * 2.87
    local r2 = 2.41 + fmRatio * 4.19
    local dt = 1.0 / sr
    local cutHz = 3000.0 + hpCut * 7000.0
    local hpCoef = math.exp(-6.2831853 * cutHz / sr)

    for i = 0, n - 1 do
        local t = age + i * dt
        p3 = (p3 + baseF * r2 * dt) % 1.0
        local mod2 = math.sin(p3 * 6.2831853) * fmDepth * 0.7

        p2 = (p2 + baseF * r1 * (1.0 + mod2) * dt) % 1.0
        local mod1 = math.sin(p2 * 6.2831853) * fmDepth

        p1 = (p1 + baseF * (1.0 + mod1) * dt) % 1.0
        local rawTone = math.sin(p1 * 6.2831853)

        if bell > 0.05 then
            rawTone = rawTone + math.sin(p1 * 3.73 * 6.2831853) * (bell * 0.4)
        end

        local nz = (rnd() * 2.0 - 1.0) * noiseMix * 0.5
        local raw = rawTone + nz

        hp0 = hpCoef * (hp0 + raw - lastSample)
        lastSample = raw

        local sig = hp0 * math.exp(-t / dec) * vel * 2.2
        if drv > 0.01 then sig = math.tanh(sig * (1.0 + drv * 3.5)) end
        outL(i, sig)
        outR(i, sig)
    end
end
)";
        list.push_back(m);
    }

    {
        ModuleInfo m;
        m.id = "hat_noise";
        m.name = "Sizzle Noise Hat";
        m.category = "Hi-Hats";
        m.description = "Triple parallel resonant formant filter banks (4kHz, 8kHz, 12kHz) with stick attack and stereo decorrelation";
        m.p1Label = "SIZZLE FREQ"; m.p2Label = "RESONANCE"; m.p3Label = "STICK TIP"; m.p4Label = "COLOR TILT"; m.p5Label = "STEREO SPREAD";
        m.defTune = 0.f; m.defDecay = 0.12f; m.defDrive = 0.04f;
        m.defP1 = 0.65f; m.defP2 = 0.45f; m.defP3 = 0.55f; m.defP4 = 0.50f; m.defP5 = 0.50f;
        m.scriptCode =
R"(-- Sizzle Noise Hat (Triple Formant Filter Bank)
local bp1L, bp1R = 0.0, 0.0
local bp2L, bp2R = 0.0, 0.0
local bp3L, bp3R = 0.0, 0.0
function process()
    local fTune   = param("tune")
    local sizzle  = param("p1")
    local reso    = param("p2")
    local stick   = param("p3")
    local tilt    = param("p4")
    local spread  = param("p5")
    local dec     = math.max(0.02, param("decay"))
    local drv     = param("drive")

    if trig then
        bp1L = 0.0; bp1R = 0.0; bp2L = 0.0; bp2R = 0.0; bp3L = 0.0; bp3R = 0.0
    end

    local pitchMult = math.pow(2.0, fTune / 12.0)
    local f1 = (3800.0 + sizzle * 2500.0) * pitchMult
    local f2 = (7200.0 + sizzle * 3000.0) * pitchMult
    local f3 = (11000.0 + sizzle * 3500.0) * pitchMult

    local dt = 1.0 / sr
    local c1 = 2.0 * math.sin(3.14159265 * math.min(18000.0, f1) / sr)
    local c2 = 2.0 * math.sin(3.14159265 * math.min(18000.0, f2) / sr)
    local c3 = 2.0 * math.sin(3.14159265 * math.min(18000.0, f3) / sr)
    local q = 2.0 + reso * 4.5

    for i = 0, n - 1 do
        local t = age + i * dt
        local whiteL = rnd() * 2.0 - 1.0
        local whiteR = whiteL * (1.0 - spread) + (rnd() * 2.0 - 1.0) * spread

        bp1L = bp1L + c1 * (whiteL - bp1L / q)
        bp1R = bp1R + c1 * (whiteR - bp1R / q)
        bp2L = bp2L + c2 * (whiteL - bp2L / q)
        bp2R = bp2R + c2 * (whiteR - bp2R / q)
        bp3L = bp3L + c3 * (whiteL - bp3L / q)
        bp3R = bp3R + c3 * (whiteR - bp3R / q)

        local shapedL = bp1L * (1.0 - tilt) * 0.8 + bp2L * 0.9 + bp3L * (0.3 + tilt * 1.2)
        local shapedR = bp1R * (1.0 - tilt) * 0.8 + bp2R * 0.9 + bp3R * (0.3 + tilt * 1.2)

        local clk = 0.0
        if t < 0.007 then clk = whiteL * math.exp(-t / 0.0015) * (stick * 1.6) end

        local env = math.exp(-t / dec)
        local sigL = (shapedL + clk) * env * vel * 2.2
        local sigR = (shapedR + clk) * env * vel * 2.2

        if drv > 0.01 then
            sigL = math.tanh(sigL * (1.0 + drv * 3.0))
            sigR = math.tanh(sigR * (1.0 + drv * 3.0))
        end
        outL(i, sigL)
        outR(i, sigR)
    end
end
)";
        list.push_back(m);
    }

    // =========================================================================
    // 4. CLAPS (3 Unique Architectures)
    // =========================================================================
    {
        ModuleInfo m;
        m.id = "clap_808";
        m.name = "808 Handclap";
        m.category = "Claps";
        m.description = "Multi-burst pre-flams (individual hands spaced in time) followed by a decaying resonant bandpass tail";
        m.p1Label = "FLAM SPREAD"; m.p2Label = "FILTER FREQ"; m.p3Label = "BANDPASS Q"; m.p4Label = "ROOM TAIL"; m.p5Label = "HAND COUNT";
        m.defTune = 0.f; m.defDecay = 0.35f; m.defDrive = 0.06f;
        m.defP1 = 0.55f; m.defP2 = 0.50f; m.defP3 = 0.50f; m.defP4 = 0.50f; m.defP5 = 0.60f;
        m.scriptCode =
R"(-- 808 Handclap (Multi-Burst Flams + Resonant Tail)
local bp0, bp1 = 0.0, 0.0
function process()
    local fTune     = param("tune")
    local spread    = param("p1")
    local filtFreq  = param("p2")
    local bpQ       = param("p3")
    local roomTail  = param("p4")
    local handCount = 2 + math.floor(param("p5") * 3.99)
    local dec       = math.max(0.04, param("decay") * (0.8 + roomTail * 0.8))
    local drv       = param("drive")

    if trig then bp0 = 0.0; bp1 = 0.0 end

    local dt = 1.0 / sr
    local centerHz = (1000.0 + filtFreq * 1600.0) * math.pow(2.0, fTune / 12.0)
    local f = 2.0 * math.sin(3.14159265 * centerHz / sr)
    local q = 1.5 + bpQ * 4.0
    local flamDelta = 0.008 + spread * 0.016

    for i = 0, n - 1 do
        local t = age + i * dt
        local burst = 0.0
        for h = 0, handCount - 1 do
            local hTime = t - h * flamDelta
            if hTime >= 0.0 and hTime < 0.016 then
                burst = burst + (rnd() * 2.0 - 1.0) * math.exp(-hTime / 0.003) * 1.2
            end
        end

        local tailStart = handCount * flamDelta
        local tail = 0.0
        if t >= tailStart then
            tail = (rnd() * 2.0 - 1.0) * math.exp(-(t - tailStart) / dec) * 0.9
        end

        local raw = burst + tail
        bp0 = bp0 + f * (raw - bp0 - bp1 / q)
        bp1 = bp1 + f * bp0

        local sig = bp0 * vel * 2.2
        if drv > 0.01 then sig = math.tanh(sig * (1.0 + drv * 3.5)) end
        outL(i, sig)
        outR(i, sig)
    end
end
)";
        list.push_back(m);
    }

    {
        ModuleInfo m;
        m.id = "clap_room";
        m.name = "Stereo Room Ambient Clap";
        m.category = "Claps";
        m.description = "Multi-tap early reflection cluster (7 decorrelated taps) simulating hands clapping in an acoustic hall with air damping";
        m.p1Label = "ROOM SIZE"; m.p2Label = "STEREO WIDTH"; m.p3Label = "FLAM GAP"; m.p4Label = "DAMPING"; m.p5Label = "BODY TONE";
        m.defTune = 0.f; m.defDecay = 0.45f; m.defDrive = 0.08f;
        m.defP1 = 0.60f; m.defP2 = 0.70f; m.defP3 = 0.55f; m.defP4 = 0.50f; m.defP5 = 0.55f;
        m.scriptCode =
R"(-- Stereo Room Ambient Clap (7-Tap Reflection Cluster)
local lpL, lpR = 0.0, 0.0
function process()
    local fTune     = param("tune")
    local roomSize  = param("p1")
    local stereoW   = param("p2")
    local flamGap   = param("p3")
    local damp      = param("p4")
    local bodyTone  = param("p5")
    local dec       = math.max(0.06, param("decay") * (0.8 + roomSize * 0.9))
    local drv       = param("drive")

    if trig then lpL = 0.0; lpR = 0.0 end

    local dt = 1.0 / sr
    local lpCut = 0.15 + (1.0 - damp) * 0.6
    local scale = 0.006 + roomSize * 0.018

    local tapTimes = {0.0, 0.011, 0.021, 0.034, 0.052, 0.075, 0.105}
    local tapPans  = {0.0, -0.4,   0.5,  -0.7,   0.8,  -0.9,   1.0}

    for i = 0, n - 1 do
        local t = age + i * dt
        local accL = 0.0
        local accR = 0.0

        for k = 1, 7 do
            local tapT = tapTimes[k] * (0.6 + flamGap * 0.8)
            local d = t - tapT
            if d >= 0.0 and d < 0.022 then
                local s = (rnd() * 2.0 - 1.0) * math.exp(-d / 0.004) * (1.1 - k * 0.1)
                local p = tapPans[k] * stereoW
                accL = accL + s * (0.5 - p * 0.5)
                accR = accR + s * (0.5 + p * 0.5)
            end
        end

        local tail = (rnd() * 2.0 - 1.0) * math.exp(-t / dec) * 0.4
        local rawL = (accL + tail)
        local rawR = (accR + tail)

        lpL = lpL + lpCut * (rawL - lpL)
        lpR = lpR + lpCut * (rawR - lpR)

        local body = math.sin(t * 380.0 * 6.2831853) * bodyTone * 0.35 * math.exp(-t / (dec * 0.5))
        local sigL = (lpL + body) * vel * 2.4
        local sigR = (lpR + body) * vel * 2.4

        if drv > 0.01 then
            sigL = math.tanh(sigL * (1.0 + drv * 3.5))
            sigR = math.tanh(sigR * (1.0 + drv * 3.5))
        end
        outL(i, sigL)
        outR(i, sigR)
    end
end
)";
        list.push_back(m);
    }

    {
        ModuleInfo m;
        m.id = "clap_trash";
        m.name = "Trash Gated Clap";
        m.category = "Claps";
        m.description = "Industrial diode-clipped clap with sample-rate bit-decimation, pre-distortion filter, and an abrupt non-linear noise gate";
        m.p1Label = "GATE TIME"; m.p2Label = "CRUSH"; m.p3Label = "DIRT"; m.p4Label = "FILTER TONE"; m.p5Label = "FLAM DENSITY";
        m.defTune = 0.f; m.defDecay = 0.28f; m.defDrive = 0.25f;
        m.defP1 = 0.70f; m.defP2 = 0.60f; m.defP3 = 0.65f; m.defP4 = 0.55f; m.defP5 = 0.55f;
        m.scriptCode =
R"(-- Trash Gated Clap (Industrial Bitcrush & Diode Gate)
local bp0, bp1 = 0.0, 0.0
function process()
    local fTune       = param("tune")
    local gateTime    = 0.04 + param("p1") * 0.35
    local crush       = param("p2")
    local dirt        = param("p3")
    local filterTone  = param("p4")
    local flamDensity = 3 + math.floor(param("p5") * 4.99)
    local dec         = math.max(0.04, param("decay"))
    local drv         = param("drive")

    if trig then bp0 = 0.0; bp1 = 0.0 end

    local dt = 1.0 / sr
    local centerHz = (900.0 + filterTone * 2200.0) * math.pow(2.0, fTune / 12.0)
    local f = 2.0 * math.sin(3.14159265 * centerHz / sr)
    local q = 2.0 + dirt * 3.0

    for i = 0, n - 1 do
        local t = age + i * dt
        local burst = 0.0

        if t < gateTime then
            for h = 0, flamDensity - 1 do
                local hTime = t - h * 0.009
                if hTime >= 0.0 and hTime < 0.015 then
                    burst = burst + (rnd() * 2.0 - 1.0) * 0.9
                end
            end
            burst = burst + (rnd() * 2.0 - 1.0) * 0.6
        end

        bp0 = bp0 + f * (burst - bp0 - bp1 / q)
        bp1 = bp1 + f * bp0

        local raw = bp0 * math.exp(-t / dec)
        if crush > 0.05 then
            local steps = 16.0 - crush * 12.0
            raw = math.floor(raw * steps) / steps
        end

        local gain = 1.0 + dirt * 6.0 + drv * 4.0
        local sig = math.tanh(raw * gain) * vel * 1.5
        outL(i, sig)
        outR(i, sig)
    end
end
)";
        list.push_back(m);
    }

    // =========================================================================
    // 5. TOMS (3 Unique Architectures)
    // =========================================================================
    {
        ModuleInfo m;
        m.id = "tom_dual";
        m.name = "Analog Dual-Head Tom";
        m.category = "Toms";
        m.description = "Dual cross-coupled bridged-T resonators (batter and resonant heads) with impact pitch bend and shell saturation";
        m.p1Label = "BEND"; m.p2Label = "HEAD RATIO"; m.p3Label = "CLICK"; m.p4Label = "SHELL RING"; m.p5Label = "WARMTH";
        m.defTune = 0.f; m.defDecay = 0.45f; m.defDrive = 0.08f;
        m.defP1 = 0.55f; m.defP2 = 0.45f; m.defP3 = 0.50f; m.defP4 = 0.50f; m.defP5 = 0.45f;
        m.scriptCode =
R"(-- Analog Dual-Head Tom (Coupled Bridged-T Resonators)
local p1, p2 = 0.0, 0.0
function process()
    local fTune     = param("tune")
    local bend      = param("p1")
    local headRatio = param("p2")
    local click     = param("p3")
    local shellRing = param("p4")
    local warmth    = param("p5")
    local dec       = math.max(0.06, param("decay") * (0.7 + shellRing * 0.8))
    local drv       = param("drive")

    if trig then p1 = 0.0; p2 = 0.0 end

    local baseF = 110.0 * math.pow(2.0, fTune / 12.0)
    local fHead2 = baseF * (0.80 + headRatio * 0.50)
    local dt = 1.0 / sr
    local pTau = 0.015 + bend * 0.045

    for i = 0, n - 1 do
        local t = age + i * dt
        local curF1 = baseF + (baseF * 1.8 * bend) * math.exp(-t / pTau)
        local curF2 = fHead2 + (fHead2 * 1.2 * bend) * math.exp(-t / pTau)

        p1 = (p1 + curF1 * dt) % 1.0
        p2 = (p2 + curF2 * dt) % 1.0

        local b1 = math.sin(p1 * 6.2831853)
        local b2 = math.sin(p2 * 6.2831853) * 0.65

        local clk = 0.0
        if t < 0.012 then clk = (rnd() * 2.0 - 1.0) * math.exp(-t / 0.002) * (click * 1.4) end

        local raw = (b1 + b2 + clk) * math.exp(-t / dec) * vel * 1.3
        if warmth > 0.02 then
            raw = raw + (raw * math.abs(raw)) * warmth * 0.35
        end

        local sig = raw
        if drv > 0.01 then sig = math.tanh(sig * (1.0 + drv * 3.5)) end
        outL(i, sig)
        outR(i, sig)
    end
end
)";
        list.push_back(m);
    }

    {
        ModuleInfo m;
        m.id = "tom_simmons";
        m.name = "Simmons Hex Electronic Tom";
        m.category = "Toms";
        m.description = "80s SDS-V style: triangle VCO with steep exponential downward sweep, analog click, and swept resonant pink noise";
        m.p1Label = "BEND RANGE"; m.p2Label = "BEND SPEED"; m.p3Label = "CLICK LEVEL"; m.p4Label = "NOISE MIX"; m.p5Label = "WAVE SHAPE";
        m.defTune = 0.f; m.defDecay = 0.38f; m.defDrive = 0.12f;
        m.defP1 = 0.75f; m.defP2 = 0.65f; m.defP3 = 0.55f; m.defP4 = 0.50f; m.defP5 = 0.45f;
        m.scriptCode =
R"(-- Simmons Hex Electronic Tom (SDS-V Triangle + Swept Noise)
local phase = 0.0
local noiseLp = 0.0
function process()
    local fTune     = param("tune")
    local bendRange = param("p1") * 4.0
    local bendSpeed = param("p2")
    local click     = param("p3")
    local noiseMix  = param("p4")
    local waveShape = param("p5")
    local dec       = math.max(0.04, param("decay"))
    local drv       = param("drive")

    if trig then phase = 0.0; noiseLp = 0.0 end

    local baseF = 95.0 * math.pow(2.0, fTune / 12.0)
    local dt = 1.0 / sr
    local pDec = math.max(0.010, 0.080 * (1.1 - bendSpeed * 0.8))

    for i = 0, n - 1 do
        local t = age + i * dt
        local curF = baseF * math.pow(2.0, bendRange * math.exp(-t / pDec))
        phase = (phase + curF * dt) % 1.0

        local triW = (math.abs(phase - 0.5) * 4.0 - 1.0)
        local sinW = math.sin(phase * 6.2831853)
        local vco = triW * (1.0 - waveShape * 0.7) + sinW * (waveShape * 0.7)

        local nz = rnd() * 2.0 - 1.0
        local cut = 0.05 + 0.5 * math.exp(-t / (dec * 0.5))
        noiseLp = noiseLp + cut * (nz - noiseLp)
        local noise = noiseLp * (noiseMix * 0.8)

        local clk = 0.0
        if t < 0.010 then
            clk = (rnd() * 2.0 - 1.0) * math.exp(-t / 0.002) * (click * 1.5)
        end

        local sig = (vco + noise + clk) * math.exp(-t / dec) * vel * 1.4
        if drv > 0.01 then sig = math.tanh(sig * (1.0 + drv * 4.0)) end
        outL(i, sig)
        outR(i, sig)
    end
end
)";
        list.push_back(m);
    }

    {
        ModuleInfo m;
        m.id = "tom_floor";
        m.name = "Deep Sub Floor Tom";
        m.category = "Toms";
        m.description = "Low-frequency modal circular membrane model with non-linear tension modulation on hard hits and deep air cavity resonance";
        m.p1Label = "SUB BOOM"; m.p2Label = "THUD"; m.p3Label = "TENSION MOD"; m.p4Label = "DAMPING"; m.p5Label = "AIR CAVITY";
        m.defTune = 0.f; m.defDecay = 0.65f; m.defDrive = 0.10f;
        m.defP1 = 0.75f; m.defP2 = 0.60f; m.defP3 = 0.50f; m.defP4 = 0.45f; m.defP5 = 0.40f;
        m.scriptCode =
R"(-- Deep Sub Floor Tom (Non-Linear Tension + Sub Air)
local phase = 0.0
local airPhase = 0.0
function process()
    local fTune      = param("tune")
    local subBoom    = param("p1")
    local thud       = param("p2")
    local tensionMod = param("p3")
    local damping    = param("p4")
    local airCavity  = param("p5")
    local dec        = math.max(0.06, param("decay") * (1.4 - damping * 0.7))
    local drv        = param("drive")

    if trig then phase = 0.0; airPhase = 0.0 end

    local baseF = 65.0 * math.pow(2.0, fTune / 12.0)
    local airF  = (50.0 + airCavity * 40.0) * math.pow(2.0, fTune / 12.0)
    local dt = 1.0 / sr

    for i = 0, n - 1 do
        local t = age + i * dt
        local tensionBend = 1.0 + tensionMod * 0.45 * math.exp(-t / 0.035)
        local curF = baseF * tensionBend * (1.0 + 1.2 * math.exp(-t / 0.020))
        phase = (phase + curF * dt) % 1.0
        airPhase = (airPhase + airF * dt) % 1.0

        local b1 = math.sin(phase * 6.2831853) * (0.6 + subBoom * 0.7)
        local bAir = math.sin(airPhase * 6.2831853) * 0.45 * math.exp(-t / (dec * 0.7))

        local thudSig = 0.0
        if t < 0.018 then
            thudSig = math.sin(t * 120.0 * 6.2831853) * (thud * 1.2) * math.exp(-t / 0.005)
        end

        local sig = (b1 + bAir + thudSig) * math.exp(-t / dec) * vel * 1.35
        if drv > 0.01 then sig = math.tanh(sig * (1.0 + drv * 3.5)) end
        outL(i, sig)
        outR(i, sig)
    end
end
)";
        list.push_back(m);
    }

    // =========================================================================
    // 6. CYMBALS (3 Unique Architectures)
    // =========================================================================
    {
        ModuleInfo m;
        m.id = "cymbal_crash";
        m.name = "Physical Modal Crash";
        m.category = "Cymbals";
        m.description = "48-mode physical modal plate with non-linear energy coupling between modes, hit position excitation, and stereo wash buffer";
        m.p1Label = "BRIGHTNESS"; m.p2Label = "SHIMMER"; m.p3Label = "HIT POS"; m.p4Label = "CHOKE/DAMP"; m.p5Label = "WASH SPREAD";
        m.defTune = 0.f; m.defDecay = 1.20f; m.defDrive = 0.08f;
        m.defP1 = 0.65f; m.defP2 = 0.60f; m.defP3 = 0.50f; m.defP4 = 0.30f; m.defP5 = 0.55f;
        m.scriptCode =
R"(-- Physical Modal Crash Cymbal (48 Modes + Buffer Accumulation)
local NUM_MODES = 48
local mode_f = {}
local mode_pL = {}
local mode_pR = {}
local mode_amp = {}
local accL = {}
local accR = {}
for k = 0, 1024 do accL[k] = 0.0; accR[k] = 0.0 end

do
    for m = 1, NUM_MODES do
        mode_f[m] = 320.0 * math.pow(m, 1.22) * (0.97 + rnd() * 0.06)
        mode_pL[m] = 0.0
        mode_pR[m] = 0.0
        mode_amp[m] = 1.0 / math.sqrt(m)
    end
end

function process()
    local fTune      = param("tune")
    local brightness = param("p1")
    local shimmer    = param("p2")
    local hitPos     = param("p3")
    local choke      = param("p4")
    local spread     = param("p5")
    local dec        = math.max(0.1, param("decay") * (1.3 - choke * 0.9))
    local drv        = param("drive")

    if trig then
        for m = 1, NUM_MODES do
            mode_pL[m] = 0.0
            mode_pR[m] = 0.0
        end
    end

    local pitchMult = math.pow(2.0, fTune / 12.0)
    local dt = 1.0 / sr

    for i = 0, n - 1 do
        accL[i] = 0.0
        accR[i] = 0.0
    end

    local env = math.exp(-age / dec)
    if env > 0.0001 then
        for m = 1, NUM_MODES do
            local f = mode_f[m] * pitchMult
            local posWeight = (m < 16) and (1.0 - hitPos * 0.5) or (0.5 + hitPos * 0.8)
            local brightWeight = math.pow(m / NUM_MODES, 1.5 - brightness)
            local mAmp = mode_amp[m] * posWeight * brightWeight * env * 0.08

            local panL = 0.5 - spread * 0.4 * ((m % 2 == 0) and 1.0 or -1.0)
            local panR = 1.0 - panL

            for i = 0, n - 1 do
                mode_pL[m] = (mode_pL[m] + f * dt) % 1.0
                local s = math.sin(mode_pL[m] * 6.2831853) * mAmp
                accL[i] = accL[i] + s * panL
                accR[i] = accR[i] + s * panR
            end
        end
    end

    local nzGain = shimmer * 0.25 * env
    for i = 0, n - 1 do
        local nz = (rnd() * 2.0 - 1.0) * nzGain
        local sL = (accL[i] + nz) * vel * 2.8
        local sR = (accR[i] + nz) * vel * 2.8

        if drv > 0.01 then
            sL = math.tanh(sL * (1.0 + drv * 3.0))
            sR = math.tanh(sR * (1.0 + drv * 3.0))
        end
        outL(i, sL)
        outR(i, sR)
    end
end
)";
        list.push_back(m);
    }

    {
        ModuleInfo m;
        m.id = "cymbal_ride";
        m.name = "Ride Bell Ping & Shimmer";
        m.category = "Cymbals";
        m.description = "Harmonic bell resonator modes (1.0, 1.48, 2.06, 2.71) layered with a diffuse bronze surface wash and shimmer";
        m.p1Label = "BELL PING"; m.p2Label = "WASH LEVEL"; m.p3Label = "BELL TONE"; m.p4Label = "SHIMMER"; m.p5Label = "DAMPING";
        m.defTune = 0.f; m.defDecay = 0.85f; m.defDrive = 0.05f;
        m.defP1 = 0.75f; m.defP2 = 0.55f; m.defP3 = 0.60f; m.defP4 = 0.50f; m.defP5 = 0.45f;
        m.scriptCode =
R"(-- Ride Bell Ping & Shimmer (Harmonic Bell Modes + Surface Wash)
local p1, p2, p3, p4 = 0.0, 0.0, 0.0, 0.0
local lpWash = 0.0
function process()
    local fTune     = param("tune")
    local bellPing  = param("p1")
    local washLvl   = param("p2")
    local bellTone  = param("p3")
    local shimmer   = param("p4")
    local damping   = param("p5")
    local dec       = math.max(0.06, param("decay") * (1.3 - damping * 0.7))
    local drv       = param("drive")

    if trig then p1 = 0.0; p2 = 0.0; p3 = 0.0; p4 = 0.0; lpWash = 0.0 end

    local baseF = (520.0 + bellTone * 380.0) * math.pow(2.0, fTune / 12.0)
    local dt = 1.0 / sr
    local washCut = 0.08 + shimmer * 0.35

    for i = 0, n - 1 do
        local t = age + i * dt
        p1 = (p1 + baseF * dt) % 1.0
        p2 = (p2 + baseF * 1.48 * dt) % 1.0
        p3 = (p3 + baseF * 2.06 * dt) % 1.0
        p4 = (p4 + baseF * 2.71 * dt) % 1.0

        local bell = (math.sin(p1 * 6.2831853) * 0.5 +
                      math.sin(p2 * 6.2831853) * 0.3 +
                      math.sin(p3 * 6.2831853) * 0.15 +
                      math.sin(p4 * 6.2831853) * 0.1) * (bellPing * 1.4)

        local nz = rnd() * 2.0 - 1.0
        lpWash = lpWash + washCut * (nz - lpWash)
        local wash = lpWash * (washLvl * 0.6)

        local clk = 0.0
        if t < 0.008 then clk = (rnd() * 2.0 - 1.0) * math.exp(-t / 0.0018) * (bellPing * 1.2) end

        local env = math.exp(-t / dec)
        local sig = (bell * env + wash * math.exp(-t / (dec * 1.3)) + clk) * vel * 2.2
        if drv > 0.01 then sig = math.tanh(sig * (1.0 + drv * 3.0)) end
        outL(i, sig)
        outR(i, sig)
    end
end
)";
        list.push_back(m);
    }

    {
        ModuleInfo m;
        m.id = "cymbal_china";
        m.name = "Inharmonic Trash Splash";
        m.category = "Cymbals";
        m.description = "Inverted flange modal model with dense dissonant mode clusters, explosive attack transient, edge bite, and fast decay";
        m.p1Label = "TRASH"; m.p2Label = "BITE"; m.p3Label = "SPLASH"; m.p4Label = "DECAY CUT"; m.p5Label = "STEREO FLUTTER";
        m.defTune = 0.f; m.defDecay = 0.38f; m.defDrive = 0.18f;
        m.defP1 = 0.75f; m.defP2 = 0.70f; m.defP3 = 0.60f; m.defP4 = 0.55f; m.defP5 = 0.50f;
        m.scriptCode =
R"(-- Inharmonic Trash Splash (Inverted Flange Modal Model)
local p1, p2, p3 = 0.0, 0.0, 0.0
local hp0, lastSample = 0.0, 0.0
function process()
    local fTune   = param("tune")
    local trash   = param("p1")
    local bite    = param("p2")
    local splash  = param("p3")
    local decCut  = param("p4")
    local flutter = param("p5")
    local dec     = math.max(0.06, param("decay") * (1.2 - decCut * 0.7))
    local drv     = param("drive")

    if trig then p1 = 0.0; p2 = 0.0; p3 = 0.0; hp0 = 0.0; lastSample = 0.0 end

    local base = 420.0 * math.pow(2.0, fTune / 12.0)
    local dt = 1.0 / sr
    local hpCut = 2400.0 + (1.0 - decCut) * 3500.0
    local hpCoef = math.exp(-6.2831853 * hpCut / sr)

    for i = 0, n - 1 do
        local t = age + i * dt
        p1 = (p1 + base * (1.37 + trash * 0.4) * dt) % 1.0
        p2 = (p2 + base * (2.19 + trash * 0.6) * dt) % 1.0
        p3 = (p3 + base * (3.41 + trash * 0.9) * dt) % 1.0

        local cluster = math.sin(p1 * 6.2831853) * math.sin(p2 * 6.2831853) * 1.5 + (p3 < 0.5 and 0.4 or -0.4) * trash
        local burst = 0.0
        if t < (0.015 + splash * 0.04) then
            burst = (rnd() * 2.0 - 1.0) * math.exp(-t / (0.006 + splash * 0.015)) * (0.6 + bite * 1.3)
        end

        local raw = cluster * (0.4 + trash * 0.7) + (rnd() * 2.0 - 1.0) * 0.35 + burst
        hp0 = hpCoef * (hp0 + raw - lastSample)
        lastSample = raw

        local flut = 1.0 + flutter * 0.3 * math.sin(t * 45.0)
        local sig = hp0 * math.exp(-t / dec) * vel * 2.4 * flut
        local totalDrive = drv * 3.5 + bite * 1.5
        if totalDrive > 0.01 then sig = math.tanh(sig * (1.0 + totalDrive)) end
        outL(i, sig)
        outR(i, sig)
    end
end
)";
        list.push_back(m);
    }

    // =========================================================================
    // 7. PERCUSSION (4 Unique Architectures)
    // =========================================================================
    {
        ModuleInfo m;
        m.id = "perc_cowbell";
        m.name = "808 Metallic Cowbell";
        m.category = "Percussion";
        m.description = "Authentic Roland TR-808 dual square wave bandpass modal cowbell (587Hz & 845Hz) with console saturation";
        m.p1Label = "TONE"; m.p2Label = "RING"; m.p3Label = "FILTER FREQ"; m.p4Label = "CLICK"; m.p5Label = "SATURATION";
        m.defTune = 0.f; m.defDecay = 0.30f; m.defDrive = 0.08f;
        m.defP1 = 0.55f; m.defP2 = 0.50f; m.defP3 = 0.50f; m.defP4 = 0.50f; m.defP5 = 0.50f;
        m.scriptCode =
R"(-- 808 Metallic Cowbell (Dual Square Wave + Bandpass)
local p1, p2 = 0.0, 0.0
local bp0, bp1 = 0.0, 0.0
function process()
    local fTune  = param("tune")
    local tone   = param("p1")
    local ring   = param("p2")
    local filt   = param("p3")
    local click  = param("p4")
    local sat    = param("p5")
    local dec    = math.max(0.04, param("decay"))
    local drv    = param("drive")

    if trig then p1 = 0.0; p2 = 0.0; bp0 = 0.0; bp1 = 0.0 end

    local f1 = (587.0 + (tone - 0.5) * 60.0) * math.pow(2.0, fTune / 12.0)
    local f2 = (845.0 + (tone - 0.5) * 90.0) * math.pow(2.0, fTune / 12.0)
    local center = (600.0 + filt * 700.0) * math.pow(2.0, fTune / 12.0)
    local q = 2.5 + ring * 6.5
    local f = 2.0 * math.sin(3.14159265 * center / sr)
    local dt = 1.0 / sr

    for i = 0, n - 1 do
        local t = age + i * dt
        p1 = (p1 + f1 * dt) % 1.0
        p2 = (p2 + f2 * dt) % 1.0

        local sq1 = (p1 < 0.5 and 0.5 or -0.5)
        local sq2 = (p2 < 0.5 and 0.5 or -0.5) * (0.5 + (1.0 - tone) * 0.5)
        local stick = 0.0
        if t < 0.008 then stick = (rnd() * 2.0 - 1.0) * math.exp(-t / 0.002) * (click * 1.5) end

        local raw = sq1 + sq2 + stick
        bp0 = bp0 + f * (raw - bp0 - bp1 / q)
        bp1 = bp1 + f * bp0

        local sig = bp0 * math.exp(-t / (dec * (0.6 + ring * 0.8))) * vel * 2.4
        local totalDrv = drv + sat * 0.4
        if totalDrv > 0.01 then sig = math.tanh(sig * (1.0 + totalDrv * 3.5)) end
        outL(i, sig)
        outR(i, sig)
    end
end
)";
        list.push_back(m);
    }

    {
        ModuleInfo m;
        m.id = "perc_conga";
        m.name = "Resonant Latin Conga";
        m.category = "Percussion";
        m.description = "Physical membrane drumhead model: fundamental open tone, palm pressure damping, wood barrel resonance, and slap overtones";
        m.p1Label = "SLAP ATTACK"; m.p2Label = "HAND PRESSURE"; m.p3Label = "BODY TONE"; m.p4Label = "RING DAMP"; m.p5Label = "TONE COLOR";
        m.defTune = 0.f; m.defDecay = 0.35f; m.defDrive = 0.05f;
        m.defP1 = 0.55f; m.defP2 = 0.50f; m.defP3 = 0.60f; m.defP4 = 0.50f; m.defP5 = 0.50f;
        m.scriptCode =
R"(-- Resonant Latin Conga (Membrane + Hand Palm Slap)
local phase = 0.0
local overtonePhase = 0.0
function process()
    local fTune     = param("tune")
    local slap      = param("p1")
    local pressure  = param("p2")
    local bodyTone  = param("p3")
    local ringDamp  = param("p4")
    local toneColor = param("p5")
    local dec       = math.max(0.04, param("decay") * (1.3 - ringDamp * 0.7 - pressure * 0.4))
    local drv       = param("drive")

    if trig then phase = 0.0; overtonePhase = 0.0 end

    local baseF = 210.0 * math.pow(2.0, fTune / 12.0)
    local dt = 1.0 / sr

    for i = 0, n - 1 do
        local t = age + i * dt
        local pBend = 1.0 + 0.35 * math.exp(-t / 0.015)
        local curF = baseF * pBend * (1.0 + pressure * 0.15)
        phase = (phase + curF * dt) % 1.0
        overtonePhase = (overtonePhase + curF * (2.1 + toneColor * 0.8) * dt) % 1.0

        local b1 = math.sin(phase * 6.2831853) * (0.6 + bodyTone * 0.6)
        local b2 = math.sin(overtonePhase * 6.2831853) * 0.35 * math.exp(-t / (dec * 0.4))

        local slapSig = 0.0
        if t < 0.014 then
            slapSig = (rnd() * 2.0 - 1.0) * math.exp(-t / 0.0022) * (slap * 1.5)
        end

        local sig = (b1 + b2 + slapSig) * math.exp(-t / dec) * vel * 1.5
        if drv > 0.01 then sig = math.tanh(sig * (1.0 + drv * 3.0)) end
        outL(i, sig)
        outR(i, sig)
    end
end
)";
        list.push_back(m);
    }

    {
        ModuleInfo m;
        m.id = "perc_agogo";
        m.name = "High-Tuned Agogo Bell";
        m.category = "Percussion";
        m.description = "Conical steel bell modal model with selectable dual-chamber interval, harmonic strike, and wall vibration beating";
        m.p1Label = "BELL SELECT"; m.p2Label = "METAL RING"; m.p3Label = "STRIKE HARD"; m.p4Label = "BODY FORMANT"; m.p5Label = "VIBRATO/BEAT";
        m.defTune = 0.f; m.defDecay = 0.40f; m.defDrive = 0.04f;
        m.defP1 = 0.60f; m.defP2 = 0.55f; m.defP3 = 0.50f; m.defP4 = 0.50f; m.defP5 = 0.45f;
        m.scriptCode =
R"(-- High-Tuned Agogo Bell (Conical Steel Bell + Beating)
local p1, p2, p3 = 0.0, 0.0, 0.0
function process()
    local fTune      = param("tune")
    local bellSelect = param("p1")
    local metalRing  = param("p2")
    local strikeHard = param("p3")
    local bodyForm   = param("p4")
    local vibrato    = param("p5")
    local dec        = math.max(0.04, param("decay") * (0.6 + metalRing * 0.9))
    local drv        = param("drive")

    if trig then p1 = 0.0; p2 = 0.0; p3 = 0.0 end

    local pitchRatio = (bellSelect < 0.5 and 1.0 or 1.3348)
    local baseF = 780.0 * pitchRatio * math.pow(2.0, fTune / 12.0)
    local dt = 1.0 / sr

    for i = 0, n - 1 do
        local t = age + i * dt
        local beatHz = vibrato * 12.0
        local fMod = 1.0 + 0.005 * math.sin(t * beatHz * 6.2831853)

        p1 = (p1 + baseF * fMod * dt) % 1.0
        p2 = (p2 + baseF * 2.76 * dt) % 1.0
        p3 = (p3 + (baseF * 1.5 + bodyForm * 400.0) * dt) % 1.0

        local b1 = math.sin(p1 * 6.2831853)
        local b2 = math.sin(p2 * 6.2831853) * 0.45
        local b3 = math.sin(p3 * 6.2831853) * 0.25

        local clk = 0.0
        if t < 0.007 then
            clk = (rnd() * 2.0 - 1.0) * math.exp(-t / 0.0015) * (0.3 + strikeHard * 1.4)
        end

        local sig = (b1 + b2 + b3 + clk) * math.exp(-t / dec) * vel * 1.8
        if drv > 0.01 then sig = math.tanh(sig * (1.0 + drv * 3.0)) end
        outL(i, sig)
        outR(i, sig)
    end
end
)";
        list.push_back(m);
    }

    {
        ModuleInfo m;
        m.id = "perc_rimshot";
        m.name = "Studio Wood Rimshot";
        m.category = "Percussion";
        m.description = "High modal resonance hardwood block tap with organic stick attack, acoustic chamber resonance, and bright overtones";
        m.p1Label = "WOOD PITCH"; m.p2Label = "CLICK SNAP"; m.p3Label = "RESONANCE"; m.p4Label = "RING DAMP"; m.p5Label = "BRIGHTNESS";
        m.defTune = 0.f; m.defDecay = 0.10f; m.defDrive = 0.06f;
        m.defP1 = 0.60f; m.defP2 = 0.55f; m.defP3 = 0.50f; m.defP4 = 0.50f; m.defP5 = 0.50f;
        m.scriptCode =
R"(-- Studio Wood Rimshot (Hardwood Block Modal Tap)
local p1, p2 = 0.0, 0.0
local lp = 0.0
function process()
    local fTune     = param("tune")
    local woodPitch = param("p1")
    local clickSnap = param("p2")
    local reson     = param("p3")
    local ringDamp  = param("p4")
    local bright    = param("p5")
    local dec       = math.max(0.02, param("decay") * (1.2 - ringDamp * 0.7))
    local drv       = param("drive")

    if trig then p1 = 0.0; p2 = 0.0; lp = 0.0 end

    local f0 = (650.0 + woodPitch * 750.0) * math.pow(2.0, fTune / 12.0)
    local f1 = f0 * (1.68 + reson * 0.3)
    local dt = 1.0 / sr
    local lpCut = 0.2 + bright * 0.75

    for i = 0, n - 1 do
        local t = age + i * dt
        p1 = (p1 + f0 * dt) % 1.0
        p2 = (p2 + f1 * dt) % 1.0

        local b1 = math.sin(p1 * 6.2831853)
        local b2 = math.sin(p2 * 6.2831853) * (0.3 + reson * 0.4)

        local click = 0.0
        if t < 0.006 then
            click = (rnd() * 2.0 - 1.0) * math.exp(-t / 0.0012) * (0.4 + clickSnap * 1.5)
        end

        local raw = b1 + b2 + click
        lp = lp + lpCut * (raw - lp)
        local sig = lp * math.exp(-t / dec) * vel * 2.0
        if drv > 0.01 then sig = math.tanh(sig * (1.0 + drv * 3.0)) end
        outL(i, sig)
        outR(i, sig)
    end
end
)";
        list.push_back(m);
    }

    // =========================================================================
    // 8. SYNTHS & NOISE (4 Unique Architectures)
    // =========================================================================
    {
        ModuleInfo m;
        m.id = "synth_zap";
        m.name = "Laser Zap Pitch Sweeper";
        m.category = "Synths & Noise";
        m.description = "Multi-stage laser frequency sweep generator with waveform morphing (sine-tri-saw-sqr), chirp feedback, and stereo detune";
        m.p1Label = "SWEEP RANGE"; m.p2Label = "SWEEP SPEED"; m.p3Label = "WAVE SHAPE"; m.p4Label = "FEEDBACK"; m.p5Label = "STEREO DETUNE";
        m.defTune = 0.f; m.defDecay = 0.22f; m.defDrive = 0.15f;
        m.defP1 = 0.80f; m.defP2 = 0.65f; m.defP3 = 0.40f; m.defP4 = 0.50f; m.defP5 = 0.45f;
        m.scriptCode =
R"(-- Laser Zap Pitch Sweeper (Exponential Sweep + Wave Morph + Stereo Detune)
local pL, pR = 0.0, 0.0
local lastSample = 0.0
function process()
    local fTune     = param("tune")
    local sweepRng  = param("p1") * 5.0
    local sweepSpd  = param("p2")
    local waveShape = param("p3")
    local fb        = param("p4") * 0.35
    local detune    = param("p5")
    local dec       = math.max(0.03, param("decay"))
    local drv       = param("drive")

    if trig then pL = 0.0; pR = 0.0; lastSample = 0.0 end

    local baseF = 180.0 * math.pow(2.0, fTune / 12.0)
    local dt = 1.0 / sr
    local pDec = math.max(0.008, 0.060 * (1.1 - sweepSpd * 0.8))

    for i = 0, n - 1 do
        local t = age + i * dt
        local curF = baseF * math.pow(2.0, sweepRng * math.exp(-t / pDec))
        local curFR = curF * (1.0 + (detune - 0.5) * 0.06)

        pL = (pL + curF * (1.0 + lastSample * fb) * dt) % 1.0
        pR = (pR + curFR * (1.0 + lastSample * fb) * dt) % 1.0

        local sL = math.sin(pL * 6.2831853)
        local sR = math.sin(pR * 6.2831853)

        if waveShape > 0.05 then
            local sawL = (pL * 2.0 - 1.0)
            local sawR = (pR * 2.0 - 1.0)
            sL = sL * (1.0 - waveShape) + sawL * waveShape
            sR = sR * (1.0 - waveShape) + sawR * waveShape
        end

        lastSample = sL
        local env = math.exp(-t / dec)
        local sigL = sL * env * vel * 1.5
        local sigR = sR * env * vel * 1.5

        if drv > 0.01 then
            sigL = math.tanh(sigL * (1.0 + drv * 4.0))
            sigR = math.tanh(sigR * (1.0 + drv * 4.0))
        end
        outL(i, sigL)
        outR(i, sigR)
    end
end
)";
        list.push_back(m);
    }

    {
        ModuleInfo m;
        m.id = "synth_acid";
        m.name = "Resonant Acid Bass Hit";
        m.category = "Synths & Noise";
        m.description = "TB-303 style saw/square oscillator through 4-pole diode ladder low-pass filter with exponential envelope mod and accent drive";
        m.p1Label = "CUTOFF"; m.p2Label = "RESONANCE"; m.p3Label = "ENV MOD"; m.p4Label = "WAVE SELECT"; m.p5Label = "ACCENT DRIVE";
        m.defTune = 0.f; m.defDecay = 0.42f; m.defDrive = 0.25f;
        m.defP1 = 0.45f; m.defP2 = 0.80f; m.defP3 = 0.70f; m.defP4 = 0.35f; m.defP5 = 0.60f;
        m.scriptCode =
R"(-- Resonant Acid Bass Hit (303 Diode-Ladder Low-Pass)
local phase = 0.0
local d0, d1, d2, d3 = 0.0, 0.0, 0.0, 0.0
function process()
    local fTune   = param("tune")
    local cutBase = param("p1")
    local reso    = param("p2")
    local envMod  = param("p3")
    local waveSel = param("p4")
    local accent  = param("p5")
    local dec     = math.max(0.04, param("decay"))
    local drv     = param("drive")

    if trig then phase = 0.0; d0 = 0.0; d1 = 0.0; d2 = 0.0; d3 = 0.0 end

    local f0 = 55.0 * math.pow(2.0, fTune / 12.0)
    local dt = 1.0 / sr

    for i = 0, n - 1 do
        local t = age + i * dt
        phase = (phase + f0 * dt) % 1.0

        local sawW = (phase * 2.0 - 1.0)
        local sqrW = (phase < 0.5 and 0.9 or -0.9)
        local raw = sawW * (1.0 - waveSel) + sqrW * waveSel

        local env = math.exp(-t / (dec * 0.65))
        local curCutHz = 60.0 + 12000.0 * math.min(1.0, cutBase * 0.4 + envMod * env * 0.85)
        local f = math.min(0.95, 2.0 * math.sin(3.14159265 * curCutHz / sr))
        local kRes = reso * 3.8

        local input = raw - d3 * kRes
        d0 = d0 + f * (math.tanh(input) - d0)
        d1 = d1 + f * (d0 - d1)
        d2 = d2 + f * (d1 - d2)
        d3 = d3 + f * (d2 - d3)

        local sig = d3 * math.exp(-t / dec) * vel * 1.6
        local totDrv = drv + accent * 0.5
        if totDrv > 0.01 then
            sig = math.tanh(sig * (1.0 + totDrv * 5.0))
        end
        outL(i, sig)
        outR(i, sig)
    end
end
)";
        list.push_back(m);
    }

    {
        ModuleInfo m;
        m.id = "synth_noise";
        m.name = "Cross-Mod FM Noise Synth";
        m.category = "Synths & Noise";
        m.description = "Two high-frequency oscillators cross-modulating each other into deterministic chaos with morphable multi-mode filter";
        m.p1Label = "CHAOS DEPTH"; m.p2Label = "FREQ RATIO"; m.p3Label = "FILTER CUT"; m.p4Label = "FILTER RES"; m.p5Label = "FILTER MODE";
        m.defTune = 0.f; m.defDecay = 0.35f; m.defDrive = 0.20f;
        m.defP1 = 0.60f; m.defP2 = 0.55f; m.defP3 = 0.65f; m.defP4 = 0.55f; m.defP5 = 0.45f;
        m.scriptCode =
R"(-- Cross-Mod FM Noise Synth (Deterministic Chaos + Multi-Mode Filter)
local p1, p2 = 0.0, 0.0
local s1, s2 = 0.0, 0.0
local fltLp, fltBp = 0.0, 0.0
function process()
    local fTune    = param("tune")
    local chaos    = param("p1") * 5.0
    local ratio    = 1.0 + param("p2") * 4.0
    local cutParam = param("p3")
    local reso     = param("p4")
    local fMode    = param("p5")
    local dec      = math.max(0.04, param("decay"))
    local drv      = param("drive")

    if trig then p1 = 0.0; p2 = 0.0; s1 = 0.0; s2 = 0.0; fltLp = 0.0; fltBp = 0.0 end

    local baseF = 440.0 * math.pow(2.0, fTune / 12.0)
    local dt = 1.0 / sr
    local cutHz = 200.0 + cutParam * 8000.0
    local f = 2.0 * math.sin(3.14159265 * math.min(18000.0, cutHz) / sr)
    local q = 1.0 + reso * 6.0

    for i = 0, n - 1 do
        local t = age + i * dt
        p1 = (p1 + baseF * (1.0 + s2 * chaos) * dt) % 1.0
        p2 = (p2 + baseF * ratio * (1.0 + s1 * chaos) * dt) % 1.0
        s1 = math.sin(p1 * 6.2831853)
        s2 = math.sin(p2 * 6.2831853)

        local raw = (s1 + s2) * 0.7
        fltBp = fltBp + f * (raw - fltLp - fltBp / q)
        fltLp = fltLp + f * fltBp
        local fltHp = raw - fltLp - fltBp / q

        local filtered = fltLp * (1.0 - fMode) + fltBp * (1.0 - math.abs(fMode - 0.5) * 2.0) + fltHp * fMode
        local sig = filtered * math.exp(-t / dec) * vel * 1.5
        if drv > 0.01 then sig = math.tanh(sig * (1.0 + drv * 4.0)) end
        outL(i, sig)
        outR(i, sig)
    end
end
)";
        list.push_back(m);
    }

    {
        ModuleInfo m;
        m.id = "synth_karplus";
        m.name = "Karplus Pluck One-Shot";
        m.category = "Synths & Noise";
        m.description = "Physical waveguide delay pluck model simulating struck nylon strings, steel wire, and acoustic metal rods";
        m.p1Label = "DAMPING"; m.p2Label = "PICK POS"; m.p3Label = "BODY RES"; m.p4Label = "BRIGHTNESS"; m.p5Label = "MATERIAL";
        m.defTune = 0.f; m.defDecay = 0.50f; m.defDrive = 0.10f;
        m.defP1 = 0.50f; m.defP2 = 0.60f; m.defP3 = 0.50f; m.defP4 = 0.50f; m.defP5 = 0.50f;
        m.scriptCode =
R"(-- Karplus Pluck One-Shot (Waveguide Delay + Body Resonator)
local buf = {}
for k = 1, 2048 do buf[k] = 0.0 end
local bLen = 256
local rIdx = 1
local body0, body1 = 0.0, 0.0
local lpOut = 0.0

function process()
    local fTune    = param("tune")
    local damp     = 0.45 + param("p1") * 0.52
    local pick     = param("p2")
    local body     = param("p3")
    local bright   = param("p4")
    local material = param("p5")
    local dec      = math.max(0.06, param("decay"))
    local drv      = param("drive")

    local f0 = 140.0 * math.pow(2.0, fTune / 12.0)
    local targetLen = math.max(8, math.min(2048, math.floor(sr / f0)))

    if trig then
        bLen = targetLen
        for k = 1, bLen do
            local comb = (k % math.max(2, math.floor(bLen * (0.1 + pick * 0.8))) == 0) and -0.5 or 0.5
            buf[k] = (rnd() * 2.0 - 1.0) * (0.5 + bright * 0.8) + comb * (1.0 - material * 0.5)
        end
        rIdx = 1
        body0 = 0.0
        body1 = 0.0
        lpOut = 0.0
    end

    local dt = 1.0 / sr
    local bodyF = 2.0 * math.sin(3.14159265 * (260.0 * math.pow(2.0, fTune / 12.0)) / sr)
    local bodyQ = 3.5
    local toneCut = 0.15 + bright * 0.8

    for i = 0, n - 1 do
        local t = age + i * dt
        local cur = buf[rIdx] or 0.0
        local nextIdx = (rIdx % bLen) + 1
        local nxt = buf[nextIdx] or 0.0

        local filtered = (cur + nxt) * 0.5 * damp
        buf[rIdx] = filtered
        rIdx = nextIdx

        body0 = body0 + bodyF * (cur - body0 - body1 / bodyQ)
        body1 = body1 + bodyF * body0

        local mixed = cur * (1.0 - body * 0.6) + body0 * (body * 1.2)
        lpOut = lpOut + toneCut * (mixed - lpOut)

        local sig = lpOut * math.exp(-t / dec) * vel * 2.2
        if drv > 0.01 then sig = math.tanh(sig * (1.0 + drv * 3.0)) end
        outL(i, sig)
        outR(i, sig)
    end
end
)";
        list.push_back(m);
    }

    return list;
}

std::vector<ModuleInfo> ModulePresetManager::getAllModules()
{
    std::vector<ModuleInfo> all = getFactoryModules();
    auto dir = getModulesDir();
    auto files = dir.findChildFiles(juce::File::findFiles, false, "*.f64mod;*.json");
    for (const auto& f : files)
    {
        auto json = juce::JSON::parse(f.loadFileAsString());
        if (json.isObject())
        {
            ModuleInfo m;
            m.id = json["id"].toString();
            m.name = json["name"].toString();
            m.category = json.hasProperty("category") ? json["category"].toString() : "Custom";
            m.description = json["description"].toString();
            m.p1Label = json["p1Label"].toString();
            m.p2Label = json["p2Label"].toString();
            m.p3Label = json["p3Label"].toString();
            m.p4Label = json["p4Label"].toString();
            m.p5Label = json.hasProperty("p5Label") ? json["p5Label"].toString() : "P5";
            m.scriptCode = json["scriptCode"].toString();
            m.defTune = (float) (double) json["tune"];
            m.defDecay = (float) (double) json["decay"];
            m.defDrive = (float) (double) json["drive"];
            m.defP1 = json.hasProperty("defP1") ? (float) (double) json["defP1"] : 0.5f;
            m.defP2 = json.hasProperty("defP2") ? (float) (double) json["defP2"] : 0.5f;
            m.defP3 = json.hasProperty("defP3") ? (float) (double) json["defP3"] : 0.5f;
            m.defP4 = json.hasProperty("defP4") ? (float) (double) json["defP4"] : 0.5f;
            m.defP5 = json.hasProperty("defP5") ? (float) (double) json["defP5"] : 0.5f;
            all.push_back(m);
        }
    }
    return all;
}

ModuleInfo ModulePresetManager::getModuleById(const juce::String& id)
{
    auto all = getAllModules();
    for (const auto& m : all)
        if (m.id == id)
            return m;
    return all.front();
}

std::vector<ModuleInfo> ModulePresetManager::getModulesForCategory(const juce::String& cat)
{
    std::vector<ModuleInfo> res;
    for (const auto& m : getAllModules())
        if (cat.isEmpty() || cat == "All Categories" || m.category == cat)
            res.push_back(m);
    return res;
}

bool ModulePresetManager::saveUserModule(const ModuleInfo& mod)
{
    auto dir = getModulesDir();
    auto f = dir.getChildFile(mod.id + ".f64mod");
    auto* obj = new juce::DynamicObject();
    obj->setProperty("id", mod.id);
    obj->setProperty("name", mod.name);
    obj->setProperty("category", mod.category);
    obj->setProperty("description", mod.description);
    obj->setProperty("p1Label", mod.p1Label);
    obj->setProperty("p2Label", mod.p2Label);
    obj->setProperty("p3Label", mod.p3Label);
    obj->setProperty("p4Label", mod.p4Label);
    obj->setProperty("p5Label", mod.p5Label);
    obj->setProperty("tune", mod.defTune);
    obj->setProperty("decay", mod.defDecay);
    obj->setProperty("drive", mod.defDrive);
    obj->setProperty("defP1", mod.defP1);
    obj->setProperty("defP2", mod.defP2);
    obj->setProperty("defP3", mod.defP3);
    obj->setProperty("defP4", mod.defP4);
    obj->setProperty("defP5", mod.defP5);
    obj->setProperty("scriptCode", mod.scriptCode);

    juce::var v(obj);
    return f.replaceWithText(juce::JSON::toString(v, true));
}

std::vector<SoundPreset> ModulePresetManager::getSoundPresetsForModule(const juce::String& moduleId)
{
    std::vector<SoundPreset> list;

    // Built-in factory sound presets for ALL 31 modules exercising all 5 knobs (P1 to P5)
    if (moduleId == "kick_808")
    {
        list.push_back({ "Default 808", moduleId, 0.f, 0.65f, 0.12f, 0.60f, 0.40f, 0.50f, 0.40f, 0.30f, 0, 20000.f, 0.707f, 0.f });
        list.push_back({ "Deep Sub Boom", moduleId, -3.f, 1.10f, 0.06f, 0.40f, 0.25f, 0.90f, 0.70f, 0.50f, 1, 850.f, 1.2f, 0.25f });
        list.push_back({ "Punchy Trap 808", moduleId, 2.f, 0.38f, 0.22f, 0.85f, 0.70f, 0.35f, 0.50f, 0.15f, 0, 20000.f, 0.707f, 0.f });
        list.push_back({ "Distorted Sag", moduleId, -1.f, 0.75f, 0.45f, 0.75f, 0.55f, 0.70f, 0.85f, 0.80f, 0, 20000.f, 0.707f, 0.f });
        list.push_back({ "Laser Beater Sub", moduleId, 6.f, 0.22f, 0.14f, 0.95f, 0.85f, 0.20f, 0.30f, 0.10f, 0, 20000.f, 0.707f, 0.f });
    }
    else if (moduleId == "kick_909")
    {
        list.push_back({ "Default 909", moduleId, 0.f, 0.36f, 0.15f, 0.65f, 0.65f, 0.50f, 0.50f, 0.45f, 0, 20000.f, 0.707f, 0.f });
        list.push_back({ "Tight Techno Thump", moduleId, 2.f, 0.24f, 0.25f, 0.80f, 0.80f, 0.35f, 0.65f, 0.60f, 0, 20000.f, 0.707f, 0.f });
        list.push_back({ "Heavy Overdrive", moduleId, -2.f, 0.48f, 0.50f, 0.50f, 0.75f, 0.70f, 0.40f, 0.85f, 1, 6500.f, 1.2f, 0.f });
        list.push_back({ "Short Club Thud", moduleId, 3.f, 0.18f, 0.10f, 0.90f, 0.90f, 0.25f, 0.70f, 0.30f, 0, 20000.f, 0.707f, 0.f });
    }
    else if (moduleId == "kick_rock")
    {
        list.push_back({ "Default Acoustic Rock", moduleId, 0.f, 0.42f, 0.14f, 0.65f, 0.50f, 0.50f, 0.50f, 0.45f, 0, 20000.f, 0.707f, 0.f });
        list.push_back({ "Beater Crack Rock", moduleId, 1.f, 0.30f, 0.18f, 0.85f, 0.75f, 0.40f, 0.60f, 0.85f, 0, 20000.f, 0.707f, 0.f });
        list.push_back({ "Deep 24 Inch Bass", moduleId, -4.f, 0.58f, 0.15f, 0.45f, 0.35f, 0.75f, 0.70f, 0.40f, 1, 950.f, 1.0f, 0.f });
    }
    else if (moduleId == "kick_electro")
    {
        list.push_back({ "Default Electro", moduleId, 0.f, 0.35f, 0.22f, 0.70f, 0.50f, 0.50f, 0.40f, 0.30f, 0, 20000.f, 0.707f, 0.f });
        list.push_back({ "Cyber Bitcrush", moduleId, 3.f, 0.22f, 0.35f, 0.90f, 0.80f, 0.30f, 0.85f, 0.70f, 0, 20000.f, 0.707f, 0.f });
        list.push_back({ "Sub Sine Sweep", moduleId, -3.f, 0.55f, 0.10f, 0.50f, 0.10f, 0.90f, 0.10f, 0.20f, 0, 20000.f, 0.707f, 0.f });
    }
    else if (moduleId == "kick_hardstyle")
    {
        list.push_back({ "Default Hardstyle", moduleId, 0.f, 0.48f, 0.35f, 0.85f, 0.65f, 0.60f, 0.60f, 0.50f, 0, 20000.f, 0.707f, 0.f });
        list.push_back({ "Brutal Folded Raw", moduleId, 1.f, 0.55f, 0.65f, 0.95f, 0.85f, 0.80f, 0.80f, 0.75f, 0, 20000.f, 0.707f, 0.f });
        list.push_back({ "Dark Sub Rumble", moduleId, -2.f, 0.65f, 0.30f, 0.70f, 0.45f, 0.40f, 0.50f, 0.95f, 1, 1200.f, 1.2f, 0.f });
    }
    else if (moduleId == "kick_sub_fm")
    {
        list.push_back({ "Default Sub FM", moduleId, 0.f, 0.60f, 0.12f, 0.50f, 0.50f, 0.60f, 0.40f, 0.40f, 0, 20000.f, 0.707f, 0.f });
        list.push_back({ "Dubstep Low Rattle", moduleId, -4.f, 0.80f, 0.28f, 0.75f, 0.70f, 0.80f, 0.60f, 0.55f, 1, 1400.f, 1.5f, 0.f });
        list.push_back({ "Clean Sub Harmonic", moduleId, 0.f, 0.95f, 0.05f, 0.20f, 0.20f, 0.30f, 0.15f, 0.20f, 0, 20000.f, 0.707f, 0.f });
    }
    else if (moduleId == "snare_808")
    {
        list.push_back({ "Default 808 Snare", moduleId, 0.f, 0.28f, 0.08f, 0.65f, 0.50f, 0.45f, 0.55f, 0.50f, 0, 20000.f, 0.707f, 0.f });
        list.push_back({ "Tight Rim Snap", moduleId, 3.f, 0.16f, 0.15f, 0.85f, 0.75f, 0.25f, 0.75f, 0.80f, 0, 20000.f, 0.707f, 0.f });
        list.push_back({ "Fat Lo-Fi Snare", moduleId, -3.f, 0.45f, 0.22f, 0.45f, 0.35f, 0.70f, 0.85f, 0.30f, 1, 4800.f, 1.0f, 0.f });
        list.push_back({ "Crisp Electro Snap", moduleId, 1.f, 0.20f, 0.12f, 0.90f, 0.80f, 0.30f, 0.40f, 0.75f, 0, 20000.f, 0.707f, 0.f });
    }
    else if (moduleId == "snare_909")
    {
        list.push_back({ "Default 909 Snare", moduleId, 0.f, 0.25f, 0.10f, 0.65f, 0.60f, 0.50f, 0.50f, 0.55f, 0, 20000.f, 0.707f, 0.f });
        list.push_back({ "Crisp Techno Crack", moduleId, 2.f, 0.18f, 0.15f, 0.85f, 0.85f, 0.65f, 0.35f, 0.75f, 0, 20000.f, 0.707f, 0.f });
        list.push_back({ "Warm Deep Snare", moduleId, -2.f, 0.35f, 0.18f, 0.45f, 0.40f, 0.35f, 0.70f, 0.40f, 1, 5500.f, 1.1f, 0.f });
    }
    else if (moduleId == "snare_rock")
    {
        list.push_back({ "Default Rock Snare", moduleId, 0.f, 0.32f, 0.12f, 0.65f, 0.55f, 0.50f, 0.60f, 0.45f, 0, 20000.f, 0.707f, 0.f });
        list.push_back({ "Crack Rimshot Hit", moduleId, 2.f, 0.20f, 0.18f, 0.80f, 0.70f, 0.90f, 0.70f, 0.60f, 0, 20000.f, 0.707f, 0.f });
        list.push_back({ "Loose Snare Rattle", moduleId, -1.f, 0.45f, 0.08f, 0.40f, 0.45f, 0.30f, 0.50f, 0.80f, 0, 20000.f, 0.707f, 0.f });
    }
    else if (moduleId == "snare_rimshot")
    {
        list.push_back({ "Default Maple Rim", moduleId, 0.f, 0.12f, 0.08f, 0.65f, 0.60f, 0.50f, 0.50f, 0.55f, 0, 20000.f, 0.707f, 0.f });
        list.push_back({ "Dry Cross Stick", moduleId, 2.f, 0.07f, 0.12f, 0.85f, 0.80f, 0.30f, 0.75f, 0.40f, 0, 20000.f, 0.707f, 0.f });
        list.push_back({ "Hollow Resonant Ping", moduleId, 5.f, 0.22f, 0.05f, 0.50f, 0.45f, 0.85f, 0.25f, 0.80f, 0, 20000.f, 0.707f, 0.f });
    }
    else if (moduleId == "hat_closed")
    {
        list.push_back({ "Default Tight Hat", moduleId, 0.f, 0.08f, 0.05f, 0.50f, 0.60f, 0.45f, 0.50f, 0.60f, 0, 20000.f, 0.707f, 0.f });
        list.push_back({ "Micro Tick", moduleId, 6.f, 0.035f, 0.02f, 0.80f, 0.85f, 0.80f, 0.75f, 0.80f, 2, 8000.f, 1.5f, 0.f });
        list.push_back({ "Dirty Detune Sizzle", moduleId, -2.f, 0.14f, 0.18f, 0.90f, 0.45f, 0.30f, 0.35f, 0.50f, 3, 6000.f, 3.5f, 0.f });
    }
    else if (moduleId == "hat_open")
    {
        list.push_back({ "Default Open Hat", moduleId, 0.f, 0.55f, 0.06f, 0.60f, 0.65f, 0.55f, 0.50f, 0.45f, 0, 20000.f, 0.707f, 0.f });
        list.push_back({ "Long Shimmering Sizzle", moduleId, 2.f, 0.85f, 0.12f, 0.80f, 0.75f, 0.60f, 0.85f, 0.60f, 2, 5500.f, 1.8f, 0.f });
        list.push_back({ "Choked Short Open", moduleId, -1.f, 0.22f, 0.05f, 0.40f, 0.50f, 0.40f, 0.30f, 0.40f, 0, 20000.f, 0.707f, 0.f });
    }
    else if (moduleId == "hat_fm")
    {
        list.push_back({ "Default FM Hat", moduleId, 0.f, 0.10f, 0.05f, 0.65f, 0.55f, 0.60f, 0.45f, 0.55f, 0, 20000.f, 0.707f, 0.f });
        list.push_back({ "Cyber Bell Hat", moduleId, 4.f, 0.18f, 0.10f, 0.90f, 0.80f, 0.70f, 0.35f, 0.85f, 0, 20000.f, 0.707f, 0.f });
        list.push_back({ "Micro Metal Plink", moduleId, 8.f, 0.05f, 0.04f, 0.45f, 0.65f, 0.80f, 0.20f, 0.70f, 0, 20000.f, 0.707f, 0.f });
    }
    else if (moduleId == "hat_noise")
    {
        list.push_back({ "Default Sizzle Noise", moduleId, 0.f, 0.12f, 0.04f, 0.65f, 0.45f, 0.55f, 0.50f, 0.50f, 0, 20000.f, 0.707f, 0.f });
        list.push_back({ "Organic Shaker", moduleId, 2.f, 0.09f, 0.02f, 0.70f, 0.50f, 0.60f, 0.50f, 0.45f, 0, 20000.f, 0.707f, 0.f });
        list.push_back({ "Wide Resonant Whistle", moduleId, 5.f, 0.18f, 0.10f, 0.85f, 0.85f, 0.40f, 0.75f, 0.90f, 0, 20000.f, 0.707f, 0.f });
    }
    else if (moduleId == "clap_808")
    {
        list.push_back({ "Default 808 Clap", moduleId, 0.f, 0.35f, 0.06f, 0.55f, 0.50f, 0.50f, 0.50f, 0.60f, 0, 20000.f, 0.707f, 0.f });
        list.push_back({ "Tight Studio Snap", moduleId, 2.f, 0.18f, 0.12f, 0.35f, 0.70f, 0.65f, 0.25f, 0.40f, 0, 20000.f, 0.707f, 0.f });
        list.push_back({ "Big Hall Reverb Clap", moduleId, -2.f, 0.65f, 0.15f, 0.75f, 0.40f, 0.35f, 0.90f, 0.85f, 0, 20000.f, 0.707f, 0.f });
    }
    else if (moduleId == "clap_room")
    {
        list.push_back({ "Default Room Clap", moduleId, 0.f, 0.45f, 0.08f, 0.60f, 0.70f, 0.55f, 0.50f, 0.55f, 0, 20000.f, 0.707f, 0.f });
        list.push_back({ "Wide Stereo Spread", moduleId, 0.f, 0.58f, 0.12f, 0.80f, 0.95f, 0.65f, 0.40f, 0.65f, 0, 20000.f, 0.707f, 0.f });
        list.push_back({ "Dark Acoustic Hall", moduleId, -3.f, 0.70f, 0.10f, 0.90f, 0.65f, 0.50f, 0.75f, 0.80f, 1, 4200.f, 1.2f, 0.f });
    }
    else if (moduleId == "clap_trash")
    {
        list.push_back({ "Default Trash Clap", moduleId, 0.f, 0.28f, 0.25f, 0.70f, 0.60f, 0.65f, 0.55f, 0.55f, 0, 20000.f, 0.707f, 0.f });
        list.push_back({ "Severe Bitcrush Gated", moduleId, 2.f, 0.15f, 0.40f, 0.45f, 0.90f, 0.85f, 0.70f, 0.80f, 0, 20000.f, 0.707f, 0.f });
        list.push_back({ "Loose Industrial Clatter", moduleId, -2.f, 0.45f, 0.30f, 0.85f, 0.50f, 0.60f, 0.45f, 0.90f, 0, 20000.f, 0.707f, 0.f });
    }
    else if (moduleId == "tom_dual")
    {
        list.push_back({ "Default Dual Tom", moduleId, 0.f, 0.45f, 0.08f, 0.55f, 0.45f, 0.50f, 0.50f, 0.45f, 0, 20000.f, 0.707f, 0.f });
        list.push_back({ "Downward Laser Chirp", moduleId, 4.f, 0.28f, 0.15f, 0.90f, 0.75f, 0.80f, 0.30f, 0.50f, 0, 20000.f, 0.707f, 0.f });
        list.push_back({ "Deep Resonant Shell", moduleId, -5.f, 0.65f, 0.08f, 0.35f, 0.30f, 0.35f, 0.85f, 0.65f, 1, 1400.f, 1.2f, 0.f });
    }
    else if (moduleId == "tom_simmons")
    {
        list.push_back({ "Default Simmons Tom", moduleId, 0.f, 0.38f, 0.12f, 0.75f, 0.65f, 0.55f, 0.50f, 0.45f, 0, 20000.f, 0.707f, 0.f });
        list.push_back({ "80s Disco Space Drop", moduleId, 5.f, 0.25f, 0.18f, 0.95f, 0.80f, 0.70f, 0.40f, 0.25f, 0, 20000.f, 0.707f, 0.f });
        list.push_back({ "Noisy Industrial Hex", moduleId, -3.f, 0.48f, 0.30f, 0.65f, 0.50f, 0.60f, 0.85f, 0.80f, 0, 20000.f, 0.707f, 0.f });
    }
    else if (moduleId == "tom_floor")
    {
        list.push_back({ "Default Floor Tom", moduleId, 0.f, 0.65f, 0.10f, 0.75f, 0.60f, 0.50f, 0.45f, 0.40f, 0, 20000.f, 0.707f, 0.f });
        list.push_back({ "Sub Thunder Thump", moduleId, -4.f, 0.85f, 0.18f, 0.90f, 0.80f, 0.70f, 0.25f, 0.60f, 1, 950.f, 1.3f, 0.f });
        list.push_back({ "Damped Punch Tom", moduleId, 2.f, 0.28f, 0.08f, 0.45f, 0.70f, 0.30f, 0.80f, 0.35f, 0, 20000.f, 0.707f, 0.f });
    }
    else if (moduleId == "cymbal_crash")
    {
        list.push_back({ "Default Modal Crash", moduleId, 0.f, 1.20f, 0.08f, 0.65f, 0.60f, 0.50f, 0.30f, 0.55f, 0, 20000.f, 0.707f, 0.f });
        list.push_back({ "Fast Splash Wash", moduleId, 3.f, 0.55f, 0.12f, 0.85f, 0.80f, 0.75f, 0.60f, 0.70f, 2, 4500.f, 1.5f, 0.f });
        list.push_back({ "Dark Hand Choke", moduleId, -2.f, 0.35f, 0.05f, 0.40f, 0.35f, 0.30f, 0.85f, 0.40f, 0, 20000.f, 0.707f, 0.f });
    }
    else if (moduleId == "cymbal_ride")
    {
        list.push_back({ "Default Ride Bell", moduleId, 0.f, 0.85f, 0.05f, 0.75f, 0.55f, 0.60f, 0.50f, 0.45f, 0, 20000.f, 0.707f, 0.f });
        list.push_back({ "Pure Bell Ping", moduleId, 4.f, 0.50f, 0.03f, 0.95f, 0.30f, 0.80f, 0.40f, 0.35f, 0, 20000.f, 0.707f, 0.f });
        list.push_back({ "Heavy Bronze Sizzle", moduleId, -2.f, 1.10f, 0.08f, 0.50f, 0.85f, 0.45f, 0.75f, 0.25f, 0, 20000.f, 0.707f, 0.f });
    }
    else if (moduleId == "cymbal_china")
    {
        list.push_back({ "Default Trash China", moduleId, 0.f, 0.38f, 0.18f, 0.75f, 0.70f, 0.60f, 0.55f, 0.50f, 0, 20000.f, 0.707f, 0.f });
        list.push_back({ "Aggressive Edge Bite", moduleId, 2.f, 0.25f, 0.28f, 0.90f, 0.90f, 0.80f, 0.70f, 0.65f, 0, 20000.f, 0.707f, 0.f });
        list.push_back({ "Dark Clatter China", moduleId, -3.f, 0.55f, 0.15f, 0.60f, 0.45f, 0.40f, 0.30f, 0.80f, 0, 20000.f, 0.707f, 0.f });
    }
    else if (moduleId == "perc_cowbell")
    {
        list.push_back({ "Default Cowbell", moduleId, 0.f, 0.30f, 0.08f, 0.55f, 0.50f, 0.50f, 0.50f, 0.50f, 0, 20000.f, 0.707f, 0.f });
        list.push_back({ "High Disco Bell", moduleId, 5.f, 0.20f, 0.10f, 0.75f, 0.65f, 0.65f, 0.65f, 0.55f, 0, 20000.f, 0.707f, 0.f });
        list.push_back({ "Low Latin Cha-Cha", moduleId, -4.f, 0.38f, 0.05f, 0.40f, 0.45f, 0.35f, 0.40f, 0.40f, 0, 20000.f, 0.707f, 0.f });
    }
    else if (moduleId == "perc_conga")
    {
        list.push_back({ "Default Open Conga", moduleId, 0.f, 0.35f, 0.05f, 0.55f, 0.50f, 0.60f, 0.50f, 0.50f, 0, 20000.f, 0.707f, 0.f });
        list.push_back({ "Slap Conga Hi", moduleId, 4.f, 0.16f, 0.10f, 0.85f, 0.70f, 0.40f, 0.65f, 0.60f, 0, 20000.f, 0.707f, 0.f });
        list.push_back({ "Low Muffled Bongo", moduleId, -5.f, 0.45f, 0.04f, 0.30f, 0.40f, 0.80f, 0.35f, 0.40f, 1, 1400.f, 1.2f, 0.f });
    }
    else if (moduleId == "perc_agogo")
    {
        list.push_back({ "Default High Agogo", moduleId, 0.f, 0.40f, 0.04f, 0.60f, 0.55f, 0.50f, 0.50f, 0.45f, 0, 20000.f, 0.707f, 0.f });
        list.push_back({ "Low Chamber Agogo", moduleId, -5.f, 0.50f, 0.05f, 0.20f, 0.65f, 0.40f, 0.40f, 0.50f, 0, 20000.f, 0.707f, 0.f });
        list.push_back({ "Hard Stick Bell", moduleId, 3.f, 0.25f, 0.12f, 0.85f, 0.80f, 0.85f, 0.65f, 0.30f, 0, 20000.f, 0.707f, 0.f });
    }
    else if (moduleId == "perc_rimshot")
    {
        list.push_back({ "Default Wood Block", moduleId, 0.f, 0.10f, 0.06f, 0.60f, 0.55f, 0.50f, 0.50f, 0.50f, 0, 20000.f, 0.707f, 0.f });
        list.push_back({ "High Tonal Clave", moduleId, 7.f, 0.08f, 0.08f, 0.80f, 0.70f, 0.60f, 0.60f, 0.65f, 0, 20000.f, 0.707f, 0.f });
        list.push_back({ "Deep Hollow Chamber", moduleId, -3.f, 0.15f, 0.05f, 0.45f, 0.45f, 0.80f, 0.40f, 0.40f, 0, 20000.f, 0.707f, 0.f });
    }
    else if (moduleId == "synth_zap")
    {
        list.push_back({ "Default Sci-Fi Zap", moduleId, 0.f, 0.22f, 0.15f, 0.80f, 0.65f, 0.40f, 0.50f, 0.45f, 0, 20000.f, 0.707f, 0.f });
        list.push_back({ "Space Invader Laser", moduleId, 6.f, 0.12f, 0.22f, 0.90f, 0.85f, 0.30f, 0.60f, 0.70f, 0, 20000.f, 0.707f, 0.f });
        list.push_back({ "Downer Sub Drop", moduleId, -4.f, 0.40f, 0.18f, 0.70f, 0.45f, 0.65f, 0.40f, 0.30f, 0, 20000.f, 0.707f, 0.f });
    }
    else if (moduleId == "synth_acid")
    {
        list.push_back({ "Default 303 Hit", moduleId, 0.f, 0.42f, 0.25f, 0.45f, 0.80f, 0.70f, 0.35f, 0.60f, 0, 20000.f, 0.707f, 0.f });
        list.push_back({ "Screaming Reso Stab", moduleId, 2.f, 0.30f, 0.45f, 0.30f, 0.95f, 0.85f, 0.15f, 0.75f, 0, 20000.f, 0.707f, 0.f });
        list.push_back({ "Sub Square Acid Pluck", moduleId, -12.f, 0.45f, 0.20f, 0.50f, 0.65f, 0.55f, 0.80f, 0.40f, 0, 20000.f, 0.707f, 0.f });
    }
    else if (moduleId == "synth_noise")
    {
        list.push_back({ "Default FM Noise", moduleId, 0.f, 0.35f, 0.20f, 0.60f, 0.55f, 0.65f, 0.55f, 0.45f, 0, 20000.f, 0.707f, 0.f });
        list.push_back({ "Static Burst Click", moduleId, 6.f, 0.10f, 0.35f, 0.85f, 0.75f, 0.80f, 0.65f, 0.70f, 0, 20000.f, 0.707f, 0.f });
        list.push_back({ "Low Drone Chaos", moduleId, -6.f, 0.75f, 0.25f, 0.45f, 0.80f, 0.50f, 0.70f, 0.30f, 0, 20000.f, 0.707f, 0.f });
    }
    else if (moduleId == "synth_karplus")
    {
        list.push_back({ "Default Pluck", moduleId, 0.f, 0.50f, 0.10f, 0.50f, 0.60f, 0.50f, 0.50f, 0.50f, 0, 20000.f, 0.707f, 0.f });
        list.push_back({ "Muted Steel Wire", moduleId, 7.f, 0.25f, 0.08f, 0.75f, 0.80f, 0.35f, 0.75f, 0.70f, 0, 20000.f, 0.707f, 0.f });
        list.push_back({ "Deep Nylon Bass", moduleId, -12.f, 0.80f, 0.12f, 0.35f, 0.45f, 0.85f, 0.35f, 0.30f, 0, 20000.f, 0.707f, 0.f });
    }
    else
    {
        auto mod = getModuleById(moduleId);
        list.push_back({ "Default", moduleId, mod.defTune, mod.defDecay, mod.defDrive,
                         mod.defP1, mod.defP2, mod.defP3, mod.defP4, mod.defP5,
                         mod.defVcfType, mod.defVcfCut, mod.defVcfRes, 0.f });
    }

    // Scan user sound presets on disk for this module
    auto dir = getSoundPresetsDir(moduleId);
    auto files = dir.findChildFiles(juce::File::findFiles, false, "*.f64snd;*.json");
    for (const auto& f : files)
    {
        auto json = juce::JSON::parse(f.loadFileAsString());
        if (json.isObject())
        {
            SoundPreset sp;
            sp.name = json["name"].toString();
            sp.moduleId = moduleId;
            sp.tune  = (float) (double) json["tune"];
            sp.decay = (float) (double) json["decay"];
            sp.drive = (float) (double) json["drive"];
            sp.p1    = (float) (double) json["p1"];
            sp.p2    = (float) (double) json["p2"];
            sp.p3    = (float) (double) json["p3"];
            sp.p4    = (float) (double) json["p4"];
            sp.p5    = json.hasProperty("p5") ? (float) (double) json["p5"] : 0.5f;
            sp.vcfType = (int) json["vcfType"];
            sp.vcfCut  = (float) (double) json["vcfCut"];
            sp.vcfRes  = (float) (double) json["vcfRes"];
            sp.vcfEnv  = (float) (double) json["vcfEnv"];
            list.push_back(sp);
        }
    }

    return list;
}

bool ModulePresetManager::saveSoundPreset(const SoundPreset& preset)
{
    auto dir = getSoundPresetsDir(preset.moduleId);
    auto f = dir.getChildFile(preset.name + ".f64snd");

    auto* obj = new juce::DynamicObject();
    obj->setProperty("name", preset.name);
    obj->setProperty("moduleId", preset.moduleId);
    obj->setProperty("tune", preset.tune);
    obj->setProperty("decay", preset.decay);
    obj->setProperty("drive", preset.drive);
    obj->setProperty("p1", preset.p1);
    obj->setProperty("p2", preset.p2);
    obj->setProperty("p3", preset.p3);
    obj->setProperty("p4", preset.p4);
    obj->setProperty("p5", preset.p5);
    obj->setProperty("vcfType", preset.vcfType);
    obj->setProperty("vcfCut", preset.vcfCut);
    obj->setProperty("vcfRes", preset.vcfRes);
    obj->setProperty("vcfEnv", preset.vcfEnv);

    juce::var v(obj);
    return f.replaceWithText(juce::JSON::toString(v, true));
}

bool ModulePresetManager::deleteSoundPreset(const juce::String& moduleId, const juce::String& presetName)
{
    auto dir = getSoundPresetsDir(moduleId);
    auto f = dir.getChildFile(presetName + ".f64snd");
    if (f.existsAsFile())
        return f.deleteFile();
    return false;
}

std::vector<ModulePresetManager::CategorySoundEntry> ModulePresetManager::getSoundsForCategory(const juce::String& cat)
{
    std::vector<CategorySoundEntry> result;
    const auto modules = getModulesForCategory(cat);
    for (const auto& m : modules)
    {
        const auto presets = getSoundPresetsForModule(m.id);
        if (presets.empty())
        {
            CategorySoundEntry e;
            e.displayName = m.name;
            e.moduleId = m.id;
            e.preset = { m.name, m.id, m.defTune, m.defDecay, m.defDrive,
                         m.defP1, m.defP2, m.defP3, m.defP4, m.defP5,
                         m.defVcfType, m.defVcfCut, m.defVcfRes, 0.0f };
            result.push_back(e);
        }
        else
        {
            for (const auto& sp : presets)
            {
                CategorySoundEntry e;
                e.displayName = m.name + " - " + sp.name;
                e.moduleId = m.id;
                e.preset = sp;
                result.push_back(e);
            }
        }
    }
    return result;
}

} // namespace f64
