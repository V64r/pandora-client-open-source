#pragma once
#include <windows.h>
#include <string>
#include <memory>
#include <vector>
#include <random>
#include <imgui.h>
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx11.h"
#include <d3d11.h>
#include <dxgi.h>
#include <chrono>

namespace gui {

    struct ProcessInfo {
        std::string name;
        std::string window_title;
        DWORD pid = 0;
        bool is_selected = false;
        float hover_animation = 0.0f;
        bool is_hovered = false;
        bool was_visible = false;
    };

    class c_gui {
    public:
        c_gui();
        ~c_gui();

        bool initialize(HWND hwnd);
        void render();
        void shutdown();
        void refresh_processes();

        void check_window_focus();
        void refresh_single_process(DWORD pid);

        void start_injection();
        void perform_injection();

        bool show_window = true;
        std::string selected_process = "";
        bool inject_button_clicked = false;

        float slide_animation = 0.0f;
        bool slide_started = false;
        float fade_animation = 1.0f;
        bool is_fading = false;
        float load_animation = 0.0f;
        bool is_loading = false;
        float load_timer = 0.0f;
        bool injection_done = false;

        float close_timer = 0.0f;
        int close_counter = 3;

        bool should_close = false;

        std::vector<ProcessInfo> processes;
        int selected_index = -1;

        IDXGISwapChain* swap_chain = nullptr;

        std::mt19937 rng;

        ImFont* fontMain = nullptr;
        ImFont* fontSmall = nullptr;
        ImFont* fontBig = nullptr;

        ImTextureID logoTexture = (ImTextureID)0;
        int logoWidth = 0;
        int logoHeight = 0;

        bool instances_shown = false;
        bool logo_animation_complete = false;
        float logo_slide_to_center = 0.0f;
        float logo_scale_effect = 1.0f;
        bool logo_selected_animation = false;

        DWORD last_foreground_pid = 0;
        std::chrono::steady_clock::time_point last_check_time;
        bool first_check_done = false;
        HWND window_handle = nullptr;

        bool force_refresh = false;

        float load_progress = 0.0f;
        bool load_complete = false;
        bool dll_found = false;

        float logo_slide_offset = 0.0f;
        float instances_fade = 0.0f;
        bool logo_sliding = true;
        bool instances_fading = false;
        float loading_fade = 0.0f;
        bool loading_fading = false;

        float window_fade = 0.0f;

        float phase_timer = 0.0f;
        int current_phase = 0;
        bool phase_paused = false;
        float pause_timer = 0.0f;
        bool injection_executed = false;

    private:
        ID3D11Device* d3d_device = nullptr;
        ID3D11DeviceContext* d3d_device_context = nullptr;
        ID3D11RenderTargetView* render_target_view = nullptr;

        bool create_device_d3d(HWND hwnd);
        void cleanup_device_d3d();
        void create_render_target();
        void cleanup_render_target();

        bool load_logo_from_memory(const unsigned char* data, size_t dataSize);

        bool check_dll_exists();
    };

    inline std::unique_ptr<c_gui> instance;

}