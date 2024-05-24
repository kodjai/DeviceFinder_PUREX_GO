
#include <iostream>
#include <windows.h>
#include <stdio.h>
#include <tchar.h>

#include <chrono>
#include <thread>

#define COMID_NEED_INPUT (FALSE)

int purge(HANDLE hSerial) {
    if ((PurgeComm(hSerial, PURGE_TXABORT | PURGE_RXABORT |
        PURGE_TXCLEAR | PURGE_RXCLEAR)) != TRUE) {
        return(FALSE);
    }
    return(TRUE);
}

int sendMessage(HANDLE hSerial, char *message) {
    DWORD bytes_written;
    //printf("send message:%s size:%d\n", message, (UINT)strlen(message));
    if (!WriteFile(hSerial, message, (UINT)strlen(message), &bytes_written, NULL))
    {
        std::cerr << "[Error] writing to serial port" << std::endl;
        CloseHandle(hSerial);
        return -1;
    }
    //std::cout << "send complete." << std::endl;
    return 0;
}

int receiveMessage(HANDLE hSerial, UINT waitms) {
    DWORD bytesRead;
    char buffer[256];

    uint32_t p = 0;
    char line[256];
    memset(line, '\0', sizeof(line));
    bool rxend = false;
    
    auto start = std::chrono::system_clock::now();
    while (true) {
        auto end = std::chrono::system_clock::now();
        auto dur = end - start;
        auto msec = std::chrono::duration_cast<std::chrono::milliseconds>(dur).count();
        //std::cout << msec << std::endl;
        if (dur > std::chrono::milliseconds(waitms)) break;

        // ------------------ RX ------------------
        if (!ReadFile(hSerial, buffer, sizeof(buffer), &bytesRead, NULL)) continue;
        for (UINT i = 0; i < bytesRead; i++) {
            putchar(buffer[i]); // Debug
            line[p++] = buffer[i];
            if (buffer[i] == '\n') {
                line[--p] = '\0';  // 改行コードを削除
                //printf("%s", line);

                // 期待値受信チェック
                char compare[] = "OK";
                if (!strncmp(line, compare, sizeof(compare))) {
                    printf("Get OK\n"); rxend = true;
                }
                p = 0; memset(line, 0, sizeof(line));
            }
        }

    }
    //std::this_thread::sleep_for(std::chrono::seconds(3));
    return 0;
}

enum {
    OPE_DISP,
    OPE_OFF,
    OPE_ON,
    OPE_OFFON,
};

int main(int argc, char* argv[]) {
    HANDLE hSerial;
    DCB dcb = { 0 };

    UINT opeMode = OPE_DISP;

    if (argc > 1 && !strcmp(argv[1], "on")) opeMode = OPE_ON;
    if (argc > 1 && !strcmp(argv[1], "off")) opeMode = OPE_OFF;
    if (argc > 1 && !strcmp(argv[1], "offon")) opeMode = OPE_OFFON;
    printf("%s mode:%d\n", argv[0], opeMode);

    std::string comid;
    std::cout << "Welcome Transmitter! Please Input COM Number\n" << ">";
#if (COMID_NEED_INPUT == TRUE)
    std::cin >> comid;
    std::string comstr = R"(\\.\COM)" + comid;
#else
	std::cout << 1 << std::endl; LPCWSTR comstr = L"\\\\.\\COM1";
#endif

    hSerial = CreateFile(comstr, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    if (hSerial == INVALID_HANDLE_VALUE)
    {
        std::cout << "Cannot open COM port" << std::endl;
        return -1;
    }

    if (!GetCommState(hSerial, &dcb))
    {
        std::cerr << "[Error] getting serial port configuration" << std::endl;
        CloseHandle(hSerial);
        return -1;
    }

    dcb.DCBlength = sizeof(dcb);
    dcb.BaudRate = CBR_9600;
    dcb.fBinary = TRUE;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    dcb.ByteSize = 8;
    if (!SetCommState(hSerial, &dcb))
    {
        std::cerr << "[Error] setting serial port configuration" << std::endl;
        CloseHandle(hSerial);
        return -1;
    }

    COMMTIMEOUTS ComTimeout;
    GetCommTimeouts(hSerial, &ComTimeout);
    ComTimeout.ReadIntervalTimeout = 500;
    ComTimeout.ReadTotalTimeoutMultiplier = 0;
    ComTimeout.ReadTotalTimeoutConstant = 500;
    SetCommTimeouts(hSerial, &ComTimeout);

    purge(hSerial);

    // ------------------ TX ------------------
    if (opeMode == OPE_ON) {
        sendMessage(hSerial, (char*)"A1,OT1\n");
        receiveMessage(hSerial, 500);
    }
    else if (opeMode == OPE_OFF) {
        sendMessage(hSerial, (char*)"A1,OT0\n");
        receiveMessage(hSerial, 500);
    }
    else if (opeMode == OPE_OFFON) {
        sendMessage(hSerial, (char*)"A1,OT0\n");
        receiveMessage(hSerial, 500);
        sendMessage(hSerial, (char*)"A1,OT1\n");
        receiveMessage(hSerial, 500);
    }

    sendMessage(hSerial, (char*)"A1,TK6\n");
    receiveMessage(hSerial, 500);
    sendMessage(hSerial, (char*)"A1,TK7\n");
    receiveMessage(hSerial, 2000);

    std::cout << "End." << std::endl;
    CloseHandle(hSerial);
    return 0;
}

