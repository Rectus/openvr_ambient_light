
#pragma once

#include "led_interface.h"

class ADALightLEDInterface : public ILEDInterface
{
public:
	ADALightLEDInterface(int numLights, std::string& comPort, int baudRate);
	~ADALightLEDInterface();
	bool InitInterface();
	void DeinitInterface();
	bool IsInitialized() { return m_fileHandle != INVALID_HANDLE_VALUE; }
	int GetNumLEDs() { return m_numLEDs; }
	void SetLEDs(std::shared_ptr<std::vector<LEDOutputData>> ledData);
	void TurnOffLEDs();

protected:

	int m_numLEDs = 0;
	std::string m_comPort;
	int m_baudRate = 0;

	HANDLE m_fileHandle = INVALID_HANDLE_VALUE;
	std::vector<uint8_t> m_transmitBuffer;
};

