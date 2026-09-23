#include "../features.hpp"
#include "../sdk.hpp"
#define NOMINMAX
#include <windows.h>
#include <random>

extern bool  gui_macros_enabled;
extern int   gui_macros_mode;        // 0=Rod, 1=Bow, 2=Fireball, 3=Gap, 4=Pot
extern int   gui_macros_bind;
extern float gui_macros_switch_delay;
extern float gui_macros_use_delay;
extern bool  gui_macros_auto_switch_back;

namespace features::combat::macros
{
    enum class State { IDLE, SWITCHING, USING, HOLDING, SWITCHING_BACK };
    static State current_state = State::IDLE;
    static ULONGLONG timer = 0;
    static int initial_slot = 0;
    static int target_slot = -1;
    static std::mt19937 s_rng(std::random_device{}());

    static int get_target_item_id(int mode) {
        switch (mode) {
        case 0: return 261;  // Bow
        case 1: return 385;  // Fire Charge
        case 2: return 322;  // Golden Apple
        case 3: return 373;  // Potion
        default: return -1;
        }
    }

    static int get_item_id(JNIEnv* env, jobject item_obj) {
        if (!env || !item_obj) return -1;
        jclass item_base_class = mapper::classes["Item"].klass;
        if (!item_base_class) return -1;

        const std::string& item_sig = mapper::classes["Item"].signature;
        std::string sig = "(" + item_sig + ")I";

        jmethodID get_id_mid = env->GetStaticMethodID(item_base_class, "getIdFromItem", sig.c_str());
        if (!get_id_mid) { env->ExceptionClear(); get_id_mid = env->GetStaticMethodID(item_base_class, "func_150891_b", sig.c_str()); }
        if (!get_id_mid) { env->ExceptionClear(); get_id_mid = env->GetStaticMethodID(item_base_class, "b", sig.c_str()); }
        if (!get_id_mid) { env->ExceptionClear(); get_id_mid = env->GetStaticMethodID(item_base_class, "a", sig.c_str()); }

        int id = -1;
        if (get_id_mid) id = env->CallStaticIntMethod(item_base_class, get_id_mid, item_obj);
        if (env->ExceptionCheck()) { env->ExceptionClear(); id = -1; }
        return id;
    }

    static int get_current_slot(JNIEnv* env, jobject player_obj) {
        if (!env || !player_obj) return 0;
        jclass player_class = env->GetObjectClass(player_obj);
        if (!player_class) return 0;
        const std::string& inv_sig = mapper::classes["InventoryPlayer"].signature;

        jfieldID inv_field = env->GetFieldID(player_class, "inventory", inv_sig.c_str());
        if (!inv_field) { env->ExceptionClear(); inv_field = env->GetFieldID(player_class, "field_71071_by", inv_sig.c_str()); }
        if (!inv_field) { env->ExceptionClear(); inv_field = env->GetFieldID(player_class, "bi", inv_sig.c_str()); }
        if (!inv_field) { env->ExceptionClear(); inv_field = env->GetFieldID(player_class, "bg", inv_sig.c_str()); } // 1.8.9
        if (!inv_field) { env->ExceptionClear(); inv_field = env->GetFieldID(player_class, "bh", inv_sig.c_str()); }
        if (!inv_field) { env->ExceptionClear(); env->DeleteLocalRef(player_class); return 0; }

        jobject inv_obj = env->GetObjectField(player_obj, inv_field);
        if (!inv_obj) { env->DeleteLocalRef(player_class); return 0; }
        jclass inv_class = env->GetObjectClass(inv_obj);
        jfieldID slot_field = env->GetFieldID(inv_class, "currentItem", "I");
        if (!slot_field) { env->ExceptionClear(); slot_field = env->GetFieldID(inv_class, "field_70461_c", "I"); }
        if (!slot_field) { env->ExceptionClear(); slot_field = env->GetFieldID(inv_class, "c", "I"); }

        int slot = 0;
        if (slot_field) slot = env->GetIntField(inv_obj, slot_field);
        if (env->ExceptionCheck()) { env->ExceptionClear(); slot = 0; }

        env->DeleteLocalRef(inv_class); env->DeleteLocalRef(inv_obj); env->DeleteLocalRef(player_class);
        return slot;
    }

    static void set_current_slot(JNIEnv* env, jobject player_obj, int slot) {
        if (!env || !player_obj) return;
        jclass player_class = env->GetObjectClass(player_obj);
        if (!player_class) return;
        const std::string& inv_sig = mapper::classes["InventoryPlayer"].signature;

        jfieldID inv_field = env->GetFieldID(player_class, "inventory", inv_sig.c_str());
        if (!inv_field) { env->ExceptionClear(); inv_field = env->GetFieldID(player_class, "field_71071_by", inv_sig.c_str()); }
        if (!inv_field) { env->ExceptionClear(); inv_field = env->GetFieldID(player_class, "bi", inv_sig.c_str()); }
        if (!inv_field) { env->ExceptionClear(); inv_field = env->GetFieldID(player_class, "bg", inv_sig.c_str()); } // 1.8.9
        if (!inv_field) { env->ExceptionClear(); inv_field = env->GetFieldID(player_class, "bh", inv_sig.c_str()); }
        if (!inv_field) { env->ExceptionClear(); env->DeleteLocalRef(player_class); return; }

        jobject inv_obj = env->GetObjectField(player_obj, inv_field);
        if (!inv_obj) { env->DeleteLocalRef(player_class); return; }
        jclass inv_class = env->GetObjectClass(inv_obj);
        jfieldID slot_field = env->GetFieldID(inv_class, "currentItem", "I");
        if (!slot_field) { env->ExceptionClear(); slot_field = env->GetFieldID(inv_class, "field_70461_c", "I"); }
        if (!slot_field) { env->ExceptionClear(); slot_field = env->GetFieldID(inv_class, "c", "I"); }

        if (slot_field) env->SetIntField(inv_obj, slot_field, slot);

        env->DeleteLocalRef(inv_class); env->DeleteLocalRef(inv_obj); env->DeleteLocalRef(player_class);
    }

    static void do_right_click(JNIEnv* env, jobject mc_obj) {
        if (!env || !mc_obj) return;
        jclass clazz = env->GetObjectClass(mc_obj);
        if (!clazz) return;

        jmethodID mid = env->GetMethodID(clazz, "rightClickMouse", "()V");
        if (!mid) { env->ExceptionClear(); mid = env->GetMethodID(clazz, "func_147121_ag", "()V"); }
        if (!mid) { env->ExceptionClear(); mid = env->GetMethodID(clazz, "ax", "()V"); }
        if (!mid) { env->ExceptionClear(); mid = env->GetMethodID(clazz, "au", "()V"); }

        if (mid) env->CallVoidMethod(mc_obj, mid);

        env->DeleteLocalRef(clazz);
        if (env->ExceptionCheck()) env->ExceptionClear();
    }

    static int random_jitter(int base) {
        std::uniform_int_distribution<int> d(-5, 8);
        return base + d(s_rng);
    }

    void run(mapper::__minecraft& minecraft)
    {
        if (!sdk::jni) return;
        if (sdk::jni->ExceptionCheck()) sdk::jni->ExceptionClear();

        auto local_player = minecraft.get_local_player();
        if (!local_player.object) { current_state = State::IDLE; return; }

        JNIEnv* env = sdk::jni;
        ULONGLONG now = GetTickCount64();
        
        if (!gui_macros_enabled) {
            if (current_state == State::HOLDING) {
                auto settings = minecraft.get_settings();
                if (settings.object) settings.set_virtual_right_click(false);
                set_current_slot(env, local_player.object, initial_slot);
            }
            current_state = State::IDLE;
            return;
        }

        int wanted_id = -1;

        switch (current_state)
        {
        case State::IDLE:
            wanted_id = get_target_item_id(gui_macros_mode);
            target_slot = -1;

            for (int i = 0; i < 9; i++) {
                auto stack = local_player.get_inventory_slot(i);
                if (env->ExceptionCheck()) { env->ExceptionClear(); continue; }
                if (stack.object != nullptr) {
                    auto item = stack.get_item();
                    if (env->ExceptionCheck()) { env->ExceptionClear(); continue; }
                    if (item.object != nullptr) {
                        bool matches = false;

                        // Check ID
                        if (get_item_id(env, item.object) == wanted_id) {
                            matches = true;
                        } else {
                            // Fallback: Check UnlocalizedName
                            jclass item_class = env->GetObjectClass(item.object);
                            if (item_class) {
                                std::string item_name = "";
                                jmethodID name_mid = env->GetMethodID(item_class, "getUnlocalizedName", "()Ljava/lang/String;");
                                if (!name_mid) { env->ExceptionClear(); name_mid = env->GetMethodID(item_class, "func_77658_a", "()Ljava/lang/String;"); }
                                if (!name_mid) { env->ExceptionClear(); name_mid = env->GetMethodID(item_class, "a", "()Ljava/lang/String;"); }
                                
                                if (name_mid) {
                                    jstring js = (jstring)env->CallObjectMethod(item.object, name_mid);
                                    if (js && !env->ExceptionCheck()) {
                                        const char* s = env->GetStringUTFChars(js, nullptr);
                                        if (s) { item_name = s; env->ReleaseStringUTFChars(js, s); }
                                        env->DeleteLocalRef(js);
                                    } else {
                                        env->ExceptionClear();
                                    }
                                }
                                env->DeleteLocalRef(item_class);

                                if (gui_macros_mode == 0 && item_name.find("bow") != std::string::npos) matches = true;
                                if (gui_macros_mode == 1 && (item_name.find("fireball") != std::string::npos || item_name.find("fireCharge") != std::string::npos)) matches = true;
                                if (gui_macros_mode == 2 && item_name.find("appleGold") != std::string::npos) matches = true;
                                if (gui_macros_mode == 3 && item_name.find("potion") != std::string::npos) matches = true;
                            }
                        }

                        if (matches) {
                            target_slot = i;
                            break;
                        }
                    }
                }
            }

            if (target_slot != -1) {
                initial_slot = get_current_slot(env, local_player.object);
                // Ensure at least 50ms delay to prevent MMC FastMacro bans
                int safe_switch = max(50, (int)gui_macros_switch_delay);
                timer = now + random_jitter(safe_switch + 15);
                current_state = State::SWITCHING;
            }
            break;

        case State::SWITCHING:
            if (now >= timer) {
                set_current_slot(env, local_player.object, target_slot);
                
                if (gui_macros_mode == 0 || gui_macros_mode == 2) { // BOW or GAP
                    auto settings = minecraft.get_settings();
                    if (settings.object) settings.set_virtual_right_click(true);
                    
                    if (gui_macros_mode == 0) {
                        timer = now + random_jitter((int)gui_macros_use_delay * 5 + 50);
                    } else {
                        timer = now + random_jitter(1650); // 1.65 seconds to eat golden apple
                    }
                    current_state = State::HOLDING;
                } else {
                    // Minemen safe use delay
                    int safe_use = max(50, (int)gui_macros_use_delay);
                    timer = now + random_jitter((int)(safe_use / 2.0f) + 30);
                    current_state = State::USING;
                }
            }
            break;

        case State::USING:
            if (now >= timer) {
                do_right_click(env, minecraft.object);
                if (gui_macros_auto_switch_back) {
                    int safe_switch = max(50, (int)gui_macros_switch_delay);
                    timer = now + random_jitter(safe_switch + 15);
                    current_state = State::SWITCHING_BACK;
                } else {
                    current_state = State::IDLE;
                    gui_macros_enabled = false;
                }
            }
            break;

        case State::HOLDING:
            if (now >= timer) {
                auto settings = minecraft.get_settings();
                if (settings.object) settings.set_virtual_right_click(false);
                
                if (gui_macros_auto_switch_back) {
                    int safe_switch = max(50, (int)gui_macros_switch_delay);
                    timer = now + random_jitter(safe_switch + 15);
                    current_state = State::SWITCHING_BACK;
                } else {
                    current_state = State::IDLE;
                    gui_macros_enabled = false;
                }
            }
            break;

        case State::SWITCHING_BACK:
            if (now >= timer) {
                set_current_slot(env, local_player.object, initial_slot);
                current_state = State::IDLE;
                gui_macros_enabled = false;
            }
            break;
        }

        if (env->ExceptionCheck()) env->ExceptionClear();
    }
}
