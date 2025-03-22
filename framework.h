
#pragma once

#include "targetver.h"

#define WIN32_LEAN_AND_MEAN 
#include <windows.h>
#include <wrl.h>
using Microsoft::WRL::ComPtr;
#include <winbase.h>
#include <unknwn.h>
#include <fileapi.h>
#include <profileapi.h>

#include <d3d11_4.h>
#include <dxgi1_4.h>

#include <stdlib.h>
#include <stdio.h>
#include <malloc.h>
#include <memory.h>
#include <memory>
#include <tchar.h>
#include <vector>
#include <cassert>
#include <thread>
#include <queue>

#include "openvr.h"

#define SPDLOG_WCHAR_TO_UTF8_SUPPORT
#include "spdlog/spdlog.h"
#include "spdlog/sinks/ringbuffer_sink.h"

extern std::shared_ptr<spdlog::logger> g_logger;
extern std::shared_ptr<spdlog::sinks::ringbuffer_sink_mt> g_logRingbuffer;

#define WM_TRAYMESSAGE (WM_USER + 1)
#define WM_SETTINGS_UPDATED (WM_USER + 2)
#define WM_MENU_QUIT (WM_USER + 3)