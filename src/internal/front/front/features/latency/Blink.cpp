#include "../features.hpp"
#include "../../hooks/hooks.hpp"
#include "sdk.hpp" 
#include "backends/imgui.h"
#define NOMINMAX
#include <windows.h>
#include <vector>
#include <mutex>
#include <cmath>
#include <cfloat>
#include <gl/GL.h>

extern bool gui_blink_enabled;

namespace features::latency::blink
{
    std::atomic_bool is_blinking{ false };
    static ULONGLONG start_time = 0;
    static std::vector<mapper::__vec3> path_coords;
    static std::mutex path_mutex;

    static const float HARD_CAP_SECONDS = 3.0f;

    void run(mapper::__minecraft& minecraft)
    {
        auto world = minecraft.get_world();
        if (world.object == nullptr) {
            if (is_blinking.exchange(false, std::memory_order_acq_rel))
                network_hooks::flush_blink_packets();
            return;
        }

        if (!enabled) {
            if (is_blinking.exchange(false, std::memory_order_acq_rel))
                network_hooks::flush_blink_packets();
            std::lock_guard<std::mutex> lock(path_mutex);
            path_coords.clear();
            return;
        }

        if (!is_blinking.exchange(true, std::memory_order_acq_rel)) {
            start_time = GetTickCount64();
            std::lock_guard<std::mutex> lock(path_mutex);
            path_coords.clear();
        }

        ULONGLONG now = GetTickCount64();

        float effective_limit = timer_limit;
        if (effective_limit > HARD_CAP_SECONDS) effective_limit = HARD_CAP_SECONDS;

        if (now - start_time > (ULONGLONG)(effective_limit * 1000.0f)) {
            is_blinking.store(false, std::memory_order_release);
            enabled = false;
            gui_blink_enabled = false;
            network_hooks::flush_blink_packets();
            std::lock_guard<std::mutex> lock(path_mutex);
            path_coords.clear();
            return;
        }

        auto local_player = minecraft.get_local_player();
        if (local_player.object != nullptr) {
            mapper::__vec3 current_pos = local_player.get_position();
            std::lock_guard<std::mutex> lock(path_mutex);
            if (path_coords.empty() || path_coords.back().get_distance_to_vec3(current_pos) > 0.1) {
                path_coords.push_back(current_pos);
            }
        }
    }

    void render_ui()
    {
        if (!enabled || !is_blinking.load(std::memory_order_acquire)) return;

        ImDrawList* draw = ImGui::GetBackgroundDrawList();
        if (!draw) return;

        if (show_path && features::visual::render_frame_snapshot_valid) {
            std::vector<mapper::__vec3> path;
            {
                std::lock_guard<std::mutex> lock(path_mutex);
                path = path_coords;
            }
            const ImU32 path_col = ImGui::ColorConvertFloat4ToU32(
                ImVec4(path_color[0], path_color[1], path_color[2], 0.9f));
            bool have_previous = false;
            ImVec2 previous{};
            for (auto point : path) {
                point.x -= features::visual::render_camera_x;
                point.y -= features::visual::render_camera_y;
                point.z -= features::visual::render_camera_z;
                const auto screen = features::visual::world_to_screen(point);
                if (screen.x == FLT_MAX || screen.y == FLT_MAX) {
                    have_previous = false;
                    continue;
                }
                const ImVec2 current(screen.x, screen.y);
                if (have_previous) draw->AddLine(previous, current, path_col, 2.0f);
                previous = current;
                have_previous = true;
            }
        }

        if (!show_timer) return;

        ULONGLONG elapsed = GetTickCount64() - start_time;
        float effective_limit = timer_limit;
        if (effective_limit > HARD_CAP_SECONDS) effective_limit = HARD_CAP_SECONDS;
        float fraction = (float)elapsed / (effective_limit * 1000.0f);
        if (fraction > 1.0f) fraction = 1.0f;

        ImVec2 screen_center = ImVec2(ImGui::GetIO().DisplaySize.x / 2.0f, ImGui::GetIO().DisplaySize.y / 2.0f);

        float bar_width = 200.0f;
        float bar_height = 8.0f;
        ImVec2 bar_pos = ImVec2(screen_center.x - (bar_width / 2.0f), screen_center.y + 30.0f);

        ImU32 col_bg = ImGui::ColorConvertFloat4ToU32(ImVec4(0.10f, 0.10f, 0.10f, 0.85f));
        ImU32 col_bar = ImGui::ColorConvertFloat4ToU32(ImVec4(path_color[0], path_color[1], path_color[2], 1.0f));

        draw->AddRectFilled(bar_pos, ImVec2(bar_pos.x + bar_width, bar_pos.y + bar_height), col_bg, 4.0f);
        draw->AddRectFilled(bar_pos, ImVec2(bar_pos.x + (bar_width * fraction), bar_pos.y + bar_height), col_bar, 4.0f);

        const char* mode_text = (mode == 0) ? "SMOOTH" : "FREEZE";
        ImVec2 textSz = ImGui::CalcTextSize(mode_text);
        draw->AddText(ImVec2(screen_center.x - textSz.x * 0.5f, bar_pos.y - 16.0f),
            ImGui::ColorConvertFloat4ToU32(ImVec4(path_color[0], path_color[1], path_color[2], 0.8f)), mode_text);
    }
}
