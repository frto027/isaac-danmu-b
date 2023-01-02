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
		TIP(L"��Ļ�̴߳���ʧ�ܣ�");
	}
}

void danmu_cleanup() {
	if (!DanmuThread)
		return;

	if (WAIT_TIMEOUT == WaitForSingleObject(DanmuThread, 10000)) {
		TIP(L"����ĳЩԭ�򣬵�Ļ�߳�û�б���ȷ�رգ������Դδ����ȷ���գ����ֶ�������Ϸ����ȷ����mod��");
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