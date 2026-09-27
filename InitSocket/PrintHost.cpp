#include "Common.h"

int main(int argc, char * argv[])
{
	hostent* host;
	if (argc != 2) {
		printf("Usage: %s <addr>\n", argv[0]);
		return 1;
	}

	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return 1;

	host = gethostbyname(argv[1]);
	if (host == NULL || host->h_addrtype != AF_INET) {
		err_display("gethostbyname()");
		WSACleanup();
		return 1;
	}

	for (int i = 0; host->h_aliases[i]; ++i)
		printf("도메인 별명: %d - %s\n",i + 1,  host->h_aliases[i]);
	for (int i = 0; host->h_addr_list[i]; ++i)
		printf("IP주소: %d - %s\n", i + 1, inet_ntoa(*(struct in_addr*)host->h_addr_list[i]));
	
	WSACleanup();
	return 0;
}


