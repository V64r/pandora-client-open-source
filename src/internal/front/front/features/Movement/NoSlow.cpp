#include "../features.hpp"
#include "../sdk.hpp"
#define NOMINMAX
#include <windows.h>
#include <cmath>

// Fórmula matemática extraída de tu Bhop
#define PI_F 3.14159265358979323846f

namespace features::movement::no_slow
{
    void run(mapper::__minecraft& minecraft)
    {
        if (!enabled || minecraft.get_current_screen().object != nullptr) return;

        auto local_player = minecraft.get_local_player();
        if (!local_player.object) return;

        if (sdk::jni && sdk::jni->ExceptionCheck()) sdk::jni->ExceptionClear();

        // Si el jugador está comiendo, bebiendo o bloqueando con la espada...

        // Bypass telarañas (FastWeb / NoWeb)
        if (sdk::jni) {
            jclass entity_class = sdk::jni->GetObjectClass(local_player.object);
            if (entity_class) {
                jfieldID web_fid = sdk::jni->GetFieldID(entity_class, "isInWeb", "Z");
                if (!web_fid) { sdk::jni->ExceptionClear(); web_fid = sdk::jni->GetFieldID(entity_class, "field_70134_J", "Z"); }
                if (!web_fid) { sdk::jni->ExceptionClear(); web_fid = sdk::jni->GetFieldID(entity_class, "I", "Z"); } // 1.8.9
                if (!web_fid) { sdk::jni->ExceptionClear(); web_fid = sdk::jni->GetFieldID(entity_class, "J", "Z"); } 
                if (!web_fid) { sdk::jni->ExceptionClear(); web_fid = sdk::jni->GetFieldID(entity_class, "K", "Z"); } // 1.7.10
                
                if (web_fid) {
                    sdk::jni->SetBooleanField(local_player.object, web_fid, JNI_FALSE);
                }
                sdk::jni->DeleteLocalRef(entity_class);
            }
            if (sdk::jni->ExceptionCheck()) sdk::jni->ExceptionClear();
        }
        if (local_player.is_using_item())
        {
            float move_forward = local_player.get_move_foreward();
            float move_strafing = local_player.get_move_strafing();

            // Solo actuamos si el jugador está intentando moverse
            if (move_forward != 0.0f || move_strafing != 0.0f)
            {
                // ¡RESTRICCIÓN DE KNOCKBACK ELIMINADA!
                // Ahora el cheat inyectará velocidad hacia adelante INCLUSO si estás recibiendo daño.

                mapper::__vec2 view_angles = local_player.get_view_angles();
                float yaw = view_angles.x;

                // --- MATEMÁTICAS DEL BHOP PARA CALCULAR TU DIRECCIÓN ---
                if (move_forward != 0.0f) {
                    if (move_strafing > 0.0f) {
                        yaw += ((move_forward > 0.0f) ? -45.0f : 45.0f);
                    }
                    else if (move_strafing < 0.0f) {
                        yaw += ((move_forward > 0.0f) ? 45.0f : -45.0f);
                    }
                    move_strafing = 0.0f;
                    if (move_forward > 0.0f) {
                        move_forward = 1.0f;
                    }
                    else if (move_forward < 0.0f) {
                        move_forward = -1.0f;
                    }
                }

                float sin_val = sinf((yaw + 90.0f) * PI_F / 180.0f);
                float cos_val = cosf((yaw + 90.0f) * PI_F / 180.0f);

                // --- VELOCIDAD PERFECTA Y AGRESIVA ---
                // Forzamos la velocidad máxima de carrera en Vanilla (0.28)
                double speed = 0.22;
                if ((GetAsyncKeyState(0x57) & 0x8000) != 0 || local_player.get_flag(3)) {
                    speed = 0.28;
                }

                double pos_x = (double)(move_forward * speed * cos_val + move_strafing * speed * sin_val);
                double pos_z = (double)(move_forward * speed * sin_val - move_strafing * speed * cos_val);

                // INYECCIÓN DIRECTA: Sobreescribimos la penalización de Vanilla Y el Knockback
                mapper::__vec3 motion = local_player.get_motion();
                motion.x = pos_x;
                motion.z = pos_z;
                local_player.set_motion(motion);
            }
        }

        if (sdk::jni && sdk::jni->ExceptionCheck()) sdk::jni->ExceptionClear();
    }
}
