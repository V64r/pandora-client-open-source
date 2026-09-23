#include "mapper.hpp"
#include <windows.h>
#include <GL/gl.h>
#include <jvmti.h>

namespace mapper
{
    extern jvmtiEnv* jvmti; // Traemos la herramienta profunda desde mapper.cpp
}

mapper::__minecraft::__minecraft()
{
    mapper::__field field = mapper::classes["Minecraft"].get_field("theMinecraft", mapper::classes["Minecraft"].signature);

    if (field.identifier == nullptr)
        field = mapper::classes["Minecraft"].get_field("field_71432_P", mapper::classes["Minecraft"].signature);

    if (field.identifier == nullptr)
        field = mapper::classes["Minecraft"].get_field("S", mapper::classes["Minecraft"].signature); // 1.8.9 Lunar/Vanilla

    if (field.identifier == nullptr)
        field = mapper::classes["Minecraft"].get_field("M", mapper::classes["Minecraft"].signature); // 1.7.10 Lunar/Vanilla

    if (field.identifier == nullptr) { return; }
    this->object = sdk::jni->GetStaticObjectField(mapper::classes["Minecraft"].klass, field.identifier);
}

mapper::__minecraft::__minecraft(const mapper::__minecraft& minecraft)
{
    if (minecraft.object != nullptr)
        this->object = sdk::jni->NewLocalRef(minecraft.object);
}

mapper::__minecraft::~__minecraft()
{
    if (this->object != nullptr)
        sdk::jni->DeleteLocalRef(this->object);
}

// --- HOOKS DE RENDERIZADO AVANZADOS ---

bool mapper::__minecraft::is_on_run_tick()
{
    bool is_on_run_tick = false;
    GLint matrix_mode = 0;

    static auto glGetIntegerv_ptr = (void(__stdcall*)(GLenum, GLint*))GetProcAddress(GetModuleHandleA("opengl32.dll"), "glGetIntegerv");
    if (!glGetIntegerv_ptr) return false;

    glGetIntegerv_ptr(0xBA0, &matrix_mode);

    if (matrix_mode == 0x1700) // GL_MODELVIEW
    {
        jthread current_thread = nullptr;
        mapper::jvmti->GetCurrentThread(&current_thread);

        jvmtiFrameInfo frame_information[1];
        jint frame_count = 0;

        mapper::jvmti->GetStackTrace(current_thread, 4, 1, frame_information, &frame_count);

        for (int x = 0; x < frame_count && !is_on_run_tick; ++x)
        {
            char* name = nullptr; char* signature = nullptr; char* reserved = nullptr;
            mapper::jvmti->GetMethodName(frame_information[x].method, &name, &signature, &reserved);

            if (name != nullptr && signature != nullptr)
            {
                if (std::string(signature) == "()V")
                {
                    std::string func_name(name);
                    if (func_name == "runTick" || func_name == "func_71407_l" || func_name == "s" || func_name == "p")
                        is_on_run_tick = true;
                }
                mapper::jvmti->Deallocate((unsigned char*)name);
            }
            if (signature != nullptr) mapper::jvmti->Deallocate((unsigned char*)signature);
            if (reserved != nullptr) mapper::jvmti->Deallocate((unsigned char*)reserved);
        }
        sdk::jni->DeleteLocalRef(current_thread);
    }
    return is_on_run_tick;
}

bool mapper::__minecraft::is_on_render_world()
{
    bool is_on_render_world = false;
    GLint matrix_mode = 0;

    static auto glGetIntegerv_ptr = (void(__stdcall*)(GLenum, GLint*))GetProcAddress(GetModuleHandleA("opengl32.dll"), "glGetIntegerv");
    if (!glGetIntegerv_ptr) return false;

    glGetIntegerv_ptr(0xBA0, &matrix_mode);

    if (matrix_mode == 0x1700)
    {
        jthread current_thread = nullptr;
        mapper::jvmti->GetCurrentThread(&current_thread);

        jvmtiFrameInfo frame_information[3];
        jint frame_count = 0;

        mapper::jvmti->GetStackTrace(current_thread, 2, 3, frame_information, &frame_count);

        for (int x = 0; x < frame_count && !is_on_render_world; ++x)
        {
            char* name = nullptr; char* signature = nullptr; char* reserved = nullptr;
            mapper::jvmti->GetMethodName(frame_information[x].method, &name, &signature, &reserved);

            if (name != nullptr && signature != nullptr)
            {
                // 1.8 uses (FJ)V, 1.7 uses (F)V
                std::string sig(signature);
                if (sig == "(FJ)V" || sig == "(F)V")
                {
                    std::string func_name(name);
                    if (func_name == "renderWorld" || func_name == "func_78471_a" || func_name == "b" || func_name == "a")
                        is_on_render_world = true;
                }
                mapper::jvmti->Deallocate((unsigned char*)name);
            }
            if (signature != nullptr) mapper::jvmti->Deallocate((unsigned char*)signature);
            if (reserved != nullptr) mapper::jvmti->Deallocate((unsigned char*)reserved);
        }
        sdk::jni->DeleteLocalRef(current_thread);
    }
    return is_on_render_world;
}

bool mapper::__minecraft::is_on_orient_camera()
{
    bool is_on_orient_camera = false;
    GLint matrix_mode = 0;

    static auto glGetIntegerv_ptr = (void(__stdcall*)(GLenum, GLint*))GetProcAddress(GetModuleHandleA("opengl32.dll"), "glGetIntegerv");
    if (!glGetIntegerv_ptr) return false;

    glGetIntegerv_ptr(0xBA0, &matrix_mode);

    if (matrix_mode == 0x1700)
    {
        jthread current_thread = nullptr;
        mapper::jvmti->GetCurrentThread(&current_thread);

        jvmtiFrameInfo frame_information[1];
        jint frame_count = 0;

        mapper::jvmti->GetStackTrace(current_thread, 3, 1, frame_information, &frame_count);

        for (int x = 0; x < frame_count && !is_on_orient_camera; ++x)
        {
            char* name = nullptr; char* signature = nullptr; char* reserved = nullptr;
            mapper::jvmti->GetMethodName(frame_information[x].method, &name, &signature, &reserved);

            if (name != nullptr && signature != nullptr)
            {
                if (std::string(signature) == "(F)V")
                {
                    std::string func_name(name);
                    if (func_name == "orientCamera" || func_name == "func_78467_g" || func_name == "f" || func_name == "h")
                        is_on_orient_camera = true;
                }
                mapper::jvmti->Deallocate((unsigned char*)name);
            }
            if (signature != nullptr) mapper::jvmti->Deallocate((unsigned char*)signature);
            if (reserved != nullptr) mapper::jvmti->Deallocate((unsigned char*)reserved);
        }
        sdk::jni->DeleteLocalRef(current_thread);
    }
    return is_on_orient_camera;
}

// --- GETTERS DEL JUEGO ---

mapper::__settings mapper::__minecraft::get_settings()
{
    if (this->object == nullptr) return mapper::__settings(nullptr);

    std::string sig = "L" + mapper::classes["GameSettings"].name + ";";

    mapper::__field field = mapper::classes["Minecraft"].get_field("gameSettings", sig);
    if (field.identifier == nullptr) field = mapper::classes["Minecraft"].get_field("field_71474_y", sig);
    if (field.identifier == nullptr) field = mapper::classes["Minecraft"].get_field("t", sig); // Obf 1.8.9
    if (field.identifier == nullptr) field = mapper::classes["Minecraft"].get_field("N", sig); // Obf 1.7.10

    if (field.identifier == nullptr) return mapper::__settings(nullptr);

    jobject settings_obj = sdk::jni->GetObjectField(this->object, field.identifier);
    return mapper::__settings(settings_obj);
}

mapper::__timer mapper::__minecraft::get_timer()
{
    mapper::__field field = mapper::classes["Minecraft"].get_field("timer", mapper::classes["Timer"].signature);
    if (field.identifier == nullptr) field = mapper::classes["Minecraft"].get_field("field_71428_T", mapper::classes["Timer"].signature);
    if (field.identifier == nullptr) field = mapper::classes["Minecraft"].get_field("Y", mapper::classes["Timer"].signature);
    if (field.identifier == nullptr) field = mapper::classes["Minecraft"].get_field("Q", mapper::classes["Timer"].signature); // 1.7.10

    if (field.identifier == nullptr) return mapper::__timer(nullptr);
    return mapper::__timer(sdk::jni->GetObjectField(this->object, field.identifier));
}

mapper::__gui_screen mapper::__minecraft::get_current_screen()
{
    mapper::__field field = mapper::classes["Minecraft"].get_field("currentScreen", mapper::classes["GuiScreen"].signature);
    if (field.identifier == nullptr) field = mapper::classes["Minecraft"].get_field("field_71462_r", mapper::classes["GuiScreen"].signature);
    if (field.identifier == nullptr) field = mapper::classes["Minecraft"].get_field("m", mapper::classes["GuiScreen"].signature);
    if (field.identifier == nullptr) field = mapper::classes["Minecraft"].get_field("n", mapper::classes["GuiScreen"].signature); // 1.7.10

    if (field.identifier == nullptr) return mapper::__gui_screen(nullptr);
    return mapper::__gui_screen(sdk::jni->GetObjectField(this->object, field.identifier));
}

mapper::__render_manager mapper::__minecraft::get_render_manager()
{
    std::string sig = mapper::classes["RenderManager"].signature;

    // 1.8: renderManager es un campo de instancia de Minecraft
    mapper::__field field = mapper::classes["Minecraft"].get_field("renderManager", sig);
    if (field.identifier == nullptr) field = mapper::classes["Minecraft"].get_field("field_175616_W", sig);
    if (field.identifier == nullptr) field = mapper::classes["Minecraft"].get_field("aa", sig);

    if (field.identifier != nullptr) {
        return mapper::__render_manager(sdk::jni->GetObjectField(this->object, field.identifier));
    }

    // 1.7: RenderManager.instance es un campo ESTATICO
    if (mapper::classes["RenderManager"].klass != nullptr) {
        jfieldID static_fid = sdk::jni->GetStaticFieldID(mapper::classes["RenderManager"].klass, "instance", sig.c_str());
        if (sdk::jni->ExceptionCheck()) sdk::jni->ExceptionClear();
        if (!static_fid) {
            static_fid = sdk::jni->GetStaticFieldID(mapper::classes["RenderManager"].klass, "field_78727_a", sig.c_str());
            if (sdk::jni->ExceptionCheck()) sdk::jni->ExceptionClear();
        }
        if (static_fid) {
            return mapper::__render_manager(sdk::jni->GetStaticObjectField(mapper::classes["RenderManager"].klass, static_fid));
        }
    }

    return mapper::__render_manager(nullptr);
}

mapper::__world mapper::__minecraft::get_world()
{
    mapper::__field field = mapper::classes["Minecraft"].get_field("theWorld", mapper::classes["WorldClient"].signature);
    if (field.identifier == nullptr) field = mapper::classes["Minecraft"].get_field("field_71441_e", mapper::classes["WorldClient"].signature);
    if (field.identifier == nullptr) field = mapper::classes["Minecraft"].get_field("f", mapper::classes["WorldClient"].signature);
    if (field.identifier == nullptr) field = mapper::classes["Minecraft"].get_field("g", mapper::classes["WorldClient"].signature); // 1.7 Lunar
    if (field.identifier == nullptr) field = mapper::classes["Minecraft"].get_field("e", mapper::classes["WorldClient"].signature); // 1.7 alt

    if (field.identifier == nullptr) return mapper::__world(nullptr);
    return mapper::__world(sdk::jni->GetObjectField(this->object, field.identifier));
}

mapper::__player mapper::__minecraft::get_local_player()
{
    std::string sig = mapper::classes["EntityPlayerXP"].signature;
    mapper::__field field = mapper::classes["Minecraft"].get_field("thePlayer", sig);
    if (field.identifier == nullptr) field = mapper::classes["Minecraft"].get_field("field_71439_g", sig);
    if (field.identifier == nullptr) field = mapper::classes["Minecraft"].get_field("h", sig);

    if (field.identifier == nullptr && mapper::classes["Minecraft"].klass != nullptr) {
        std::string alt_sig = "Lnet/minecraft/client/entity/EntityClientPlayerMP;";
        jfieldID fid = sdk::jni->GetFieldID(mapper::classes["Minecraft"].klass, "thePlayer", alt_sig.c_str());
        if (sdk::jni->ExceptionCheck()) sdk::jni->ExceptionClear();
        if (fid) field.identifier = fid;
    }

    if (field.identifier == nullptr) return mapper::__player(nullptr);
    return mapper::__player(sdk::jni->GetObjectField(this->object, field.identifier));
}

mapper::__moving_object_position mapper::__minecraft::get_object_mouse_over()
{
    if (this->object == nullptr) return mapper::__moving_object_position(nullptr);

    mapper::__field field = mapper::classes["Minecraft"].get_field("objectMouseOver", mapper::classes["MovingObjectPosition"].signature);
    if (field.identifier == nullptr) field = mapper::classes["Minecraft"].get_field("field_71476_x", mapper::classes["MovingObjectPosition"].signature);
    if (field.identifier == nullptr) field = mapper::classes["Minecraft"].get_field("s", mapper::classes["MovingObjectPosition"].signature);
    if (field.identifier == nullptr) field = mapper::classes["Minecraft"].get_field("t", mapper::classes["MovingObjectPosition"].signature); // 1.7.10

    if (field.identifier == nullptr) return mapper::__moving_object_position(nullptr);
    jobject result = sdk::jni->GetObjectField(this->object, field.identifier);
    if (sdk::jni->ExceptionCheck()) { sdk::jni->ExceptionClear(); return mapper::__moving_object_position(nullptr); }
    return mapper::__moving_object_position(result);
}

mapper::__player mapper::__minecraft::get_pointed_entity()
{
    mapper::__field field = mapper::classes["Minecraft"].get_field("pointedEntity", mapper::classes["Entity"].signature);
    if (field.identifier == nullptr) field = mapper::classes["Minecraft"].get_field("field_147125_j", mapper::classes["Entity"].signature);
    if (field.identifier == nullptr) field = mapper::classes["Minecraft"].get_field("i", mapper::classes["Entity"].signature);

    if (field.identifier == nullptr) return mapper::__player(nullptr);
    return mapper::__player(sdk::jni->GetObjectField(this->object, field.identifier));
}

__int32 mapper::__minecraft::get_right_click_delay_timer()
{
    mapper::__field field = mapper::classes["Minecraft"].get_field("rightClickDelayTimer", "I");
    if (field.identifier == nullptr) field = mapper::classes["Minecraft"].get_field("field_71467_ac", "I");
    if (field.identifier == nullptr) field = mapper::classes["Minecraft"].get_field("ap", "I");
    if (field.identifier == nullptr) field = mapper::classes["Minecraft"].get_field("ah", "I"); // 1.7.10

    if (field.identifier == nullptr) return 0;
    return sdk::jni->GetIntField(this->object, field.identifier);
}

void mapper::__minecraft::set_right_click_delay_timer(__int32 right_click_delay_timer)
{
    mapper::__field field = mapper::classes["Minecraft"].get_field("rightClickDelayTimer", "I");
    if (field.identifier == nullptr) field = mapper::classes["Minecraft"].get_field("field_71467_ac", "I");
    if (field.identifier == nullptr) field = mapper::classes["Minecraft"].get_field("ap", "I");
    if (field.identifier == nullptr) field = mapper::classes["Minecraft"].get_field("ah", "I"); // 1.7.10

    if (field.identifier == nullptr) return;
    sdk::jni->SetIntField(this->object, field.identifier, right_click_delay_timer);
}

bool mapper::__minecraft::window_click(int window_id, int slot_id, int mouse_button, int mode, mapper::__player player)
{
    mapper::__field field = mapper::classes["Minecraft"].get_field("playerController", mapper::classes["PlayerControllerMP"].signature);
    if (field.identifier == nullptr) field = mapper::classes["Minecraft"].get_field("field_71442_b", mapper::classes["PlayerControllerMP"].signature);
    if (field.identifier == nullptr) field = mapper::classes["Minecraft"].get_field("c", mapper::classes["PlayerControllerMP"].signature);
    if (field.identifier == nullptr) field = mapper::classes["Minecraft"].get_field("b", mapper::classes["PlayerControllerMP"].signature); // 1.7.10

    if (field.identifier == nullptr) {
        for (const auto& candidate : mapper::classes["Minecraft"].fields) {
            if (candidate.signature == mapper::classes["PlayerControllerMP"].signature) {
                field = candidate;
                break;
            }
        }
    }
    if (field.identifier == nullptr) return false;

    jobject player_controller = sdk::jni->GetObjectField(this->object, field.identifier);
    if (player_controller == nullptr) return false;

    mapper::__method method = mapper::classes["PlayerControllerMP"].get_method("windowClick", "(IIII" + mapper::classes["EntityPlayer"].signature + ")" + mapper::classes["ItemStack"].signature);
    if (method.identifier == nullptr) method = mapper::classes["PlayerControllerMP"].get_method("func_78753_a", "(IIII" + mapper::classes["EntityPlayer"].signature + ")" + mapper::classes["ItemStack"].signature);
    if (method.identifier == nullptr) method = mapper::classes["PlayerControllerMP"].get_method("a", "(IIII" + mapper::classes["EntityPlayer"].signature + ")" + mapper::classes["ItemStack"].signature);

    if (method.identifier == nullptr) {
        const std::string signature = "(IIII" + mapper::classes["EntityPlayer"].signature + ")" +
            mapper::classes["ItemStack"].signature;
        for (const auto& candidate : mapper::classes["PlayerControllerMP"].methods) {
            if (candidate.signature == signature) {
                method = candidate;
                break;
            }
        }
    }

    bool clicked = false;
    if (method.identifier != nullptr) {
        jobject result = sdk::jni->CallObjectMethod(player_controller, method.identifier, window_id, slot_id, mouse_button, mode, player.object);
        if (!sdk::jni->ExceptionCheck()) clicked = true;
        else sdk::jni->ExceptionClear();
        if (result != nullptr) sdk::jni->DeleteLocalRef(result);
    }
    sdk::jni->DeleteLocalRef(player_controller);
    return clicked;
}

bool mapper::__minecraft::is_valid()
{
    if (this->object == nullptr) return false;
    if (sdk::jni->ExceptionCheck()) sdk::jni->ExceptionClear();

    auto settings = this->get_settings();
    if (sdk::jni->ExceptionCheck()) sdk::jni->ExceptionClear();
    if (settings.object == nullptr) return false;

    auto timer = this->get_timer();
    if (sdk::jni->ExceptionCheck()) sdk::jni->ExceptionClear();
    if (timer.object == nullptr) return false;

    auto rm = this->get_render_manager();
    if (sdk::jni->ExceptionCheck()) sdk::jni->ExceptionClear();
    if (rm.object == nullptr) return false;

    auto world = this->get_world();
    if (sdk::jni->ExceptionCheck()) { sdk::jni->ExceptionClear(); }
    if (world.object == nullptr) return false;

    auto lp = this->get_local_player();
    if (sdk::jni->ExceptionCheck()) { sdk::jni->ExceptionClear(); }
    if (lp.object == nullptr) return false;

    return true;
}
std::string mapper::__minecraft::get_server_ip()
{
    if (this->object == nullptr) return "";
    JNIEnv* env = sdk::jni;
    if (!env) return "";

    jclass mcClass = env->GetObjectClass(this->object);
    if (!mcClass) { if (env->ExceptionCheck()) env->ExceptionClear(); return ""; }

    jfieldID serverDataFid = env->GetFieldID(mcClass, "currentServerData", "Lnet/minecraft/client/multiplayer/ServerData;");
    if (!serverDataFid) { env->ExceptionClear(); serverDataFid = env->GetFieldID(mcClass, "field_71422_O", "Lnet/minecraft/client/multiplayer/ServerData;"); }
    if (!serverDataFid) { env->ExceptionClear(); serverDataFid = env->GetFieldID(mcClass, "aD", "Lnet/minecraft/client/multiplayer/ServerData;"); }

    if (!serverDataFid) { env->DeleteLocalRef(mcClass); return ""; }

    jobject serverData = env->GetObjectField(this->object, serverDataFid);
    env->DeleteLocalRef(mcClass);
    if (!serverData) { if (env->ExceptionCheck()) env->ExceptionClear(); return ""; }

    jclass sdClass = env->GetObjectClass(serverData);
    if (!sdClass) { env->DeleteLocalRef(serverData); if (env->ExceptionCheck()) env->ExceptionClear(); return ""; }

    jfieldID ipFid = env->GetFieldID(sdClass, "serverIP", "Ljava/lang/String;");
    if (!ipFid) { env->ExceptionClear(); ipFid = env->GetFieldID(sdClass, "field_78845_b", "Ljava/lang/String;"); }
    if (!ipFid) { env->ExceptionClear(); ipFid = env->GetFieldID(sdClass, "b", "Ljava/lang/String;"); }

    std::string ip = "";
    if (ipFid) {
        jstring ipStr = (jstring)env->GetObjectField(serverData, ipFid);
        if (ipStr) {
            const char* chars = env->GetStringUTFChars(ipStr, nullptr);
            if (chars) {
                ip = chars;
                env->ReleaseStringUTFChars(ipStr, chars);
            }
            env->DeleteLocalRef(ipStr);
        }
    }

    env->DeleteLocalRef(sdClass);
    env->DeleteLocalRef(serverData);
    if (env->ExceptionCheck()) env->ExceptionClear();

    return ip;
}
