#include "Common.h"

int main(int argc, char *argv[])
{
	// 윈속 초기화
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(1, 1), &wsa) != 0)	// 1.1version
		return 1;
	printf("[알림] 윈속 초기화 성공\n");

	printf("wVersion       : %d.%d\n", LOBYTE(wsa.wVersion), HIBYTE(wsa.wVersion));			// 주 버전, 부 버전
	printf("wHighVersion   : %d.%d\n", LOBYTE(wsa.wHighVersion), HIBYTE(wsa.wHighVersion));	// 하이 주 버전, 하이 부 버전
	printf("szDescription  : %s\n", wsa.szDescription);	// 윈속 설명
	printf("szSystemStatus : %s\n", wsa.szSystemStatus);	// 윈속 상태

	// 소켓 생성
	SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);	// IPv4, 연결형 프로토콜 타입, TCP
	if (sock == INVALID_SOCKET) err_quit("socket()");	// 소켓 생성 실패
	printf("[알림] 소켓 생성 성공\n");

	// 소켓 닫기
	closesocket(sock);

	// 윈속 종료
	WSACleanup();
	return 0;
}
