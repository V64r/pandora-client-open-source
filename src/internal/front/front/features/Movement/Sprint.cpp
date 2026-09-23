#include "../features.hpp"
#include <windows.h>

namespace features::movement::sprint
{
    void run(mapper::__minecraft& minecraft)
    {
        static bool owns_key = false;
        JNIEnv* env = sdk::jni;
        if ((!enabled && !owns_key) || !env || !minecraft.object || env->ExceptionCheck()) return;
        auto settings = minecraft.get_settings();
        if (!settings.object || env->ExceptionCheck()) return;
        // Publish input only: setSprinting changes Java attribute maps which
        // the game thread also changes when processing attacks.
        const auto& settings_class = mapper::classes["GameSettings"];
        const auto& key_class = mapper::classes["KeyBinding"];
        auto sprint_field = settings_class.get_field("keyBindSprint", key_class.signature);
        if (!sprint_field.identifier)
            sprint_field = settings_class.get_field("field_151444_V", key_class.signature);
        auto pressed_field = key_class.get_field("pressed", "Z");
        if (!pressed_field.identifier) pressed_field = key_class.get_field("field_74513_e", "Z");
        if (!sprint_field.identifier || !pressed_field.identifier) return;
        jobject key = env->GetObjectField(settings.object, sprint_field.identifier);
        if (env->ExceptionCheck() || !key) {
            if (key) env->DeleteLocalRef(key);
            return;
        }
        auto screen = minecraft.get_current_screen();
        if (!env->ExceptionCheck()) {
            const bool down = enabled && !screen.object && (GetAsyncKeyState('W') & 0x8000);
            if (down || owns_key) env->SetBooleanField(key, pressed_field.identifier, down ? JNI_TRUE : JNI_FALSE);
            owns_key = down;
        }
        env->DeleteLocalRef(key);
    }
}
