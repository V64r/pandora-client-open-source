#include "../features.hpp"
#include "../sdk.hpp"
#define NOMINMAX
#include <windows.h>
#include <random>

// AutoArmor: Equipa automaticamente la mejor armadura del inventario
//
// Approach robusto:
//   3. Iteramos slots 9-44 del container (inventario principal + hotbar)
//      que contiene "helmet", "chestplate", "leggings", "boots"

extern bool  gui_autoarmor_enabled;
extern float gui_autoarmor_delay;
extern bool  gui_autoarmor_only_better;
extern std::atomic<bool> g_MenuVisible;

namespace features::misc::auto_armor
{
    static ULONGLONG s_last_swap = 0;
    static std::mt19937 s_rng(std::random_device{}());

    struct ArmorInfo {
        int container_slot = -1;
        int protection = -1;
        int max_damage = -1;
        int damage = 999999;

        bool is_better_than(const ArmorInfo& other) const {
            if (protection > other.protection) return true;
            if (protection == other.protection) {
                int remaining_this = max_damage - damage;
                int remaining_other = other.max_damage - other.damage;
                if (remaining_this > remaining_other + 10) return true; // Require at least 10 more durability to swap
            }
            return false;
        }
    };

    static int get_armor_type(JNIEnv* env, jobject item) {
        if (!item) return -1;
        jclass clazz = env->GetObjectClass(item);
        if (!clazz) return -1;
        jfieldID at_fid = env->GetFieldID(clazz, "armorType", "I");
        if (!at_fid) { env->ExceptionClear(); at_fid = env->GetFieldID(clazz, "field_77881_a", "I"); }
        if (!at_fid) { env->ExceptionClear(); at_fid = env->GetFieldID(clazz, "c", "I"); }
        if (!at_fid) { env->ExceptionClear(); at_fid = env->GetFieldID(clazz, "b", "I"); }
        int type = -1;
        if (at_fid) { type = env->GetIntField(item, at_fid); if (env->ExceptionCheck()) env->ExceptionClear(); }
        env->DeleteLocalRef(clazz);
        return type;
    }

    static int get_armor_protection(JNIEnv* env, jobject item) {
        if (!item) return 0;
        jclass clazz = env->GetObjectClass(item);
        if (!clazz) return 0;
        jfieldID dmg_fid = env->GetFieldID(clazz, "damageReduceAmount", "I");
        if (!dmg_fid) { env->ExceptionClear(); dmg_fid = env->GetFieldID(clazz, "field_77879_b", "I"); }
        if (!dmg_fid) { env->ExceptionClear(); dmg_fid = env->GetFieldID(clazz, "d", "I"); }
        if (!dmg_fid) { env->ExceptionClear(); dmg_fid = env->GetFieldID(clazz, "c", "I"); }
        int prot = 0;
        if (dmg_fid) { prot = env->GetIntField(item, dmg_fid); if (env->ExceptionCheck()) env->ExceptionClear(); }
        env->DeleteLocalRef(clazz);
        return prot;
    }

    static int get_damage(JNIEnv* env, jobject stack) {
        if (!stack) return 0;
        jclass clazz = env->GetObjectClass(stack);
        if (!clazz) return 0;
        jmethodID mid = env->GetMethodID(clazz, "getItemDamage", "()I");
        if (!mid) { env->ExceptionClear(); mid = env->GetMethodID(clazz, "func_77960_j", "()I"); }
        if (!mid) { env->ExceptionClear(); mid = env->GetMethodID(clazz, "i", "()I"); }
        int dmg = 0;
        if (mid) { dmg = env->CallIntMethod(stack, mid); if (env->ExceptionCheck()) env->ExceptionClear(); }
        env->DeleteLocalRef(clazz);
        return dmg;
    }

    static int get_max_damage(JNIEnv* env, jobject stack) {
        if (!stack) return 0;
        jclass clazz = env->GetObjectClass(stack);
        if (!clazz) return 0;
        jmethodID mid = env->GetMethodID(clazz, "getMaxDamage", "()I");
        if (!mid) { env->ExceptionClear(); mid = env->GetMethodID(clazz, "func_77958_k", "()I"); }
        if (!mid) { env->ExceptionClear(); mid = env->GetMethodID(clazz, "j", "()I"); }
        int max_dmg = 0;
        if (mid) { max_dmg = env->CallIntMethod(stack, mid); if (env->ExceptionCheck()) env->ExceptionClear(); }
        env->DeleteLocalRef(clazz);
        return max_dmg;
    }

    void run(mapper::__minecraft& minecraft)
    {
        if (!gui_autoarmor_enabled || !sdk::jni) return;
        if (g_MenuVisible) return;
        if (sdk::jni->ExceptionCheck()) sdk::jni->ExceptionClear();

        auto local_player = minecraft.get_local_player();
        if (!local_player.object) return;

        auto current_screen = minecraft.get_current_screen();
        if (current_screen.object != nullptr) {
            jclass screen_class = sdk::jni->GetObjectClass(current_screen.object);
            if (!screen_class) return;

            jfieldID container_fid = nullptr;
            jclass classes_to_try[3] = { screen_class, nullptr, nullptr };
            classes_to_try[1] = sdk::jni->GetSuperclass(screen_class);
            if (classes_to_try[1]) classes_to_try[2] = sdk::jni->GetSuperclass(classes_to_try[1]);

            const char* field_names[] = { "inventorySlots", "field_147002_h", "d" };
            const char* field_sigs[] = { "Lnet/minecraft/inventory/Container;", "Lzk;", "Lye;" }; // Forge, Lunar 1.8, Lunar 1.7

            for (int c = 0; c < 3 && !container_fid; c++) {
                if (!classes_to_try[c]) continue;
                for (auto& fname : field_names) {
                    if (container_fid) break;
                    for (auto& fsig : field_sigs) {
                        container_fid = sdk::jni->GetFieldID(classes_to_try[c], fname, fsig);
                        if (sdk::jni->ExceptionCheck()) { sdk::jni->ExceptionClear(); container_fid = nullptr; }
                        if (container_fid) break;
                    }
                }
            }

            if (classes_to_try[1]) sdk::jni->DeleteLocalRef(classes_to_try[1]);
            if (classes_to_try[2]) sdk::jni->DeleteLocalRef(classes_to_try[2]);
            sdk::jni->DeleteLocalRef(screen_class);

            if (!container_fid) return;

            jobject container = sdk::jni->GetObjectField(current_screen.object, container_fid);
            if (!container) return;

            jclass container_class = sdk::jni->GetObjectClass(container);
            jfieldID wid_fid = sdk::jni->GetFieldID(container_class, "windowId", "I");
            if (sdk::jni->ExceptionCheck()) { sdk::jni->ExceptionClear(); wid_fid = nullptr; }
            if (!wid_fid) {
                wid_fid = sdk::jni->GetFieldID(container_class, "field_75152_c", "I");
                if (sdk::jni->ExceptionCheck()) { sdk::jni->ExceptionClear(); wid_fid = nullptr; }
            }
            if (!wid_fid) {
                wid_fid = sdk::jni->GetFieldID(container_class, "a", "I");
                if (sdk::jni->ExceptionCheck()) { sdk::jni->ExceptionClear(); wid_fid = nullptr; }
            }

            int window_id = -1;
            if (wid_fid) {
                window_id = sdk::jni->GetIntField(container, wid_fid);
                if (sdk::jni->ExceptionCheck()) { sdk::jni->ExceptionClear(); window_id = -1; }
            }

            sdk::jni->DeleteLocalRef(container_class);
            sdk::jni->DeleteLocalRef(container);

            if (window_id != 0) return;
        }

        // 4. Delay entre swaps
        ULONGLONG now = GetTickCount64();
        int delay_ms = (int)gui_autoarmor_delay;
        std::uniform_int_distribution<int> d(-15, 15);
        delay_ms += d(s_rng);
        if (delay_ms < 20) delay_ms = 20;
        if ((now - s_last_swap) < (ULONGLONG)delay_ms) return;

        JNIEnv* env = sdk::jni;

        ArmorInfo best_armor[4];
        ArmorInfo equipped_armor[4];

        for (int container_slot = 5; container_slot <= 44; container_slot++)
        {
            int inv_idx = -1;
            if (container_slot >= 5 && container_slot <= 8) {
                inv_idx = 39 - (container_slot - 5);
            } else if (container_slot >= 36) {
                inv_idx = container_slot - 36;
            } else {
                inv_idx = container_slot;
            }

            auto stack = local_player.get_inventory_slot(inv_idx);
            if (env->ExceptionCheck()) { env->ExceptionClear(); continue; }
            if (!stack.object) continue;

            auto item = stack.get_item();
            if (env->ExceptionCheck()) { env->ExceptionClear(); continue; }
            if (!item.object) continue;

            bool is_armor = false;
            const char* armor_classes_names[] = { "net/minecraft/item/ItemArmor", "bkk", "blg", "bkn", "yq" };
            static jclass cached_armor_classes[5] = { nullptr };
            static bool initialized_armor_classes = false;

            if (!initialized_armor_classes) {
                for (int i = 0; i < 5; i++) {
                    jclass temp_class = env->FindClass(armor_classes_names[i]);
                    if (env->ExceptionCheck()) { env->ExceptionClear(); }
                    if (temp_class) {
                        cached_armor_classes[i] = (jclass)env->NewGlobalRef(temp_class);
                        env->DeleteLocalRef(temp_class);
                    }
                }
                initialized_armor_classes = true;
            }

            for (int i = 0; i < 5; i++) {
                if (cached_armor_classes[i] == nullptr) continue;
                is_armor = env->IsInstanceOf(item.object, cached_armor_classes[i]);
                if (env->ExceptionCheck()) { env->ExceptionClear(); }
                if (is_armor) break;
            }

            if (!is_armor) {
                jclass item_class = env->GetObjectClass(item.object);
                if (item_class) {
                    jfieldID at_fid = env->GetFieldID(item_class, "armorType", "I");
                    if (env->ExceptionCheck()) { env->ExceptionClear(); at_fid = nullptr; }
                    if (!at_fid) { at_fid = env->GetFieldID(item_class, "field_77881_a", "I"); if (env->ExceptionCheck()) { env->ExceptionClear(); at_fid = nullptr; } }
                    if (!at_fid) { at_fid = env->GetFieldID(item_class, "c", "I"); if (env->ExceptionCheck()) { env->ExceptionClear(); at_fid = nullptr; } }
                    if (at_fid) is_armor = true;
                    env->DeleteLocalRef(item_class);
                }
            }

            if (!is_armor) continue;

            int type = get_armor_type(env, item.object);
            if (type < 0 || type > 3) continue;

            int prot = get_armor_protection(env, item.object);
            int dmg = get_damage(env, stack.object);
            int max_dmg = get_max_damage(env, stack.object);

            ArmorInfo info = { container_slot, prot, max_dmg, dmg };

            if (container_slot >= 5 && container_slot <= 8) {
                equipped_armor[type] = info;
                best_armor[type] = info;
            } else {
                if (best_armor[type].container_slot == -1 || info.is_better_than(best_armor[type])) {
                    best_armor[type] = info;
                }
            }
        }

        // Evaluate if we should swap any armor
        for (int i = 0; i < 4; i++) {
            if (best_armor[i].container_slot >= 9) {
                // The best armor is in the inventory (not equipped)
                if (equipped_armor[i].container_slot != -1) {
                    // Unequip current armor first
                    minecraft.window_click(0, equipped_armor[i].container_slot, 0, 1, local_player);
                    if (env->ExceptionCheck()) env->ExceptionClear();
                    s_last_swap = now;
                    return; // One swap per tick
                } else {
                    // Slot is empty, equip the best armor
                    minecraft.window_click(0, best_armor[i].container_slot, 0, 1, local_player);
                    if (env->ExceptionCheck()) env->ExceptionClear();
                    s_last_swap = now;
                    return; // One swap per tick
                }
            }
        }
    }
}
