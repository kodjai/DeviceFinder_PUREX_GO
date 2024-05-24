// *****************************************************************************
//
//     Copyright (c) 2013, Pleora Technologies Inc., All rights reserved.
//
// *****************************************************************************

//
// This sample shows how to find GEV Devices, U3V and Pleora protocol device on a network.
//


#include <PvSampleUtils.h>
#include <PvInterface.h>
#include <PvDevice.h>

#include <iostream>
#include <windows.h>
#include <stdio.h>
#include <tchar.h>

#include <chrono>
#include <thread>
#include <vector>
#include <string>
#include <ctime>

#include <fstream>
#include <sstream>
#include <map>

#define COMID_NEED_INPUT (FALSE)

#define OFFON_ENABLE 1
/*
#define WAIT_FIND_SECONDS 35
#define RECONNECT_LIMIT_COUNT 3
#define CAM_MAX_NUMBER 1
#define CYCLE_NUMBER 100
*/
struct Config {
	int WAIT_FIND_SECONDS;
	int RECONNECT_LIMIT_COUNT;
	int CAM_MAX_NUMBER;
	int CYCLE_NUMBER;
};

PV_INIT_SIGNAL_HANDLER();

std::vector<std::string> userDefinedNameList;

Config readConfig(const std::string& filename) {
	std::ifstream file(filename);
	if (!file.is_open()) {
		throw std::runtime_error("Could not open config file");
	}

	Config config;
	std::map<std::string, int*> configMap = {
		{"WAIT_FIND_SECONDS", &config.WAIT_FIND_SECONDS},
		{"RECONNECT_LIMIT_COUNT", &config.RECONNECT_LIMIT_COUNT},
		{"CAM_MAX_NUMBER", &config.CAM_MAX_NUMBER},
		{"CYCLE_NUMBER", &config.CYCLE_NUMBER}
	};

	std::string line;
	while (std::getline(file, line)) {
		std::istringstream lineStream(line);
		std::string key;
		if (std::getline(lineStream, key, '=')) {
			std::string valueStr;
			if (std::getline(lineStream, valueStr)) {
				int value = std::stoi(valueStr);
				auto it = configMap.find(key);
				if (it != configMap.end()) {
					*(it->second) = value;
				}
			}
		}
	}

	return config;
}

void PrintCurrentTime()
{
	std::time_t currentTime = std::time(nullptr);
	std::cout << "Timestamp: " << std::asctime(std::localtime(&currentTime)) << std::endl;
}

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

int DeviceFinding()
{
	int wait_find_seconds, reconnect_limit_count, cam_max_number, cycle_number;

	try {
		Config config = readConfig("config.txt");

		// 読み込んだ設定を出力して確認
		std::cout << "WAIT_FIND_SECONDS: " << config.WAIT_FIND_SECONDS << std::endl;
		wait_find_seconds = config.WAIT_FIND_SECONDS;

		std::cout << "RECONNECT_LIMIT_COUNT: " << config.RECONNECT_LIMIT_COUNT << std::endl;
		reconnect_limit_count = config.RECONNECT_LIMIT_COUNT;

		std::cout << "CAM_MAX_NUMBER: " << config.CAM_MAX_NUMBER << std::endl;
		cam_max_number = config.CAM_MAX_NUMBER;

		std::cout << "CYCLE_NUMBER: " << config.CYCLE_NUMBER << std::endl;
		cycle_number = config.CYCLE_NUMBER;

	}
	catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}

	//COM1 PowerSupply Control
#if OFFON_ENABLE
	//COM Port Initial
	HANDLE hSerial;
	DCB dcb = { 0 };

	std::string comid;
	std::cout << "Welcome Transmitter!\nConnect to COM1\n" << std::endl;

	//std::cout << 1 << std::endl; 
	LPCWSTR comstr = L"\\\\.\\COM1";

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

#endif // 0

	//Pv Initial
	PvResult lResult;

	//using for check availability of all individuals
	//const PvDeviceInfo* lLastDeviceInfo = NULL;
	//const PvDeviceInfo* lDeviceInfoList[cam_max_number];
	//for (uint32_t x = 0; x < cam_max_number; x++)
	//{
	//	lDeviceInfoList[x] = NULL;
	//}
	std::vector<PvDeviceInfo*> lDeviceInfoList(cam_max_number);
	for (int i = 0; i < cam_max_number; ++i) {
		lDeviceInfoList[i] = nullptr; // 初期化
	}

	PvSystem lSystem;
	int cycleNumber = 1;

	// UserDefinedNameのリストを作成
	//std::vector<std::string> userDefinedNameList = { "Cam00" };
	//std::vector<std::string> userDefinedNameList = { "Cam00", "Cam11" };
	//std::vector<std::string> userDefinedNameList = { "Cam00", "Cam11", "Cam10" };

	//Open log file for recording
	std::ofstream logFile("log.txt", std::ios::app);
	if (!logFile.is_open())
	{
		std::cout << "Log file error!" << endl;
		return false;
	}
	// log current time
	auto now = std::chrono::system_clock::now();
	std::time_t now_time = std::chrono::system_clock::to_time_t(now);
	logFile << "-----------------Log Time: " << std::put_time(std::localtime(&now_time), "%Y-%m-%d %H:%M") << "-----------------" << std::endl;


	while (cycleNumber<= cycle_number)
	{
#if OFFON_ENABLE
		std::cout << "Power cycle No. " << cycleNumber << endl;
		//Power Off->On cycle
		purge(hSerial);

		std::cout << "Power Off" << endl;
		sendMessage(hSerial, (char*)"A1,OT0\n");
		receiveMessage(hSerial, 500);
		std::cout << "Power On" << endl;
		sendMessage(hSerial, (char*)"A1,OT1\n");
		receiveMessage(hSerial, 500);

		std::cout << "Read back of voltage: ";
		sendMessage(hSerial, (char*)"A1,TK6\n");
		receiveMessage(hSerial, 500);
		std::cout << "Read back of current: ";
		sendMessage(hSerial, (char*)"A1,TK7\n");
		receiveMessage(hSerial, 2000);

		for (int t = wait_find_seconds; t > 0; t--)
		{
			std::this_thread::sleep_for(std::chrono::seconds(1));
			std::cout << "Waiting to find camera for " << t << " second(s)\r";
		}
		std::cout << std::endl;
#endif

		// Find all devices on the network.
		//cout << "Find devices" << endl << endl;
		lResult = lSystem.Find();
		if (!lResult.IsOK())
		{
			std::cout << "PvSystem::Find Error: " << lResult.GetCodeString().GetAscii();
			return -1;
		}

		// Go through all interfaces 
		uint32_t lInterfaceCount = lSystem.GetInterfaceCount();
		if (lInterfaceCount <= 0)
		{
			std::cout << "No Interface\n";
		}
		uint32_t lDeviceCount = 0;
		for (uint32_t x = 0; x < lInterfaceCount; x++)
		{
			//std::cout << "Interface " << x << endl;

			// Get pointer to the interface.
			const PvInterface* lInterface = lSystem.GetInterface(x);

			// Is it a PvNetworkAdapter?
			const PvNetworkAdapter* lNIC = dynamic_cast<const PvNetworkAdapter*>(lInterface);
			if (lNIC != NULL)
			{
				//cout << "  MAC Address: " << lNIC->GetMACAddress().GetAscii() << endl;

				uint32_t lIPCount = lNIC->GetIPAddressCount();
				for (uint32_t i = 0; i < lIPCount; i++)
				{
					//cout << "  IP Address " << i << ": " << lNIC->GetIPAddress( i ).GetAscii() << endl;
					//cout << "  Subnet Mask " << i << ": " << lNIC->GetSubnetMask( i ).GetAscii() << endl << endl;
				}
			}
/*
			// Is it a PvUSBHostController?
			const PvUSBHostController* lUSB = dynamic_cast<const PvUSBHostController*>(lInterface);
			if (lUSB != NULL)
			{
				//cout << "  Name: " << lUSB->GetName().GetAscii() << endl << endl;
			}
*/
			// Go through all the devices attached to the interface
			uint32_t lDeviceCountNow = lInterface->GetDeviceCount();
			lDeviceCount += lDeviceCountNow;
			for (uint32_t y = 0; y < lDeviceCountNow; y++)
			{
				const PvDeviceInfo *lDeviceInfo = lInterface->GetDeviceInfo(y);

				std::cout << "  Device " << y + lDeviceCount - lDeviceCountNow << endl;
				std::cout << "    Display ID: " << lDeviceInfo->GetDisplayID().GetAscii() << endl;

				const PvDeviceInfoGEV* lDeviceInfoGEV = dynamic_cast<const PvDeviceInfoGEV*>(lDeviceInfo);
				//const PvDeviceInfoU3V *lDeviceInfoU3V = dynamic_cast<const PvDeviceInfoU3V *>(lDeviceInfo);
				//const PvDeviceInfoUSB *lDeviceInfoUSB = dynamic_cast<const PvDeviceInfoUSB *>(lDeviceInfo);
				//const PvDeviceInfoPleoraProtocol* lDeviceInfoPleora = dynamic_cast<const PvDeviceInfoPleoraProtocol*>(lDeviceInfo);

				if (lDeviceInfoGEV != NULL) // Is it a GigE Vision device?
				{
					std::cout << "    MAC Address: " << lDeviceInfoGEV->GetMACAddress().GetAscii() << endl;
					std::cout << "    IP Address: " << lDeviceInfoGEV->GetIPAddress().GetAscii() << endl;
					std::cout << "    Serial number: " << lDeviceInfoGEV->GetSerialNumber().GetAscii() << endl;
					std::cout << "    UserDefinedName: " << lDeviceInfoGEV->GetUserDefinedName().GetAscii() << endl << endl;
					lDeviceInfoList[y + lDeviceCount - lDeviceCountNow] = lDeviceInfo;
				}
				/*
				else if (lDeviceInfoU3V != NULL) // Is it a USB3 Vision device?
				{
					std::cout << "    GUID: " << lDeviceInfoU3V->GetDeviceGUID().GetAscii() << endl;
					std::cout << "    S/N: " << lDeviceInfoU3V->GetSerialNumber().GetAscii() << endl;
					std::cout << "    Speed: " << lUSB->GetSpeed() << endl;
					std::cout << "    UserDefinedName: " << lDeviceInfoU3V->GetUserDefinedName().GetAscii() << endl << endl;
					lDeviceInfoList[y + lDeviceCount - lDeviceCountNow] = lDeviceInfo;
				}
				*/
				else
				{
					std::cout << "    No Device found! " << endl;
				}
			}
		}

		if (lDeviceCount <= 0)
		{
			std::cout << "No device found" << endl;
		}
		else if(lDeviceCount < cam_max_number)
		{
			std::cout << "Lost camera" << endl;
			PrintCurrentTime();
			break;
		}
		else 
		{
			bool connectFailed = false;
			// Connect to the last device found
			bool reconnectFlag = false;

			for (uint32_t y = 0; y < lDeviceCount; y++)
			{
				int i = reconnect_limit_count;


				if (lDeviceInfoList[y] != NULL)
				{
					do
					{
						std::cout << "Connecting to " << lDeviceInfoList[y]->GetDisplayID().GetAscii() << endl;

						// Creates and connects the device controller based on the selected device.
						PvDevice* lDevice = PvDevice::CreateAndConnect(lDeviceInfoList[y], &lResult);
						if (!lResult.IsOK())
						{
							std::cout << "Unable to connect to " << lDeviceInfoList[y]->GetDisplayID().GetAscii() << endl;
							reconnectFlag = true;
							i--;
							if (i == 0)
							{
								connectFailed = true;
								break;
							}
						}
						else
						{
							std::cout << "Successfully connected to " << lDeviceInfoList[y]->GetDisplayID().GetAscii() << endl;
							PvDeviceGEV *lDeviceGEV = dynamic_cast<PvDeviceGEV *>(lDevice);
							PvGenParameterArray *lDeviceParams = lDevice->GetParameters();

							/*
							//Parameters for GOX debug

							int64_t lDeviceSensorVerifyErrorCount = 0, lDeviceSensorComErrorCount = 0, lDeviceSLVSLinkErrorCount;

							PvResult lResult = lDeviceParams->GetIntegerValue("DeviceSensorVerifyErrorCount", lDeviceSensorVerifyErrorCount);
							if (!lResult.IsOK())
							{
								cerr << "Could not get DeviceSensorVerifyErrorCount" << endl;
								PvDevice::Free(lDevice);
								return false;
							}
							lResult = lDeviceParams->GetIntegerValue("DeviceSensorComErrorCount", lDeviceSensorComErrorCount);
							if (!lResult.IsOK())
							{
								cerr << "Could not get DeviceSensorComErrorCount" << endl;
								PvDevice::Free(lDevice);
								return false;
							}
							lResult = lDeviceParams->GetIntegerValue("DeviceSLVSLinkErrorCount", lDeviceSLVSLinkErrorCount);
							if (!lResult.IsOK())
							{
								cerr << "Could not get DeviceSLVSLinkErrorCount" << endl;
								PvDevice::Free(lDevice);
								return false;
							}
							
							std::cout << "DeviceSensorVerifyErrorCount = " << lDeviceSensorVerifyErrorCount << endl;
							std::cout << "DeviceSensorComErrorCount = " << lDeviceSensorComErrorCount << endl;
							std::cout << "DeviceSLVSLinkErrorCount = " << lDeviceSLVSLinkErrorCount << endl;

							logFile << "DeviceSerialNumber = " << lDeviceInfoList[y]->GetSerialNumber().GetAscii() << endl;
							logFile << "DeviceSensorVerifyErrorCount: " << lDeviceSensorVerifyErrorCount << std::endl;
							logFile << "DeviceSensorComErrorCount: " << lDeviceSensorVerifyErrorCount << std::endl;
							logFile << "DeviceSLVSLinkErrorCount: " << lDeviceSLVSLinkErrorCount << std::endl;
							*/

							PvDevice::Free(lDevice);
							std::cout << "Disconnecting the device " << lDeviceInfoList[y]->GetDisplayID().GetAscii() << endl;
							reconnectFlag = false;
						}
					} while (reconnectFlag && !connectFailed);

					if (connectFailed)
					{
						std::cout << "connectFailed" << endl;
						logFile << "connectFailed" << std::endl;
						return false;
					}
				}
				else
				{
					std::cout << "No device found" << endl;
				}
				std::cout << endl;
			}
			std::cout << "Cycle Number: " << cycleNumber << endl << endl;
			logFile << "Cycle Number: " << cycleNumber << std::endl << std::endl;
		}

		// Check if all user-defined names are present
		bool allUserDefinedNamesPresent = true;
		for (const auto& userDefinedName : userDefinedNameList)
		{
			bool found = false;
			for (uint32_t y = 0; y < lDeviceCount; y++)
			{
				if (lDeviceInfoList[y] != nullptr && userDefinedName == lDeviceInfoList[y]->GetUserDefinedName().GetAscii())
				{
					found = true;
					break;
				}
			}
			if (!found)
			{
				allUserDefinedNamesPresent = false;
				break;
			}
		}

		if (allUserDefinedNamesPresent)
		{
			std::cout << "All user-defined names are present." << std::endl;
		}
		else
		{
			std::cout << "Not all user-defined names are present. Ending loop..." << std::endl;
			PrintCurrentTime();
			break;
		}
		cycleNumber++;
	}
#if OFFON_ENABLE
	CloseHandle(hSerial);
#endif
	logFile.close();
    return 0;
}


//
// Main function.
//

int main()
{
    PV_SAMPLE_INIT();

	std::cout << "DeviceFinder sample" << endl << endl;
    // Find devices.
    
    DeviceFinding();

	std::cout << endl;
	std::cout << "<press a key to exit>" << endl;
    PvWaitForKeyPress();

    PV_SAMPLE_TERMINATE();

    return 0;
}

