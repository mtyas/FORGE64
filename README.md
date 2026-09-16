# FORGE64

A cross-platform (VST3 + CLAP + Standalone) **modular drum sampler/synthesizer** built in
modern C++20 on JUCE 8. 64 independent pads across 4 banks, per-pad DSP chains, a 53-source
modulation matrix with drag-and-drop patching and live modulation rings, sandboxed per-pad
Lua scripting, 16 stereo output buses, choke groups, and a three-tier preset system.

> **Status: Phase 1 — architecture-complete, compiling foundation.**
> Everything below marked ✅ is implemented in this tree. Items marked 🔜 are designed-for
> (state/serialization slots exist) but land in later phases. See *Roadmap*.

---

## Feature status vs. spec

| Spec item | Status | Notes |
|---|---|---|
| 4 banks × 16 pads = 64 independent cells | ✅ | Bank A/B/C/D buttons + MIDI program-change bank select |
| Responsive 4×4 grid UI | ✅ | Hit-flash animation, per-pad colour, rename, drag-drop sample loading |
| 16 stereo output buses, per-pad selector | ✅ | Disabled buses fall back to Main (nothing goes silent) |
| Choke groups 1–16 / Off, cross-bank | ✅ | 5 ms click-free fade, any pad kills its group |
| Drum trigger mode (MIDI note mapping) | ✅ | Default notes 36–99, per-pad remap, channel filter |
| Chromatic mode (keyboard → pad DSP) | ✅ | Loops sample while held, note-tracking pitch, note-off release |
| Per-pad 3-band parametric EQ | ✅ | Low shelf / peak / high shelf, smoothed coefficient updates |
| Per-pad VCA compressor | ✅ | Stereo-linked detector, thr/ratio/atk/rel |
| Per-pad saturation/drive | ✅ | tanh waveshaper with makeup normalization |
| Per-pad multi-FX insert | ✅ | Flanger / Chorus / Bitcrusher / Phaser (4 generic knobs) |
| 2 aux sends → global FX (A: reverb, B: delay) | ✅ | Post-fader sends; Freeverb-style reverb + stereo delay; returns to Main |
| 10× multi-wave LFOs (S&H, glide, uni/bi, Hz or tempo-sync) | ✅ | 6 shapes incl. S&H + S&H-glide, 15 sync divisions |
| 10× random/chaos (S&H, smooth, probabilistic, drunk, Lorenz) | ✅ | Real Lorenz attractor integration |
| 10× DAHDSR envelopes (curve shaping, loop, retrigger) | ✅ | Global instance **plus 16 per-voice instances** for voice-level routing |
| 10× step/CC mod sequencers (16/32 steps, gate, slew, swing) | ✅ | 4 play directions, interactive step grid editor |
| MIDI sources: velocity, mod wheel, pitch bend, chan/poly AT | ✅ | Latched per block |
| 8 global macro knobs | ✅ | Real host-automatable parameters, matrix-routable |
| Drag-and-drop modulation patching | ✅ | Drag source badges onto **any** knob/chip |
| Coloured modulation rings on controls | ✅ | Per-source-class colours, bipolar sweep arcs, effective-value playhead dot, live at 30 fps |
| Modulation matrix dashboard (amount/invert/mute/delete) | ✅ | Global list + per-pad filtered list; curve-shaping per connection |
| Per-voice modulation targets (amp/pitch/pan) | ✅ | Drop chips in pad dashboard; polyphonic env retrigger |
| Pad zoom/focus dashboard | ✅ | Click pad → full strip (source/EQ/comp/drive/FX/routing/Lua/mods), fade transition |
| Pad copy via drag (ALT-drag pad→pad) | ✅ | Copies state + params + sample; restores default note |
| Sandboxed Lua scripting per pad | ✅ | Lua 5.4, restricted stdlib, CPU-budget hook, C buffer API; 🔜 LuaJIT/FFI swap |
| Kit/Bank/Pad presets (.kit/.bnk/.pad) | ✅ | XML, version-tagged; bank/pad remap on load |
| Host state save/restore | ✅ | Full kit incl. scripts + matrix |
| VST3 | ✅ | Via JUCE 8 |
| CLAP | ✅ | Via clap-juce-extensions (native CLAP params/ports) |
| Hardware-accelerated GUI renderer | 🔜 | Phase 2: JUCE OpenGL/ARF renderer; current UI is vector-drawn at 30–60 fps |
| Sample editing (start/end/loop/fades) | 🔜 | Phase 2 |
| MIDI learn for mappings | 🔜 | Phase 2 |
| Module Parameter API for Lua-exposed params | 🔜 | Phase 2 (`param()` reads DSP params today) |

---

## Building

### Dependencies
- CMake ≥ 3.22, Ninja (or Make), a C++20 compiler (GCC 12+, Clang 14+, MSVC 2022+)
- git + network (JUCE, Lua, clap-juce-extensions are fetched at configure time)
- **Linux:** ALSA, freetype, X11 (+ Xrandr/Xinerama/Xcursor/Xext/Xcomposite) dev packages
  - Debian/Ubuntu: `sudo apt install build-essential cmake ninja-build pkg-config libasound2-dev libfreetype-dev libx11-dev libxext-dev libxrandr-dev libxinerama-dev libxcursor-dev libxcomposite-dev`
  - Homebrew (Linux): `brew install cmake ninja pkg-config alsa-lib freetype libx11 libxext libxrandr libxinerama libxcursor libxcomposite`
- **macOS / Windows:** nothing beyond the toolchain.

### Configure & build
```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```
On Homebrew/Linux you may need:
```bash
export PKG_CONFIG_PATH="$(brew --prefix)/lib/pkgconfig:$(brew --prefix)/share/pkgconfig"
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$(brew --prefix)"
```

### Targets
| Target | Output |
|---|---|
| `Forge64_VST3` | `build/Forge64_artefacts/Release/VST3/FORGE64.vst3` |
| `Forge64_CLAP` | `build/Forge64_artefacts/Release/CLAP/FORGE64.clap` |
| `Forge64_Standalone` | runnable app (great for testing without a host) |

Options: `-DFORGE64_BUILD_CLAP=OFF` (skip CLAP), `-DFORGE64_SCRIPTING=OFF` (drop Lua engine).

---

## Architecture

```
Source/
├── PluginProcessor.*      Audio graph owner: 16 buses, MIDI routing, block orchestration
├── PluginEditor.*         Shell: header (banks/master/preset menus), grid↔zoom switch, timer
├── Engine/
│   ├── PadDefs.h          Constants, slot/colour tables, PadParams POD, param id helpers
│   ├── SampleManager.h    Decode + cache audio files (shared_ptr hand-off to audio thread)
│   ├── PadGrid.*          PADS ValueTree + audio-safe runtime atomics, pad copy/clear
│   ├── VoicePool.*        64 voices: sample-accurate triggers, choke, steal, per-voice mods
│   ├── PadChain.*         Per-pad EQ → comp → drive → multi-FX (64 instances)
│   └── GlobalFX.h         Aux A reverb (Freeverb-style), aux B stereo delay
├── Modulation/
│   ├── ModSources.*       LFO / Random / DAHDSR-Env (16 voice instances) / StepSeq / MIDI / Macro
│   └── ModMatrix.*        Connection cache (lock-free snapshot), per-block offsets, rings-for-UI
├── Scripting/LuaEngine.*  Per-pad sandboxed Lua 5.4 VMs, C buffer API, instruction-budget hook
├── Presets/PresetManager.*  .kit / .bnk / .pad XML read-write with in-place tree merge
└── UI/
    ├── UICommon.h         Dark theme colours + widget styling helpers
    ├── ModRingKnob.*      Knob with live modulation rings; badge drop target; conn menu
    ├── PadGridView.*      4×4 grid: flash anim, file drop, ALT-drag pad copy, context menu
    ├── PadEditor.*        Zoom dashboard: all pad DSP, routing, Lua editor, local matrix
    └── ModPanel.*         53-source list (draggable badges, popup editors) + matrix table
```

### Audio-thread contract
- No allocation, locking, or ValueTree access in `processBlock` beyond:
  - per-pad `PadRuntime` atomics + a `CriticalSection` only at **trigger** time (sample hand-off),
  - a spinlock-protected `shared_ptr` snapshot of the modulation connection cache,
  - source parameters stored in `std::atomic<float>` POD, mirrored from the tree on the message thread.
- MIDI events are applied **sample-accurately**: each pad renders in sub-segments between events.

### Lua script API (per pad, `process()` per block)
```lua
function process()             -- runs pre-FX while the pad sounds
  for i = 0, n - 1 do          -- n, sr, vel, age are globals
    outL(i, inL(i) * param("level"))
    outR(i, inR(i) * param("level"))
  end
end
```
Sandbox: base/table/string/math only — no io/os/package/load/dofile/print — plus a
4M-instruction hook; failing scripts disable themselves and surface the error in the UI.
The API shape mirrors the planned LuaJIT C-FFI buffer interface (drop-in swap, Phase 2).

---

## Roadmap
- **Phase 2 (sound & depth):** sample editor (start/end/loop/fades), better resampling
  (SSRC/lagrange), FDN reverb upgrade, MIDI-learn, curve editor in matrix, undo/redo,
  LuaJIT+FFI swap, per-sample modulation option, poly-AT per-voice routing.
- **Phase 3 (polish & perf):** GPU renderer (OpenGL/Metal/DX via JUCE ARF), pad-zoom
  morph animation, factory kit content + kit browser, CPU metering, multithreaded pad render.
- **Phase 4 (release):** AU/AAX wrappers, installer, host-tested matrix ( Ableton/Reaper/Bitwig/FL ),
  docs site, demo kits.

## Caveats (Phase 1)
- GUI/audio not yet runtime-tested in a host on this build machine (compiled headless);
  expect a first-run polish pass.
- Kits reference samples by absolute path (no embedding yet).
- No undo history yet (attachments use `nullptr` UndoManager).
- FX returns land on the Main bus (dedicated return buses = Phase 2).
