#include "ScriptPresetManager.h"

namespace f64 {

juce::File ScriptPresetManager::getPresetsDirectory()
{
    auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                   .getChildFile("Forge64")
                   .getChildFile("ScriptPresets");
    if (! dir.exists())
        dir.createDirectory();
    return dir;
}

juce::StringArray ScriptPresetManager::getCategories()
{
    return {
        "Kicks",
        "Snares",
        "Hi-Hats",
        "Cymbals",
        "Claps",
        "Toms & Perc",
        "Synths & Noise",
        "Custom"
    };
}

std::vector<ScriptPreset> ScriptPresetManager::getFactoryPresets()
{
    std::vector<ScriptPreset> list;

    // =========================================================================
    // KICKS
    // =========================================================================
    {
        ScriptPreset p;
        p.name = "Rock Membrane Kick";
        p.category = "Kicks";
        p.author = "Matthew Tyas / Web Drum";
        p.description = "Rock kit kick: 5-octave sine membrane with fast transient pitch decay and deep punch";
        p.k1Label = "PITCH"; p.k2Label = "PUNCH"; p.k3Label = "CLICK"; p.k4Label = "OCTAVES";
        p.defTune = 0.f; p.defFx1 = 0.6f; p.defFx2 = 0.4f; p.defFx3 = 0.5f; p.defDecay = 0.4f; p.defDrive = 0.1f;
        p.scriptCode =
R"(-- Rock Membrane Kick (converted from Web Drum Rock Kit)
local phase = 0.0
local clickNoise = 0.0
function process()
    local fTune = param("tune")
    local punch = param("fx1")
    local click = param("fx2")
    local octs  = 3.0 + param("fx3") * 4.0
    local dec   = math.max(0.05, param("decay"))
    local drv   = param("drive")

    local baseF = 45.0 * math.pow(2.0, fTune / 12.0)
    local pDec  = math.max(0.01, 0.06 * (1.0 - punch * 0.5))
    local dt    = 1.0 / sr

    for i = 0, n - 1 do
        local t = age + i * dt
        local curF = baseF * math.pow(2.0, octs * math.exp(-t / pDec))
        phase = phase + curF * dt
        if phase >= 1.0 then phase = phase - math.floor(phase) end

        local body = math.sin(phase * 2.0 * math.pi) * math.exp(-t / dec)
        local clk = (math.random() * 2.0 - 1.0) * math.exp(-t / 0.003) * click * 0.7
        local sig = body + clk
        if drv > 0.01 then sig = math.tanh(sig * (1.0 + drv * 6.0)) end
        outL(i, sig * vel)
        outR(i, sig * vel)
    end
end
)";
        list.push_back(p);
    }

    {
        ScriptPreset p;
        p.name = "Electro 7-Octave Kick";
        p.category = "Kicks";
        p.author = "Matthew Tyas / Web Drum";
        p.description = "Electro kit kick: 7-octave triangle pitch drop with massive low-end and razor click";
        p.k1Label = "PITCH"; p.k2Label = "DROP"; p.k3Label = "SHAPE"; p.k4Label = "BOOM";
        p.defTune = 0.f; p.defFx1 = 0.7f; p.defFx2 = 0.5f; p.defFx3 = 0.5f; p.defDecay = 0.35f; p.defDrive = 0.25f;
        p.scriptCode =
R"(-- Electro 7-Octave Kick (converted from Web Drum Electro Kit)
local phase = 0.0
function process()
    local fTune = param("tune")
    local drop  = param("fx1")
    local shape = param("fx2")
    local boom  = param("fx3")
    local dec   = math.max(0.04, param("decay"))
    local drv   = param("drive")

    local baseF = 40.0 * math.pow(2.0, fTune / 12.0)
    local octs  = 4.0 + drop * 4.0
    local pDec  = 0.012 + drop * 0.025
    local dt    = 1.0 / sr

    for i = 0, n - 1 do
        local t = age + i * dt
        local curF = baseF * math.pow(2.0, octs * math.exp(-t / pDec))
        phase = phase + curF * dt
        if phase >= 1.0 then phase = phase - math.floor(phase) end

        -- Blend sine and triangle
        local sinW = math.sin(phase * 2.0 * math.pi)
        local triW = (math.abs(phase - 0.5) * 4.0 - 1.0)
        local wave = sinW * (1.0 - shape * 0.7) + triW * (shape * 0.7)
        local body = wave * math.exp(-t / dec)
        local sig = body * (1.0 + boom * 0.5)
        if drv > 0.01 then sig = math.tanh(sig * (1.0 + drv * 8.0)) end
        outL(i, sig * vel * 1.2)
        outR(i, sig * vel * 1.2)
    end
end
)";
        list.push_back(p);
    }

    {
        ScriptPreset p;
        p.name = "Latin Warm Kick";
        p.category = "Kicks";
        p.author = "Matthew Tyas / Web Drum";
        p.description = "Latin kit kick: 4-octave acoustic body kick with gentle decay and woody thump";
        p.k1Label = "TUNE"; p.k2Label = "WARMTH"; p.k3Label = "IMPACT"; p.k4Label = "AIR";
        p.defTune = 0.f; p.defFx1 = 0.5f; p.defFx2 = 0.3f; p.defFx3 = 0.2f; p.defDecay = 0.25f; p.defDrive = 0.05f;
        p.scriptCode =
R"(-- Latin Warm Kick (converted from Web Drum Latin Kit)
local phase = 0.0
function process()
    local fTune = param("tune")
    local warm  = param("fx1")
    local imp   = param("fx2")
    local air   = param("fx3")
    local dec   = math.max(0.04, param("decay"))
    local drv   = param("drive")

    local baseF = 55.0 * math.pow(2.0, fTune / 12.0)
    local pDec  = 0.08
    local dt    = 1.0 / sr

    for i = 0, n - 1 do
        local t = age + i * dt
        local curF = baseF * math.pow(2.0, 4.0 * math.exp(-t / pDec))
        phase = phase + curF * dt
        if phase >= 1.0 then phase = phase - math.floor(phase) end

        local body = math.sin(phase * 2.0 * math.pi) * math.exp(-t / dec)
        local wood = math.sin(phase * 4.0 * math.pi) * math.exp(-t / 0.02) * imp * 0.4
        local sig = body + wood
        if drv > 0.01 then sig = math.tanh(sig * (1.0 + drv * 4.0)) end
        outL(i, sig * vel)
        outR(i, sig * vel)
    end
end
)";
        list.push_back(p);
    }

    // =========================================================================
    // SNARES
    // =========================================================================
    {
        ScriptPreset p;
        p.name = "Rock Brown Noise Snare";
        p.category = "Snares";
        p.author = "Matthew Tyas / Web Drum";
        p.description = "Rock kit snare: Warm brown noise + dual tuned membrane body snap";
        p.k1Label = "TUNE"; p.k2Label = "SNAPPY"; p.k3Label = "BODY"; p.k4Label = "COLOR";
        p.defTune = 0.f; p.defFx1 = 0.6f; p.defFx2 = 0.5f; p.defFx3 = 0.4f; p.defDecay = 0.2f; p.defDrive = 0.1f;
        p.scriptCode =
R"(-- Rock Brown Noise Snare (converted from Web Drum Rock Kit)
local brownVal = 0.0
local bodyPhase = 0.0
function process()
    local fTune  = param("tune")
    local snappy = param("fx1")
    local bodyW  = param("fx2")
    local color  = param("fx3")
    local dec    = math.max(0.04, param("decay"))
    local drv    = param("drive")

    local f0 = 185.0 * math.pow(2.0, fTune / 12.0)
    local dt = 1.0 / sr

    for i = 0, n - 1 do
        local t = age + i * dt
        bodyPhase = bodyPhase + f0 * dt
        if bodyPhase >= 1.0 then bodyPhase = bodyPhase - math.floor(bodyPhase) end
        local body = math.sin(bodyPhase * 2.0 * math.pi) * math.exp(-t / 0.06) * bodyW

        -- Brown noise filter
        local white = math.random() * 2.0 - 1.0
        brownVal = (brownVal + (0.05 + color * 0.1) * white) / 1.05
        local noise = brownVal * 4.0 * math.exp(-t / dec) * snappy

        local sig = body * 0.8 + noise
        if drv > 0.01 then sig = math.tanh(sig * (1.0 + drv * 5.0)) end
        outL(i, sig * vel)
        outR(i, sig * vel)
    end
end
)";
        list.push_back(p);
    }

    {
        ScriptPreset p;
        p.name = "Electro Bright Snare";
        p.category = "Snares";
        p.author = "Matthew Tyas / Web Drum";
        p.description = "Electro kit snare: 2x rate high-impact snap with crisp digital crack";
        p.k1Label = "TUNE"; p.k2Label = "CRACK"; p.k3Label = "SNAP"; p.k4Label = "RING";
        p.defTune = 0.f; p.defFx1 = 0.7f; p.defFx2 = 0.6f; p.defFx3 = 0.3f; p.defDecay = 0.12f; p.defDrive = 0.15f;
        p.scriptCode =
R"(-- Electro Bright Snare (converted from Web Drum Electro Kit)
local lp = 0.0
local phase = 0.0
function process()
    local fTune = param("tune")
    local crack = param("fx1")
    local snap  = param("fx2")
    local ring  = param("fx3")
    local dec   = math.max(0.03, param("decay"))
    local drv   = param("drive")

    local f0 = 240.0 * math.pow(2.0, fTune / 12.0)
    local dt = 1.0 / sr

    for i = 0, n - 1 do
        local t = age + i * dt
        phase = phase + f0 * dt
        if phase >= 1.0 then phase = phase - math.floor(phase) end
        local tone = math.sin(phase * 2.0 * math.pi) * math.exp(-t / 0.04) * ring

        local white = (math.random() * 2.0 - 1.0)
        local nEnv = math.exp(-t / dec)
        local impact = (math.random() * 2.0 - 1.0) * math.exp(-t / 0.005) * crack * 1.5
        local sig = tone * 0.5 + white * nEnv * snap + impact
        if drv > 0.01 then sig = math.tanh(sig * (1.0 + drv * 6.0)) end
        outL(i, sig * vel)
        outR(i, sig * vel)
    end
end
)";
        list.push_back(p);
    }

    {
        ScriptPreset p;
        p.name = "Latin Body Snare";
        p.category = "Snares";
        p.author = "Matthew Tyas / Web Drum";
        p.description = "Latin kit snare: 1000Hz HPF filtered pink noise with warm snare shell";
        p.k1Label = "TUNE"; p.k2Label = "HPF CUT"; p.k3Label = "SHELL"; p.k4Label = "WIRES";
        p.defTune = 0.f; p.defFx1 = 0.4f; p.defFx2 = 0.6f; p.defFx3 = 0.5f; p.defDecay = 0.15f; p.defDrive = 0.05f;
        p.scriptCode =
R"(-- Latin Body Snare (converted from Web Drum Latin Kit)
local b0, b1, b2 = 0, 0, 0
local hpMem = 0.0
local bodyP = 0.0
function process()
    local fTune = param("tune")
    local hpfC  = param("fx1")
    local shell = param("fx2")
    local wires = param("fx3")
    local dec   = math.max(0.03, param("decay"))
    local drv   = param("drive")

    local f0 = 190.0 * math.pow(2.0, fTune / 12.0)
    local dt = 1.0 / sr
    local alpha = math.min(0.9, 0.2 + hpfC * 0.5)

    for i = 0, n - 1 do
        local t = age + i * dt
        bodyP = bodyP + f0 * dt
        if bodyP >= 1.0 then bodyP = bodyP - math.floor(bodyP) end
        local tone = math.sin(bodyP * 2.0 * math.pi) * math.exp(-t / 0.05) * shell

        -- Pink noise via simple 3-pole IIR
        local white = math.random() * 2.0 - 1.0
        b0 = 0.99765 * b0 + white * 0.0990460
        b1 = 0.96300 * b1 + white * 0.2965164
        b2 = 0.57000 * b2 + white * 1.0526913
        local pink = (b0 + b1 + b2 + white * 0.1848) * 0.25

        -- Highpass filter
        hpMem = hpMem + alpha * (pink - hpMem)
        local highPink = pink - hpMem

        local sig = tone * 0.7 + highPink * math.exp(-t / dec) * wires * 1.2
        if drv > 0.01 then sig = math.tanh(sig * (1.0 + drv * 5.0)) end
        outL(i, sig * vel)
        outR(i, sig * vel)
    end
end
)";
        list.push_back(p);
    }

    // =========================================================================
    // HI-HATS & CYMBALS
    // =========================================================================
    {
        ScriptPreset p;
        p.name = "Rock Closed Hat";
        p.category = "Hi-Hats";
        p.author = "Matthew Tyas / Web Drum";
        p.description = "Rock kit hi-hat: 7000Hz HPF white noise with ultra-tight cut";
        p.k1Label = "PITCH"; p.k2Label = "CUTOFF"; p.k3Label = "SIZZLE"; p.k4Label = "CHOKE";
        p.defTune = 0.f; p.defFx1 = 0.6f; p.defFx2 = 0.5f; p.defFx3 = 0.5f; p.defDecay = 0.04f; p.defDrive = 0.05f;
        p.scriptCode =
R"(-- Rock Closed Hat (converted from Web Drum Rock Kit)
local hpMem = 0.0
function process()
    local fTune  = param("tune")
    local cutoff = param("fx1")
    local sizzle = param("fx2")
    local choke  = param("fx3")
    local dec    = math.max(0.01, param("decay") * 0.1)

    local dt = 1.0 / sr
    local alpha = math.min(0.95, 0.4 + cutoff * 0.4)

    for i = 0, n - 1 do
        local t = age + i * dt
        local white = math.random() * 2.0 - 1.0
        hpMem = hpMem + alpha * (white - hpMem)
        local hp = white - hpMem

        local env = math.exp(-t / dec)
        local sig = hp * env * 1.4
        outL(i, sig * vel)
        outR(i, sig * vel)
    end
end
)";
        list.push_back(p);
    }

    {
        ScriptPreset p;
        p.name = "Electro High-Freq Tick";
        p.category = "Hi-Hats";
        p.author = "Matthew Tyas / Web Drum";
        p.description = "Electro kit hat: 9000Hz HPF with high-speed crisp sizzle";
        p.k1Label = "PITCH"; p.k2Label = "TICK"; p.k3Label = "RING"; p.k4Label = "AIR";
        p.defTune = 0.f; p.defFx1 = 0.8f; p.defFx2 = 0.4f; p.defFx3 = 0.5f; p.defDecay = 0.03f; p.defDrive = 0.0f;
        p.scriptCode =
R"(-- Electro High-Freq Tick (converted from Web Drum Electro Kit)
local hp1, hp2 = 0.0, 0.0
function process()
    local fTune = param("tune")
    local tick  = param("fx1")
    local ring  = param("fx2")
    local dec   = math.max(0.01, param("decay") * 0.08)

    local dt = 1.0 / sr
    local alpha = 0.65 + tick * 0.25

    for i = 0, n - 1 do
        local t = age + i * dt
        local white = math.random() * 2.0 - 1.0
        hp1 = hp1 + alpha * (white - hp1)
        local h1 = white - hp1
        hp2 = hp2 + alpha * (h1 - hp2)
        local h2 = h1 - hp2

        local env = math.exp(-t / dec)
        local sig = h2 * env * 1.8
        outL(i, sig * vel)
        outR(i, sig * vel)
    end
end
)";
        list.push_back(p);
    }

    {
        ScriptPreset p;
        p.name = "Rock Crash Cymbal";
        p.category = "Cymbals";
        p.author = "Matthew Tyas / Web Drum";
        p.description = "Rock crash: 8000Hz resonant BPF Q=0.8 metallic wash with long decay";
        p.k1Label = "TUNE"; p.k2Label = "FREQ"; p.k3Label = "RESON"; p.k4Label = "SHIMMER";
        p.defTune = 0.f; p.defFx1 = 0.5f; p.defFx2 = 0.6f; p.defFx3 = 0.5f; p.defDecay = 1.2f; p.defDrive = 0.05f;
        p.scriptCode =
R"(-- Rock Crash Cymbal (converted from Web Drum Rock Kit)
local s1, s2 = 0.0, 0.0
function process()
    local fTune   = param("tune")
    local freqP   = param("fx1")
    local reson   = param("fx2")
    local shimmer = param("fx3")
    local dec     = math.max(0.2, param("decay"))

    local f0 = (6500.0 + freqP * 4000.0) * math.pow(2.0, fTune / 12.0)
    local q  = 0.5 + reson * 2.5
    local dt = 1.0 / sr
    local omega = 2.0 * math.pi * f0 * dt
    local k = math.tan(omega * 0.5)
    local norm = 1.0 / (1.0 + k / q + k * k)

    for i = 0, n - 1 do
        local t = age + i * dt
        local inSamp = math.random() * 2.0 - 1.0
        -- BPF state variable
        local hp = (inSamp - (1.0 / q + k) * s1 - s2) * norm
        local bp = hp * k + s1
        s1 = bp * k + hp
        s2 = bp

        local env = math.exp(-t / dec)
        local sig = bp * env * 2.0
        outL(i, sig * vel)
        outR(i, sig * vel)
    end
end
)";
        list.push_back(p);
    }

    // =========================================================================
    // CLAPS
    // =========================================================================
    {
        ScriptPreset p;
        p.name = "Rock Multi-Burst Clap";
        p.category = "Claps";
        p.author = "Matthew Tyas / Web Drum";
        p.description = "Rock kit clap: Staggered micro-burst white noise group";
        p.k1Label = "TUNE"; p.k2Label = "SPREAD"; p.k3Label = "ROOM"; p.k4Label = "TAIL";
        p.defTune = 0.f; p.defFx1 = 0.5f; p.defFx2 = 0.4f; p.defFx3 = 0.5f; p.defDecay = 0.18f; p.defDrive = 0.1f;
        p.scriptCode =
R"(-- Rock Multi-Burst Clap (converted from Web Drum Rock Kit)
local bp1, bp2 = 0.0, 0.0
function process()
    local fTune  = param("tune")
    local spread = param("fx1")
    local room   = param("fx2")
    local tail   = param("fx3")
    local dec    = math.max(0.04, param("decay"))
    local drv    = param("drive")

    local dt = 1.0 / sr
    local bDelays = { 0.0, 0.012 + spread * 0.008, 0.024 + spread * 0.015, 0.038 + spread * 0.02 }

    for i = 0, n - 1 do
        local t = age + i * dt
        local env = 0.0
        for b = 1, 4 do
            local bt = t - bDelays[b]
            if bt >= 0.0 then
                env = env + math.exp(-bt / 0.015) * 0.6
            end
        end
        if t > bDelays[4] then
            env = env + math.exp(-(t - bDelays[4]) / dec) * 0.9 * tail
        end

        local noise = math.random() * 2.0 - 1.0
        bp1 = bp1 + 0.3 * (noise - bp1)
        local sig = bp1 * env * 1.5
        if drv > 0.01 then sig = math.tanh(sig * (1.0 + drv * 6.0)) end
        outL(i, sig * vel)
        outR(i, sig * vel)
    end
end
)";
        list.push_back(p);
    }

    // =========================================================================
    // TOMS & PERCUSSION
    // =========================================================================
    {
        ScriptPreset p;
        p.name = "Rock Triangle Tom";
        p.category = "Toms & Perc";
        p.author = "Matthew Tyas / Web Drum";
        p.description = "Rock kit tom: Triangle oscillator membrane with 4-octave sweep";
        p.k1Label = "TUNE"; p.k2Label = "BEND"; p.k3Label = "OCTAVES"; p.k4Label = "IMPACT";
        p.defTune = 0.f; p.defFx1 = 0.5f; p.defFx2 = 0.5f; p.defFx3 = 0.4f; p.defDecay = 0.3f; p.defDrive = 0.05f;
        p.scriptCode =
R"(-- Rock Triangle Tom (converted from Web Drum Rock Kit)
local phase = 0.0
function process()
    local fTune = param("tune")
    local bend  = param("fx1")
    local octs  = 2.0 + param("fx2") * 3.0
    local imp   = param("fx3")
    local dec   = math.max(0.05, param("decay"))
    local drv   = param("drive")

    local baseF = 110.0 * math.pow(2.0, fTune / 12.0)
    local pDec  = 0.08 + bend * 0.08
    local dt    = 1.0 / sr

    for i = 0, n - 1 do
        local t = age + i * dt
        local curF = baseF * math.pow(2.0, octs * math.exp(-t / pDec))
        phase = phase + curF * dt
        if phase >= 1.0 then phase = phase - math.floor(phase) end

        -- Triangle wave
        local tri = math.abs(phase - 0.5) * 4.0 - 1.0
        local body = tri * math.exp(-t / dec)
        local click = (math.random() * 2.0 - 1.0) * math.exp(-t / 0.005) * imp * 0.5
        local sig = body + click
        if drv > 0.01 then sig = math.tanh(sig * (1.0 + drv * 4.0)) end
        outL(i, sig * vel)
        outR(i, sig * vel)
    end
end
)";
        list.push_back(p);
    }

    // =========================================================================
    // SYNTHS & NOISE
    // =========================================================================
    {
        ScriptPreset p;
        p.name = "Cross-Mod Noise Synth";
        p.category = "Synths & Noise";
        p.author = "Matthew Tyas";
        p.description = "Direct port of noise.html: Osc2->Osc1 FM + Osc3 AM Tremolo + Resonant Filter + Saturation";
        p.k1Label = "OSC 1 PITCH"; p.k2Label = "FM DEPTH"; p.k3Label = "OSC 2 PITCH"; p.k4Label = "FILTER CUTOFF";
        p.defTune = 0.f; p.defFx1 = 0.7f; p.defFx2 = 0.4f; p.defFx3 = 0.6f; p.defDecay = 1.0f; p.defDrive = 0.2f;
        p.scriptCode =
R"(-- Cross-Mod Noise Synth (Direct port of Matthew Tyas's noise.html)
-- Osc2 -> Osc1 Frequency Modulation + Osc3 Amplitude Modulation + State Variable Filter
local p1, p2, p3 = 0.0, 0.0, 0.0
local s1, s2 = 0.0, 0.0
function process()
    local fTune = param("tune")
    local fm    = param("fx1") * 400.0
    local p2St  = (param("fx2") - 0.5) * 24.0
    local cutP  = param("fx3")
    local dec   = math.max(0.1, param("decay"))
    local drv   = param("drive")

    local dt = 1.0 / sr
    local f1 = 220.0 * math.pow(2.0, fTune / 12.0)
    local f2 = 110.0 * math.pow(2.0, (fTune + p2St) / 12.0)
    local f3 = 4.0 + param("fx2") * 12.0

    local cutHz = 100.0 * math.pow(18000.0 / 100.0, cutP)
    local q = 2.5
    local omega = 2.0 * math.pi * cutHz * dt
    local k = math.tan(omega * 0.5)
    local norm = 1.0 / (1.0 + k / q + k * k)

    for i = 0, n - 1 do
        local t = age + i * dt

        -- Osc 2 (Modulator)
        p2 = p2 + f2 * dt
        if p2 >= 1.0 then p2 = p2 - math.floor(p2) end
        local modSig = math.sin(p2 * 2.0 * math.pi)

        -- Osc 1 (Audible Carrier with FM from Osc 2)
        local curF1 = math.max(20.0, f1 + modSig * fm)
        p1 = p1 + curF1 * dt
        if p1 >= 1.0 then p1 = p1 - math.floor(p1) end
        local osc1Sig = math.sin(p1 * 2.0 * math.pi)

        -- Osc 3 (AM tremolo)
        p3 = p3 + f3 * dt
        if p3 >= 1.0 then p3 = p3 - math.floor(p3) end
        local amSig = (math.sin(p3 * 2.0 * math.pi) + 1.0) * 0.5

        local amOut = osc1Sig * amSig

        -- Lowpass filter
        local hp = (amOut - (1.0 / q + k) * s1 - s2) * norm
        local bp = hp * k + s1
        local lp = bp * k + s2
        s1 = bp * k + hp
        s2 = lp

        local env = math.exp(-t / dec)
        local sig = lp * env
        if drv > 0.01 then sig = math.tanh(sig * (1.0 + drv * 6.0)) end
        outL(i, sig * vel)
        outR(i, sig * vel)
    end
end
)";
        list.push_back(p);
    }

    {
        ScriptPreset p;
        p.name = "808 Cowbell & Bell";
        p.category = "Synths & Noise";
        p.author = "Matthew Tyas";
        p.description = "Twin square wave metallic ring-mod bell with fast transient bandpass";
        p.k1Label = "TUNE"; p.k2Label = "RING"; p.k3Label = "TONE"; p.k4Label = "METALLIC";
        p.defTune = 0.f; p.defFx1 = 0.6f; p.defFx2 = 0.5f; p.defFx3 = 0.4f; p.defDecay = 0.35f; p.defDrive = 0.1f;
        p.scriptCode =
R"(-- 808 Cowbell & Metallic Bell
local p1, p2 = 0.0, 0.0
local bp1, bp2 = 0.0, 0.0
function process()
    local fTune = param("tune")
    local ring  = param("fx1")
    local tone  = param("fx2")
    local metal = param("fx3")
    local dec   = math.max(0.05, param("decay"))
    local drv   = param("drive")

    local dt = 1.0 / sr
    local f1 = 540.0 * math.pow(2.0, fTune / 12.0)
    local f2 = 800.0 * math.pow(2.0, fTune / 12.0)

    for i = 0, n - 1 do
        local t = age + i * dt
        p1 = p1 + f1 * dt
        p2 = p2 + f2 * dt
        if p1 >= 1.0 then p1 = p1 - math.floor(p1) end
        if p2 >= 1.0 then p2 = p2 - math.floor(p2) end

        local sq1 = p1 < 0.5 and 1.0 or -1.0
        local sq2 = p2 < 0.5 and 1.0 or -1.0
        local mixed = sq1 * 0.6 + sq2 * 0.4

        -- Bandpass filter
        bp1 = bp1 + 0.18 * (mixed - bp1)
        bp2 = bp2 + 0.18 * (bp1 - bp2)
        local bp = bp1 - bp2

        local env = math.exp(-t / dec)
        local sig = bp * env * 1.6
        if drv > 0.01 then sig = math.tanh(sig * (1.0 + drv * 4.0)) end
        outL(i, sig * vel)
        outR(i, sig * vel)
    end
end
)";
        list.push_back(p);
    }

    return list;
}

void ScriptPresetManager::initializePresetsOnDisk()
{
    auto root = getPresetsDirectory();
    auto factory = getFactoryPresets();

    for (const auto& cat : getCategories())
    {
        auto catDir = root.getChildFile(cat);
        if (! catDir.exists())
            catDir.createDirectory();
    }

    for (const auto& p : factory)
    {
        auto catDir = root.getChildFile(p.category);
        if (! catDir.exists()) catDir.createDirectory();
        auto file = catDir.getChildFile(p.name + ".f64lua");
        if (! file.existsAsFile())
            exportPresetToFile(p, file);
    }
}

bool ScriptPresetManager::exportPresetToFile(const ScriptPreset& preset, const juce::File& file)
{
    juce::var obj = new juce::DynamicObject();
    obj.getDynamicObject()->setProperty("name", preset.name);
    obj.getDynamicObject()->setProperty("category", preset.category);
    obj.getDynamicObject()->setProperty("author", preset.author);
    obj.getDynamicObject()->setProperty("description", preset.description);
    obj.getDynamicObject()->setProperty("k1Label", preset.k1Label);
    obj.getDynamicObject()->setProperty("k2Label", preset.k2Label);
    obj.getDynamicObject()->setProperty("k3Label", preset.k3Label);
    obj.getDynamicObject()->setProperty("k4Label", preset.k4Label);
    obj.getDynamicObject()->setProperty("defTune", preset.defTune);
    obj.getDynamicObject()->setProperty("defFx1", preset.defFx1);
    obj.getDynamicObject()->setProperty("defFx2", preset.defFx2);
    obj.getDynamicObject()->setProperty("defFx3", preset.defFx3);
    obj.getDynamicObject()->setProperty("defFx4", preset.defFx4);
    obj.getDynamicObject()->setProperty("defDecay", preset.defDecay);
    obj.getDynamicObject()->setProperty("defDrive", preset.defDrive);
    obj.getDynamicObject()->setProperty("scriptCode", preset.scriptCode);

    return file.replaceWithText(juce::JSON::toString(obj, true));
}

bool ScriptPresetManager::loadPresetFromFile(const juce::File& file, ScriptPreset& outPreset)
{
    if (! file.existsAsFile()) return false;
    const auto text = file.loadFileAsString();

    auto varJson = juce::JSON::parse(text);
    if (varJson.isObject())
    {
        outPreset.name = varJson.getProperty("name", file.getFileNameWithoutExtension()).toString();
        outPreset.category = varJson.getProperty("category", "Custom").toString();
        outPreset.author = varJson.getProperty("author", "User").toString();
        outPreset.description = varJson.getProperty("description", "").toString();
        outPreset.k1Label = varJson.getProperty("k1Label", "PARAM 1").toString();
        outPreset.k2Label = varJson.getProperty("k2Label", "PARAM 2").toString();
        outPreset.k3Label = varJson.getProperty("k3Label", "PARAM 3").toString();
        outPreset.k4Label = varJson.getProperty("k4Label", "PARAM 4").toString();
        outPreset.defTune = (float) varJson.getProperty("defTune", 0.0);
        outPreset.defFx1 = (float) varJson.getProperty("defFx1", 0.5);
        outPreset.defFx2 = (float) varJson.getProperty("defFx2", 0.5);
        outPreset.defFx3 = (float) varJson.getProperty("defFx3", 0.5);
        outPreset.defFx4 = (float) varJson.getProperty("defFx4", 0.5);
        outPreset.defDecay = (float) varJson.getProperty("defDecay", 1.0);
        outPreset.defDrive = (float) varJson.getProperty("defDrive", 0.0);
        outPreset.scriptCode = varJson.getProperty("scriptCode", "").toString();
        outPreset.filePath = file;
        return true;
    }

    // Fallback: raw Lua script file
    outPreset.name = file.getFileNameWithoutExtension();
    outPreset.category = "Custom";
    outPreset.author = "User";
    outPreset.description = "Custom Lua Script";
    outPreset.scriptCode = text;
    outPreset.filePath = file;
    return true;
}

bool ScriptPresetManager::savePreset(const ScriptPreset& preset)
{
    auto root = getPresetsDirectory();
    auto catDir = root.getChildFile(preset.category.isEmpty() ? "Custom" : preset.category);
    if (! catDir.exists()) catDir.createDirectory();
    auto target = catDir.getChildFile(preset.name + ".f64lua");
    return exportPresetToFile(preset, target);
}

std::vector<ScriptPreset> ScriptPresetManager::getAllPresets()
{
    initializePresetsOnDisk();
    std::vector<ScriptPreset> results;
    auto root = getPresetsDirectory();

    juce::Array<juce::File> files;
    root.findChildFiles(files, juce::File::findFiles, true, "*.f64lua;*.lua");
    for (const auto& f : files)
    {
        ScriptPreset p;
        if (loadPresetFromFile(f, p))
            results.push_back(p);
    }
    return results;
}

std::vector<ScriptPreset> ScriptPresetManager::getPresetsForCategory(const juce::String& category)
{
    auto all = getAllPresets();
    std::vector<ScriptPreset> filtered;
    for (const auto& p : all)
        if (p.category.equalsIgnoreCase(category))
            filtered.push_back(p);
    return filtered;
}

} // namespace f64
