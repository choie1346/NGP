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

// 4바이트 정수를 네트워크 바이트 순서로 전송
static bool send_uint32(SOCKET sock, uint32_t value)
{
    uint32_t netValue = htonl(value);
    return send_all(sock, &netValue, sizeof(netValue));
}

static bool upload_file(SOCKET sock, const char* filepath)
{
    FILE* fp = fopen(filepath, "rb");

    if (fp == NULL) {
        perror("파일 열기 실패");
        return false;
    }

    // 파일 크기 확인
    if (_fseeki64(fp, 0, SEEK_END) != 0) {
        printf("파일 위치 이동 실패\n");
        fclose(fp);
        return false;
    }

    __int64 size = _ftelli64(fp);

    if (size < 0 || size > 0xFFFFFFFFLL) {
        printf("파일 크기를 확인할 수 없거나 4GiB 이상입니다.\n");
        fclose(fp);
        return false;
    }

    if (_fseeki64(fp, 0, SEEK_SET) != 0) {
        printf("파일 위치 이동 실패\n");
        fclose(fp);
        return false;
    }

    uint32_t fileSize = (uint32_t)size;

    // 전체 경로에서 파일명만 추출
    const char* filename = filepath;

    for (const char* p = filepath; *p != '\0'; ++p) {
        if (*p == '\\' || *p == '/')
            filename = p + 1;
    }

    size_t nameSize = strlen(filename);

    if (nameSize == 0 || nameSize > MAX_FILENAME) {
        printf("파일명 길이가 올바르지 않습니다.\n");
        fclose(fp);
        return false;
    }

    uint32_t nameLen = (uint32_t)nameSize;

    // [파일명 길이][파일명][파일 크기] 전송
    if (!send_uint32(sock, nameLen) ||
        !send_all(sock, filename, (int)nameLen) ||
        !send_uint32(sock, fileSize)) {
        printf("파일 정보 전송 실패\n");
        fclose(fp);
        return false;
    }

    // 서버의 수신 준비 결과 확인
    // 1: 준비 완료, 0: 실패
    unsigned char status = 0;

    if (!recv_all(sock, &status, sizeof(status)) || status != 1) {
        printf("서버가 파일을 받을 준비를 하지 못했습니다.\n");
        fclose(fp);
        return false;
    }

    char buf[BUFSIZE];
    uint32_t remaining = fileSize;
    bool success = true;

    while (remaining > 0) {
        int chunk = remaining > BUFSIZE
            ? BUFSIZE
            : (int)remaining;

        size_t n = fread(buf, 1, chunk, fp);

        if (n != (size_t)chunk) {
            printf("파일 읽기 실패 또는 파일 크기 변경\n");
            success = false;
            break;
        }

        if (!send_all(sock, buf, chunk)) {
            printf("파일 내용 전송 실패\n");
            success = false;
            break;
        }

        remaining -= (uint32_t)chunk;
    }

    fclose(fp);

    if (!success)
        return false;

    // 서버의 최종 저장 결과 확인
    status = 0;

    if (!recv_all(sock, &status, sizeof(status)) || status != 1) {
        printf("서버 저장 실패 또는 연결 종료\n");
        return false;
    }

    printf("파일 전송 완료: %s (%llu바이트)\n",
        filename,
        (unsigned long long)fileSize);

    return true;
}

int main(int argc, char* argv[])
{
    const char* serverIP = "127.0.0.1";

    if (argc > 1)
        serverIP = argv[1];

    char filepath[1024];

    printf("전송할 파일명 또는 경로: ");

    if (fgets(filepath, sizeof(filepath), stdin) == NULL)
        return 1;

    filepath[strcspn(filepath, "\r\n")] = '\0';

    if (filepath[0] == '\0') {
        printf("파일명을 입력해야 합니다.\n");
        return 1;
    }

    // 윈속 초기화
    WSADATA wsa;
    int result = WSAStartup(MAKEWORD(2, 2), &wsa);

    if (result != 0) {
        printf("WSAStartup 실패: %d\n", result);
        return 1;
    }

    // 소켓 생성
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);

    if (sock == INVALID_SOCKET) {
        err_display("socket()");
        WSACleanup();
        return 1;
    }

    // 서버 주소 설정
    sockaddr_in serveraddr = {};
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(SERVERPORT);

    if (inet_pton(AF_INET, serverIP, &serveraddr.sin_addr) != 1) {
        printf("잘못된 서버 IPv4 주소입니다.\n");
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    // 서버 접속
    result = connect(
        sock,
        (sockaddr*)&serveraddr,
        sizeof(serveraddr)
    );

    if (result == SOCKET_ERROR) {
        err_display("connect()");
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    bool success = upload_file(sock, filepath);

    closesocket(sock);
    WSACleanup();

    return success ? 0 : 1;
}