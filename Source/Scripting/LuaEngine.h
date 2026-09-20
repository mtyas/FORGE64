#pragma once
#include <JuceHeader.h>
#include "../Engine/PadDefs.h"

#if FORGE64_SCRIPTING
extern "C" {
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}
#endif

namespace f64 {

// Embedded, sandboxed Lua engine - one VM per pad.
//
// Script API (v1, maps 1:1 onto the planned LuaJIT C-FFI interface):
//   function process()        -- called once per audio block while the pad sounds
//     inL(i) / inR(i)         -- read input sample, i in [0, n)
//     outL(i, v) / outR(i, v) -- write output sample
//     n, sr                   -- block size, sample rate
//     vel, age                -- trigger velocity, seconds since last hit
//     param("level"|"tune"|"drive"|"fx1"...) -- effective pad DSP parameters
//
// Sandbox: base/table/string/math only (no io/os/package/load/dofile/print),
// plus an instruction-count hook so runaway scripts are killed per call.
class LuaEngine
{
public:
    LuaEngine() = default;
    ~LuaEngine();

    void setScript(int pad, const juce::String& code, bool enable);   // message thread
    bool enabledFor(int pad) const { return slots[(size_t) pad].enabled.load(); }
    juce::String errorFor(int pad) const;

    // Audio thread: runs the pad's process() in-place over the pad buffer.
    void process(int pad, float* L, float* R, int n, double sr,
                 float vel, double age, const PadParams& p, bool trig = false, int note = 60);

    struct Ctx
    {
        const float* inL = nullptr;
        const float* inR = nullptr;
        float* outL = nullptr;
        float* outR = nullptr;
        int n = 0;
        double sr = 48000.0;
        float vel = 1.f;
        double age = 0.0;
        bool trig = false;
        int note = 60;
        const PadParams* params = nullptr;
    };

    Ctx* activeCtxGet() { return activeCtx; } // used by C closures

private:
    struct Slot
    {
        juce::CriticalSection lock;      // setScript (message) vs process (audio)
        std::atomic<bool> enabled { false };
        juce::String error;
#if FORGE64_SCRIPTING
        lua_State* L = nullptr;
#endif
    };

#if FORGE64_SCRIPTING
    void closeState(Slot& s);
#endif

    Slot slots[kNumPads];
    Ctx* activeCtx = nullptr; // audio thread only
};

} // namespace f64
