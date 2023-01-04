#include "pch.h"
#include "danmu.h"
#include "malloc.h"
#include "stdio.h"
#include <Windows.h>
#include <wininet.h>
#include <json/json.h>
#include <string>
#include "winsock.h"
#include <zlib.h>
#include <brotli/decode.h>

#pragma comment(lib, "Ws2_32.lib")

DWORD WINAPI DanmuReceiveThread(LPVOID lpParam);

HANDLE DanmuThread;

HINTERNET internet;

volatile int roomId = -1;
volatile BOOL isExit = 0;
volatile BOOL isReading = 1;

volatile unsigned int popularity = 0;

/*

为了进行廉价的线程间无锁通信，我们使用循环队列
该通信方式限定唯一读者、唯一写者，请勿整花活

*/
#define DANMU_POOL_SIZE 0x10000
DanmuItem *danmu_pool[DANMU_POOL_SIZE];
size_t danmu_pool_nextread = 0, danmu_pool_nextwrite = 0;//不求模，允许溢出

/// <summary>
/// 将item指针入队列，限定必须在网络线程中使用此函数
/// </summary>
/// <param name="item">入队列的指针</param>
/// <returns>实际写入数量</returns>
size_t danmu_pool_write(DanmuItem* items, size_t count) {
	MemoryBarrier();//刷新nextread，以最大限度提升写入空间
	size_t r = danmu_pool_nextread, w = danmu_pool_nextwrite;

	size_t max_item_count = w - r;//队列的长度，只会比它少
	size_t min_empty_count = DANMU_POOL_SIZE - max_item_count;
	size_t write_count = min(min_empty_count, count);
	for (auto i = 0; i < write_count; i++) {
		danmu_pool[(w + i) % DANMU_POOL_SIZE] = &items[i];
	}
	MemoryBarrier();//避免乱序造成的写入错误
	danmu_pool_nextwrite = w + write_count;
	return write_count;
}


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
	internet = InternetOpen(L"isaac danmaku plugin/1.0", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, NULL);
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData)) {
		TIP(L"socket功能未能正确初始化，本mod无法使用，请确认系统环境正常");
	}
}

void danmu_cleanup() {
	if (!DanmuThread)
		return;

	isExit = 1;
	MemoryBarrier();

	if (WAIT_TIMEOUT == WaitForSingleObject(DanmuThread, 10000)) {
		TIP(L"由于某些原因，弹幕线程没有被正确关闭，相关资源未能正确回收，请手动重启游戏以正确加载mod。");
		//return;
	}

	CloseHandle(DanmuThread);
	DanmuThread = NULL;

	{//清理线程队列
		MemoryBarrier();
		size_t r = danmu_pool_nextread, w = danmu_pool_nextwrite;
		while (r != w) {
			auto target = danmu_pool[r % DANMU_POOL_SIZE];
			free(target);
			r++;
		}
		danmu_pool_nextread = r;
	}

	if (internet) {
		InternetCloseHandle(internet);
	}
	WSACleanup();
}

void danmu_set_reading_enabled(BOOL enabled) {
	isReading = enabled;
	MemoryBarrier();
}
BOOL danmu_get_reading_enabled() {
	return isReading;
}

void danmu_set_roomid(int pRoomId) {
	roomId = pRoomId;
}



#define MAX_PACKAGE_SIZE 1024*1024*10

char buffer[MAX_PACKAGE_SIZE];

BOOL AccessPage(HINTERNET internet, LPCWSTR url, char* buffer, DWORD bufferLen, DWORD* oBuffer) {
	HINTERNET hurl = InternetOpenUrl(internet, url, NULL, -1, INTERNET_FLAG_NO_UI, NULL);

	if (hurl) {
		if (InternetReadFile(hurl, buffer, bufferLen, oBuffer)) {
			InternetCloseHandle(hurl);
			return true;
		}
		InternetCloseHandle(hurl);
	}
	return false;
}

BOOL GetBiliToken(HINTERNET internet, int room_id, std::string& token, std::string& host, int& port, int host_hint = 0) {
	WCHAR url[4096];
	swprintf_s(url, 4096, L"https://api.live.bilibili.com/xlive/web-room/v1/index/getDanmuInfo?id=%d", room_id);

	DWORD len;
	if (!AccessPage(internet, url, buffer, sizeof(buffer), &len))
		return FALSE;

	Json::Value v;
	if (!Json::Reader().parse(buffer, buffer + len, v))
		return FALSE;

	Json::Value& data = v["data"];
	if (!data.isObject())
		return FALSE;

	Json::Value& jtoken = data["token"];
	if (!jtoken.isString())
		return FALSE;

	auto& hlist = data["host_list"];
	if (!hlist.isArray())
		return FALSE;

	auto& ihost = hlist[host_hint % hlist.size()];

	if (!ihost.isObject())
		return FALSE;

	auto hstr = ihost["host"];
	if (!hstr.isString())
		return FALSE;

	auto hport = ihost["port"];
	if (!hport.isNumeric())
		return FALSE;

	token = jtoken.asCString();
	host = hstr.asCString();
	port = hport.asInt();

	return TRUE;
}

struct MQHeader {
	uint32_t size;
	uint16_t header_size;
	uint16_t version;
	uint32_t op;
	uint32_t sequence;
	char buffer[0];
};

#define OP_HEARTBEAT 2
#define OP_HEATBEAT_REPLY 3
#define OP_CMD 5
#define OP_AUTH 7
#define OP_AUTH_REPLY 8

BOOL SendAuthPackage(SOCKET sock, int room_id, std::string token) {
	MQHeader* package = (MQHeader*)buffer;

	sprintf_s(package->buffer, sizeof(buffer) - sizeof(MQHeader), "{\"uid\":0,\"protover\":3,\"platform\":\"web\",\"type\":2,\"roomid\":%d,\"key\":\"%s\"}", room_id, token.c_str());
	uint32_t willsend = sizeof(MQHeader) + strlen(package->buffer);

	package->size = htonl(willsend);
	package->header_size = htons(sizeof(MQHeader));
	package->version = htons(1);
	package->op = htonl(OP_AUTH);
	package->sequence = htonl(1);

	int sent = 0;
	while (sent < willsend) {
		int r = send(sock, (const char*)package, willsend, 0);
		if (r == SOCKET_ERROR)
			return FALSE;
		sent += r;
	}
	return TRUE;
}

BOOL SendHeartBeat(SOCKET sock, const char* msg) {
	MQHeader* package = (MQHeader*)buffer;

	sprintf_s(package->buffer, sizeof(buffer) - sizeof(MQHeader), "%s", msg);
	uint32_t willsend = sizeof(MQHeader) + strlen(package->buffer);

	package->size = htonl(willsend);
	package->header_size = htons(sizeof(MQHeader));
	package->version = htons(1);
	package->op = htonl(OP_HEARTBEAT);
	package->sequence = htonl(0);

	int sent = 0;
	while (sent < willsend) {
		int r = send(sock, (const char*)package, willsend, 0);
		if (r == SOCKET_ERROR)
			return FALSE;
		sent += r;
	}
	return TRUE;
}

BOOL ReceivePackage(SOCKET sock, int& received_size, bool& is_timeout, int& err_code) {
	MQHeader* package = (MQHeader*)buffer;
	is_timeout = false;
	while (true) {
		int next_receive_size;
		if (received_size < sizeof(MQHeader)) {
			next_receive_size = sizeof(MQHeader) - received_size;
		}
		else {
			next_receive_size = ntohl(package->size) - received_size;
		}

		if (received_size >= MAX_PACKAGE_SIZE || next_receive_size >= MAX_PACKAGE_SIZE || received_size + next_receive_size >= MAX_PACKAGE_SIZE)
			return FALSE;

		int r = recv(sock, buffer + received_size, next_receive_size, 0);
		if (r == -1) {
			err_code = GetLastError();
			if (err_code == WSAETIMEDOUT)
				is_timeout = true;
			return FALSE;
		}
		received_size += r;
		if (received_size >= sizeof(MQHeader) && received_size >= ntohl(package->size)) {
			received_size = 0;
			return TRUE;
		}
	}
}

void DanmuSendSysInfo(const char* str) {
	unsigned int len = strlen(str);
	DanmuItem* item = (DanmuItem*)malloc(sizeof(DanmuItem) + len + 1);
	if (item) {
		item->type = DANMUITEM_TYPE_SYSTEM;
		strcpy_s(item->text, len + 1, str);
		danmu_pool_write(item, 1);
	}
}

unsigned char zlib_inflate_buffer[1024 * 1024 * 10];//我们预留10MB的内存给zlib解压（不会有单条10MB的弹幕数据吧）

DWORD WINAPI DanmuReceiveThread(LPVOID lpParam) {

	int myRoomId = -1;

	int random_danmu_id = 0;

	std::string token, host;
	int port;


	SOCKET sock = NULL;

	int received_package = 0;
	bool is_timeout;
	time_t last_heartbeat_send_time;
	int errcode;


	while (1) {
		MemoryBarrier();

		if (isExit) {
			break;
		}

		if (myRoomId != roomId && roomId > 0) {
			//switch room id
			if (sock) {
				closesocket(sock);
				sock = NULL;
			}
			myRoomId = roomId;

			if (!GetBiliToken(internet, myRoomId, token, host, port)) {
				// 获取tcp链接地址失败
				Sleep(2000);
				continue;
			}

			last_heartbeat_send_time = time(NULL) - 1000;

			struct sockaddr_in danmuServer;
			danmuServer.sin_family = AF_INET;
			danmuServer.sin_addr.s_addr = inet_addr(inet_ntoa(*(struct in_addr*)*(gethostbyname(host.c_str()))->h_addr_list));
			danmuServer.sin_port = htons(port);

			sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
			DWORD timeout = 5 * 1000;
			setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
			connect(sock, (SOCKADDR*)&danmuServer, sizeof(danmuServer));
			received_package = 0;

			if (!SendAuthPackage(sock, myRoomId, token)) {
				//bilibili服务器授权失败
				closesocket(sock);
				sock = NULL;
				myRoomId = 0;
				Sleep(2000);
				continue;
			}
			continue;
		}

		if (sock) {
			time_t tick = time(NULL);
			if (tick - last_heartbeat_send_time > 20) {
				if (!SendHeartBeat(sock, "")) {
					closesocket(sock);
					sock = NULL;
					myRoomId = 0;
					Sleep(2000);
					continue;
				}
				last_heartbeat_send_time = tick;
			}
			if (ReceivePackage(sock, received_package, is_timeout, errcode)) {
				MQHeader* package = (MQHeader*)buffer;
				//std::cout << std::string(package->buffer, ntohl(package->size) - sizeof(MQHeader)) << std::endl;
				switch (ntohl(package->op))
				{
				case OP_AUTH_REPLY:
					break;
				case OP_HEATBEAT_REPLY:
					popularity = ntohl(*(uint32_t*)package->buffer);
					MemoryBarrier();
					break;
				case OP_CMD:
					if (isReading) {
						uint32_t msglen = ntohl(package->size) - ntohs(package->header_size);
						if (msglen < sizeof(buffer)) {
							int version = ntohs(package->version);
							if (version == 2) {
								z_stream strm;
								strm.zalloc = Z_NULL;
								strm.zfree = Z_NULL;
								strm.opaque = Z_NULL;
								strm.avail_in = 0;
								strm.next_in = Z_NULL;
								int ret = inflateInit(&strm);

								strm.avail_in = msglen;
								strm.next_in = (unsigned char *)package->buffer;

								strm.avail_out = sizeof(zlib_inflate_buffer);
								strm.next_out = (unsigned char*)zlib_inflate_buffer;

								if (inflate(&strm, Z_FINISH) == Z_STREAM_END) {
									int msglen = sizeof(zlib_inflate_buffer) - strm.avail_out;
									DanmuItem* item = (DanmuItem*)malloc(sizeof(DanmuItem) + msglen + 1);
									if (item) {
										item->type = DANMUITEM_TYPE_MSG;
										memcpy_s(item->text, msglen + 1, zlib_inflate_buffer, msglen);
										item->text[msglen] = '\0';
										danmu_pool_write(item, 1);
									}
								}
							}
							else if (version == 3) {
								unsigned int buf_size = sizeof(zlib_inflate_buffer);
								if (BrotliDecoderDecompress(
									msglen, (unsigned char*)package->buffer, &buf_size, (unsigned char*)zlib_inflate_buffer)
									 == BROTLI_DECODER_SUCCESS) {

									unsigned int i = 0;
									while (true) {
										MQHeader* subpackage = (MQHeader*)&zlib_inflate_buffer[i];

										if (
											i + 0x10 < buf_size &&
											ntohl(subpackage->size) > 0x10 &&
											i + ntohl(subpackage->size) <= buf_size &&
											ntohl(subpackage->op) == OP_CMD && ntohs(subpackage->version) == 0
											) {
											int msglen = ntohl(subpackage->size) - sizeof(MQHeader);
											DanmuItem* item = (DanmuItem*)malloc(sizeof(DanmuItem) + msglen + 1);
											if (item) {
												item->type = DANMUITEM_TYPE_MSG;
												memcpy_s(item->text, msglen + 1, subpackage->buffer, msglen);
												item->text[msglen] = '\0';
												danmu_pool_write(item, 1);
											}
											i += ntohl(subpackage->size);
										}
										else {
											break;
										}

									}


								}
							
							}else {
								DanmuItem* item = (DanmuItem*)malloc(sizeof(DanmuItem) + msglen + 1);
								if (item) {
									item->type = DANMUITEM_TYPE_MSG;
									memcpy_s(item->text, msglen + 1, package->buffer, msglen);
									item->text[msglen] = '\0';
									danmu_pool_write(item, 1);
								}

							}
						}
					}
					break;
				default:
					break;
				}
			}
			else {
				if (!is_timeout) {
					closesocket(sock);
					sock = NULL;
					myRoomId = 0;
				}
			}
		}
	}
	//clean up

	return 0;
}

void danmu_get_buffered(DanmuItemHandlerType handler){	
	MemoryBarrier();
	size_t r = danmu_pool_nextread, w = danmu_pool_nextwrite;
	while (r != w) {
		auto target = danmu_pool[r % DANMU_POOL_SIZE];
		handler(target);
		free(target);
		r++;
	}
	danmu_pool_nextread = r;
}

unsigned int danmu_get_popularity() {
	return popularity;
}