#include "../features.hpp"
#include "sdk.hpp"
#define NOMINMAX
#include <windows.h>
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <chrono>
#include <random>
#include <vector>

extern std::atomic<bool> g_MenuVisible;

namespace features::combat::aim_assist {
namespace {
int locked_entity_id = 0;
bool lock_valid = false;
float acquired_fov = 180.0f;
std::mt19937 rng{std::random_device{}()};

float precise_time_ms() {
    static const auto start = std::chrono::high_resolution_clock::now();
    const auto now = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<float, std::milli>(now - start).count();
}

void clear_target() {
    locked_entity_id = 0;
    lock_valid = false;
    angle_x_changes = 0.0f;
    angle_y_changes = 0.0f;
}

float choose_fov() {
    const float low = static_cast<float>((std::max)(0.0, (std::min)(minimum_fov, maximum_fov)));
    const float high = static_cast<float>((std::clamp)((std::max)(minimum_fov, maximum_fov), 0.0, 360.0));
    return std::uniform_real_distribution<float>(low, (std::max)(low, high))(rng);
}

bool valid_weapon(mapper::__player& local) {
    if (!weapons_only) return true;
    auto stack = local.get_held_item_stack();
    if (!stack.object) return false;
    auto item = stack.get_item();
    return item.object && (item.is_sword() || item.is_axe());
}
}

void run(mapper::__minecraft& minecraft) {
    const bool clicking = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    const bool locked_mode_requires_click = mode == 1 || target_mode == 0;
    if (!enabled || g_MenuVisible || ((clicking_only || locked_mode_requires_click) && !clicking) || minecraft.get_current_screen().object) {
        clear_target();
        return;
    }
    auto world = minecraft.get_world();
    auto local = minecraft.get_local_player();
    auto timer = minecraft.get_timer();
    if (!world.object || !local.object || !timer.object || !valid_weapon(local)) { clear_target(); return; }
    if (break_blocks) {
        auto hit = minecraft.get_object_mouse_over();
        if (hit.object && hit.get_type_of_hit() == 1) { clear_target(); return; }
    }

    auto players = world.get_players();
    auto eye = local.get_view_position(timer.get_partial_ticks());
    eye.y += 1.62;
    auto angles = local.get_view_angles();
    const double min_distance = (std::max)(0.0, (std::min)(minimum_distance, maximum_distance));
    const double max_distance = (std::max)(min_distance, (std::max)(minimum_distance, maximum_distance));
    const bool force_lock = mode == 1 || target_mode == 0;
    mapper::__player* selected = nullptr;
    mapper::__vec2 selected_diff{FLT_MAX, FLT_MAX};
    float best_score = FLT_MAX;

    auto consider = [&](mapper::__player& player, bool require_locked) {
        if (!player.object || sdk::jni->IsSameObject(player.object, local.object)) return;
        const int entity_id = player.get_entity_id();
        if (require_locked && (!lock_valid || entity_id != locked_entity_id)) return;
        const float health = player.get_health();
        if (!std::isfinite(health) || health <= 0.0f || player.is_invisible() && ignore_invisible) return;
        if (!through_walls && !local.can_entity_be_seen(player)) return;
        if (features::friends::is_teammate(player, local)) return;
        auto position = player.get_position();
        auto old_position = player.get_old_position();
        const float pt = timer.get_partial_ticks();
        mapper::__vec3 interp_pos;
        interp_pos.x = old_position.x + (position.x - old_position.x) * pt;
        interp_pos.y = old_position.y + (position.y - old_position.y) * pt;
        interp_pos.z = old_position.z + (position.z - old_position.z) * pt;
        const double distance = eye.get_distance_to_vec3(interp_pos);
        if (distance < min_distance || distance > max_distance) return;
        const float yaw = eye.get_angle_x_difference_to_vec3(interp_pos, angles.x);
        auto head_pos = interp_pos;
        head_pos.y += 1.62;
        const float pitch = eye.get_angle_y_difference_to_vec3(head_pos, angles.y);
        const float angular = std::abs(yaw);
        if (angular > acquired_fov * 0.5f) return;
        float score = angular;
        if (priority == 1) score = static_cast<float>(distance);
        else if (priority == 2) score = health;
        if (!selected || score < best_score) {
            selected = &player;
            selected_diff = {yaw, pitch};
            best_score = score;
        }
    };

    if (force_lock && lock_valid) {
        for (auto& player : players) consider(player, true);
        if (!selected) clear_target();
    }
    if (!selected && !lock_valid) {
        acquired_fov = choose_fov();
        for (auto& player : players) consider(player, false);
        if (selected && force_lock) { locked_entity_id = selected->get_entity_id(); lock_valid = true; }
    } else if (!force_lock) {
        selected = nullptr; best_score = FLT_MAX;
        for (auto& player : players) consider(player, false);
    }
    if (!selected) { angle_x_changes = angle_y_changes = 0.0f; return; }

    float move_x = 0.0f, move_y = 0.0f;
    if (mode == 1) {
        const float smooth_factor = 0.35f;
        move_x = selected_diff.x * smooth_factor;
        move_y = selected_diff.y * smooth_factor;
        const float max_turn = 15.0f;
        move_x = (std::clamp)(move_x, -max_turn, max_turn);
        move_y = (std::clamp)(move_y, -max_turn, max_turn);
    } else {
        const float horizontal = static_cast<float>((std::clamp)(horizontal_speed, 1.0, 100.0));
        const float vertical = static_cast<float>((std::clamp)(vertical_speed, 1.0, 100.0));
        const float now = precise_time_ms();
        const float wave = std::sin(now * 0.003f) * 0.10f;

        if (aim_mode == 0 || aim_mode == 2) {
            float smooth = (std::pow(horizontal, 1.5f) / 10000.0f) * (1.0f + wave);
            const float target = selected_diff.x + std::sin(now * 0.0015f) * 1.2f;
            const float distance = std::abs(target);
            if (distance < 2.0f) smooth *= (std::clamp)(distance / 2.0f, 0.1f, 1.0f);
            const float limit = 0.5f + horizontal / 20.0f;
            move_x = (std::clamp)(target * smooth, -limit, limit);
        }
        if (aim_mode == 1 || aim_mode == 2) {
            float smooth = (std::pow(vertical, 1.5f) / 10000.0f) * (1.0f + wave);
            const float distance = std::abs(selected_diff.y);
            if (distance < 2.0f) smooth *= (std::clamp)(distance / 2.0f, 0.1f, 1.0f);
            const float limit = 0.5f + vertical / 20.0f;
            move_y = (std::clamp)(selected_diff.y * smooth, -limit, limit);
        }
    }
    if (silent) { angle_x_changes = move_x; angle_y_changes = move_y; }
    else {
        angle_x_changes = angle_y_changes = 0.0f;
        local.set_old_view_angles(angles);
        local.set_view_angles({angles.x + move_x, (std::clamp)(angles.y + move_y, -90.0f, 90.0f)});
    }
    if (sdk::jni->ExceptionCheck()) sdk::jni->ExceptionClear();
}
}
