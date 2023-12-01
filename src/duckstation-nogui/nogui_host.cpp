// SPDX-FileCopyrightText: 2019-2022 Connor McLaughlin <stenzek@gmail.com>
// SPDX-License-Identifier: (GPL-3.0 OR CC-BY-NC-ND-4.0)

#include "nogui_host.h"
#include "nogui_platform.h"

#include "scmversion/scmversion.h"

#include "core/achievements.h"
#include "core/controller.h"
#include "core/fullscreen_ui.h"
#include "core/game_list.h"
#include "core/gpu.h"
#include "core/host.h"
#include "core/imgui_overlays.h"
#include "core/settings.h"
#include "core/system.h"

#include "util/gpu_device.h"
#include "util/imgui_manager.h"
#include "util/ini_settings_interface.h"
#include "util/input_manager.h"
#include "util/platform_misc.h"

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_stdlib.h"

#include "common/assert.h"
#include "common/byte_stream.h"
#include "common/crash_handler.h"
#include "common/file_system.h"
#include "common/log.h"
#include "common/path.h"
#include "common/string_util.h"
#include "common/threading.h"

#include <cinttypes>
#include <cmath>
#include <condition_variable>
#include <csignal>
#include <thread>

Log_SetChannel(NoGUIHost);

#ifdef _WIN32
#include "common/windows_headers.h"
#include <ShlObj.h>
#include <shellapi.h>
#endif

static constexpr u32 SETTINGS_VERSION = 3;
static constexpr auto CPU_THREAD_POLL_INTERVAL =
  std::chrono::milliseconds(8); // how often we'll poll controllers when paused

std::unique_ptr<NoGUIPlatform> g_nogui_window;

//////////////////////////////////////////////////////////////////////////
// Local function declarations
//////////////////////////////////////////////////////////////////////////
namespace NoGUIHost {
/// Starts the virtual machine.
static void StartSystem(SystemBootParameters params);

static bool ParseCommandLineParametersAndInitializeConfig(int argc, char* argv[],
                                                          std::optional<SystemBootParameters>& autoboot);
static void PrintCommandLineVersion();
static void PrintCommandLineHelp(const char* progname);
static bool InitializeConfig(std::string settings_filename);
static void InitializeEarlyConsole();
static void HookSignals();
static bool ShouldUsePortableMode();
static void SetAppRoot();
static void SetResourcesDirectory();
static void SetDataDirectory();
static bool SetCriticalFolders();
static void SetDefaultSettings(SettingsInterface& si, bool system, bool controller);
static void StartCPUThread();
static void StopCPUThread();
static void ProcessCPUThreadEvents(bool block);
static void ProcessCPUThreadPlatformMessages();
static void CPUThreadEntryPoint();
static void CPUThreadMainLoop();
static std::unique_ptr<NoGUIPlatform> CreatePlatform();
static std::string GetWindowTitle(const std::string& game_title);
static void UpdateWindowTitle(const std::string& game_title);
static void CancelAsyncOp();
static void StartAsyncOp(std::function<void(ProgressCallback*)> callback);
static void AsyncOpThreadEntryPoint(std::function<void(ProgressCallback*)> callback);
} // namespace NoGUIHost

//////////////////////////////////////////////////////////////////////////
// Local variable declarations
//////////////////////////////////////////////////////////////////////////
static std::unique_ptr<INISettingsInterface> s_base_settings_interface;
static bool s_batch_mode = false;
static bool s_is_fullscreen = false;
static bool s_was_paused_by_focus_loss = false;

static Threading::Thread s_cpu_thread;
static Threading::KernelSemaphore s_platform_window_updated;
static std::atomic_bool s_running{false};
static std::mutex s_cpu_thread_events_mutex;
static std::condition_variable s_cpu_thread_event_done;
static std::condition_variable s_cpu_thread_event_posted;
static std::deque<std::pair<std::function<void()>, bool>> s_cpu_thread_events;
static u32 s_blocking_cpu_events_pending = 0; // TODO: Token system would work better here.

static std::mutex s_async_op_mutex;
static std::thread s_async_op_thread;
static FullscreenUI::ProgressCallback* s_async_op_progress = nullptr;

//////////////////////////////////////////////////////////////////////////
// Initialization/Shutdown
//////////////////////////////////////////////////////////////////////////

bool NoGUIHost::SetCriticalFolders()
{
  SetAppRoot();
  SetResourcesDirectory();
  SetDataDirectory();

  // logging of directories in case something goes wrong super early
  Log_DevPrintf("AppRoot Directory: %s", EmuFolders::AppRoot.c_str());
  Log_DevPrintf("DataRoot Directory: %s", EmuFolders::DataRoot.c_str());
  Log_DevPrintf("Resources Directory: %s", EmuFolders::Resources.c_str());

  // Write crash dumps to the data directory, since that'll be accessible for certain.
  CrashHandler::SetWriteDirectory(EmuFolders::DataRoot);

  // the resources directory should exist, bail out if not
  if (!FileSystem::DirectoryExists(EmuFolders::Resources.c_str()))
  {
    g_nogui_window->ReportError("Error", "Resources directory is missing, your installation is incomplete.");
    return false;
  }

  return true;
}

bool NoGUIHost::ShouldUsePortableMode()
{
  // Check whether portable.ini exists in the program directory.
  return (FileSystem::FileExists(Path::Combine(EmuFolders::AppRoot, "portable.txt").c_str()) ||
          FileSystem::FileExists(Path::Combine(EmuFolders::AppRoot, "settings.ini").c_str()));
}

void NoGUIHost::SetAppRoot()
{
  std::string program_path(FileSystem::GetProgramPath());
  Log_InfoPrintf("Program Path: %s", program_path.c_str());

  EmuFolders::AppRoot = Path::Canonicalize(Path::GetDirectory(program_path));
}

void NoGUIHost::SetResourcesDirectory()
{
#ifndef __APPLE__NOT_USED // Not using bundles yet.
  // On Windows/Linux, these are in the binary directory.
  EmuFolders::Resources = Path::Combine(EmuFolders::AppRoot, "resources");
#else
  // On macOS, this is in the bundle resources directory.
  EmuFolders::Resources = Path::Canonicalize(Path::Combine(EmuFolders::AppRoot, "../Resources"));
#endif
}

void NoGUIHost::SetDataDirectory()
{
  // Already set, e.g. by -portable.
  if (!EmuFolders::DataRoot.empty())
    return;

  if (ShouldUsePortableMode())
  {
    EmuFolders::DataRoot = EmuFolders::AppRoot;
    return;
  }

#if defined(_WIN32)
  // On Windows, use My Documents\DuckStation.
  PWSTR documents_directory;
  if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Documents, 0, NULL, &documents_directory)))
  {
    if (std::wcslen(documents_directory) > 0)
      EmuFolders::DataRoot = Path::Combine(StringUtil::WideStringToUTF8String(documents_directory), "DuckStation");
    CoTaskMemFree(documents_directory);
  }
#elif defined(__linux__)
  // Use $XDG_CONFIG_HOME/duckstation if it exists.
  const char* xdg_config_home = getenv("XDG_CONFIG_HOME");
  if (xdg_config_home && Path::IsAbsolute(xdg_config_home))
  {
    EmuFolders::DataRoot = Path::Combine(xdg_config_home, "duckstation");
  }
  else
  {
    // Use ~/.local/share/duckstation otherwise.
    const char* home_dir = getenv("HOME");
    if (home_dir)
    {
      // ~/.local/share should exist, but just in case it doesn't and this is a fresh profile..
      const std::string local_dir(Path::Combine(home_dir, ".local"));
      const std::string share_dir(Path::Combine(local_dir, "share"));
      FileSystem::EnsureDirectoryExists(local_dir.c_str(), false);
      FileSystem::EnsureDirectoryExists(share_dir.c_str(), false);
      EmuFolders::DataRoot = Path::Combine(share_dir, "duckstation");
    }
  }
#elif defined(__APPLE__)
  static constexpr char MAC_DATA_DIR[] = "Library/Application Support/DuckStation";
  const char* home_dir = getenv("HOME");
  if (home_dir)
    EmuFolders::DataRoot = Path::Combine(home_dir, MAC_DATA_DIR);
#endif

  // make sure it exists
  if (!EmuFolders::DataRoot.empty() && !FileSystem::DirectoryExists(EmuFolders::DataRoot.c_str()))
  {
    // we're in trouble if we fail to create this directory... but try to hobble on with portable
    if (!FileSystem::EnsureDirectoryExists(EmuFolders::DataRoot.c_str(), false))
      EmuFolders::DataRoot.clear();
  }

  // couldn't determine the data directory? fallback to portable.
  if (EmuFolders::DataRoot.empty())
    EmuFolders::DataRoot = EmuFolders::AppRoot;
}

bool NoGUIHost::InitializeConfig(std::string settings_filename)
{
  if (!SetCriticalFolders())
    return false;

  if (settings_filename.empty())
    settings_filename = Path::Combine(EmuFolders::DataRoot, "settings.ini");

  Log_InfoPrintf("Loading config from %s.", settings_filename.c_str());
  s_base_settings_interface = std::make_unique<INISettingsInterface>(std::move(settings_filename));
  Host::Internal::SetBaseSettingsLayer(s_base_settings_interface.get());

  u32 settings_version;
  if (!s_base_settings_interface->Load() ||
      !s_base_settings_interface->GetUIntValue("Main", "SettingsVersion", &settings_version) ||
      settings_version != SETTINGS_VERSION)
  {
    if (s_base_settings_interface->ContainsValue("Main", "SettingsVersion"))
    {
      // NOTE: No point translating this, because there's no config loaded, so no language loaded.
      Host::ReportErrorAsync("Error", fmt::format("Settings version {} does not match expected version {}, resetting.",
                                                  settings_version, SETTINGS_VERSION));
    }

    s_base_settings_interface->SetUIntValue("Main", "SettingsVersion", SETTINGS_VERSION);
    SetDefaultSettings(*s_base_settings_interface, true, true);
    s_base_settings_interface->Save();
  }

  EmuFolders::LoadConfig(*s_base_settings_interface.get());
  EmuFolders::EnsureFoldersExist();

  // We need to create the console window early, otherwise it appears behind the main window.
  if (!Log::IsConsoleOutputEnabled() &&
      s_base_settings_interface->GetBoolValue("Logging", "LogToConsole", Settings::DEFAULT_LOG_TO_CONSOLE))
  {
    Log::SetConsoleOutputParams(true, s_base_settings_interface->GetBoolValue("Logging", "LogTimestamps", true));
  }

  return true;
}

void NoGUIHost::SetDefaultSettings(SettingsInterface& si, bool system, bool controller)
{
  if (system)
  {
    System::SetDefaultSettings(si);
    EmuFolders::SetDefaults();
    EmuFolders::Save(si);
  }

  if (controller)
  {
    InputManager::SetDefaultSourceConfig(si);
    Settings::SetDefaultControllerConfig(si);
    Settings::SetDefaultHotkeyConfig(si);
  }

  g_nogui_window->SetDefaultConfig(si);
}

void Host::ReportErrorAsync(const std::string_view& title, const std::string_view& message)
{
  if (!title.empty() && !message.empty())
  {
    Log_ErrorPrintf("ReportErrorAsync: %.*s: %.*s", static_cast<int>(title.size()), title.data(),
                    static_cast<int>(message.size()), message.data());
  }
  else if (!message.empty())
  {
    Log_ErrorPrintf("ReportErrorAsync: %.*s", static_cast<int>(message.size()), message.data());
  }

  g_nogui_window->ReportError(title, message);
}

bool Host::ConfirmMessage(const std::string_view& title, const std::string_view& message)
{
  if (!title.empty() && !message.empty())
  {
    Log_ErrorPrintf("ConfirmMessage: %.*s: %.*s", static_cast<int>(title.size()), title.data(),
                    static_cast<int>(message.size()), message.data());
  }
  else if (!message.empty())
  {
    Log_ErrorPrintf("ConfirmMessage: %.*s", static_cast<int>(message.size()), message.data());
  }

  return g_nogui_window->ConfirmMessage(title, message);
}

void Host::ReportDebuggerMessage(const std::string_view& message)
{
  Log_ErrorPrintf("ReportDebuggerMessage: %.*s", static_cast<int>(message.size()), message.data());
}

std::span<const std::pair<const char*, const char*>> Host::GetAvailableLanguageList()
{
  return {};
}

bool Host::ChangeLanguage(const char* new_language)
{
  return false;
}

void Host::AddFixedInputBindings(SettingsInterface& si)
{
}

void Host::OnInputDeviceConnected(const std::string_view& identifier, const std::string_view& device_name)
{
  Host::AddKeyedOSDMessage(fmt::format("InputDeviceConnected-{}", identifier),
                           fmt::format("Input device {0} ({1}) connected.", device_name, identifier), 10.0f);
}

void Host::OnInputDeviceDisconnected(const std::string_view& identifier)
{
  Host::AddKeyedOSDMessage(fmt::format("InputDeviceConnected-{}", identifier),
                           fmt::format("Input device {} disconnected.", identifier), 10.0f);
}

s32 Host::Internal::GetTranslatedStringImpl(const std::string_view& context, const std::string_view& msg, char* tbuf,
                                            size_t tbuf_space)
{
  if (msg.size() > tbuf_space)
    return -1;
  else if (msg.empty())
    return 0;

  std::memcpy(tbuf, msg.data(), msg.size());
  return static_cast<s32>(msg.size());
}

bool Host::ResourceFileExists(const char* filename)
{
  const std::string path(Path::Combine(EmuFolders::Resources, filename));
  return FileSystem::FileExists(path.c_str());
}

std::optional<std::vector<u8>> Host::ReadResourceFile(const char* filename)
{
  const std::string path(Path::Combine(EmuFolders::Resources, filename));
  std::optional<std::vector<u8>> ret(FileSystem::ReadBinaryFile(path.c_str()));
  if (!ret.has_value())
    Log_ErrorPrintf("Failed to read resource file '%s'", filename);
  return ret;
}

std::optional<std::string> Host::ReadResourceFileToString(const char* filename)
{
  const std::string path(Path::Combine(EmuFolders::Resources, filename));
  std::optional<std::string> ret(FileSystem::ReadFileToString(path.c_str()));
  if (!ret.has_value())
    Log_ErrorPrintf("Failed to read resource file to string '%s'", filename);
  return ret;
}

std::optional<std::time_t> Host::GetResourceFileTimestamp(const char* filename)
{
  const std::string path(Path::Combine(EmuFolders::Resources, filename));
  FILESYSTEM_STAT_DATA sd;
  if (!FileSystem::StatFile(path.c_str(), &sd))
  {
    Log_ErrorPrintf("Failed to stat resource file '%s'", filename);
    return std::nullopt;
  }

  return sd.ModificationTime;
}

void Host::LoadSettings(SettingsInterface& si, std::unique_lock<std::mutex>& lock)
{
}

void Host::CheckForSettingsChanges(const Settings& old_settings)
{
}

void Host::CommitBaseSettingChanges()
{
  NoGUIHost::SaveSettings();
}

void NoGUIHost::SaveSettings()
{
  auto lock = Host::GetSettingsLock();
  if (!s_base_settings_interface->Save())
    Log_ErrorPrintf("Failed to save settings.");
}

bool NoGUIHost::InBatchMode()
{
  return s_batch_mode;
}

void NoGUIHost::SetBatchMode(bool enabled)
{
  s_batch_mode = enabled;
  if (enabled)
    GameList::Refresh(false, true);
}

void NoGUIHost::StartSystem(SystemBootParameters params)
{
  Host::RunOnCPUThread([params = std::move(params)]() { System::BootSystem(std::move(params)); });
}

void NoGUIHost::ProcessPlatformWindowResize(s32 width, s32 height, float scale)
{
  Host::RunOnCPUThread([width, height, scale]() {
    g_gpu_device->ResizeWindow(width, height, scale);
    ImGuiManager::WindowResized();
    System::HostDisplayResized();
  });
}

void NoGUIHost::ProcessPlatformMouseMoveEvent(float x, float y)
{
  InputManager::UpdatePointerAbsolutePosition(0, x, y);
  ImGuiManager::UpdateMousePosition(x, y);
}

void NoGUIHost::ProcessPlatformMouseButtonEvent(s32 button, bool pressed)
{
  Host::RunOnCPUThread([button, pressed]() {
    InputManager::InvokeEvents(InputManager::MakePointerButtonKey(0, button), pressed ? 1.0f : 0.0f,
                               GenericInputBinding::Unknown);
  });
}

void NoGUIHost::ProcessPlatformMouseWheelEvent(float x, float y)
{
  if (x != 0.0f)
    InputManager::UpdatePointerRelativeDelta(0, InputPointerAxis::WheelX, x);
  if (y != 0.0f)
    InputManager::UpdatePointerRelativeDelta(0, InputPointerAxis::WheelY, y);
}

void NoGUIHost::ProcessPlatformKeyEvent(s32 key, bool pressed)
{
  Host::RunOnCPUThread([key, pressed]() {
    InputManager::InvokeEvents(InputManager::MakeHostKeyboardKey(key), pressed ? 1.0f : 0.0f,
                               GenericInputBinding::Unknown);
  });
}

void NoGUIHost::ProcessPlatformTextEvent(const char* text)
{
  if (!ImGuiManager::WantsTextInput())
    return;

  Host::RunOnCPUThread([text = std::string(text)]() { ImGuiManager::AddTextInput(std::move(text)); });
}

void NoGUIHost::PlatformWindowFocusGained()
{
  Host::RunOnCPUThread([]() {
    if (!System::IsValid() || !s_was_paused_by_focus_loss)
      return;

    System::PauseSystem(false);
    s_was_paused_by_focus_loss = false;
  });
}

void NoGUIHost::PlatformWindowFocusLost()
{
  Host::RunOnCPUThread([]() {
    if (!System::IsRunning() || !g_settings.pause_on_focus_loss)
      return;

    s_was_paused_by_focus_loss = true;
    System::PauseSystem(true);
  });
}

void NoGUIHost::PlatformDevicesChanged()
{
  Host::RunOnCPUThread([]() { InputManager::ReloadDevices(); });
}

bool NoGUIHost::GetSavedPlatformWindowGeometry(s32* x, s32* y, s32* width, s32* height)
{
  auto lock = Host::GetSettingsLock();

  bool result = s_base_settings_interface->GetIntValue("NoGUI", "WindowX", x);
  result = result && s_base_settings_interface->GetIntValue("NoGUI", "WindowY", y);
  result = result && s_base_settings_interface->GetIntValue("NoGUI", "WindowWidth", width);
  result = result && s_base_settings_interface->GetIntValue("NoGUI", "WindowHeight", height);
  return result;
}

void NoGUIHost::SavePlatformWindowGeometry(s32 x, s32 y, s32 width, s32 height)
{
  if (s_is_fullscreen)
    return;

  auto lock = Host::GetSettingsLock();
  s_base_settings_interface->SetIntValue("NoGUI", "WindowX", x);
  s_base_settings_interface->SetIntValue("NoGUI", "WindowY", y);
  s_base_settings_interface->SetIntValue("NoGUI", "WindowWidth", width);
  s_base_settings_interface->SetIntValue("NoGUI", "WindowHeight", height);
  s_base_settings_interface->Save();
}

std::string NoGUIHost::GetAppNameAndVersion()
{
  return fmt::format("DuckStation {}", g_scm_tag_str);
}

std::string NoGUIHost::GetAppConfigSuffix()
{
#if defined(_DEBUGFAST)
  return " [DebugFast]";
#elif defined(_DEBUG)
  return " [Debug]";
#else
  return std::string();
#endif
}

void NoGUIHost::StartCPUThread()
{
  s_running.store(true, std::memory_order_release);
  s_cpu_thread.Start(CPUThreadEntryPoint);
}

void NoGUIHost::StopCPUThread()
{
  if (!s_cpu_thread.Joinable())
    return;

  {
    std::unique_lock lock(s_cpu_thread_events_mutex);
    s_running.store(false, std::memory_order_release);
    s_cpu_thread_event_posted.notify_one();
  }
  s_cpu_thread.Join();
}

void NoGUIHost::ProcessCPUThreadPlatformMessages()
{
  // This is lame. On Win32, we need to pump messages, even though *we* don't have any windows
  // on the CPU thread, because SDL creates a hidden window for raw input for some game controllers.
  // If we don't do this, we don't get any controller events.
#ifdef _WIN32
  MSG msg;
  while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE))
  {
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }
#endif
}

void NoGUIHost::ProcessCPUThreadEvents(bool block)
{
  std::unique_lock lock(s_cpu_thread_events_mutex);

  for (;;)
  {
    if (s_cpu_thread_events.empty())
    {
      if (!block || !s_running.load(std::memory_order_acquire))
        return;

      // we still need to keep polling the controllers when we're paused
      do
      {
        ProcessCPUThreadPlatformMessages();
        InputManager::PollSources();
      } while (!s_cpu_thread_event_posted.wait_for(lock, CPU_THREAD_POLL_INTERVAL,
                                                   []() { return !s_cpu_thread_events.empty(); }));
    }

    // return after processing all events if we had one
    block = false;

    auto event = std::move(s_cpu_thread_events.front());
    s_cpu_thread_events.pop_front();
    lock.unlock();
    event.first();
    lock.lock();

    if (event.second)
    {
      s_blocking_cpu_events_pending--;
      s_cpu_thread_event_done.notify_one();
    }
  }
}

void NoGUIHost::CPUThreadEntryPoint()
{
  Threading::SetNameOfCurrentThread("CPU Thread");

  // input source setup must happen on emu thread
  System::Internal::ProcessStartup();

  // start the fullscreen UI and get it going
  if (Host::CreateGPUDevice(Settings::GetRenderAPIForRenderer(g_settings.gpu_renderer)) && FullscreenUI::Initialize())
  {
    // kick a game list refresh if we're not in batch mode
    if (!InBatchMode())
      Host::RefreshGameListAsync(false);

    CPUThreadMainLoop();

    Host::CancelGameListRefresh();
  }
  else
  {
    g_nogui_window->ReportError("Error", "Failed to open host display.");
  }

  // finish any events off (e.g. shutdown system with save)
  ProcessCPUThreadEvents(false);

  if (System::IsValid())
    System::ShutdownSystem(false);
  Host::ReleaseGPUDevice();
  Host::ReleaseRenderWindow();

  System::Internal::ProcessShutdown();
  g_nogui_window->QuitMessageLoop();
}

void NoGUIHost::CPUThreadMainLoop()
{
  while (s_running.load(std::memory_order_acquire))
  {
    if (System::IsRunning())
    {
      System::Execute();
      continue;
    }

    Host::PumpMessagesOnCPUThread();
    System::Internal::IdlePollUpdate();
    System::PresentDisplay(false);
    if (!g_gpu_device->IsVsyncEnabled())
      g_gpu_device->ThrottlePresentation();
  }
}

std::optional<WindowInfo> Host::AcquireRenderWindow(bool recreate_window)
{
  std::optional<WindowInfo> wi;

  g_nogui_window->ExecuteInMessageLoop([&wi, recreate_window]() {
    bool res = g_nogui_window->HasPlatformWindow();
    if (!res || recreate_window)
    {
      if (res)
        g_nogui_window->DestroyPlatformWindow();

      res = g_nogui_window->CreatePlatformWindow(NoGUIHost::GetWindowTitle(System::GetGameTitle()));
    }
    if (res)
      wi = g_nogui_window->GetPlatformWindowInfo();
    s_platform_window_updated.Post();
  });

  s_platform_window_updated.Wait();

  if (!wi.has_value())
  {
    g_nogui_window->ReportError("Error", "Failed to create render window.");
    return std::nullopt;
  }

  // reload input sources, since it might use the window handle
  {
    auto lock = Host::GetSettingsLock();
    InputManager::ReloadSources(*Host::GetSettingsInterface(), lock);
  }

  return wi;
}

void Host::ReleaseRenderWindow()
{
  // Need to block here, otherwise the recreation message associates with the old window.
  g_nogui_window->ExecuteInMessageLoop([]() {
    g_nogui_window->DestroyPlatformWindow();
    s_platform_window_updated.Post();
  });
  s_platform_window_updated.Wait();
}

void Host::OnSystemStarting()
{
  s_was_paused_by_focus_loss = false;
}

void Host::OnSystemStarted()
{
}

void Host::OnSystemPaused()
{
}

void Host::OnSystemResumed()
{
}

void Host::OnSystemDestroyed()
{
}

void Host::BeginPresentFrame()
{
}

void Host::RequestResizeHostDisplay(s32 width, s32 height)
{
  g_nogui_window->RequestRenderWindowSize(width, height);
}

void Host::OpenURL(const std::string_view& url)
{
  g_nogui_window->OpenURL(url);
}

bool Host::CopyTextToClipboard(const std::string_view& text)
{
  return g_nogui_window->CopyTextToClipboard(text);
}

void Host::OnPerformanceCountersUpdated()
{
  // noop
}

void Host::OnGameChanged(const std::string& disc_path, const std::string& game_serial, const std::string& game_name)
{
  Log_VerbosePrintf("Host::OnGameChanged(\"%s\", \"%s\", \"%s\")", disc_path.c_str(), game_serial.c_str(),
                    game_name.c_str());
  NoGUIHost::UpdateWindowTitle(game_name);
}

void Host::OnAchievementsLoginRequested(Achievements::LoginRequestReason reason)
{
  // noop
}

void Host::OnAchievementsLoginSuccess(const char* username, u32 points, u32 sc_points, u32 unread_messages)
{
  // noop
}

void Host::OnAchievementsRefreshed()
{
  // noop
}

void Host::OnAchievementsHardcoreModeChanged(bool enabled)
{
  // noop
}

void Host::SetMouseMode(bool relative, bool hide_cursor)
{
  // noop
}

void Host::PumpMessagesOnCPUThread()
{
  NoGUIHost::ProcessCPUThreadPlatformMessages();
  NoGUIHost::ProcessCPUThreadEvents(false);
}

std::unique_ptr<NoGUIPlatform> NoGUIHost::CreatePlatform()
{
  std::unique_ptr<NoGUIPlatform> ret;

#if defined(_WIN32)
  ret = NoGUIPlatform::CreateWin32Platform();
#elif defined(__APPLE__)
  ret = NoGUIPlatform::CreateCocoaPlatform();
#else
  // linux
  const char* platform = std::getenv("DUCKSTATION_NOGUI_PLATFORM");
#ifdef NOGUI_PLATFORM_WAYLAND
  if (!ret && (!platform || StringUtil::Strcasecmp(platform, "wayland") == 0) && std::getenv("WAYLAND_DISPLAY"))
    ret = NoGUIPlatform::CreateWaylandPlatform();
#endif
#ifdef NOGUI_PLATFORM_X11
  if (!ret && (!platform || StringUtil::Strcasecmp(platform, "x11") == 0) && std::getenv("DISPLAY"))
    ret = NoGUIPlatform::CreateX11Platform();
#endif
#endif

  return ret;
}

std::string NoGUIHost::GetWindowTitle(const std::string& game_title)
{
  std::string suffix(GetAppConfigSuffix());
  std::string window_title;
  if (System::IsShutdown() || game_title.empty())
    window_title = GetAppNameAndVersion() + suffix;
  else
    window_title = game_title;

  return window_title;
}

void NoGUIHost::UpdateWindowTitle(const std::string& game_title)
{
  g_nogui_window->SetPlatformWindowTitle(GetWindowTitle(game_title));
}

void Host::RunOnCPUThread(std::function<void()> function, bool block /* = false */)
{
  std::unique_lock lock(s_cpu_thread_events_mutex);
  s_cpu_thread_events.emplace_back(std::move(function), block);
  s_cpu_thread_event_posted.notify_one();
  if (block)
    s_cpu_thread_event_done.wait(lock, []() { return s_blocking_cpu_events_pending == 0; });
}

void NoGUIHost::StartAsyncOp(std::function<void(ProgressCallback*)> callback)
{
  CancelAsyncOp();
  s_async_op_thread = std::thread(AsyncOpThreadEntryPoint, std::move(callback));
}

void NoGUIHost::CancelAsyncOp()
{
  std::unique_lock lock(s_async_op_mutex);
  if (!s_async_op_thread.joinable())
    return;

  if (s_async_op_progress)
    s_async_op_progress->SetCancelled();

  lock.unlock();
  s_async_op_thread.join();
}

void NoGUIHost::AsyncOpThreadEntryPoint(std::function<void(ProgressCallback*)> callback)
{
  Threading::SetNameOfCurrentThread("Async Op");

  FullscreenUI::ProgressCallback fs_callback("async_op");
  std::unique_lock lock(s_async_op_mutex);
  s_async_op_progress = &fs_callback;

  lock.unlock();
  callback(&fs_callback);
  lock.lock();

  s_async_op_progress = nullptr;
}

void Host::RefreshGameListAsync(bool invalidate_cache)
{
  NoGUIHost::StartAsyncOp(
    [invalidate_cache](ProgressCallback* progress) { GameList::Refresh(invalidate_cache, false, progress); });
}

void Host::CancelGameListRefresh()
{
  NoGUIHost::CancelAsyncOp();
}

bool Host::IsFullscreen()
{
  return s_is_fullscreen;
}

void Host::SetFullscreen(bool enabled)
{
  if (s_is_fullscreen == enabled)
    return;

  s_is_fullscreen = enabled;
  g_nogui_window->SetFullscreen(enabled);
}

std::optional<WindowInfo> Host::GetTopLevelWindowInfo()
{
  return g_nogui_window->GetPlatformWindowInfo();
}

void Host::RequestExit(bool allow_confirm)
{
  if (System::IsValid())
  {
    Host::RunOnCPUThread([]() { System::ShutdownSystem(g_settings.save_state_on_exit); });
  }

  // clear the running flag, this'll break out of the main CPU loop once the VM is shutdown.
  s_running.store(false, std::memory_order_release);
}

void Host::RequestSystemShutdown(bool allow_confirm, bool save_state)
{
  // TODO: Confirm
  if (System::IsValid())
  {
    Host::RunOnCPUThread([save_state]() { System::ShutdownSystem(save_state); });
  }
}

std::optional<u32> InputManager::ConvertHostKeyboardStringToCode(const std::string_view& str)
{
  return g_nogui_window->ConvertHostKeyboardStringToCode(str);
}

std::optional<std::string> InputManager::ConvertHostKeyboardCodeToString(u32 code)
{
  return g_nogui_window->ConvertHostKeyboardCodeToString(code);
}

const char* InputManager::ConvertHostKeyboardCodeToIcon(u32 code)
{
  return nullptr;
}

BEGIN_HOTKEY_LIST(g_host_hotkeys)
END_HOTKEY_LIST()

static void SignalHandler(int signal)
{
  // First try the normal (graceful) shutdown/exit.
  static bool graceful_shutdown_attempted = false;
  if (!graceful_shutdown_attempted)
  {
    std::fprintf(stderr, "Received CTRL+C, attempting graceful shutdown. Press CTRL+C again to force.\n");
    graceful_shutdown_attempted = true;
    Host::RequestExit(false);
    return;
  }

  std::signal(signal, SIG_DFL);

  // MacOS is missing std::quick_exit() despite it being C++11...
#ifndef __APPLE__
  std::quick_exit(1);
#else
  _Exit(1);
#endif
}

void NoGUIHost::HookSignals()
{
  std::signal(SIGINT, SignalHandler);
  std::signal(SIGTERM, SignalHandler);
}

void NoGUIHost::InitializeEarlyConsole()
{
  const bool was_console_enabled = Log::IsConsoleOutputEnabled();
  if (!was_console_enabled)
    Log::SetConsoleOutputParams(true);
}

void NoGUIHost::PrintCommandLineVersion()
{
  InitializeEarlyConsole();

  std::fprintf(stderr, "DuckStation Version %s (%s)\n", g_scm_tag_str, g_scm_branch_str);
  std::fprintf(stderr, "https://github.com/mattysweeps/duckstation\n");
  std::fprintf(stderr, "\n");
}

void NoGUIHost::PrintCommandLineHelp(const char* progname)
{
  InitializeEarlyConsole();

  PrintCommandLineVersion();
  std::fprintf(stderr, "Usage: %s [parameters] [--] [boot filename]\n", progname);
  std::fprintf(stderr, "\n");
  std::fprintf(stderr, "  -help: Displays this information and exits.\n");
  std::fprintf(stderr, "  -version: Displays version information and exits.\n");
  std::fprintf(stderr, "  -batch: Enables batch mode (exits after powering off).\n");
  std::fprintf(stderr, "  -fastboot: Force fast boot for provided filename.\n");
  std::fprintf(stderr, "  -slowboot: Force slow boot for provided filename.\n");
  std::fprintf(stderr, "  -bios: Boot into the BIOS shell.\n");
  std::fprintf(stderr, "  -resume: Load resume save state. If a boot filename is provided,\n"
                       "    that game's resume state will be loaded, otherwise the most\n"
                       "    recent resume save state will be loaded.\n");
  std::fprintf(stderr, "  -state <index>: Loads specified save state by index. If a boot\n"
                       "    filename is provided, a per-game state will be loaded, otherwise\n"
                       "    a global state will be loaded.\n");
  std::fprintf(stderr, "  -statefile <filename>: Loads state from the specified filename.\n"
                       "    No boot filename is required with this option.\n");
  std::fprintf(stderr, "  -exe <filename>: Boot the specified exe instead of loading from disc.\n");
  std::fprintf(stderr, "  -fullscreen: Enters fullscreen mode immediately after starting.\n");
  std::fprintf(stderr, "  -nofullscreen: Prevents fullscreen mode from triggering if enabled.\n");
  std::fprintf(stderr, "  -portable: Forces \"portable mode\", data in same directory.\n");
  std::fprintf(stderr, "  -settings <filename>: Loads a custom settings configuration from the\n"
                       "    specified filename. Default settings applied if file not found.\n");
  std::fprintf(stderr, "  -earlyconsole: Creates console as early as possible, for logging.\n");
  std::fprintf(stderr, "  --: Signals that no more arguments will follow and the remaining\n"
                       "    parameters make up the filename. Use when the filename contains\n"
                       "    spaces or starts with a dash.\n");
  std::fprintf(stderr, "\n");
}

std::optional<SystemBootParameters>& AutoBoot(std::optional<SystemBootParameters>& autoboot)
{
  if (!autoboot)
    autoboot.emplace();

  return autoboot;
}

bool NoGUIHost::ParseCommandLineParametersAndInitializeConfig(int argc, char* argv[],
                                                              std::optional<SystemBootParameters>& autoboot)
{
  std::optional<s32> state_index;
  std::string settings_filename;
  bool starting_bios = false;

  bool no_more_args = false;

  for (int i = 1; i < argc; i++)
  {
    if (!no_more_args)
    {
#define CHECK_ARG(str) (std::strcmp(argv[i], (str)) == 0)
#define CHECK_ARG_PARAM(str) (std::strcmp(argv[i], (str)) == 0 && ((i + 1) < argc))

      if (CHECK_ARG("-help"))
      {
        PrintCommandLineHelp(argv[0]);
        return false;
      }
      else if (CHECK_ARG("-version"))
      {
        PrintCommandLineVersion();
        return false;
      }
      else if (CHECK_ARG("-batch"))
      {
        Log_InfoPrintf("Command Line: Using batch mode.");
        s_batch_mode = true;
        continue;
      }
      else if (CHECK_ARG("-bios"))
      {
        Log_InfoPrintf("Command Line: Starting BIOS.");
        AutoBoot(autoboot);
        starting_bios = true;
        continue;
      }
      else if (CHECK_ARG("-fastboot"))
      {
        Log_InfoPrintf("Command Line: Forcing fast boot.");
        AutoBoot(autoboot)->override_fast_boot = true;
        continue;
      }
      else if (CHECK_ARG("-slowboot"))
      {
        Log_InfoPrintf("Command Line: Forcing slow boot.");
        AutoBoot(autoboot)->override_fast_boot = false;
        continue;
      }
      else if (CHECK_ARG("-resume"))
      {
        state_index = -1;
        Log_InfoPrintf("Command Line: Loading resume state.");
        continue;
      }
      else if (CHECK_ARG_PARAM("-state"))
      {
        state_index = StringUtil::FromChars<s32>(argv[++i]);
        if (!state_index.has_value())
        {
          Log_ErrorPrintf("Invalid state index");
          return false;
        }

        Log_InfoPrintf("Command Line: Loading state index: %d", state_index.value());
        continue;
      }
      else if (CHECK_ARG_PARAM("-statefile"))
      {
        AutoBoot(autoboot)->save_state = argv[++i];
        Log_InfoPrintf("Command Line: Loading state file: '%s'", autoboot->save_state.c_str());
        continue;
      }
      else if (CHECK_ARG_PARAM("-exe"))
      {
        AutoBoot(autoboot)->override_exe = argv[++i];
        Log_InfoPrintf("Command Line: Overriding EXE file: '%s'", autoboot->override_exe.c_str());
        continue;
      }
      else if (CHECK_ARG("-fullscreen"))
      {
        Log_InfoPrintf("Command Line: Using fullscreen.");
        AutoBoot(autoboot)->override_fullscreen = true;
        // s_start_fullscreen_ui_fullscreen = true;
        continue;
      }
      else if (CHECK_ARG("-nofullscreen"))
      {
        Log_InfoPrintf("Command Line: Not using fullscreen.");
        AutoBoot(autoboot)->override_fullscreen = false;
        continue;
      }
      else if (CHECK_ARG("-portable"))
      {
        Log_InfoPrintf("Command Line: Using portable mode.");
        EmuFolders::DataRoot = EmuFolders::AppRoot;
        continue;
      }
      else if (CHECK_ARG_PARAM("-settings"))
      {
        settings_filename = argv[++i];
        Log_InfoPrintf("Command Line: Overriding settings filename: %s", settings_filename.c_str());
        continue;
      }
      else if (CHECK_ARG("-earlyconsole"))
      {
        InitializeEarlyConsole();
        continue;
      }
      else if (CHECK_ARG("--"))
      {
        no_more_args = true;
        continue;
      }
      else if (argv[i][0] == '-')
      {
        g_nogui_window->ReportError("Error", fmt::format("Unknown parameter: {}", argv[i]));
        return false;
      }

#undef CHECK_ARG
#undef CHECK_ARG_PARAM
    }

    if (autoboot && !autoboot->filename.empty())
      autoboot->filename += ' ';
    AutoBoot(autoboot)->filename += argv[i];
  }

  // To do anything useful, we need the config initialized.
  if (!NoGUIHost::InitializeConfig(std::move(settings_filename)))
  {
    // NOTE: No point translating this, because no config means the language won't be loaded anyway.
    g_nogui_window->ReportError("Error", "Failed to initialize config.");
    return EXIT_FAILURE;
  }

  // Check the file we're starting actually exists.

  if (autoboot && !autoboot->filename.empty() && !FileSystem::FileExists(autoboot->filename.c_str()))
  {
    g_nogui_window->ReportError("Error", fmt::format("File '{}' does not exist.", autoboot->filename));
    return false;
  }

  if (state_index.has_value())
  {
    AutoBoot(autoboot);

    if (autoboot->filename.empty())
    {
      // loading global state, -1 means resume the last game
      if (state_index.value() < 0)
        autoboot->save_state = System::GetMostRecentResumeSaveStatePath();
      else
        autoboot->save_state = System::GetGlobalSaveStateFileName(state_index.value());
    }
    else
    {
      // loading game state
      const std::string game_serial(GameDatabase::GetSerialForPath(autoboot->filename.c_str()));
      autoboot->save_state = System::GetGameSaveStateFileName(game_serial, state_index.value());
    }

    if (autoboot->save_state.empty() || !FileSystem::FileExists(autoboot->save_state.c_str()))
    {
      g_nogui_window->ReportError("Error", "The specified save state does not exist.");
      return false;
    }
  }

  // check autoboot parameters, if we set something like fullscreen without a bios
  // or disc, we don't want to actually start.
  if (autoboot && autoboot->filename.empty() && autoboot->save_state.empty() && !starting_bios)
    autoboot.reset();

  return true;
}

int main(int argc, char* argv[])
{
  CrashHandler::Install();

  g_nogui_window = NoGUIHost::CreatePlatform();
  if (!g_nogui_window)
    return EXIT_FAILURE;

  std::optional<SystemBootParameters> autoboot;
  if (!NoGUIHost::ParseCommandLineParametersAndInitializeConfig(argc, argv, autoboot))
    return EXIT_FAILURE;

  // the rest of initialization happens on the CPU thread.
  NoGUIHost::HookSignals();
  NoGUIHost::StartCPUThread();

  if (autoboot)
    NoGUIHost::StartSystem(std::move(autoboot.value()));

  g_nogui_window->RunMessageLoop();

  NoGUIHost::CancelAsyncOp();
  NoGUIHost::StopCPUThread();

  // Ensure log is flushed.
  Log::SetFileOutputParams(false, nullptr);

  s_base_settings_interface.reset();
  g_nogui_window.reset();
  return EXIT_SUCCESS;
}

#ifdef _WIN32

int wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nShowCmd)
{
  std::vector<std::string> argc_strings;
  argc_strings.reserve(1);

  // CommandLineToArgvW() only adds the program path if the command line is empty?!
  argc_strings.push_back(FileSystem::GetProgramPath());

  if (std::wcslen(lpCmdLine) > 0)
  {
    int argc;
    LPWSTR* argv_wide = CommandLineToArgvW(lpCmdLine, &argc);
    if (argv_wide)
    {
      for (int i = 0; i < argc; i++)
        argc_strings.push_back(StringUtil::WideStringToUTF8String(argv_wide[i]));

      LocalFree(argv_wide);
    }
  }

  std::vector<char*> argc_pointers;
  argc_pointers.reserve(argc_strings.size());
  for (std::string& arg : argc_strings)
    argc_pointers.push_back(arg.data());

  return main(static_cast<int>(argc_pointers.size()), argc_pointers.data());
}

#endif
