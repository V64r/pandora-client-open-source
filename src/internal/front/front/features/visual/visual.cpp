#include "../features.hpp"
#include <string>
#include <imgui.h>
#include <cfloat>
#include <unordered_map>
#include <vector>
#include <cmath>
#include <algorithm>

void* features::visual::window = nullptr;
double features::visual::model_view_matrix[16];
double features::visual::projection_matrix[16];
int features::visual::view_port[4];
double features::visual::render_camera_x = 0.0;
double features::visual::render_camera_y = 0.0;
double features::visual::render_camera_z = 0.0;
double features::visual::render_local_player_x = 0.0;
double features::visual::render_local_player_y = 0.0;
double features::visual::render_local_player_z = 0.0;
bool features::visual::render_local_player_valid = false;
float features::visual::render_partial_ticks = 0.0f;
int features::visual::render_world_tick = -1;
bool features::visual::render_frame_snapshot_valid = false;

auto features::visual::world_to_screen(mapper::__vec3 data, bool can_reverse, bool ignore_z) -> mapper::__vec2
{
    auto multiply_matrix = [&](mapper::__vec4 vec4, double matrix[16]) -> mapper::__vec4
        {
            mapper::__vec4 res;
            res.x = vec4.x * matrix[0] + vec4.y * matrix[4] + vec4.z * matrix[8] + vec4.w * matrix[12];
            res.y = vec4.x * matrix[1] + vec4.y * matrix[5] + vec4.z * matrix[9] + vec4.w * matrix[13];
            res.z = vec4.x * matrix[2] + vec4.y * matrix[6] + vec4.z * matrix[10] + vec4.w * matrix[14];
            res.w = vec4.x * matrix[3] + vec4.y * matrix[7] + vec4.z * matrix[11] + vec4.w * matrix[15];
            return res;
        };

    mapper::__vec4 initial_vec;
    initial_vec.x = data.x;
    initial_vec.y = data.y;
    initial_vec.z = data.z;
    initial_vec.w = 1.f;

    auto clipped_space_position = multiply_matrix(multiply_matrix(initial_vec, features::visual::model_view_matrix), features::visual::projection_matrix);
    mapper::__vec2 out_fail;
    out_fail.x = FLT_MAX;
    out_fail.y = FLT_MAX;

    if (!std::isfinite(clipped_space_position.x) ||
        !std::isfinite(clipped_space_position.y) ||
        !std::isfinite(clipped_space_position.z) ||
        !std::isfinite(clipped_space_position.w))
        return out_fail;

    constexpr double min_w = 0.0001;
    if ((!can_reverse && clipped_space_position.w <= min_w) ||
        (can_reverse && std::abs(clipped_space_position.w) <= min_w))
        return out_fail;

    mapper::__vec3 space_position;
    space_position.x = clipped_space_position.x / clipped_space_position.w;
    space_position.y = clipped_space_position.y / clipped_space_position.w;
    space_position.z = clipped_space_position.z / clipped_space_position.w;

    if (!std::isfinite(space_position.x) ||
        !std::isfinite(space_position.y) ||
        !std::isfinite(space_position.z))
        return out_fail;

    // Valores NDC desmedidos aparecen al cruzar el plano de la camara. No son
    if (!can_reverse && (std::abs(space_position.x) > 32.0 ||
                         std::abs(space_position.y) > 32.0))
        return out_fail;

    if ((space_position.z < -1.f || space_position.z > 1.f) && !can_reverse && !ignore_z)
        return out_fail;

    const float viewport_x = static_cast<float>(features::visual::view_port[0]);
    const float viewport_y = static_cast<float>(features::visual::view_port[1]);
    const float viewport_w = static_cast<float>(features::visual::view_port[2]);
    const float viewport_h = static_cast<float>(features::visual::view_port[3]);
    if (viewport_w <= 0.0f || viewport_h <= 0.0f)
        return out_fail;

    mapper::__vec2 out_success;
    if (can_reverse && clipped_space_position.w < 0.0)
    {
        out_success.x = viewport_x + viewport_w - (((space_position.x + 1.f) * 0.5f) * viewport_w);
        out_success.y = viewport_y + viewport_h - (((1.f - space_position.y) * 0.5f) * viewport_h);
        return out_success;
    }
    else
    {
        out_success.x = viewport_x + (((space_position.x + 1.f) * 0.5f) * viewport_w);
        out_success.y = viewport_y + (((1.f - space_position.y) * 0.5f) * viewport_h);
        return out_success;
    }
}

auto features::visual::render_nametag(std::string name, mapper::__vec3 vec3, mapper::__vec4 color, bool draw_health, float health, bool draw_distance, double distance, bool draw_hurt_time, __int32 hurt_time) -> void
{
    auto min_x = vec3.x - .35, min_y = vec3.y, min_z = vec3.z - .35, max_x = vec3.x + .35, max_y = vec3.y + 1.85, max_z = vec3.z + .35;

    mapper::__vec3 bounding_box[] = {
        { min_x, min_y, min_z }, { min_x, max_y, min_z }, { max_x, max_y, min_z }, { max_x, min_y, min_z },
        { max_x, max_y, max_z }, { min_x, max_y, max_z }, { min_x, min_y, max_z }, { max_x, min_y, max_z }
    };

    mapper::__vec4 bounding_box_screen_position;
    bounding_box_screen_position.x = DBL_MAX;
    bounding_box_screen_position.y = DBL_MAX;
    bounding_box_screen_position.z = DBL_MIN;
    bounding_box_screen_position.w = DBL_MIN;

    for (auto x = 0; x < 8; ++x)
    {
        auto screen_position = features::visual::world_to_screen({ bounding_box[x].x, bounding_box[x].y, bounding_box[x].z });
        if (screen_position.x == FLT_MAX || screen_position.y == FLT_MAX) continue;

        bounding_box_screen_position.x = min((double)screen_position.x, bounding_box_screen_position.x);
        bounding_box_screen_position.y = min((double)screen_position.y, bounding_box_screen_position.y);
        bounding_box_screen_position.z = max((double)screen_position.x, bounding_box_screen_position.z);
        bounding_box_screen_position.w = max((double)screen_position.y, bounding_box_screen_position.w);
    }

    if (bounding_box_screen_position.x == DBL_MAX || bounding_box_screen_position.y == DBL_MAX || bounding_box_screen_position.z == DBL_MIN || bounding_box_screen_position.w == DBL_MIN)
        return;

    mapper::__vec2 screen_position;
    screen_position.x = (float)(bounding_box_screen_position.x + (bounding_box_screen_position.z - bounding_box_screen_position.x) * .5);
    screen_position.y = (float)bounding_box_screen_position.y;

    auto name_size = ImGui::CalcTextSize(name.c_str());

    ImGui::GetBackgroundDrawList()->AddRectFilled(
        { screen_position.x - name_size.x * .5f - 4.f, screen_position.y - name_size.y - 7.f },
        { screen_position.x + name_size.x * .5f + 4.f, screen_position.y - 5.f },
        ImGui::ColorConvertFloat4ToU32({ .1175f, .1175f, .1175f, .8f })
    );


    if (draw_hurt_time) {
        ImGui::GetBackgroundDrawList()->AddRectFilled(
            { screen_position.x - name_size.x * .5f - 4.f, screen_position.y - name_size.y - 7.f },
            { screen_position.x + name_size.x * .5f + 4.f, screen_position.y - 5.f },
            ImGui::ColorConvertFloat4ToU32({ .8f, .35f, .35f, .5f * (float)hurt_time / 10.f })
        );
    }

    ImGui::GetBackgroundDrawList()->AddText(
        { (float)screen_position.x - name_size.x * .5f, (float)screen_position.y - name_size.y - 7.f },
        ImGui::ColorConvertFloat4ToU32({ (float)color.x, (float)color.y, (float)color.z, (float)color.w }),
        name.c_str()
    );

    if (draw_health) {
        mapper::__vec2 minimun_position;
        minimun_position.x = screen_position.x - name_size.x * .5f - 4.f;
        minimun_position.y = screen_position.y - 5.f;

        mapper::__vec2 maximun_position;
        maximun_position.x = screen_position.x + name_size.x * .5f + 4.f;
        maximun_position.y = screen_position.y - 2.f;

        ImGui::GetBackgroundDrawList()->AddRectFilled(
            { minimun_position.x, minimun_position.y }, { maximun_position.x, maximun_position.y },
            ImGui::ColorConvertFloat4ToU32({ .1175f, .1175f, .1175f, .8f })
        );

        ImGui::GetBackgroundDrawList()->AddRectFilled(
            { minimun_position.x + 1.f, minimun_position.y + 1.f },
            { minimun_position.x + (maximun_position.x - minimun_position.x) * health / 20.f - 1.f, maximun_position.y - 1.f },
            ImGui::ColorConvertFloat4ToU32({ (20.f - health) / 20.f, (health < 5.f ? 5.f : health) / 20.f, .35f, 1.f })
        );
    }

    if (draw_distance) {
        char fixed_distance[32];
        snprintf(fixed_distance, sizeof(fixed_distance), "%.2f", distance);
        auto fixed_distance_size = ImGui::CalcTextSize(fixed_distance);

        ImGui::GetBackgroundDrawList()->AddRectFilled(
            { (float)screen_position.x - fixed_distance_size.x * .5f - 4.f, (float)screen_position.y - name_size.y - fixed_distance_size.y - 15.f },
            { (float)screen_position.x + fixed_distance_size.x * .5f + 4.f, (float)screen_position.y - name_size.y - 13.f },
            ImGui::ColorConvertFloat4ToU32({ .1175f, .1175f, .1175f, .8f })
        );

        ImGui::GetBackgroundDrawList()->AddText(
            { (float)screen_position.x - fixed_distance_size.x * .5f, (float)screen_position.y - name_size.y - fixed_distance_size.y - 7.f },
            ImGui::ColorConvertFloat4ToU32({ (2.f - ((float)distance < 5. ? 5.f : (float)distance) / 20.f), ((float)distance < 5. ? 5.f : (float)distance) / 20.f, 0.f, (float)color.w }),
            fixed_distance
        );
    }
}

auto features::visual::render_tracer(mapper::__vec3 vec3, mapper::__vec4 color, bool draw_distance, double distance, bool draw_hurt_time, __int32 hurt_time) -> void
{
    auto screen_position = features::visual::world_to_screen(vec3, true);
    if (screen_position.x == FLT_MAX || screen_position.y == FLT_MAX) return;

    ImGui::GetBackgroundDrawList()->AddLine(
        { (float)features::visual::view_port[2] * .5f, (float)features::visual::view_port[3] * .5f },
        { screen_position.x, screen_position.y },
        ImGui::ColorConvertFloat4ToU32({ (float)color.x, (float)color.y, (float)color.z, (float)color.w })
    );

    if (draw_distance) {
        ImGui::GetBackgroundDrawList()->AddLine({ (float)features::visual::view_port[2] * .5f, (float)features::visual::view_port[3] * .5f }, { screen_position.x, screen_position.y }, ImGui::ColorConvertFloat4ToU32({ (2.f - ((float)distance < 5. ? 5.f : (float)distance) / 20.f), ((float)distance < 5. ? 5.f : (float)distance) / 20.f, 0.f, (float)color.w }), 2.f);
    }
    if (draw_hurt_time) {
        ImGui::GetBackgroundDrawList()->AddLine({ (float)features::visual::view_port[2] * .5f, (float)features::visual::view_port[3] * .5f }, { screen_position.x, screen_position.y }, ImGui::ColorConvertFloat4ToU32({ .8f, .35f, .35f, (float)color.w * (float)hurt_time / 10.f }), 2.f);
    }
}
