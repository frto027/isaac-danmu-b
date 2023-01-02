
#ifndef ILUA_H
#define ILUA_H
/*

此文件包含与以撒交互的api接口
此文件将lua虚拟机接口进行二次封装，如果不需要，请直接编写Lua库即可
*/

// #define EXPORT_LUA extern "C"

extern "C"{
    //注意，src是来自lua5.3.3的源码，但c文件没有参与编译，而是与Lua5.3.3r.dll（转换而来的lib文件）进行动态链接
    #include "lua-src/lua.h"
}

//由于全局只有一个Lua虚拟机，我们通过LuaState传递lua_State参数，避免繁琐的参数传递
class LuaState{
private:
    lua_State *L, *PL;
    static lua_State * GL;
public:
    LuaState(lua_State *L);
    ~LuaState();
    static lua_State * getL();
};

/*
由于lua通过栈来传递参数，我们将c函数返回逻辑进行适当封装，方便使用
使用LuaRet函数将返回值传递给lua，使用例：
return LuaRet();
return LuaRet(1234);
return LuaRet(1.0);
return LuaRet(true);
return LuaRet("xxxx");
return LuaRet(1234,1.0,true,"xxxx");
*/
inline void lua_return(const char*t){
    lua_pushstring(LuaState::getL(),t);
}
inline void lua_return(double t){
    lua_pushnumber(LuaState::getL(),t);
}
inline void lua_return(int t){
    lua_return((double)t);
}
inline void lua_return(bool t){
    lua_pushboolean(LuaState::getL(),t);
}

template<typename... Types>
inline int LuaRet(Types... args);
template<typename T, typename... Types>
inline int LuaRet(T arg, Types... args){
    lua_return(arg);
    return LuaRet(args...) + 1;
}
template<>inline int LuaRet(){return 0;}

#define EXTERN_FUNC(name) extern "C" int name(lua_State *L)
#define EXTERN_FUNC_INIT LuaState luastate(L);

//游戏lua接口
namespace Isaac{
    void DebugString(const char *string);
    void ConsoleOutput(const char *string);
}

#endif