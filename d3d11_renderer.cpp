

#include <d3dcompiler.h>
#include "external/renderdoc_app.h"

#include <dxgidebug.h>

#include "shaders/gather_light_cs.h"
#include "shaders/combine_light_cs.h"



#include "d3d11_renderer.h"


RENDERDOC_API_1_6_0* g_renderDocAPI = nullptr;


#define SET_DXGI_DEBUGNAME(object) \
constexpr char object##_Name[] = #object; \
object->SetPrivateData(WKPDID_D3DDebugObjectName, sizeof(object##_Name), object##_Name);

struct alignas(16) CSConstantBuffer
{
	uint32_t frameSize[2];
	uint32_t numLEDs;
	uint32_t LEDStartIndex;
};


static inline uint32_t Align(const uint32_t value, const uint32_t alignment)
{
	return (value + alignment - 1) & ~(alignment - 1);
}


D3D11Renderer::D3D11Renderer(std::shared_ptr<SettingsManager> settingsManager)
	:m_settingsManager(settingsManager)
{


}

D3D11Renderer::~D3D11Renderer()
{
	IDXGIDebug* debugDevice;
	if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&debugDevice))))
	{
		debugDevice->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
		debugDevice->Release();
	}
	
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


bool D3D11Renderer::InitRenderer()
{
	if (HMODULE module = GetModuleHandleA("renderdoc.dll"))
	{
		pRENDERDOC_GetAPI RENDERDOC_GetAPI = (pRENDERDOC_GetAPI)GetProcAddress(module, "RENDERDOC_GetAPI");
		int ret = RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_6_0, (void**)&g_renderDocAPI);
		assert(ret == 1);
	}


	IDXGIFactory1* factory = nullptr;
	if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)(&factory))))
	{
		g_logger->error("CreateDXGIFactory failure!");
		return false;
	}

	int32_t adapterIndex;
	vr::VRSystem()->GetDXGIOutputInfo(&adapterIndex);
	IDXGIAdapter1* adapter = nullptr;

	if (factory->EnumAdapters1(adapterIndex, &adapter) == DXGI_ERROR_NOT_FOUND)
	{
		factory->Release();
		return false;
	}

	factory->Release();

	std::vector<D3D_FEATURE_LEVEL> featureLevels = { D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0, D3D_FEATURE_LEVEL_11_1 };

	ComPtr<ID3D11Device> device;
	ComPtr <ID3D11DeviceContext> deviceContext;

	HRESULT res = D3D11CreateDevice(adapter, D3D_DRIVER_TYPE_UNKNOWN, NULL, 0, featureLevels.data(), (uint32_t)featureLevels.size(), D3D11_SDK_VERSION, &device, NULL, &deviceContext);

	adapter->Release();

	if (FAILED(res))
	{
		g_logger->error("D3D11CreateDevice failure: 0x%x", res);
		return false;
	}

	if (FAILED(device->QueryInterface(__uuidof(ID3D11Device5), (void**)m_device.GetAddressOf())))
	{
		g_logger->error("Querying ID3D11Device5 failure!");
		return false;
	}

	if (FAILED(deviceContext->QueryInterface(__uuidof(ID3D11DeviceContext4), (void**)m_deviceContext.GetAddressOf())))
	{
		g_logger->error("Querying ID3D11DeviceContext4 failure!");
		return false;
	}


	if (FAILED(m_device->CreateComputeShader(g_gatherLightCS, sizeof(g_gatherLightCS), nullptr, &m_gatherLightCS)))
	{
		g_logger->error("g_gatherLightCS creation failure!");
		return false;
	}
	SET_DXGI_DEBUGNAME(m_gatherLightCS);

	if (FAILED(m_device->CreateComputeShader(g_combineLightCS, sizeof(g_combineLightCS), nullptr, &m_combineLightCS)))
	{
		g_logger->error("g_gatherLightCS creation failure!");
		return false;
	}
	SET_DXGI_DEBUGNAME(m_combineLightCS);

	D3D11_SAMPLER_DESC samplerDesc = {};
	samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
	samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
	if (FAILED(m_device->CreateSamplerState(&samplerDesc, m_bilinearSampler.GetAddressOf())))
	{
		g_logger->error("m_bilinearSampler creation failure!\n");
		return false;
	}
	SET_DXGI_DEBUGNAME(m_bilinearSampler);

	D3D11_BUFFER_DESC bufferDesc = {};
	bufferDesc.ByteWidth = Align(sizeof(CSConstantBuffer), 16);
	bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;


	if (FAILED(m_device->CreateBuffer(&bufferDesc, nullptr, &m_csConstantBuffer)))
	{
		g_logger->error("m_csConstantBuffer creation failure!");
		return false;
	}
	SET_DXGI_DEBUGNAME(m_csConstantBuffer)

	m_bIsInitalized = true;
	return true;
}

bool D3D11Renderer::Render(std::shared_ptr<LEDSampleData> ledData)
{
	{
		if (!m_settingsManager->GetSettings_Main().SkipMirrorTextureRelease)
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

		vr::EVRCompositorError compError;

		compError = vr::VRCompositor()->GetMirrorTextureD3D11(vr::Eye_Left, m_device.Get(), (void**)&m_mirrorSRVLeft);

		if (compError != vr::VRCompositorError_None)
		{
			g_logger->error("Error getting mirror texture!");
			return false;
		}

		compError = vr::VRCompositor()->GetMirrorTextureD3D11(vr::Eye_Right, m_device.Get(), (void**)&m_mirrorSRVRight);

		if (compError != vr::VRCompositorError_None)
		{
			g_logger->error("Error getting mirror texture!");
			vr::VRCompositor()->ReleaseMirrorTextureD3D11(m_mirrorSRVLeft);
			m_mirrorSRVLeft = nullptr;
			return false;
		}
	}

	uint32_t frameWidth = 1;
	uint32_t frameHeight = 1;

	{
		ComPtr<ID3D11Resource> res;
		ComPtr <ID3D11Texture2D> tex;

		m_mirrorSRVLeft->GetResource(res.GetAddressOf());

		if (res.Get() && FAILED(res->QueryInterface(IID_PPV_ARGS(tex.GetAddressOf()))))
		{
			vr::VRCompositor()->ReleaseMirrorTextureD3D11(m_mirrorSRVLeft);
			vr::VRCompositor()->ReleaseMirrorTextureD3D11(m_mirrorSRVRight);
			m_mirrorSRVLeft = nullptr;
			m_mirrorSRVRight = nullptr;

			g_logger->error("Querying mirror texture resource failure!");
			return false;
		}
		D3D11_TEXTURE2D_DESC desc = {};
		tex->GetDesc(&desc);

		frameWidth = desc.Width;
		frameHeight = desc.Height;
	}


	uint32_t numTiles = 0;
	uint32_t maxTilesX = 1;
	uint32_t maxTilesY = 1;

	for (int i = 0; i < ledData->NumLEDs; i++)
	{
		uint32_t xMin = (uint32_t)floor(ledData->sampleAreas[i].xMin * frameWidth);
		uint32_t yMin = (uint32_t)floor(ledData->sampleAreas[i].yMin * frameHeight);
		uint32_t xMax = (uint32_t)ceil(ledData->sampleAreas[i].xMax * frameWidth);
		uint32_t yMax = (uint32_t)ceil(ledData->sampleAreas[i].yMax * frameHeight);

		uint32_t numTilesX = (uint32_t)ceil((xMax - xMin) / 32);
		uint32_t numTilesY = (uint32_t)ceil((yMax - yMin) / 32);

		numTiles += numTilesX * numTilesY;

		maxTilesX = maxTilesX < numTilesX ? numTilesX : maxTilesX;
		maxTilesY = maxTilesY < numTilesY ? numTilesY : maxTilesY;
	}

	if (m_numLEDs != ledData->NumLEDs || ledData->IsInputUpdated)
	{
		if (!CreateBuffers(ledData, numTiles))
		{
			vr::VRCompositor()->ReleaseMirrorTextureD3D11(m_mirrorSRVLeft);
			vr::VRCompositor()->ReleaseMirrorTextureD3D11(m_mirrorSRVRight);
			m_mirrorSRVLeft = nullptr;
			m_mirrorSRVRight = nullptr;
			return false;
		}

		m_numLEDs = ledData->NumLEDs;
		ledData->IsInputUpdated = false;
	}

	m_frameIndex++;

	if (g_renderDocAPI && m_frameIndex == 100)
	{
		g_renderDocAPI->StartFrameCapture(m_device.Get(), NULL);
	}

	m_deviceContext->CSSetShader(m_gatherLightCS.Get(), nullptr, 0);

	ID3D11ShaderResourceView* SRVs[2] = { m_mirrorSRVLeft , m_lightInputDataSRV.Get() };
	m_deviceContext->CSSetShaderResources(0, 2, SRVs);
	ID3D11UnorderedAccessView* UAVs[2] = { m_lightIntermediaryUAV.Get() , m_lightOutputDataUAV.Get()};
	m_deviceContext->CSSetUnorderedAccessViews(0, 2, UAVs, nullptr);
	m_deviceContext->CSSetConstantBuffers(0, 1, m_csConstantBuffer.GetAddressOf());
	m_deviceContext->CSSetSamplers(0, 1, m_bilinearSampler.GetAddressOf());

	CSConstantBuffer csBuffer = {};
	csBuffer.frameSize[0] = frameWidth;
	csBuffer.frameSize[1] = frameHeight;
	csBuffer.numLEDs = ledData->NumLEDs / 2;
	csBuffer.LEDStartIndex = 0;

	m_deviceContext->UpdateSubresource(m_csConstantBuffer.Get(), 0, nullptr, &csBuffer, 0, 0);

	m_deviceContext->Dispatch(maxTilesX, maxTilesY, ledData->NumLEDs / 2);
	m_deviceContext->CSSetShader(m_combineLightCS.Get(), nullptr, 0);
	m_deviceContext->Dispatch(1, 1, ledData->NumLEDs / 2);


	SRVs[0] = m_mirrorSRVRight;
	m_deviceContext->CSSetShaderResources(0, 2, SRVs);

	csBuffer.LEDStartIndex = ledData->NumLEDs / 2;
	m_deviceContext->UpdateSubresource(m_csConstantBuffer.Get(), 0, nullptr, &csBuffer, 0, 0);


	m_deviceContext->CSSetShader(m_gatherLightCS.Get(), nullptr, 0);
	m_deviceContext->Dispatch(maxTilesX, maxTilesY, ledData->NumLEDs / 2);
	m_deviceContext->CSSetShader(m_combineLightCS.Get(), nullptr, 0);
	m_deviceContext->Dispatch(1, 1, ledData->NumLEDs / 2);


	m_deviceContext->CSSetShader(nullptr, nullptr, 0);
	ID3D11ShaderResourceView* nullSRVs[2] = { nullptr, nullptr };
	m_deviceContext->CSSetShaderResources(0, 2, nullSRVs);
	ID3D11UnorderedAccessView* nullUAVs[2] = { nullptr, nullptr };
	m_deviceContext->CSSetUnorderedAccessViews(0, 2, nullUAVs, nullptr);
	ID3D11Buffer* nullBuffer = nullptr;
	m_deviceContext->CSSetConstantBuffers(0, 1, &nullBuffer);
	ID3D11SamplerState* nullSampler = nullptr;
	m_deviceContext->CSSetSamplers(0, 1, &nullSampler);
	


	m_deviceContext->CopyResource(m_lightOutputDownload.Get(), m_lightOutputData.Get());

	D3D11_MAPPED_SUBRESOURCE resource;
	HRESULT result = m_deviceContext->Map(m_lightOutputDownload.Get(), 0, D3D11_MAP_READ, 0, &resource);

	if (SUCCEEDED(result))
	{
		ledData->sampleOutput.resize(m_numLEDs);
		memcpy(ledData->sampleOutput.data(), resource.pData, sizeof(LEDShaderOutput) * m_numLEDs);

		m_deviceContext->Unmap(m_lightOutputDownload.Get(), 0);
	}


	if (g_renderDocAPI && m_frameIndex == 100)
	{
		g_renderDocAPI->EndFrameCapture(m_device.Get(), NULL);
	}

	return SUCCEEDED(result);
}


bool D3D11Renderer::CreateBuffers(std::shared_ptr<LEDSampleData> ledData, uint32_t numTiles)
{
	{
		D3D11_BUFFER_DESC bufferDesc = {};
		bufferDesc.ByteWidth = sizeof(LEDSampleArea) * ledData->NumLEDs;
		bufferDesc.StructureByteStride = sizeof(LEDSampleArea);
		bufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
		bufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;

		D3D11_SUBRESOURCE_DATA data = {};
		data.pSysMem = ledData->sampleAreas.data();

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		srvDesc.BufferEx.FirstElement = 0;
		srvDesc.BufferEx.NumElements = ledData->NumLEDs;

		if (FAILED(m_device->CreateBuffer(&bufferDesc, &data, &m_lightInputData)))
		{
			g_logger->error("m_lightInputData creation failure!");
			return false;
		}

		if (FAILED(m_device->CreateShaderResourceView(m_lightInputData.Get(), &srvDesc, &m_lightInputDataSRV)))
		{
			g_logger->error("m_lightInputDataSRV creation error!");
			return false;
		}

		SET_DXGI_DEBUGNAME(m_lightInputData)
		SET_DXGI_DEBUGNAME(m_lightInputDataSRV)
	}

	{
		
		D3D11_BUFFER_DESC bufferDesc = {};
		bufferDesc.ByteWidth = sizeof(uint32_t) * numTiles * 3;
		bufferDesc.Usage = D3D11_USAGE_DEFAULT;
		bufferDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;

		D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = DXGI_FORMAT_R32_UINT;
		uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		uavDesc.Buffer.FirstElement = 0;
		uavDesc.Buffer.NumElements = numTiles * 3;

		if (FAILED(m_device->CreateBuffer(&bufferDesc, nullptr, &m_lightIntermediary)))
		{
			g_logger->error("m_lightIntermediary creation failure!");
			return false;
		}

		if (FAILED(m_device->CreateUnorderedAccessView(m_lightIntermediary.Get(), &uavDesc, &m_lightIntermediaryUAV)))
		{
			g_logger->error("m_lightIntermediaryUAV creation error!");
			return false;
		}

		SET_DXGI_DEBUGNAME(m_lightIntermediary)
		SET_DXGI_DEBUGNAME(m_lightIntermediaryUAV)
	}

	{
		D3D11_BUFFER_DESC bufferDesc = {};
		bufferDesc.ByteWidth = sizeof(LEDShaderOutput) * ledData->NumLEDs;
		bufferDesc.StructureByteStride = sizeof(LEDShaderOutput);
		bufferDesc.Usage = D3D11_USAGE_DEFAULT;
		bufferDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
		bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;

		D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = DXGI_FORMAT_UNKNOWN;
		uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		uavDesc.Buffer.FirstElement = 0;
		uavDesc.Buffer.NumElements = ledData->NumLEDs;

		if (FAILED(m_device->CreateBuffer(&bufferDesc, nullptr, &m_lightOutputData)))
		{
			g_logger->error("m_lightOutputData creation failure!");
			return false;
		}

		if (FAILED(m_device->CreateUnorderedAccessView(m_lightOutputData.Get(), &uavDesc, &m_lightOutputDataUAV)))
		{
			g_logger->error("m_lightOutputDataUAV creation error!");
			return false;
		}

		bufferDesc.Usage = D3D11_USAGE_STAGING;
		bufferDesc.BindFlags = 0;
		bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

		if (FAILED(m_device->CreateBuffer(&bufferDesc, nullptr, &m_lightOutputDownload)))
		{
			g_logger->error("m_lightOutputData creation failure!");
			return false;
		}

		SET_DXGI_DEBUGNAME(m_lightOutputData)
		SET_DXGI_DEBUGNAME(m_lightOutputDataUAV)
		SET_DXGI_DEBUGNAME(m_lightOutputDownload)		
	}

	return true;
}
