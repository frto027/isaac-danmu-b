#include "pch.h"
#include "ilua.h"
#include "danmu.h"

int c_module_reference_count = 0;

extern "C" {
    __declspec(dllexport) int open(lua_State* L);
}
int close_danmuB(lua_State* L);

int open(lua_State * L) {
    if (c_module_reference_count) {
        // we don't need to reinit the danmuB object...
        return 0;
    }
    LuaState luastate(L);

    lua_createtable(L, 0, 10); // the danmuB object
    lua_createtable(L, 0, 1);  // the metatable of danmuB object
    lua_pushstring(L, "__gc");
    lua_pushcfunction(L, close_danmuB);
    lua_settable(L, -3);
    lua_setmetatable(L, -2);

    //now, the stack top is danmuB object;
    c_module_reference_count++;
    lua_setglobal(L, "danmuB");
    Isaac::ConsoleOutput("danmuB service started.\n");

    //do some init works
    danmu_init();

    return 0;
}

int close_danmuB(lua_State* L) {
    LuaState luastate(L);

    c_module_reference_count--;
    if (c_module_reference_count == 0) {
        Isaac::ConsoleOutput("danmuB service stoped.\n");
        danmu_cleanup();
    }
    else if (c_module_reference_count < 0) {
        Isaac::ConsoleOutput("something bad happened(reference count error).\n");
    }

    return 0;
}
