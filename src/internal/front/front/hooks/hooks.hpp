#pragma once
#include <windows.h>
#include <mutex>
#include <atomic>
#include <jni.h>

struct ImGuiContext; // forward declaration

namespace hooks
{
    // Refresh camera and ActiveRenderInfo matrices on the render thread, after
    // Minecraft has completed the current world frame.
    bool capture_render_frame_java_state();
    // Every callback that can execute code from this DLL participates in the
    // unload barrier. MinHook/JNI detachment prevents new entries; the final
    // unload then waits until all callbacks that were already in flight leave.
    extern std::atomic<unsigned long> active_callbacks;
    struct callback_guard {
        callback_guard() { active_callbacks.fetch_add(1, std::memory_order_acq_rel); }
        ~callback_guard() { active_callbacks.fetch_sub(1, std::memory_order_acq_rel); }
        callback_guard(const callback_guard&) = delete;
        callback_guard& operator=(const callback_guard&) = delete;
    };
    extern void wait_for_callbacks();

    extern auto initialize() -> __int32;
    extern auto uninitialize_jni() -> void;
    extern auto uninitialize() -> __int32;

    // --- SWAP BUFFERS ---
    inline void* original_swap_buffers = nullptr;
    extern auto swap_buffers(HDC hdc) -> __int32;


    // --- GL CLEAR (MinHook directo sobre opengl32.glClear) ---
    // limpia el depth buffer antes de renderizar entidades.
    inline void* original_gl_clear = nullptr;
    extern std::atomic<bool> frame_matrices_captured;
    extern std::atomic<DWORD> main_thread_id;
    extern auto gl_clear_hook(unsigned int mask) -> void;

    // Mutex global render
    inline std::recursive_mutex render_mutex;

    // ImGui cleanup flag for destruct synchronization
    extern std::atomic<bool> g_ImGuiCleanupDone;
    extern ImGuiContext* g_GameImGuiContext;
    void abandon_imgui_after_render_timeout();
}

namespace network_hooks
{
    void flush_blink_packets();
}
