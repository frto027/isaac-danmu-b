// bili-danmu-test.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>
#include <Windows.h>
#include <wininet.h>
#include <json/json.h>
#include <string>

#pragma comment(lib, "Ws2_32.lib")

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

BOOL GetBiliToken(HINTERNET internet, int room_id, std::string &token, std::string &host, int &port, int host_hint = 0) {
	WCHAR url[4096];
	swprintf_s(url,4096, L"https://api.live.bilibili.com/xlive/web-room/v1/index/getDanmuInfo?id=%d", room_id);

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

	sprintf_s(package->buffer, sizeof(buffer) - sizeof(MQHeader), "{\"uid\":0,\"protover\":3,\"platform\":\"web\",\"type\":2,\"roomid\":%d,\"key\":\"%s\"}", room_id,token.c_str());
	std::cout << package->buffer << std::endl;
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
}

BOOL ReceivePackage(SOCKET sock, int &received_size, bool &is_timeout, int &err_code) {
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

int main()
{
	HINTERNET internet = InternetOpen(L"isaac danmaku plugin/1.0", INTERNET_OPEN_TYPE_PRECONFIG,NULL, NULL, NULL);
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData)) {
		return 1;
	}

	std::string token, host;
	int port;

	int room_id = 12265;

	std::cout<< GetBiliToken(internet, room_id,token,host,port) << std::endl;
	//std::cout << token << ":" << host << ":" << port << std::endl;

	struct sockaddr_in danmuServer;
	danmuServer.sin_family = AF_INET;
	danmuServer.sin_addr.s_addr = inet_addr(inet_ntoa(*(struct in_addr*)*(gethostbyname(host.c_str()))->h_addr_list));
	danmuServer.sin_port = htons(port);

	SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	DWORD timeout = 5 * 1000;
	setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO,(char *) & timeout, sizeof(timeout));
	connect(sock, (SOCKADDR*)&danmuServer, sizeof(danmuServer));

	SendAuthPackage(sock, room_id, token);

	int received_package = 0;
	bool is_timeout;
	time_t last_heartbeat_send_time = time(NULL);
	int errcode;

	while (true) {
		if (ReceivePackage(sock, received_package, is_timeout,errcode)) {
			MQHeader* package = (MQHeader*)buffer;
			//std::cout << std::string(package->buffer, ntohl(package->size) - sizeof(MQHeader)) << std::endl;
			switch (ntohl(package->op))
			{
			case OP_AUTH_REPLY:
				break;
			case OP_HEATBEAT_REPLY:
				std::cout << "doki-doki " << ntohl(*(uint32_t*)package->buffer) << std::endl;
				break;
				
			default:
				std::cout << "unhandled:" << ntohl(package->op) << std::endl;
				break;
			}
		}
		else {
			if (!is_timeout) {
				break;
			}
			std::cout << "timeout report:" << is_timeout << std::endl;
		}
		time_t tick = time(NULL);
		if (tick - last_heartbeat_send_time > 20) {
			SendHeartBeat(sock, "");
			last_heartbeat_send_time = tick;
			std::cout << "heart beat!" << std::endl;
		}
	}

	InternetCloseHandle(internet);
	WSACleanup();
}

// 运行程序: Ctrl + F5 或调试 >“开始执行(不调试)”菜单
// 调试程序: F5 或调试 >“开始调试”菜单

// 入门使用技巧: 
//   1. 使用解决方案资源管理器窗口添加/管理文件
//   2. 使用团队资源管理器窗口连接到源代码管理
//   3. 使用输出窗口查看生成输出和其他消息
//   4. 使用错误列表窗口查看错误
//   5. 转到“项目”>“添加新项”以创建新的代码文件，或转到“项目”>“添加现有项”以将现有代码文件添加到项目
//   6. 将来，若要再次打开此项目，请转到“文件”>“打开”>“项目”并选择 .sln 文件
