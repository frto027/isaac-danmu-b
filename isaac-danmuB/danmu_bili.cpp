#include "pch.h"
#include "danmu.h"

DWORD WINAPI DanmuReceiveThread(LPVOID lpParam);

HANDLE DanmuThread;

void danmu_init() {
	assert(!DanmuThread);
	DanmuThread = CreateThread(
		NULL,
		0,
		DanmuReceiveThread,
		NULL,
		0,
		NULL
	);
	if (!DanmuThread) {
		TIP(L"弹幕线程创建失败！");
	}
}

void danmu_cleanup() {
	if (!DanmuThread)
		return;

	if (WAIT_TIMEOUT == WaitForSingleObject(DanmuThread, 10000)) {
		TIP(L"由于某些原因，弹幕线程没有被正确关闭，相关资源未能正确回收，请手动重启游戏以正确加载mod。");
		return;
	}

	CloseHandle(DanmuThread);
	DanmuThread = NULL;
}

DWORD WINAPI DanmuReceiveThread(LPVOID lpParam) {
	while (0) {
		
	}
	return 0;
}