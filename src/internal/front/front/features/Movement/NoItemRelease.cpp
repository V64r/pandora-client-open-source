#include "../features.hpp"
#include "../sdk.hpp"
#define NOMINMAX
#include <windows.h>
#include <string>

namespace features::movement::no_item_release
{
    static bool s_last_rbutton = false;
    static bool s_virtual_hold = false;
    static bool weapon_held = false;
    static int held_id = -1;
    static mapper::__method s_use_action_m;
    static bool s_cached = false;

    static void ensure_cache()
    {
        if (s_cached) return;
        // Construir firma dinamica: (ItemStack) -> EnumAction
        // En 1.7.10 los nombres de clase son diferentes, asi que usamos
        // la firma de ItemStack del mapper y buscamos EnumAction por reflexion
        std::string is_sig = mapper::classes["ItemStack"].signature;

        // Intentar con firma larga (Forge) primero, luego ofuscadas
        s_use_action_m = mapper::classes["Item"].get_method("getItemUseAction", "(" + is_sig + ")Lnet/minecraft/item/EnumAction;");
        if (!s_use_action_m.identifier) s_use_action_m = mapper::classes["Item"].get_method("func_77661_b", "(" + is_sig + ")Lnet/minecraft/item/EnumAction;");
        if (!s_use_action_m.identifier) {
            // En versiones ofuscadas, la firma contiene los nombres ofuscados
            for (auto& m : mapper::classes["Item"].methods) {
                if (m.signature.find(is_sig) != std::string::npos &&
                    m.signature.find(")L") != std::string::npos &&
                    m.signature.find("()" ) == std::string::npos &&
                    m.signature.find("(" + is_sig + ")") == 0) {
                    // Verificar que no devuelve void ni int
                    std::string ret = m.signature.substr(m.signature.find(')') + 1);
                    if (ret[0] == 'L') {
                        s_use_action_m = m;
                        break;
                    }
                }
            }
        }
        s_cached = true;
    }

    static std::string get_action(JNIEnv* env, jobject item_obj, jobject stack_obj)
    {
        if (!s_use_action_m.identifier || !item_obj) return "NONE";
        jobject action = env->CallObjectMethod(item_obj, s_use_action_m.identifier, stack_obj);
        if (!action || env->ExceptionCheck()) { env->ExceptionClear(); return "NONE"; }
        jclass ac = env->GetObjectClass(action);
        
        static jmethodID nm_cached = nullptr;
        if (!nm_cached) {
            nm_cached = env->GetMethodID(ac, "name", "()Ljava/lang/String;");
        }
        
        std::string result = "NONE";
        if (nm_cached) {
            jstring js = (jstring)env->CallObjectMethod(action, nm_cached);
            if (js) {
                const char* s = env->GetStringUTFChars(js, nullptr);
                if (s) { result = s; env->ReleaseStringUTFChars(js, s); }
                env->DeleteLocalRef(js);
            }
        }
        env->DeleteLocalRef(ac); env->DeleteLocalRef(action);
        if (env->ExceptionCheck()) env->ExceptionClear();
        return result;
    }



    void send_ghost_consume(mapper::__player& player, mapper::__item_stack& hs)
    {
        JNIEnv* env = sdk::jni;
        jclass c08_class = mapper::classes["C08PacketPlayerBlockPlacement"].klass;
        if (!c08_class) return;
        
        // Usar firmas dinamicas del mapper
        std::string is_sig = mapper::classes["ItemStack"].signature;
        
        jobject packet = nullptr;
        
        if (mapper::version == mapper::MINECRAFT_18) {
            jclass bp_class = env->FindClass("net/minecraft/util/BlockPos");
            if (!bp_class) { env->ExceptionClear(); bp_class = env->FindClass("cj"); } // ofuscado
            
            if (bp_class) {
                jmethodID bp_init = env->GetMethodID(bp_class, "<init>", "(III)V");
                if (bp_init) {
                    jobject bp_obj = env->NewObject(bp_class, bp_init, -1, -1, -1);
                    if (bp_obj) {
                        std::string sig18 = "(L" + std::string(mapper::classes["BlockPos"].name) + ";I" + is_sig + "FFF)V";
                        if (mapper::classes["BlockPos"].name == "cj") sig18 = "(Lcj;I" + is_sig + "FFF)V";

                        jmethodID init = env->GetMethodID(c08_class, "<init>", sig18.c_str());
                        if (env->ExceptionCheck()) { env->ExceptionClear(); init = nullptr; }
                        
                        if (init) {
                            packet = env->NewObject(c08_class, init, bp_obj, 255, hs.object, 0.0f, 0.0f, 0.0f);
                        }
                        env->DeleteLocalRef(bp_obj);
                    }
                }
                env->DeleteLocalRef(bp_class);
            }
        } else {
            std::string sig17 = "(IIII" + is_sig + "FFF)V";
            jmethodID init = env->GetMethodID(c08_class, "<init>", sig17.c_str());
            if (env->ExceptionCheck()) { env->ExceptionClear(); init = nullptr; }
            if (init) packet = env->NewObject(c08_class, init, -1, -1, -1, 255, hs.object, 0.0f, 0.0f, 0.0f);
            if (env->ExceptionCheck()) { env->ExceptionClear(); packet = nullptr; }
        }
        
        if (packet) {
            player.send_packet(packet);
            env->DeleteLocalRef(packet);
        }
    }

    void clear_item_in_use(mapper::__player& player)
    {
        JNIEnv* env = sdk::jni;
        mapper::__method method = mapper::classes["EntityPlayer"].get_method("clearItemInUse", "()V");
        if (!method.identifier) method = mapper::classes["EntityPlayer"].get_method("func_71028_bD", "()V");
        if (!method.identifier) method = mapper::classes["EntityPlayer"].get_method("bU", "()V"); // 1.8.9
        if (!method.identifier) method = mapper::classes["EntityPlayer"].get_method("bT", "()V"); // 1.7.10
        if (!method.identifier) method = mapper::classes["EntityPlayer"].get_method("bS", "()V"); // fallback

        if (method.identifier) {
            env->CallVoidMethod(player.object, method.identifier);
            if (env->ExceptionCheck()) env->ExceptionClear();
        }
    }

    void run(mapper::__minecraft& minecraft)
    {
        JNIEnv* env = sdk::jni;
        if (!env) return;

        if (!enabled || minecraft.get_current_screen().object != nullptr) {
            if (weapon_held) { auto settings=minecraft.get_settings(); if(settings.object) settings.set_virtual_right_click(false); }
            weapon_held=false; held_id=-1; s_last_rbutton=false; s_virtual_hold=false;
            return;
        }

        bool rbutton = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
        bool lbutton = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;

        bool just_released = !rbutton && s_last_rbutton;
        bool just_pressed_l = lbutton && !s_virtual_hold; // reusing s_virtual_hold as last_lbutton state
        s_last_rbutton = rbutton;
        s_virtual_hold = lbutton;

        auto local_player = minecraft.get_local_player();
        if (!local_player.object) return;

        ensure_cache();
        auto hs = local_player.get_held_item_stack();
        if (!hs.object) {
            if (weapon_held) { auto settings=minecraft.get_settings(); if(settings.object) settings.set_virtual_right_click(false); }
            weapon_held=false; held_id=-1; return;
        }

        auto it = hs.get_item();
        if (!it.object) return;

        std::string action = get_action(env, it.object, hs.object);
        const int id = it.get_id();
        const bool potion = id == 373 && !(hs.get_item_damage() & 16384);
        const bool is_consumable = (food && (action == "EAT" || (action == "DRINK" && !potion))) ||
            (potions && potion && action == "DRINK");
        const bool weapon_selected = (sword && action == "BLOCK") || (bow && action == "BOW");
        if (weapon_held && (!weapon_selected || held_id != id || lbutton || rbutton)) {
            auto settings=minecraft.get_settings();
            if(settings.object) settings.set_virtual_right_click(false);
            weapon_held=false;
        }
        if (weapon_selected && just_released) {
            auto settings=minecraft.get_settings();
            if(settings.object) { settings.set_virtual_right_click(true); weapon_held=true; held_id=id; }
        }

        mapper::__method is_using_m = mapper::classes["EntityPlayer"].get_method("isUsingItem", "()Z");
        if (!is_using_m.identifier) is_using_m = mapper::classes["EntityPlayer"].get_method("func_71039_bw", "()Z");
        if (!is_using_m.identifier) is_using_m = mapper::classes["EntityPlayer"].get_method("bS", "()Z"); // 1.8.9
        if (!is_using_m.identifier) is_using_m = mapper::classes["EntityPlayer"].get_method("bR", "()Z"); // 1.7.10
        if (!is_using_m.identifier) is_using_m = mapper::classes["EntityPlayer"].get_method("bQ", "()Z");

        bool is_using = false;
        if (is_using_m.identifier) {
            is_using = env->CallBooleanMethod(local_player.object, is_using_m.identifier);
            if (env->ExceptionCheck()) env->ExceptionClear();
        }

        if (is_consumable && is_using) {
            if (just_pressed_l || just_released) {
                send_ghost_consume(local_player, hs);
                clear_item_in_use(local_player);
            }
        }
    }
}
