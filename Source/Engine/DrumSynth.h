#pragma once
#include <JuceHeader.h>
#include <cmath>
#include <algorithm>

namespace f64 {

// ---------------------------------------------------------------------------
// High-performance procedural drum synthesizer engine.
// Generates authentic, punchy analog/electronic drum sounds without requiring
// external sample files.
// ---------------------------------------------------------------------------
class DrumSynth
{
public:
    struct VoiceState
    {
        double phase1 = 0.0;
        double phase2 = 0.0;
        double phase3 = 0.0;
        double pitchEnv = 1.0;
        double ampEnv = 1.0;
        double noiseEnv = 1.0;
        float  filterB0 = 0.f;
        float  filterB1 = 0.f;
        float  filterB2 = 0.f;
        float  lastSample = 0.f;
        float  noiseSeed = 0.35f;
        int    burstCount = 0;
        float  burstTimer = 0.f;
        bool   initialized = false;

        void reset()
        {
            phase1 = 0.0;
            phase2 = 0.0;
            phase3 = 0.0;
            pitchEnv = 1.0;
            ampEnv = 1.0;
            noiseEnv = 1.0;
            filterB0 = 0.f;
            filterB1 = 0.f;
            filterB2 = 0.f;
            lastSample = 0.f;
            burstCount = 0;
            burstTimer = 0.f;
            initialized = false;
        }

        inline float fastRandom()
        {
            // Fast linear congruential noise generator
            uint32_t s = *reinterpret_cast<uint32_t*>(&noiseSeed);
            s = s * 1664525u + 1013904223u;
            noiseSeed = *reinterpret_cast<float*>(&s);
            return (float)(s & 0x007FFFFF) * (1.0f / 4194304.0f) - 1.0f;
        }
    };

    // -----------------------------------------------------------------------
    // KICK: Punchy exponential pitch envelope, transient click, sub drive
    // -----------------------------------------------------------------------
    static inline float renderKick(VoiceState& s, double sr, float tuneSt, float decaySec,
                                   float punch, float click, float sub, float drive) noexcept
    {
        if (! s.initialized)
        {
            s.reset();
            s.pitchEnv = 1.0;
            s.ampEnv = 1.0;
            s.initialized = true;
        }

        // Base frequency: 48 Hz tuned by semitones
        const double baseFreq = 48.0 * std::pow(2.0, (double) tuneSt / 12.0);
        // Exponential pitch envelope drop (fast initial punch)
        const double pitchDrop = 180.0 * (1.0 + (double) punch * 2.0);
        const double freq = baseFreq + pitchDrop * s.pitchEnv;

        s.phase1 += freq / sr;
        if (s.phase1 >= 1.0) s.phase1 -= std::floor(s.phase1);

        // Sub-octave frequency (0.5x fundamental)
        s.phase2 += (baseFreq * 0.5) / sr;
        if (s.phase2 >= 1.0) s.phase2 -= std::floor(s.phase2);

        // Sub sine oscillator + sub-octave reinforcement
        float osc = std::sin((float)(s.phase1 * 2.0 * juce::MathConstants<double>::pi));
        float subOsc = std::sin((float)(s.phase2 * 2.0 * juce::MathConstants<double>::pi)) * sub * 0.85f;

        // Click transient in first 15ms
        float clickSignal = 0.f;
        if (s.pitchEnv > 0.05)
        {
            const float n = s.fastRandom();
            // High-pass filter click
            s.filterB0 += 0.4f * (n - s.filterB0);
            clickSignal = (n - s.filterB0) * (float) s.pitchEnv * (0.3f + click * 0.85f);
        }

        // Saturation / drive
        float out = osc + subOsc + clickSignal;
        const float driveAmt = 1.0f + drive * 2.5f;
        out = std::tanh(out * driveAmt) * (float) s.ampEnv;

        // Envelope updates
        const double pitchDecCoef = std::exp(-1.0 / (0.025 * sr));
        const double ampDecSec = juce::jmax(0.04, (double) decaySec);
        const double ampDecCoef = std::exp(-1.0 / (ampDecSec * sr));

        s.pitchEnv *= pitchDecCoef;
        s.ampEnv *= ampDecCoef;

        return out;
    }

    // -----------------------------------------------------------------------
    // SNARE: Dual body resonance tone + snappy filtered noise burst
    // -----------------------------------------------------------------------
    static inline float renderSnare(VoiceState& s, double sr, float tuneSt, float decaySec,
                                    float snappy, float tone, float impact) noexcept
    {
        if (! s.initialized)
        {
            s.reset();
            s.ampEnv = 1.0;
            s.noiseEnv = 1.0;
            s.pitchEnv = 1.0;
            s.initialized = true;
        }

        // Body fundamentals (dual modal resonance)
        const double freq1 = 185.0 * std::pow(2.0, (double) tuneSt / 12.0);
        const double freq2 = freq1 * 1.82;

        s.phase1 += freq1 / sr;
        if (s.phase1 >= 1.0) s.phase1 -= std::floor(s.phase1);
        s.phase2 += freq2 / sr;
        if (s.phase2 >= 1.0) s.phase2 -= std::floor(s.phase2);

        const float body = (std::sin((float)(s.phase1 * 2.0 * juce::MathConstants<double>::pi)) * 0.7f +
                            std::sin((float)(s.phase2 * 2.0 * juce::MathConstants<double>::pi)) * 0.3f)
                           * (float) s.pitchEnv;

        // Filtered noise for snare rattle wires
        const float rawNoise = s.fastRandom();
        // 2-pole resonant bandpass filter for snare noise
        const float cutoff = juce::jlimit(800.f, 8000.f, 1500.f + tone * 4500.f);
        const float q = 1.8f;
        const float f = 2.f * std::sin((float)(juce::MathConstants<double>::pi * cutoff / sr));
        s.filterB0 += f * (rawNoise - s.filterB0 - s.filterB1 * (1.f / q));
        s.filterB1 += f * s.filterB0;
        const float noise = s.filterB0;

        // Blend body vs snappy rattle
        const float snappyMix = juce::jlimit(0.1f, 0.9f, 0.45f + snappy * 0.45f);
        float out = (body * (1.f - snappyMix) + noise * snappyMix * 1.4f);

        // Initial stick impact transient
        if (s.pitchEnv > 0.6)
            out += rawNoise * (float) (s.pitchEnv - 0.6) * 2.0f * (0.5f + impact);

        out *= (float) s.ampEnv;

        // Envelope decays
        const double bodyDecCoef = std::exp(-1.0 / (juce::jmax(0.03, (double) decaySec * 0.35) * sr));
        const double noiseDecCoef = std::exp(-1.0 / (juce::jmax(0.04, (double) decaySec) * sr));
        const double masterDecCoef = std::exp(-1.0 / (juce::jmax(0.05, (double) decaySec * 1.2) * sr));

        s.pitchEnv *= bodyDecCoef;
        s.noiseEnv *= noiseDecCoef;
        s.ampEnv *= masterDecCoef;

        return std::tanh(out * 1.3f);
    }

    // -----------------------------------------------------------------------
    // HI-HAT (Closed & Open): 6-oscillator metallic cluster + HPF
    // -----------------------------------------------------------------------
    static inline float renderHiHat(VoiceState& s, double sr, float tuneSt, float decaySec,
                                    float sizzle, float metallic, bool isOpen) noexcept
    {
        if (! s.initialized)
        {
            s.reset();
            s.ampEnv = 1.0;
            s.initialized = true;
        }

        // Classic 808 6-square-wave metallic ratios:
        // 205Hz, 305Hz, 365Hz, 417Hz, 540Hz, 588Hz (scaled by tune)
        const double base = 250.0 * std::pow(2.0, (double) tuneSt / 12.0);
        static const double ratios[6] = { 0.82, 1.22, 1.46, 1.67, 2.16, 2.35 };

        float cluster = 0.f;
        for (int i = 0; i < 6; ++i)
        {
            const double f = base * ratios[i];
            s.phase1 += f / sr;
            if (s.phase1 >= 1.0) s.phase1 -= std::floor(s.phase1);
            // Square wave (-1 or 1)
            cluster += (s.phase1 < 0.5 ? 0.16f : -0.16f);
        }

        // Blend metallic cluster with white noise for high-end sizzle
        const float n = s.fastRandom();
        float sig = cluster * (0.4f + metallic * 0.5f) + n * (0.3f + sizzle * 0.4f);

        // High-pass filter (cutoff ~ 7500 Hz)
        const float hpCut = juce::jlimit(4000.f, 12000.f, 7000.f + sizzle * 3000.f);
        const float hpCoef = std::exp(-2.f * (float) juce::MathConstants<double>::pi * hpCut / (float) sr);
        s.filterB0 = hpCoef * (s.filterB0 + sig - s.lastSample);
        s.lastSample = sig;

        float out = s.filterB0 * (float) s.ampEnv;

        // Decay envelope
        const double decTime = isOpen ? juce::jmax(0.15, (double) decaySec)
                                      : juce::jlimit(0.015, 0.12, (double) decaySec * 0.12);
        const double decCoef = std::exp(-1.0 / (decTime * sr));
        s.ampEnv *= decCoef;

        return out * 1.8f;
    }

    // -----------------------------------------------------------------------
    // HANDCLAP: Multi-pulse micro-bursts + resonant diffuse reverb tail
    // -----------------------------------------------------------------------
    static inline float renderClap(VoiceState& s, double sr, float tuneSt, float decaySec,
                                   float spread, float resonance) noexcept
    {
        if (! s.initialized)
        {
            s.reset();
            s.ampEnv = 1.0;
            s.burstCount = 0;
            s.burstTimer = 0.f;
            s.initialized = true;
        }

        // Staggered claps: 3 initial rapid bursts spaced ~11ms apart
        const float burstSpacing = (float)(0.009 + spread * 0.008) * (float) sr;
        s.burstTimer += 1.f;
        if (s.burstCount < 3 && s.burstTimer >= burstSpacing)
        {
            s.burstTimer = 0.f;
            s.burstCount++;
            s.ampEnv = 1.0; // retrigger burst envelope
        }

        const float raw = s.fastRandom();

        // Bandpass filter centered at ~1200 Hz
        const float centerFreq = (float)(1100.0 * std::pow(2.0, (double) tuneSt / 12.0));
        const float q = 2.0f + resonance * 2.5f;
        const float f = 2.f * std::sin((float)(juce::MathConstants<double>::pi * centerFreq / sr));
        s.filterB0 += f * (raw - s.filterB0 - s.filterB1 * (1.f / q));
        s.filterB1 += f * s.filterB0;

        float out = s.filterB0 * (float) s.ampEnv;

        // During burst phase: fast decay. After 3 bursts: diffuse tail decay
        const double decTime = (s.burstCount < 3) ? 0.012 : juce::jmax(0.08, (double) decaySec);
        const double decCoef = std::exp(-1.0 / (decTime * sr));
        s.ampEnv *= decCoef;

        return std::tanh(out * 2.4f);
    }

    // -----------------------------------------------------------------------
    // TOM: Dual head resonant drum with downward pitch sweep
    // -----------------------------------------------------------------------
    static inline float renderTom(VoiceState& s, double sr, float tuneSt, float decaySec,
                                  float pitchBend, float click) noexcept
    {
        if (! s.initialized)
        {
            s.reset();
            s.pitchEnv = 1.0;
            s.ampEnv = 1.0;
            s.initialized = true;
        }

        // Tunable tom root: ~110 Hz for mid tom
        const double baseFreq = 110.0 * std::pow(2.0, (double) tuneSt / 12.0);
        const double sweep = 75.0 * (1.0 + pitchBend * 1.5);
        const double freq = baseFreq + sweep * s.pitchEnv;

        s.phase1 += freq / sr;
        if (s.phase1 >= 1.0) s.phase1 -= std::floor(s.phase1);

        float osc = std::sin((float)(s.phase1 * 2.0 * juce::MathConstants<double>::pi));

        // Attack click
        float stickClick = 0.f;
        if (s.pitchEnv > 0.4)
            stickClick = s.fastRandom() * (float)(s.pitchEnv - 0.4) * (0.4f + click);

        float out = (osc + stickClick) * (float) s.ampEnv;

        const double pitchDecCoef = std::exp(-1.0 / (0.045 * sr));
        const double ampDecCoef = std::exp(-1.0 / (juce::jmax(0.06, (double) decaySec) * sr));

        s.pitchEnv *= pitchDecCoef;
        s.ampEnv *= ampDecCoef;

        return std::tanh(out * 1.4f);
    }

    // -----------------------------------------------------------------------
    // PERCUSSION / COWBELL / RIMSHOT / ZAP
    // -----------------------------------------------------------------------
    static inline float renderPerc(VoiceState& s, double sr, float tuneSt, float decaySec,
                                   float tone, float mod) noexcept
    {
        if (! s.initialized)
        {
            s.reset();
            s.ampEnv = 1.0;
            s.pitchEnv = 1.0;
            s.initialized = true;
        }

        // Dual square wave modal cowbell fundamentals: 587Hz & 845Hz
        const double f1 = 587.0 * std::pow(2.0, (double) tuneSt / 12.0);
        const double f2 = 845.0 * std::pow(2.0, (double) tuneSt / 12.0);

        s.phase1 += f1 / sr;
        if (s.phase1 >= 1.0) s.phase1 -= std::floor(s.phase1);
        s.phase2 += f2 / sr;
        if (s.phase2 >= 1.0) s.phase2 -= std::floor(s.phase2);

        const float sq1 = (s.phase1 < 0.5 ? 0.5f : -0.5f);
        const float sq2 = (s.phase2 < 0.5 ? 0.5f : -0.5f);
        const float raw = sq1 + sq2;

        // Bandpass filter to round into metallic ding
        const float center = (float)(780.0 * std::pow(2.0, (double) tuneSt / 12.0));
        const float q = 4.5f + tone * 4.0f;
        const float f = 2.f * std::sin((float)(juce::MathConstants<double>::pi * center / sr));
        s.filterB0 += f * (raw - s.filterB0 - s.filterB1 * (1.f / q));
        s.filterB1 += f * s.filterB0;

        float out = s.filterB0 * (float) s.ampEnv;

        const double decCoef = std::exp(-1.0 / (juce::jmax(0.04, (double) decaySec * 0.45) * sr));
        s.ampEnv *= decCoef;

        return std::tanh(out * 2.2f);
    }

    // -----------------------------------------------------------------------
    // CYMBAL / CRASH / RIDE: Dense metallic shimmer
    // -----------------------------------------------------------------------
    static inline float renderCymbal(VoiceState& s, double sr, float tuneSt, float decaySec,
                                     float shimmer, float brightness) noexcept
    {
        if (! s.initialized)
        {
            s.reset();
            s.ampEnv = 1.0;
            s.initialized = true;
        }

        // Inharmonic metallic ring modulation
        const double base = 380.0 * std::pow(2.0, (double) tuneSt / 12.0);
        s.phase1 += base * 1.34 / sr;
        if (s.phase1 >= 1.0) s.phase1 -= std::floor(s.phase1);
        s.phase2 += base * 2.11 / sr;
        if (s.phase2 >= 1.0) s.phase2 -= std::floor(s.phase2);
        s.phase3 += base * 3.47 / sr;
        if (s.phase3 >= 1.0) s.phase3 -= std::floor(s.phase3);

        const float ring = (float) (std::sin(s.phase1 * 6.28318) * std::sin(s.phase2 * 6.28318) +
                                    std::sin(s.phase3 * 6.28318) * 0.5);

        const float n = s.fastRandom();
        float sig = ring * (0.35f + shimmer * 0.35f) + n * (0.35f + brightness * 0.4f);

        // High-pass filter
        const float hpCut = 5000.f + brightness * 3500.f;
        const float hpCoef = std::exp(-2.f * (float) juce::MathConstants<double>::pi * hpCut / (float) sr);
        s.filterB0 = hpCoef * (s.filterB0 + sig - s.lastSample);
        s.lastSample = sig;

        float out = s.filterB0 * (float) s.ampEnv;

        const double decTime = juce::jmax(0.3, (double) decaySec * 2.2);
        const double decCoef = std::exp(-1.0 / (decTime * sr));
        s.ampEnv *= decCoef;

        return out * 1.6f;
    }

    // -----------------------------------------------------------------------
    // CRASH CYMBAL: Wide inharmonic metallic wash & shimmer
    // -----------------------------------------------------------------------
    static inline float renderCrash(VoiceState& s, double sr, float tuneSt, float decaySec,
                                    float shimmer, float spread) noexcept
    {
        if (! s.initialized)
        {
            s.reset();
            s.ampEnv = 1.0;
            s.initialized = true;
        }

        const double base = 320.0 * std::pow(2.0, (double) tuneSt / 12.0);
        const double sp = 1.0 + (double) spread * 0.4;
        s.phase1 += base * 1.29 * sp / sr;
        if (s.phase1 >= 1.0) s.phase1 -= std::floor(s.phase1);
        s.phase2 += base * 1.94 / sr;
        if (s.phase2 >= 1.0) s.phase2 -= std::floor(s.phase2);
        s.phase3 += base * 2.87 * sp / sr;
        if (s.phase3 >= 1.0) s.phase3 -= std::floor(s.phase3);

        const float ring = (float) (std::sin(s.phase1 * 6.28318) * std::sin(s.phase2 * 6.28318) +
                                    std::sin(s.phase3 * 6.28318) * 0.6f);
        const float n = s.fastRandom();
        float sig = ring * (0.4f + shimmer * 0.35f) + n * (0.45f + shimmer * 0.4f);

        const float hpCut = 4200.f + shimmer * 3800.f;
        const float hpCoef = std::exp(-2.f * (float) juce::MathConstants<double>::pi * hpCut / (float) sr);
        s.filterB0 = hpCoef * (s.filterB0 + sig - s.lastSample);
        s.lastSample = sig;

        float out = s.filterB0 * (float) s.ampEnv;
        const double decTime = juce::jmax(0.4, (double) decaySec * 2.8);
        const double decCoef = std::exp(-1.0 / (decTime * sr));
        s.ampEnv *= decCoef;

        return std::tanh(out * 1.8f);
    }

    // -----------------------------------------------------------------------
    // RIDE CYMBAL: Resonant metallic bell ping + shimmer wash
    // -----------------------------------------------------------------------
    static inline float renderRide(VoiceState& s, double sr, float tuneSt, float decaySec,
                                   float ping, float bright) noexcept
    {
        if (! s.initialized)
        {
            s.reset();
            s.ampEnv = 1.0;
            s.pitchEnv = 1.0;
            s.initialized = true;
        }

        const double f1 = 810.0 * std::pow(2.0, (double) tuneSt / 12.0);
        const double f2 = f1 * 1.63;
        s.phase1 += f1 / sr;
        if (s.phase1 >= 1.0) s.phase1 -= std::floor(s.phase1);
        s.phase2 += f2 / sr;
        if (s.phase2 >= 1.0) s.phase2 -= std::floor(s.phase2);

        const float bell = (float) (std::sin(s.phase1 * 6.28318) * 0.6 + std::sin(s.phase2 * 6.28318) * 0.4);
        const float n = s.fastRandom();
        float sig = bell * (0.5f + ping * 0.5f) + n * (0.25f + bright * 0.4f);

        const float hpCut = 3000.f + bright * 3500.f;
        const float hpCoef = std::exp(-2.f * (float) juce::MathConstants<double>::pi * hpCut / (float) sr);
        s.filterB0 = hpCoef * (s.filterB0 + sig - s.lastSample);
        s.lastSample = sig;

        float out = s.filterB0 * (float) s.ampEnv;
        const double decTime = juce::jmax(0.25, (double) decaySec * 2.0);
        const double decCoef = std::exp(-1.0 / (decTime * sr));
        s.ampEnv *= decCoef;

        return std::tanh(out * 2.0f);
    }

    // -----------------------------------------------------------------------
    // RIMSHOT: Wooden modal click & narrow resonance
    // -----------------------------------------------------------------------
    static inline float renderRim(VoiceState& s, double sr, float tuneSt, float decaySec,
                                  float snap, float body) noexcept
    {
        if (! s.initialized)
        {
            s.reset();
            s.ampEnv = 1.0;
            s.pitchEnv = 1.0;
            s.initialized = true;
        }

        const double freq = 1650.0 * std::pow(2.0, (double) tuneSt / 12.0);
        s.phase1 += freq / sr;
        if (s.phase1 >= 1.0) s.phase1 -= std::floor(s.phase1);

        const float osc = std::sin((float)(s.phase1 * 6.28318));
        const float rawNoise = s.fastRandom();

        float click = 0.f;
        if (s.pitchEnv > 0.4)
            click = rawNoise * (float)(s.pitchEnv - 0.4) * (1.2f + snap * 1.5f);

        float sig = osc * (0.4f + body * 0.6f) + click;
        float out = sig * (float) s.ampEnv;

        const double pitchDec = std::exp(-1.0 / (0.008 * sr));
        const double ampDec = std::exp(-1.0 / (juce::jlimit(0.015, 0.20, (double) decaySec * 0.15) * sr));
        s.pitchEnv *= pitchDec;
        s.ampEnv *= ampDec;

        return std::tanh(out * 2.6f);
    }

    // -----------------------------------------------------------------------
    // COWBELL / BELL: Dual 808 square-wave modal ring
    // -----------------------------------------------------------------------
    static inline float renderBell(VoiceState& s, double sr, float tuneSt, float decaySec,
                                   float ring, float tone) noexcept
    {
        if (! s.initialized)
        {
            s.reset();
            s.ampEnv = 1.0;
            s.initialized = true;
        }

        const double f1 = 587.0 * std::pow(2.0, (double) tuneSt / 12.0);
        const double f2 = 845.0 * std::pow(2.0, (double) tuneSt / 12.0);
        s.phase1 += f1 / sr;
        if (s.phase1 >= 1.0) s.phase1 -= std::floor(s.phase1);
        s.phase2 += f2 / sr;
        if (s.phase2 >= 1.0) s.phase2 -= std::floor(s.phase2);

        const float sq1 = (s.phase1 < 0.5 ? 0.5f : -0.5f);
        const float sq2 = (s.phase2 < 0.5 ? 0.5f : -0.5f);
        const float raw = sq1 + sq2;

        const float center = (float)(780.0 * std::pow(2.0, (double) tuneSt / 12.0));
        const float q = 4.5f + tone * 4.0f;
        const float f = 2.f * std::sin((float)(juce::MathConstants<double>::pi * center / sr));
        s.filterB0 += f * (raw - s.filterB0 - s.filterB1 * (1.f / q));
        s.filterB1 += f * s.filterB0;

        float out = s.filterB0 * (float) s.ampEnv;
        const double decTime = juce::jlimit(0.05, 1.2, 0.08 + (double) decaySec * 0.6 * (0.5 + ring * 0.5));
        const double decCoef = std::exp(-1.0 / (decTime * sr));
        s.ampEnv *= decCoef;

        return std::tanh(out * 2.2f);
    }

    // -----------------------------------------------------------------------
    // CONGA / BONGO: Resonant membrane body with subtle pitch drop & slap
    // -----------------------------------------------------------------------
    static inline float renderConga(VoiceState& s, double sr, float tuneSt, float decaySec,
                                    float slap, float bend) noexcept
    {
        if (! s.initialized)
        {
            s.reset();
            s.ampEnv = 1.0;
            s.pitchEnv = 1.0;
            s.initialized = true;
        }

        const double baseFreq = 220.0 * std::pow(2.0, (double) tuneSt / 12.0);
        const double drop = 60.0 * (0.2 + (double) bend * 1.2);
        const double freq = baseFreq + drop * s.pitchEnv;

        s.phase1 += freq / sr;
        if (s.phase1 >= 1.0) s.phase1 -= std::floor(s.phase1);

        float osc = std::sin((float)(s.phase1 * 6.28318));

        float slapNoise = 0.f;
        if (s.pitchEnv > 0.5)
        {
            const float n = s.fastRandom();
            s.filterB0 += 0.35f * (n - s.filterB0);
            slapNoise = (n - s.filterB0) * (float)(s.pitchEnv - 0.5) * (0.8f + slap * 1.5f);
        }

        float out = (osc + slapNoise) * (float) s.ampEnv;

        const double pitchDec = std::exp(-1.0 / (0.025 * sr));
        const double ampDec = std::exp(-1.0 / (juce::jmax(0.04, (double) decaySec * 0.4) * sr));
        s.pitchEnv *= pitchDec;
        s.ampEnv *= ampDec;

        return std::tanh(out * 1.8f);
    }
};

} // namespace f64
