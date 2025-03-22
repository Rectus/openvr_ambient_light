
#include "ambient_light_sampler.h"

#include <cmath>
#include "mathutil.h"

#include "profiling.h"

#define WAIT_FRAME_TIMEOUT_MS 100

AmbientLightSampler::AmbientLightSampler(std::shared_ptr<SettingsManager> settingsManager, AsyncData& asyncData)
	: m_settingsManager(settingsManager)
	, m_asyncData(asyncData)
	, m_renderer(D3D11Renderer(settingsManager))
{
	
}

AmbientLightSampler::~AmbientLightSampler()
{
	m_asyncData.LightsConnected = false;

	if (m_thread.joinable())
	{
		m_bRun = false;
		m_thread.join();
	}

	if (m_interface.get())
	{
		m_interface->TurnOffLEDs();
		m_interface->DeinitInterface();
		m_interface.reset();
		m_interface = nullptr;
	}
}

bool AmbientLightSampler::InitSampler()
{
	if (m_thread.joinable())
	{
		m_bRun = false;
		m_thread.join();
	}


	m_bRun = true;
	m_bThreadIntialized = false;
	m_bThreadFailed = false;
	m_thread = std::thread(&AmbientLightSampler::RunThread, this);

	while (m_thread.joinable() && !m_bThreadIntialized && !m_bThreadFailed)
	{
		std::this_thread::yield();
	}

	if (m_bThreadIntialized)
	{
		g_logger->info("Light sampler initialized");

		return true;
	}

	return false;
}

void AmbientLightSampler::CalculateOutputColor(LEDShaderOutput& input, LEDOutputData& output)
{
	Settings_Main& mainSettings = m_settingsManager->GetSettings_Main();

	double L, a, b, red, green, blue;

	LinearRGBtoLAB_D65(input.r, input.g, input.b, L, a, b);

	L = min(max((L - 50.0) * mainSettings.Contrast + 50.0, 0.0), 100.0);
	//L = min(max(L + (mainSettings.Brightness - 1.0) * 100.0, 0.0), 100.0);
	a *= mainSettings.Saturation;
	b *= mainSettings.Saturation;

	LABtoLinearRGB_D65(L, a, b, red, green, blue);

	output.r = (uint8_t)(min(max((std::pow((red - mainSettings.MinRed) * mainSettings.Brightness / (1.0 - mainSettings.MinRed), 1.0 / mainSettings.GammaRed) * mainSettings.MaxRed), 0.0), 1.0) * 255.0);

	output.g = (uint8_t)(min(max((std::pow((green - mainSettings.MinGreen) * mainSettings.Brightness / (1.0 - mainSettings.MinGreen), 1.0 / mainSettings.GammaGreen) * mainSettings.MaxGreen), 0.0), 1.0) * 255.0);

	output.b = (uint8_t)(min(max((std::pow((blue - mainSettings.MinBlue) * mainSettings.Brightness / (1.0 - mainSettings.MinBlue), 1.0 / mainSettings.GammaBlue) * mainSettings.MaxBlue), 0.0), 1.0) * 255.0);
}

void AmbientLightSampler::RunThread()
{
	{
		if (!m_renderer.IsIntialized() && !m_renderer.InitRenderer())
		{
			m_bThreadFailed = true;
			return;
		}

		Settings_Main& mainSettings = m_settingsManager->GetSettings_Main();
		Settings_AdaLight& adaSettings = m_settingsManager->GetSettings_AdaLight();

		int numLEDs = mainSettings.NumLights;

		if (m_interface.get())
		{
			m_interface->TurnOffLEDs();
			m_interface->DeinitInterface();
			m_interface.reset();
			m_interface = nullptr;
		}

		m_asyncData.LightsConnected = false;
		m_asyncData.InterfaceInitFailed = false;

		m_interface = std::make_unique<ADALightLEDInterface>(numLEDs, adaSettings.ComPort, adaSettings.BaudRate);

		if (!m_interface->InitInterface())
		{
			g_logger->warn("Failed to initialize LED interface.");
			m_asyncData.InterfaceInitFailed = true;
			m_bThreadFailed = true;
			return;
		}

		m_asyncData.LightsConnected = true;

		m_ledData = std::make_shared<LEDSampleData>(numLEDs);
		UpdateSampleArea();

		m_writeData = std::make_shared<std::vector<LEDOutputData>>(numLEDs);

		m_bThreadIntialized = true;
	}


	while (m_bRun)
	{
		Settings_Main& mainSettings = m_settingsManager->GetSettings_Main();

		if (!m_interface->IsInitialized())
		{
			g_logger->error("LED interface failure!");
			m_asyncData.LightsConnected = false;
			m_bRun = false;
			break;
		}

		if (mainSettings.PreviewMode > 0)
		{
			LEDShaderOutput input;

			if (mainSettings.PreviewMode == 1)
			{
				input = { mainSettings.PreviewValue, mainSettings.PreviewValue, mainSettings.PreviewValue };
			}
			else if (mainSettings.PreviewMode == 2)
			{
				input = { mainSettings.PreviewValue, 0.0, 0.0 };
			}
			else if (mainSettings.PreviewMode == 3)
			{
				input = { 0.0, mainSettings.PreviewValue, 0.0 };
			}
			else if (mainSettings.PreviewMode == 4)
			{
				input = { 0.0, 0.0, mainSettings.PreviewValue };
			}

			for (int i = 0; i < m_ledData->NumLEDs; i++)
			{
				CalculateOutputColor(input, (*m_writeData.get())[i]);
			}

			m_asyncData.PreviewActive = true;
			m_interface->SetLEDs(m_writeData);
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
			continue;
		}
		m_asyncData.PreviewActive = false;

		vr::EDeviceActivityLevel level = vr::VRSystem()->GetTrackedDeviceActivityLevel(vr::k_unTrackedDeviceIndex_Hmd);

		if (level == vr::k_EDeviceActivityLevel_Unknown || level == vr::k_EDeviceActivityLevel_Standby || level == vr::k_EDeviceActivityLevel_Idle_Timeout)
		{
			m_asyncData.OpenVRSampling = false;
			m_interface->TurnOffLEDs();
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
			continue;
		}


		vr::EVROverlayError error = vr::VROverlay()->WaitFrameSync(WAIT_FRAME_TIMEOUT_MS);

		if (error == vr::VROverlayError_TimedOut)
		{
			m_asyncData.OpenVRSampling = false;
			g_logger->warn("Frame sync timed out.");
			m_interface->TurnOffLEDs();
			std::this_thread::yield();
			continue;
		}	

		LARGE_INTEGER preRenderTime = StartPerfTimer();

		

		if (m_bGeometryUpdated)
		{
			m_bGeometryUpdated = false;
			UpdateSampleArea();
			m_ledData->IsInputUpdated = true;
		}


		if (m_renderer.Render(m_ledData))
		{
			if (!m_bRun) { break; }

			for (int i = 0; i < m_ledData->NumLEDs; i++)
			{
				CalculateOutputColor(m_ledData->sampleOutput[i], (*m_writeData.get())[i]);
			}

			float renderTime = EndPerfTimer(preRenderTime.QuadPart);
			LARGE_INTEGER prePresentTime = StartPerfTimer();

			if (!m_bRun) { break; }

			m_interface->SetLEDs(m_writeData);

			float presentTime = EndPerfTimer(prePresentTime.QuadPart);

			float frameInterval = EndPerfTimer(m_lastRenderTime);
			m_lastRenderTime = StartPerfTimer();

			m_asyncData.OpenVRSampling = true;
			m_asyncData.RenderTimeMS = UpdateAveragePerfTime(m_renderTimes, renderTime, 20);
			m_asyncData.PresentTimeMS = UpdateAveragePerfTime(m_presentTimes, presentTime, 20);
			m_asyncData.FrameIntervalMS = UpdateAveragePerfTime(m_frameIntervals, frameInterval, 20);
		}

		std::this_thread::yield();
	}

	m_asyncData.OpenVRSampling = false;
}

void AmbientLightSampler::UpdateSampleArea()
{
	Settings_Main& mainSettings = m_settingsManager->GetSettings_Main();

	float vertFrac = mainSettings.HeightFraction / (mainSettings.NumLights / 2);
	float vertRadius = mainSettings.HeightFraction * mainSettings.VerticalAreaSize / (mainSettings.NumLights / 2) / 2.0f;

	float curvatureFactor = mainSettings.Curvature * 1.0f / mainSettings.NumLights;
	float curvatureHalfway = mainSettings.CurvatureShape * (mainSettings.NumLights / 4.0f) + (mainSettings.NumLights / 4.0f) - 0.5f;


	for (int i = 0; i < mainSettings.NumLights / 2; i++)
	{
		int index = mainSettings.SwapLeftRight ?
			(mainSettings.BottomToTopLeft ? mainSettings.NumLights - 1 - i : i + mainSettings.NumLights / 2) :
			(mainSettings.BottomToTopLeft ? mainSettings.NumLights / 2 - 1 - i : i);

		float xOrigin = mainSettings.HorizontalOffset + curvatureFactor * pow(fabsf(i - curvatureHalfway), 2.0f);
		float yOrigin = vertFrac * (i + 0.5f) - mainSettings.VerticalOffset + (1.0f - mainSettings.HeightFraction) / 2.0f;

		m_ledData->sampleAreas[index].xMin = (xOrigin - mainSettings.WidthFraction / 2.0f);
		m_ledData->sampleAreas[index].xMax = (xOrigin + mainSettings.WidthFraction / 2.0f);

		m_ledData->sampleAreas[index].yMin = (yOrigin - vertRadius);
		m_ledData->sampleAreas[index].yMax = (yOrigin + vertRadius);

		
	}

	for (int i = 0; i < mainSettings.NumLights / 2; i++)
	{
		int index = mainSettings.SwapLeftRight ?
			(mainSettings.BottomToTopRight ? mainSettings.NumLights / 2 - 1 - i : i) :
			(mainSettings.BottomToTopRight ? mainSettings.NumLights - 1 - i : i + mainSettings.NumLights / 2);

		float xOrigin = 1.0f - mainSettings.HorizontalOffset - curvatureFactor * pow(fabsf(i - curvatureHalfway), 2.0f);
		float yOrigin = vertFrac * (i + 0.5f) - mainSettings.VerticalOffset + (1.0f - mainSettings.HeightFraction) / 2.0f;

		m_ledData->sampleAreas[index].xMin = (xOrigin - mainSettings.WidthFraction / 2.0f);
		m_ledData->sampleAreas[index].xMax = (xOrigin + mainSettings.WidthFraction / 2.0f);

		m_ledData->sampleAreas[index].yMin = (yOrigin - vertRadius);
		m_ledData->sampleAreas[index].yMax = (yOrigin + vertRadius);
	}
}
