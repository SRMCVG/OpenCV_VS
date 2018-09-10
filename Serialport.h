#pragma once
#include <windows.h>

#define OK 1
#define FAIL 0

class Serialport
{
public:
	Serialport(int baudRate);
	~Serialport();
	int sendData(double angle_x, double angle);
private:
	HANDLE hCom;//串口
	DWORD wCount;//发送的字节数	
};

