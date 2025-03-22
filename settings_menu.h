
#pragma once

#include "framework.h"
#include "settings_manager.h"
#include "async_data.h"
#include "imgui.h"


enum EMenuTab
{
	TabMain,
	TabDevice,
	TabGeometry,
	TabColor,
	TabLog
};

class SettingsMenu
{
public:

	SettingsMenu(std::shared_ptr<SettingsManager> settingsManager, HWND hWnd, AsyncData& asyncData);

	~SettingsMenu();

	void TickMenu();
	LRESULT HandleWin32Events(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:

	void SetupDX11();

	void TextDescription(const char* fmt, ...);
	void TextDescriptionSpaced(const char* fmt, ...);

	std::shared_ptr<SettingsManager> m_settingsManager;
	AsyncData& m_asyncData;

	HWND m_windowHandle;

	ComPtr<ID3D11Device> m_d3d11Device;
	ComPtr<ID3D11DeviceContext> m_d3d11DeviceContext;
	ComPtr<IDXGISwapChain> m_d3d11SwapChain;
	ComPtr<ID3D11RenderTargetView> m_d3d11RTV;

	bool m_bMenuIsVisible;
	EMenuTab m_activeTab;

	ImFont* m_mainFont;
	ImFont* m_largeFont;
	ImFont* m_smallFont;
	ImFont* m_fixedFont;

	uint32_t m_resizeWidth = 0;
	uint32_t m_resizeHeight = 0;

	bool m_bGeometryTouched = false;

	ID3D11ShaderResourceView* m_mirrorSRVLeft = nullptr;
	ID3D11ShaderResourceView* m_mirrorSRVRight = nullptr;
};

