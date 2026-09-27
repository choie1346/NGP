#include "Common.h"
#include <iostream>

bool IsLittleEndian()
{
	u_short s = 0x1234;
	return s != htons(s);
}

bool IsBigEndian()
{
	u_short s = 0x1234;
	return s == htons(s);
}

int main(int argc, char* argv[])
{
	std::cout << "==============호스트의 바이트 순서를 검사================" << std::endl;

	if (IsLittleEndian())
		std::cout << "호스트의 바이트 순서는 리틀 엔디안입니다." << std::endl;
	else if(IsBigEndian())
		std::cout << "호스트의 바이트 순서는 빅 엔디안입니다." << std::endl;
}