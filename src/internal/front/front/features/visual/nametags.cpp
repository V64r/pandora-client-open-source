#include "../features.hpp"
#include "../sdk.hpp"
#include <string>
#include <vector>
#include <mutex>
#include <imgui.h>
#include <cfloat>
#include <cmath>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <chrono>
#include <memory>
#include <jvmti.h>
#include <functional>
#include <cstring>
#include <cstdint>
#include <gl/GL.h>
extern std::atomic<bool> g_PlayerInGui;
extern std::atomic<bool> g_ChatOpen;

extern int   gui_nametags_health_format;
extern float gui_nametags_health_segments;
extern bool  gui_nametags_show_own;
extern bool  gui_nametags_hide_vanilla;
extern float gui_nametags_scale;
extern bool  gui_nametags_distance_scaling;
extern bool  gui_nametags_show_equipment;
extern bool  gui_nametags_show_enchantments;
extern bool  gui_nametags_show_name;

static int get_third_person_view(mapper::__minecraft& minecraft) {
    auto settings = minecraft.get_settings();
    if (!settings.object) return 0;
    mapper::__field field = mapper::classes["GameSettings"].get_field("thirdPersonView", "I");
    if (!field.identifier) field = mapper::classes["GameSettings"].get_field("field_74320_O", "I");
    if (!field.identifier) field = mapper::classes["GameSettings"].get_field("aw", "I");
    return field.identifier ? sdk::jni->GetIntField(settings.object, field.identifier) : 0;
}

namespace mapper { extern jvmtiEnv* jvmti; }
extern "C" void swiftLog(const char* format, ...);

static jmethodID find_method_dynamic(jclass cls, std::initializer_list<const char*> names,
    const std::function<bool(const std::string&)>& signature_ok) {
    if (!cls || !mapper::jvmti) return nullptr;
    jint count = 0; jmethodID* methods = nullptr;
    if (mapper::jvmti->GetClassMethods(cls, &count, &methods) != JVMTI_ERROR_NONE || !methods) return nullptr;
    jmethodID result = nullptr;
    for (jint i = 0; i < count && !result; ++i) {
        char *name = nullptr, *sig = nullptr, *generic = nullptr;
        if (mapper::jvmti->GetMethodName(methods[i], &name, &sig, &generic) == JVMTI_ERROR_NONE) {
            bool name_ok = names.size() == 0;
            for (const char* candidate : names) if (name && std::strcmp(name, candidate) == 0) { name_ok = true; break; }
            if (name_ok && sig && signature_ok(sig)) result = methods[i];
        }
        if (name) mapper::jvmti->Deallocate((unsigned char*)name);
        if (sig) mapper::jvmti->Deallocate((unsigned char*)sig);
        if (generic) mapper::jvmti->Deallocate((unsigned char*)generic);
    }
    mapper::jvmti->Deallocate((unsigned char*)methods);
    return result;
}

namespace
{
    PROC safe_gl_proc(const char* name) {
        const auto proc = wglGetProcAddress(name);
        const auto address = reinterpret_cast<std::intptr_t>(proc);
        return address == 0 || address == 1 || address == 2 || address == 3 || address == -1
            ? nullptr : proc;
    }
    struct MinecraftTextCommand
    {
        std::string text;
        float x = 0.0f;
        float y = 0.0f;
		float width = 0.0f;
		float height = 0.0f;
        float scale = 1.0f;
        int color = -1;
    };

    constexpr float default_nametag_scale = 1.30f;
    constexpr float default_minecraft_font_scale = 1.58f;

    float nametag_scale_factor()
    {
        return std::clamp(gui_nametags_scale, 0.85f, 4.00f) / default_nametag_scale;
    }

    float minecraft_font_scale()
    {
        return default_minecraft_font_scale * nametag_scale_factor();
    }

    std::vector<MinecraftTextCommand> minecraft_text_commands;
    JNIEnv* current_thread_env();
    struct EquipmentItem {
        jobject stack = nullptr;
        int item_id = -1;
        int metadata = 0;
        int count = 1;
        float durability = -1.0f;
        int dye_color = -1;
        std::vector<std::string> enchantments;
    };

    struct EquipmentSnapshot {
        std::vector<EquipmentItem> items;
        ~EquipmentSnapshot() {
            JNIEnv* env = current_thread_env();
            if (!env) return;
            for (auto& item : items)
                if (item.stack) env->DeleteGlobalRef(item.stack);
        }
    };

    struct MinecraftItemCommand {
        std::shared_ptr<const EquipmentSnapshot> equipment;
        size_t item_index = 0;
        float x = 0.0f;
        float y = 0.0f;
        float scale = 1.0f;
        float durability = -1.0f;
    };

    std::vector<MinecraftItemCommand> minecraft_item_commands;
    std::recursive_mutex minecraft_render_mutex;

    using GlGenFramebuffers = void(APIENTRY*)(GLsizei, GLuint*);
    using GlBindFramebuffer = void(APIENTRY*)(GLenum, GLuint);
    using GlDeleteFramebuffers = void(APIENTRY*)(GLsizei, const GLuint*);
    using GlFramebufferTexture2D = void(APIENTRY*)(GLenum, GLenum, GLenum, GLuint, GLint);
    using GlCheckFramebufferStatus = GLenum(APIENTRY*)(GLenum);
    using GlGenRenderbuffers = void(APIENTRY*)(GLsizei, GLuint*);
    using GlBindRenderbuffer = void(APIENTRY*)(GLenum, GLuint);
    using GlRenderbufferStorage = void(APIENTRY*)(GLenum, GLenum, GLsizei, GLsizei);
    using GlFramebufferRenderbuffer = void(APIENTRY*)(GLenum, GLenum, GLenum, GLuint);
    using GlDeleteRenderbuffers = void(APIENTRY*)(GLsizei, const GLuint*);
    constexpr GLenum gl_framebuffer = 0x8D40;
    constexpr GLenum gl_renderbuffer = 0x8D41;
    constexpr GLenum gl_color_attachment0 = 0x8CE0;
    constexpr GLenum gl_depth_attachment = 0x8D00;
    constexpr GLenum gl_depth_component24 = 0x81A6;
    constexpr GLenum gl_framebuffer_complete = 0x8CD5;
    constexpr GLenum gl_framebuffer_binding = 0x8CA6;

    struct ItemIconCacheEntry {
        GLuint framebuffer = 0;
        GLuint texture = 0;
        GLuint depth = 0;
        bool ready = false;
    };
    std::unordered_map<unsigned long long, ItemIconCacheEntry> minecraft_item_icon_cache;

    static unsigned long long equipment_cache_key(const EquipmentItem& item)
    {
        unsigned long long key = (static_cast<unsigned long long>(static_cast<unsigned int>(item.item_id)) << 32) |
            static_cast<unsigned int>(item.metadata);
        if (item.dye_color >= 0)
            key ^= static_cast<unsigned long long>(static_cast<unsigned int>(item.dye_color)) * 2654435761ULL;
        return key;
    }
    jobject minecraft_font_renderer = nullptr;
    jmethodID minecraft_font_width_method = nullptr;
    jmethodID minecraft_font_draw_method = nullptr;
    jobject minecraft_item_renderer = nullptr;
    jmethodID minecraft_item_draw_method = nullptr;
    jclass minecraft_render_helper_class = nullptr;
    jmethodID minecraft_enable_item_lighting = nullptr;
    jmethodID minecraft_disable_item_lighting = nullptr;
    jclass minecraft_gl_state_class = nullptr;
    jmethodID minecraft_push_matrix = nullptr;
    jmethodID minecraft_pop_matrix = nullptr;
    jmethodID minecraft_enable_rescale_normal = nullptr;
    jmethodID minecraft_disable_rescale_normal = nullptr;
    jmethodID minecraft_disable_lighting = nullptr;
    jmethodID minecraft_color = nullptr;
    jmethodID minecraft_scale = nullptr;
    jmethodID minecraft_bind_texture = nullptr;
    jclass minecraft_enchantment_helper_class = nullptr;
    jmethodID minecraft_get_enchantment_level = nullptr;
    jmethodID minecraft_enable_depth = nullptr, minecraft_disable_depth = nullptr;
    jmethodID minecraft_enable_blend = nullptr, minecraft_disable_blend = nullptr;
    jmethodID minecraft_enable_alpha = nullptr, minecraft_disable_alpha = nullptr;
    jmethodID minecraft_enable_texture = nullptr, minecraft_disable_texture = nullptr;
    jmethodID minecraft_enable_lighting = nullptr;
    jmethodID minecraft_enable_cull = nullptr, minecraft_disable_cull = nullptr;
    jmethodID minecraft_enable_fog = nullptr, minecraft_disable_fog = nullptr;

    JNIEnv* current_thread_env()
    {
        JNIEnv* env = nullptr;
        if (!sdk::jvm || sdk::jvm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK)
            return nullptr;
        return env;
    }

    std::string minecraft_format(std::string text)
    {
        std::string formatted;
        formatted.reserve(text.size() + 8);
        for (size_t i = 0; i < text.size(); ++i) {
            if (text[i] == '&' && i + 1 < text.size()) {
                formatted.append("\xC2\xA7");
                continue;
            }
            formatted.push_back(text[i]);
        }
        return formatted;
    }

    bool resolve_minecraft_font()
    {
        if (minecraft_font_renderer && minecraft_font_width_method && minecraft_font_draw_method)
            return true;
        if (!sdk::jni || !mapper::classes["Minecraft"].klass || !mapper::classes["FontRenderer"].klass)
            return false;

        mapper::__minecraft minecraft;
        if (!minecraft.object) return false;

        const std::string signature = mapper::classes["FontRenderer"].signature;
        jobject local_renderer = nullptr;
        for (const auto& field : mapper::classes["Minecraft"].fields) {
            if (field.signature != signature || !field.identifier) continue;
            local_renderer = sdk::jni->GetObjectField(minecraft.object, field.identifier);
            if (sdk::jni->ExceptionCheck()) sdk::jni->ExceptionClear();
            if (local_renderer) break;
        }
        if (!local_renderer) return false;

        jclass renderer_class = sdk::jni->GetObjectClass(local_renderer);
        if (!renderer_class) {
            sdk::jni->DeleteLocalRef(local_renderer);
            return false;
        }

        minecraft_font_width_method = find_method_dynamic(renderer_class,
            {"getStringWidth", "func_78256_a", "a"},
            [](const std::string& signature) { return signature == "(Ljava/lang/String;)I"; });
        minecraft_font_draw_method = find_method_dynamic(renderer_class,
            {"drawStringWithShadow", "func_175063_a", "a"},
            [](const std::string& signature) { return signature == "(Ljava/lang/String;FFI)I"; });

        if (minecraft_font_width_method && minecraft_font_draw_method)
            minecraft_font_renderer = sdk::jni->NewGlobalRef(local_renderer);

        sdk::jni->DeleteLocalRef(renderer_class);
        sdk::jni->DeleteLocalRef(local_renderer);
        return minecraft_font_renderer && minecraft_font_width_method && minecraft_font_draw_method;
    }

    static std::string abbreviated_enchantment(int id, int level)
    {
        if (level <= 0) return {};
        const char* abbreviation = nullptr;
        switch (id) {
        case 0: abbreviation = "P"; break;   case 1: abbreviation = "FP"; break;
        case 2: abbreviation = "FF"; break; case 3: abbreviation = "BP"; break;
        case 4: abbreviation = "PP"; break; case 5: abbreviation = "R"; break;
        case 6: abbreviation = "A"; break;  case 7: abbreviation = "T"; break;
        case 8: abbreviation = "DS"; break; case 16: abbreviation = "S"; break;
        case 17: abbreviation = "Sm"; break; case 18: abbreviation = "BoA"; break;
        case 19: abbreviation = "K"; break; case 20: abbreviation = "FA"; break;
        case 21: abbreviation = "L"; break; case 32: abbreviation = "E"; break;
        case 33: abbreviation = "ST"; break; case 34: abbreviation = "U"; break;
        case 35: abbreviation = "F"; break; case 48: abbreviation = "Po"; break;
        case 49: abbreviation = "Pu"; break; case 50: abbreviation = "Fl"; break;
        case 51: abbreviation = "I"; break; default: return {};
        }
        return std::string(abbreviation) + std::to_string(level);
    }

    static std::vector<std::string> read_enchantments(jobject stack)
    {
        std::vector<std::string> result;
        if (!sdk::jni || !stack) return result;
        JNIEnv* env = sdk::jni;
        if (minecraft_enchantment_helper_class && minecraft_get_enchantment_level) {
            // This is the same public 1.8.9 path used by the reference client.
            // It works for remote-player stacks even when a server/client proxy
            // exposes the enchantment list through a nonstandard NBT wrapper.
            static constexpr int displayed_ids[] = {
                0, 34, 16, 20, 32, 2, 48, 50, 49, 35, 51, 7, 19,
                1, 3, 4, 5, 6, 8, 17, 18, 21, 33
            };
            for (int id : displayed_ids) {
                const int level = env->CallStaticIntMethod(
                    minecraft_enchantment_helper_class, minecraft_get_enchantment_level, id, stack);
                if (env->ExceptionCheck()) { env->ExceptionClear(); continue; }
                if (level > 0) {
                    std::string text = abbreviated_enchantment(id, level);
                    if (!text.empty()) result.push_back(std::move(text));
                }
            }
            if (!result.empty()) return result;
        }
        jclass stack_class = env->GetObjectClass(stack);
        if (!stack_class) return result;
        jmethodID get_list = find_method_dynamic(stack_class,
            {"getEnchantmentTagList", "func_77986_q", "p"},
            [](const std::string& sig) { return sig.rfind("()L", 0) == 0; });
        jobject list = get_list ? env->CallObjectMethod(stack, get_list) : nullptr;
        if (env->ExceptionCheck()) { env->ExceptionClear(); list = nullptr; }
        env->DeleteLocalRef(stack_class);
        if (!list) return result;

        jclass list_class = env->GetObjectClass(list);
        jmethodID count = list_class ? find_method_dynamic(list_class,
            {"tagCount", "func_74745_c", "c"}, [](const std::string& sig) { return sig == "()I"; }) : nullptr;
        jmethodID compound_at = list_class ? find_method_dynamic(list_class,
            {"getCompoundTagAt", "func_150305_b", "b"},
            [](const std::string& sig) { return sig.rfind("(I)L", 0) == 0; }) : nullptr;
        const int total = count ? env->CallIntMethod(list, count) : 0;
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            if (list_class) env->DeleteLocalRef(list_class);
            env->DeleteLocalRef(list);
            return result;
        }
        for (int i = 0; compound_at && i < total && i < 12; ++i) {
            jobject tag = env->CallObjectMethod(list, compound_at, i);
            if (env->ExceptionCheck()) { env->ExceptionClear(); continue; }
            if (!tag) continue;
            jclass tag_class = env->GetObjectClass(tag);
            jmethodID get_short = tag_class ? find_method_dynamic(tag_class,
                {"getShort", "func_74765_d", "e"},
                [](const std::string& sig) { return sig == "(Ljava/lang/String;)S"; }) : nullptr;
            if (get_short) {
                jstring id_key = env->NewStringUTF("id");
                jstring level_key = env->NewStringUTF("lvl");
                const int id = env->CallShortMethod(tag, get_short, id_key);
                const int level = env->CallShortMethod(tag, get_short, level_key);
                env->DeleteLocalRef(id_key);
                env->DeleteLocalRef(level_key);
                if (!env->ExceptionCheck()) {
                    std::string text = abbreviated_enchantment(id, level);
                    if (!text.empty()) result.push_back(std::move(text));
                } else env->ExceptionClear();
            }
            if (tag_class) env->DeleteLocalRef(tag_class);
            env->DeleteLocalRef(tag);
        }
        if (list_class) env->DeleteLocalRef(list_class);
        env->DeleteLocalRef(list);
        return result;
    }

    static bool resolve_item_renderer()
    {
        if (minecraft_item_renderer && minecraft_item_draw_method) return true;
        if (!sdk::jni || mapper::version != mapper::MINECRAFT_18 ||
            !mapper::classes["Minecraft"].klass || !mapper::classes["RenderItem"].klass) return false;
        mapper::__minecraft minecraft;
        if (!minecraft.object) return false;
        const std::string renderer_sig = mapper::classes["RenderItem"].signature;
        jmethodID get_renderer = find_method_dynamic(mapper::classes["Minecraft"].klass,
            {"getRenderItem", "func_175599_af", "ag"},
            [&](const std::string& sig) { return sig == "()" + renderer_sig; });
        jobject renderer = get_renderer ? sdk::jni->CallObjectMethod(minecraft.object, get_renderer) : nullptr;
        if (sdk::jni->ExceptionCheck()) { sdk::jni->ExceptionClear(); renderer = nullptr; }
        if (!renderer) return false;
        minecraft_item_draw_method = find_method_dynamic(mapper::classes["RenderItem"].klass,
            {"renderItemAndEffectIntoGUI", "func_180450_b", "b"},
            [&](const std::string& sig) { return sig == "(" + mapper::classes["ItemStack"].signature + "II)V"; });
        if (minecraft_item_draw_method) minecraft_item_renderer = sdk::jni->NewGlobalRef(renderer);
        sdk::jni->DeleteLocalRef(renderer);

        if (!mapper::classes.contains("GlStateManager"))
            mapper::classes["GlStateManager"] = mapper::get_class(
                mapper::classes["Minecraft"].name.find('/') != std::string::npos ?
                "net/minecraft/client/renderer/GlStateManager" : "bfl");
        if (!mapper::classes.contains("RenderHelper"))
            mapper::classes["RenderHelper"] = mapper::get_class(
                mapper::classes["Minecraft"].name.find('/') != std::string::npos ?
                "net/minecraft/client/renderer/RenderHelper" : "avc");
        if (!mapper::classes.contains("EnchantmentHelper"))
            mapper::classes["EnchantmentHelper"] = mapper::get_class(
                mapper::classes["Minecraft"].name.find('/') != std::string::npos ?
                "net/minecraft/enchantment/EnchantmentHelper" : "ack");
        minecraft_gl_state_class = mapper::classes["GlStateManager"].klass;
        minecraft_render_helper_class = mapper::classes["RenderHelper"].klass;
        minecraft_enchantment_helper_class = mapper::classes["EnchantmentHelper"].klass;
        if (minecraft_enchantment_helper_class) {
            minecraft_get_enchantment_level = find_method_dynamic(minecraft_enchantment_helper_class,
                {"getEnchantmentLevel", "func_77506_a", "a"},
                [&](const std::string& sig) {
                    return sig == "(I" + mapper::classes["ItemStack"].signature + ")I";
                });
        }
        if (minecraft_gl_state_class) {
            minecraft_push_matrix = find_method_dynamic(minecraft_gl_state_class,
                {"pushMatrix", "func_179094_E", "E"}, [](const std::string& sig) { return sig == "()V"; });
            minecraft_pop_matrix = find_method_dynamic(minecraft_gl_state_class,
                {"popMatrix", "func_179121_F", "F"}, [](const std::string& sig) { return sig == "()V"; });
            minecraft_enable_rescale_normal = find_method_dynamic(minecraft_gl_state_class,
                {"enableRescaleNormal", "func_179091_B", "B"}, [](const std::string& sig) { return sig == "()V"; });
            minecraft_disable_rescale_normal = find_method_dynamic(minecraft_gl_state_class,
                {"disableRescaleNormal", "func_179101_C", "C"}, [](const std::string& sig) { return sig == "()V"; });
            minecraft_disable_lighting = find_method_dynamic(minecraft_gl_state_class,
                {"disableLighting", "func_179140_f", "f"}, [](const std::string& sig) { return sig == "()V"; });
            minecraft_color = find_method_dynamic(minecraft_gl_state_class,
                {"color", "func_179131_c", "c"}, [](const std::string& sig) { return sig == "(FFFF)V"; });
            minecraft_scale = find_method_dynamic(minecraft_gl_state_class,
                {"scale", "func_179152_a", "a"}, [](const std::string& sig) { return sig == "(FFF)V"; });
            minecraft_bind_texture = find_method_dynamic(minecraft_gl_state_class,
                {"bindTexture", "func_179144_i", "i"}, [](const std::string& sig) { return sig == "(I)V"; });
            auto state_method = [&](std::initializer_list<const char*> names) {
                return find_method_dynamic(minecraft_gl_state_class, names,
                    [](const std::string& sig) { return sig == "()V"; });
            };
            minecraft_enable_depth = state_method({"enableDepth", "func_179126_j", "j"});
            minecraft_disable_depth = state_method({"disableDepth", "func_179097_i", "i"});
            minecraft_enable_blend = state_method({"enableBlend", "func_179147_l", "l"});
            minecraft_disable_blend = state_method({"disableBlend", "func_179084_k", "k"});
            minecraft_enable_alpha = state_method({"enableAlpha", "func_179141_d", "d"});
            minecraft_disable_alpha = state_method({"disableAlpha", "func_179118_c", "c"});
            minecraft_enable_texture = state_method({"enableTexture2D", "func_179098_w", "w"});
            minecraft_disable_texture = state_method({"disableTexture2D", "func_179090_x", "x"});
            minecraft_enable_lighting = state_method({"enableLighting", "func_179145_e", "e"});
            minecraft_enable_cull = state_method({"enableCull", "func_179089_o", "o"});
            minecraft_disable_cull = state_method({"disableCull", "func_179129_p", "p"});
            minecraft_enable_fog = state_method({"enableFog", "func_179127_m", "m"});
            minecraft_disable_fog = state_method({"disableFog", "func_179106_n", "n"});
        }
        if (minecraft_render_helper_class) {
            minecraft_enable_item_lighting = find_method_dynamic(minecraft_render_helper_class,
                {"enableGUIStandardItemLighting", "func_74520_c", "c"}, [](const std::string& sig) { return sig == "()V"; });
            minecraft_disable_item_lighting = find_method_dynamic(minecraft_render_helper_class,
                {"disableStandardItemLighting", "func_74518_a", "a"}, [](const std::string& sig) { return sig == "()V"; });
        }
        return minecraft_item_renderer && minecraft_item_draw_method;
    }


    float minecraft_text_width(const std::string& text, float render_scale = -1.0f)
    {
        JNIEnv* env = current_thread_env();
        if (!env || !minecraft_font_renderer || !minecraft_font_width_method) return -1.0f;
        const std::string formatted = minecraft_format(text);
        jstring java_text = env->NewStringUTF(formatted.c_str());
        if (!java_text) return -1.0f;
        const jint width = env->CallIntMethod(minecraft_font_renderer, minecraft_font_width_method, java_text);
        env->DeleteLocalRef(java_text);
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            return -1.0f;
        }
        return static_cast<float>(width) * (render_scale > 0.0f ? render_scale : minecraft_font_scale());
    }

    void queue_minecraft_text(const std::string& text, float x, float y, ImU32 color, float render_scale)
    {
        const ImVec4 rgba = ImGui::ColorConvertU32ToFloat4(color);
        const int packed = ((int)(rgba.w * 255.0f) << 24) |
            ((int)(rgba.x * 255.0f) << 16) |
            ((int)(rgba.y * 255.0f) << 8) |
            (int)(rgba.z * 255.0f);
        const float width = (std::max)(0.0f, minecraft_text_width(text, render_scale));
        minecraft_text_commands.push_back({minecraft_format(text), x, y, width,
            9.0f * render_scale, render_scale, packed});
    }
}

struct Nametag_Entry
{
    std::string    formatted_name;
    mapper::__vec3 position;
    mapper::__vec3 old_position;
    float          height_offset = 2.15f;
    mapper::__vec4 color;
    float          health = 0.f;
    float          player_distance = 0.0f;
    bool own = false;
    std::shared_ptr<const EquipmentSnapshot> equipment;
};

static std::shared_ptr<const std::vector<Nametag_Entry>> g_nametag_snapshot =
    std::make_shared<const std::vector<Nametag_Entry>>();
static std::mutex                 g_nametag_mutex;

static void clear_nametag_snapshot()
{
    std::lock_guard<std::mutex> lock(g_nametag_mutex);
    g_nametag_snapshot = std::make_shared<const std::vector<Nametag_Entry>>();
}

static double g_cam_x = 0.0, g_cam_y = 0.0, g_cam_z = 0.0;
static float g_buffer_partial_ticks = 0.0f;
static int g_buffer_world_tick = -1;
static mapper::__vec2 project_nametag(const mapper::__vec3& p,
    const double model[16], const double projection[16], const int viewport[4])
{
    double eye[4] = {
        p.x * model[0] + p.y * model[4] + p.z * model[8]  + model[12],
        p.x * model[1] + p.y * model[5] + p.z * model[9]  + model[13],
        p.x * model[2] + p.y * model[6] + p.z * model[10] + model[14],
        p.x * model[3] + p.y * model[7] + p.z * model[11] + model[15]
    };
    double clip[4] = {
        eye[0] * projection[0] + eye[1] * projection[4] + eye[2] * projection[8]  + eye[3] * projection[12],
        eye[0] * projection[1] + eye[1] * projection[5] + eye[2] * projection[9]  + eye[3] * projection[13],
        eye[0] * projection[2] + eye[1] * projection[6] + eye[2] * projection[10] + eye[3] * projection[14],
        eye[0] * projection[3] + eye[1] * projection[7] + eye[2] * projection[11] + eye[3] * projection[15]
    };
    if (!std::isfinite(clip[3]) || clip[3] <= 0.0001) return {FLT_MAX, FLT_MAX};
    const double nx = clip[0] / clip[3], ny = clip[1] / clip[3], nz = clip[2] / clip[3];
    if (!std::isfinite(nx) || !std::isfinite(ny) || nz < -1.0 || nz > 1.0)
        return {FLT_MAX, FLT_MAX};
    return {
        (float)(viewport[0] + (nx + 1.0) * viewport[2] * 0.5),
        (float)(viewport[1] + (1.0 - ny) * viewport[3] * 0.5)
    };
}


struct ProfileCacheEntry {
    std::string original_name;
};
static std::unordered_map<int, ProfileCacheEntry> g_profile_cache;

static std::string get_tablist_name(mapper::__minecraft& minecraft, mapper::__player& player) {
    if (!sdk::jni || !minecraft.object || !player.object) return {};
    JNIEnv* env = sdk::jni;
    jclass player_cls = env->GetObjectClass(player.object);
    jmethodID get_uuid = player_cls ? env->GetMethodID(player_cls, "getUniqueID", "()Ljava/util/UUID;") : nullptr;
    if (!get_uuid) { env->ExceptionClear(); get_uuid = player_cls ? env->GetMethodID(player_cls, "func_110124_au", "()Ljava/util/UUID;") : nullptr; }
    if (!get_uuid) { env->ExceptionClear(); get_uuid = player_cls ? find_method_dynamic(player_cls, {}, [](const std::string& s) { return s == "()Ljava/util/UUID;"; }) : nullptr; }
    jobject uuid = get_uuid ? env->CallObjectMethod(player.object, get_uuid) : nullptr;
    if (env->ExceptionCheck()) { env->ExceptionClear(); uuid = nullptr; }
    if (player_cls) env->DeleteLocalRef(player_cls);
    if (!uuid) return {};

    auto& mc_cls = mapper::classes["Minecraft"];
    if (!mc_cls.klass) {
        env->DeleteLocalRef(uuid);
        return {};
    }
    std::string handler_sig = "()" + mapper::classes["NetHandlerPlayClient"].signature;
    if (handler_sig == "()") {
        env->DeleteLocalRef(uuid);
        return {};
    }
    jmethodID get_handler = env->GetMethodID(mc_cls.klass, "getNetHandler", handler_sig.c_str());
    if (!get_handler) { env->ExceptionClear(); get_handler = env->GetMethodID(mc_cls.klass, "func_147114_u", handler_sig.c_str()); }
    if (!get_handler) { env->ExceptionClear(); get_handler = find_method_dynamic(mc_cls.klass, {}, [&](const std::string& s) { return s == handler_sig; }); }
    jobject handler = get_handler ? env->CallObjectMethod(minecraft.object, get_handler) : nullptr;
    if (env->ExceptionCheck()) { env->ExceptionClear(); handler = nullptr; }
    if (!handler) { env->DeleteLocalRef(uuid); return {}; }

    jclass handler_cls = env->GetObjectClass(handler);
    jmethodID get_info = handler_cls ? find_method_dynamic(handler_cls,
        {"getPlayerInfo", "func_175102_a"}, [](const std::string& s) {
            return s.rfind("(Ljava/util/UUID;)", 0) == 0 && s.back() == ';';
        }) : nullptr;
    jobject info = get_info ? env->CallObjectMethod(handler, get_info, uuid) : nullptr;
    if (env->ExceptionCheck()) { env->ExceptionClear(); info = nullptr; }
    env->DeleteLocalRef(uuid);
    if (handler_cls) env->DeleteLocalRef(handler_cls);
    env->DeleteLocalRef(handler);
    if (!info) return {};

    jclass info_cls = env->GetObjectClass(info);
    jmethodID get_profile = info_cls ? find_method_dynamic(info_cls,
        {"getGameProfile", "func_178845_a"}, [](const std::string& s) {
            return s.find("GameProfile;") != std::string::npos && s.rfind("()", 0) == 0;
        }) : nullptr;
    jobject profile = get_profile ? env->CallObjectMethod(info, get_profile) : nullptr;
    if (env->ExceptionCheck()) { env->ExceptionClear(); profile = nullptr; }
    if (info_cls) env->DeleteLocalRef(info_cls);
    env->DeleteLocalRef(info);
    if (!profile) return {};

    jclass profile_cls = env->GetObjectClass(profile);
    jmethodID get_name = profile_cls ? env->GetMethodID(profile_cls, "getName", "()Ljava/lang/String;") : nullptr;
    jstring value = get_name ? (jstring)env->CallObjectMethod(profile, get_name) : nullptr;
    if (env->ExceptionCheck()) { env->ExceptionClear(); value = nullptr; }
    std::string result;
    if (value) {
        const char* chars = env->GetStringUTFChars(value, nullptr);
        if (chars) {
            // Sanitize: strip Minecraft formatting sequences (§ + code) and
            // non-ASCII/non-printable characters, matching player.get_name().
            // Without this, trailing formatting codes like §a leave an orphan 'a'.
            for (int i = 0; chars[i] != '\0'; ++i) {
                unsigned char c = (unsigned char)chars[i];
                // Skip UTF-8 section sign (0xC2 0xA7) + the color/format char after it
                if (c == 0xC2 && (unsigned char)chars[i + 1] == 0xA7 && chars[i + 2] != '\0') {
                    i += 2; // skip §X (3 bytes total)
                    continue;
                }
                if (c >= 32 && c <= 126) {
                    result += (char)c;
                }
            }
            env->ReleaseStringUTFChars(value, chars);
        }
        env->DeleteLocalRef(value);
    }
    if (profile_cls) env->DeleteLocalRef(profile_cls);
    env->DeleteLocalRef(profile);
    return result;
}

// ============================================================
// ============================================================
struct JNIFrame {
    JNIEnv* env;
    bool active = false;
    JNIFrame(JNIEnv* e, int cap) : env(e) {
        active = env && env->PushLocalFrame(cap) == JNI_OK;
        if (env && !active && env->ExceptionCheck()) env->ExceptionClear();
    }
    ~JNIFrame() { if (active) env->PopLocalFrame(nullptr); }
};

auto features::visual::nametags::run(mapper::__minecraft& minecraft) -> void
{
    const bool current_enabled = features::visual::nametags::enabled;
    sync_hide_vanilla_hook(current_enabled && gui_nametags_show_name && gui_nametags_hide_vanilla);

    if (!current_enabled) {
        clear_nametag_snapshot();
        return;
    }

    // Resolve/cache the renderer only from the logic thread, where sdk::jni is valid.
    resolve_minecraft_font();
    if (gui_nametags_show_equipment) resolve_item_renderer();


    auto timer = minecraft.get_timer();
    if (timer.object == nullptr) {
        clear_nametag_snapshot();
        return;
    }
    float partial_ticks = timer.get_partial_ticks();

    auto world = minecraft.get_world();
    auto local_player = minecraft.get_local_player();
    if (world.object == nullptr || local_player.object == nullptr) {
        clear_nametag_snapshot();
        g_profile_cache.clear();
        return;
    }

    auto local_pos = local_player.get_view_position(partial_ticks);
    const int world_tick = local_player.get_ticks_existed();
    auto world_players = world.get_players();
    if (world_players.empty()) {
        clear_nametag_snapshot();
        return;
    }

    auto rm = minecraft.get_render_manager();
    double cam_x = 0.0, cam_y = 0.0, cam_z = 0.0;
    if (rm.object) {
        cam_x = rm.get_render_pos_x();
        cam_y = rm.get_render_pos_y();
        cam_z = rm.get_render_pos_z();
    }

    std::vector<Nametag_Entry> temp;
    temp.reserve(world_players.size());
    std::unordered_set<int> live_entity_ids;
    live_entity_ids.reserve(world_players.size());

    const bool   use_fake = features::visual::nametags::use_fake_name;
    const int third_person_view = get_third_person_view(minecraft);
    for (auto& player : world_players)
    {
        if (player.object == nullptr) continue;
        if (sdk::jni->ExceptionCheck()) sdk::jni->ExceptionClear();
        const bool is_own = sdk::jni->IsSameObject(local_player.object, player.object);
        if (is_own && !gui_nametags_show_own) continue;
        if (is_own && third_person_view == 0) continue;

        JNIFrame frame(sdk::jni, 128);

        float health = player.get_health();
        if (health <= 0.f) continue;

        int eid = player.get_entity_id();
        // Entity IDs are signed. Servers commonly assign negative IDs to
        // player-shaped NPCs/bots, and ESP intentionally accepts them too.
        if (!live_entity_ids.insert(eid).second) continue;

        ProfileCacheEntry& entry_data = g_profile_cache[eid];

        // Use the same entity name path as the original implementation. This
        // preserves the server/Lunar behavior that was stable before the
        // profile/API experiments.
        std::string current_name = entry_data.original_name;
        // The local player never needs the network tab-list path. Avoiding it
        // removes the only extra NetHandler lookup introduced by Show Own.
        if (current_name.empty() && !is_own) current_name = get_tablist_name(minecraft, player);
        if (current_name.empty()) current_name = player.get_name();
        // NPCs/bots are often absent from the tab list; Entity#getName above
        // is their valid fallback and matches the entity selection used by ESP.
        if (current_name.empty() && !is_own) continue;
        if (entry_data.original_name.empty()) {
            if (!current_name.empty() && current_name != " ") {
                entry_data.original_name = current_name;
                
            }
        }

        std::string name = entry_data.original_name;
        if (name.empty()) continue;

        if (!features::visual::nametags::draw_invisible_players && player.get_flag(5)) continue;

        auto   player_pos = player.get_view_position(partial_ticks);
        double distance = local_pos.get_distance_to_vec3(player_pos);
        if (distance > 255.0) continue;

        bool is_sneaking = player.get_flag(1);
        float height_offset = is_sneaking ? 1.85f : 2.15f;
        // Reserve world-space room above the vanilla label, including while zoomed.
        if (!gui_nametags_show_name) height_offset += 0.4f;

        Nametag_Entry entry;
        entry.formatted_name = name;
        entry.position = player.get_position();
        entry.old_position = player.get_old_position();
        entry.height_offset = height_offset;
        entry.color = features::visual::nametags::color;
        entry.health = health;
        entry.player_distance = static_cast<float>(distance);
        entry.own = is_own;

        if (gui_nametags_show_equipment && minecraft_item_renderer) {
            auto equipment = std::make_shared<EquipmentSnapshot>();
            auto add_stack = [&](mapper::__item_stack& stack) {
                if (!stack.object) return;
                EquipmentItem item;
                item.stack = sdk::jni->NewGlobalRef(stack.object);
                auto base_item = stack.get_item();
                item.item_id = base_item.object ? base_item.get_id() : -1;
                jclass stack_class = sdk::jni->GetObjectClass(stack.object);
                jfieldID count_field = stack_class ? sdk::jni->GetFieldID(stack_class, "stackSize", "I") : nullptr;
                if (!count_field) { sdk::jni->ExceptionClear(); count_field = stack_class ? sdk::jni->GetFieldID(stack_class, "field_77994_a", "I") : nullptr; }
                if (!count_field) { sdk::jni->ExceptionClear(); count_field = stack_class ? sdk::jni->GetFieldID(stack_class, "b", "I") : nullptr; }
                if (count_field) item.count = (std::max)(1, static_cast<int>(sdk::jni->GetIntField(stack.object, count_field)));
                else sdk::jni->ExceptionClear();
                jmethodID damageable = stack_class ? sdk::jni->GetMethodID(stack_class, "isItemStackDamageable", "()Z") : nullptr;
                if (!damageable) { sdk::jni->ExceptionClear(); damageable = stack_class ? sdk::jni->GetMethodID(stack_class, "func_77984_f", "()Z") : nullptr; }
                if (!damageable) { sdk::jni->ExceptionClear(); damageable = stack_class ? sdk::jni->GetMethodID(stack_class, "e", "()Z") : nullptr; }
                bool can_wear = damageable && sdk::jni->CallBooleanMethod(stack.object, damageable);
                if (sdk::jni->ExceptionCheck()) { sdk::jni->ExceptionClear(); can_wear = false; }
                if (stack_class) sdk::jni->DeleteLocalRef(stack_class);
                const int max_damage = stack.get_max_damage();
                item.metadata = max_damage > 0 ? 0 : stack.get_item_damage();
                if (can_wear && max_damage > 0) {
                    const int damage = (std::clamp)(stack.get_item_damage(), 0, max_damage);
                    item.durability = 1.0f - static_cast<float>(damage) / static_cast<float>(max_damage);
                }
                if (item.item_id == 298 || item.item_id == 299 || item.item_id == 300 || item.item_id == 301) {
                    JNIEnv* env = sdk::jni;
                    jclass sc = env->GetObjectClass(stack.object);
                    jmethodID get_tag = sc ? find_method_dynamic(sc,
                        {"getTagCompound", "func_77978_p", "p"},
                        [](const std::string& s) { return s.rfind("()L", 0) == 0; }) : nullptr;
                    jobject nbt = get_tag ? env->CallObjectMethod(stack.object, get_tag) : nullptr;
                    if (env->ExceptionCheck()) { env->ExceptionClear(); nbt = nullptr; }
                    if (nbt) {
                        jclass nbt_class = env->GetObjectClass(nbt);
                        jmethodID get_compound = nbt_class ? find_method_dynamic(nbt_class,
                            {"getCompoundTag", "func_74775_l", "k"},
                            [](const std::string& s) { return s == "(Ljava/lang/String;)Lnet/minecraft/nbt/NBTTagCompound;" || (s.rfind("(Ljava/lang/String;)L", 0) == 0 && s.back() == ';'); }) : nullptr;
                        if (get_compound) {
                            jstring display_key = env->NewStringUTF("display");
                            jobject display = display_key ? env->CallObjectMethod(nbt, get_compound, display_key) : nullptr;
                            if (env->ExceptionCheck()) { env->ExceptionClear(); display = nullptr; }
                            if (display_key) env->DeleteLocalRef(display_key);
                            if (display) {
                                jclass display_class = env->GetObjectClass(display);
                                jmethodID has_key = display_class ? find_method_dynamic(display_class,
                                    {"hasKey", "func_74764_b", "b"},
                                    [](const std::string& s) { return s == "(Ljava/lang/String;)Z"; }) : nullptr;
                                jmethodID get_int = display_class ? find_method_dynamic(display_class,
                                    {"getInteger", "func_74762_e", "e"},
                                    [](const std::string& s) { return s == "(Ljava/lang/String;)I"; }) : nullptr;
                                if (has_key && get_int) {
                                    jstring color_key = env->NewStringUTF("color");
                                    if (color_key) {
                                        jboolean has_color = env->CallBooleanMethod(display, has_key, color_key);
                                        if (!env->ExceptionCheck() && has_color) {
                                            item.dye_color = env->CallIntMethod(display, get_int, color_key);
                                            if (env->ExceptionCheck()) { env->ExceptionClear(); item.dye_color = -1; }
                                        } else if (env->ExceptionCheck()) env->ExceptionClear();
                                        env->DeleteLocalRef(color_key);
                                    }
                                }
                                if (display_class) env->DeleteLocalRef(display_class);
                                env->DeleteLocalRef(display);
                            }
                        }
                        if (nbt_class) env->DeleteLocalRef(nbt_class);
                        env->DeleteLocalRef(nbt);
                    }
                    if (sc) env->DeleteLocalRef(sc);
                }
                if (gui_nametags_show_enchantments)
                    item.enchantments = read_enchantments(stack.object);
                if (item.stack) equipment->items.push_back(std::move(item));
            };
            auto held = player.get_held_item_stack();
            add_stack(held);
            for (int slot = 39; slot >= 36; --slot) {
                auto armor = player.get_inventory_slot(slot);
                add_stack(armor);
            }
            if (!equipment->items.empty()) entry.equipment = std::move(equipment);
        }

        temp.push_back(std::move(entry));
    }

    // Entity IDs are reused between worlds. Keeping old entries indefinitely
    // caused stale names and unbounded growth on long sessions/server changes.
    for (auto it = g_profile_cache.begin(); it != g_profile_cache.end();) {
        if (!live_entity_ids.contains(it->first)) it = g_profile_cache.erase(it);
        else ++it;
    }

    {
        std::lock_guard<std::mutex> lock(g_nametag_mutex);
        g_nametag_snapshot = std::make_shared<const std::vector<Nametag_Entry>>(std::move(temp));
        g_cam_x = cam_x;
        g_cam_y = cam_y;
        g_cam_z = cam_z;
        g_buffer_partial_ticks = partial_ticks;
        g_buffer_world_tick = world_tick;
    }
}

void features::visual::nametags::shutdown(mapper::__minecraft& minecraft)
{
    std::lock_guard<std::recursive_mutex> resource_lock(minecraft_render_mutex);
    g_profile_cache.clear();
    minecraft_text_commands.clear();
    minecraft_item_commands.clear();
	{
		std::lock_guard<std::mutex> lock(g_nametag_mutex);
		g_nametag_snapshot = std::make_shared<const std::vector<Nametag_Entry>>();
	}
    if (minecraft_font_renderer && sdk::jni) {
        sdk::jni->DeleteGlobalRef(minecraft_font_renderer);
        minecraft_font_renderer = nullptr;
    }
    minecraft_font_width_method = nullptr;
    minecraft_font_draw_method = nullptr;
    if (minecraft_item_renderer && sdk::jni) {
        sdk::jni->DeleteGlobalRef(minecraft_item_renderer);
        minecraft_item_renderer = nullptr;
    }
    minecraft_item_draw_method = nullptr;
}

void features::visual::nametags::abandon_render_resources()
{
    std::lock_guard<std::recursive_mutex> resource_lock(minecraft_render_mutex);
    const auto delete_framebuffers = reinterpret_cast<GlDeleteFramebuffers>(safe_gl_proc("glDeleteFramebuffers"));
    const auto delete_renderbuffers = reinterpret_cast<GlDeleteRenderbuffers>(safe_gl_proc("glDeleteRenderbuffers"));
    for (auto& [key, icon] : minecraft_item_icon_cache) {
        if (icon.depth && delete_renderbuffers) delete_renderbuffers(1, &icon.depth);
        if (icon.framebuffer && delete_framebuffers) delete_framebuffers(1, &icon.framebuffer);
        if (icon.texture) glDeleteTextures(1, &icon.texture);
    }
    minecraft_item_icon_cache.clear();
    minecraft_item_commands.clear();
}

// ============================================================
// ============================================================
static inline void DrawShadowText(ImDrawList* draw, ImFont* font, float font_size,
    ImVec2 pos, ImU32 color, const char* text)
{
    draw->AddText(font, font_size, { pos.x + 1.f, pos.y + 1.f }, IM_COL32(20, 20, 20, 240), text);
    draw->AddText(font, font_size, pos, color, text);
}

// ============================================================
// CALCULO DE TAMA?O ? buffer en stack, cero heap allocations
// ============================================================
static void DrawPixelHeart(ImDrawList* draw, ImVec2 pos, float scale, ImU32 color) {
    const char* heart_pixels =
        ".XX.XX."
        "XXXXXXX"
        "XXXXXXX"
        ".XXXXX."
        "..XXX.."
        "...X...";
    float p_sz = roundf(scale * 3.0f);
    if (p_sz < 1.0f) p_sz = 1.0f;
    for (int y = 0; y < 6; ++y) {
        for (int x = 0; x < 7; ++x) {
            if (heart_pixels[y * 7 + x] == 'X') {
                ImVec2 p_min = { pos.x + x * p_sz + 1.f, pos.y + y * p_sz + 1.f };
                ImVec2 p_max = { p_min.x + p_sz, p_min.y + p_sz };
                draw->AddRectFilled(p_min, p_max, IM_COL32(20,20,20,240));
            }
        }
    }
    for (int y = 0; y < 6; ++y) {
        for (int x = 0; x < 7; ++x) {
            if (heart_pixels[y * 7 + x] == 'X') {
                ImVec2 p_min = { pos.x + x * p_sz, pos.y + y * p_sz };
                ImVec2 p_max = { p_min.x + p_sz, p_min.y + p_sz };
                draw->AddRectFilled(p_min, p_max, color);
            }
        }
    }
}

static ImVec2 CalcFormattedTextSize(ImFont* font, float font_size, const std::string& text) {
    char clean[128];
    int  ci = 0;
    int  len = (int)text.length();
    for (int i = 0; i < len && ci < 127; ++i) {
        if ((unsigned char)text[i] == 0xC2 && i + 2 < len && (unsigned char)text[i + 1] == 0xA7)
            i += 2;
        else if (text[i] == '&' && i + 1 < len)
            i += 1;
        else
            clean[ci++] = text[i];
    }
    clean[ci] = '\0';
    return font->CalcTextSizeA(font_size, FLT_MAX, 0.f, clean);
}

// ============================================================
// Chunk en stack ? sin std::string en hot path
// ============================================================
static void DrawFormattedTextShadow(ImDrawList* draw, ImFont* font, float font_size,
    ImVec2 pos, ImU32 default_col, const std::string& text)
{
    ImVec2 cur_pos = pos;
    ImU32  cur_col = default_col;
    char   chunk[128];
    int    chunk_len = 0;

    auto flush = [&]() {
        if (chunk_len == 0) return;
        chunk[chunk_len] = '\0';
        
        // Forzamos a que las coordenadas de dibujo sean enteros perfectos 
        ImVec2 draw_pos = cur_pos;

        // Single crisp shadow offset for pixel fonts
        draw->AddText(font, font_size, { draw_pos.x + 1.f, draw_pos.y + 1.f },
            IM_COL32(20, 20, 20, 240), chunk);
        draw->AddText(font, font_size, draw_pos, cur_col, chunk);
        cur_pos.x += font->CalcTextSizeA(font_size, FLT_MAX, 0.f, chunk, chunk + chunk_len).x;
        chunk_len = 0;
        };

    const int len = (int)text.length();
    for (int i = 0; i < len; ++i) {
        bool is_cc = false;
        char cc_char = 0;

        if ((unsigned char)text[i] == 0xC2 && i + 2 < len && (unsigned char)text[i + 1] == 0xA7)
        {
            is_cc = true; cc_char = text[i + 2]; i += 2;
        }
        else if (text[i] == '&' && i + 1 < len)
        {
            is_cc = true; cc_char = text[i + 1]; i += 1;
        }

        if (is_cc) {
            flush();
            switch (tolower((unsigned char)cc_char)) {
            case '0': cur_col = IM_COL32(0, 0, 0, 255); break;
            case '1': cur_col = IM_COL32(0, 0, 170, 255); break;
            case '2': cur_col = IM_COL32(0, 170, 0, 255); break;
            case '3': cur_col = IM_COL32(0, 170, 170, 255); break;
            case '4': cur_col = IM_COL32(170, 0, 0, 255); break;
            case '5': cur_col = IM_COL32(170, 0, 170, 255); break;
            case '6': cur_col = IM_COL32(255, 170, 0, 255); break;
            case '7': cur_col = IM_COL32(170, 170, 170, 255); break;
            case '8': cur_col = IM_COL32(85, 85, 85, 255); break;
            case '9': cur_col = IM_COL32(85, 85, 255, 255); break;
            case 'a': cur_col = IM_COL32(85, 255, 85, 255); break;
            case 'b': cur_col = IM_COL32(85, 255, 255, 255); break;
            case 'c': cur_col = IM_COL32(255, 85, 85, 255); break;
            case 'd': cur_col = IM_COL32(255, 85, 255, 255); break;
            case 'e': cur_col = IM_COL32(255, 255, 85, 255); break;
            case 'f': cur_col = IM_COL32(255, 255, 255, 255); break;
            case 'r': cur_col = default_col;                  break;
            }
        }
        else if (chunk_len < 127) {
            chunk[chunk_len++] = text[i];
        }
    }
    flush();
}

auto features::visual::nametags::render() -> void
{
    std::lock_guard<std::recursive_mutex> resource_lock(minecraft_render_mutex);
    static bool previous_gui_frame = false;
    minecraft_text_commands.clear();
    minecraft_item_commands.clear();
    if (!features::visual::nametags::enabled) return;
    const bool gui_frame = g_PlayerInGui.load(std::memory_order_acquire);
    if (gui_frame) {
        previous_gui_frame = true;
        return;
    }
    // Resource-pack reloads finish while returning from a GUI. Cached icon
    // textures belong to the old atlas, so rebuild them on the first world frame.
    if (previous_gui_frame) {
        abandon_render_resources();
        previous_gui_frame = false;
    }

    if (features::visual::view_port[2] == 0) {
        features::visual::view_port[0] = 0;
        features::visual::view_port[1] = 0;
        features::visual::view_port[2] = static_cast<int>(ImGui::GetIO().DisplaySize.x);
        features::visual::view_port[3] = static_cast<int>(ImGui::GetIO().DisplaySize.y);
    }
    if (features::visual::view_port[2] == 0) return;

    std::shared_ptr<const std::vector<Nametag_Entry>> local_buf;
    double rm_x, rm_y, rm_z;
    float buffer_partial = 0.0f;
    int snapshot_tick = -1;
    {
        std::lock_guard<std::mutex> lock(g_nametag_mutex);
        local_buf = g_nametag_snapshot;
        if (!local_buf || local_buf->empty()) return;
        rm_x = g_cam_x;
        rm_y = g_cam_y;
        rm_z = g_cam_z;
        buffer_partial = g_buffer_partial_ticks;
        snapshot_tick = g_buffer_world_tick;
    }
    float frame_partial = buffer_partial;
    if (features::visual::render_frame_snapshot_valid) {
        rm_x = features::visual::render_camera_x;
        rm_y = features::visual::render_camera_y;
        rm_z = features::visual::render_camera_z;
        frame_partial = (std::clamp)(
            features::visual::render_partial_ticks, 0.0f, 1.0f);
        if (features::visual::render_world_tick >= 0 && snapshot_tick >= 0)
            frame_partial += static_cast<float>(features::visual::render_world_tick - snapshot_tick);
    }
    // Keep entity interpolation on the same world tick as the camera. Permit
    // one tick of bounded extrapolation while the async snapshot catches up;
    // freezing at its endpoint creates a visible step on jumps.
    frame_partial = (std::clamp)(frame_partial, 0.0f, 2.0f);

    auto* draw = ImGui::GetBackgroundDrawList();
    if (!draw) return;

    ImFont* font = ImGui::GetFont();
    ImGuiIO& io_ref = ImGui::GetIO();
    if ((!font || !font->IsLoaded()) && io_ref.Fonts && io_ref.Fonts->Fonts.Size > 0) {
        if (io_ref.Fonts->Fonts[0] && io_ref.Fonts->Fonts[0]->IsLoaded())
            font = io_ref.Fonts->Fonts[0];
    }
    if (!font) return; 
    const bool use_minecraft_font = minecraft_font_renderer &&
        minecraft_font_width_method && minecraft_font_draw_method && current_thread_env();
    const float configured_scale = nametag_scale_factor();
    const ImU32  bg_normal = ImGui::ColorConvertFloat4ToU32({ 0.08f, 0.08f, 0.08f, 0.45f });
    const ImU32  dist_color = ImGui::ColorConvertFloat4ToU32({ 1.00f, 1.00f, 1.00f, 1.00f });
    const float  vw = (float)features::visual::view_port[2];
    const float  vh = (float)features::visual::view_port[3];
    const bool   show_health = gui_nametags_show_name && features::visual::nametags::draw_health;
    const bool   show_dist = gui_nametags_show_name && features::visual::nametags::draw_distance;
    const bool   use_fake = features::visual::nametags::use_fake_name;

    for (const auto& e : *local_buf)
    {
        double interp_x = e.old_position.x + (e.position.x - e.old_position.x) * frame_partial;
        double interp_y = e.old_position.y + (e.position.y - e.old_position.y) * frame_partial;
        double interp_z = e.old_position.z + (e.position.z - e.old_position.z) * frame_partial;
        if (e.own && features::visual::render_local_player_valid) {
            interp_x = features::visual::render_local_player_x;
            interp_y = features::visual::render_local_player_y;
            interp_z = features::visual::render_local_player_z;
        }
        mapper::__vec3 rel_pos;
        rel_pos.x = (float)(interp_x - rm_x);
        rel_pos.y = (float)(interp_y + e.height_offset - rm_y);
        rel_pos.z = (float)(interp_z - rm_z);

        // A first-person own tag sits inside the camera. Never project that
        // near-zero vector even if a client's third-person field mapping is stale.
        const double camera_distance_sq = rel_pos.x * rel_pos.x + rel_pos.y * rel_pos.y + rel_pos.z * rel_pos.z;
        if (e.own && camera_distance_sq < 0.36) continue;
        // Projection tracks the entity; screen-space sizing deliberately ignores zoom/FOV.
        const float current_scale = configured_scale; // Screen-space size is independent of zoom/FOV and toggle state.
        const float text_render_scale = default_minecraft_font_scale * current_scale;
        const float base_font_size = use_minecraft_font
            ? 9.0f * text_render_scale
            : 16.0f * current_scale;

        auto screen = project_nametag(rel_pos, features::visual::model_view_matrix,
            features::visual::projection_matrix, features::visual::view_port);
        if (screen.x == FLT_MAX || screen.y == FLT_MAX) continue;

        if (screen.x < -150.f || screen.x > vw + 150.f)  continue;
        if (screen.y < -150.f || screen.y > vh + 150.f)  continue;
        const float font_size = base_font_size;

        std::string display_name;
        if (use_fake)
            display_name = features::visual::nametags::fake_name.empty() ? "Nick" : features::visual::nametags::fake_name;
        else
            display_name = e.formatted_name;

        // Keep the overlay anchor continuous. Rounding the complete nametag made
        // slow camera/player motion alternate between adjacent pixels and looked
        // like vibration. Only the bitmap-font command is snapped in its queue.
        ImVec2 target_screen = { screen.x, screen.y };

        ImVec2 name_size;
        const float native_name_width = use_minecraft_font ? minecraft_text_width(display_name, text_render_scale) : -1.0f;
        if (native_name_width >= 0.0f) name_size = {native_name_width, base_font_size};
        else if (use_fake) name_size = font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, display_name.c_str());
        else name_size = CalcFormattedTextSize(font, font_size, display_name);

        char hp_num_str[32] = {};
        char hp_bracket_str[8] = {};
        ImVec2 hp_num_size = {};
        ImVec2 hp_bracket_size = {};
        const bool hearts_format = gui_nametags_health_format == 1;
        const bool bar_format = gui_nametags_health_format == 2;
        const float heart_scale = 0.72f * current_scale;
        float heart_width = hearts_format ? 18.0f * current_scale : 0.0f;
        ImU32 heart_color_u32 = IM_COL32(255, 255, 255, 255);
        
        if (show_health && !use_fake && !bar_format) {
            char hp_color = 'a';
            if (e.health > 16.0f)      { hp_color = 'a'; heart_color_u32 = IM_COL32(85, 255, 85, 255); }
            else if (e.health > 12.0f) { hp_color = 'e'; heart_color_u32 = IM_COL32(255, 255, 85, 255); }
            else if (e.health > 8.0f)  { hp_color = '6'; heart_color_u32 = IM_COL32(255, 170, 0, 255); }
            else if (e.health > 4.0f)  { hp_color = 'c'; heart_color_u32 = IM_COL32(255, 85, 85, 255); }
            else                       { hp_color = '4'; heart_color_u32 = IM_COL32(170, 0, 0, 255); }

            const float shown_health = hearts_format ? e.health * 0.5f : e.health;
            const char* fmt = hearts_format ? " &%c[&%c%.1f " : " &%c[&%c%.0f";
            snprintf(hp_num_str, sizeof(hp_num_str), fmt, hp_color, hp_color, shown_health);
            snprintf(hp_bracket_str, sizeof(hp_bracket_str), "&%c]", hp_color);
            if (use_minecraft_font) {
                hp_num_size = {minecraft_text_width(hp_num_str, text_render_scale), base_font_size};
                hp_bracket_size = {minecraft_text_width(hp_bracket_str, text_render_scale), base_font_size};
            } else {
                hp_num_size = CalcFormattedTextSize(font, font_size, hp_num_str);
                hp_bracket_size = CalcFormattedTextSize(font, font_size, hp_bracket_str);
            }
        }

        char dist_str[32] = {};
        ImVec2 dist_size = {};
        if (show_dist) {
            snprintf(dist_str, sizeof(dist_str), " &8[&7%dm&8]", (int)e.player_distance);
            if (use_minecraft_font)
                dist_size = {minecraft_text_width(dist_str, text_render_scale), base_font_size};
            else
                dist_size = CalcFormattedTextSize(font, font_size, dist_str);
        }

        float total_hp_width = show_health && !use_fake && !bar_format ? (hp_num_size.x + heart_width + hp_bracket_size.x) : 0.0f;
        const float total_width = total_hp_width + name_size.x + dist_size.x;
        
        const float start_x = target_screen.x - total_width * 0.5f;
        const float ny = target_screen.y - name_size.y * 0.8f;
        
        const float name_x = start_x;
        const float hp_x = name_x + name_size.x;
        const float dist_x = hp_x + total_hp_width;
        
        const float box_left = start_x;
        const float box_right = start_x + total_width;

        float current_pad_x = 2.0f * current_scale;
        float current_pad_y = 0.5f * current_scale;
        ImU32 current_bg = bg_normal;

        if (gui_nametags_show_name && features::visual::nametags::background) {
            ImVec2 rect_min = { box_left - current_pad_x, ny - current_pad_y };
            ImVec2 rect_max = { box_right + current_pad_x, ny + name_size.y + current_pad_y + 2.0f };

            draw->AddRectFilled(rect_min, rect_max, current_bg, 0.0f);
        }

        if (show_health && !use_fake && !bar_format) {
            if (use_minecraft_font) queue_minecraft_text(hp_num_str, hp_x, ny, dist_color, text_render_scale);
            else DrawFormattedTextShadow(draw, font, font_size, { hp_x, ny }, dist_color, hp_num_str);
            
            if (hearts_format) {
                // Keep the original pixel-heart artwork, aligned to the text
                // line instead of using the old 32 px-font baseline.
                DrawPixelHeart(draw,
                    { std::round(hp_x + hp_num_size.x + 2.0f), std::round(ny + 1.0f) },
                    heart_scale, heart_color_u32);
            }
            
            if (use_minecraft_font) queue_minecraft_text(hp_bracket_str, hp_x + hp_num_size.x + heart_width, ny, dist_color, text_render_scale);
            else DrawFormattedTextShadow(draw, font, font_size, { hp_x + hp_num_size.x + heart_width, ny }, dist_color, hp_bracket_str);
        }

        ImU32 name_col = ImGui::ColorConvertFloat4ToU32({ (float)e.color.x, (float)e.color.y, (float)e.color.z, (float)e.color.w });

        if (gui_nametags_show_name) {
            if (use_minecraft_font)
                queue_minecraft_text(display_name, name_x, ny, name_col, text_render_scale);
            else if (use_fake)
                DrawShadowText(draw, font, font_size, { name_x, ny }, name_col, display_name.c_str());
            else
                DrawFormattedTextShadow(draw, font, font_size, { name_x, ny }, name_col, display_name);
        }

        if (show_dist && dist_size.x > 0.f) {
            if (use_minecraft_font) queue_minecraft_text(dist_str, dist_x, ny, dist_color, text_render_scale);
            else DrawFormattedTextShadow(draw, font, font_size, { dist_x, ny }, dist_color, dist_str);
        }

        if (show_health && !use_fake && bar_format) {
            const int segments = (std::max)(1, (std::min)(20, (int)std::round(gui_nametags_health_segments)));
            const float bar_y = ny - 5.0f * current_scale;
            const float bar_h = (std::max)(2.0f, 3.0f * current_scale);
            const float ratio = (std::max)(0.0f, (std::min)(1.0f, e.health / 20.0f));
            draw->AddRectFilled({box_left, bar_y}, {box_right, bar_y + bar_h}, IM_COL32(20, 20, 23, 230), 1.0f);
            draw->AddRectFilled({box_left, bar_y}, {box_left + (box_right - box_left) * ratio, bar_y + bar_h},
                IM_COL32((int)((1.f - ratio) * 255), (int)(ratio * 255), 45, 255), 1.0f);
            for (int seg = 1; seg < segments; ++seg) {
                float sx = box_left + (box_right - box_left) * ((float)seg / segments);
                draw->AddLine({sx, bar_y}, {sx, bar_y + bar_h}, IM_COL32(8, 8, 10, 210), 1.0f);
            }
        }

        if (gui_nametags_show_equipment && e.equipment && !e.equipment->items.empty()) {
            // Equipment is intentionally one visual step larger than the tag's
            // base scale. Keeping it proportional preserves Distance Scaling without
            // making nearby icons jump between integer sizes.
            // Equipment, enchantments, durability and stack count all derive
            // from the same slider/Distance Scaling value as the name.
            const float equipment_scale = current_scale * 1.50f;
            const float icon_size = 16.0f * equipment_scale;
            const float row_width = icon_size * static_cast<float>(e.equipment->items.size());
            const float row_x = target_screen.x - row_width * 0.5f;
            const float row_y = ny - icon_size - 4.0f * equipment_scale;
            for (size_t item_index = 0; item_index < e.equipment->items.size(); ++item_index) {
                const float item_x = row_x + icon_size * static_cast<float>(item_index);
                const EquipmentItem& item = e.equipment->items[item_index];
                const auto cached = minecraft_item_icon_cache.find(equipment_cache_key(item));
                if (cached != minecraft_item_icon_cache.end() && cached->second.ready) {
                    draw->AddImage(reinterpret_cast<ImTextureID>(static_cast<intptr_t>(cached->second.texture)),
                        {item_x, row_y}, {item_x + icon_size, row_y + icon_size},
                        {0.0f, 1.0f}, {1.0f, 0.0f});
                } else {
                    minecraft_item_commands.push_back(
                        {e.equipment, item_index, item_x, row_y, equipment_scale, item.durability});
                }
                if (item.durability >= 0.0f) {
                    const float ratio = (std::clamp)(item.durability, 0.0f, 1.0f);
                    const float left = item_x + 2.0f * equipment_scale;
                    const float top = row_y + 13.0f * equipment_scale;
                    const float full_width = 13.0f * equipment_scale;
                    draw->AddRectFilled({left, top},
                        {left + full_width, top + 2.0f * equipment_scale}, IM_COL32(0, 0, 0, 255));
                    const int worn = static_cast<int>((1.0f - ratio) * 255.0f);
                    draw->AddRectFilled({left, top},
                        {left + 12.0f * equipment_scale, top + equipment_scale},
                        IM_COL32((255 - worn) / 4, 64, 0, 255));
                    draw->AddRectFilled({left, top},
                        {left + full_width * ratio, top + equipment_scale},
                        IM_COL32(static_cast<int>((1.0f - ratio) * 255.0f),
                            static_cast<int>(ratio * 255.0f), 0, 255));
                }
                if (gui_nametags_show_enchantments && !item.enchantments.empty()) {
                    const float enchant_scale = (std::max)(0.72f, text_render_scale * 0.58f);
                    float enchant_y = row_y + icon_size - 7.0f * equipment_scale;
                    for (const auto& enchantment : item.enchantments) {
                        const float width = minecraft_text_width(enchantment, enchant_scale);
                        const float fitted_scale = width > 0.0f
                            ? enchant_scale * (std::min)(1.0f, (icon_size - 2.0f * equipment_scale) / width)
                            : enchant_scale;
                        if (use_minecraft_font) {
                            queue_minecraft_text(enchantment, item_x, enchant_y,
                                IM_COL32(255, 255, 255, 255), fitted_scale);
                        } else {
                            DrawFormattedTextShadow(draw, font, font_size * 0.52f,
                                {item_x, enchant_y}, IM_COL32(255, 255, 255, 255), enchantment);
                        }
                        enchant_y -= 9.0f * enchant_scale;
                    }
                }
                if (item.count > 1) {
                    const std::string count_text = std::to_string(item.count);
                    const float count_scale = equipment_scale;
                    const float count_width = (std::max)(0.0f, minecraft_text_width(count_text, count_scale));
                    if (use_minecraft_font) {
                        queue_minecraft_text(count_text,
                            item_x + icon_size - count_width,
                            row_y + icon_size - 9.0f * count_scale,
                            IM_COL32(255, 255, 255, 255), count_scale);
                    } else {
                        const float count_font_size = 9.f * count_scale;
                        const auto count_size = font->CalcTextSizeA(count_font_size, FLT_MAX, 0.0f, count_text.c_str());
                        DrawFormattedTextShadow(draw, font, count_font_size,
                            {item_x + icon_size - count_size.x, row_y + icon_size - count_size.y},
                            IM_COL32(255, 255, 255, 255), count_text);
                    }
                }
            }
        }


    }
}

bool features::visual::nametags::has_pending_minecraft_render()
{
    std::lock_guard<std::recursive_mutex> lock(minecraft_render_mutex);
    return !minecraft_text_commands.empty() || !minecraft_item_commands.empty();
}

void features::visual::nametags::render_minecraft_font()
{
    std::lock_guard<std::recursive_mutex> resource_lock(minecraft_render_mutex);
    JNIEnv* env = current_thread_env();
	if (minecraft_text_commands.empty() && minecraft_item_commands.empty()) return;
	if (!env) {
        minecraft_text_commands.clear();
        minecraft_item_commands.clear();
        return;
    }

    const ImVec2 display = ImGui::GetIO().DisplaySize;
    using UseProgram = void (APIENTRY*)(GLuint);
    const auto use_program = reinterpret_cast<UseProgram>(safe_gl_proc("glUseProgram"));
    GLint previous_program = 0;
    if (use_program) {
        glGetIntegerv(0x8B8D, &previous_program); // GL_CURRENT_PROGRAM
        use_program(0);
    }
	GLint previous_texture = 0;
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous_texture);
    const bool previous_depth = glIsEnabled(GL_DEPTH_TEST) == GL_TRUE;
    const bool previous_blend = glIsEnabled(GL_BLEND) == GL_TRUE;
    const bool previous_alpha = glIsEnabled(GL_ALPHA_TEST) == GL_TRUE;
    const bool previous_texture_2d = glIsEnabled(GL_TEXTURE_2D) == GL_TRUE;
    const bool previous_lighting = glIsEnabled(GL_LIGHTING) == GL_TRUE;
    const bool previous_rescale = glIsEnabled(0x803A) == GL_TRUE;
    const bool previous_cull = glIsEnabled(GL_CULL_FACE) == GL_TRUE;
    const bool previous_fog = glIsEnabled(GL_FOG) == GL_TRUE;
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glDisable(GL_SCISSOR_TEST);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0.0, display.x, display.y, 0.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Match the reference client's legacy path: capture one unique item into
    // an isolated framebuffer, then draw only the cached texture on nametags.
    // RenderItem is never invoked directly on the game framebuffer.
    if (minecraft_item_renderer && minecraft_item_draw_method && !minecraft_item_commands.empty()) {
        const auto gl_gen_framebuffers = reinterpret_cast<GlGenFramebuffers>(safe_gl_proc("glGenFramebuffers"));
        const auto gl_bind_framebuffer = reinterpret_cast<GlBindFramebuffer>(safe_gl_proc("glBindFramebuffer"));
        const auto gl_delete_framebuffers = reinterpret_cast<GlDeleteFramebuffers>(safe_gl_proc("glDeleteFramebuffers"));
        const auto gl_framebuffer_texture = reinterpret_cast<GlFramebufferTexture2D>(safe_gl_proc("glFramebufferTexture2D"));
        const auto gl_check_framebuffer = reinterpret_cast<GlCheckFramebufferStatus>(safe_gl_proc("glCheckFramebufferStatus"));
        const auto gl_gen_renderbuffers = reinterpret_cast<GlGenRenderbuffers>(safe_gl_proc("glGenRenderbuffers"));
        const auto gl_bind_renderbuffer = reinterpret_cast<GlBindRenderbuffer>(safe_gl_proc("glBindRenderbuffer"));
        const auto gl_renderbuffer_storage = reinterpret_cast<GlRenderbufferStorage>(safe_gl_proc("glRenderbufferStorage"));
        const auto gl_framebuffer_renderbuffer = reinterpret_cast<GlFramebufferRenderbuffer>(safe_gl_proc("glFramebufferRenderbuffer"));
        const auto gl_delete_renderbuffers = reinterpret_cast<GlDeleteRenderbuffers>(safe_gl_proc("glDeleteRenderbuffers"));
        const bool framebuffer_api = gl_gen_framebuffers && gl_bind_framebuffer && gl_delete_framebuffers &&
            gl_framebuffer_texture && gl_check_framebuffer && gl_gen_renderbuffers && gl_bind_renderbuffer &&
            gl_renderbuffer_storage && gl_framebuffer_renderbuffer && gl_delete_renderbuffers;

        for (const auto& command : minecraft_item_commands) {
            if (!framebuffer_api || !command.equipment || command.item_index >= command.equipment->items.size()) break;
            const EquipmentItem& item = command.equipment->items[command.item_index];
            if (!item.stack || item.item_id < 0) continue;
            const unsigned long long key = equipment_cache_key(item);
            if (minecraft_item_icon_cache.contains(key)) continue;

            ItemIconCacheEntry icon;
            GLint previous_framebuffer = 0, previous_viewport[4] = {}, previous_matrix_mode = GL_MODELVIEW;
            GLfloat previous_clear[4] = {};
            glGetIntegerv(gl_framebuffer_binding, &previous_framebuffer);
            glGetIntegerv(GL_VIEWPORT, previous_viewport);
            glGetIntegerv(GL_MATRIX_MODE, &previous_matrix_mode);
            glGetFloatv(GL_COLOR_CLEAR_VALUE, previous_clear);

            glGenTextures(1, &icon.texture);
            if (minecraft_bind_texture) env->CallStaticVoidMethod(minecraft_gl_state_class, minecraft_bind_texture, icon.texture);
            else glBindTexture(GL_TEXTURE_2D, icon.texture);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 32, 32, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            gl_gen_framebuffers(1, &icon.framebuffer);
            gl_bind_framebuffer(gl_framebuffer, icon.framebuffer);
            gl_framebuffer_texture(gl_framebuffer, gl_color_attachment0, GL_TEXTURE_2D, icon.texture, 0);
            gl_gen_renderbuffers(1, &icon.depth);
            gl_bind_renderbuffer(gl_renderbuffer, icon.depth);
            gl_renderbuffer_storage(gl_renderbuffer, gl_depth_component24, 32, 32);
            gl_framebuffer_renderbuffer(gl_framebuffer, gl_depth_attachment, gl_renderbuffer, icon.depth);

            if (gl_check_framebuffer(gl_framebuffer) == gl_framebuffer_complete) {
                glViewport(0, 0, 32, 32);
                glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                glMatrixMode(GL_PROJECTION);
                glPushMatrix();
                glLoadIdentity();
                glOrtho(0.0, 16.0, 16.0, 0.0, -1000.0, 3000.0);
                glMatrixMode(GL_MODELVIEW);
                glPushMatrix();
                glLoadIdentity();
                glTranslatef(0.0f, 0.0f, -2000.0f);
                glEnable(GL_DEPTH_TEST);
                glDepthMask(GL_TRUE);
                if (minecraft_enable_item_lighting)
                    env->CallStaticVoidMethod(minecraft_render_helper_class, minecraft_enable_item_lighting);
                if (minecraft_enable_rescale_normal)
                    env->CallStaticVoidMethod(minecraft_gl_state_class, minecraft_enable_rescale_normal);
                env->CallVoidMethod(minecraft_item_renderer, minecraft_item_draw_method, item.stack, 0, 0);
                if (env->ExceptionCheck()) env->ExceptionClear();
                if (minecraft_disable_item_lighting)
                    env->CallStaticVoidMethod(minecraft_render_helper_class, minecraft_disable_item_lighting);
                if (minecraft_disable_rescale_normal)
                    env->CallStaticVoidMethod(minecraft_gl_state_class, minecraft_disable_rescale_normal);
                if (minecraft_disable_lighting)
                    env->CallStaticVoidMethod(minecraft_gl_state_class, minecraft_disable_lighting);
                if (minecraft_color)
                    env->CallStaticVoidMethod(minecraft_gl_state_class, minecraft_color, 1.0f, 1.0f, 1.0f, 1.0f);
                glMatrixMode(GL_MODELVIEW);
                glPopMatrix();
                glMatrixMode(GL_PROJECTION);
                glPopMatrix();
                icon.ready = true;
            }

            gl_bind_framebuffer(gl_framebuffer, static_cast<GLuint>(previous_framebuffer));
            glViewport(previous_viewport[0], previous_viewport[1], previous_viewport[2], previous_viewport[3]);
            glClearColor(previous_clear[0], previous_clear[1], previous_clear[2], previous_clear[3]);
            glMatrixMode(previous_matrix_mode);
            if (!icon.ready) {
                if (icon.depth) gl_delete_renderbuffers(1, &icon.depth);
                if (icon.framebuffer) gl_delete_framebuffers(1, &icon.framebuffer);
                if (icon.texture) glDeleteTextures(1, &icon.texture);
            } else {
                minecraft_item_icon_cache.emplace(key, icon);
            }
            // Spread captures over frames to avoid a visible hitch when many
            // players enter view simultaneously.
            break;
        }
    }

	if (minecraft_font_renderer && minecraft_font_draw_method) for (const auto& command : minecraft_text_commands) {
        jstring text = env->NewStringUTF(command.text.c_str());
        if (!text) continue;
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glTranslatef(command.x, command.y, 0.0f);
        glScalef(command.scale, command.scale, 1.0f);
        env->CallIntMethod(minecraft_font_renderer, minecraft_font_draw_method, text, 0.0f, 0.0f, command.color);
        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();
        env->DeleteLocalRef(text);
        if (env->ExceptionCheck()) env->ExceptionClear();

    }
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopAttrib();
    if (minecraft_gl_state_class) {
        auto synchronize_state = [&](bool enabled, jmethodID enable_method, jmethodID disable_method) {
            if (!enable_method || !disable_method) return;
            // Drive the cached state through both values so it cannot retain a
            // stale RenderItem value after glPopAttrib restores the real state.
            env->CallStaticVoidMethod(minecraft_gl_state_class,
                enabled ? disable_method : enable_method);
            env->CallStaticVoidMethod(minecraft_gl_state_class,
                enabled ? enable_method : disable_method);
        };
        synchronize_state(previous_depth, minecraft_enable_depth, minecraft_disable_depth);
        synchronize_state(previous_blend, minecraft_enable_blend, minecraft_disable_blend);
        synchronize_state(previous_alpha, minecraft_enable_alpha, minecraft_disable_alpha);
        synchronize_state(previous_texture_2d, minecraft_enable_texture, minecraft_disable_texture);
        synchronize_state(previous_lighting, minecraft_enable_lighting, minecraft_disable_lighting);
        synchronize_state(previous_rescale, minecraft_enable_rescale_normal, minecraft_disable_rescale_normal);
        synchronize_state(previous_cull, minecraft_enable_cull, minecraft_disable_cull);
        synchronize_state(previous_fog, minecraft_enable_fog, minecraft_disable_fog);
        if (minecraft_color) {
            env->CallStaticVoidMethod(minecraft_gl_state_class, minecraft_color, 0.99f, 0.99f, 0.99f, 0.99f);
            env->CallStaticVoidMethod(minecraft_gl_state_class, minecraft_color, 1.0f, 1.0f, 1.0f, 1.0f);
        }
    }
    if (minecraft_gl_state_class && minecraft_bind_texture) {
        env->CallStaticVoidMethod(minecraft_gl_state_class, minecraft_bind_texture, -1);
        env->CallStaticVoidMethod(minecraft_gl_state_class, minecraft_bind_texture, previous_texture);
    }
    else
	    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previous_texture));
    if (env->ExceptionCheck()) env->ExceptionClear();
    minecraft_text_commands.clear();
    minecraft_item_commands.clear();
    if (use_program) use_program(static_cast<GLuint>(previous_program));
}
namespace features::visual::nametags {
void run_on_game_frame() {
    JNIEnv* env = nullptr;
    if (!sdk::jvm || sdk::jvm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK ||
        !env || env->ExceptionCheck()) return;
    struct EnvScope {
        JNIEnv* previous;
        explicit EnvScope(JNIEnv* current) : previous(sdk::jni) { sdk::jni = current; }
        ~EnvScope() { sdk::jni = previous; }
    } scope(env);
    mapper::__minecraft minecraft;
    if (minecraft.object) run(minecraft);
    else clear_nametag_snapshot();
    if (env->ExceptionCheck()) { env->ExceptionClear(); clear_nametag_snapshot(); }
}
}
