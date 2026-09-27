#include "Common.h"

char* SERVERIP = (char*)"127.0.0.1";
#define SERVERPORT 9000
#define BUFSIZE    512

int send_all(SOCKET sock, const char* data, int size)
{
	int total = 0;

	while (total < size) {
		int sent = send(sock, data + total, size - total, 0);
		if(sent == SOCKET_ERROR || sent == 0)
			return 0;

		total += sent;
	}
	return 1;
}

int main(int argc, char* argv[])
{
	int retval;

	// 윈속 초기화
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return 1;

	// 소켓 생성
	SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock == INVALID_SOCKET) err_quit("socket()");

	// connect()
	struct sockaddr_in serveraddr;
	memset(&serveraddr, 0, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	inet_pton(AF_INET, SERVERIP, &serveraddr.sin_addr);
	serveraddr.sin_port = htons(SERVERPORT);
	retval = connect(sock, (struct sockaddr*)&serveraddr, sizeof(serveraddr));
	if (retval == SOCKET_ERROR) err_quit("connect()");

	char buf[BUFSIZE];
	char filename[BUFSIZE];

	strcpy(filename, argv[1]);

	if(filename[0] == '\0') {
		printf("전송할 파일을 입력해주세요.\n");
		closesocket(sock);
		WSACleanup();
		return 1;
	}

	int name_len = (int)strlen(filename);
	
	FILE* fp = fopen(filename, "rb");
	if (!fp) {
		printf("파일 열기 실패: %s\n", filename);
		closesocket(sock);
		WSACleanup();
		return 1;
	}

	int succes = 1;
	__int64 total = 0;

	if(!send_all(sock, (const char*)&name_len, sizeof(name_len)) ||
		!send_all(sock, filename, name_len)) {
		printf("파일명 전송 실패\n");
		succes = 0;
	}

	while (succes) {
		int len = (int)fread(buf, 1, BUFSIZE, fp);
		if (len <= 0) break;
		
		if (!send_all(sock, buf, len)) {
			printf("파일 전송 실패\n");
			succes = 0;
			break;
		}

		total += len;
	}

	fclose(fp);

	if (succes)
		printf("파일 전송 완료: %lld바이트\n", total);

	// 소켓 닫기
	closesocket(sock);

	// 윈속 종료
	WSACleanup();
	return 0;
}