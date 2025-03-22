

#pragma once


struct AsyncData
{
	bool SteamVRInitialized = false;
	bool LightsConnected = false;
	bool OpenVRSampling = false;
	bool PreviewActive = false;
	bool InterfaceInitFailed = false;

	float FrameIntervalMS = 0;
	float RenderTimeMS = 0;
	float PresentTimeMS = 0;

	AsyncData()
	{

	}
};

