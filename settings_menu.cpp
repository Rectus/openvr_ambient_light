

#include "imgui.h"
#include "imgui_internal.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx11.h"
#include "misc/cpp/imgui_stdlib.h"
#include "implot.h"
#include <cmath>
#include "mathutil.h"
#include "settings_menu.h"

#include "fonts/roboto_medium.cpp"
#include "fonts/cousine_regular.cpp"


#define OVERLAY_RES_WIDTH 1280
#define OVERLAY_RES_HEIGHT 960

using Microsoft::WRL::ComPtr;

SettingsMenu::SettingsMenu(std::shared_ptr<SettingsManager> settingsManager, HWND window, AsyncData& asyncData)
	: m_settingsManager(settingsManager)
	, m_windowHandle(window)
	, m_asyncData(asyncData)
{

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImPlot::CreateContext();

	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.DisplaySize = ImVec2(OVERLAY_RES_WIDTH, OVERLAY_RES_HEIGHT);
	io.IniFilename = nullptr;
	io.LogFilename = nullptr;
	m_largeFont = io.Fonts->AddFontFromMemoryCompressedTTF(roboto_medium_compressed_data, roboto_medium_compressed_size, 22);
	m_mainFont = io.Fonts->AddFontFromMemoryCompressedTTF(roboto_medium_compressed_data, roboto_medium_compressed_size, 18);
	m_smallFont = io.Fonts->AddFontFromMemoryCompressedTTF(roboto_medium_compressed_data, roboto_medium_compressed_size, 17);
	m_fixedFont = io.Fonts->AddFontFromMemoryCompressedTTF(cousine_regular_compressed_data, cousine_regular_compressed_size, 13);
	ImGui::StyleColorsDark();


	ImGui_ImplWin32_Init(m_windowHandle);
	SetupDX11();
	ImGui_ImplDX11_Init(m_d3d11Device.Get(), m_d3d11DeviceContext.Get());


	// Hack to fix Dear ImGui not rendering (mostly) correctly to sRGB target
	// From: https://github.com/ocornut/imgui/issues/8271#issuecomment-2564954070
	ImGuiStyle& style = ImGui::GetStyle();
	for (int i = 0; i < ImGuiCol_COUNT; i++)
	{
		ImVec4& col = style.Colors[i];

		// Multiply out the alpha factor, and add it back afterwards
		col.x *= col.w;
		col.y *= col.w;
		col.z *= col.w;

		col.x = (col.x <= 0.04045f ? col.x / 12.92f : pow((col.x + 0.055f) / 1.055f, 2.4f)) / max(col.w, 0.01f);
		col.y = (col.y <= 0.04045f ? col.y / 12.92f : pow((col.y + 0.055f) / 1.055f, 2.4f)) / max(col.w, 0.01f);
		col.z = (col.z <= 0.04045f ? col.z / 12.92f : pow((col.z + 0.055f) / 1.055f, 2.4f)) / max(col.w, 0.01f);
	}
}

SettingsMenu::~SettingsMenu()
{
	if (m_asyncData.SteamVRInitialized)
	{
		if (m_mirrorSRVLeft)
		{
			vr::VRCompositor()->ReleaseMirrorTextureD3D11(m_mirrorSRVLeft);
			m_mirrorSRVLeft = nullptr;
		}
		if (m_mirrorSRVRight)
		{
			vr::VRCompositor()->ReleaseMirrorTextureD3D11(m_mirrorSRVRight);
			m_mirrorSRVRight = nullptr;
		}
	}

	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::GetIO().BackendRendererUserData = NULL;
	ImPlot::DestroyContext();
	ImGui::DestroyContext();
}



static inline void ScrollableSlider(const char* label, float* v, float v_min, float v_max, const char* format, float scrollFactor)
{
	ImGui::SliderFloat(label, v, v_min, v_max, format, ImGuiSliderFlags_None);
	ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelY);
	if (ImGui::IsItemHovered())
	{
		float wheel = ImGui::GetIO().MouseWheel;
		if (wheel)
		{
			if (ImGui::IsItemActive())
			{
				ImGui::ClearActiveID();
			}
			else
			{
				*v += wheel * scrollFactor;
				if (*v < v_min) { *v = v_min; }
				else if (*v > v_max) { *v = v_max; }
			}
		}
	}
}


static inline void ScrollableSliderInt(const char* label, int* v, int v_min, int v_max, const char* format, int scrollFactor)
{
	ImGui::SliderInt(label, v, v_min, v_max, format, ImGuiSliderFlags_None);
	ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelY);
	if (ImGui::IsItemHovered())
	{
		float wheel = ImGui::GetIO().MouseWheel;
		if (wheel)
		{
			if (ImGui::IsItemActive())
			{
				ImGui::ClearActiveID();
			}
			else
			{
				*v += (int)wheel * scrollFactor;
				if (*v < v_min) { *v = v_min; }
				else if (*v > v_max) { *v = v_max; }
			}
		}
	}
}

#define IMGUI_BIG_SPACING ImGui::Dummy(ImVec2(0.0f, 20.0f))

inline void SettingsMenu::TextDescription(const char* fmt, ...)
{
	ImGui::Indent();
	ImGui::PushFont(m_smallFont);
	ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x);
	va_list args;
	va_start(args, fmt);
	ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), fmt, args);
	va_end(args);
	ImGui::PopTextWrapPos();
	ImGui::PopFont();
	ImGui::Unindent();
}

inline void SettingsMenu::TextDescriptionSpaced(const char* fmt, ...)
{
	ImGui::Indent();
	ImGui::PushFont(m_smallFont);
	ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x);
	va_list args;
	va_start(args, fmt);
	ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), fmt, args);
	va_end(args);
	ImGui::PopTextWrapPos();
	ImGui::PopFont();
	ImGui::Unindent();
	IMGUI_BIG_SPACING;
}

static inline void BeginSoftDisabled(bool bIsDisabled)
{
	if (bIsDisabled)
	{
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().DisabledAlpha);
	}
}

static inline void EndSoftDisabled(bool bIsDisabled)
{
	if (bIsDisabled)
	{
		ImGui::PopStyleVar();
	}
}



void SettingsMenu::TickMenu()
{
	if (m_asyncData.SteamVRInitialized)
	{
		if (m_mirrorSRVLeft)
		{
			vr::VRCompositor()->ReleaseMirrorTextureD3D11(m_mirrorSRVLeft);
			m_mirrorSRVLeft = nullptr;
		}
		if (m_mirrorSRVRight)
		{
			vr::VRCompositor()->ReleaseMirrorTextureD3D11(m_mirrorSRVRight);
			m_mirrorSRVRight = nullptr;
		}
	}

	if (!IsWindowVisible(m_windowHandle) || (!m_bMenuIsVisible && m_d3d11SwapChain->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED))
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
		return;
	}
	m_bMenuIsVisible = true;

	if (m_resizeWidth != 0 && m_resizeHeight != 0)
	{
		if (m_d3d11RTV.Get()) 
		{ 
			m_d3d11RTV.Reset(); 
		}

		m_d3d11SwapChain->ResizeBuffers(0, m_resizeWidth, m_resizeHeight, DXGI_FORMAT_UNKNOWN, 0);
		m_resizeWidth = m_resizeHeight = 0;

		ComPtr<ID3D11Texture2D> backBuffer;
		m_d3d11SwapChain->GetBuffer(0, IID_PPV_ARGS(backBuffer.GetAddressOf()));
		m_d3d11Device->CreateRenderTargetView(backBuffer.Get(), nullptr, &m_d3d11RTV);
	}


	bool bReloadSystem = false;
	bool bAppManifestUpdatePending = false;

	Settings_Main& mainSettings = m_settingsManager->GetSettings_Main();
	Settings_AdaLight& adaLightSettings = m_settingsManager->GetSettings_AdaLight();


	ImVec4 colorTextGreen(0.1f, 0.7f, 0.1f, 1.0f);
	ImVec4 colorTextRed(0.7f, 0.1f, 0.1f, 1.0f);
	ImVec4 colorTextOrange(0.7f, 0.65f, 0.1f, 1.0f);

	ImGuiIO& io = ImGui::GetIO();

	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();

	ImGui::NewFrame();

	ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
	ImGui::SetNextWindowSize(io.DisplaySize);

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_GrabMinSize, 20);
	ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 20);
	ImGui::PushFont(m_mainFont);

	ImGui::Begin("OpenVR Ambient Light", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize);



	ImGui::BeginChild("Tab buttons", ImVec2(OVERLAY_RES_WIDTH * 0.15f, 0));

	ImVec2 tabButtonSize(OVERLAY_RES_WIDTH * 0.14f, 55);
	ImVec4 colorActiveTab(0.05f, 0.26f, 0.75f, 1.0f);

	bool bIsActiveTab = false;

#define TAB_BUTTON(name, tab) if (m_activeTab == tab) { ImGui::PushStyleColor(ImGuiCol_Button, colorActiveTab); bIsActiveTab = true; } \
if (ImGui::Button(name, tabButtonSize)) { m_activeTab = tab; } \
if (bIsActiveTab) { ImGui::PopStyleColor(1); bIsActiveTab = false; }

	ImGui::PushFont(m_largeFont);
	TAB_BUTTON("Main", TabMain);
	TAB_BUTTON("Device", TabDevice);
	TAB_BUTTON("Geometry", TabGeometry);
	TAB_BUTTON("Color", TabColor);
	TAB_BUTTON("Log", TabLog);
	ImGui::PopFont();


	ImGui::BeginChild("Sep1", ImVec2(0, 20));
	ImGui::EndChild();

	ImGui::PushFont(m_smallFont);
	ImGui::Indent();

	if (m_asyncData.PreviewActive)
	{
		ImGui::TextColored(colorTextOrange, "Preview Enabled");
	}
	else if (m_asyncData.LightsConnected && m_asyncData.OpenVRSampling)
	{
		ImGui::TextColored(colorTextGreen, "Active");
	}
	else if (m_asyncData.LightsConnected)
	{
		ImGui::TextColored(colorTextOrange, "HMD Idle");
	}
	else if (!mainSettings.EnableLights)
	{
		ImGui::TextColored(colorTextRed, "Lights Disabled");
	}
	else if(m_asyncData.SteamVRInitialized)
	{
		ImGui::TextColored(colorTextRed, "LEDs Not Connected");
	}
	else
	{
		ImGui::TextColored(colorTextRed, "SteamVR not Running");
	}

	if (!mainSettings.InterfaceConfigured)
	{
		ImGui::TextColored(colorTextRed, "Device not Configured");
	}

	if (m_asyncData.LightsConnected && m_asyncData.OpenVRSampling)
	{
		IMGUI_BIG_SPACING;

		ImGui::Text("Frame rate\n %.1fHz", 1000.0f / m_asyncData.FrameIntervalMS);
		ImGui::Text("Render time\n %.1fms", m_asyncData.RenderTimeMS);
		ImGui::Text("LED time\n %.1fms", m_asyncData.PresentTimeMS);
	}
	ImGui::Unindent();
	ImGui::PopFont();

	ImGui::BeginChild("Sep2", ImVec2(0, -ImGui::GetFrameHeightWithSpacing() - 90));
	ImGui::EndChild();

	ImGui::PushFont(m_largeFont);

	if (ImGui::Button(mainSettings.EnableLights ? "Disable Lights" : "Enable Lights", tabButtonSize))
	{
		mainSettings.EnableLights = !mainSettings.EnableLights;
		bReloadSystem = true;
	}

	if (ImGui::Button("Quit", tabButtonSize))
	{
		SendMessage(m_windowHandle, WM_MENU_QUIT, 0, 0);
	}
	ImGui::PopFont();

	ImGui::EndChild();
	ImGui::SameLine();




	if (m_activeTab == TabMain)
	{
		ImGui::BeginChild("Main#TabMain", ImVec2(0, 0), false);

		ImGui::Text("Main Settings");
		ImGui::Separator();
		ImGui::Spacing();

		ImGui::Checkbox("Enable Lights on Startup", &mainSettings.EnableLightsOnStartup);

		bool storedStart = mainSettings.StartWithSteamVR;
		ImGui::Checkbox("Start Application With SteamVR", &mainSettings.StartWithSteamVR);
		if (mainSettings.StartWithSteamVR != storedStart)
		{
			bAppManifestUpdatePending = true;
		}

		IMGUI_BIG_SPACING;
		
		ImGui::BeginChild("Sep3", ImVec2(0, -ImGui::GetFrameHeightWithSpacing() - 180));
		ImGui::EndChild();

		ImGui::Checkbox("Skip Releasing Mirror Textures", &mainSettings.SkipMirrorTextureRelease);
		TextDescription("Skips calling ReleaseMirrorTextureD3D11 every frame.\nThis a workaround for a SteamVR(?) bug that may cause hitching.\nDisable this if you notice memory leaks.");

		IMGUI_BIG_SPACING;

		if (ImGui::Button("Reset To Defaults", tabButtonSize))
		{
			m_settingsManager->ResetToDefaults();
			bReloadSystem = true;
			bAppManifestUpdatePending = true;
		}

		ImGui::EndChild();
	}



	if (m_activeTab == TabDevice)
	{
		ImGui::BeginChild("Device#TabDevice", ImVec2(0, 0), false);

		ImGui::Text("Device Settings");
		ImGui::Separator();
		ImGui::Spacing();

		ImGui::Text("Device: AdaLight");

		
		TextDescription("Only serial devices using the AdaLight protocol are currently supported.");

		IMGUI_BIG_SPACING;

		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
		ImGui::SetNextItemWidth(256);
		ImGui::InputText("", &adaLightSettings.ComPort);

		ImGui::SameLine();

		ImGui::PopStyleVar();
		if (ImGui::BeginCombo("Serial Port", "", ImGuiComboFlags_NoPreview | ImGuiComboFlags_PopupAlignLeft))
		{
			ULONG portNumbers[100];
			ULONG numPorts = 0;
			bool bFoundPorts = GetCommPorts(portNumbers, 100, &numPorts) == ERROR_SUCCESS;
			static int selectedPort = 0;

			if (bFoundPorts)
			{
				for (uint32_t i = 0; i < numPorts; i++)
				{
					std::string port = std::format("COM{}", portNumbers[i]);
					if (ImGui::Selectable(port.data(), i == selectedPort))
					{
						selectedPort = i;
						adaLightSettings.ComPort = port;
					}
					if (i == selectedPort) { ImGui::SetItemDefaultFocus(); }
				}
			}
			ImGui::EndCombo();
		}
		
		TextDescription("Serial port as listed in the Windows Device Manager.");
		IMGUI_BIG_SPACING;

		ImGui::SetNextItemWidth(280);
		ImGui::InputInt("Baud Rate", &adaLightSettings.BaudRate);
		TextDescription("Common values are 115200 and 9600.");
		


		IMGUI_BIG_SPACING;

		ImGui::PushFont(m_largeFont);
		if (ImGui::Button("Apply", tabButtonSize))
		{
			if (adaLightSettings.ComPort.size() > 0 && adaLightSettings.BaudRate > 0)
			{
				mainSettings.InterfaceConfigured = true;
				bReloadSystem = true;
			}
			else
			{
				g_logger->warn("Invalid device configuration!");
				mainSettings.InterfaceConfigured = false;
			}
		}
		ImGui::PopFont();

		IMGUI_BIG_SPACING;

		if (m_asyncData.InterfaceInitFailed)
		{
			ImGui::TextColored(colorTextRed, "Interface initialization failed. See log for more information.");
		}

		ImGui::EndChild();
	}



	if (m_activeTab == TabGeometry)
	{
		ImGui::BeginChild("Geometry#TabGeometry", ImVec2(0, 0), false);

		ImGui::Text("Light Sampling Geometry");
		ImGui::Separator();
		ImGui::Spacing();

		static bool bHasRing = false;

		float previewWindowWidth = max(ImGui::GetContentRegionAvail().x * 1.00f, 200);

		ImGui::BeginChild("Preview", ImVec2(previewWindowWidth, previewWindowWidth * 0.5f));

		ImVec2 previewMin = ImGui::GetCursorScreenPos();
		ImVec2 previewSize = ImGui::GetContentRegionAvail();
		ImVec2 previewMax = ImVec2(previewMin.x + previewSize.x, previewMin.y + previewSize.y);

		ImVec2 previewLeftMax = ImVec2(previewMin.x + previewSize.x / 2.0f, previewMax.y);
		ImVec2 previewRightMin = ImVec2(previewMin.x + previewSize.x / 2.0f, previewMin.y);

		ImDrawList* drawList = ImGui::GetWindowDrawList();

#define FOREGROUND_COLOR IM_COL32(160, 160, 160, 255)

		if(m_asyncData.SteamVRInitialized)
		{
			vr::EVRCompositorError compError;

			compError = vr::VRCompositor()->GetMirrorTextureD3D11(vr::Eye_Left, m_d3d11Device.Get(), (void**)&m_mirrorSRVLeft);

			if (compError != vr::VRCompositorError_None)
			{
				g_logger->error("Settings Menu: Error getting mirror texture!");
			}
			else
			{
				drawList->AddImage((ImTextureID)m_mirrorSRVLeft, previewMin, previewLeftMax);
			}

			compError = vr::VRCompositor()->GetMirrorTextureD3D11(vr::Eye_Right, m_d3d11Device.Get(), (void**)&m_mirrorSRVRight);

			if (compError != vr::VRCompositorError_None)
			{
				g_logger->error("Settings Menu: Error getting mirror texture!");
			}
			else
			{
				drawList->AddImage((ImTextureID)m_mirrorSRVRight, previewRightMin, previewMax);
			}
		}

		drawList->AddRect(previewMin, previewMax, FOREGROUND_COLOR);


		float fontHalfSize = ImGui::GetFontSize() / 2.0f;

		float vertFrac = mainSettings.HeightFraction / (mainSettings.NumLights / 2);
		float vertRadius = mainSettings.HeightFraction * mainSettings.VerticalAreaSize / (mainSettings.NumLights / 2) / 2.0f;

		float curvatureFactor = mainSettings.Curvature * 1.0f / mainSettings.NumLights;
		float curvatureHalfway = mainSettings.CurvatureShape * (mainSettings.NumLights / 4.0f) + (mainSettings.NumLights / 4.0f) - 0.5f;

		drawList->PushClipRect(previewMin, previewLeftMax, true);

		for (int i = 0; i < mainSettings.NumLights / 2; i++)
		{
			int index = mainSettings.SwapLeftRight ?
				(mainSettings.BottomToTopLeft ? mainSettings.NumLights - 1 - i : i + mainSettings.NumLights / 2) :
				(mainSettings.BottomToTopLeft ? mainSettings.NumLights / 2 - 1 - i : i);

			float xOrigin = mainSettings.HorizontalOffset + curvatureFactor * pow(fabsf(i - curvatureHalfway), 2.0f);
			float yOrigin = vertFrac * (i + 0.5f) - mainSettings.VerticalOffset + (1.0f - mainSettings.HeightFraction) / 2.0f;

			float xMin = (xOrigin - mainSettings.WidthFraction / 2.0f) * previewSize.x * 0.5f + previewMin.x;
			float xMax = (xOrigin + mainSettings.WidthFraction / 2.0f) * previewSize.x * 0.5f + previewMin.x;

			float yMin = (yOrigin - vertRadius) * previewSize.y + previewMin.y;
			float yMax = (yOrigin + vertRadius) * previewSize.y + previewMin.y;

			drawList->AddRect(ImVec2(xMin, yMin), ImVec2(xMax, yMax), FOREGROUND_COLOR);
			std::string number = std::format("{}", index);
			drawList->AddText(ImVec2(xOrigin * previewSize.x * 0.5f + previewMin.x - fontHalfSize, yOrigin * previewSize.y + previewMin.y - fontHalfSize), FOREGROUND_COLOR, number.c_str());

			if (mainSettings.NumLights == 8 && i == 1 && bHasRing) { drawList->AddRect(ImVec2(xOrigin * previewSize.x * 0.5f + previewMin.x - 10, yMin - 5), ImVec2(xOrigin * previewSize.x * 0.5f + previewMin.x + 10, yMax + 5), IM_COL32(255, 255, 0, 255)); }
		}

		drawList->PopClipRect();

		drawList->PushClipRect(previewRightMin, previewMax, true);

		for (int i = 0; i < mainSettings.NumLights / 2; i++)
		{
			int index = mainSettings.SwapLeftRight ? 
				(mainSettings.BottomToTopRight ? mainSettings.NumLights / 2 - 1 - i : i) :
				(mainSettings.BottomToTopRight ? mainSettings.NumLights - 1 - i : i + mainSettings.NumLights / 2);

			float xOrigin = 1.0f - mainSettings.HorizontalOffset - curvatureFactor * pow(fabsf(i - curvatureHalfway), 2.0f);
			float yOrigin = vertFrac * (i + 0.5f) - mainSettings.VerticalOffset + (1.0f - mainSettings.HeightFraction) / 2.0f;

			float xMin = (xOrigin - mainSettings.WidthFraction / 2.0f) * previewSize.x * 0.5f + previewMin.x + previewSize.x * 0.5f;
			float xMax = (xOrigin + mainSettings.WidthFraction / 2.0f) * previewSize.x * 0.5f + previewMin.x + previewSize.x * 0.5f;

			float yMin = (yOrigin - vertRadius) * previewSize.y + previewMin.y;
			float yMax = (yOrigin + vertRadius) * previewSize.y + previewMin.y;

			drawList->AddRect(ImVec2(xMin, yMin), ImVec2(xMax, yMax), FOREGROUND_COLOR);
			std::string number = std::format("{}", index);
			drawList->AddText(ImVec2(xOrigin * previewSize.x * 0.5f + previewMin.x + previewSize.x * 0.5f - fontHalfSize, yOrigin * previewSize.y + previewMin.y - fontHalfSize), FOREGROUND_COLOR, number.c_str());
		}

		drawList->PopClipRect();

		ImGui::EndChild();

		IMGUI_BIG_SPACING;

		ImGui::BeginChild("GeometrySettingsLeft", ImVec2(ImGui::GetContentRegionAvail().x * 0.5f, 0));

		ImGui::InputInt("Number of Lights", &mainSettings.NumLights, 2);
		if (mainSettings.NumLights % 2) { mainSettings.NumLights -= 1; }
		if (mainSettings.NumLights < 0) { mainSettings.NumLights = 0; }

		IMGUI_BIG_SPACING;

		ImGui::Checkbox("Swap Left and Right", &mainSettings.SwapLeftRight);
		ImGui::Checkbox("Left Side - LEDs Bottom to Top", &mainSettings.BottomToTopLeft);
		ImGui::Checkbox("Right Side - LEDs Bottom to Top", &mainSettings.BottomToTopRight);
		if (mainSettings.NumLights == 8) { ImGui::Checkbox("Ring", &bHasRing); }

		ImGui::EndChild();

		ImGui::SameLine();

		ImGui::BeginChild("GeometrySettingsRight");

		ImGui::SliderFloat("Width", &mainSettings.WidthFraction, 0.0f, 1.0f, "%.2f");
		ImGui::SliderFloat("Height", &mainSettings.HeightFraction, 0.0f, 1.0f, "%.2f");
		

		IMGUI_BIG_SPACING;

		ImGui::SliderFloat("Horizontal Offset", &mainSettings.HorizontalOffset, 0.0f, 1.0f, "%.2f");
		ImGui::SliderFloat("Vertical Offset", &mainSettings.VerticalOffset, -1.0f, 1.0f, "%.2f");
		
		IMGUI_BIG_SPACING;

		ImGui::SliderFloat("Vertical Area Size", &mainSettings.VerticalAreaSize, 0.0f, 2.0f, "%.2f");
		ImGui::SliderFloat("Curvature", &mainSettings.Curvature, 0.0f, 1.0f, "%.2f");
		ImGui::SliderFloat("Curvature Shape", &mainSettings.CurvatureShape, -1.0f, 1.0f, "%.2f");

		ImGui::EndChild();

		ImGui::EndChild();
	}



	if (m_activeTab == TabColor)
	{
		ImGui::BeginChild("ColorPlot", ImVec2(0, 0), false);

		ImGui::Text("Color Adjustment");
		ImGui::Separator();
		ImGui::Spacing();

		ImGui::PushFont(m_smallFont);
		if (ImPlot::BeginPlot("Color Curves"))
		{
			ImPlot::SetupAxisLimits(ImAxis_X1, 0, 1, ImPlotCond_Always);
			ImPlot::SetupAxisLimits(ImAxis_Y1, 0, 1, ImPlotCond_Always);

				
			double xvals[256];
			double yvals[3][256];

			for (int i = 0; i < 256; i++)
			{
				xvals[i] = i / 256.0;

				double L, a, b, red, green, blue;

				LinearRGBtoLAB_D65(xvals[i], xvals[i], xvals[i], L, a, b);
				
				L = min(max((L - 50.0) * mainSettings.Contrast + 50.0, 0.0), 100.0);
				//L = min(max(L + (mainSettings.Brightness - 1.0) * 100.0, 0.0), 100.0);
				a *= mainSettings.Saturation;
				b *= mainSettings.Saturation;

				LABtoLinearRGB_D65(L, a, b, red, green, blue);

				red = min(max((std::pow((red - mainSettings.MinRed) * mainSettings.Brightness / (1.0 - mainSettings.MinRed), 1.0 / mainSettings.GammaRed) * mainSettings.MaxRed), 0.0), 1.0);
				yvals[0][i] = red;

				green = min(max((std::pow((green - mainSettings.MinGreen) * mainSettings.Brightness / (1.0 - mainSettings.MinGreen), 1.0 / mainSettings.GammaGreen) * mainSettings.MaxGreen), 0.0), 1.0);
				yvals[1][i] = green;

				blue = min(max((std::pow((blue - mainSettings.MinBlue) * mainSettings.Brightness / (1.0 - mainSettings.MinBlue), 1.0 / mainSettings.GammaBlue) * mainSettings.MaxBlue), 0.0), 1.0);
				yvals[2][i] = blue;
				
			}

			ImPlot::PushStyleColor(ImPlotCol_Line, ImVec4(0.8f, 0, 0, 1));
			ImPlot::PlotLine("Red", xvals, yvals[0], 256);
			ImPlot::PopStyleColor();
			ImPlot::PushStyleColor(ImPlotCol_Line, ImVec4(0, 0.8f, 0, 1));
			ImPlot::PlotLine("Green", xvals, yvals[1], 256);

			ImPlot::PopStyleColor();

			ImPlot::PushStyleColor(ImPlotCol_Line, ImVec4(0, 0, 0.8f, 1));
			ImPlot::PlotLine("Blue", xvals, yvals[2], 256);
			ImPlot::PopStyleColor();

			ImPlot::EndPlot();
		}
		ImGui::PopFont();

		IMGUI_BIG_SPACING;

		ImGui::BeginChild("ColorSettingsLeft", ImVec2(ImGui::GetContentRegionAvail().x * 0.5f, 0));

		ImGui::SliderFloat("Brightness", &mainSettings.Brightness, 0.0f, 2.0f, "%.2f");
		ImGui::SliderFloat("Saturation", &mainSettings.Saturation, 0.0f, 2.0f, "%.2f");
		ImGui::SliderFloat("Contrast", &mainSettings.Contrast, 0.0f, 2.0f, "%.2f");

		IMGUI_BIG_SPACING;

		ImGui::SliderFloat("Min Red", &mainSettings.MinRed, 0.0f, 1.0f, "%.2f");
		ImGui::SliderFloat("Min Green", &mainSettings.MinGreen, 0.0f, 1.0f, "%.2f");
		ImGui::SliderFloat("Min Blue", &mainSettings.MinBlue, 0.0f, 1.0f, "%.2f");

		IMGUI_BIG_SPACING;

		ImGui::BeginGroup();
		ImGui::Text("Preview");
		if (ImGui::RadioButton("Off", mainSettings.PreviewMode == 0)) { mainSettings.PreviewMode = 0; }
		ImGui::SameLine();
		if (ImGui::RadioButton("White", mainSettings.PreviewMode == 1)) { mainSettings.PreviewMode = 1; }
		ImGui::SameLine();
		if (ImGui::RadioButton("Red", mainSettings.PreviewMode == 2)) { mainSettings.PreviewMode = 2; }
		ImGui::SameLine();
		if (ImGui::RadioButton("Green", mainSettings.PreviewMode == 3)) { mainSettings.PreviewMode = 3; }
		ImGui::SameLine();
		if (ImGui::RadioButton("Blue", mainSettings.PreviewMode == 4)) { mainSettings.PreviewMode = 4; }
		ImGui::SliderFloat("Value", &mainSettings.PreviewValue, 0.0f, 1.0f, "%.2f");
		ImGui::EndGroup();

		ImGui::EndChild();
			
		ImGui::SameLine();

		ImGui::BeginChild("ColorSettingsRight");

		ImGui::SliderFloat("Gamma Red", &mainSettings.GammaRed, 0.1f, 4.0f, "%.2f");
		ImGui::SliderFloat("Gamma Green", &mainSettings.GammaGreen, 0.1f, 4.0f, "%.2f");
		ImGui::SliderFloat("Gamma Blue", &mainSettings.GammaBlue, 0.1f, 4.0f, "%.2f");

		IMGUI_BIG_SPACING;

		ImGui::SliderFloat("Max Red", &mainSettings.MaxRed, 0.0f, 1.0f, "%.2f");
		ImGui::SliderFloat("Max Green", &mainSettings.MaxGreen, 0.0f, 1.0f, "%.2f");
		ImGui::SliderFloat("Max Blue", &mainSettings.MaxBlue, 0.0f, 1.0f, "%.2f");

		ImGui::EndChild();

		ImGui::EndChild();
	}
	else
	{
		mainSettings.PreviewMode = 0;
	}



	if (m_activeTab == TabLog)
	{
		ImGui::BeginChild("Log#TabLog", ImVec2(0, 0), false);

		ImGui::PushFont(m_fixedFont);
		ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x);

		std::vector<std::string> logMessages = g_logRingbuffer->last_formatted();

		for (auto it = logMessages.begin(); it != logMessages.end(); it++)
		{
			ImGui::Text(it->c_str());
		}

		ImGui::PopTextWrapPos();
		ImGui::PopFont();

		ImGui::EndChild();
	}

	bool bGeometryUpdated = false;

	if (ImGui::IsAnyItemActive())
	{
		m_settingsManager->SettingsUpdated();

		if (m_activeTab == TabGeometry)
		{
			m_bGeometryTouched = true;
		}
	}
	else if (m_bGeometryTouched)
	{
		m_bGeometryTouched = false;
		bGeometryUpdated = true;
	}

	if (bReloadSystem)
	{
		SendMessage(m_windowHandle, WM_SETTINGS_UPDATED, 0, 0);
	}
	else if(bGeometryUpdated)
	{
		SendMessage(m_windowHandle, WM_SETTINGS_UPDATED, 1, 0);
	}

	if (bAppManifestUpdatePending)
	{
		SendMessage(m_windowHandle, WM_SETTINGS_UPDATED, 2, 0);
	}

	ImGui::End();

	ImGui::PopFont();
	ImGui::PopStyleVar(3);

	ImGui::Render();

	ID3D11RenderTargetView* rtv = m_d3d11RTV.Get();
	m_d3d11DeviceContext->OMSetRenderTargets(1, &rtv, NULL);
	const float clearColor[4] = { 0, 0, 0, 1 };
	m_d3d11DeviceContext->ClearRenderTargetView(m_d3d11RTV.Get(), clearColor);
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

	HRESULT hr = m_d3d11SwapChain->Present(1, 0);

	m_bMenuIsVisible = (hr != DXGI_STATUS_OCCLUDED);
}


extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT SettingsMenu::HandleWin32Events(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
	{
		return true;
	}

	switch (msg)
	{
	case WM_SIZE:
		if (wParam == SIZE_MINIMIZED)
		{
			return 0;
		}
		m_resizeWidth = (UINT)LOWORD(lParam);
		m_resizeHeight = (UINT)HIWORD(lParam);
		return 0;
	

	}
	return 0;
}


void SettingsMenu::SetupDX11()
{
	DXGI_SWAP_CHAIN_DESC swapchainDesc = {};
	swapchainDesc.BufferCount = 2;
	swapchainDesc.BufferDesc.Width = 0;
	swapchainDesc.BufferDesc.Height = 0;
	swapchainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	swapchainDesc.BufferDesc.RefreshRate.Numerator = 60;
	swapchainDesc.BufferDesc.RefreshRate.Denominator = 1;
	swapchainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	swapchainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapchainDesc.OutputWindow = m_windowHandle;
	swapchainDesc.SampleDesc.Count = 1;
	swapchainDesc.SampleDesc.Quality = 0;
	swapchainDesc.Windowed = TRUE;
	swapchainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

	std::vector<D3D_FEATURE_LEVEL> featureLevels = { D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0, D3D_FEATURE_LEVEL_11_1 };

	D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, featureLevels.data(), (UINT)featureLevels.size(), D3D11_SDK_VERSION, &swapchainDesc, &m_d3d11SwapChain, &m_d3d11Device, NULL, &m_d3d11DeviceContext);

	ComPtr<ID3D11Texture2D> backBuffer;
	m_d3d11SwapChain->GetBuffer(0, IID_PPV_ARGS(backBuffer.GetAddressOf()));
	m_d3d11Device->CreateRenderTargetView(backBuffer.Get(), nullptr, &m_d3d11RTV);
}
