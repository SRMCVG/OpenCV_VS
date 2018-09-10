#include "stdafx.h"
#include "Serialport.h"


Serialport::Serialport(int baudRate)
{
	hCom = CreateFile(TEXT("COM2"),//COM1口
		GENERIC_READ | GENERIC_WRITE, //允许读和写
		0, //独占方式
		NULL,
		OPEN_EXISTING, //打开而不是创建
		0, //同步方式 
		NULL);

	if (hCom == (HANDLE)-1)
	{
		MessageBox(NULL, TEXT("打开COM失败"), TEXT("Error"), MB_OK);
		//return -1;
	}

	COMMTIMEOUTS TimeOuts;
	//设定读超时
	TimeOuts.ReadIntervalTimeout = 10;
	TimeOuts.ReadTotalTimeoutMultiplier = 20;
	TimeOuts.ReadTotalTimeoutConstant = 20;
	//设定写超时
	TimeOuts.WriteTotalTimeoutMultiplier = 10;
	TimeOuts.WriteTotalTimeoutConstant = 20;
	// 写入串口超时参数
	if (!SetCommTimeouts(hCom, &TimeOuts))
	{
		MessageBox(NULL, TEXT("写入超时参数错误"), TEXT("Error"), MB_OK);
		//return -1;
	}
	//
	// 设置输入输出缓冲区参数，返回非0表示成功 
	if (!SetupComm(hCom, 1024, 1024))
	{
		MessageBox(NULL, TEXT("设置串口读写缓冲区失败"), TEXT("Error"), MB_OK);
		//return -1;
	}

	DCB dcb;
	// 获取当前串口状态信息(只需要修改部分串口信息),调用失败返回0
	if (!GetCommState(hCom, &dcb))
	{
		MessageBox(NULL, TEXT("获取串口属性失败"), TEXT("Error"), MB_OK);
		//return -1;
	}
	dcb.BaudRate = baudRate; //波特率
	dcb.ByteSize = 8; //每个字节有8位
	dcb.Parity = NOPARITY; //无奇偶校验位
	dcb.StopBits = ONESTOPBIT; //一个停止位
	if (!SetCommState(hCom, &dcb))
	{
		MessageBox(NULL, TEXT("设置串口参数出错"), TEXT("Error"), MB_OK);
		//return -1;
	}
}


int Serialport::sendData(double angle_x,double angle_y)
{
	char outputData[64];

	int x = (int)angle_x * 10;
	int y = (int)angle_y * 10;
	if (x < 0)
	{
		x = abs(x);
		outputData[0] = '-';
	}
	else
		outputData[0] = '+';
	if (x / 100)
		outputData[1] = '0' + x / 100;
	else
		outputData[1] = '0';
	x = x % 100;
	if (x / 10)
		outputData[2] = '0' + x / 10;
	else
		outputData[2] = '0';
	x = x % 10;
	if (x)
		outputData[3] = '0' + x;
	else
		outputData[3] = '0';

	if (y < 0)
	{
		y = abs(y);
		outputData[4] = '-';
	}
	else
		outputData[4] = '+';
	if (y / 100)
		outputData[5] = '0' + y / 100;
	else
		outputData[5] = '0';
	y = y % 100;
	if (y / 10)
		outputData[6] = '0' + y / 10;
	else
		outputData[6] = '0';
	y = y % 10;
	if (y)
		outputData[7] = '0' + y;
	else
		outputData[7] = '0';


	if (WriteFile(hCom, outputData, 8, &wCount, NULL) == 0)
	{
		MessageBoxA(NULL, "写入串口数据失败", MB_OK, 0);
		return FAIL;
	}
	return OK;
}


Serialport::~Serialport()
{
}

/*
x = 50;
y = 50;
outputData[0] = 0;
if (x < 0)
{
x = abs(x);
outputData[1] = '-';
}
else
outputData[1] = '+';

outputData[2] = 1;
if (x / 100)
outputData[3] = '0' + x / 100;
else
outputData[3] = '0';

outputData[4] = 2;
x = x % 100;
if (x / 10)
outputData[5] = '0' + x / 10;
else
outputData[5] = '0';

outputData[6] = 3;
x = x % 10;
if (x)
outputData[7] = '0' + x;
else
outputData[7] = '0';

outputData[8] = 4;

if (y < 0)
{
y = abs(y);
outputData[9] = '-';
}
else
outputData[9] = '+';

outputData[10] = 5;
if (y / 100)
outputData[11] = '0' + y / 100;
else
outputData[11] = '0';

outputData[12] = 6;
y = y % 100;
if (y / 10)
outputData[13] = '0' + y / 10;
else
outputData[13] = '0';

outputData[14] = 7;
y = y % 10;
if (y)
outputData[15] = '0' + y;
else
outputData[15] = '0';

WriteFile(hCom, outputData, 16, &wCount, NULL);*/
