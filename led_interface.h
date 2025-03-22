#pragma once

#include "structures.h"

class ILEDInterface
{
public:

	virtual bool InitInterface() = 0;
	virtual void DeinitInterface() = 0;
	virtual bool IsInitialized() { return false; }
	virtual int GetNumLEDs() = 0;
	virtual void SetLEDs(std::shared_ptr<std::vector<LEDOutputData>> ledData) = 0;
	virtual void TurnOffLEDs() = 0;
};
