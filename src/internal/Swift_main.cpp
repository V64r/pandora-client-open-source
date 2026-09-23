#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <algorithm>
#include <functional>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <chrono>
#include <ctime>
#include <cmath>
#include <atomic>
#include <mutex>
#include <jvmti.h>
#include <commdlg.h>
#include <winhttp.h>
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "winhttp.lib")
#include <gl/GL.h>
#include <propsys.h>
#include <propkey.h>
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")

#include "sdk.hpp"
#include "mapper.hpp"
#include "features.hpp"
#include "front/front/hooks/hooks.hpp"

#define IMGUI_DEFINE_MATH_OPERATORS
#include "backends/imgui.h"
#include "front/back/misc/imgui/misc/freetype/imgui_freetype.h"
#include "interface/altmanager/alt_auth.hh"
#include "backends/imgui_internal.h"
#include "backends/imgui_impl_opengl3.h"
#include "interface/w_imgui_port/includes.hh"
#include "backends/imgui_impl_win32.h"

#include "front/back/misc/imgui/fonts/font_manager.h"

#include "front/back/misc/imgui/imgui_settings.h"
#define STB_IMAGE_IMPLEMENTATION
#include "interface/w_imgui_port/render/stb/stb_image.hh"

namespace mapper { extern jvmtiEnv* jvmti; }

#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "dwmapi.lib")

HMODULE myModule = NULL;
std::atomic<HWND> g_GameWindow{ NULL };

// ============================================================
// ANTI-CRACK / ANTI-DEBUG TRICKS
// ============================================================
void AntiDebugChecks() {
    // Cierra el juego si hay un depurador (Cheat Engine, x64dbg) adjuntado.
    if (IsDebuggerPresent()) {
        ExitProcess(0);
    }
    BOOL remoteDebugger = FALSE;
    CheckRemoteDebuggerPresent(GetCurrentProcess(), &remoteDebugger);
    if (remoteDebugger) {
        ExitProcess(0);
    }
}

void AntiCrackTimeSync() {
    unsigned __int64 start = __rdtsc();
    int volatile dummy = 0;
    for (int i = 0; i < 100; i++) {
        dummy += i;
    }
    unsigned __int64 end = __rdtsc();

    // rdtsc measures wall-clock cycles, so any normal OS preemption, GC pause or
    // heavy system load between the two reads inflates this delta. A ~100-iteration
    // loop costs only hundreds of cycles, so a debugger single-stepping through it
    // takes several seconds (billions of cycles). The old 5e8 (~0.15 s) threshold
    // fired on ordinary scheduler jitter and closed the game at random; require a
    // multi-second stall so only genuine single-stepping trips it.
    if ((end - start) > 6000000000ULL) {
        ExitProcess(0);
    }

    static SYSTEMTIME last_st = {0};
    
    if (last_st.wYear == 0) {
        GetSystemTime(&last_st);
        return;
    }
    
    SYSTEMTIME current_st;
    GetSystemTime(&current_st);
    
    // Verificar si el reloj de Windows ha retrocedido repentinamente mientras el juego esta abierto
    FILETIME ftLast, ftCurrent;
    SystemTimeToFileTime(&last_st, &ftLast);
    SystemTimeToFileTime(&current_st, &ftCurrent);
    
    ULARGE_INTEGER uLast, uCurrent;
    uLast.LowPart = ftLast.dwLowDateTime; uLast.HighPart = ftLast.dwHighDateTime;
    uCurrent.LowPart = ftCurrent.dwLowDateTime; uCurrent.HighPart = ftCurrent.dwHighDateTime;
    
    if (uCurrent.QuadPart < uLast.QuadPart && (uLast.QuadPart - uCurrent.QuadPart) > 20000000ULL) {
        ExitProcess(0);
    }
    
    last_st = current_st;
}
// ============================================================

std::atomic<bool> g_Running{ true };
std::atomic<bool> g_ShouldDestruct{ false };
static std::atomic<bool> g_DestructStarted{ false };
static HANDLE g_LogicThreadHandle = nullptr;
static float g_DestructHoldProgress = 0.0f;
static LPTOP_LEVEL_EXCEPTION_FILTER g_PreviousExceptionFilter = nullptr;

#include "interface/gui/gui.h"
#include "userconfig/userconfig.h"
#include "interface/altmanager/altmanager.h"
#include "interface/gui/gui.cpp"

// ============================================================
// SELF DESTRUCT
// ============================================================
#include <stdarg.h>
static bool GetSwiftLogPath(char (&logPath)[MAX_PATH]) {
    const DWORD pathLength = GetModuleFileNameA(myModule, logPath, MAX_PATH);
    if (pathLength == 0 || pathLength >= MAX_PATH) return false;

    // Swift.dll is emitted to <solution>\bin (two levels up lands on the repo root).
    for (int level = 0; level < 2; ++level) {
        char* separator = std::strrchr(logPath, '\\');
        if (!separator) separator = std::strrchr(logPath, '/');
        if (!separator) return false;
        *separator = '\0';
    }
    return strcat_s(logPath, "\\swiftlogs.txt") == 0;
}

static void ResetSwiftLog() {
    char logPath[MAX_PATH]{};
    if (!GetSwiftLogPath(logPath)) return;
    FILE* file = nullptr;
    fopen_s(&file, logPath, "w");
    if (file) fclose(file);
}

extern "C" void swiftLog(const char* format, ...) {
    char logPath[MAX_PATH]{};
    if (!GetSwiftLogPath(logPath)) return;

    FILE* f = nullptr;
    fopen_s(&f, logPath, "a");
    if (f) {
        char message[4096]{};
        va_list args;
        va_start(args, format);
        vsnprintf_s(message, sizeof(message), _TRUNCATE, format, args);
        va_end(args);

        for (char* cursor = message; *cursor; ++cursor) {
            if (*cursor == '\r' || *cursor == '\n') *cursor = ' ';
        }
        size_t length = std::strlen(message);
        while (length > 0 && message[length - 1] == ' ') message[--length] = '\0';

        SYSTEMTIME now{};
        GetLocalTime(&now);
        fprintf(f, "[%02u:%02u:%02u] %s\n", now.wHour, now.wMinute, now.wSecond, message);
        fflush(f);
        fclose(f);
    }
}

static LONG WINAPI SwiftExceptionLogger(EXCEPTION_POINTERS* info) {
    const DWORD code = info && info->ExceptionRecord ? info->ExceptionRecord->ExceptionCode : 0;
    void* address = info && info->ExceptionRecord ? info->ExceptionRecord->ExceptionAddress : nullptr;
    swiftLog("FATAL SEH exception code=0x%08lX address=%p destruct=%d running=%d", code, address,
        g_ShouldDestruct.load() ? 1 : 0, g_Running.load() ? 1 : 0);
    MEMORY_BASIC_INFORMATION memory{};
    if (address && VirtualQuery(address, &memory, sizeof(memory)) && memory.AllocationBase) {
        char modulePath[MAX_PATH]{};
        if (GetModuleFileNameA(static_cast<HMODULE>(memory.AllocationBase), modulePath, MAX_PATH))
            swiftLog("FATAL module=%s offset=0x%llX", modulePath,
                static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(address) -
                    reinterpret_cast<uintptr_t>(memory.AllocationBase)));
    }
    // Preserve the JVM's fatal-error reporting (including hs_err_pid logs).
    if (g_PreviousExceptionFilter && g_PreviousExceptionFilter != SwiftExceptionLogger)
        return g_PreviousExceptionFilter(info);
    return EXCEPTION_CONTINUE_SEARCH;
}

DWORD WINAPI DestructThread(LPVOID) {
    swiftLog("DestructThread: Started");

    g_Running = false;
	swiftLog("DestructThread: ShutdownAltAuthWorker BEGIN"); ShutdownAltAuthWorker(); swiftLog("DestructThread: ShutdownAltAuthWorker END");
    swiftLog("DestructThread: ShutdownPlayerSkinLoaders BEGIN"); ShutdownPlayerSkinLoaders(); swiftLog("DestructThread: ShutdownPlayerSkinLoaders END");
    g_MenuVisible = false;
    g_ImGuiReady = false;
    MenuClose();
    if (g_LogicThreadHandle) {
        swiftLog("DestructThread: Waiting for LogicThread...");
        WaitForSingleObject(g_LogicThreadHandle, INFINITE);
        swiftLog("DestructThread: LogicThread joined.");
        CloseHandle(g_LogicThreadHandle);
        g_LogicThreadHandle = nullptr;
    }
    // All hooks and Java state were already released by LogicThread.
    g_OurImGuiCtx = nullptr;
    swiftLog("DestructThread: Sleeping 1000ms...");
    Sleep(1000);

    swiftLog("DestructThread: Restoring previous exception filter BEGIN");
    SetUnhandledExceptionFilter(g_PreviousExceptionFilter);
    g_PreviousExceptionFilter = nullptr;
    swiftLog("DestructThread: Restoring previous exception filter END");
    swiftLog("DestructThread: Calling FreeLibraryAndExitThread...");
    FreeLibraryAndExitThread(myModule, 0);
    return 0;
}
// ============================================================
// LOGICTHREAD
// ============================================================
DWORD WINAPI LogicThread(LPVOID lpParam) {
    const bool sdkInitialized = sdk::init();
    if (!sdkInitialized) {
        swiftLog("[LOGIC] [!] FATAL ERROR: Could not connect to the Java process.\n");

        const std::string eventName =
            "Global\\SwiftClientFailed_" + std::to_string(GetCurrentProcessId());
        HANDLE failedEvent = OpenEventA(EVENT_MODIFY_STATE, FALSE, eventName.c_str());
        if (failedEvent) {
            SetEvent(failedEvent);
            CloseHandle(failedEvent);
        }

        if (g_LogicThreadHandle) {
            CloseHandle(g_LogicThreadHandle);
            g_LogicThreadHandle = nullptr;
        }
        FreeLibraryAndExitThread(myModule, 1);
        return 1;
    }

    // ============================================================
    // INICIALIZAR MAPPER
    // ============================================================
    if (mapper::initialize() != 0) {
        swiftLog("[LOGIC] [!] JVMTI mapper initialization returned error, but continuing...");
    }
    // ============================================================
    // VERIFICAR CLASES CORE (SIN RETRY)
    // ============================================================
    bool coreReady = true;
    const char* required[] = {
        "Minecraft", "GameSettings", "KeyBinding", "Entity", "EntityPlayer",
        "EntityPlayerXP", "WorldClient", "RenderManager", "InventoryPlayer",
        "ItemStack", "Item", "Timer"
    };

    for (const char* key : required) {
        const auto found = mapper::classes.find(key);
        if (found == mapper::classes.end() || !found->second.klass) {
            swiftLog("[LOGIC]  Core class missing: %s", key);
            coreReady = false;
        }
    }

    if (!coreReady) {
        swiftLog("[LOGIC]  Some core classes missing, continuing anyway...");
    }
    else {
        swiftLog("[LOGIC]  Core classes verified");
    }

    // ============================================================
    // INICIALIZAR HOOKS
    // ============================================================
    if (hooks::initialize() != 0) {
        swiftLog("[LOGIC] [!] Native hooks failed, continuing anyway...");
    }
    else {
        features::visual::nametags::initialize_hook();
    }

    // ============================================================
    // SEÑALIZAR AL INJECTOR
    // ============================================================
    {
        std::string eventName = "Global\\SwiftClientReady_" + std::to_string(GetCurrentProcessId());
        HANDLE readyEvent = OpenEventA(EVENT_MODIFY_STATE, FALSE, eventName.c_str());
        if (readyEvent) {
            SetEvent(readyEvent);
            CloseHandle(readyEvent);
            swiftLog("[LOGIC] Signaled injector: init complete.");
        }
    }

    TriggerNotification("Swift", "Client loaded", "CLOUD");
    TriggerNotification("Keybind", "Insert to open menu", "KEYBIND");

    ULONGLONG lastIntegrityCheck = 0;
    ULONGLONG lastIdentityRefresh = 0;

    while (g_Running) {
        const ULONGLONG loopNow = GetTickCount64();
        if (loopNow - lastIntegrityCheck >= 1000) {
            lastIntegrityCheck = loopNow;
            AntiDebugChecks();
            AntiCrackTimeSync();
        }

        if (g_ShouldDestruct) {
            g_MenuVisible = false;

            TriggerNotification("Destruct successful", "Self Destruct.", "DESTRUCT");

            Sleep(2500);

            if (sdkInitialized) {
                mapper::__minecraft minecraft;
                if (minecraft.object) {
                    swiftLog("Unload preflight: nametags shutdown BEGIN mc=%p", minecraft.object);
                    features::visual::nametags::shutdown(minecraft);
                    swiftLog("Unload preflight: nametags shutdown END");
                }
                if (!features::visual::nametags::uninitialize_hook()) {
                    swiftLog("Unload cancelled: JVM class restoration failed; runtime remains active");
                    g_ShouldDestruct.store(false, std::memory_order_release);
                    g_DestructStarted.store(false, std::memory_order_release);
                    g_DestructHoldProgress = 0.0f;
                    TriggerNotification("Destruct cancelled", "JVM restoration will be retried.", "ERROR");
                    continue;
                }
            }

            swiftLog("LogicThread: g_Running = false");
            g_Running = false;

            const ULONGLONG renderCleanupDeadline = GetTickCount64() + 3000ULL;
            while (!hooks::g_ImGuiCleanupDone.load(std::memory_order_acquire) &&
                GetTickCount64() < renderCleanupDeadline) {
                Sleep(10);
            }

            swiftLog("LogicThread: ImGui cleanup finished/timed out");

            features::visual::render_valid.store(false, std::memory_order_release);
            features::visual::nametags::enabled = false;
            gui_esp_enabled = false;
            features::visual::esp::clear();
            features::visual::tracers::enabled = false;

            swiftLog("Unload: hooks::uninitialize BEGIN"); hooks::uninitialize(); swiftLog("Unload: hooks::uninitialize END");
            swiftLog("Unload: abandon_imgui BEGIN"); hooks::abandon_imgui_after_render_timeout(); swiftLog("Unload: abandon_imgui END");

            swiftLog("Unload: autoclick shutdown BEGIN"); features::combat::auto_click::shutdown(); swiftLog("Unload: autoclick shutdown END");
            swiftLog("Unload: friends shutdown BEGIN"); features::friends::shutdown(); swiftLog("Unload: friends shutdown END");

            if (sdkInitialized) {
                if (sdk::jni && !g_AltManagerPremiumStringFields.empty() &&
                    !g_AltManagerPremiumName.empty() &&
                    g_AltManagerPremiumName != "Unknown") {
                    mapper::__minecraft minecraft;
                    if (minecraft.object) {
                        jfieldID sessionField = nullptr;
                        jobject liveSession = nullptr;
                        if (AltManagerFindSession(sdk::jni, minecraft.object,
                            sessionField, liveSession) &&
                            liveSession) {
                            AltManagerRestorePremiumSession(sdk::jni, liveSession);
                            AltManagerSetSessionName(sdk::jni, liveSession,
                                g_AltManagerPremiumName.c_str());
                            sdk::jni->DeleteLocalRef(liveSession);
                        }
                    }
                }

                if (g_AltManagerPremiumSession && sdk::jni) {
                    sdk::jni->DeleteGlobalRef(g_AltManagerPremiumSession);
                    g_AltManagerPremiumSession = nullptr;
                }
                if (sdk::jni) {
                    for (auto& savedField : g_AltManagerPremiumStringFields) {
                        if (savedField.second) sdk::jni->DeleteGlobalRef(savedField.second);
                    }
                }
                g_AltManagerPremiumStringFields.clear();

                swiftLog("LogicThread: Uninitializing JNI mapping...");
                swiftLog("Unload: hooks::uninitialize_jni BEGIN"); hooks::uninitialize_jni(); swiftLog("Unload: hooks::uninitialize_jni END");
                features::visual::outline::release_jni();
                swiftLog("Unload: mapper::uninitialize BEGIN"); mapper::uninitialize(); swiftLog("Unload: mapper::uninitialize END");
            }
            swiftLog("LogicThread: Detaching JVM...");
            if (sdkInitialized && sdk::jvm) {
                const jint detachJvmResult = sdk::jvm->DetachCurrentThread();
                swiftLog("Unload: JVM DetachCurrentThread result=%d", (int)detachJvmResult);
                sdk::jni = nullptr;
                sdk::jvmti = nullptr;
            }

            swiftLog("LogicThread: Spawning DestructThread...");
            HANDLE destructThread = CreateThread(
                nullptr, 0, DestructThread, nullptr, 0, nullptr);
            if (destructThread) CloseHandle(destructThread);
            return 0;
        }

        {
            std::lock_guard<std::recursive_mutex> settingsLock(hooks::render_mutex);
            if (!g_GameWindow || !IsWindow(g_GameWindow)) {
                g_GameWindow = FindWindowA("LWJGL", nullptr);
                if (!g_GameWindow) g_GameWindow = FindWindowA("GLFW30", nullptr);
                features::visual::window = g_GameWindow;
            }

            features::visual::nametags::enabled = gui_nametags_enabled;
            features::visual::nametags::draw_health = gui_nametags_draw_health;
            features::visual::nametags::draw_distance = gui_nametags_draw_distance;
            features::visual::nametags::draw_hurt_time = gui_nametags_draw_hurt_time;
            features::visual::nametags::draw_invisible_players = true;
            features::visual::nametags::background = gui_nametags_background;
            features::visual::nametags::use_fake_name = gui_nametags_use_fake_name;
            features::visual::nametags::fake_name = gui_nametags_fake_name;
            features::visual::nametags::color = mapper::__vec4{ gui_nametags_color[0],gui_nametags_color[1],gui_nametags_color[2],gui_nametags_color[3] };
            features::visual::tracers::enabled = gui_tracers_enabled;
            features::visual::tracers::draw_distance = gui_tracers_draw_distance;
            features::visual::tracers::draw_hurt_time = gui_tracers_draw_hurt_time;
            features::visual::tracers::draw_invisible_players = true;
            features::visual::tracers::thickness = gui_tracers_thickness;
            features::visual::tracers::color = mapper::__vec4{ gui_tracers_color_4[0],gui_tracers_color_4[1],gui_tracers_color_4[2],gui_tracers_color_4[3] };

            features::combat::auto_click::min_cps = (double)gui_min_cps;
            features::combat::auto_click::max_cps = (double)gui_max_cps;
            features::combat::auto_click::inventory_cps = (double)gui_inv_cps;
            features::combat::auto_click::break_blocks = gui_ac_break_blocks;
            features::combat::auto_click::click_method = gui_ac_click_method;
            features::combat::aim_assist::minimum_distance = gui_aa_min_dist;
            features::combat::aim_assist::maximum_distance = gui_aa_max_dist;
            features::combat::aim_assist::minimum_fov = gui_aa_min_fov;
            features::combat::aim_assist::maximum_fov = gui_aa_max_fov;
            features::combat::aim_assist::horizontal_speed = gui_aa_horizontal_speed;
            features::combat::aim_assist::vertical_speed = gui_aa_vertical_speed;
            features::combat::aim_assist::silent = gui_aa_silent;
            features::combat::reach::enabled = gui_reach_enabled;
            features::combat::reach::min_distance = gui_reach_min_distance; features::combat::reach::max_distance = gui_reach_max_distance; features::combat::reach::hitbox_enabled = gui_reach_hitbox_enabled; features::combat::reach::hitbox_size = gui_reach_hitbox_size;
            features::combat::reach::chance = gui_reach_chance;
            features::combat::reach::ground_only = gui_reach_ground_only;
            features::combat::reach::weapon_only = gui_reach_weapon_only;
            features::combat::reach::liquid_check = gui_reach_liquid_check;
            features::combat::reach::combo_mode = gui_reach_combo_mode;
            features::combat::reach::hit_through_walls = gui_reach_hit_through_walls;
            features::combat::refill::delay_ms = (int)gui_refill_delay;
            features::combat::velocity::enabled = gui_velo_enabled;
            features::combat::velocity::air_only = gui_velo_air_only;
            features::combat::velocity::moving_only = gui_velo_moving_only;
            features::combat::velocity::weapon_only = gui_velo_weapon_only;
            features::combat::velocity::push_back = gui_velo_push_back;
            features::combat::velocity::clicking_only = gui_velo_clicking_only;
            features::combat::velocity::horizontal = gui_velo_horizontal;
            features::combat::velocity::vertical = gui_velo_vertical;
            features::combat::velocity::chance = gui_velo_chance;
            features::combat::velocity::delay = gui_velo_delay;

            features::combat::no_hit_delay::enabled = gui_nohitdelay_enabled;

            features::movement::no_jump_delay::enabled = gui_nojumpdelay_enabled;
            features::movement::no_slow::enabled = gui_noslow_enabled;
            features::movement::no_item_release::enabled = gui_noitemrelease_enabled;
            features::movement::no_item_release::food = gui_noitemrelease_food;
            features::movement::sprint::enabled = gui_sprint_enabled;

            features::latency::blink::enabled = gui_blink_enabled;
            features::latency::blink::show_path = gui_blink_show_path;
            features::latency::blink::show_timer = gui_blink_show_timer;
            features::latency::blink::path_color[0] = gui_blink_path_color[0];
            features::latency::blink::path_color[1] = gui_blink_path_color[1];
            features::latency::blink::path_color[2] = gui_blink_path_color[2];
            features::latency::blink::timer_limit = gui_blink_timer_limit;
            features::visual::arraylist::enabled = gui_arraylist_enabled;
            features::visual::arraylist::watermark = gui_arraylist_watermark;

            for (auto& mod : modules) {
                if (mod.name == "Blink")         features::latency::blink::bind = mod.keybind;
                if (mod.name == "Refill")        features::combat::refill::bind = mod.keybind;
                if (mod.name == "ArmorSwitcher") gui_armorswitcher_bind = mod.keybind;
                if (mod.name == "BardHelper") features::misc::bard_helper::bind = mod.keybind;
                g_KeybindMap[mod.name] = mod.keybind;
            }

            if (sdk::jni != nullptr) {
                mapper::__minecraft minecraft;
                if (minecraft.object != nullptr) {
                    auto menuScreen = minecraft.get_current_screen();
                    AltManagerUpdateOnGameThread(sdk::jni, minecraft.object, menuScreen.object);
                    g_PlayerInGui = menuScreen.object != nullptr;

                    static bool rightShiftWasDownOutsideWorld = false;
                    const bool gameHasFocus = g_GameWindow &&
                        GetForegroundWindow() == g_GameWindow;
                    const bool rightShiftDownOutsideWorld = gameHasFocus &&
                        (GetAsyncKeyState(VK_RSHIFT) & 0x8000) != 0;
                    if (rightShiftDownOutsideWorld && !rightShiftWasDownOutsideWorld &&
                        g_OnMultiplayerScreen.load(std::memory_order_acquire) && !g_MenuVisible) {
                        g_AltManagerMode = true;
                        g_AltManagerRefocusInput.store(true, std::memory_order_release);
                        g_MenuVisible = true;
                        if (g_GameWindow) MenuOpen(g_GameWindow);
                    }
                    rightShiftWasDownOutsideWorld = rightShiftDownOutsideWorld;
                }
                else {
                    g_PlayerInGui = false;
                }

                const bool worldLoaded = minecraft.is_valid();
                g_MinecraftWorldLoaded.store(worldLoaded, std::memory_order_release);
                if (worldLoaded) {
                    features::visual::render_valid.store(true, std::memory_order_release);

                    //lazy class loading
                    static ULONGLONG nextLazyScan = 0;
                    if (loopNow >= nextLazyScan) {
                        nextLazyScan = loopNow + 500;

                        struct LazyEntry { const char* key; const char* jni; };
                        static const LazyEntry lazyClasses[] = {
                            // class lazy
                            { "PlayerControllerMP", "net/minecraft/client/multiplayer/PlayerControllerMP" },
                            { "ActiveRenderInfo", "net/minecraft/client/renderer/ActiveRenderInfo" },
                            { "MovingObjectPosition","net/minecraft/util/MovingObjectPosition" },
                            { "MovingObjectPosition_MovingObjectType", "net/minecraft/util/MovingObjectPosition$MovingObjectType" },
                            { "Packet", "net/minecraft/network/Packet" },
                            { "C16PacketClientStatus", "net/minecraft/network/play/client/C16PacketClientStatus" },
                            { "C16PacketClientStatus$EnumState", "net/minecraft/network/play/client/C16PacketClientStatus$EnumState" },
                            { "C0DPacketCloseWindow", "net/minecraft/network/play/client/C0DPacketCloseWindow" },
                            { "C08PacketPlayerBlockPlacement", "net/minecraft/network/play/client/C08PacketPlayerBlockPlacement" },                             
                            { "GuiInventory", "net/minecraft/client/gui/inventory/GuiInventory" },
                            { "GuiChest",  "net/minecraft/client/gui/inventory/GuiChest" },
                        };

                        for (const auto& lc : lazyClasses) {
                            mapper::try_resolve_class(lc.key, lc.jni);
                        }
                    }

                    {
                        if (loopNow - lastIdentityRefresh >= 1000) {
                            lastIdentityRefresh = loopNow;
                            auto lp = minecraft.get_local_player();
                            if (lp.object != nullptr) {
                                std::string pname = lp.get_name();
                                std::string sip = minecraft.get_server_ip();
                                std::lock_guard<std::mutex> identityLock(g_CachedIdentityMutex);
                                if (!pname.empty()) g_CachedPlayerName = std::move(pname);
                                g_CachedServerIP = sip.empty() ? "Singleplayer" : std::move(sip);
                            }
                        }
                    }
                    features::run_on_run_tick(minecraft);

                    static ULONGLONG nextVisualScan = 0;
                    if (loopNow >= nextVisualScan) {
                        nextVisualScan = loopNow + 8;
                        features::visual::outline::sync_hooks(minecraft);
                        features::visual::esp::run(minecraft);
                        features::visual::tracers::run(minecraft);
                        features::visual::hit_markers::run(minecraft);
                    }
                }
                else {
                    features::visual::render_valid.store(false, std::memory_order_release);
                }
            }
            else {
                g_MinecraftWorldLoaded.store(false, std::memory_order_release);
            }
        }

        Sleep(1);
    }

    return 0;
}

// ============================================================
// LAUNCHER THREAD
// ============================================================
static DWORD WINAPI LauncherThread(LPVOID lpParam)
{
    g_PreviousExceptionFilter = SetUnhandledExceptionFilter(SwiftExceptionLogger);
    ResetSwiftLog();
    swiftLog("Session started");
    g_LogicThreadHandle = CreateThread(nullptr, 0, LogicThread, lpParam, 0, nullptr);
    if (!g_LogicThreadHandle) {
        swiftLog("LauncherThread: failed to create LogicThread error=%lu", GetLastError());
        SetUnhandledExceptionFilter(g_PreviousExceptionFilter);
        g_PreviousExceptionFilter = nullptr;
        FreeLibraryAndExitThread(static_cast<HMODULE>(lpParam), 3);
    }
    return 0;
}

// ============================================================
// DLLMAIN
// ============================================================
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        myModule = hModule;
        // Keep the PE header intact: Windows needs valid module metadata for
        // reliable unwinding, FreeLibrary and later reinjection.
        HANDLE launcher = CreateThread(nullptr, 0, LauncherThread, hModule, 0, nullptr);
        if (launcher) CloseHandle(launcher);
    }
    return TRUE;
}
