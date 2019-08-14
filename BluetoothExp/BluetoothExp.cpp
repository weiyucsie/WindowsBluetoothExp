// BluetoothExp.cpp : 定義主控台應用程式的進入點。
//

#include "stdafx.h"
#define ESTR(e) #e

static char *errorMsg(int werr) {
	static char mbuf[1024];

	switch (werr)
	{
	case WSAEINVAL:
		return ESTR(WSAEINVAL);
	case WSAEFAULT:
		return ESTR(WSAEFAULT);
	case WSAEADDRNOTAVAIL:
		return ESTR(WSAEADDRNOTAVAIL);
	case WSA_E_NO_MORE:
		return ESTR(WSA_E_NO_MORE);
	case WSA_INVALID_HANDLE:
		return ESTR(WSA_INVALID_HANDLE);
	case WSAENOTSOCK:
		return ESTR(WSAENOTSOCK);
	case WSAEHOSTDOWN:
		return ESTR(WSAEHOSTDOWN);
	default:
		sprintf_s(mbuf, "%d", werr);
		return mbuf;
	}
}

void SetChecksum(char bArr[], int len) {
	int byteSum = 0;

	for (int i = 2; i < len; i++) {
		byteSum += bArr[i];
	}
	bArr[len - 1] = (char)(unsigned char)(byteSum ^ -1);
}

void SetChecksumAndSend(SOCKET sock, char msg[], int len) {
	SetChecksum(msg, len);
	send(sock, msg, len, 0);
}

int RecvTillSize(SOCKET sock, char buffer[], int len) {
	int offset = 0;
	int recvCount;

	while (offset < len && (recvCount = recv(sock, buffer + offset, len - offset, 0)) > 0) {
		offset += recvCount;
	}
	return offset;
}

void ShowBuffer(char buffer[], int len) {
	for (int i = 0; i < len; i++) {
		printf("%d", buffer[i]);
		if (i == len - 1)
		{
			printf("\n");
		}
		else
		{
			printf(" ");
		}
	}
}

bool parseDateTime(char bArr[], int len, char buffer[], int bufLen)
{
	if (!(len == 13 && bArr[2] == 5)) {
		printf("Invalid DateTime byte array\n");
		return false;
	}
	int year = ((unsigned char)bArr[4]) + ((unsigned char)bArr[5]) * 256;
	sprintf_s(buffer, bufLen, "%d-%02d-%02d %02d:%02d:%02d", year, bArr[6], bArr[7], bArr[9], bArr[10], bArr[11]);

	return true;
}

void TurnOnOff(SOCKET sock, bool turnOn) {
	char msg[] = { -1, 85, 18, 1, turnOn ? 1 : 0, 0 };
	char buffer[6];
	SetChecksumAndSend(sock, msg, _countof(msg));
	RecvTillSize(sock, buffer, _countof(buffer));
}

void CheckOnOff(SOCKET sock) {
	char msg[] = { -1, 85, 16, 0, 0 };
	char buffer[6];
	SetChecksumAndSend(sock, msg, _countof(msg));
	int recvCount = RecvTillSize(sock, buffer, _countof(buffer));
	if (recvCount == 6)
	{
		if (buffer[4] != 1)
		{
			printf("Off\n");
		}
		else
		{
			printf("On\n");
		}
	}
	else
	{
		printf("Read Failed!\n");
	}
}

void ShowDateTime(SOCKET sock)
{
	char msg[] = { -1, 85, 4, 0, 0 };
	char buffer[13];
	char strBuffer[1024];
	SetChecksumAndSend(sock, msg, _countof(msg));
	int recvCount = RecvTillSize(sock, buffer, _countof(buffer));
	if (parseDateTime(buffer, recvCount, strBuffer, _countof(strBuffer)))
	{
		printf("日期時間： %s\n", strBuffer);
	}
}

void SetDateTime(SOCKET sock)
{
	time_t now = time(NULL);
	tm utc;
	gmtime_s(&utc, &now);
	int year = utc.tm_year + 1900;
	printf("Year %d %d %d\n", year, (char)(year % 256), (char)(year / 256));
	char msg[] = { -1, 85, 2, 8, (char)(year % 256), (char)(year / 256), (char)(utc.tm_mon + 1), (char)utc.tm_mday, (char)utc.tm_wday, (char)utc.tm_hour, (char)utc.tm_min, (char)utc.tm_sec, 0 };
	char buffer[6];
	SetChecksumAndSend(sock, msg, _countof(msg));
	int recvCount = RecvTillSize(sock, buffer, _countof(buffer));
	if (recvCount == 6 && buffer[4] == 0) {
		printf("設定時間成功\n");
	}
}

void MaybeGetFirmwareVersion(SOCKET sock) {
	char msg[] = { -1, 85, -18, 0, 0 };
	char buffer[7];
	SetChecksumAndSend(sock, msg, _countof(msg));
	int recvCount = RecvTillSize(sock, buffer, _countof(buffer));
	ShowBuffer(buffer, recvCount);
}

int main()
{
	UUID serviceClassId = { 0xd9de1d91, 0xff94, 0x11e0,{ 0xbe, 0x50, 0x08, 0x00, 0x20, 0x0c, 0x9a, 0x99 } };
	WSADATA wsaData;
	int LastError;

	WSAStartup(MAKEWORD(2, 2), &wsaData);
	WSAQUERYSET qset;
	HANDLE lHandle = 0;
	DWORD ctrlFlag = LUP_CONTAINERS | LUP_RETURN_ADDR | LUP_RETURN_NAME;
	ZeroMemory(&qset, sizeof(qset));
	qset.dwSize = sizeof(WSAQUERYSET);
	qset.dwNameSpace = NS_BTH;
	if (WSALookupServiceBegin(&qset, ctrlFlag | LUP_FLUSHCACHE, &lHandle) == S_OK) {
		DWORD resultSize = sizeof(WSAQUERYSET);
		WSAQUERYSET *pqset = (WSAQUERYSET *)calloc(1, sizeof(WSAQUERYSET));
		WSAQUERYSET *psqset = (WSAQUERYSET *)calloc(1, sizeof(WSAQUERYSET));
		CSADDR_INFO *addrInfo = NULL;
		while (true)
		{
			if (WSALookupServiceNext(lHandle, ctrlFlag, &resultSize, pqset) == S_OK)
			{
				//TCHAR Yn;

				//_tprintf(_T("Connect %s? [Y/n] "), pqset->lpszServiceInstanceName);
				//Yn = _gettche();
				//_tprintf(_T("\n"));
				if (lstrcmp(pqset->lpszServiceInstanceName, _T("Bill Master")) == 0 && pqset->dwNumberOfCsAddrs > 0) {
					TCHAR szAddressString[1024];
					DWORD dwAddressStringLength = sizeof(szAddressString) / sizeof(szAddressString[0]);
					WSAAddressToString(pqset->lpcsaBuffer[0].RemoteAddr.lpSockaddr, pqset->lpcsaBuffer[0].RemoteAddr.iSockaddrLength, NULL, szAddressString, &dwAddressStringLength);
					WSAQUERYSET sqset = { 0 };
					HANDLE hLookup = NULL;
					sqset.dwSize = sizeof(WSAQUERYSET);
					sqset.lpServiceClassId = &serviceClassId;
					sqset.dwNameSpace = NS_BTH;
					sqset.dwNumberOfCsAddrs = 0;
					sqset.lpszContext = szAddressString;
					if (WSALookupServiceBegin(&sqset, LUP_RETURN_ADDR, &hLookup) == S_OK) {
						DWORD lsqset = sizeof(WSAQUERYSET);
						while (true)
						{
							if (WSALookupServiceNext(hLookup, LUP_RETURN_ADDR, &lsqset, psqset) == S_OK)
							{
								addrInfo = &psqset->lpcsaBuffer[0];
								break;
							}
							else
							{
								LastError = WSAGetLastError();
								if (LastError == WSA_E_NO_MORE) {
									break;
								}
								else if (LastError == WSAEFAULT && lsqset > 0)
								{
									psqset = (WSAQUERYSET *)realloc(psqset, lsqset);
								}
								else
								{
									printf("Service WSALookupServiceNext %s\n", errorMsg(LastError));
									break;
								}
							}
						}
						WSALookupServiceEnd(hLookup);
					}
					else
					{
						printf("Service WSALookupServiceBegin %s\n", errorMsg(LastError));
					}
					break;
				}
			}
			else
			{
				LastError = WSAGetLastError();

				if (LastError == WSAEFAULT && resultSize > 0)
				{
					pqset = (WSAQUERYSET *)realloc(pqset, resultSize);
				}
				else if (LastError == WSA_E_NO_MORE)
				{
					break;
				}
				else
				{
					printf("WSALookupServiceNext %s\n", errorMsg(LastError));
					break;
				}
			}
		}
		WSALookupServiceEnd(lHandle);

		if (addrInfo != NULL) {
			SOCKET sock;
			sock = socket(addrInfo->RemoteAddr.lpSockaddr->sa_family, addrInfo->iSocketType, addrInfo->iProtocol);
			if (sock != INVALID_SOCKET) {
				int cret = connect(sock, addrInfo->RemoteAddr.lpSockaddr, addrInfo->RemoteAddr.iSockaddrLength);

				if (cret == SOCKET_ERROR) {
					LastError = WSAGetLastError();
					printf("connect %s\n", errorMsg(LastError));
				}
				else
				{
					int recvTimeout = 15000;
					bool toQuit = false;

					setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char *)&recvTimeout, sizeof(recvTimeout));
					Sleep(150);
					printf("Connected\n");

					printf("1. Turn On 2. Turn Off 3. Check On/Off 4. 顯示時間 5.設定時間 Q. Quit\n");
					while (!toQuit)
					{
						TCHAR choice = _gettch();
						switch (choice)
						{
						case L'1':
							TurnOnOff(sock, true);
							break;

						case L'2':
							TurnOnOff(sock, false);
							break;

						case L'3':
							CheckOnOff(sock);
							break;

						case L'4':
							ShowDateTime(sock);
							break;

						case L'5':
							SetDateTime(sock);
							break;

						case L'Q':
						case L'q':
							toQuit = true;
							break;

						default:
							break;
						}
					}
				}
				closesocket(sock);
			}
		}
		if (psqset)
		{
			free(psqset);
		}
		if (pqset)
		{
			free(pqset);
		}
	}
	else
	{
		printf("WSALookupServiceBegin Error: %s\n", errorMsg(WSAGetLastError()));
	}
	WSACleanup();

    return 0;
}

