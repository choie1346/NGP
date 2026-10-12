#include "Common.h"

#define SERVERPORT 9000
#define BUFSIZE    512
#define MAXCLIENT 2

CRITICAL_SECTION cs;

// 127.0.0.1

double p[MAXCLIENT]{};
int index = 0;

struct ClientInfo {
	SOCKET sock;
	int number;
};

// 클라이언트와 데이터 통신
DWORD WINAPI ProcessClient(LPVOID arg)
{
	int retval;
	ClientInfo* info = (ClientInfo*)arg;
	SOCKET client_sock = info->sock;
	int index = info->number;
	free(info);
	struct sockaddr_in clientaddr;
	char addr[INET_ADDRSTRLEN];
	int addrlen;
	char buf[BUFSIZE + 1]; // 파일 이름 및 파일 데이터 수신용 버퍼
	int len; // 파일 이름 길이
	long origin_filesize;	// 받을 파일의 크기

	// 클라이언트 정보 얻기
	addrlen = sizeof(clientaddr);
	getpeername(client_sock, (struct sockaddr*)&clientaddr, &addrlen);
	inet_ntop(AF_INET, &clientaddr.sin_addr, addr, sizeof(addr));

	int succes = 1;	// 성공 여부 판단용

	// 파일 이름 길이(고정) -> 파일 이름(가변) -> 파일 크기(고정) -> 파일 내용(가변) 순으로 받기(클라이언트와 같은 순서로)
	while (1) {
		// 파일 이름 길이 받기(고정 길이)
		retval = recv(client_sock, (char*)&len, sizeof(int), MSG_WAITALL);
		if (retval == SOCKET_ERROR) {
			err_display("recv()");
			break;
		}
		else if (retval == 0)
			break;

		// 파일 이름 받기(가변 길이)
		retval = recv(client_sock, buf, len, MSG_WAITALL);
		if (retval == SOCKET_ERROR) {
			err_display("recv()");
			break;
		}
		else if (retval == 0)
			break;
		buf[len] = '\0';	// 파일 이름 끝에 널 문자를 추가하여 끝 표시

		// 파일 크기 받기(고정 길이)
		retval = recv(client_sock, (char*)&origin_filesize, sizeof(origin_filesize), MSG_WAITALL);
		if (origin_filesize <= 0) {	// 만약 파일 크기가 0보다 작거나 같으면 에러 처리
			err_display("recv()");
			break;
		}
		else if (retval == 0)
			break;

		FILE* fp = fopen(buf, "wb");	// 파일 바이너리 쓰기 모드로 열기
		if (!fp) {
			printf("파일 열기 실패: %s\n", buf);
			break;
		}

		long total = 0;	// 받은 총 바이트 수 (수신률 계산용)

		while (1) {
			retval = recv(client_sock, buf, BUFSIZE, 0);	// 파일 데이터를 buf에 최대 BUFSIZE(512)만큼 받기

			if (retval == 0)	// 클라이언트가 연결을 종료하면 루프 탈출
				break;

			if (retval == SOCKET_ERROR) {
				err_display("recv()");
				succes = 0;
				break;
			}

			if (fwrite(buf, 1, retval, fp) != (size_t)retval) {	// buf에 받은 데이터를 retval만큼 파일에 쓰기
				printf("파일 쓰기 실패\n");
				succes = 0;
				break;
			}

			total += retval;	// 받은 총 바이트 수 누적

			double percent = (double)total / origin_filesize * 100;	// 수신률 계산

			EnterCriticalSection(&cs);
			p[index - 1] = percent;
			printf("\r");
			// 퍼센트 출력
			for (int i{ 0 }; i < MAXCLIENT; ++i) {
				printf("클라%d: %6.2f%%    ", i + 1, p[i]);	// 소수점 두 자리까지 출력하기
			}

			fflush(stdout);
			LeaveCriticalSection(&cs);


			Sleep(10);	// 수신률 캡쳐용 대기
		}

		fclose(fp);

		// printf("\n");

		// if (succes && total == origin_filesize)	// succes && 만약 받은 데이터의 총 바이트 수가 원래 파일 크기와 같으면 성공
			// printf("파일 수신 완료\n");

		break;
	}
	// 소켓 닫기
	closesocket(client_sock);
	// printf("[TCP 서버] 클라이언트 종료: IP 주소=%s, 포트 번호=%d\n",
		// addr, ntohs(clientaddr.sin_port));

	return 0;
}

int main(int argc, char* argv[])
{
	InitializeCriticalSection(&cs);

	int retval;

	// 윈속 초기화
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return 1;

	// 소켓 생성
	SOCKET listen_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_sock == INVALID_SOCKET) err_quit("socket()");

	// bind()
	struct sockaddr_in serveraddr;
	memset(&serveraddr, 0, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons(SERVERPORT);
	retval = bind(listen_sock, (struct sockaddr*)&serveraddr, sizeof(serveraddr));
	if (retval == SOCKET_ERROR) err_quit("bind()");

	// listen()
	retval = listen(listen_sock, SOMAXCONN);
	if (retval == SOCKET_ERROR) err_quit("listen()");

	// 데이터 통신에 사용할 변수
	SOCKET client_sock;
	struct sockaddr_in clientaddr;
	int addrlen;
	HANDLE hThread;

	while (1) {

		addrlen = sizeof(clientaddr);
		client_sock = accept(listen_sock, (struct sockaddr*)&clientaddr, &addrlen);
		if (client_sock == INVALID_SOCKET) {
			err_display("accept()");
			break;
		}
		if (index >= MAXCLIENT) {
			closesocket(client_sock);
			continue;
		}
		++index;

		char addr[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &clientaddr.sin_addr, addr, sizeof(addr));
		// printf("[TCP 서버] 클라이언트 접속: IP 주소=%s, 포트 번호=%d\n",
			// addr, ntohs(clientaddr.sin_port));

		ClientInfo* info = (ClientInfo*)malloc(sizeof(ClientInfo));
		if (info == NULL) {
			closesocket(client_sock);
			--index;
			continue;
		}
		info->sock = client_sock;
		info->number = index;

		hThread = CreateThread(NULL, 0, ProcessClient, info, 0, NULL);
		if (hThread == NULL) {
			closesocket(client_sock);
			free(info);
			--index;
		}
		else {
			CloseHandle(hThread);
		}
	}


	// 소켓 닫기
	closesocket(listen_sock);

	// 윈속 종료
	WSACleanup();
	return 0;
}