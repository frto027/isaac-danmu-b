// dllmain.cpp : 定义 DLL 应用程序的入口点。
#include "pch.h"

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
        //我们可以在这里做初始化和清理工作，但我们不在这里做，而是在open函数中做。
    case DLL_PROCESS_ATTACH:
        //MessageBox(NULL, L"ATTACH", L"ATTACH", MB_OK);
        break;
    case DLL_PROCESS_DETACH:
        //MessageBox(NULL, L"DETATCH", L"DETATCH", MB_OK);
        break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
        break;
    }
    return TRUE;
}

