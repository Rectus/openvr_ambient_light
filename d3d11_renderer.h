
#pragma once

#include "framework.h"
#include "structures.h"
#include "settings_manager.h"


class D3D11Renderer
{
public:
	D3D11Renderer(std::shared_ptr<SettingsManager> settingsManager);
	~D3D11Renderer();

	bool InitRenderer();
	bool Render(std::shared_ptr<LEDSampleData> ledData);
	const bool IsIntialized() { return m_bIsInitalized; }

protected:

	bool CreateBuffers(std::shared_ptr<LEDSampleData> ledData, uint32_t numTiles);

	std::shared_ptr<SettingsManager> m_settingsManager;

	bool m_bIsInitalized = false;

	ComPtr<ID3D11Device5> m_device;
	ComPtr<ID3D11DeviceContext4> m_deviceContext;

	ComPtr<ID3D11ComputeShader> m_gatherLightCS;
	ComPtr<ID3D11ComputeShader> m_combineLightCS;

	ID3D11ShaderResourceView *m_mirrorSRVLeft = nullptr;
	ID3D11ShaderResourceView *m_mirrorSRVRight = nullptr;

	ComPtr<ID3D11Buffer> m_csConstantBuffer;

	ComPtr<ID3D11Buffer> m_lightInputData;
	ComPtr<ID3D11ShaderResourceView> m_lightInputDataSRV;

	ComPtr<ID3D11Buffer> m_lightIntermediary;
	ComPtr<ID3D11UnorderedAccessView> m_lightIntermediaryUAV;

	ComPtr<ID3D11Buffer> m_lightOutputData;
	ComPtr<ID3D11UnorderedAccessView> m_lightOutputDataUAV;
	ComPtr<ID3D11Buffer> m_lightOutputDownload;

	ComPtr<ID3D11SamplerState> m_bilinearSampler;

	int m_numLEDs = 0;
	uint64_t m_frameIndex = 0;
};

