#include <winsock2.h>
#include "hooks.hpp"
#include "../../../sdk.hpp"
#include "../features/features.hpp"
#include <MinHook.h>
#include <gl/GL.h>
#include <iostream>
#include <cmath>
#include <algorithm>

extern std::atomic<bool> g_PlayerInGui;
extern std::atomic<bool> g_MenuVisible;
extern "C" void swiftLog(const char* format, ...);
namespace hooks
{
    static jclass float_buffer_class = nullptr;
    static jmethodID float_buffer_get = nullptr;

    bool capture_render_frame_java_state()
    {
        features::visual::render_local_player_valid = false;
        JNIEnv* env = nullptr;
        if (!sdk::jvm || sdk::jvm->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK || !env)
            return false;
        auto& mc_class = mapper::classes["Minecraft"];
        auto& rm_class = mapper::classes["RenderManager"];
        auto& timer_class = mapper::classes["Timer"];
        mapper::__field mc_f = mc_class.get_field("theMinecraft", mc_class.signature);
        if (!mc_f.identifier) mc_f = mc_class.get_field("field_71432_P", mc_class.signature);
        if (!mc_f.identifier) mc_f = mc_class.get_field("S", mc_class.signature);
        if (!mc_f.identifier) return false;
        jobject mc = env->GetStaticObjectField(mc_class.klass, mc_f.identifier);
        if (!mc) return false;

        mapper::__field rm_f = mc_class.get_field("renderManager", rm_class.signature);
        if (!rm_f.identifier) rm_f = mc_class.get_field("field_175616_W", rm_class.signature);
        if (!rm_f.identifier) rm_f = mc_class.get_field("aa", rm_class.signature);
        mapper::__field timer_f = mc_class.get_field("timer", timer_class.signature);
        if (!timer_f.identifier) timer_f = mc_class.get_field("field_71428_T", timer_class.signature);
        if (!timer_f.identifier) timer_f = mc_class.get_field("Y", timer_class.signature);
        jobject rm = rm_f.identifier ? env->GetObjectField(mc, rm_f.identifier) : nullptr;
        jobject timer = timer_f.identifier ? env->GetObjectField(mc, timer_f.identifier) : nullptr;

        mapper::__field fx = rm_class.get_field("renderPosX", "D");
        if (!fx.identifier) fx = rm_class.get_field("field_78725_b", "D");
        if (!fx.identifier) fx = rm_class.get_field("o", "D");
        mapper::__field fy = rm_class.get_field("renderPosY", "D");
        if (!fy.identifier) fy = rm_class.get_field("field_78726_c", "D");
        if (!fy.identifier) fy = rm_class.get_field("p", "D");
        mapper::__field fz = rm_class.get_field("renderPosZ", "D");
        if (!fz.identifier) fz = rm_class.get_field("field_78723_d", "D");
        if (!fz.identifier) fz = rm_class.get_field("q", "D");
        mapper::__field fp = timer_class.get_field("renderPartialTicks", "F");
        if (!fp.identifier) fp = timer_class.get_field("field_74281_c", "F");
        if (!fp.identifier) fp = timer_class.get_field("c", "F");

        bool snapshot_ok = false;
        const bool fields_ok = rm && timer && fx.identifier && fy.identifier && fz.identifier && fp.identifier;
        if (fields_ok) {
            const double camera_x = env->GetDoubleField(rm, fx.identifier);
            const double camera_y = env->GetDoubleField(rm, fy.identifier);
            const double camera_z = env->GetDoubleField(rm, fz.identifier);
            const float partial_ticks = env->GetFloatField(timer, fp.identifier);
            int world_tick = -1;
            bool local_player_ok = false;
            double local_player_x = 0.0;
            double local_player_y = 0.0;
            double local_player_z = 0.0;
            mapper::__field player_field = mc_class.get_field("thePlayer", mapper::classes["EntityPlayerXP"].signature);
            if (!player_field.identifier) player_field = mc_class.get_field("field_71439_g", mapper::classes["EntityPlayerXP"].signature);
            if (!player_field.identifier) player_field = mc_class.get_field("h", mapper::classes["EntityPlayerXP"].signature);
            jobject player = player_field.identifier ? env->GetObjectField(mc, player_field.identifier) : nullptr;
            if (player) {
                auto& player_class = mapper::classes["EntityPlayerXP"];
                mapper::__field tick_field = mapper::classes["EntityPlayerXP"].get_field("ticksExisted", "I");
                if (!tick_field.identifier) tick_field = mapper::classes["EntityPlayerXP"].get_field("field_70173_aa", "I");
                if (!tick_field.identifier) tick_field = mapper::classes["EntityPlayerXP"].get_field("W", "I");
                if (tick_field.identifier) world_tick = env->GetIntField(player, tick_field.identifier);

                auto position_field = [&](const char* named, const char* srg, const char* obf) {
                    mapper::__field field = player_class.get_field(named, "D");
                    if (!field.identifier) field = player_class.get_field(srg, "D");
                    if (!field.identifier) field = player_class.get_field(obf, "D");
                    return field;
                };
                const mapper::__field px = position_field("posX", "field_70165_t", "s");
                const mapper::__field py = position_field("posY", "field_70163_u", "t");
                const mapper::__field pz = position_field("posZ", "field_70161_v", "u");
                const mapper::__field lx = position_field("lastTickPosX", "field_70142_S", "P");
                const mapper::__field ly = position_field("lastTickPosY", "field_70137_T", "Q");
                const mapper::__field lz = position_field("lastTickPosZ", "field_70136_U", "R");
                if (px.identifier && py.identifier && pz.identifier &&
                    lx.identifier && ly.identifier && lz.identifier) {
                    const double x = env->GetDoubleField(player, px.identifier);
                    const double y = env->GetDoubleField(player, py.identifier);
                    const double z = env->GetDoubleField(player, pz.identifier);
                    const double last_x = env->GetDoubleField(player, lx.identifier);
                    const double last_y = env->GetDoubleField(player, ly.identifier);
                    const double last_z = env->GetDoubleField(player, lz.identifier);
                    const double partial = (std::clamp)(static_cast<double>(partial_ticks), 0.0, 1.0);
                    local_player_x = last_x + (x - last_x) * partial;
                    local_player_y = last_y + (y - last_y) * partial;
                    local_player_z = last_z + (z - last_z) * partial;
                    local_player_ok = true;
                }
                env->DeleteLocalRef(player);
            }
            double model_view[16] = {};
            double projection[16] = {};
            // ActiveRenderInfo is filled immediately after
            // setupCameraTransform. Reading it at SwapBuffers gives the exact
            // matrices used for the entities in this frame; glClear happens
            // before camera setup and therefore contains the previous frame.
            auto& ari = mapper::classes["ActiveRenderInfo"];
            if (!float_buffer_class) {
                jclass local = env->FindClass("java/nio/FloatBuffer");
                if (local) {
                    float_buffer_class = (jclass)env->NewGlobalRef(local);
                    float_buffer_get = env->GetMethodID(float_buffer_class, "get", "(I)F");
                    env->DeleteLocalRef(local);
                }
            }
            auto read_matrix = [&](const char* named, const char* srg,
                                   const char* obf, double* output) -> bool {
                mapper::__field field = ari.get_field(named, "Ljava/nio/FloatBuffer;");
                if (!field.identifier) field = ari.get_field(srg, "Ljava/nio/FloatBuffer;");
                if (!field.identifier) field = ari.get_field(obf, "Ljava/nio/FloatBuffer;");
                if (!field.identifier || !float_buffer_get) return false;
                jobject buffer = env->GetStaticObjectField(ari.klass, field.identifier);
                if (!buffer) return false;
                for (int i = 0; i < 16; ++i)
                    output[i] = (double)env->CallFloatMethod(buffer, float_buffer_get, i);
                env->DeleteLocalRef(buffer);
                return !env->ExceptionCheck();
            };
            const bool matrices_ok = ari.klass &&
                read_matrix("MODELVIEW", "field_78726_a", "a", model_view) &&
                read_matrix("PROJECTION", "field_78725_b", "b", projection);
            if (matrices_ok && !env->ExceptionCheck()) {
                features::visual::render_camera_x = camera_x;
                features::visual::render_camera_y = camera_y;
                features::visual::render_camera_z = camera_z;
                features::visual::render_local_player_x = local_player_x;
                features::visual::render_local_player_y = local_player_y;
                features::visual::render_local_player_z = local_player_z;
                features::visual::render_local_player_valid = local_player_ok;
                features::visual::render_partial_ticks = partial_ticks;
                features::visual::render_world_tick = world_tick;
                for (int i = 0; i < 16; ++i) {
                    features::visual::model_view_matrix[i] = model_view[i];
                    features::visual::projection_matrix[i] = projection[i];
                }
                glGetIntegerv(GL_VIEWPORT, features::visual::view_port);
                features::visual::matrix_capture_tick =
                    static_cast<unsigned long long>(GetTickCount64());
                snapshot_ok = features::visual::view_port[2] > 0 &&
                    features::visual::view_port[3] > 0;
            }
        }
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (timer) env->DeleteLocalRef(timer);
        if (rm) env->DeleteLocalRef(rm);
        env->DeleteLocalRef(mc);
        return snapshot_ok;
    }

    std::atomic<unsigned long> active_callbacks{ 0 };

    void wait_for_callbacks()
    {
        while (active_callbacks.load(std::memory_order_acquire) != 0)
            Sleep(1);
    }
}
#ifndef GL_COMBINE
#define GL_COMBINE                        0x8570
#define GL_COMBINE_RGB                    0x8571
#define GL_COMBINE_ALPHA                  0x8572
#define GL_SOURCE0_RGB                    0x8580
#define GL_SOURCE1_RGB                    0x8581
#define GL_SOURCE0_ALPHA                  0x8588
#define GL_SOURCE1_ALPHA                  0x8589
#define GL_OPERAND0_RGB                   0x8590
#define GL_OPERAND1_RGB                   0x8591
#define GL_OPERAND0_ALPHA                 0x8598
#define GL_OPERAND1_ALPHA                 0x8599
#define GL_PRIMARY_COLOR                  0x8577
#endif

namespace network_hooks {
    typedef int(WSAAPI* WSASend_t)(SOCKET, LPWSABUF, DWORD, LPDWORD, DWORD, LPWSAOVERLAPPED, LPWSAOVERLAPPED_COMPLETION_ROUTINE);
    extern WSASend_t original_WSASend;
    int WSAAPI hooked_WSASend(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesSent, DWORD dwFlags, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine);
}

namespace hooks
{
    std::atomic<bool> frame_matrices_captured{ false };
    std::atomic<DWORD> main_thread_id{ 0 };

    static std::atomic<bool> minhook_initialized{ false };

    auto gl_clear_hook(unsigned int mask) -> void
    {
        callback_guard callback;
        typedef void(__stdcall* glClear_t)(unsigned int);

        // Matrix capture must run on the primary render thread.
        if (main_thread_id == 0 || GetCurrentThreadId() != main_thread_id)
        {
            return ((glClear_t)original_gl_clear)(mask);
        }

        static constexpr unsigned int DEPTH_BIT = 0x0100;
        static constexpr unsigned int COLOR_BIT = 0x4000;

        const bool has_depth = (mask & DEPTH_BIT) != 0;
        const bool has_color = (mask & COLOR_BIT) != 0;

        // A depth+color clear starts a new world frame. Reset before capturing
        // it; resetting after capture left the latch open and later hand/HUD
        // passes replaced the world matrices, which made overlays jitter.
        if (has_depth && has_color)
        {
            hooks::frame_matrices_captured = false;
        }


        if (has_depth && !frame_matrices_captured)
        {
            GLint vp[4] = {};
            glGetIntegerv(GL_VIEWPORT, vp);

            if (vp[2] > 0 && vp[3] > 0)
            {
                double proj[16] = {};
                glGetDoublev(GL_PROJECTION_MATRIX, proj);

                // Capturamos si es perspectiva 3D (proj[11] es -1.0)
                if (proj[11] < -0.5)
                {
                    features::visual::view_port[0] = vp[0];
                    features::visual::view_port[1] = vp[1];
                    features::visual::view_port[2] = vp[2];
                    features::visual::view_port[3] = vp[3];

                    glGetDoublev(GL_MODELVIEW_MATRIX, features::visual::model_view_matrix);
                    for (int i = 0; i < 16; ++i)
                        features::visual::projection_matrix[i] = proj[i];

                    features::visual::render_frame_snapshot_valid =
                        capture_render_frame_java_state();
                    features::visual::matrix_capture_tick =
                        static_cast<unsigned long long>(GetTickCount64());

                    frame_matrices_captured = true;
                }
            }
        }

        return ((glClear_t)original_gl_clear)(mask);
    }


    auto initialize() -> __int32
    {
        g_ImGuiCleanupDone.store(false);
        if (MH_Initialize() != MH_OK) return 1;
        minhook_initialized.store(true);

        const auto rollback = []() -> __int32 {
            MH_DisableHook(MH_ALL_HOOKS);
            MH_RemoveHook(MH_ALL_HOOKS);
            MH_Uninitialize();
            minhook_initialized.store(false);
            return 1;
        };

        HMODULE opengl_module = GetModuleHandleA("opengl32.dll");
        if (opengl_module == nullptr) return rollback();

        void* swap_addr = (void*)GetProcAddress(opengl_module, "wglSwapBuffers");
        void* clear_addr = (void*)GetProcAddress(opengl_module, "glClear");
        if (!swap_addr || !clear_addr) return rollback();
        if (MH_CreateHook(swap_addr, &hooks::swap_buffers, &hooks::original_swap_buffers) != MH_OK)
            return rollback();
        if (MH_CreateHook(clear_addr, &hooks::gl_clear_hook, &hooks::original_gl_clear) != MH_OK)
            return rollback();

        HMODULE ws2_module = GetModuleHandleA("ws2_32.dll");
        if (ws2_module) {
            void* wsasend_addr = (void*)GetProcAddress(ws2_module, "WSASend");
            if (wsasend_addr) {
                if (MH_CreateHook(wsasend_addr, (void*)&network_hooks::hooked_WSASend,
                    (void**)&network_hooks::original_WSASend) != MH_OK)
                    return rollback();
            }
        }

        if (MH_EnableHook(MH_ALL_HOOKS) != MH_OK)
            return rollback();
        return 0;
    }

    auto uninitialize_jni() -> void
    {
        if (sdk::jni && float_buffer_class) {
            sdk::jni->DeleteGlobalRef(float_buffer_class);
        }
        float_buffer_class = nullptr;
        float_buffer_get = nullptr;
    }

    auto uninitialize() -> __int32
    {
        if (!minhook_initialized.exchange(false)) return 0;
        network_hooks::flush_blink_packets();
        MH_DisableHook(MH_ALL_HOOKS);
        // Wait for a SwapBuffers callback that entered before DisableHook to
        // leave this DLL. A fixed Sleep could unload while rendering was still
        // executing and made the next injection crash.
        {
            std::lock_guard<std::recursive_mutex> lock(hooks::render_mutex);
        }
        wait_for_callbacks();
        MH_RemoveHook(MH_ALL_HOOKS);
        MH_Uninitialize();
        return 0;
    }
}
