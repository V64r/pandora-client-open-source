#include "../features.hpp"
#include "../sdk.hpp"
#define NOMINMAX
#include <windows.h>
#include <cmath>
#include <random>
#include <algorithm>

extern std::atomic<bool> g_MenuVisible;

// =============================================================
// Reach: BYPASS EXTREMO
// =============================================================
// Tecnicas anti-deteccion implementadas:
//   3. Limita golpes extendidos consecutivos (patron humano)
//   4. No ataca durante knockback (hurtTime > 0)
//   8. No produce swing duplicado si vanilla ya swingeo
// =============================================================

namespace features::combat::reach
{
    static ULONGLONG s_last_attack = 0;
    static int       s_consecutive_extended = 0;

    static std::mt19937& get_rng() {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        return gen;
    }

    static bool roll_chance(float chance_val)
    {
        if (chance_val >= 100.0f) return true;
        if (chance_val <= 0.0f)   return false;
        std::uniform_real_distribution<float> distr(0.0f, 100.0f);
        return distr(get_rng()) <= chance_val;
    }

    static bool ray_hits_box(const mapper::__vec3& origin, const mapper::__vec3& direction,
        const mapper::__vec3& minimum, const mapper::__vec3& maximum,
        double max_distance, double& hit_distance)
    {
        double near_t = 0.0;
        double far_t = max_distance;
        const double origins[3] = { origin.x, origin.y, origin.z };
        const double directions[3] = { direction.x, direction.y, direction.z };
        const double mins[3] = { minimum.x, minimum.y, minimum.z };
        const double maxs[3] = { maximum.x, maximum.y, maximum.z };

        for (int axis = 0; axis < 3; ++axis) {
            if (std::abs(directions[axis]) < 1.0e-8) {
                if (origins[axis] < mins[axis] || origins[axis] > maxs[axis]) return false;
                continue;
            }
            double first = (mins[axis] - origins[axis]) / directions[axis];
            double second = (maxs[axis] - origins[axis]) / directions[axis];
            if (first > second) std::swap(first, second);
            near_t = (std::max)(near_t, first);
            far_t = (std::min)(far_t, second);
            if (near_t > far_t) return false;
        }
        hit_distance = near_t;
        return near_t >= 0.0 && near_t <= max_distance;
    }

    static jmethodID s_attack_mid = nullptr;
    static jmethodID find_attack_method(JNIEnv* env, jclass pc_class)
    {
        if (s_attack_mid) return s_attack_mid;
        if (!env || !pc_class) return nullptr;

        std::string ep_sig = mapper::classes["EntityPlayer"].signature;
        std::string e_sig = mapper::classes["Entity"].signature;

        std::string sigs[] = {
            "(" + ep_sig + e_sig + ")V"
        };

        const char* names[] = { "attackEntity", "func_78764_a", "a", "b", "c", "d", "e" };
        
        for (auto& sig : sigs) {
            for (const char* name : names) {
                jmethodID mid = env->GetMethodID(pc_class, name, sig.c_str());
                if (mid) {
                    s_attack_mid = mid;
                    return mid;
                }
                env->ExceptionClear();
            }
        }

        return nullptr;
    }

    static jmethodID s_swing_mid = nullptr;
    static jmethodID find_swing_method(JNIEnv* env, jclass player_class)
    {
        if (s_swing_mid) return s_swing_mid;
        if (!env || !player_class) return nullptr;

        const char* names[] = { "swingItem", "func_71038_i", "bw", "bv", "bx", "bu", "aZ" };
        for (const char* name : names)
        {
            jmethodID mid = env->GetMethodID(player_class, name, "()V");
            if (mid) {
                s_swing_mid = mid;
                return mid;
            }
            env->ExceptionClear();
        }

        return nullptr;
    }

    static jfieldID s_pc_fid = nullptr;
    static jobject get_player_controller(JNIEnv* env, jobject mc_obj)
    {
        if (!env || !mc_obj) return nullptr;
        jclass mc_class = env->GetObjectClass(mc_obj);
        if (!mc_class) return nullptr;

        if (!s_pc_fid) {
            std::string pc_sig = mapper::classes["PlayerControllerMP"].signature;
            const char* field_names[] = { "playerController", "field_71442_b", "c", "b", "d" };

            for (auto name : field_names) {
                s_pc_fid = env->GetFieldID(mc_class, name, pc_sig.c_str());
                if (s_pc_fid) break;
                env->ExceptionClear();
            }
        }

        env->DeleteLocalRef(mc_class);
        if (!s_pc_fid) return nullptr;

        return env->GetObjectField(mc_obj, s_pc_fid);
    }

    void run(mapper::__minecraft& minecraft)
    {
        if (!enabled || !sdk::jni) return;
        if (sdk::jni->ExceptionCheck()) sdk::jni->ExceptionClear();

        // ============ CHECKS DE SEGURIDAD ============
        if (g_MenuVisible) return;

        HWND window = (HWND)features::visual::window;
        if (window && GetForegroundWindow() != window) return;

        auto current_screen = minecraft.get_current_screen();
        if (current_screen.object != nullptr) return;

        bool is_clicking = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
        if (!is_clicking) {
            s_consecutive_extended = 0;
            return;
        }

        ULONGLONG now = GetTickCount64();

        // ============ COOLDOWN DINAMICO ============
        std::uniform_int_distribution<int> cd_dist(45, 55);
        int dynamic_cooldown = cd_dist(get_rng());
        if (now - s_last_attack < (ULONGLONG)dynamic_cooldown) return;

        auto world = minecraft.get_world();
        auto local_player = minecraft.get_local_player();
        if (world.object == nullptr || local_player.object == nullptr) return;

        // ============ BYPASS: NO INTERFERIR CON VANILLA ============
        auto pointed = minecraft.get_pointed_entity();
        if (pointed.object != nullptr) {
            float hp = pointed.get_health();
            if (sdk::jni->ExceptionCheck()) sdk::jni->ExceptionClear();
            else if (hp > 0.f && hp <= 100.f) {
                s_consecutive_extended = 0;
                return;
            }
        }

        int hurt_time = local_player.get_hurt_time();
        if (hurt_time > 0) return;

        if (!roll_chance(chance)) return;
        if (ground_only && !local_player.get_on_ground()) return;

        auto local_feet = local_player.get_position();
        if (liquid_check && local_player.is_offset_position_in_liquid(
            local_feet.x, local_feet.y, local_feet.z)) return;

        if (weapon_only)
        {
            auto held_stack = local_player.get_held_item_stack();
            if (held_stack.object == nullptr) return;
            auto item = held_stack.get_item();
            if (item.object == nullptr || !item.is_sword()) return;
        }

        auto timer = minecraft.get_timer();
        if (timer.object == nullptr) return;
        float p_ticks = timer.get_partial_ticks();

        mapper::__vec3 local_eye = local_player.get_view_position(p_ticks);
        local_eye.y += 1.62;

        mapper::__vec2 local_angles = local_player.get_view_angles();
        auto world_entities = world.get_players();

        float safe_min = min_distance;
        float safe_max = max_distance;
        if (safe_min > safe_max) safe_max = safe_min;
        
        std::uniform_real_distribution<float> dist_rand(safe_min, safe_max);
        float current_reach = dist_rand(get_rng());

        constexpr double degrees_to_radians = 3.14159265358979323846 / 180.0;
        const double yaw = local_angles.x * degrees_to_radians;
        const double pitch = local_angles.y * degrees_to_radians;
        const double pitch_cos = std::cos(pitch);
        const mapper::__vec3 ray_direction{
            -std::sin(yaw) * pitch_cos,
            -std::sin(pitch),
            std::cos(yaw) * pitch_cos
        };

        jobject best_target = nullptr;
        double best_dist = static_cast<double>(current_reach) + 1.0;

        for (auto& entity : world_entities)
        {
            if (entity.object == nullptr) continue;
            if (sdk::jni->IsSameObject(local_player.object, entity.object)) continue;

            float hp = entity.get_health();
            if (sdk::jni->ExceptionCheck()) { sdk::jni->ExceptionClear(); continue; }
            if (!std::isfinite(hp) || hp <= 0.f) continue;
            if (!hit_through_walls && !local_player.can_entity_be_seen(entity)) continue;

            mapper::__vec3 target_position = entity.get_view_position(p_ticks);
            if (sdk::jni->ExceptionCheck()) { sdk::jni->ExceptionClear(); continue; }
            const double expansion = hitbox_enabled ? (std::max)(0.0f, hitbox_size) : 0.0;
            const double half_width = 0.30 + expansion;
            const mapper::__vec3 box_min{
                target_position.x - half_width,
                target_position.y - expansion,
                target_position.z - half_width
            };
            const mapper::__vec3 box_max{
                target_position.x + half_width,
                target_position.y + 1.80 + expansion,
                target_position.z + half_width
            };
            double ray_distance = 0.0;
            if (!ray_hits_box(local_eye, ray_direction, box_min, box_max,
                    static_cast<double>(current_reach), ray_distance)) continue;

            if (!hit_through_walls && ray_distance <= 3.0) continue;
            if (ray_distance < best_dist) {
                best_dist = ray_distance;
                best_target = entity.object;
            }
        }

        // Combo mode — no atacar si estas recibiendo KB
        if (combo_mode && best_target != nullptr)
        {
            if (local_player.get_hurt_time() >= 8) return;
        }

        if (best_target == nullptr) return;

        // ============ BYPASS: LIMITAR HITS CONSECUTIVOS ============
        // Los anticheats cuentan cuantos hits extendidos das seguidos.
        s_consecutive_extended++;
        if (s_consecutive_extended > 3) {
            // Probabilidad de skip aumenta con cada hit consecutivo
            std::uniform_int_distribution<int> skip_dist(0, 100);
            int skip_roll = skip_dist(get_rng());
            if (skip_roll < 40) { // 40% chance de skipear despues del 3er hit
                s_consecutive_extended = 0;
                return;
            }
        }
        if (s_consecutive_extended > 6) {
            // Forzar skip despues de 6 hits extendidos seguidos
            s_consecutive_extended = 0;
            return;
        }

        s_last_attack = now;

        // ============ ATAQUE VIA PlayerControllerMP ============
        JNIEnv* env = sdk::jni;

        jobject pc_obj = get_player_controller(env, minecraft.object);
        if (!pc_obj) return;

        jclass pc_class = env->GetObjectClass(pc_obj);
        if (!pc_class) { env->DeleteLocalRef(pc_obj); return; }

        jmethodID atk_mid = find_attack_method(env, pc_class);
        if (!atk_mid) {
            env->DeleteLocalRef(pc_class);
            env->DeleteLocalRef(pc_obj);
            return;
        }

        env->CallVoidMethod(pc_obj, atk_mid, local_player.object, best_target);
        if (env->ExceptionCheck()) env->ExceptionClear();

        env->DeleteLocalRef(pc_class);
        env->DeleteLocalRef(pc_obj);

        // Swing arm animation
        jclass player_class = env->GetObjectClass(local_player.object);
        if (player_class) {
            jmethodID swing_mid = find_swing_method(env, player_class);
            if (swing_mid) {
                env->CallVoidMethod(local_player.object, swing_mid);
                if (env->ExceptionCheck()) env->ExceptionClear();
            }
            env->DeleteLocalRef(player_class);
        }

        if (env->ExceptionCheck()) env->ExceptionClear();
    }
}
