#include "../misc/InputSupport.hpp"
#include <algorithm>
#include <cmath>

namespace features::movement::bunnyhop {
namespace {
ULONGLONG last_jump = 0;
int last_tick = -1;
double player_speed = 0.0;
bool slowdown = false;
float bounded(float value, float fallback, float low, float high) {
    return std::clamp(std::isfinite(value) ? value : fallback, low, high);
}
}
void cancel() {
    last_jump = 0;
    last_tick = -1;
    player_speed = 0.0;
    slowdown = false;
}
void run(mapper::__minecraft& minecraft) {
    if (!enabled) { cancel(); return; }
    JNIEnv* env = sdk::jni;
    auto player = minecraft.get_local_player();
    if (!player.object || env->ExceptionCheck()) return;
    const int tick = player.get_ticks_existed();
    if (env->ExceptionCheck() || tick == last_tick) return;
    last_tick = tick; // Movement is tick-based, never render-FPS based.
    if (liquid_check) {
        const auto water = misc::input_support::field("EntityPlayerXP", "Z", {"inWater", "field_70171_ac"});
        const auto water_method = misc::input_support::method("EntityPlayerXP", "()Z", {"isInWater", "func_70090_H", "V"});
        if ((!water && !water_method) || (water ? env->GetBooleanField(player.object, water) :
            env->CallBooleanMethod(player.object, water_method))) {
            player_speed = 0.0; slowdown = false; return;
        }
    }
    if (env->ExceptionCheck()) return;
    float forward = player.get_move_foreward(), side = player.get_move_strafing();
    const bool moving_now = std::abs(forward) + std::abs(side) > 0.001f;
    if (env->ExceptionCheck()) return;
    if (only_moving && !moving_now) { player_speed = 0.0; slowdown = false; return; }
    auto motion = player.get_motion();
    float yaw = player.get_view_angles().x;
    const bool grounded = player.get_on_ground();
    if (env->ExceptionCheck() || !std::isfinite(yaw) || !std::isfinite(forward) ||
        !std::isfinite(side) || !std::isfinite(motion.y)) return;
    const auto now = GetTickCount64();
    const double base = (bounded(power, 1.45f, 0.1f, 4.f) / 1.45) * 0.2873;
    if (grounded && now - last_jump >= static_cast<ULONGLONG>(bounded(jump_delay, 0.f, 0.f, 1000.f))) {
        last_jump = now;
        motion.y = bounded(jump_height, 0.42f, 0.f, 1.f);
        player_speed = base * bounded(speed_multiplier, 1.6f, 0.1f, 3.f);
        slowdown = true;
    } else if (slowdown) {
        // The supplied code assigned playerSpeed inside this expression;
        // subtract from the existing speed without overwriting it first.
        player_speed -= bounded(slowdown_factor, 0.66f, 0.f, 1.f) * 0.2873;
        slowdown = false;
    } else {
        player_speed -= player_speed / bounded(friction, 159.f, 2.f, 200.f);
    }
    player_speed = (std::max)(player_speed, base);
    if (forward != 0.f) {
        if (side > 0.f) yaw += forward > 0.f ? -45.f : 45.f;
        else if (side < 0.f) yaw += forward > 0.f ? 45.f : -45.f;
        side = 0.f;
        forward = forward > 0.f ? 1.f : -1.f;
    }
    const double angle = (yaw + 90.0) * 3.14159265358979323846 / 180.0;
    const double x = forward * player_speed * std::cos(angle) + side * player_speed * std::sin(angle);
    const double z = forward * player_speed * std::sin(angle) - side * player_speed * std::cos(angle);
    const double limit = bounded(direction_threshold, 10.f, 0.1f, 10.f);
    if (std::abs(x) < limit && std::abs(z) < limit) { motion.x = x; motion.z = z; }
    player.set_motion(motion);
}
}
