
#pragma once

#include "framework.h"
#include "SimpleIni.h"

struct Settings_Main
{
	bool InterfaceConfigured = false;
	bool EnableLights = true; // Transient
	bool EnableLightsOnStartup = true;
	bool StartWithSteamVR = false;
	bool SkipMirrorTextureRelease = true;

	int NumLights = 18;

	bool SwapLeftRight = false;
	bool BottomToTopLeft = false;
	bool BottomToTopRight = true;

	float HeightFraction = 0.5f;
	float WidthFraction = 0.35f;
	float VerticalOffset = -0.02f;
	float HorizontalOffset = 0.22f;
	float VerticalAreaSize = 1.0f;
	float Curvature = 0.12f;
	float CurvatureShape = 0.12f;

	int PreviewMode = 0; // Transient
	float PreviewValue = 0.0f; // Transient

	float Brightness = 1.0f;
	float Contrast = 1.0f;
	float Saturation = 1.0f;

	float MinRed = 0.0f;
	float MinGreen = 0.0f;
	float MinBlue = 0.0f;

	float MaxRed = 1.0f;
	float MaxGreen = 1.0f;
	float MaxBlue = 1.0f;

	float GammaRed = 2.2f;
	float GammaGreen = 2.2f;
	float GammaBlue = 2.2f;

	void ParseSettings(CSimpleIniA& ini, const char* section)
	{
		InterfaceConfigured = ini.GetBoolValue(section, "InterfaceConfigured", InterfaceConfigured);
		EnableLightsOnStartup = ini.GetBoolValue(section, "EnableLightsOnStartup", EnableLightsOnStartup);
		NumLights = (int)ini.GetLongValue(section, "NumLights", NumLights);
		StartWithSteamVR = ini.GetBoolValue(section, "StartWithSteamVR", StartWithSteamVR);
		SkipMirrorTextureRelease = ini.GetBoolValue(section, "SkipMirrorTextureRelease", SkipMirrorTextureRelease);

		SwapLeftRight = ini.GetBoolValue(section, "SwapLeftRight", SwapLeftRight);
		BottomToTopLeft = ini.GetBoolValue(section, "BottomToTopLeft", BottomToTopLeft);
		BottomToTopRight = ini.GetBoolValue(section, "BottomToTopRight", BottomToTopRight);

		HeightFraction = (float)ini.GetDoubleValue(section, "HeightFraction", HeightFraction);
		WidthFraction = (float)ini.GetDoubleValue(section, "WidthFraction", WidthFraction);
		VerticalOffset = (float)ini.GetDoubleValue(section, "VerticalOffset", VerticalOffset);
		HorizontalOffset = (float)ini.GetDoubleValue(section, "HorizontalOffset", HorizontalOffset);
		VerticalAreaSize = (float)ini.GetDoubleValue(section, "VerticalAreaSize", VerticalAreaSize);
		Curvature = (float)ini.GetDoubleValue(section, "Curvature", Curvature);
		CurvatureShape = (float)ini.GetDoubleValue(section, "CurvatureShape", CurvatureShape);

		Brightness = (float)ini.GetDoubleValue(section, "Brightness", Brightness);
		Contrast = (float)ini.GetDoubleValue(section, "Contrast", Contrast);
		Saturation = (float)ini.GetDoubleValue(section, "Saturation", Saturation);

		MinRed = (float)ini.GetDoubleValue(section, "MinRed", MinRed);
		MinGreen = (float)ini.GetDoubleValue(section, "MinGreen", MinGreen);
		MinBlue = (float)ini.GetDoubleValue(section, "MinBlue", MinBlue);

		MaxRed = (float)ini.GetDoubleValue(section, "MaxRed", MaxRed);
		MaxGreen = (float)ini.GetDoubleValue(section, "MaxGreen", MaxGreen);
		MaxBlue = (float)ini.GetDoubleValue(section, "MaxBlue", MaxBlue);

		GammaRed = (float)ini.GetDoubleValue(section, "GammaRed", GammaRed);
		GammaGreen = (float)ini.GetDoubleValue(section, "GammaGreen", GammaGreen);
		GammaBlue = (float)ini.GetDoubleValue(section, "GammaBlue", GammaBlue);
	}

	void UpdateSettings(CSimpleIniA& ini, const char* section)
	{
		ini.SetBoolValue(section, "InterfaceConfigured", InterfaceConfigured);
		ini.SetBoolValue(section, "EnableLightsOnStartup", EnableLightsOnStartup);
		ini.SetLongValue(section, "NumLights", NumLights);
		ini.SetBoolValue(section, "StartWithSteamVR", StartWithSteamVR);
		ini.SetBoolValue(section, "SkipMirrorTextureRelease", SkipMirrorTextureRelease);

		ini.SetBoolValue(section, "SwapLeftRight", SwapLeftRight);
		ini.SetBoolValue(section, "BottomToTopLeft", BottomToTopLeft);
		ini.SetBoolValue(section, "BottomToTopRight", BottomToTopRight);

		ini.SetDoubleValue(section, "HeightFraction", HeightFraction);
		ini.SetDoubleValue(section, "WidthFraction", WidthFraction);
		ini.SetDoubleValue(section, "VerticalOffset", VerticalOffset);
		ini.SetDoubleValue(section, "HorizontalOffset", HorizontalOffset);
		ini.SetDoubleValue(section, "VerticalAreaSize", VerticalAreaSize);
		ini.SetDoubleValue(section, "Curvature", Curvature);
		ini.SetDoubleValue(section, "CurvatureShape", CurvatureShape);

		ini.SetDoubleValue(section, "Brightness", Brightness);
		ini.SetDoubleValue(section, "Contrast", Contrast);
		ini.SetDoubleValue(section, "Saturation", Saturation);

		ini.SetDoubleValue(section, "MinRed", MinRed);
		ini.SetDoubleValue(section, "MinGreen", MinGreen);
		ini.SetDoubleValue(section, "MinBlue", MinBlue);

		ini.SetDoubleValue(section, "MaxRed", MaxRed);
		ini.SetDoubleValue(section, "MaxGreen", MaxGreen);
		ini.SetDoubleValue(section, "MaxBlue", MaxBlue);

		ini.SetDoubleValue(section, "GammaRed", GammaRed);
		ini.SetDoubleValue(section, "GammaGreen", GammaGreen);
		ini.SetDoubleValue(section, "GammaBlue", GammaBlue);
	}
};

struct Settings_AdaLight
{
	std::string ComPort = "";
	int BaudRate = 115200;


	void ParseSettings(CSimpleIniA& ini, const char* section)
	{
		ComPort = ini.GetValue(section, "ComPort", ComPort.data());
		BaudRate = (int)ini.GetLongValue(section, "BaudRate", BaudRate);
	}

	void UpdateSettings(CSimpleIniA& ini, const char* section)
	{
		ini.SetValue(section, "ComPort", ComPort.data());
		ini.SetLongValue(section, "BaudRate", BaudRate);
	}
};


class SettingsManager
{
public:
	SettingsManager(std::wstring settingsFile);
	~SettingsManager();

	void ReadSettingsFile();
	void SettingsUpdated();
	void DispatchUpdate();
	void ResetToDefaults();


	Settings_Main& GetSettings_Main() { return m_settingsMain; }
	Settings_AdaLight& GetSettings_AdaLight() { return m_settingsAdaLight; }
	
protected:

	void UpdateSettingsFile();

	std::wstring m_settingsFile;
	CSimpleIniA m_iniData;
	bool m_bSettingsUpdated = false;

	Settings_Main m_settingsMain;
	Settings_AdaLight m_settingsAdaLight;
};

