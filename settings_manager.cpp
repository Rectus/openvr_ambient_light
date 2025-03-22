
#include "framework.h"
#include "settings_manager.h"

SettingsManager::SettingsManager(std::wstring settingsFile)
	: m_settingsFile(settingsFile)
	, m_bSettingsUpdated(false)
	, m_iniData()
{
	m_iniData.SetUnicode(true);
}

SettingsManager::~SettingsManager()
{
	DispatchUpdate();
}

void SettingsManager::ReadSettingsFile()
{
	SI_Error result = m_iniData.LoadFile(m_settingsFile.c_str());
	if (result < 0)
	{
		g_logger->warn("Failed to read settings file, writing default values...");
		UpdateSettingsFile();
	}
	else
	{
		m_settingsMain.ParseSettings(m_iniData, "Main");
		m_settingsAdaLight.ParseSettings(m_iniData, "AdaLight");
	}
	m_bSettingsUpdated = false;
}

void SettingsManager::UpdateSettingsFile()
{
	m_settingsMain.UpdateSettings(m_iniData, "Main");
	m_settingsAdaLight.UpdateSettings(m_iniData, "AdaLight");

	SI_Error result = m_iniData.SaveFile(m_settingsFile.c_str());
	if (result < 0)
	{
		g_logger->error("Failed to save settings file, {}", errno);
	}

	m_bSettingsUpdated = false;
}

void SettingsManager::SettingsUpdated()
{
	m_bSettingsUpdated = true;
}

void SettingsManager::DispatchUpdate()
{
	if (m_bSettingsUpdated)
	{
		UpdateSettingsFile();
	}
}

void SettingsManager::ResetToDefaults()
{
	m_settingsMain = Settings_Main();
	m_settingsAdaLight = Settings_AdaLight();
	UpdateSettingsFile();
}


