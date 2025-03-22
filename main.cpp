


#include "framework.h"
#include <shellapi.h>
#include <shlobj_core.h>
#include <pathcch.h>
#include <dwmapi.h>
#include <Uxtheme.h>
#include "ambient_light_sampler.h"
#include "settings_manager.h"
#include "settings_menu.h"
#include "async_data.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/dup_filter_sink.h"
#include "spdlog/sinks/msvc_sink.h"
#include <filesystem>
#include "main.h"


#define APP_VERSION "1.0.0"

#define MAX_LOADSTRING 100

#define WINDOW_WIDTH 1024
#define WINDOW_HEIGHT 768

// Directory under AppData to write config.
#define SETTINGS_FILE_DIR L"\\OpenVR Ambient Light\\"
#define SETTINGS_FILE_NAME L"settings.ini"

#define LOG_FILE_DIR L"OpenVR Ambient Light"
#define LOG_FILE_NAME L"log.txt"

#define OPENVR_APP_KEY "no_vendor.openvr_ambient_light"
#define VR_MANIFEST_FILE_NAME "openvr_ambient_light.vrmanifest"

#define MUTEX_APP_KEY L"Global\\openvr_ambient_light"

std::unique_ptr<AmbientLightSampler> g_lightSampler;
std::shared_ptr<SettingsManager> g_settingsManager;
std::unique_ptr<SettingsMenu> g_settingsMenu;

std::shared_ptr<spdlog::logger> g_logger;
std::shared_ptr<spdlog::sinks::ringbuffer_sink_mt> g_logRingbuffer;

AsyncData g_asyncData;

HINSTANCE g_hInstance;
HANDLE g_instanceMutex;
HWND g_hSettingsWindow;
HICON g_iconOn;
HICON g_iconOff;
bool g_bIsSettingsWindowShown = false;
bool g_bIsDoubleClicking = false;
std::atomic_bool g_bIsPainting = false;
POINT g_trayCursorPos;
INT_PTR g_trayClickTimer = NULL;
WCHAR g_titleString[MAX_LOADSTRING];
WCHAR g_windowClass[MAX_LOADSTRING];
bool g_bExitOnClose = false;
bool g_bRun = true;


bool TryInitSteamVRAndSampler();
void UpdateOpenVRAppManifest(bool bInstall = false, bool bUninstall = false);
ATOM RegisterSettingsWindowClass();
bool InitSettingsWindow();
bool AddTrayIcon();
bool ModifyTrayIcon();
void RemoveTrayIcon();
void DispatchSettingsUpdated(int updateType);
LRESULT CALLBACK ProcessWindowMessages(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK About(HWND, UINT, WPARAM, LPARAM);



int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int       nCmdShow)
{
    g_hInstance = hInstance;

    g_iconOn = LoadIconW(g_hInstance, MAKEINTRESOURCE(IDI_ICON1_ON));
    g_iconOff = LoadIconW(g_hInstance, MAKEINTRESOURCE(IDI_ICON2_OFF));
    LoadStringW(g_hInstance, IDS_APP_TITLE, g_titleString, MAX_LOADSTRING);
    LoadStringW(g_hInstance, IDC_OPENVRAMBIENTLIGHT, g_windowClass, MAX_LOADSTRING);

    bool bStartWindow = true;
    bool bDoInstall = false;
    bool bDoUninstall = false;
    int numArgs;
    LPWSTR* arglist = CommandLineToArgvW(GetCommandLineW(), &numArgs);

    for (int i = 0; i < numArgs; i++)
    {
        if (_wcsnicmp(arglist[i], L"--install", 9) == 0)
        {
            bDoInstall = true;
        }
        if (_wcsnicmp(arglist[i], L"--uninstall", 11) == 0)
        {
            bDoUninstall = true;
        }
        if (_wcsnicmp(arglist[i], L"--minimized", 11) == 0)
        {
            bStartWindow = false;
        }
        if (_wcsnicmp(arglist[i], L"--exitonclose", 13) == 0)
        {
            g_bExitOnClose = true;
        }
    }

    // Set a mutex to prevent multiple instances of the application.
    {
        
        g_instanceMutex = CreateMutexW(NULL, TRUE, MUTEX_APP_KEY);
        if (GetLastError() == ERROR_ALREADY_EXISTS)
        {
            HWND window = FindWindow(NULL, g_titleString);
            if (bStartWindow && window != NULL)
            {
                SendMessage(window, WM_COMMAND, IDM_TRAYOPEN, 0);
                SetForegroundWindow(window);
                ShowWindow(window, SW_RESTORE);
            }
            return false;
        }
    }

    {
        std::shared_ptr<spdlog::sinks::dup_filter_sink_mt> duplicateFilter = std::make_shared<spdlog::sinks::dup_filter_sink_mt>(std::chrono::minutes(10));

        g_logRingbuffer = std::make_shared<spdlog::sinks::ringbuffer_sink_mt>(2000);
        duplicateFilter->add_sink(g_logRingbuffer);

        PWSTR localAppDataPath;

        SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, NULL, &localAppDataPath);
        std::string logFileName = (std::filesystem::path(localAppDataPath) / LOG_FILE_DIR / LOG_FILE_NAME).string();
        duplicateFilter->add_sink(std::make_shared<spdlog::sinks::basic_file_sink_mt>(logFileName, true));
        duplicateFilter->add_sink(std::make_shared<spdlog::sinks::msvc_sink_mt>());

        g_logger = std::make_shared<spdlog::logger>("openvr_ambient_light", duplicateFilter);
        g_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");

        g_logger->info("Starting OpenVR Ambient Light version {}", APP_VERSION);
        g_logger->info("Logging to {}", logFileName);
    }

    {
        PWSTR path;
        wchar_t filePath[MAX_PATH + 1] = {};

        SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, NULL, &path);
        lstrcpyW((PWSTR)filePath, path);
        PathCchAppend((PWSTR)filePath, MAX_PATH, SETTINGS_FILE_DIR);
        CreateDirectoryW((PWSTR)filePath, NULL);
        PathCchAppend((PWSTR)filePath, MAX_PATH, SETTINGS_FILE_NAME);

        g_logger->info(L"Using settings file {}", filePath);
        g_settingsManager = std::make_shared<SettingsManager>(filePath);
        g_settingsManager->ReadSettingsFile();
    }

    if (bDoInstall || bDoUninstall)
    {
        vr::EVRInitError initError;

        vr::VR_Init(&initError, vr::VRApplication_Utility, nullptr);

        if (initError != vr::VRInitError_None)
        {
            g_logger->error("SteamVR not initialized: {}", vr::VR_GetVRInitErrorAsEnglishDescription(initError));
            return initError;
        }
        UpdateOpenVRAppManifest(bDoInstall, bDoUninstall);
        vr::VR_Shutdown();
        return TRUE;
    }
    
    RegisterSettingsWindowClass();

    if (!InitSettingsWindow())
    {
        g_logger->error("Failed to create application window!");
        return FALSE;
    }

    g_settingsManager->GetSettings_Main().EnableLights = g_settingsManager->GetSettings_Main().EnableLightsOnStartup;

    if (g_settingsManager->GetSettings_Main().EnableLights)
    {
        TryInitSteamVRAndSampler();
    }

    if (bStartWindow)
    {
        g_bIsSettingsWindowShown = true;
        ShowWindow(g_hSettingsWindow, nCmdShow);
        UpdateWindow(g_hSettingsWindow);
    }

    HACCEL hAccelTable = LoadAccelerators(g_hInstance, MAKEINTRESOURCE(IDC_OPENVRAMBIENTLIGHT));

    g_settingsMenu = std::make_unique<SettingsMenu>(g_settingsManager, g_hSettingsWindow, g_asyncData);


    MSG msg = {};
    vr::VREvent_t event;

    // Main message loop:
    while (g_bRun)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }

        if (g_asyncData.SteamVRInitialized && vr::VRSystem()->PollNextEvent(&event, sizeof(event)))
        {
            if (event.eventType == vr::VREvent_Quit)
            {
                vr::VRSystem()->AcknowledgeQuit_Exiting();
                g_logger->info("SteamVR telling application to quit");
                RemoveTrayIcon();
                PostQuitMessage(0);
                g_bRun = false;
            }
        }

        std::this_thread::yield();
    }

    g_logger->info("Shutting down...");

    g_settingsMenu.reset();

    if (g_lightSampler.get())
    {
        g_lightSampler.reset();
    }

    if (g_asyncData.SteamVRInitialized)
    {
        vr::VR_Shutdown();
    }

    return (int) msg.wParam;
}


bool TryInitSteamVRAndSampler()
{
    vr::EVRInitError initError;

    vr::VR_Init(&initError, vr::VRApplication_Background, nullptr);

    if (initError != vr::VRInitError_None)
    {
        g_logger->warn("SteamVR not initialized: {}", vr::VR_GetVRInitErrorAsEnglishDescription(initError));
        return false;
    }
    else
    {
        UpdateOpenVRAppManifest();
        g_asyncData.SteamVRInitialized = true;
        g_lightSampler = std::make_unique<AmbientLightSampler>(g_settingsManager, g_asyncData);
        g_lightSampler->InitSampler();
        return true;
    }
}

void UpdateOpenVRAppManifest(bool bInstall, bool bUninstall)
{
    Settings_Main& mainSettings = g_settingsManager->GetSettings_Main();

    vr::IVRApplications* vrApps = vr::VRApplications();

    vr::EVRApplicationError error;

    wchar_t filePath[MAX_PATH + 1];
    GetModuleFileNameW(NULL, filePath, MAX_PATH);
    std::string exeName = std::filesystem::path(filePath).string();
    std::string fileName = std::filesystem::path(filePath).replace_filename(VR_MANIFEST_FILE_NAME).string();

    bool bForceUpdateManifest = false;

    if (vrApps->IsApplicationInstalled(OPENVR_APP_KEY)) // Check if in correct directory
    {
        char comparePath[MAX_PATH + 1];
        vrApps->GetApplicationPropertyString(OPENVR_APP_KEY, vr::VRApplicationProperty_BinaryPath_String, comparePath, MAX_PATH, &error);
        if (error != vr::VRApplicationError_None)
        {
            g_logger->error("Failed to get installed manifest path, updating: {}", vrApps->GetApplicationsErrorNameFromEnum(error));
            bForceUpdateManifest = true;
        }
        else if (strncmp(exeName.c_str(), comparePath, exeName.size()) != 0)
        {
            g_logger->warn("Application manifest is in wrong location, updating: {}", comparePath);
            bForceUpdateManifest = true;
        }
    }

    if ((!vrApps->IsApplicationInstalled(OPENVR_APP_KEY) || bForceUpdateManifest) && !bUninstall)
    {
        error = vrApps->AddApplicationManifest(fileName.c_str());

        if (error != vr::VRApplicationError_None)
        {
            g_logger->error("Failed to add application to SteamVR manifest: {}", vrApps->GetApplicationsErrorNameFromEnum(error));
            mainSettings.StartWithSteamVR = false;
            return;
        }

        g_logger->info("Added application to SteamVR manifest: {}", fileName);

        if (bInstall)
        {
            return;
        }
    }
    else if (vrApps->IsApplicationInstalled(OPENVR_APP_KEY) && bUninstall)
    {
        error = vrApps->RemoveApplicationManifest(fileName.c_str());

        if (error != vr::VRApplicationError_None)
        {
            g_logger->error("Failed to remove application from SteamVR manifest: {}", vrApps->GetApplicationsErrorNameFromEnum(error));
            return;
        }
        mainSettings.StartWithSteamVR = false;
        g_logger->info("Removed application from SteamVR manifest: {}", fileName);
    }
    else if (vrApps->GetApplicationAutoLaunch(OPENVR_APP_KEY) != mainSettings.StartWithSteamVR && !bUninstall)
    {
        error = vrApps->SetApplicationAutoLaunch(OPENVR_APP_KEY, mainSettings.StartWithSteamVR);

        if (error != vr::VRApplicationError_None)
        {
            g_logger->error("Failed to change autolaunch setting: {}", vrApps->GetApplicationsErrorNameFromEnum(error));
            mainSettings.StartWithSteamVR = false;
            return;
        }

        g_logger->info("Autolaunch {}", mainSettings.StartWithSteamVR ? "enabled" : "disabled");
    }
    if (bInstall)
    {
        g_logger->info("Application already in manifest.");
    }
}


void DispatchSettingsUpdated(int updateType)
{
    if (updateType == 1)
    {
        if (g_lightSampler.get())
        {
            g_lightSampler->SetGeometryUpdated();
        }
    }
    else if (updateType == 2)
    {
        if (g_asyncData.SteamVRInitialized)
        {
            UpdateOpenVRAppManifest(false);
        }
    }
    else
    {
        ModifyTrayIcon();
        HICON icon = g_settingsManager->GetSettings_Main().EnableLights ? g_iconOn : g_iconOff;
        SendMessage(g_hSettingsWindow, WM_SETICON, ICON_BIG, (LPARAM)icon);
        SendMessage(g_hSettingsWindow, WM_SETICON, ICON_BIG, (LPARAM)icon);

        if (g_settingsManager->GetSettings_Main().EnableLights)
        {
            if (g_lightSampler.get()) // Reinit sample on settings changes
            {
                g_lightSampler->InitSampler();
            }
            else
            {
                g_logger->info("Enabling lights...");

                if (!g_asyncData.SteamVRInitialized && !TryInitSteamVRAndSampler())
                {
                     return;
                }

                g_lightSampler.reset();
                g_lightSampler = std::make_unique<AmbientLightSampler>(g_settingsManager, g_asyncData);
                g_lightSampler->InitSampler();
            }
        }
        else
        {
            g_logger->info("Disabling lights...");

            if (g_lightSampler.get())
            {
                g_lightSampler.reset();
                g_lightSampler = nullptr;
            }
        }
    }
}


ATOM RegisterSettingsWindowClass()
{
    WNDCLASSEXW wcex = {};

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = ProcessWindowMessages;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = g_hInstance;
    wcex.hIcon          = g_iconOn;
    //wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)GetStockObject(BLACK_BRUSH);
    //wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_OPENVRAMBIENTLIGHT);
    wcex.lpszClassName  = g_windowClass;
    wcex.hIconSm        = g_iconOn;

    return RegisterClassExW(&wcex);
}

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

bool InitSettingsWindow()
{
    g_hSettingsWindow = CreateWindowW(g_windowClass, g_titleString, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, 0, WINDOW_WIDTH, WINDOW_HEIGHT, nullptr, nullptr, g_hInstance, nullptr);

    if (!g_hSettingsWindow)
    {
        return false;
    }

    // Hack for enabling dark mode menus in Win32.
    // Mostly from: https://gist.github.com/rounk-ctrl/b04e5622e30e0d62956870d5c22b7017

    HMODULE uxtheme = LoadLibraryExW(L"uxtheme.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);

    if (uxtheme)
    {
        enum class PreferredAppMode
        {
            Default,
            AllowDark,
            ForceDark,
            ForceLight,
            Max
        };

        using fnShouldAppsUseDarkMode = bool (WINAPI*)(); // ordinal 132
        using fnAllowDarkModeForWindow = bool (WINAPI*)(HWND hWnd, bool allow); // ordinal 133
        using fnSetPreferredAppMode = PreferredAppMode(WINAPI*)(PreferredAppMode appMode); // ordinal 135
        using fnFlushMenuThemes = void (WINAPI*)(); // ordinal 136

        fnShouldAppsUseDarkMode ShouldAppsUseDarkMode = (fnShouldAppsUseDarkMode)GetProcAddress(uxtheme, MAKEINTRESOURCEA(132));
        fnSetPreferredAppMode SetPreferredAppMode = (fnSetPreferredAppMode)GetProcAddress(uxtheme, MAKEINTRESOURCEA(135));
        fnAllowDarkModeForWindow AllowDarkModeForWindow = (fnAllowDarkModeForWindow)GetProcAddress(uxtheme, MAKEINTRESOURCEA(133));
        fnFlushMenuThemes FlushMenuThemes = (fnFlushMenuThemes)GetProcAddress(uxtheme, MAKEINTRESOURCEA(136));

        if (ShouldAppsUseDarkMode)
        {
            SetPreferredAppMode(PreferredAppMode::AllowDark);

            SetWindowTheme(g_hSettingsWindow, L"Explorer", nullptr);
            AllowDarkModeForWindow(g_hSettingsWindow, true);
            SendMessageW(g_hSettingsWindow, WM_THEMECHANGED, 0, 0);
            FlushMenuThemes();

            BOOL value = TRUE;
            DwmSetWindowAttribute(g_hSettingsWindow, DWMWA_USE_IMMERSIVE_DARK_MODE, &value, sizeof(value));
        }
        FreeLibrary(uxtheme);
    }


    if (!AddTrayIcon())
    {
        return false;
    }

   return true;
}


bool AddTrayIcon()
{
    NOTIFYICONDATA iconData = {};

    iconData.cbSize = sizeof(NOTIFYICONDATA);
    iconData.hWnd = g_hSettingsWindow;
    iconData.uID = IDI_ICON1_ON;
    iconData.uVersion = NOTIFYICON_VERSION;
    iconData.hIcon = g_settingsManager->GetSettings_Main().EnableLights ? g_iconOn : g_iconOff;
    LoadString(g_hInstance, IDS_APP_TITLE, iconData.szTip, 128);
    iconData.uCallbackMessage = WM_TRAYMESSAGE;
    iconData.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;

    if (!Shell_NotifyIcon(NIM_ADD, &iconData))
    {
        return false;
    }

    return true;
}

bool ModifyTrayIcon()
{
    NOTIFYICONDATA iconData = {};

    iconData.cbSize = sizeof(NOTIFYICONDATA);
    iconData.hWnd = g_hSettingsWindow;
    iconData.uID = IDI_ICON1_ON;
    iconData.uVersion = NOTIFYICON_VERSION;
    iconData.hIcon = g_settingsManager->GetSettings_Main().EnableLights ? g_iconOn : g_iconOff;
    LoadString(g_hInstance, IDS_APP_TITLE, iconData.szTip, 128);
    iconData.uCallbackMessage = WM_TRAYMESSAGE;
    iconData.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;

    if (!Shell_NotifyIcon(NIM_MODIFY, &iconData))
    {
        return false;
    }

    return true;
}

void RemoveTrayIcon()
{
    NOTIFYICONDATA iconData = {};
    iconData.cbSize = sizeof(NOTIFYICONDATA);
    iconData.hWnd = g_hSettingsWindow;
    iconData.uID = IDI_ICON1_ON;

    Shell_NotifyIcon(NIM_DELETE, &iconData);
}

void TrayShowMenu()
{
    // Needed to be able to cancel the menu.
    SetForegroundWindow(g_hSettingsWindow);

    HMENU menu = (HMENU)GetSubMenu(LoadMenu(g_hInstance, MAKEINTRESOURCE(IDC_OPENVRAMBIENTLIGHT)), 0);
    SetMenuDefaultItem(menu, IDM_TRAYOPEN, false);
    TrackPopupMenu(menu, TPM_LEFTALIGN, g_trayCursorPos.x, g_trayCursorPos.y, 0, g_hSettingsWindow, NULL);
}

void CALLBACK TrayClickTimerCallback(HWND hWnd, UINT message, UINT_PTR event, DWORD time) 
{
    KillTimer(g_hSettingsWindow, g_trayClickTimer);
    TrayShowMenu();  
}


LRESULT CALLBACK ProcessWindowMessages(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            switch (wmId)
            {
            case SC_KEYMENU:
                return 0; // Disable alt menu
                break;

            case IDM_TRAYOPEN:
                g_bIsSettingsWindowShown = true;
                ShowWindow(g_hSettingsWindow, SW_SHOW);
                break;

            case IDM_ABOUT:
                DialogBox(g_hInstance, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
                break;

            case IDM_EXIT:

                DestroyWindow(hWnd);
                break;

            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        break;

    case WM_PAINT:
    case WM_MOVE: // Paint on move to prevent the window from blanking out (only works partially).

        if (g_bIsSettingsWindowShown && !g_bIsPainting && g_settingsMenu.get())
        {
            g_bIsPainting = true;
            g_settingsMenu->TickMenu();
            g_bIsPainting = false;
        }

        break;

    case WM_CLOSE:
        if (g_bExitOnClose)
        {
            DestroyWindow(hWnd);
        }
        else
        {
            g_bIsSettingsWindowShown = false;
            ShowWindow(g_hSettingsWindow, SW_HIDE);
        }
        
        break;

    case WM_DESTROY:
        g_bRun = false;
        RemoveTrayIcon();
        PostQuitMessage(0);
        break;


    case WM_TRAYMESSAGE:
        switch (lParam) 
        {
        case WM_LBUTTONDBLCLK:

            KillTimer(g_hSettingsWindow, g_trayClickTimer);
            g_bIsDoubleClicking = true;

            break;

        case WM_LBUTTONDOWN:

            g_trayClickTimer = SetTimer(g_hSettingsWindow, 100, GetDoubleClickTime(), TrayClickTimerCallback);
            GetCursorPos(&g_trayCursorPos);

            break;

        case WM_LBUTTONUP:

            if (g_bIsDoubleClicking)
            {
                g_bIsDoubleClicking = false;
                ShowWindow(g_hSettingsWindow, g_bIsSettingsWindowShown ? SW_HIDE : SW_SHOW);
                SetForegroundWindow(g_hSettingsWindow);
                g_bIsSettingsWindowShown = !g_bIsSettingsWindowShown;
            }

            break;

        case WM_RBUTTONUP:

            GetCursorPos(&g_trayCursorPos);
            TrayShowMenu();
            break;

        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
        break;

    case WM_SETTINGS_UPDATED:

        DispatchSettingsUpdated(LOWORD(wParam));

        break;

    case WM_MENU_QUIT:

        DestroyWindow(hWnd);

        break;

    default:

        if (!g_settingsMenu.get() || !g_settingsMenu->HandleWin32Events(hWnd, message, wParam, lParam))
        {
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
    }

    

    return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}
