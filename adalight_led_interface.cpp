
#include "adalight_led_interface.h"


ADALightLEDInterface::ADALightLEDInterface(int numLights, std::string& comPort, int baudRate)
	:m_numLEDs(numLights)
	,m_comPort(comPort)
	,m_baudRate(baudRate)
{
	m_comPort = std::string("\\\\.\\") + comPort;
}

ADALightLEDInterface::~ADALightLEDInterface()
{
	DeinitInterface();
}

void ADALightLEDInterface::DeinitInterface()
{
	if (m_fileHandle != INVALID_HANDLE_VALUE)
	{
		if (CloseHandle(m_fileHandle))
		{
			g_logger->info("AdaLight disconnected");
		}
		else
		{
			g_logger->warn("AdaLight: failed to properly close serial port!");
		}

		m_fileHandle = INVALID_HANDLE_VALUE;
	}
	
}


bool ADALightLEDInterface::InitInterface()
{
	m_fileHandle = CreateFileA(m_comPort.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);

	if (m_fileHandle == INVALID_HANDLE_VALUE)
	{
		LPWSTR messageBuffer;

		DWORD length = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, GetLastError(), MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), (LPWSTR)&messageBuffer, 0, NULL);

		if (length > 2) // Remove trailing newline
		{
			messageBuffer[length - 1] = '\0';
			messageBuffer[length - 2] = '\0';
		}

		g_logger->warn(L"AdaLight: failed to open serial port: {}", messageBuffer);

		LocalFree(messageBuffer);

		return false;
	}

	DCB params = {};

	GetCommState(m_fileHandle, &params);

	params.BaudRate = m_baudRate;
	params.ByteSize = 8;
	params.StopBits = ONESTOPBIT;
	params.Parity = NOPARITY;

	SetCommState(m_fileHandle, &params);

	COMMTIMEOUTS timeouts = {};

	GetCommTimeouts(m_fileHandle, &timeouts);

	timeouts.WriteTotalTimeoutConstant = 100;
	timeouts.WriteTotalTimeoutMultiplier = 10;

	SetCommTimeouts(m_fileHandle, &timeouts);

	m_transmitBuffer.resize(6 + (m_numLEDs * 3));

	m_transmitBuffer[0] = 'A';
	m_transmitBuffer[1] = 'd';
	m_transmitBuffer[2] = 'a';
	m_transmitBuffer[3] = (m_numLEDs - 1) >> 8;
	m_transmitBuffer[4] = (m_numLEDs - 1) & 0xff;
	m_transmitBuffer[5] = m_transmitBuffer[3] ^ m_transmitBuffer[4] ^ 0x55;

	TurnOffLEDs();

	g_logger->info("AdaLight interface initialized on serial port {}", m_comPort);

	return true;
}

void ADALightLEDInterface::SetLEDs(std::shared_ptr<std::vector<LEDOutputData>> ledData)
{
	if (m_fileHandle == INVALID_HANDLE_VALUE)
	{
		return;
	}

	DWORD bytesWritten = 0;

	memcpy(&m_transmitBuffer[6], ledData->data(), min(ledData->size() * 3, m_numLEDs * 3));

	WriteFile(m_fileHandle, m_transmitBuffer.data(), (DWORD)m_transmitBuffer.size(), &bytesWritten, nullptr);

}

void ADALightLEDInterface::TurnOffLEDs()
{
	if (m_fileHandle == INVALID_HANDLE_VALUE)
	{
		return;
	}

	DWORD bytesWritten = 0;

	memset(&m_transmitBuffer[6], '\0', m_numLEDs * 3);

	WriteFile(m_fileHandle, m_transmitBuffer.data(), (DWORD)m_transmitBuffer.size(), &bytesWritten, nullptr);
}
