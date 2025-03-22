
#pragma once

#include "framework.h"
#include "structures.h"
#include "d3d11_renderer.h"
#include "led_interface.h"
#include "adalight_led_interface.h"
#include "async_data.h"
#include "settings_manager.h"

class AmbientLightSampler
{
public:

	AmbientLightSampler(std::shared_ptr<SettingsManager> settingsManager, AsyncData& asyncData);
	~AmbientLightSampler();

	bool InitSampler();

	void SetGeometryUpdated() { m_bGeometryUpdated = true; }

protected:
	void CalculateOutputColor(LEDShaderOutput& input, LEDOutputData& output);
	void RunThread();
	void UpdateSampleArea();

	std::atomic_bool m_bRun = true;
	std::atomic_bool m_bThreadIntialized = false;
	std::atomic_bool m_bThreadFailed = false;
	std::atomic_bool m_bGeometryUpdated = false;
	std::thread m_thread;

	std::shared_ptr<SettingsManager> m_settingsManager;
	AsyncData& m_asyncData;

	D3D11Renderer m_renderer;

	std::shared_ptr<LEDSampleData> m_ledData;

	std::shared_ptr<std::vector<LEDOutputData>> m_writeData;

	std::unique_ptr<ILEDInterface> m_interface;

	LARGE_INTEGER m_lastRenderTime = {};
	std::deque<float> m_frameIntervals;
	std::deque<float> m_renderTimes;
	std::deque<float> m_presentTimes;
};

