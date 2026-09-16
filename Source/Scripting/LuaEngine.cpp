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
    const lua_Integer i = lua_getinteger(Ls, 1);
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
    const lua_Integer i = lua_getinteger(Ls, 1);
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
    const lua_Integer i = lua_getinteger(Ls, 1);
    if (ctx != nullptr && ctx->outL != nullptr && i >= 0 && i < ctx->n)
        ctx->outL[i] = (float) lua_tonumber(Ls, 2);
    return 0;
}

int lf_outR(lua_State* Ls)
{
    auto* e = (LuaEngine*) lua_touserdata(Ls, lua_upvalueindex(1));
    auto* ctx = e->activeCtxGet();
    const lua_Integer i = lua_getinteger(Ls, 1);
    if (ctx != nullptr && ctx->outR != nullptr && i >= 0 && i < ctx->n)
        ctx->outR[i] = (float) lua_tonumber(Ls, 2);
    return 0;
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
            const juce::StringRef s(name);
            if      (s == "level")  v = p.level;
            else if (s == "pan")    v = p.pan;
            else if (s == "tune")   v = p.tune;
            else if (s == "decay")  v = p.decay;
            else if (s == "drive")  v = p.drive;
            else if (s == "fx1")    v = p.fx1;
            else if (s == "fx2")    v = p.fx2;
            else if (s == "fx3")    v = p.fx3;
            else if (s == "fx4")    v = p.fx4;
            else if (s == "fxtype") v = (float) p.fxType;
            else if (s == "senda")  v = p.sendA;
            else if (s == "sendb")  v = p.sendB;
            else if (s == "eqlg")   v = p.eqLG;
            else if (s == "eqmg")   v = p.eqMG;
            else if (s == "eqhg")   v = p.eqHG;
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
        { LUA_GNAME,       luaopen_base   },
        { LUA_TABLIBNAME,  luaopen_table  },
        { LUA_STRLIBNAME,  luaopen_string },
        { LUA_MATHLIBNAME, luaopen_math   },
        { nullptr, nullptr }
    };
    for (const luaL_Reg* lib = libs; lib->func != nullptr; ++lib)
    {
        lua_pushcfunction(L, lib->func);
        lua_pushstring(L, lib->name);
        lua_call(L, 1, 0);
    }

    static const char* banned[] = { "dofile", "loadfile", "load", "print", nullptr };
    for (const char** b = banned; *b != nullptr; ++b)
    {
        lua_pushnil(L);
        lua_setglobal(L, *b);
    }

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
                        float vel, double age, const PadParams& p)
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
    ctx.params = &p;
    activeCtx = &ctx;

    lua_State* Ls = s.L;
    lua_pushinteger(Ls, n);        lua_setglobal(Ls, "n");
    lua_pushnumber(Ls, sr);        lua_setglobal(Ls, "sr");
    lua_pushnumber(Ls, vel);       lua_setglobal(Ls, "vel");
    lua_pushnumber(Ls, age);       lua_setglobal(Ls, "age");

    lua_getglobal(Ls, "process");
    if (lua_isfunction(Ls, -1))
    {
        // pcall catches script errors; the instruction hook cannot longjmp
        // past this frame, so the C++ lock scope stays intact.
        lua_sethook(Ls, luaHook, LUA_MASKCOUNT, 4000000);
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

void LuaEngine::process(int, float*, float*, int, double, float, double, const PadParams&) {}

#endif

} // namespace f64
