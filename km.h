#pragma once
#include <iostream>
#include <windows.h>  
using namespace std;


class serail
{
public:
	//���ò���
	bool setconfig(int baudrate);

	//��
	bool open(LPCSTR COMx, int baudrate);

	//�ر�
	bool close();

	//����
	bool send(char* str);
	bool send(const char* str);

	//��ȡ
	string read();

private:

}ser;

