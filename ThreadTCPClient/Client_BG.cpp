#define _CRT_SECURE_NO_WARNINGS
#include "common.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define SERVERPORT 9000
#define BUFSIZE    256
#define NAMESIZE   256

// 지정한 길이만큼 모두 전송: 성공 1, 실패 0
int send_all(SOCKET sock, const char* buf, int len)
{
    int sent = 0;

    while (sent < len) {
        int retval = send(sock, buf + sent, len - sent, 0);

        if (retval == SOCKET_ERROR) {
            err_display("send()");
            return 0;
        }

        if (retval == 0)
            return 0;

        sent += retval;
    }

    return 1;
}

int main(int argc, char* argv[])
{
    int retval;

    // 실행 파일 이름 + 서버 IP + 파일 이름
    if (argc != 3) {
        printf("사용법: %s 서버IP 파일명\n", argv[0]);
        return 1;
    }

    const char* SERVERIP = argv[1];
    const char* filename = argv[2];

    // 서버의 파일 이름 버퍼에 들어갈 수 있는지 확인
    if (strlen(filename) == 0 || strlen(filename) >= NAMESIZE) {
        printf("파일 이름의 길이가 올바르지 않습니다.\n");
        return 1;
    }

    // 윈속 초기화
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
        return 1;

    // 소켓 생성
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET)
        err_quit("socket()");

    // 서버 주소 설정
    struct sockaddr_in serveraddr;
    memset(&serveraddr, 0, sizeof(serveraddr));

    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(SERVERPORT);

    // 명령행 인수로 받은 서버 IPv4 주소 사용
    if (inet_pton(AF_INET, SERVERIP, &serveraddr.sin_addr) != 1) {
        printf("IPv4 주소가 올바르지 않습니다: %s\n", SERVERIP);
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    // 서버 연결
    retval = connect(
        sock,
        (struct sockaddr*)&serveraddr,
        sizeof(serveraddr)
    );

    if (retval == SOCKET_ERROR)
        err_quit("connect()");

    // 파일 전송에 사용할 변수
    char buf[BUFSIZE];
    FILE* fp = NULL;
    size_t n;
    uint32_t net_len;
    int success = 0;

    printf("서버 IP: %s\n", SERVERIP);
    printf("전송할 파일: %s\n", filename);

    // 바이너리 읽기 모드로 파일 열기
    fp = fopen(filename, "rb");

    if (fp == NULL) {
        printf("파일을 열 수 없습니다.\n");
    }
    else {
        long file_size;

        // 파일 크기 확인 후 처음으로 이동
        if (fseek(fp, 0, SEEK_END) != 0 ||
            (file_size = ftell(fp)) < 0 ||
            fseek(fp, 0, SEEK_SET) != 0) {

            printf("파일 크기를 확인할 수 없습니다.\n");
            fclose(fp);
            closesocket(sock);
            WSACleanup();
            return 1;
        }

        // 파일 이름 길이 전송
        int len = (int)strlen(filename);
        net_len = htonl((uint32_t)len);

        success = send_all(
            sock,
            (const char*)&net_len,
            sizeof(net_len)
        );

        // 파일 이름 전송
        if (success)
            success = send_all(sock, filename, len);

        // 전체 파일 크기 전송
        if (success) {
            net_len = htonl((uint32_t)file_size);

            success = send_all(
                sock,
                (const char*)&net_len,
                sizeof(net_len)
            );
        }

        // 파일 내용을 읽어서 전송
        while (success && (n = fread(buf, 1, sizeof(buf), fp)) > 0) {
            net_len = htonl((uint32_t)n);

            // 이번에 전송할 데이터의 길이 전송
            if (!send_all(
                sock,
                (const char*)&net_len,
                sizeof(net_len))) {

                success = 0;
                break;
            }

            // 파일 데이터 전송
            if (!send_all(sock, buf, (int)n)) {
                success = 0;
                break;
            }
        }

        // 파일 읽기 오류 확인
        if (ferror(fp)) {
            printf("파일 읽기 오류가 발생했습니다.\n");
            success = 0;
        }

        // 길이 0을 보내 전송 종료 알림
        if (success) {
            net_len = htonl(0);

            success = send_all(
                sock,
                (const char*)&net_len,
                sizeof(net_len)
            );
        }

        fclose(fp);

        if (success)
            printf("파일 전송 완료\n");
        else
            printf("파일 전송 실패\n");
    }

    closesocket(sock);
    WSACleanup();

    return success ? 0 : 1;
}