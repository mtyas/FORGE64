# FORGE64

**64-Pad Modular Drum Sampler & Lua DSP Synthesizer Workstation** | Created by **mtyas**

![C++20](https://img.shields.io/badge/C++-20-blue.svg)
![JUCE](https://img.shields.io/badge/JUCE-8.0-orange.svg)
![Format](https://img.shields.io/badge/Formats-VST3_%7C_CLAP_%7C_Standalone-green.svg)
![Version](https://img.shields.io/badge/Version-0.77--Beta-red.svg)
![License](https://img.shields.io/badge/License-GPL_3.0-lightgrey.svg)
![Tests](https://img.shields.io/badge/Tests-64%2F64_Passing-brightgreen.svg)

---

![FORGE64 Pad Grid](docs/images/forge64_grid.png)

---

## Overview

**FORGE64** is an advanced, high-performance 64-pad modular drum sampler, procedural percussion synthesizer, and sequencing workstation built with modern C++20 and the JUCE 8 framework. 

Combining the tactile immediacy of iconic hardware drum machines with the infinite flexibility of modular synthesis and live Lua DSP scripting, FORGE64 is designed for producers, sound designers, and live electronic performers who demand distinctive, organic, and punchy percussive textures without compromise.

FORGE64 runs natively as a **VST3**, **CLAP**, and **Standalone application** on Windows, macOS, and Linux.

---

## Visual Tour

### 🎛️ Pad Editor & Dynamics Sculpting
Each of the 64 pads features a dedicated sound-sculpting suite: 5 dynamic Lua macro knobs, multimode resonant VCF filter with interactive curve, 3-band parametric EQ, VCA compressor with gain-reduction graph, analog waveshaping drive, and dedicated insert multi-FX.

![FORGE64 Pad Editor](docs/images/forge64_pad_edit.png)

---

### 🎼 Polyrhythmic Step Sequencer & Parameter Locks
An 8-track polyrhythmic step sequencer with independent track lengths (1 to 64 steps), customizable swing, per-step velocity bars, ratchets (subdivisions), probability triggers, microtiming offsets, and full Elektron-style Parameter Locks (P-Locks) on all synthesis, filter, and FX parameters. Includes a convenient collapsible Sequencer Drawer accessible from every page!

![FORGE64 Sequencer](docs/images/forge64_sequencer.png)

---

### 🎚️ Studio Mixing & Mastering Console
Route any pad to 16 stereo output buses or 4 studio Aux FX returns (Studio Plate/Hall Reverb, Stereo Tempo-Synced Delay, Analog Tape Saturation, and BBD Modulation). The Master Bus Console features an analog-modeled VCA Glue Compressor with live gain-reduction meter, 4-band Harmonic Master EQ, and Tape Drive.

![FORGE64 Mixer & FX](docs/images/forge64_mixer_fx.png)

---

### ⚡ Live Performance & Macro Dashboard
Dual interactive XY expression pads with customizable axis routings, 8 global performance macro knobs, and 16 roll/repeat buttons for dynamic stutter fills, build-ups, and live stage jamming.

![FORGE64 Performance Mode](docs/images/forge64_performance.png)

---

### 💻 Embedded Lua 5.4 DSP Script Editor
Write, audition, and compile custom real-time synthesis algorithms directly inside the plugin. Features large readable code fonts, syntax highlighting, instruction quota protection (never freezes your DAW), file import/export, and a built-in **AI Prompt Generator** to create new DSP scripts with AI models.

![FORGE64 Lua Editor](docs/images/forge64_lua_editor.png)

---

## Key Features

- **64 Independent Drum Cells Across 4 Banks**:
  - Bank A: Core Electronic & Acoustic
  - Bank B: Heavy / Electro / Industrial Club
  - Bank C: World & Acoustic Percussion
  - Bank D: Melodic Synths, Acid, Plucks & Cyber FX
- **31 Curated Factory DSP Algorithms**:
  - 100% real-time mathematical sound generation with zero dynamic memory allocation in the audio thread.
  - Authentic analog circuit models, physical modal plate/beam resonators, inharmonic metallic FM clusters, Poisson-distribution snare wires, and Karplus-Strong waveguides.
- **5 Dynamic Macro Knobs per Module (`P1` to `P5`)**:
  - Every module custom-labels its 5 macros to its specific DSP architecture (e.g. `PUNCH`, `CLICK`, `SUB`, `WARMTH`, `TAIL BEND` on the 808 Kick).
- **Per-Pad Processing Chain**:
  - **Multimode Resonant VCF**: Lowpass (24 dB/oct), Highpass, Bandpass, Notch with interactive frequency curve and envelope modulation.
  - **3-Band Parametric EQ**: Low Shelf, Parametric Peak, High Shelf with interactive draggable handles.
  - **VCA Compressor**: Stereo-linked detector, Threshold, Ratio (1:1 to 20:1), Attack, Release, and live transfer curve display.
  - **Analog Waveshaping Drive**: Asymmetric tanh saturation with auto-gain compensation.
  - **Analog Waveshaping Drive**: Asymmetric tanh saturation with auto-gain compensation.
  - **22 Dedicated Insert Multi-FX**: Flanger, Chorus, Bitcrusher, Phaser, Hall Reverb (8-delay FDN), Shimmer Reverb, Spring Reverb, Gated Reverb, Mono Delay, Stereo Ping-Pong Delay, Filtered Dub Delay, Tremolo, Vibrato, Auto-Pan, Ring Modulator, Frequency Shifter (quadrature Hilbert), Stereo Detuner, Overdrive, Tube Saturator, Wavefolder, and Compressor.
  - **Multi-Bus Routing**: Route pads across 16 stereo DAW outputs with fallback summing to Main.
  - **16 Choke Groups**: Seamless 5ms click-free cross-fade choking across pads and banks.
- **53-Source Modulation Matrix with Drag-and-Drop Patching**:
  - Live animated waveform and level visualizers directly in the side panel for all 53 sources (LFO curves, chaotic paths, DAHDSR envelopes, step sequences, and MIDI meters).
  - 10 Multi-Wave LFOs (Sine, Triangle, Saw, Ramp, Square, S&H, Glide S&H with 15 tempo divisions).
  - 2 Chaos & Random Generators featuring real-time Lorenz Attractor 3D integration.
  - 10 DAHDSR Envelopes with voice-level polyphonic retriggering.
  - 10 Mod Sequencers with smoothing slew and swing.
  - 8 Global Macro Knobs and full MIDI Latched Sources (Velocity, Mod Wheel, Pitch Bend, Aftertouch).
  - Visual feedback with colored animated modulation rings and live value dots on every knob.
- **Master FX & 14 Aux Return Processors**:
  - 4 independent stereo studio aux buses selectable between 14 algorithms: Studio Reverb, Hall Reverb, Shimmer Reverb, Spring Reverb, Gated Reverb, Stereo Delay, Ping-Pong Delay, Filtered Dub Delay, Tape Saturation, Tube Saturator, Wavefolder, Modulation Ensemble, Frequency Shifter, and Stereo Pitch Detuner.
  - **Master Bus Console**: VCA Stereo Bus Glue Compressor, 4-Band Harmonic EQ, and Tape Drive.
- **Complete MIDI Learn & Full Undo/Redo**:
  - 1-click MIDI CC learn on all parameters and macro controls.
  - Full history undo/redo (`Ctrl+Z` / `Ctrl+Y`) with dedicated header toolbar buttons.
- **Sample Engine**:
  - Drag-and-drop WAV/AIFF sample loading with waveform preview, start/end trimming, loop markers, reverse, and ADSR envelope.

---

## Factory Module Roster (31 Unique DSP Engines)

| Category | Module ID | Name | Core Synthesis Algorithm | Macro Knobs (`P1` – `P5`) |
| :--- | :--- | :--- | :--- | :--- |
| **Kicks** | `kick_808` | 808 Bass Kick | KD-01 Resonant Bridged-T with sub-sine sweep & tail bend | PUNCH, CLICK, SUB, WARMTH, TAIL BEND |
| | `kick_909` | 909 Dance Beater | Dual-envelope transistor punch with diode saturation | BEATER, PUNCH, BODY RES, CURVE, DIODE GRIT |
| | `kick_rock` | Rock Membrane Kick | Two-mode coupled acoustic membrane with beater slap | BEATER WT, HEAD RATIO, MEMBRANE, SHELL AIR, BEATER SLAP |
| | `kick_electro` | Electro 7-Octave | 7-octave exponential sweep with wavetable fold | SWEEP SPD, WAVE MORPH, SUB BOOM, CRUNCH, SPREAD |
| | `kick_hardstyle`| Hardstyle Wavefolder | Overdriven triple Chebyshev wavefolder with tail distortion | PUNCH SPIKE, WAVEFOLD, DIST FILT, TAIL DRIVE, RUMBLE TONE |
| | `kick_sub_fm` | Deep Sub FM Kick | 2-Operator linear phase modulation with decay warp | FM DEPTH, FM RATIO, MOD DECAY, FEEDBACK, WARP |
| **Snares** | `snare_808` | 808 Snare | Dual bridged-T resonators with bandpass noise burst | SNAPPY, TONE, WIRE DECAY, BODY Q, CLICK |
| | `snare_909` | 909 Dance Snare | Tuned dual-triangle body with snappy noise & compression | SNAPPY, CRACK, TONE, SHELL DECAY, COMPRESSION |
| | `snare_rock` | Rock Noise Snare | Acoustic wood shell modal body with Poisson wire impulses | WIRE TENS, SHELL TONE, RIM HIT, BOTTOM HEAD, STEREO AIR |
| | `snare_rimshot`| Maple Rimshot | Resonant hollow hardwood rim strike with acoustic damping | WOOD PITCH, STICK SNAP, CHAMBER, RING DAMP, BRIGHT |
| **Hi-Hats** | `hat_closed` | Closed 808 Hat | 6-Oscillator Schmitt trigger square cluster with highpass | DETUNE, HP CUTOFF, CHIRP, DAMPING, VEL SENS |
| | `hat_open` | Open 808 Hat | 6-Oscillator cluster with dual-decay bell tail & chorus | SIZZLE, METAL RING, BP FILTER, SHIMMER, BELL TONE |
| | `hat_fm` | Linear FM Hat | 3-Operator metallic inharmonic FM network | FM RATIO, FM DEPTH, HP CUT, NOISE MIX, METALLIC BELL |
| | `hat_noise` | Noise Shaker | Triple formant-filtered noise with stick attack | SIZZLE FREQ, RESONANCE, STICK TIP, COLOR TILT, STEREO SPREAD |
| **Claps** | `clap_808` | 808 Handclap | Multi-stage analog flam burst into shaped reverb tail | FLAM SPREAD, FILTER FREQ, BANDPASS Q, ROOM TAIL, HAND COUNT |
| | `clap_room` | Stereo Room Clap | 7-Tap early reflection cluster with stereo diffusion | ROOM SIZE, STEREO WIDTH, FLAM GAP, DAMPING, BODY TONE |
| | `clap_trash` | Trash Gated Clap | Industrial bitcrushed & downsampled gated clap | GATE TIME, CRUSH, DIRT, FILTER TONE, FLAM DENSITY |
| **Toms** | `tom_dual` | Dual-Head Tom | Acoustic dual-head membrane with air cavity coupling | BEND, HEAD RATIO, CLICK, SHELL RING, WARMTH |
| | `tom_simmons` | Simmons SDS-V Tom | Hex-oscillator analog synth tom with noise click | BEND RANGE, BEND SPEED, CLICK LEVEL, NOISE MIX, WAVE SHAPE |
| | `tom_floor` | Floor Tom Sub | Heavy bass drum shell resonance with sub-harmonic thud | SUB BOOM, THUD, TENSION MOD, DAMPING, AIR CAVITY |
| **Cymbals** | `cymbal_crash` | Modal Crash Cymbal | 48-Mode physical modal plate with non-linear strike | BRIGHTNESS, SHIMMER, HIT POS, CHOKE/DAMP, WASH SPREAD |
| | `cymbal_ride` | Acoustic Ride Bell | High-Q bronze bell modes with stick ping & wash | BELL PING, WASH LEVEL, BELL TONE, SHIMMER, DAMPING |
| | `cymbal_china` | China Splash | Inharmonic trash plate with inverted wash & flutter | TRASH, BITE, SPLASH, DECAY CUT, STEREO FLUTTER |
| **Percussion**| `perc_cowbell` | 808 Cowbell | Dual square-wave bandpass filtered metallic bell | TONE, RING, FILTER FREQ, CLICK, SATURATION |
| | `perc_conga` | Latin Conga | Tuned membrane with palm pressure & open/slap strike | SLAP ATTACK, HAND PRESSURE, BODY TONE, RING DAMP, TONE COLOR |
| | `perc_agogo` | Agogo Bell | Dual-chamber high-tuned Latin agogo bell pair | BELL SELECT, METAL RING, STRIKE HARD, BODY FORMANT, BEAT TUNE |
| | `perc_rimshot`| Hardwood Block | High-resonance wood block percussion | WOOD PITCH, CLICK SNAP, RESONANCE, RING DAMP, BRIGHTNESS |
| **Synths** | `synth_zap` | Laser Zap | Exponential dual-slope analog pitch sweeper | SWEEP RANGE, SWEEP SPEED, WAVE SHAPE, FEEDBACK, STEREO DETUNE |
| | `synth_acid` | 303 Acid Bass | Diode-ladder resonant lowpass with accent envelope | CUTOFF, RESONANCE, ENV MOD, WAVE SELECT, ACCENT DRIVE |
| | `synth_noise` | Cross-Mod Noise | Cross-modulated dual-oscillator FM chaos generator | CHAOS DEPTH, FREQ RATIO, FILTER CUT, FILTER RES, FILTER MODE |
| | `synth_karplus`| Karplus-Strong Pluck| Physical waveguide string with pick position & damping | DAMPING, PICK POS, BODY RES, BRIGHTNESS, MATERIAL |

---

## 64 Default Curated Pads Map

FORGE64 loads with 64 individually tuned, distinct pads configured across 4 banks:

```
[ BANK A: Core Electronic & Acoustic ]
A01: 808 Sub Kick      A02: 808 Snare        A03: Closed Hat       A04: Open Hat
A05: 808 Clap          A06: Acoustic Low Tom A07: Acoustic Mid Tom A08: Acoustic Hi Tom
A09: Maple Rimshot     A10: 808 Cowbell      A11: Sizzle Shaker    A12: Modal Crash
A13: Ride Bell         A14: Latin Conga      A15: Laser Zap        A16: Rock Kick

[ BANK B: Heavy / Electro / Industrial Club ]
B01: 909 Punch Kick    B02: 909 Dance Snare  B03: Linear FM Hat    B04: FM Cyber Bell
B05: Stereo Room Clap  B06: Simmons Low Tom  B07: Simmons Mid Tom  B08: Simmons Hi Tom
B09: Hardstyle Kick    B10: Trash Gated Clap B11: Rock Noise Snare B12: Trash China Splash
B13: Electro 7-Oct Kick B14: Cross-Mod Noise B15: 303 Acid Stab    B16: Deep Sub FM Kick

[ BANK C: World & Acoustic Percussion ]
C01: Floor Tom Sub     C02: Studio Wood Rim  C03: Agogo High Bell  C04: Agogo Low Bell
C05: Conga Slap        C06: Conga Low Mute   C07: Hardwood Block Hi C08: Hardwood Block Lo
C09: High Disco Cowbell C10: Low Latin Cha-Cha C11: Pure Ride Ping C12: China Choke
C13: Fast Splash Wash  C14: Air Noise Shaker C15: Acoustic Snare Rim C16: 24-Inch Deep Bass

[ BANK D: Melodic Synths, Acid, Plucks & Cyber FX ]
D01: Karplus Nylon     D02: Karplus Steel Wire D03: Karplus Bass Pluck D04: 303 Screaming Reso
D05: 303 Square Bass   D06: Acid Rave Lead   D07: Space Invader Zap D08: Downer Laser Drop
D09: Laser Chirp Stereo D10: FM Chaos Static D11: Metallic FM Drone D12: Dubstep Low Rattle
D13: Brutal Folded Raw D14: Electro Air Crunch D15: Ambient Hall Clap D16: Plate Shimmer
```

---

## User Manual

For a complete step-by-step walkthrough of all features, modulation routing, sequencer parameter locks, and Lua DSP scripting:

📖 **[Read the Full FORGE64 User Manual](docs/MANUAL.md)**

---

## Installation & Binary Paths

Pre-compiled binary packages for **Windows** (x86_64) are available on the [GitHub Releases Page](https://github.com/mtyas/FORGE64/releases).

| Format | Windows Destination Path |
| :--- | :--- |
| **VST3 (System)** | `C:\Program Files\Common Files\VST3\FORGE64.vst3` |
| **VST3 (User)** | `%LOCALAPPDATA%\Programs\Common\VST3\FORGE64.vst3` |
| **CLAP (System)** | `C:\Program Files\Common Files\CLAP\FORGE64.clap` |
| **CLAP (User)** | `%LOCALAPPDATA%\Programs\Common\CLAP\FORGE64.clap` |
| **Standalone** | Any directory (e.g. `C:\Program Files\FORGE64\FORGE64.exe`) |

---

## Building from Source

FORGE64 uses modern **CMake 3.22+**, **JUCE 8.0.8**, and embedded **Lua 5.4**. All external dependencies are automatically fetched at configure time via `FetchContent`.

### Prerequisites
- Windows: Visual Studio 2022 (MSVC v143) with C++20 support
- macOS: Xcode 14+ (Universal binary arm64 / x86_64)
- Linux: GCC 12+ or Clang 14+ with ALSA, X11, and FreeType dev libraries

### Build Commands
```bash
# Clone repository
git clone https://github.com/mtyas/FORGE64.git
cd FORGE64

# Configure CMake
cmake -B build_win -S .

# Compile Release binaries (VST3, CLAP, Standalone, and Tests)
cmake --build build_win --config Release -j 8
```

### Automated Verification Test Suite
FORGE64 includes an automated headless DSP and audio test suite (`test_audio.exe`) verifying that all 64 pads compile cleanly in Lua, execute safely within CPU budget, trigger via audition and MIDI, and play back through the internal step sequencer:

```bash
.\build_win\test_audio_artefacts\Release\test_audio.exe
```

---

## Support & Donations

To support my work and encourage future synthesizer development, please consider buying me a coffee:

[![Support on Ko-fi](https://img.shields.io/badge/Ko--fi-Support%20My%20Work-ff5e5b?logo=ko-fi&logoColor=white)](https://ko-fi.com/mtyas)

---

## License

This software is released under the **GNU General Public License v3.0 (GPL-3.0)**. See [LICENSE](LICENSE) for details.

Developed with passion by **Matthew Tyas (mtyas)**.
