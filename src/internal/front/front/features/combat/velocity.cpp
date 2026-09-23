#include "../features.hpp"
#include "../sdk.hpp"
#define NOMINMAX
#include <windows.h>
#include <cmath>
#include <random>

extern int gui_velo_mode;

namespace features::combat::velocity
{
    static std::mt19937 s_rng(std::random_device{}());

    static bool s_should_apply = false;
    static int  s_target_tick = 10;
    static int  s_last_hurt = 0;

    // ============================================================
    // ============================================================
    static void run_default(mapper::__minecraft& minecraft)
    {
        if (minecraft.get_current_screen().object != nullptr) return;

        if (clicking_only && !(GetAsyncKeyState(VK_LBUTTON) & 0x8000)) return;

        JNIEnv* env = sdk::jni;
        if (!env) return;
        if (env->ExceptionCheck()) env->ExceptionClear();

        auto local_player = minecraft.get_local_player();
        if (!local_player.object) return;

        if (weapon_only) {
            auto hs = local_player.get_held_item_stack();
            if (!hs.object) return;
            auto it = hs.get_item();
            if (!it.object) return;
            if (!it.is_sword() && !it.is_axe() && !it.is_pickaxe() && !it.is_shovel()) return;
        }

        int hurt = local_player.get_hurt_time();
        if (env->ExceptionCheck()) { env->ExceptionClear(); return; }

        // LÓGICA DE DRIP LITE
        if (hurt == 10 && s_last_hurt != 10)
        {
            std::uniform_real_distribution<float> dist(0.0f, 100.0f);
            if (dist(s_rng) <= chance)
            {
                s_should_apply = true;
                int safe_delay = std::clamp((int)std::round(delay), 0, 9);
                s_target_tick = 10 - safe_delay;
            }
            else
            {
                s_should_apply = false;
            }
        }

        s_last_hurt = hurt;

        // APLICACIÓN DEL VELOCITY
        bool is_valid_tick = (hurt == s_target_tick);

        if (s_should_apply && is_valid_tick)
        {
            jclass player_class = env->GetObjectClass(local_player.object);
            if (!player_class) return;

            if (air_only)
            {
                jfieldID onGround_fid = env->GetFieldID(player_class, "onGround", "Z");
                if (!onGround_fid) { env->ExceptionClear(); onGround_fid = env->GetFieldID(player_class, "field_70122_E", "Z"); }
                if (!onGround_fid) { env->ExceptionClear(); onGround_fid = env->GetFieldID(player_class, "C", "Z"); }

                if (onGround_fid) {
                    bool on_ground = env->GetBooleanField(local_player.object, onGround_fid);
                    if (on_ground) {
                        env->DeleteLocalRef(player_class);
                        s_should_apply = false;
                        return;
                    }
                }
            }

            if (moving_only)
            {
                jfieldID moveForward_fid = env->GetFieldID(player_class, "moveForward", "F");
                if (!moveForward_fid) { env->ExceptionClear(); moveForward_fid = env->GetFieldID(player_class, "field_70701_bs", "F"); }
                if (!moveForward_fid) { env->ExceptionClear(); moveForward_fid = env->GetFieldID(player_class, "ba", "F"); }
                if (!moveForward_fid) { env->ExceptionClear(); moveForward_fid = env->GetFieldID(player_class, "be", "F"); }

                jfieldID moveStrafing_fid = env->GetFieldID(player_class, "moveStrafing", "F");
                if (!moveStrafing_fid) { env->ExceptionClear(); moveStrafing_fid = env->GetFieldID(player_class, "field_70702_br", "F"); }
                if (!moveStrafing_fid) { env->ExceptionClear(); moveStrafing_fid = env->GetFieldID(player_class, "aZ", "F"); }
                if (!moveStrafing_fid) { env->ExceptionClear(); moveStrafing_fid = env->GetFieldID(player_class, "bd", "F"); }

                bool is_moving = false;
                if (moveForward_fid && moveStrafing_fid) {
                    float mf = env->GetFloatField(local_player.object, moveForward_fid);
                    float ms = env->GetFloatField(local_player.object, moveStrafing_fid);
                    if (std::abs(mf) > 0.01f || std::abs(ms) > 0.01f) {
                        is_moving = true;
                    }
                }

                if (!is_moving) {
                    env->DeleteLocalRef(player_class);
                    s_should_apply = false;
                    return;
                }
            }
            env->DeleteLocalRef(player_class);

            // Scale horizontal and vertical motion independently.
            auto motion = local_player.get_motion();

            double hm = (double)horizontal / 100.0;
            double vm = (double)vertical / 100.0;
            double dir = push_back ? -1.0 : 1.0;

            local_player.set_motion({
                (float)(motion.x * hm * dir),
                (float)(motion.y * vm),
                (float)(motion.z * hm * dir)
                });

            s_should_apply = false;
        }

        if (env->ExceptionCheck()) env->ExceptionClear();
    }

    // ============================================================
    // ============================================================
    void run(mapper::__minecraft& minecraft)
    {
        if (!enabled) return;

        mode = gui_velo_mode;
        run_default(minecraft);
    }
}
