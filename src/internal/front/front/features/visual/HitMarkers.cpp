#include "../features.hpp"
#include "../sdk.hpp"
#include <imgui.h>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <chrono>
#include <cfloat>
#include <cmath>
#include <algorithm>
#include <Windows.h>

extern std::atomic<bool> g_PlayerInGui;

namespace {
struct HitEvent {
    mapper::__vec3 world;
    std::chrono::steady_clock::time_point born;
};
std::vector<HitEvent> g_hits;
std::unordered_map<int, int> g_previous_hurt;
std::mutex g_hit_mutex;
double g_hit_cam_x = 0.0, g_hit_cam_y = 0.0, g_hit_cam_z = 0.0;
std::chrono::steady_clock::time_point g_last_attack{};

static ImVec2 project_world(const mapper::__vec3& p, double cam_x, double cam_y, double cam_z) {
    mapper::__vec3 relative{
        (float)(p.x - cam_x),
        (float)(p.y - cam_y),
        (float)(p.z - cam_z)
    };
    auto screen = features::visual::world_to_screen(relative);
    return ImVec2(screen.x, screen.y);
}

static void draw_cross(ImDrawList* dl, ImVec2 c, float size, ImU32 color, float width) {
    const float gap = size * 0.28f;
    dl->AddLine({c.x - size, c.y - size}, {c.x - gap, c.y - gap}, color, width);
    dl->AddLine({c.x + gap, c.y + gap}, {c.x + size, c.y + size}, color, width);
    dl->AddLine({c.x + size, c.y - size}, {c.x + gap, c.y - gap}, color, width);
    dl->AddLine({c.x - gap, c.y + gap}, {c.x - size, c.y + size}, color, width);
}
}

void features::visual::hit_markers::run(mapper::__minecraft& minecraft) {
    if (!enabled) {
        std::lock_guard<std::mutex> lock(g_hit_mutex);
        g_hits.clear();
        g_previous_hurt.clear();
        return;
    }
    auto world = minecraft.get_world();
    auto local = minecraft.get_local_player();
    auto timer = minecraft.get_timer();
    if (!world.object || !local.object || !timer.object) return;

    auto rm = minecraft.get_render_manager();
    if (rm.object) {
        std::lock_guard<std::mutex> lock(g_hit_mutex);
        g_hit_cam_x = rm.get_render_pos_x();
        g_hit_cam_y = rm.get_render_pos_y();
        g_hit_cam_z = rm.get_render_pos_z();
    }

    const bool attacking = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    const float partial = timer.get_partial_ticks();
    auto local_pos = local.get_view_position(partial);
    auto players = world.get_players();
    const auto now = std::chrono::steady_clock::now();
    if (attacking) g_last_attack = now;
    const bool recent_attack = std::chrono::duration<float>(now - g_last_attack).count() <= 0.55f;

    for (auto& player : players) {
        if (!player.object || sdk::jni->IsSameObject(local.object, player.object)) continue;
        const int id = player.get_entity_id();
        const int hurt = player.get_hurt_time();
        const int previous = g_previous_hurt[id];
        g_previous_hurt[id] = hurt;
        if (!recent_attack || hurt < 8 || previous >= 8) continue;

        auto pos = player.get_view_position(partial);
        if (local_pos.get_distance_to_vec3(pos) > 6.5) continue;
        pos.y += 0.9f;
        std::lock_guard<std::mutex> lock(g_hit_mutex);
        g_hits.push_back({pos, now});
    }
}

void features::visual::hit_markers::render() {
    if (!enabled || g_PlayerInGui) return;
    const auto now = std::chrono::steady_clock::now();
    const float life = (std::max)(0.05f, duration);
    thread_local std::vector<HitEvent> hits;
    double cam_x, cam_y, cam_z;
    {
        std::lock_guard<std::mutex> lock(g_hit_mutex);
        g_hits.erase(std::remove_if(g_hits.begin(), g_hits.end(), [&](const HitEvent& h) {
            return std::chrono::duration<float>(now - h.born).count() >= life;
        }), g_hits.end());
        hits = g_hits;
        cam_x = g_hit_cam_x; cam_y = g_hit_cam_y; cam_z = g_hit_cam_z;
    }
    auto* dl = ImGui::GetBackgroundDrawList();
    if (!dl) return;

    for (const auto& hit : hits) {
        float progress = std::chrono::duration<float>(now - hit.born).count() / life;
        progress = (std::max)(0.f, (std::min)(progress, 1.f));
        const float alpha = fade_out ? (1.f - progress) : 1.f;
        const float anim_scale = scale_animation ? (scale_amount + (1.f - scale_amount) * progress) : 1.f;
        ImVec4 main_col(color[0], color[1], color[2], color[3] * alpha);
        ImVec4 out_col(outline_color[0], outline_color[1], outline_color[2], outline_color[3] * alpha);

        ImVec2 center;
        float marker_size = size * anim_scale;
        if (mode == 0) {
            const ImVec2 display = ImGui::GetIO().DisplaySize;
            center = { display.x * 0.5f, display.y * 0.5f };
        } else {
            // True world-space size: project two nearby points so perspective
            // naturally makes the marker smaller with distance.
            const float radius = size * 0.0125f * anim_scale;
            mapper::__vec3 left = hit.world, right = hit.world;
            left.x -= radius; right.x += radius;
            const ImVec2 a = project_world(left, cam_x, cam_y, cam_z), b = project_world(right, cam_x, cam_y, cam_z);
            center = project_world(hit.world, cam_x, cam_y, cam_z);
            if (a.x != FLT_MAX && b.x != FLT_MAX)
                marker_size = (std::max)(2.f, (std::min)(12.f, std::abs(b.x - a.x) * 0.5f));
        }
        if (center.x == FLT_MAX || center.y == FLT_MAX) continue;
        if (outline)
            draw_cross(dl, center, marker_size, ImGui::ColorConvertFloat4ToU32(out_col), line_width + outline_width * 2.f);
        draw_cross(dl, center, marker_size, ImGui::ColorConvertFloat4ToU32(main_col), line_width);
    }
}
