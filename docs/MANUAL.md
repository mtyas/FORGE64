# FORGE64 User Manual

**Version 0.77 (Beta)** | Created by **Matthew Tyas (mtyas)**

---

## Table of Contents
1. [Introduction & Architecture](#1-introduction--architecture)
2. [Interface & Global Controls](#2-interface--global-controls)
3. [The Pad Grid (Banks A–D)](#3-the-pad-grid-banks-ad)
4. [Pad Editor & Sound Sculpting](#4-pad-editor--sound-sculpting)
   - [Synthesis Engine vs. Sample Mode](#synthesis-engine-vs-sample-mode)
   - [The 5 Macro Knobs (P1–P5)](#the-5-macro-knobs-p1p5)
   - [Multimode Resonant VCF](#multimode-resonant-vcf)
   - [3-Band Parametric EQ](#3-band-parametric-eq)
   - [VCA Dynamics Compressor](#vca-dynamics-compressor)
   - [Insert Multi-FX & Aux Sends](#insert-multi-fx--aux-sends)
5. [Embedded Lua DSP Scripting](#5-embedded-lua-dsp-scripting)
   - [Real-Time Audio Safety (The Zero-Allocation Rule)](#real-time-audio-safety-the-zero-allocation-rule)
   - [Lua DSP API Reference](#lua-dsp-api-reference)
   - [Using the AI Prompt Generator](#using-the-ai-prompt-generator)
6. [Polyrhythmic Step Sequencer](#6-polyrhythmic-step-sequencer)
   - [Header Navigation & Pattern Selection](#header-navigation--pattern-selection)
   - [Track Settings & Polyrhythms](#track-settings--polyrhythms)
   - [Step Editing: Velocity, Ratchets & Probability](#step-editing-velocity-ratchets--probability)
   - [Parameter Locks (P-Locks)](#parameter-locks-p-locks)
   - [Collapsible Sequencer Drawer](#collapsible-sequencer-drawer)
7. [Song Mode & Arranger](#7-song-mode--arranger)
8. [Mixer & Studio FX Console](#8-mixer--studio-fx-console)
   - [16-Bus Multi-Out Routing](#16-bus-multi-out-routing)
   - [Aux Return FX Processors](#aux-return-fx-processors)
   - [Master Bus Console](#master-bus-console)
9. [Performance Mode](#9-performance-mode)
10. [53-Source Modulation Matrix](#10-53-source-modulation-matrix)
11. [Preset System & File Formats](#11-preset-system--file-formats)
12. [Appendix: 31 Factory DSP Modules Reference](#12-appendix-31-factory-dsp-modules-reference)

---

## 1. Introduction & Architecture

**FORGE64** is a hybrid percussion workstation combining algorithmic procedural sound design, acoustic physical modeling, sample playback, and polyrhythmic sequencing into a unified, responsive instrument.

```
                    ┌──────────────────────────────────────────────┐
                    │               FORGE64 WORKSTATION            │
                    └──────────────────────┬───────────────────────┘
                                           │
         ┌───────────────────┬─────────────┴───────┬───────────────────┐
         ▼                   ▼                     ▼                   ▼
    [ PAD GRID ]       [ PAD EDITOR ]        [ SEQUENCER ]       [ MIXER & FX ]
    64 Cells (4 Banks)  5-Macro Lua DSP       8-Track Polyrhythm  16 Stereo Buses
    Drag Sample Import  Resonant VCF          P-Locks & Ratchet   4 Aux Return FX
    Choke & Bus Matrix  EQ / Comp / Multi-FX  Song Mode Arranger  Master Bus VCA
```

Every pad operates as an autonomous voice capable of running procedural Lua DSP scripts or streaming multi-channel samples through an analog-modeled processing chain.

---

## 2. Interface & Global Controls

The top header bar provides instant access to global navigation and session utilities:

- **Logo & Branding**: Displays plugin name and `A CREATION OF MTYAS`.
- **Bank Selectors (`A`, `B`, `C`, `D`)**: Quickly switches active bank (Pads 1–16, 17–32, 33–48, 49–64).
- **Navigation Tabs**:
  - `PAD GRID`: 16-pad view with drag-and-drop sample import and solo/mute switches.
  - `PAD EDIT`: Focused deep-dive editor for the selected pad.
  - `MIX & FX`: Studio console with aux sends and master bus dynamics.
  - `SEQUENCER`: 8-track polyrhythmic sequencer and song arranger.
  - `PERFORM`: Dual XY expression pads, macro controls, and roll buttons.
- **Undo / Redo (`Ctrl+Z` / `Ctrl+Y`)**: Deep history stack for parameter adjustments.
- **MIDI Learn**: Click to arm. Touch any on-screen knob or slider, then move your hardware MIDI controller to pair instantly.
- **KIT / BANK / PAD Menus**: Three-tier preset saving and loading.
- **MASTER Knob**: Global master output level with animated modulation ring.

---

## 3. The Pad Grid (Banks A–D)

The Pad Grid displays a responsive 4×4 layout representing the active bank.

- **Auditioning**: Left-click any pad to trigger an audition hit at default velocity.
- **Selecting / Editing**: Click the `EDIT` button on any pad to jump directly to its Pad Editor.
- **Mute / Solo (`M` / `S`)**: Mute or solo pads independently. Soloing overrides all non-soloed pads.
- **Renaming**: Double-click any pad name to enter a custom label.
- **Sample Drag & Drop**: Drag any `.wav`, `.aif`, `.flac`, or `.ogg` file from your desktop or file browser directly onto a pad to immediately load it.
- **Pad Copy (Alt + Drag)**: Hold `Alt` and drag one pad onto another to duplicate its entire sound design, parameters, and script.

---

## 4. Pad Editor & Sound Sculpting

### Synthesis Engine vs. Sample Mode
Each pad can toggle between **Lua DSP Synthesizer** and **Sample Playback**:
- In **Lua DSP Mode**, sound is generated mathematically in real time with continuous parameter responsiveness and unlimited velocity dynamics.
- In **Sample Mode**, an audio file is decoded with customizable start, end, loop points, reverse playback, and an ADSR amp envelope.

### The 5 Macro Knobs (P1–P5)
The 5 macro knobs dynamically adapt their labels and behavioral ranges according to the currently loaded DSP algorithm:
- On an **808 Kick**: `PUNCH`, `CLICK`, `SUB`, `WARMTH`, `TAIL BEND`.
- On a **909 Snare**: `SNAPPY`, `CRACK`, `TONE`, `SHELL DECAY`, `COMPRESSION`.
- On a **Modal Crash**: `BRIGHTNESS`, `SHIMMER`, `HIT POS`, `CHOKE/DAMP`, `WASH SPREAD`.
- In addition, **Pitch / Tune** (±24 semitones), **Decay**, and **Drive** provide core shaping across all modules.

### Multimode Resonant VCF
- Modes: `Bypass`, `24dB Lowpass`, `Highpass`, `Bandpass`, `Notch`.
- Controls: Cutoff (20 Hz – 20 kHz), Resonance (0.1 – 10.0), and Envelope Amount (bipolar -1.0 to +1.0).
- Features an interactive frequency curve display showing filter cutoff slope and peak resonance.

### 3-Band Parametric EQ
- **Low Band**: Low Shelf with frequency (20 Hz – 2 kHz) and gain (±18 dB).
- **Mid Band**: Parametric Bell with frequency (100 Hz – 8 kHz) and gain (±18 dB).
- **High Band**: High Shelf with frequency (2 kHz – 16 kHz) and gain (±18 dB).
- Includes an interactive draggable frequency-response graph.

### VCA Dynamics Compressor
- Controls: Threshold (-60 dB to 0 dB), Ratio (1:1 to 20:1), Attack (0.1 ms – 100 ms), and Release (10 ms – 1000 ms).
- Interactive transfer characteristic plot displaying knee and compression slope.

### Insert Multi-FX & Aux Sends
- **Dedicated Multi-FX Insert**: Choose between `Flanger`, `Chorus`, `Bitcrusher`, and `Phaser` with 4 dedicated modulation parameters (`P1` to `P4`).
- **Aux Sends (`Send A`, `Send B`, `Send C`, `Send D`)**: Route dry signal post-insert into the 4 studio Aux FX buses.

---

## 5. Embedded Lua DSP Scripting

### Real-Time Audio Safety (The Zero-Allocation Rule)
FORGE64 runs Lua 5.4 in the real-time audio thread. To prevent audio dropouts, clicks, or host glitches:
1. **Never allocate tables `{}` inside `process()`**.
2. Pre-allocate all delay buffers, modal tables, and filter state variables at file scope.
3. Every script execution is governed by a **strict instruction hook quota** to protect against infinite loops.

### Lua DSP API Reference
```lua
-- Available global variables inside process():
n     -- Number of samples in the current audio block (e.g. 64, 128, 256, 512)
sr    -- Sample rate in Hz (e.g. 44100.0, 48000.0, 96000.0)
vel   -- Velocity of current trigger (0.0 to 1.0)
age   -- Time elapsed since trigger event in seconds (floating point)
trig  -- Boolean true on the exact block a new trigger occurs; false otherwise
note  -- MIDI note number of trigger (0 to 127)

-- Functions:
param("pitch" | "decay" | "drive" | "p1" | "p2" | "p3" | "p4" | "p5")
outL(sampleIndex, floatValue)  -- Write left channel sample (-1.0 to 1.0)
outR(sampleIndex, floatValue)  -- Write right channel sample (-1.0 to 1.0)
inL(sampleIndex)               -- Read input left buffer
inR(sampleIndex)               -- Read input right buffer
rnd(min, max)                  -- Fast uniform random number generator
```

### Using the AI Prompt Generator
Click the **AI PROMPT** button in the Pad Editor or Lua Editor to copy a formatted system prompt to your clipboard. Paste this prompt into Claude, ChatGPT, or Gemini to generate new physical models, analog drums, or synthetic textures fully compliant with FORGE64's API.

---

## 6. Polyrhythmic Step Sequencer

### Header Navigation & Pattern Selection
The sequencer header is divided into two ergonomic rows:
- **Row 1**: Mode toggle (`PATTERN MODE` / `SONG MODE`), internal transport `PLAY`, `SYNC: DAW` follow toggle, global `SWING` slider (50%–75%), pattern preset selector, and page jumps (`1-16`, `17-32`, `33-48`, `49-64`).
- **Row 2**: 16 dedicated pattern buttons (`1` to `16`) and clipboard operations (`COPY`, `PASTE`, `CLEAR`, `RAND`).

### Track Settings & Polyrhythms
Each of the 8 tracks features:
- **Assigned Pad**: Click the pad dropdown to trigger any of the 64 pads.
- **Track Length**: Set step length individually from **1 to 64 steps**. Combining a 16-step kick with a 14-step snare and 7-step hi-hat creates evolving polyrhythms.

### Step Editing: Velocity, Ratchets & Probability
- **Step Click**: Left-click to activate or deactivate a step.
- **Velocity Drag**: Click and drag vertically inside an active step to adjust velocity.
- **Ratchets**: Subdivide any step into 2, 3, or 4 quick bursts (trap rolls and flams).
- **Probability**: Set triggering probability from 0% to 100% for humanized organic variations.
- **Microtiming**: Shift step placement earlier or later (-50% to +50% of a step).

### Parameter Locks (P-Locks)
Hold `Ctrl` and click any step in the sequencer to enter **P-Lock Mode**. Any parameter moved on the Pad Editor while P-Lock is active will latch exclusively to that step!

### Collapsible Sequencer Drawer
Located at the bottom of the plugin window, the **Sequencer Drawer** can be expanded at any time using `[^] SEQUENCER DRAWER`, allowing you to tweak steps while remaining in the Pad Grid or Pad Editor views.

---

## 7. Song Mode & Arranger

Click `PATTERN MODE` to toggle into `SONG MODE`. 
- Build a song sequence by chaining pattern blocks.
- Set repeat counts (1 to 16 times) per block.
- Create song structures: Intro (Pattern 1 x 2) -> Verse (Pattern 2 x 4) -> Chorus (Pattern 3 x 4) -> Outro.

---

## 8. Mixer & Studio FX Console

### 16-Bus Multi-Out Routing
- Assign any pad to one of 16 stereo output buses (`Bus 1` to `Bus 16`).
- Enable multiple outputs in your DAW to mix individual drum elements on dedicated DAW channels.
- If a secondary bus is disabled in the host, audio automatically sums cleanly to the Main bus.

### Aux Return FX Processors
- **Aux 1 (Studio Reverb)**: Algorithmic plate and room reverb with damping, pre-delay, and stereo width.
- **Aux 2 (Stereo Delay)**: Stereo cross-feedback delay with millisecond or tempo sync divisions and high-frequency damping.
- **Aux 3 (Tape / Saturation)**: ADAA non-linear tape and tube saturation with adjustable bias and tone control.
- **Aux 4 (Modulation Ensemble)**: Dimensional BBD chorus and multi-stage flanger.

### Master Bus Console
- **VCA Glue Compressor**: Solid-state bus compressor modeled after legendary British consoles. Features gain-reduction metering and auto-makeup gain.
- **4-Band Harmonic Master EQ**: Low, Low-Mid, High-Mid, and High mastering shelves with analog phase response.
- **Master Tape Drive**: Subtle harmonic warmth on the final mix.

---

## 9. Performance Mode

The Performance tab is designed for live stage improvisation:
- **Dual Interactive XY Expression Pads**: Freely drag pucks to modulate two parameters simultaneously per pad.
- **8 Global Macro Knobs**: Assignable to any parameter across the plugin.
- **16 Stutter Roll Buttons**: Instant rhythmic rolls at 1/4, 1/8, 1/16, 1/32, and triplet divisions.

---

## 10. 53-Source Modulation Matrix

FORGE64 features a modulation system with visual feedback:

### Available Modulation Sources
- **10 Multi-Wave LFOs**: Sine, Triangle, Sawtooth, Ramp, Square, S&H, and S&H Glide with 15 tempo sync divisions.
- **2 Chaos & Random Generators**: Mathematical Lorenz Attractor 3D chaotic trajectory integration.
- **10 DAHDSR Envelopes**: Delay, Attack, Hold, Decay, Sustain, Release envelopes with per-voice polyphonic retriggering.
- **10 Modulation Sequencers**: 16/32 step CC sequencers with slew smoothing.
- **8 Global Macro Knobs**: Automatable from your DAW.
- **MIDI Sources**: Velocity, Mod Wheel (CC 1), Pitch Bend, Channel Pressure, Polyphonic Aftertouch.

### Drag-and-Drop Patching
1. Grab any colored source badge from the right-hand **Modulation Sources** panel.
2. Drag and drop it directly onto any knob on the screen.
3. An animated modulation ring appears around the knob, showing the modulation depth arc and effective real-time value.

---

## 11. Preset System & File Formats

FORGE64 uses a hierarchy of XML-based presets:

- **Kit Presets (`.kit`)**: Saves the entire state of all 64 pads, sequencer patterns, song arranger, and modulation matrix.
- **Bank Presets (`.bnk`)**: Saves a single 16-pad bank (A, B, C, or D).
- **Pad Presets (`.pad`)**: Saves an individual pad's DSP script, parameters, EQ, compressor, and FX.
- **Module Sound Presets**: Factory and user presets stored in `%LOCALAPPDATA%\Forge64\SoundPresets\<moduleId>`.

---

## 12. Appendix: 31 Factory DSP Modules Reference

### 1. Kicks
- `kick_808`: **KD-01 Resonant Bridged-T Kick**. Sub-sine sweep with punch envelope, beater click, harmonic warmth, and tail pitch sag. (`PUNCH`, `CLICK`, `SUB`, `WARMTH`, `TAIL BEND`).
- `kick_909`: **909 Dance Beater Kick**. Dual-envelope transistor punch with diode saturation. (`BEATER`, `PUNCH`, `BODY RES`, `CURVE`, `DIODE GRIT`).
- `kick_rock`: **Rock Membrane Kick**. Coupled dual-mode acoustic drum head with beater slap. (`BEATER WT`, `HEAD RATIO`, `MEMBRANE`, `SHELL AIR`, `BEATER SLAP`).
- `kick_electro`: **Electro 7-Octave Sweep**. Exponential multi-octave sweep with wavetable fold. (`SWEEP SPD`, `WAVE MORPH`, `SUB BOOM`, `CRUNCH`, `SPREAD`).
- `kick_hardstyle`: **Hardstyle Saturated Wavefolder**. Overdriven Chebyshev wavefolder with tail distortion. (`PUNCH SPIKE`, `WAVEFOLD`, `DIST FILT`, `TAIL DRIVE`, `RUMBLE TONE`).
- `kick_sub_fm`: **Deep Sub FM Kick**. 2-Operator linear phase modulation with decay warp. (`FM DEPTH`, `FM RATIO`, `MOD DECAY`, `FEEDBACK`, `WARP`).

### 2. Snares
- `snare_808`: **808 Dual Bridged-T Resonator**. Tuned twin bridged-T body with highpass noise burst. (`SNAPPY`, `TONE`, `WIRE DECAY`, `BODY Q`, `CLICK`).
- `snare_909`: **909 Dance Snare**. Tuned dual-triangle body with snappy noise and internal compression. (`SNAPPY`, `CRACK`, `TONE`, `SHELL DECAY`, `COMPRESSION`).
- `snare_rock`: **Rock Acoustic Snare**. Wood shell modal body with Poisson-distribution wire impulses. (`WIRE TENS`, `SHELL TONE`, `RIM HIT`, `BOTTOM HEAD`, `STEREO AIR`).
- `snare_rimshot`: **Maple Wood Rimshot**. Hollow resonant hardwood strike with acoustic chamber damping. (`WOOD PITCH`, `STICK SNAP`, `CHAMBER`, `RING DAMP`, `BRIGHT`).

### 3. Hi-Hats
- `hat_closed`: **6-Oscillator 808 Hat**. Inharmonic square-wave cluster with resonant highpass filter. (`DETUNE`, `HP CUTOFF`, `CHIRP`, `DAMPING`, `VEL SENS`).
- `hat_open`: **6-Oscillator Open Hat**. Dual-decay envelope cluster with chorus shimmer and metallic ring. (`SIZZLE`, `METAL RING`, `BP FILTER`, `SHIMMER`, `BELL TONE`).
- `hat_fm`: **Linear FM Hat**. 3-Operator metallic inharmonic phase-modulation network. (`FM RATIO`, `FM DEPTH`, `HP CUT`, `NOISE MIX`, `METALLIC BELL`).
- `hat_noise`: **Noise Shaker**. Triple formant-filtered noise with stick attack impulse. (`SIZZLE FREQ`, `RESONANCE`, `STICK TIP`, `COLOR TILT`, `STEREO SPREAD`).

### 4. Claps
- `clap_808`: **808 Analog Handclap**. Multi-burst flam generator into shaped reverb tail. (`FLAM SPREAD`, `FILTER FREQ`, `BANDPASS Q`, `ROOM TAIL`, `HAND COUNT`).
- `clap_room`: **Stereo Room Clap**. 7-Tap early reflection cluster with stereo diffusion. (`ROOM SIZE`, `STEREO WIDTH`, `FLAM GAP`, `DAMPING`, `BODY TONE`).
- `clap_trash`: **Trash Gated Clap**. Industrial bitcrushed and downsampled gated clap. (`GATE TIME`, `CRUSH`, `DIRT`, `FILTER TONE`, `FLAM DENSITY`).

### 5. Toms
- `tom_dual`: **Acoustic Dual-Head Tom**. Coupled membrane simulation with air cavity resonance. (`BEND`, `HEAD RATIO`, `CLICK`, `SHELL RING`, `WARMTH`).
- `tom_simmons`: **Simmons SDS-V Hex Tom**. Hex-oscillator analog synth tom with noise click. (`BEND RANGE`, `BEND SPEED`, `CLICK LEVEL`, `NOISE MIX`, `WAVE SHAPE`).
- `tom_floor`: **Deep Floor Tom Sub**. Massive drum shell resonance with sub-harmonic thud. (`SUB BOOM`, `THUD`, `TENSION MOD`, `DAMPING`, `AIR CAVITY`).

### 6. Cymbals
- `cymbal_crash`: **Modal Crash Cymbal**. 48-Mode physical modal plate with non-linear strike. (`BRIGHTNESS`, `SHIMMER`, `HIT POS`, `CHOKE/DAMP`, `WASH SPREAD`).
- `cymbal_ride`: **Acoustic Ride Bell**. High-Q bronze bell modes with stick ping and wash. (`BELL PING`, `WASH LEVEL`, `BELL TONE`, `SHIMMER`, `DAMPING`).
- `cymbal_china`: **China Splash**. Inharmonic trash plate with inverted wash and flutter. (`TRASH`, `BITE`, `SPLASH`, `DECAY CUT`, `STEREO FLUTTER`).

### 7. Percussion
- `perc_cowbell`: **808 Dual Square Cowbell**. Bandpass-filtered metallic bell pair. (`TONE`, `RING`, `FILTER FREQ`, `CLICK`, `SATURATION`).
- `perc_conga`: **Latin Conga**. Membrane simulation with slap, open tone, and palm pressure. (`SLAP ATTACK`, `HAND PRESSURE`, `BODY TONE`, `RING DAMP`, `TONE COLOR`).
- `perc_agogo`: **Latin Agogo Bell**. Dual-chamber high-tuned Latin agogo bell pair. (`BELL SELECT`, `METAL RING`, `STRIKE HARD`, `BODY FORMANT`, `BEAT TUNE`).
- `perc_rimshot`: **Hardwood Block**. Resonant hardwood block percussion strike. (`WOOD PITCH`, `CLICK SNAP`, `RESONANCE`, `RING DAMP`, `BRIGHTNESS`).

### 8. Synths & Noise
- `synth_zap`: **Laser Zap**. Exponential dual-slope analog pitch sweeper. (`SWEEP RANGE`, `SWEEP SPEED`, `WAVE SHAPE`, `FEEDBACK`, `STEREO DETUNE`).
- `synth_acid`: **303 Acid Bass**. Diode-ladder resonant lowpass with accent envelope. (`CUTOFF`, `RESONANCE`, `ENV MOD`, `WAVE SELECT`, `ACCENT DRIVE`).
- `synth_noise`: **Cross-Mod Chaos Noise**. Cross-modulated dual-oscillator FM chaos generator. (`CHAOS DEPTH`, `FREQ RATIO`, `FILTER CUT`, `FILTER RES`, `FILTER MODE`).
- `synth_karplus`: **Karplus-Strong Pluck**. Waveguide string pluck with pick position and damping. (`DAMPING`, `PICK POS`, `BODY RES`, `BRIGHTNESS`, `MATERIAL`).

---

*FORGE64 — Built with modern C++20 and JUCE 8 by Matthew Tyas (mtyas).*
