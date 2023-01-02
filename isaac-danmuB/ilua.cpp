#include "pch.h"
#include "ilua.h"

LuaState::LuaState(lua_State *L):L(L),PL(LuaState::GL){
    LuaState::GL = L;
}
LuaState::~LuaState(){
    assert(LuaState::GL == L);
    LuaState::GL = PL;
}

lua_State *LuaState::GL = nullptr;
lua_State *LuaState::getL(){
    assert(LuaState::GL);
    return LuaState::GL;
}

namespace Isaac{
    void DebugString(const char *string){
        auto L = LuaState::getL();
        lua_getglobal(L,"Isaac"); //push Isaac
        lua_getfield(L,-1,"DebugString");//push Isaac.DebugString
        lua_pushstring(L, string);//push argument 1
        lua_call(L, 1, 0);//call Isaac.DebugString; pop argument1 and Isaac.DebugString
        lua_pop(L,1);//pop Isaac
    }
    void ConsoleOutput(const char *string){
        auto L = LuaState::getL();
        lua_getglobal(L,"Isaac");
        lua_getfield(L,-1,"ConsoleOutput");
        lua_pushstring(L, string);
        lua_call(L, 1, 0);
        lua_pop(L,1);
    }
}
