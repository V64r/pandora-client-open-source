#include "../features.hpp"
#include "../sdk.hpp"
#define NOMINMAX
#include <windows.h>

namespace features::combat::no_hit_delay
{
    bool enabled = false;

    static int get_left_click_counter(JNIEnv* env, jobject mc_obj)
    {
        if (!env || !mc_obj) return 0;
        jclass clazz = env->GetObjectClass(mc_obj);
        if (!clazz) return 0;

        jfieldID field = env->GetFieldID(clazz, "leftClickCounter", "I");
        if (!field) { env->ExceptionClear(); field = env->GetFieldID(clazz, "field_71429_W", "I"); }
        if (!field) { env->ExceptionClear(); field = env->GetFieldID(clazz, "ag", "I"); }
        if (!field) { env->ExceptionClear(); field = env->GetFieldID(clazz, "Y", "I"); } // 1.7.10

        int value = 0;
        if (field) value = env->GetIntField(mc_obj, field);

        env->DeleteLocalRef(clazz);
        if (env->ExceptionCheck()) env->ExceptionClear();
        return value;
    }

    static void set_left_click_counter(JNIEnv* env, jobject mc_obj, int value)
    {
        if (!env || !mc_obj) return;
        jclass clazz = env->GetObjectClass(mc_obj);
        if (!clazz) return;

        jfieldID field = env->GetFieldID(clazz, "leftClickCounter", "I");
        if (!field) { env->ExceptionClear(); field = env->GetFieldID(clazz, "field_71429_W", "I"); }
        if (!field) { env->ExceptionClear(); field = env->GetFieldID(clazz, "ag", "I"); }
        if (!field) { env->ExceptionClear(); field = env->GetFieldID(clazz, "Y", "I"); } // 1.7.10

        if (field) env->SetIntField(mc_obj, field, value);

        env->DeleteLocalRef(clazz);
        if (env->ExceptionCheck()) env->ExceptionClear();
    }

    void run(mapper::__minecraft& minecraft)
    {
        if (!enabled) return;

        JNIEnv* env = sdk::jni;
        if (!env) return;
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (!minecraft.object) return;

        auto screen = minecraft.get_current_screen();
        if (env->ExceptionCheck()) { env->ExceptionClear(); return; }
        if (screen.object != nullptr) return;

        auto local_player = minecraft.get_local_player();
        if (!local_player.object) return;

        int current = get_left_click_counter(env, minecraft.object);
        if (current > 1) {
            set_left_click_counter(env, minecraft.object, 1);
        }

        if (env->ExceptionCheck()) env->ExceptionClear();
    }
}
