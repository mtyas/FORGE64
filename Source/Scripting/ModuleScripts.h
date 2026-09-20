#pragma once
#include <JuceHeader.h>
#include "../Engine/PadDefs.h"

namespace f64 {

// ---------------------------------------------------------------------------
// Pre-compiled, authentic DSP algorithms converted from the KD-01 and physical
// modeling synthesizers. Loaded directly into the pad's Lua console.
// ---------------------------------------------------------------------------
struct ModuleScripts
{
    static juce::String scriptFor(int srcType)
    {
        switch (srcType)
        {
            case SRC_KICK:
                return
R"(-- FORGE64 808 BASS KICK SYNTHESIZER (KD-01 Physical Model)
-- Converted from KD-01 Drum Synth
-- API: inL, inR, outL, outR, n, sr, vel, age, param("tune"|"fx1"|"fx2"|"fx3"|"decay"|"drive")

local phase = 0.0
local subPhase = 0.0
local noiseSeed = 0.35

local function fastRand()
    noiseSeed = (noiseSeed * 1664525 + 1013904223) % 4294967296
    return (noiseSeed / 2147483648.0) - 1.0
end

function process()
    local fTune    = param("tune")     -- PITCH (-24 .. +24 semitones)
    local punch    = param("fx1")      -- PUNCH (pitch drop envelope depth)
    local click    = param("fx2")      -- CLICK (transient beater impact)
    local sub      = param("fx3")      -- SUB (sub-octave sine reinforcement)
    local warmth   = param("fx4")      -- WARMTH (body saturation & pitch glide)
    local decaySec = param("decay")    -- DECAY (boom sustain time in seconds)
    local drive    = param("drive")    -- DRIVE (saturation / tape drive)

    local f0 = 48.0 * math.pow(2.0, fTune / 12.0)
    local f1 = f0 + (160.0 + punch * 240.0)
    local pd = math.max(0.015, 0.024 + punch * 0.03 + warmth * 0.015)
    local bd = math.max(0.04, decaySec)
    local dt = 1.0 / sr
    local k  = (drive + warmth * 0.3) * (drive + warmth * 0.3) * 14.0

    for i = 0, n - 1 do
        local t = age + i * dt
        local curFreq = f0 + (f1 - f0) * math.exp(-t / pd)
        phase = phase + curFreq * dt
        if phase >= 1.0 then phase = phase - math.floor(phase) end

        -- Membrane sine body + exponential amplitude decay
        local body = math.sin(phase * 2.0 * math.pi)
        local ampEnv = math.exp(-t / bd)

        -- 0.5x sub-octave reinforcement
        subPhase = subPhase + (curFreq * 0.5) * dt
        if subPhase >= 1.0 then subPhase = subPhase - math.floor(subPhase) end
        local subOsc = math.sin(subPhase * 2.0 * math.pi) * sub * 0.8

        -- Initial beater click transient
        local clickSig = 0.0
        if t < 0.016 then
            local cEnv = math.exp(-t / 0.0035)
            clickSig = fastRand() * cEnv * (0.3 + click * 0.8)
        end

        local sig = (body + subOsc + clickSig) * ampEnv * vel
        if k > 0.01 then
            sig = (1.0 + k) * sig / (1.0 + k * math.abs(sig))
        end

        outL(i, sig)
        outR(i, sig)
    end
end
)";

            case SRC_SNARE:
                return
R"(-- FORGE64 808 ANALOG SNARE SYNTHESIZER (Modal Shell & Wire Rattle)
-- Converted from Modal Snare Synthesizer
local phase1 = 0.0
local phase2 = 0.0
local bpB0 = 0.0
local bpB1 = 0.0
local noiseSeed = 0.42

local function fastRand()
    noiseSeed = (noiseSeed * 1664525 + 1013904223) % 4294967296
    return (noiseSeed / 2147483648.0) - 1.0
end

function process()
    local fTune    = param("tune")     -- TUNE: body modal tone
    local snappy   = param("fx1")      -- SNAPPY: wire rattle level
    local tone     = param("fx2")      -- TONE: filter cutoff center
    local impact   = param("fx3")      -- IMPACT: stick transient click
    local shell    = param("fx4")      -- SHELL: body resonance & overtone mix
    local decaySec = param("decay")    -- DECAY
    local drive    = param("drive")    -- DRIVE

    local f1 = 185.0 * math.pow(2.0, fTune / 12.0)
    local f2 = f1 * (1.65 + shell * 0.35)
    local dt = 1.0 / sr
    local cut = math.max(600.0, math.min(9000.0, 1400.0 + tone * 4500.0))
    local q = 2.2
    local f = 2.0 * math.sin(math.pi * cut / sr)

    for i = 0, n - 1 do
        local t = age + i * dt
        phase1 = phase1 + f1 * dt
        if phase1 >= 1.0 then phase1 = phase1 - math.floor(phase1) end
        phase2 = phase2 + f2 * dt
        if phase2 >= 1.0 then phase2 = phase2 - math.floor(phase2) end

        local body = (math.sin(phase1 * 2.0 * math.pi) * 0.7 + math.sin(phase2 * 2.0 * math.pi) * (0.2 + shell * 0.3))
        local bodyEnv = math.exp(-t / math.max(0.03, decaySec * (0.3 + shell * 0.2)))

        -- Snare bottom wires resonant noise
        local rawNoise = fastRand()
        bpB0 = bpB0 + f * (rawNoise - bpB0 - bpB1 / q)
        bpB1 = bpB1 + f * bpB0
        local noise = bpB0
        local noiseEnv = math.exp(-t / math.max(0.04, decaySec))

        local click = 0.0
        if t < 0.010 then
            click = rawNoise * math.exp(-t / 0.0028) * (0.4 + impact * 1.2)
        end

        local sig = (body * bodyEnv * (1.0 - snappy * 0.55) + noise * noiseEnv * (0.35 + snappy * 0.9) + click) * vel
        if drive > 0.01 then
            local k = drive * 3.2
            sig = math.tanh(sig * (1.0 + k))
        end

        outL(i, sig)
        outR(i, sig)
    end
end
)";

            case SRC_HAT_CLOSED:
            case SRC_HAT_OPEN:
                return
R"(-- FORGE64 METALLIC HI-HAT SYNTHESIZER (6-Square Cluster + High-Pass Sizzle)
-- Converted from 808 Metallic Cymbal / Hat Cluster
local p1, p2, p3, p4, p5, p6 = 0, 0, 0, 0, 0, 0
local hpB0 = 0.0
local lastSample = 0.0
local noiseSeed = 0.73

local function fastRand()
    noiseSeed = (noiseSeed * 1664525 + 1013904223) % 4294967296
    return (noiseSeed / 2147483648.0) - 1.0
end

function process()
    local fTune    = param("tune")     -- PITCH
    local sizzle   = param("fx1")      -- SIZZLE: white noise balance
    local metallic = param("fx2")      -- METALLIC: 6-square cluster mix
    local tone     = param("fx3")      -- TONE / AIR: filter resonance
    local chirp    = param("fx4")      -- CHIRP: pedal attack pitch drop
    local decaySec = param("decay")    -- DECAY
    local drive    = param("drive")    -- DRIVE

    local base = 250.0 * math.pow(2.0, fTune / 12.0)
    local dt = 1.0 / sr
    local r1, r2, r3 = base * 0.82, base * 1.22, base * 1.46
    local r4, r5, r6 = base * 1.67, base * 2.16, base * 2.35
    local hpCut = math.max(3500.0, math.min(13000.0, 7000.0 + (sizzle - 0.5) * 3500.0 + tone * 2000.0))
    local hpCoef = math.exp(-2.0 * math.pi * hpCut / sr)

    for i = 0, n - 1 do
        local t = age + i * dt
        local cMod = 1.0 + (chirp * 1.5) * math.exp(-t / 0.008)
        p1 = (p1 + r1 * cMod * dt) % 1.0; p2 = (p2 + r2 * cMod * dt) % 1.0; p3 = (p3 + r3 * cMod * dt) % 1.0
        p4 = (p4 + r4 * cMod * dt) % 1.0; p5 = (p5 + r5 * cMod * dt) % 1.0; p6 = (p6 + r6 * cMod * dt) % 1.0
        local sq = (p1 < 0.5 and 1 or -1) + (p2 < 0.5 and 1 or -1) + (p3 < 0.5 and 1 or -1) +
                   (p4 < 0.5 and 1 or -1) + (p5 < 0.5 and 1 or -1) + (p6 < 0.5 and 1 or -1)
        local cluster = sq * 0.16 * (0.35 + metallic * 0.65)
        local rawNoise = fastRand() * (0.25 + sizzle * 0.6)
        local raw = cluster + rawNoise

        hpB0 = hpCoef * (hpB0 + raw - lastSample)
        lastSample = raw

        local env = math.exp(-t / math.max(0.015, decaySec))
        local sig = hpB0 * env * vel * 1.8
        if drive > 0.01 then
            sig = math.tanh(sig * (1.0 + drive * 2.5))
        end

        outL(i, sig)
        outR(i, sig)
    end
end
)";

            case SRC_CLAP:
                return
R"(-- FORGE64 HANDCLAP SYNTHESIZER (Multi-Burst & Diffuse Reverb Tail)
local bpB0, bpB1 = 0, 0
local lpB = 0.0
local noiseSeed = 0.58

local function fastRand()
    noiseSeed = (noiseSeed * 1664525 + 1013904223) % 4294967296
    return (noiseSeed / 2147483648.0) - 1.0
end

function process()
    local fTune    = param("tune")     -- TUNE: bandpass center frequency
    local spread   = param("fx1")      -- SPREAD: burst spacing
    local reson    = param("fx2")      -- RESON: tail filter Q
    local room     = param("fx3")      -- ROOM: diffuse room reverb tail
    local color    = param("fx4")      -- COLOR: lowpass tone & warmth
    local decaySec = param("decay")    -- DECAY
    local drive    = param("drive")    -- DRIVE

    local center = 1100.0 * math.pow(2.0, fTune / 12.0)
    local q = 2.2 + reson * 2.5
    local f = 2.0 * math.sin(math.pi * center / sr)
    local spacing = 0.009 + spread * 0.008
    local colCut = 0.2 + color * 0.75
    local dt = 1.0 / sr

    for i = 0, n - 1 do
        local t = age + i * dt
        local burstEnv = 0.0
        if t < spacing then
            burstEnv = math.exp(-t / 0.008)
        elseif t < spacing * 2.0 then
            burstEnv = math.exp(-(t - spacing) / 0.008)
        elseif t < spacing * 3.0 then
            burstEnv = math.exp(-(t - spacing * 2.0) / 0.008)
        else
            burstEnv = math.exp(-(t - spacing * 3.0) / math.max(0.06, decaySec)) * (0.7 + room * 0.6)
        end

        local raw = fastRand()
        bpB0 = bpB0 + f * (raw - bpB0 - bpB1 / q)
        bpB1 = bpB1 + f * bpB0

        lpB = lpB + colCut * (bpB0 - lpB)

        local sig = lpB * burstEnv * vel * 2.4
        if drive > 0.01 then sig = math.tanh(sig * (1.0 + drive * 2.2)) end
        outL(i, sig)
        outR(i, sig)
    end
end
)";

            case SRC_TOM:
                return
R"(-- FORGE64 ANALOG TOM SYNTHESIZER (Dual Head Resonant Pitch Sweep)
-- Converted from Thunder Tom
local phase = 0.0
local phase2 = 0.0
local noiseSeed = 0.91

local function fastRand()
    noiseSeed = (noiseSeed * 1664525 + 1013904223) % 4294967296
    return (noiseSeed / 2147483648.0) - 1.0
end

function process()
    local fTune    = param("tune")     -- TUNE: root tone
    local bend     = param("fx1")      -- BEND: pitch sweep depth
    local click    = param("fx2")      -- CLICK: stick strike transient
    local damp     = param("fx3")      -- DAMP: membrane damping
    local ring     = param("fx4")      -- RING: resonant overtone
    local decaySec = param("decay")    -- DECAY
    local drive    = param("drive")    -- DRIVE

    local base = 110.0 * math.pow(2.0, fTune / 12.0)
    local sweep = 80.0 * (1.0 + bend * 1.6)
    local dt = 1.0 / sr

    for i = 0, n - 1 do
        local t = age + i * dt
        local pEnv = math.exp(-t / (0.04 + (1.0 - damp) * 0.03))
        local curFreq = base + sweep * pEnv
        phase = (phase + curFreq * dt) % 1.0
        phase2 = (phase2 + (curFreq * (1.3 + ring * 0.15)) * dt) % 1.0

        local osc1 = math.sin(phase * 2.0 * math.pi)
        local osc2 = math.sin(phase2 * 2.0 * math.pi) * (ring * 0.4)
        local stick = 0.0
        if t < 0.012 then
            stick = fastRand() * math.exp(-t / 0.003) * (0.3 + click * 0.9)
        end

        local env = math.exp(-t / math.max(0.06, decaySec * (1.2 - damp * 0.5)))
        local sig = (osc1 + osc2 + stick) * env * vel * 1.5
        if drive > 0.01 then sig = math.tanh(sig * (1.0 + drive * 2.5)) end
        outL(i, sig)
        outR(i, sig)
    end
end
)";

            case SRC_CRASH:
            case SRC_RIDE:
                return
R"(-- FORGE64 CYMBAL SYNTHESIZER (Inharmonic Metallic Ring Modulation)
-- Converted from Metallic Cymbal Model
local p1, p2, p3 = 0, 0, 0
local hpB0 = 0.0
local lastSample = 0.0
local noiseSeed = 0.81

local function fastRand()
    noiseSeed = (noiseSeed * 1664525 + 1013904223) % 4294967296
    return (noiseSeed / 2147483648.0) - 1.0
end

function process()
    local fTune    = param("tune")     -- PITCH / TUNE
    local shimmer  = param("fx1")      -- SHIMMER / PING
    local spread   = param("fx2")      -- SPREAD / BRIGHT
    local air      = param("fx3")      -- AIR / WASH
    local ping     = param("fx4")      -- PING: bell mode ping
    local decaySec = param("decay")    -- DECAY
    local drive    = param("drive")    -- DRIVE

    local base = 350.0 * math.pow(2.0, fTune / 12.0)
    local dt = 1.0 / sr
    local sp = 1.0 + spread * 0.4
    local hpCut = math.max(2500.0, math.min(12000.0, 4500.0 + (air - 0.5) * 4000.0))
    local hpCoef = math.exp(-2.0 * math.pi * hpCut / sr)

    for i = 0, n - 1 do
        local t = age + i * dt
        p1 = (p1 + base * 1.29 * sp * dt) % 1.0
        p2 = (p2 + base * 1.94 * dt) % 1.0
        p3 = (p3 + base * 2.87 * sp * dt) % 1.0

        local ring = math.sin(p1 * 6.28318) * math.sin(p2 * 6.28318) + math.sin(p3 * 6.28318) * (0.4 + ping * 0.5)
        local nz = fastRand() * (0.35 + air * 0.4)
        local raw = ring * (0.4 + shimmer * 0.4) + nz

        hpB0 = hpCoef * (hpB0 + raw - lastSample)
        lastSample = raw

        local env = math.exp(-t / math.max(0.2, decaySec * 2.2))
        local sig = hpB0 * env * vel * 1.7
        if drive > 0.01 then sig = math.tanh(sig * (1.0 + drive * 2.0)) end
        outL(i, sig)
        outR(i, sig)
    end
end
)";

            case SRC_BELL:
                return
R"(-- FORGE64 808 COWBELL SYNTHESIZER (Dual Square Wave Modal Ding)
-- Converted from 808 Bell Model
local p1, p2 = 0, 0
local bpB0, bpB1 = 0, 0
local noiseSeed = 0.65

local function fastRand()
    noiseSeed = (noiseSeed * 1664525 + 1013904223) % 4294967296
    return (noiseSeed / 2147483648.0) - 1.0
end

function process()
    local fTune    = param("tune")     -- TUNE
    local ring     = param("fx1")      -- RING: resonance decay length
    local tone     = param("fx2")      -- TONE: filter center brightness
    local harm     = param("fx3")      -- HARMONICS: dual modal balance
    local strike   = param("fx4")      -- STRIKE: click attack
    local decaySec = param("decay")    -- DECAY
    local drive    = param("drive")    -- DRIVE

    local f1 = 587.0 * math.pow(2.0, fTune / 12.0)
    local f2 = 845.0 * math.pow(2.0, fTune / 12.0)
    local dt = 1.0 / sr
    local center = 780.0 * math.pow(2.0, fTune / 12.0)
    local q = 4.5 + tone * 4.0
    local f = 2.0 * math.sin(math.pi * center / sr)

    for i = 0, n - 1 do
        local t = age + i * dt
        p1 = (p1 + f1 * dt) % 1.0
        p2 = (p2 + f2 * dt) % 1.0
        local sq1 = p1 < 0.5 and 0.5 or -0.5
        local sq2 = p2 < 0.5 and 0.5 or -0.5
        local clk = 0.0
        if t < 0.008 then clk = fastRand() * math.exp(-t / 0.0018) * (strike * 0.8) end
        local raw = sq1 + sq2 * (0.7 + harm * 0.6) + clk

        bpB0 = bpB0 + f * (raw - bpB0 - bpB1 / q)
        bpB1 = bpB1 + f * bpB0

        local env = math.exp(-t / math.max(0.04, decaySec * 0.6 * (0.6 + ring * 0.5)))
        local sig = bpB0 * env * vel * 2.2
        if drive > 0.01 then sig = math.tanh(sig * (1.0 + drive * 2.0)) end
        outL(i, sig)
        outR(i, sig)
    end
end
)";

            default:
                return
R"(-- FORGE64 PROCEDURAL DRUM SCRIPT
-- API: inL(i), inR(i), outL(i,v), outR(i,v), n, sr, vel, age, param(...)

function process()
    local decaySec = param("decay")
    local drive    = param("drive")
    local dt = 1.0 / sr

    for i = 0, n - 1 do
        local t = age + i * dt
        local env = math.exp(-t / math.max(0.02, decaySec))
        local sL = inL(i) * env
        local sR = inR(i) * env
        if drive > 0.01 then
            sL = math.tanh(sL * (1.0 + drive * 2.5))
            sR = math.tanh(sR * (1.0 + drive * 2.5))
        end
        outL(i, sL)
        outR(i, sR)
    end
end
)";
        }
    }
};

} // namespace f64
