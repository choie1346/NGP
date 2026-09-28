#include "Common.h"

char* SERVERIP = (char*)"127.0.0.1";
#define SERVERPORT 9000
#define BUFSIZE    512

int send_all(SOCKET sock, const char* data, int size)	// 데이터 전송 함수
{
	int total = 0;	// 전송한 데이터의 총 크기

	while (total < size) {	// 전송할 데이터가 남아있으면 계속
		int sent = send(sock, data + total, size - total, 0);	// send() 함수로 데이터 전송
		if(sent == SOCKET_ERROR || sent == 0)	// 전송 실패 시 리턴
			return 0;

		total += sent;	 // 전송한 데이터 크기 누적
	}
	return 1;
}

int main(int argc, char* argv[])
{
	int retval;	// 함수 반환값 저장용 변수

	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return 1;

	SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock == INVALID_SOCKET) err_quit("socket()");

	// 서버 주소 구조체 초기화
	struct sockaddr_in serveraddr;
	memset(&serveraddr, 0, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	inet_pton(AF_INET, SERVERIP, &serveraddr.sin_addr);
	serveraddr.sin_port = htons(SERVERPORT);
	retval = connect(sock, (struct sockaddr*)&serveraddr, sizeof(serveraddr));
	if (retval == SOCKET_ERROR) err_quit("connect()");

	if (argc < 2 || argv[1][0] == '\0') {	// 명령행 인자를 입력하지 않았거나 파일명이 없으면 종료
		printf("전송할 파일을 입력해주세요.\n");
		closesocket(sock);
		WSACleanup();
		return 1;
	}

	char buf[BUFSIZE];	// 전송할 파일의 내용을 담을 버퍼
	const char* filename = argv[1];	// 명령행 인자로 파일명 받기
	
	FILE* fp = fopen(filename, "rb");	// 파일을 바이너리 읽기 모드로 열기
	if (!fp) {
		printf("파일 열기 실패: %s\n", filename);
		closesocket(sock);
		WSACleanup();
		return 1;
	}

	fseek(fp, 0, SEEK_END);	// 파일 포인터를 마지막 위치로 이동
	long filesize = ftell(fp);	// 파일 크기 구하기 -> 파일 포인터 위치를 바이트 단위로 반환
	rewind(fp);	// 파일 포인터를 처음으로 되돌리기

	int succes = 1;	// 성공 여부 판단용

	int name_len = (int)strlen(filename);	// 파일 이름 길이 구하기

	// 파일 이름 길이(고정) -> 파일 이름(가변) -> 파일 크기(고정) -> 파일 내용(가변) 순으로 전송(서버와 같은 순서로)
	if(!send_all(sock, (const char*)&name_len, sizeof(name_len)) ||	// 소켓에 파일 이름 길이 전송
		!send_all(sock, filename, name_len)) {		// 소켓에 파일 이름 전송
		printf("파일명 전송 실패\n");
		succes = 0;
	}
	if (!send_all(sock, (const char*)&filesize, sizeof(filesize))) {	// 소켓에 파일 크기 전송
		printf("파일 크기 전송 실패\n");
		succes = 0;
	}

	while (succes) {
		int len = (int)fread(buf, 1, BUFSIZE, fp);	// 파일 내용을 버퍼에 최대 BUFSIZE만큼 읽기
		if (len == 0) break;	// 더 이상 읽을 내용이 없으면 끝내기
		
		if (!send_all(sock, buf, len)) {	// 파일 내용 전송
			printf("파일 전송 실패\n");
			succes = 0;
			break;
		}
	}

	fclose(fp);

	if (succes)
		printf("파일 전송 완료: %lld바이트\n", filesize);	// 파일 전송 완료 메세지 출력

	closesocket(sock);

	WSACleanup();
	return 0;
}