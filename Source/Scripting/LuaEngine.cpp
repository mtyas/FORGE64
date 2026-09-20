#include "LuaEngine.h"

namespace f64 {

LuaEngine::~LuaEngine()
{
#if FORGE64_SCRIPTING
    for (auto& s : slots)
    {
        const juce::ScopedLock sl(s.lock);
        closeState(s);
    }
#endif
}

juce::String LuaEngine::errorFor(int pad) const
{
    if (pad < 0 || pad >= kNumPads)
        return {};
    const juce::ScopedLock sl(slots[(size_t) pad].lock);
    return slots[(size_t) pad].error;
}

#if FORGE64_SCRIPTING

namespace {

void luaHook(lua_State* L, lua_Debug*)
{
    luaL_error(L, "script exceeded CPU budget");
}

int lf_inL(lua_State* Ls)
{
    auto* e = (LuaEngine*) lua_touserdata(Ls, lua_upvalueindex(1));
    auto* ctx = e->activeCtxGet();
    const lua_Integer i = lua_tointeger(Ls, 1);
    if (ctx != nullptr && ctx->inL != nullptr && i >= 0 && i < ctx->n)
        lua_pushnumber(Ls, ctx->inL[i]);
    else
        lua_pushnumber(Ls, 0);
    return 1;
}

int lf_inR(lua_State* Ls)
{
    auto* e = (LuaEngine*) lua_touserdata(Ls, lua_upvalueindex(1));
    auto* ctx = e->activeCtxGet();
    const lua_Integer i = lua_tointeger(Ls, 1);
    if (ctx != nullptr && ctx->inR != nullptr && i >= 0 && i < ctx->n)
        lua_pushnumber(Ls, ctx->inR[i]);
    else
        lua_pushnumber(Ls, 0);
    return 1;
}

int lf_outL(lua_State* Ls)
{
    auto* e = (LuaEngine*) lua_touserdata(Ls, lua_upvalueindex(1));
    auto* ctx = e->activeCtxGet();
    const lua_Integer i = lua_tointeger(Ls, 1);
    if (ctx != nullptr && ctx->outL != nullptr && i >= 0 && i < ctx->n)
        ctx->outL[i] = (float) lua_tonumber(Ls, 2);
    return 0;
}

int lf_outR(lua_State* Ls)
{
    auto* e = (LuaEngine*) lua_touserdata(Ls, lua_upvalueindex(1));
    auto* ctx = e->activeCtxGet();
    const lua_Integer i = lua_tointeger(Ls, 1);
    if (ctx != nullptr && ctx->outR != nullptr && i >= 0 && i < ctx->n)
        ctx->outR[i] = (float) lua_tonumber(Ls, 2);
    return 0;
}

int lf_rnd(lua_State* Ls)
{
    static uint32_t seed = 123456789;
    seed = seed * 1664525u + 1013904223u;
    const float f = (float) seed / 4294967296.0f;
    const int top = lua_gettop(Ls);
    if (top == 1)
    {
        const float mx = (float) lua_tonumber(Ls, 1);
        lua_pushnumber(Ls, f * mx);
    }
    else if (top >= 2)
    {
        const float mn = (float) lua_tonumber(Ls, 1);
        const float mx = (float) lua_tonumber(Ls, 2);
        lua_pushnumber(Ls, mn + f * (mx - mn));
    }
    else
    {
        lua_pushnumber(Ls, f);
    }
    return 1;
}

int lf_pow(lua_State* Ls)
{
    const double b = lua_tonumber(Ls, 1);
    const double e = lua_tonumber(Ls, 2);
    lua_pushnumber(Ls, std::pow(b, e));
    return 1;
}

int lf_atan2(lua_State* Ls)
{
    const double y = lua_tonumber(Ls, 1);
    const double x = lua_tonumber(Ls, 2);
    lua_pushnumber(Ls, std::atan2(y, x));
    return 1;
}

int lf_clamp(lua_State* Ls)
{
    const double v = lua_tonumber(Ls, 1);
    const double mn = lua_tonumber(Ls, 2);
    const double mx = lua_tonumber(Ls, 3);
    lua_pushnumber(Ls, juce::jlimit(mn, mx, v));
    return 1;
}

int lf_log10(lua_State* Ls)
{
    const double x = lua_tonumber(Ls, 1);
    lua_pushnumber(Ls, std::log10(x));
    return 1;
}

int lf_tanh(lua_State* Ls)
{
    const double x = lua_tonumber(Ls, 1);
    lua_pushnumber(Ls, std::tanh(x));
    return 1;
}

int lf_sinh(lua_State* Ls)
{
    const double x = lua_tonumber(Ls, 1);
    lua_pushnumber(Ls, std::sinh(x));
    return 1;
}

int lf_cosh(lua_State* Ls)
{
    const double x = lua_tonumber(Ls, 1);
    lua_pushnumber(Ls, std::cosh(x));
    return 1;
}

int lf_sign(lua_State* Ls)
{
    const double x = lua_tonumber(Ls, 1);
    lua_pushnumber(Ls, (x > 0.0) ? 1.0 : ((x < 0.0) ? -1.0 : 0.0));
    return 1;
}

int lf_round(lua_State* Ls)
{
    const double x = lua_tonumber(Ls, 1);
    lua_pushnumber(Ls, std::round(x));
    return 1;
}

int lf_param(lua_State* Ls)
{
    auto* e = (LuaEngine*) lua_touserdata(Ls, lua_upvalueindex(1));
    auto* ctx = e->activeCtxGet();
    float v = 0.f;
    if (ctx != nullptr && ctx->params != nullptr)
    {
        const char* name = lua_tostring(Ls, 1);
        if (name != nullptr)
        {
            const auto& p = *ctx->params;
            const juce::String s(name);
            if      (s == "level")                v = p.level;
            else if (s == "pan")                  v = p.pan;
            else if (s == "tune" || s == "pitch") v = p.tune;
            else if (s == "decay" || s == "dec")  v = p.decay;
            else if (s == "drive" || s == "drv")  v = p.drive;
            else if (s == "fx1" || s == "p1")     v = p.fx1;
            else if (s == "fx2" || s == "p2")     v = p.fx2;
            else if (s == "fx3" || s == "p3")     v = p.fx3;
            else if (s == "fx4" || s == "p4")     v = p.fx4;
            else if (s == "fx5" || s == "p5")     v = p.fx5;
            else if (s == "fxtype")               v = (float) p.fxType;
            else if (s == "vcft")                 v = (float) p.vcfType;
            else if (s == "vcfc")                 v = p.vcfCut;
            else if (s == "vcfr")                 v = p.vcfRes;
            else if (s == "vcfe")                 v = p.vcfEnv;
            else if (s == "senda")                v = p.sendA;
            else if (s == "sendb")                v = p.sendB;
            else if (s == "eqlg")                 v = p.eqLG;
            else if (s == "eqmg")                 v = p.eqMG;
            else if (s == "eqhg")                 v = p.eqHG;
        }
    }
    lua_pushnumber(Ls, v);
    return 1;
}

lua_State* createSandbox(LuaEngine* engine, const char* code, size_t len, juce::String& err)
{
    lua_State* L = luaL_newstate();
    if (L == nullptr)
    {
        err = "out of memory";
        return nullptr;
    }

    static const luaL_Reg libs[] = {
        { "_G",            luaopen_base   },
        { LUA_TABLIBNAME,  luaopen_table  },
        { LUA_STRLIBNAME,  luaopen_string },
        { LUA_MATHLIBNAME, luaopen_math   },
        { nullptr, nullptr }
    };
    for (const luaL_Reg* lib = libs; lib->func != nullptr; ++lib)
    {
        luaL_requiref(L, lib->name, lib->func, 1);
        lua_pop(L, 1); // remove lib table from stack
    }

    // Expose common math functions directly to globals for DSP convenience
    lua_getglobal(L, "math");
    if (lua_istable(L, -1))
    {
        for (const char* fn : { "sin", "cos", "tan", "asin", "acos", "atan", "exp", "log",
                                "abs", "floor", "ceil", "min", "max", "sqrt", "random" })
        {
            lua_getfield(L, -1, fn);
            lua_setglobal(L, fn);
        }
        lua_getfield(L, -1, "pi");
        lua_setglobal(L, "pi");

        // Backward-compatibility and convenience functions
        lua_pushcfunction(L, lf_pow);   lua_setfield(L, -2, "pow");
        lua_pushcfunction(L, lf_tanh);  lua_setfield(L, -2, "tanh");
        lua_pushcfunction(L, lf_sinh);  lua_setfield(L, -2, "sinh");
        lua_pushcfunction(L, lf_cosh);  lua_setfield(L, -2, "cosh");
        lua_pushcfunction(L, lf_atan2); lua_setfield(L, -2, "atan2");
        lua_pushcfunction(L, lf_clamp); lua_setfield(L, -2, "clamp");
        lua_pushcfunction(L, lf_log10); lua_setfield(L, -2, "log10");
        lua_pushcfunction(L, lf_sign);  lua_setfield(L, -2, "sign");
        lua_pushcfunction(L, lf_round); lua_setfield(L, -2, "round");
    }
    lua_pop(L, 1); // pop math table

    // Also expose global pow, tanh, sinh, cosh, atan2, clamp, log10, sign, round
    lua_pushcfunction(L, lf_pow);   lua_setglobal(L, "pow");
    lua_pushcfunction(L, lf_tanh);  lua_setglobal(L, "tanh");
    lua_pushcfunction(L, lf_sinh);  lua_setglobal(L, "sinh");
    lua_pushcfunction(L, lf_cosh);  lua_setglobal(L, "cosh");
    lua_pushcfunction(L, lf_atan2); lua_setglobal(L, "atan2");
    lua_pushcfunction(L, lf_clamp); lua_setglobal(L, "clamp");
    lua_pushcfunction(L, lf_log10); lua_setglobal(L, "log10");
    lua_pushcfunction(L, lf_sign);  lua_setglobal(L, "sign");
    lua_pushcfunction(L, lf_round); lua_setglobal(L, "round");

    static const char* banned[] = { "dofile", "loadfile", "load", "print", nullptr };
    for (const char** b = banned; *b != nullptr; ++b)
    {
        lua_pushnil(L);
        lua_setglobal(L, *b);
    }

    lua_pushcfunction(L, lf_rnd);
    lua_setglobal(L, "rnd");

    lua_pushlightuserdata(L, engine);
    lua_pushcclosure(L, lf_inL, 1);  lua_setglobal(L, "inL");
    lua_pushlightuserdata(L, engine);
    lua_pushcclosure(L, lf_inR, 1);  lua_setglobal(L, "inR");
    lua_pushlightuserdata(L, engine);
    lua_pushcclosure(L, lf_outL, 1); lua_setglobal(L, "outL");
    lua_pushlightuserdata(L, engine);
    lua_pushcclosure(L, lf_outR, 1); lua_setglobal(L, "outR");
    lua_pushlightuserdata(L, engine);
    lua_pushcclosure(L, lf_param, 1); lua_setglobal(L, "param");

    lua_sethook(L, luaHook, LUA_MASKCOUNT, 4000000);
    int status = luaL_loadbuffer(L, code, len, "=f64pad");
    if (status == 0)
        status = lua_pcall(L, 0, 0, 0); // run chunk to define process()
    lua_sethook(L, nullptr, 0, 0);

    if (status != 0)
    {
        const char* msg = lua_tostring(L, -1);
        err = msg != nullptr ? juce::String(msg) : "script compile error";
        lua_close(L);
        return nullptr;
    }
    return L;
}

} // namespace

void LuaEngine::closeState(Slot& s)
{
    if (s.L != nullptr)
    {
        lua_close(s.L);
        s.L = nullptr;
    }
}

void LuaEngine::setScript(int pad, const juce::String& code, bool enable)
{
    if (pad < 0 || pad >= kNumPads)
        return;

    Slot& s = slots[(size_t) pad];
    const juce::ScopedLock sl(s.lock);

    if (! enable || code.trim().isEmpty())
    {
        closeState(s);
        s.enabled = false;
        s.error = {};
        return;
    }

    juce::String err;
    lua_State* NL = createSandbox(this, code.toRawUTF8(), (size_t) code.getNumBytesAsUTF8(), err);
    closeState(s);
    if (NL != nullptr)
    {
        s.L = NL;
        s.enabled = true;
        s.error = {};
    }
    else
    {
        s.enabled = false;
        s.error = err;
    }
}

void LuaEngine::process(int pad, float* L, float* R, int n, double sr,
                        float vel, double age, const PadParams& p, bool trig, int note)
{
    if (pad < 0 || pad >= kNumPads || n <= 0)
        return;

    Slot& s = slots[(size_t) pad];
    const juce::ScopedLock sl(s.lock);
    if (! s.enabled.load() || s.L == nullptr)
        return;

    Ctx ctx;
    ctx.inL = L; ctx.inR = R;
    ctx.outL = L; ctx.outR = R;
    ctx.n = n; ctx.sr = sr;
    ctx.vel = vel; ctx.age = age;
    ctx.trig = trig;
    ctx.note = note;
    ctx.params = &p;
    activeCtx = &ctx;

    lua_State* Ls = s.L;
    lua_pushinteger(Ls, n);             lua_setglobal(Ls, "n");
    lua_pushnumber(Ls, sr);             lua_setglobal(Ls, "sr");
    lua_pushnumber(Ls, vel);            lua_setglobal(Ls, "vel");
    lua_pushnumber(Ls, age);            lua_setglobal(Ls, "age");
    lua_pushboolean(Ls, trig ? 1 : 0);   lua_setglobal(Ls, "trig");
    lua_pushinteger(Ls, note);          lua_setglobal(Ls, "note");

    lua_getglobal(Ls, "process");
    if (lua_isfunction(Ls, -1))
    {
        // pcall catches script errors; the instruction hook cannot longjmp
        // past this frame, so the C++ lock scope stays intact.
        lua_sethook(Ls, luaHook, LUA_MASKCOUNT, 8000000);
        if (lua_pcall(Ls, 0, 0, 0) != 0)
        {
            const char* msg = lua_tostring(Ls, -1);
            s.error = msg != nullptr ? juce::String(msg) : "script runtime error";
            lua_pop(Ls, 1);
            s.enabled = false;
        }
        lua_sethook(Ls, nullptr, 0, 0);
    }
    else
    {
        lua_pop(Ls, 1);
        s.error = "script has no process() function";
        s.enabled = false;
    }

    activeCtx = nullptr;
}

#else // !FORGE64_SCRIPTING

void LuaEngine::setScript(int pad, const juce::String&, bool)
{
    if (pad >= 0 && pad < kNumPads)
    {
        slots[(size_t) pad].enabled = false;
        slots[(size_t) pad].error = "Lua scripting is disabled in this build";
    }
}

void LuaEngine::process(int, float*, float*, int, double, float, double, const PadParams&, bool, int) {}

#endif

} // namespace f64
