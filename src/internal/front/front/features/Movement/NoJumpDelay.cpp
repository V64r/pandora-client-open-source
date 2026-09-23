#include "../features.hpp"
#include "../sdk.hpp"
#define NOMINMAX
#include <windows.h>

namespace features::movement::no_jump_delay
{
    static jfieldID s_field   = nullptr;
    static bool     s_resolved = false;

    static jfieldID resolve_field(JNIEnv* env, jobject player_obj)
    {
        if (s_resolved) return s_field;

        jclass clazz = env->GetObjectClass(player_obj);
        if (!clazz) return nullptr;

        const char* names[] = {
            "jumpTicks",       // MCP deobf (dev/some Forge builds)
            "field_70773_bE",
            "bF",              // Notch 1.8.9
            "bG",              // Notch 1.8.9 alt
            "bi",              // Notch 1.7.10
            "field_70703_bu"   // SRG legacy fallback
        };

        for (const char* name : names) {
            jfieldID f = env->GetFieldID(clazz, name, "I");
            if (f) {
                s_field = f;
                s_resolved = true;
                env->DeleteLocalRef(clazz);
                return s_field;
            }
            env->ExceptionClear();
        }

        env->DeleteLocalRef(clazz);
        s_resolved = true;
        return nullptr;
    }

    void run(mapper::__minecraft& minecraft)
    {
        if (!enabled) return;

        JNIEnv* env = sdk::jni;
        if (!env) return;

        auto local_player = minecraft.get_local_player();
        if (!local_player.object) return;
        if (env->ExceptionCheck()) env->ExceptionClear();

        // No tocar en GUI
        if (minecraft.get_current_screen().object != nullptr) return;

        // No durante daño — evita flags de Velocity
        if (local_player.get_hurt_time() > 0) return;

        if (!local_player.get_on_ground()) return;

        // motionY estable — no tocar en transiciones de caída
        auto motion = local_player.get_motion();
        if (motion.y > 0.01 || motion.y < -0.08) return;

        // No en líquido
        if (local_player.is_offset_position_in_liquid(0.0, 0.0, 0.0)) return;

        // Resolver campo
        jfieldID field = resolve_field(env, local_player.object);
        if (!field) return;

        int ticks = env->GetIntField(local_player.object, field);
        if (ticks > 0) {
            env->SetIntField(local_player.object, field, 0);
        }

        if (env->ExceptionCheck()) env->ExceptionClear();
    }
}
