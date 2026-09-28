#include "Common.h"

#define SERVERPORT 9000
#define BUFSIZE    512

int main(int argc, char* argv[])
{
	int retval;	// 함수 반환값 저장용

	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return 1;

	SOCKET listen_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_sock == INVALID_SOCKET) err_quit("socket()");

	// 서버 주소 구조체 초기화
	struct sockaddr_in serveraddr;
	memset(&serveraddr, 0, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons(SERVERPORT);
	retval = bind(listen_sock, (struct sockaddr*)&serveraddr, sizeof(serveraddr));
	if (retval == SOCKET_ERROR) err_quit("bind()");

	// 연결 요청 대기 상태로 전환
	retval = listen(listen_sock, SOMAXCONN);
	if (retval == SOCKET_ERROR) err_quit("listen()");

	SOCKET client_sock;
	struct sockaddr_in clientaddr;
	int addrlen;
	int len; // 파일 이름 길이
	char buf[BUFSIZE + 1]; // 파일 이름 및 파일 데이터 수신용 버퍼
	long origin_filesize;	// 받을 파일의 크기

	printf("파일 수신 대기 중...\n");

	while (1) {
		addrlen = sizeof(clientaddr);	// 클라이언트 주소 구조체 크기
		client_sock = accept(listen_sock, (struct sockaddr*)&clientaddr, &addrlen);	// 클라이언트 연결 수락
		if (client_sock == INVALID_SOCKET) {
			err_display("accept()");
			break;
		}

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
			if(origin_filesize <=0) {	// 만약 파일 크기가 0보다 작거나 같으면 에러 처리
				err_display("recv()");
				break;
			}
			else if(retval == 0)
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

				printf("\r수신률: %.2f%%", percent);	// 소수점 두 자리까지 출력하기
				

				Sleep(10);	// 수신률 캡쳐용 대기
			}

			fclose(fp);

			printf("\n");

			if (succes && total == origin_filesize)	// succes && 만약 받은 데이터의 총 바이트 수가 원래 파일 크기와 같으면 성공
				printf("파일 수신 완료\n");

			break;
		}
		closesocket(client_sock);
	}
	closesocket(listen_sock);

	WSACleanup();
	return 0;
}