#include "../hooks.hpp"
#include "../../features/features.hpp"
#include "backends/imgui.h"
#include "backends/imgui_impl_opengl3.h"
#include "backends/imgui_impl_win32.h"
#include <gl/GL.h>

extern std::atomic<bool> g_Running;
extern std::atomic<bool> g_MenuVisible;
extern bool  g_AltManagerMode;
extern float g_MenuOpenAnim;
extern std::atomic<bool> g_PlayerInGui;
extern std::atomic<bool> g_ImGuiReady;
extern std::atomic<HWND> g_GameWindow;
extern float g_PendingWheelDelta;
extern void  InitInGameImGui(HWND hwnd);
extern void  InvalidateInGameImGuiContext(ImGuiContext* dyingContext);
extern void  InvalidatePlayerSkinTexturesAfterContextLoss();
extern void  ShutdownPlayerSkinLoaders();
extern void  RenderMenuContents();
extern void  ProcessKeybinds();
extern void  RenderNotifications();
extern WNDPROC g_OrigWndProc;
extern "C" void swiftLog(const char* format, ...);

bool g_swiftContextLost = false;

namespace hooks
{
    // Ordering marker only: never calls Minecraft from inside the ImGui backend.
    static void WorldLayerBoundary(const ImDrawList*, const ImDrawCmd*) {}

    static void RenderOverlayLayers(ImDrawData* data)
    {
        if (!data) return;
        for (int i = 0; i < data->CmdListsCount; ++i) {
            ImDrawList* list = data->CmdLists[i];
            for (int c = 0; c < list->CmdBuffer.Size; ++c) {
                if (list->CmdBuffer[c].UserCallback != WorldLayerBoundary) continue;
                // Retain vertex/index buffers and their original offsets. Only
                // expose the commands belonging to each pass to the backend.
                const int count = list->CmdBuffer.Size;
                ImDrawCmd* commands = list->CmdBuffer.Data;
                ImDrawData world = *data;
                world.CmdLists = &list;
                world.CmdListsCount = 1;
                world.TotalIdxCount = list->IdxBuffer.Size;
                world.TotalVtxCount = list->VtxBuffer.Size;
                list->CmdBuffer.Size = c;
                __try { ImGui_ImplOpenGL3_RenderDrawData(&world); }
                __finally { list->CmdBuffer.Size = count; }

                features::visual::nametags::render_minecraft_font();

                list->CmdBuffer.Data = commands + c + 1;
                list->CmdBuffer.Size = count - c - 1;
                __try { ImGui_ImplOpenGL3_RenderDrawData(data); }
                __finally {
                    list->CmdBuffer.Data = commands;
                    list->CmdBuffer.Size = count;
                }
                return;
            }
        }
        ImGui_ImplOpenGL3_RenderDrawData(data);
    }

    static int LogRenderException(EXCEPTION_POINTERS* info) {
        static ULONGLONG last_report = 0;
        const auto now = GetTickCount64();
        if (last_report == 0 || now - last_report >= 5000) {
            last_report = now;
            swiftLog("Render exception code=0x%08lX address=%p GL=%p thread=%lu",
                info->ExceptionRecord->ExceptionCode, info->ExceptionRecord->ExceptionAddress,
                wglGetCurrentContext(), GetCurrentThreadId());
        }
        return EXCEPTION_EXECUTE_HANDLER;
    }
    typedef __int32(__stdcall* SwapBuffers_t)(HDC);

    ImGuiContext* g_GameImGuiContext = nullptr;
    static bool          g_GameImGuiInit = false;
    static HWND          g_LastHwnd = nullptr;
    static HGLRC         g_LastGLRC = nullptr;
    static HWND          g_PendingHwnd = nullptr;
    static HGLRC         g_PendingGLRC = nullptr;
    static unsigned int  g_PendingStableFrames = 0;
    static DWORD        g_GameProcessId = 0;
    static char         g_GameWindowClass[256] = {};

    // ============================================================
    // ImGui en el GL thread (nico thread con contexto GL activo).
    // Una vez limpio, swap_buffers pone g_ImGuiCleanupDone = true.
    // ============================================================
    std::atomic<bool> g_ImGuiCleanupDone{ false };

    void abandon_imgui_after_render_timeout()
    {
        // Called only after hooks are disabled and callbacks have drained.
        // Without a current GLRC, release CPU/Win32 state and never leave
        // WndProc pointing into an unloading DLL.
        // Rather than calling SetWindowLongPtrA across threads (which fails and leaves a hook),
        // we send a message to the window so its own thread unhooks it.
        if (::g_OrigWndProc && g_GameWindow && IsWindow(g_GameWindow)) {
            DWORD_PTR messageResult = 0;
            SendMessageTimeoutA(g_GameWindow, WM_NULL, 0, 0,
                SMTO_ABORTIFHUNG | SMTO_BLOCK, 1000, &messageResult);
        }

        std::lock_guard<std::recursive_mutex> lock(render_mutex);
        // The SendMessageTimeoutA above forces HookedWndProc to run on that thread and do it safely.

        ImGuiContext* dyingContext = g_GameImGuiContext;
        if (dyingContext) {
            ImGuiContext* previous = ImGui::GetCurrentContext();
            const bool previousWasDying = previous == dyingContext;
            InvalidateInGameImGuiContext(dyingContext);
            InvalidatePlayerSkinTexturesAfterContextLoss();
            ImGui::SetCurrentContext(dyingContext);
            g_swiftContextLost = true;
            ImGui_ImplWin32_Shutdown();
            // No GL context is guaranteed on this timeout path (especially after F11).
            // Releasing the GL backend here can use a stale driver dispatch table.
            // The normal render-thread cleanup still releases it safely.
            // ImGui::DestroyContext(dyingContext); // CAUSES CRASH: Heap corruption or double free. Leaking it is safe.
            g_swiftContextLost = false;
            ImGui::SetCurrentContext(previousWasDying ? nullptr : previous);
        }
        g_GameImGuiContext = nullptr;
        g_GameImGuiInit = false;
        g_ImGuiReady = false;
        g_LastHwnd = nullptr;
        g_LastGLRC = nullptr;
        g_PendingHwnd = nullptr;
        g_PendingGLRC = nullptr;
        g_PendingStableFrames = 0;
        g_ImGuiCleanupDone.store(true, std::memory_order_release);
    }
    
#pragma optimize("", off)
    __declspec(noinline) static void SafeRenderImGui(HWND current_hwnd) {
        ImGuiContext* prev = ImGui::GetCurrentContext();
        __try {
            ImGui::SetCurrentContext(g_GameImGuiContext);

            GLint viewport[4] = {};
            glGetIntegerv(GL_VIEWPORT, viewport);

            if (viewport[2] > 0 && viewport[3] > 0) {
                features::visual::view_port[0] = viewport[0];
                features::visual::view_port[1] = viewport[1];
                features::visual::view_port[2] = viewport[2];
                features::visual::view_port[3] = viewport[3];
            }

            if (viewport[2] <= 0 || viewport[3] <= 0) {
                ImGui::SetCurrentContext(prev);
                return;
            }

            ImGuiIO& io = ImGui::GetIO();
            // Minecraft may temporarily leave GL_VIEWPORT restricted to its
            // world/HUD render area. ImGui overlays must use the complete
            // client surface or Background Dim ends in a visible vertical cut.
            RECT clientRect{};
            const bool hasClientRect = GetClientRect(current_hwnd, &clientRect) != FALSE;
            const int overlayWidth = hasClientRect
                ? (clientRect.right - clientRect.left) : viewport[2];
            const int overlayHeight = hasClientRect
                ? (clientRect.bottom - clientRect.top) : viewport[3];
            if (overlayWidth <= 0 || overlayHeight <= 0) {
                ImGui::SetCurrentContext(prev);
                return;
            }
            io.DisplaySize = ImVec2((float)overlayWidth, (float)overlayHeight);
            CURSORINFO ci = { sizeof(CURSORINFO) };
            GetCursorInfo(&ci);
            bool osCursorVisible = (ci.flags & CURSOR_SHOWING) != 0;
            // Alt Manager always uses ImGui's cursor. Some Minecraft clients
            // report an OS cursor as visible while keeping it clipped or parked
            // outside the client area, leaving this screen with no cursor.
            io.MouseDrawCursor = g_MenuVisible &&
                (g_AltManagerMode || !osCursorVisible);

            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplWin32_NewFrame();

            if (g_MenuVisible) {
                ClipCursor(nullptr);

                POINT pt;
                if (GetCursorPos(&pt) && ScreenToClient(current_hwnd, &pt))
                    io.AddMousePosEvent((float)pt.x, (float)pt.y);

                io.AddMouseButtonEvent(0, (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0);
                io.AddMouseButtonEvent(1, (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0);
                io.AddMouseButtonEvent(2, (GetAsyncKeyState(VK_MBUTTON) & 0x8000) != 0);

                if (g_PendingWheelDelta != 0.0f) {
                    io.AddMouseWheelEvent(0.0f, g_PendingWheelDelta);
                    g_PendingWheelDelta = 0.0f;
                }
            }

            ImGui::NewFrame();
            features::visual::nametags::run_on_game_frame();
            features::combat::refill::run_on_game_frame();
            features::combat::armor_switcher::run_on_game_frame();
            features::misc::run_inputs_on_game_frame();

            // Synchronize projection, camera origin and partial tick after the
            // world finished rendering, before generating any overlay points.
            features::visual::render_frame_snapshot_valid =
                hooks::capture_render_frame_java_state();

            if (g_MenuVisible)
                ClipCursor(nullptr);

            features::visual::outline::render();
            if (features::visual::render_valid.load(std::memory_order_acquire))
            {
                features::visual::esp::render();
                features::visual::tracers::render();
                features::visual::nametags::render();
                features::visual::hit_markers::render();
            }

            if (features::visual::nametags::has_pending_minecraft_render())
                ImGui::GetBackgroundDrawList()->AddCallback(WorldLayerBoundary, nullptr);
            features::latency::blink::render_ui();

            // Alt Manager dims only the game. HUD elements are intentionally
            // submitted afterwards so Array List and watermark stay crisp.
            if (g_MenuVisible && g_AltManagerMode) {
                RenderMenuContents();
                features::visual::arraylist::run();
            } else {
                if (g_MenuVisible || g_MenuOpenAnim > 0.0f)
                    RenderMenuContents();
                // HUD is intentionally submitted after Background Dim so the
                // watermark and Array List stay crisp while Insert is open.
                features::visual::arraylist::run();
            }

            RenderNotifications();

            // Decide cursor ownership once, after the active panel rendered.
            // This prevents Alt Manager from leaving the following Insert menu
            // with the previous frame's hidden-cursor state.
            CURSORINFO ci2 = { sizeof(CURSORINFO) };
            GetCursorInfo(&ci2);
            bool osCursorVisible2 = (ci2.flags & CURSOR_SHOWING) != 0;
            io.MouseDrawCursor = g_MenuVisible &&
                (g_AltManagerMode || !osCursorVisible2);

            ImGui::Render();

            glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);

            const bool last_depth = glIsEnabled(GL_DEPTH_TEST) == GL_TRUE;
            glDisable(GL_DEPTH_TEST);

            RenderOverlayLayers(ImGui::GetDrawData());

            if (last_depth) glEnable(GL_DEPTH_TEST);

            ImGui::SetCurrentContext(prev);
        } __except (LogRenderException(GetExceptionInformation())) {
            ImGui::SetCurrentContext(prev);
        }
    }
#pragma optimize("", on)

    extern "C" void swiftLog(const char* format, ...);

    auto swap_buffers(HDC hdc) -> __int32
    {
        callback_guard callback;
        if (hooks::main_thread_id == 0)
            hooks::main_thread_id = GetCurrentThreadId();

        // ?? SELF DESTRUCT PATH ???????????????????????????????????
        if (!g_Running)
        {
            if (g_GameImGuiInit && !g_ImGuiCleanupDone.load())
            {
                // Defer cleanup by one frame if another thread currently owns the lock.
                std::lock_guard<std::recursive_mutex> lock(hooks::render_mutex);

                if (g_GameImGuiInit) // doble check dentro del lock
                {
                    swiftLog("SwapBuffers: Start cleanup");
                    ImGuiContext* dyingContext = g_GameImGuiContext;
                    ImGuiContext* prev = ImGui::GetCurrentContext();
                    const bool previousWasDying = prev == dyingContext;
                    swiftLog("SwapBuffers: Calling InvalidateInGameImGuiContext");
                    InvalidateInGameImGuiContext(dyingContext);
                    ShutdownPlayerSkinLoaders();
                    InvalidatePlayerSkinTexturesAfterContextLoss();
                    swiftLog("SwapBuffers: Setting Context");
                    ImGui::SetCurrentContext(dyingContext);
                    swiftLog("SwapBuffers: Abandoning Nametags resources");
                    features::visual::nametags::abandon_render_resources();
                    features::visual::outline::release_gl();
                    features::combat::refill::cancel_on_game_frame();
                    features::combat::armor_switcher::cancel_on_game_frame();
                    features::misc::cancel_inputs_on_game_frame();
                    g_swiftContextLost = true;
                    swiftLog("SwapBuffers: Shutdown OpenGL3");
                    ImGui_ImplOpenGL3_Shutdown();
                    swiftLog("SwapBuffers: Shutdown Win32");
                    ImGui_ImplWin32_Shutdown();
                    swiftLog("SwapBuffers: DestroyContext (SKIPPED)");
                    // ImGui::DestroyContext(dyingContext); // CAUSES CRASH: Heap corruption or double free. Leaking it is safe.
                    g_swiftContextLost = false;
                    g_GameImGuiContext = nullptr;
                    ImGui::SetCurrentContext(previousWasDying ? nullptr : prev);
                    g_GameImGuiInit = false;
                    g_ImGuiReady = false;
                    swiftLog("SwapBuffers: Cleanup completed successfully");
                }
                // Notify DestructThread that cleanup finished.
                g_ImGuiCleanupDone.store(true);
            }
            else if (!g_GameImGuiInit)
            {
                // Nothing was initialized on this context, so cleanup is
                // already complete. Do not make the unload thread time out.
                g_ImGuiCleanupDone.store(true);
            }

            if (::g_OrigWndProc && g_GameWindow && IsWindow(g_GameWindow)) {
                SetWindowLongPtrA(g_GameWindow, GWLP_WNDPROC, (LONG_PTR)::g_OrigWndProc);
                ::g_OrigWndProc = nullptr;
            }

            return ((SwapBuffers_t)original_swap_buffers)(hdc);
        }

        // ?? RUTA NORMAL ??????????????????????????????????????????
        HWND current_hwnd = WindowFromDC(hdc);

        if (!current_hwnd || !IsWindow(current_hwnd))
            return ((SwapBuffers_t)original_swap_buffers)(hdc);

        char className[256];
        GetClassNameA(current_hwnd, className, sizeof(className));

        if (strcmp(className, "UchihaMenuClass") == 0)
            return ((SwapBuffers_t)original_swap_buffers)(hdc);

        if (!wglGetCurrentContext())
            return ((SwapBuffers_t)original_swap_buffers)(hdc);

        if (!wglGetCurrentDC() || wglGetCurrentDC() != hdc)
            return ((SwapBuffers_t)original_swap_buffers)(hdc);

        HGLRC currentGLRC = wglGetCurrentContext();

        // F11 briefly calls SwapBuffers without a usable current GL context.
        // Querying GL_VIEWPORT before this validation can dereference an
        // invalid driver dispatch table and crash the process.

        std::lock_guard<std::recursive_mutex> lock(hooks::render_mutex);

        // Recheck after acquiring the lock.
        if (!g_Running)
            return ((SwapBuffers_t)original_swap_buffers)(hdc);

        // F11 may submit temporary HWND/HGLRC pairs. Require the same plausible
        // game pair for several consecutive swaps before rebuilding ImGui.
        if (g_GameImGuiInit) {
            const bool contextChanged = g_LastHwnd != current_hwnd ||
                (g_LastGLRC != nullptr && g_LastGLRC != currentGLRC);
            
            if (contextChanged) {
                // Fullscreen briefly presents through a temporary GLRC.
                // Wait until the new pair survives several complete frames
                // before rebuilding. This preserves the old menu during the
                // transition but restores it after a real F11 switch.
                if (g_PendingHwnd == current_hwnd && g_PendingGLRC == currentGLRC)
                    ++g_PendingStableFrames;
                else {
                    g_PendingHwnd = current_hwnd;
                    g_PendingGLRC = currentGLRC;
                    g_PendingStableFrames = 1;
                }

				// A few confirmed frames are enough to reject the transient HDC/GLRC
				// pair produced by F11. Twelve frames made every toggle feel frozen.
                constexpr unsigned int kF11StableFrames = 3;
                if (g_PendingStableFrames < kF11StableFrames)
                    return ((SwapBuffers_t)original_swap_buffers)(hdc);

                const bool windowChanged = g_LastHwnd != current_hwnd;
                // If the OpenGL Context (GLRC) changes, we MUST completely rebuild ImGui.
                // The old fonts, textures, and VBOs are no longer valid on the new GLRC.
                g_ImGuiReady = false;
                g_GameImGuiInit = false;
                ImGuiContext* dyingContext = g_GameImGuiContext;
                ImGuiContext* prev = ImGui::GetCurrentContext();
                const bool previousWasDying = prev == dyingContext;
                InvalidateInGameImGuiContext(dyingContext);
                InvalidatePlayerSkinTexturesAfterContextLoss();
                ImGui::SetCurrentContext(dyingContext);
                g_swiftContextLost = true;
                // The old OpenGL context is already destroyed. Calling Shutdown here
                // would execute glDeleteTextures using the NEW context with OLD IDs,
                // destroying the game's new textures and causing a crash!
                // ImGui_ImplWin32_Shutdown();
                // ImGui::GetIO().BackendRendererUserData = nullptr;
                // ImGui::GetIO().Fonts->SetTexID(0);
                // ImGui::DestroyContext(dyingContext);
                g_swiftContextLost = false;
                g_GameImGuiContext = nullptr;
                ImGui::SetCurrentContext(previousWasDying ? nullptr : prev);
                
                if (windowChanged) {
                    if (::g_OrigWndProc && g_LastHwnd && IsWindow(g_LastHwnd)) {
                        SetWindowLongPtrA(g_LastHwnd, GWLP_WNDPROC, (LONG_PTR)::g_OrigWndProc);
                    }
                    ::g_OrigWndProc = nullptr;
                }
                
                g_LastHwnd = current_hwnd;
                g_LastGLRC = currentGLRC;
                g_PendingHwnd = nullptr;
                g_PendingGLRC = nullptr;
                g_PendingStableFrames = 0;
            }
        }
        else {
            g_LastHwnd = current_hwnd;
            g_LastGLRC = currentGLRC;
        }

        if (!g_GameImGuiInit) {
            char candidateClass[256] = { 0 };
            GetClassNameA(current_hwnd, candidateClass, sizeof(candidateClass));
            DWORD candidatePid = 0;
            GetWindowThreadProcessId(current_hwnd, &candidatePid);
            RECT candidateRect{};
            const bool hasRect = GetClientRect(current_hwnd, &candidateRect) != FALSE;
            const bool usableSurface = hasRect && candidateRect.right > candidateRect.left &&
                candidateRect.bottom > candidateRect.top;
            
            const bool plausibleWindow = candidatePid != 0 &&
                (g_GameProcessId == 0 || g_GameProcessId == candidatePid) &&
                usableSurface;
                
            if (!plausibleWindow)
                return ((SwapBuffers_t)original_swap_buffers)(hdc);

            if (g_PendingHwnd == current_hwnd && g_PendingGLRC == currentGLRC)
                ++g_PendingStableFrames;
            else {
                g_PendingHwnd = current_hwnd;
                g_PendingGLRC = currentGLRC;
                g_PendingStableFrames = 1;
            }

            constexpr unsigned int kStableFramesRequired = 6;
            if (g_PendingStableFrames < kStableFramesRequired)
                return ((SwapBuffers_t)original_swap_buffers)(hdc);

            ImGuiContext* prev = ImGui::GetCurrentContext();
            g_GameImGuiContext = ImGui::CreateContext();
            ImGui::SetCurrentContext(g_GameImGuiContext);

            ImGui_ImplWin32_Init(current_hwnd);

            ImGuiIO& io = ImGui::GetIO();
            io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;

            ImGui_ImplOpenGL3_Init();
            InitInGameImGui(current_hwnd);

            ImGui::SetCurrentContext(prev);
            g_GameImGuiInit = true;
            g_GameWindow = current_hwnd;
            if (g_GameProcessId == 0) {
                GetWindowThreadProcessId(current_hwnd, &g_GameProcessId);
                GetClassNameA(current_hwnd, g_GameWindowClass, sizeof(g_GameWindowClass));
            }
        }

        ProcessKeybinds();

        if (g_GameImGuiInit) {
            SafeRenderImGui(current_hwnd);
        }

        // ==========================================================
        // AQUI, al final de la renderizacion del frame.
        // la matriz, independientemente de si el cliente limpia el
        // depth buffer y color buffer juntos o por separado.
        // ==========================================================
        hooks::frame_matrices_captured = false;

        return ((SwapBuffers_t)original_swap_buffers)(hdc);
    }
}
