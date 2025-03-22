
#pragma once

#include "framework.h"


struct alignas(16) LEDSampleArea
{
	float xMin = 0;
	float yMin = 0;
	float xMax = 0;
	float yMax = 0;

	LEDSampleArea(float inXMin, float inYMin, float inXMax, float inYMax)
	{
		xMin = inXMin;
		yMin = inYMin;
		xMax = inXMax;
		yMax = inYMax;
	}

	LEDSampleArea() {}
};


struct alignas(16) LEDShaderOutput
{
	double r = 0;
	double g = 0;
	double b = 0;
	double _pad = 0;

	LEDShaderOutput(double red, double green, double blue)
	{
		r = red;
		g = green;
		b = blue;
	}

	LEDShaderOutput() {}
};


struct LEDSampleData
{
	int NumLEDs = 0;
	bool IsInputUpdated = false;
	std::vector<LEDSampleArea> sampleAreas;
	std::vector<LEDShaderOutput> sampleOutput;

	LEDSampleData() {}

	LEDSampleData(int num)
	{
		NumLEDs = num;
		IsInputUpdated = true;
		sampleAreas.resize(num);
		sampleOutput.resize(num);
	}
};


struct LEDOutputData
{
	uint8_t r;
	uint8_t g;
	uint8_t b;
};