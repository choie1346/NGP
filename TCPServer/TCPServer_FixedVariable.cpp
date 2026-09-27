#include "Common.h"
#include <stdint.h>

#define SERVERPORT 9000
#define BUFSIZE 4096
#define MAX_FILENAME 200

// 데이터를 지정한 크기만큼 모두 전송
static bool send_all(SOCKET sock, const void* data, int size)
{
    const char* p = (const char*)data;
    int total = 0;

    while (total < size) {
        int n = send(sock, p + total, size - total, 0);

        if (n == SOCKET_ERROR || n == 0)
            return false;

        total += n;
    }

    return true;
}

// 데이터를 지정한 크기만큼 모두 수신
static bool recv_all(SOCKET sock, void* data, int size)
{
    char* p = (char*)data;
    int total = 0;

    while (total < size) {
        int n = recv(sock, p + total, size - total, 0);

        if (n == SOCKET_ERROR || n == 0)
            return false;

        total += n;
    }

    return true;
}

// 네트워크 바이트 순서의 4바이트 정수를 수신
static bool recv_uint32(SOCKET sock, uint32_t& value)
{
    uint32_t netValue;

    if (!recv_all(sock, &netValue, sizeof(netValue)))
        return false;

    value = ntohl(netValue);
    return true;
}

// 1: 성공, 0: 실패
static bool send_status(SOCKET sock, unsigned char status)
{
    return send_all(sock, &status, sizeof(status));
}

// 경로 구분자와 Windows 파일명 금지 문자 검사
static bool valid_filename(const char* filename, uint32_t len)
{
    if (len == 0 || len > MAX_FILENAME)
        return false;

    if (filename[len - 1] == '.' || filename[len - 1] == ' ')
        return false;

    for (uint32_t i = 0; i < len; ++i) {
        unsigned char ch = (unsigned char)filename[i];

        if (ch < 32 || strchr("\\/:*?\"<>|", ch) != NULL)
            return false;
    }

    return true;
}

static bool receive_file(SOCKET sock)
{
    uint32_t nameLen;
    uint32_t fileSize;
    char filename[MAX_FILENAME + 1];

    // 파일명 길이 수신
    if (!recv_uint32(sock, nameLen))
        return false;

    if (nameLen == 0 || nameLen > MAX_FILENAME) {
        printf("잘못된 파일명 길이\n");
        send_status(sock, 0);
        return false;
    }

    // 파일명 수신
    if (!recv_all(sock, filename, (int)nameLen))
        return false;

    filename[nameLen] = '\0';

    // 파일 크기 수신
    if (!recv_uint32(sock, fileSize))
        return false;

    if (!valid_filename(filename, nameLen)) {
        printf("사용할 수 없는 파일명\n");
        send_status(sock, 0);
        return false;
    }

    // 서버의 현재 작업 폴더에 저장
    char savepath[MAX_FILENAME + 32];

    snprintf(
        savepath,
        sizeof(savepath),
        "%s",
        filename
    );

    FILE* fp = fopen(savepath, "wb");

    if (fp == NULL) {
        perror("저장 파일 생성 실패");
        send_status(sock, 0);
        return false;
    }

    // 클라이언트에 수신 준비 완료 알림
    if (!send_status(sock, 1)) {
        fclose(fp);
        remove(savepath);
        return false;
    }

    printf("파일 수신 시작: %s (%llu바이트)\n",
        filename,
        (unsigned long long)fileSize);

    char buf[BUFSIZE];
    uint32_t remaining = fileSize;
    bool success = true;

    while (remaining > 0) {
        int want = remaining > BUFSIZE
            ? BUFSIZE
            : (int)remaining;

        int received = recv(sock, buf, want, 0);

        if (received == SOCKET_ERROR) {
            err_display("recv()");
            success = false;
            break;
        }

        if (received == 0) {
            printf("파일 수신 도중 연결 종료\n");
            success = false;
            break;
        }

        size_t written = fwrite(buf, 1, received, fp);

        if (written != (size_t)received) {
            printf("파일 쓰기 실패\n");
            success = false;
            break;
        }

        remaining -= (uint32_t)received;
    }

    // 실제 파일 저장 완료 여부 확인
    if (fclose(fp) != 0)
        success = false;

    if (!success) {
        remove(savepath);
        send_status(sock, 0);
        return false;
    }

    printf("파일 저장 완료: %s\n", savepath);

    if (!send_status(sock, 1)) {
        printf("저장 완료 응답 전송 실패\n");
        return false;
    }

    return true;
}

int main(int argc, char* argv[])
{
    // 윈속 초기화
    WSADATA wsa;
    int result = WSAStartup(MAKEWORD(2, 2), &wsa);

    if (result != 0) {
        printf("WSAStartup 실패: %d\n", result);
        return 1;
    }

    // 연결 수신용 소켓 생성
    SOCKET listen_sock = socket(AF_INET, SOCK_STREAM, 0);

    if (listen_sock == INVALID_SOCKET) {
        err_display("socket()");
        WSACleanup();
        return 1;
    }

    // 서버 주소 설정
    sockaddr_in serveraddr = {};
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons(SERVERPORT);

    // bind()
    result = bind(
        listen_sock,
        (sockaddr*)&serveraddr,
        sizeof(serveraddr)
    );

    if (result == SOCKET_ERROR) {
        err_display("bind()");
        closesocket(listen_sock);
        WSACleanup();
        return 1;
    }

    // listen()
    result = listen(listen_sock, SOMAXCONN);

    if (result == SOCKET_ERROR) {
        err_display("listen()");
        closesocket(listen_sock);
        WSACleanup();
        return 1;
    }

    printf("파일 수신 서버 시작: 포트 %d\n", SERVERPORT);

    while (true) {
        sockaddr_in clientaddr = {};
        int addrlen = sizeof(clientaddr);

        // accept()
        SOCKET client_sock = accept(
            listen_sock,
            (sockaddr*)&clientaddr,
            &addrlen
        );

        if (client_sock == INVALID_SOCKET) {
            err_display("accept()");
            break;
        }

        char addr[INET_ADDRSTRLEN] = {};

        inet_ntop(
            AF_INET,
            &clientaddr.sin_addr,
            addr,
            sizeof(addr)
        );

        printf("\n클라이언트 접속: %s:%d\n",
            addr,
            ntohs(clientaddr.sin_port));

        // 연결 한 번에 파일 하나 수신
        if (!receive_file(client_sock))
            printf("파일 수신 처리 실패\n");

        closesocket(client_sock);

        printf("클라이언트 연결 종료\n");
    }

    closesocket(listen_sock);
    WSACleanup();

    return 0;
}