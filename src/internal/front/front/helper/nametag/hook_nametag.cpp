#include "../../features/features.hpp"
#include "../../mapper/mapper.hpp"
#include <jnihook.h>
#include <atomic>
#include "../../hooks/hooks.hpp"

extern "C" void swiftLog(const char* format, ...);

namespace features::visual::nametags
{
    static jmethodID g_render_name_method = nullptr;
    static bool g_jnihook_initialized = false;
    static bool g_hook_attached = false;
    static std::atomic<bool> g_suppress_vanilla{ false };
    static jmethodID g_original_render_name = nullptr;
    static std::atomic<bool> g_accept_render{true};
    static std::atomic<unsigned long> g_active_render{0};

    static void JNICALL render_name_hook(JNIEnv* env, jobject renderer, jobject entity, jdouble x, jdouble y, jdouble z)
    {
        hooks::callback_guard callback;
        struct RenderGuard {
            RenderGuard() { g_active_render.fetch_add(1); }
            ~RenderGuard() { g_active_render.fetch_sub(1); }
        } active;
        if (!g_accept_render.load() || g_suppress_vanilla.load(std::memory_order_relaxed)) return;
        if (g_original_render_name) {
            env->CallVoidMethod(renderer, g_original_render_name, entity, x, y, z);
            if (env->ExceptionCheck()) env->ExceptionClear();
        }
    }

    static jmethodID find_render_name()
    {
        const mapper::__class& renderer = mapper::classes["RendererLivingEntity"];
        if (!renderer.klass || !sdk::jni) return nullptr;
        const std::string& minecraft_name = mapper::classes["Minecraft"].name;
        const char* name = "renderName";
        const char* sig = "(Lnet/minecraft/entity/EntityLivingBase;DDD)V";
        if (minecraft_name == "ave") { name = "a"; sig = "(Lpr;DDD)V"; }
        else if (minecraft_name == "bao") { name = "a"; sig = "(Lsv;DDD)V"; }
        jmethodID method = sdk::jni->GetMethodID(renderer.klass, name, sig);
        if (!method && sdk::jni->ExceptionCheck()) sdk::jni->ExceptionClear();
        return method;
    }

    void initialize_hook()
    {
        g_accept_render.store(false);
        const auto init_result = JNIHook_Init(sdk::jvm);
        swiftLog("JNIHook: initialization result=%d", static_cast<int>(init_result));
        if (init_result != JNIHOOK_OK) return;
        g_jnihook_initialized = true;
        g_render_name_method = find_render_name();
        if (!g_render_name_method) {
            const auto cleanup_result = JNIHook_Shutdown();
            swiftLog("JNIHook: incomplete setup cleanup=%d", static_cast<int>(cleanup_result));
            g_jnihook_initialized = cleanup_result != JNIHOOK_OK;
            return;
        }
        const jnihook_result_t name_result = JNIHook_Attach(g_render_name_method,
            (void*)render_name_hook, &g_original_render_name);
        g_hook_attached = name_result == JNIHOOK_OK && g_original_render_name != nullptr;
        g_accept_render.store(g_hook_attached);
        swiftLog("JNIHook: Nametags attachment=%d original=%p ready=%d",
            static_cast<int>(name_result), g_original_render_name, g_hook_attached ? 1 : 0);
        features::visual::outline::set_hook_runtime(g_hook_attached);
        if (!g_hook_attached) {
            const auto cleanup_result = JNIHook_Shutdown();
            swiftLog("JNIHook: incomplete setup cleanup=%d", static_cast<int>(cleanup_result));
            g_jnihook_initialized = cleanup_result != JNIHOOK_OK;
            g_render_name_method = nullptr;
            g_original_render_name = nullptr;
            return;
        }
    }
    void sync_hide_vanilla_hook(bool should_attach)
    {
        g_suppress_vanilla.store(should_attach && g_hook_attached, std::memory_order_relaxed);
    }

    bool uninitialize_hook()
    {
        if (!g_jnihook_initialized) return true;
        g_suppress_vanilla.store(false, std::memory_order_relaxed);
        // Attaching redefines RendererLivingEntity, so the jmethodID captured
        // before that redefinition is not guaranteed to remain valid on every
        // Java 8 VM. Passing that stale ID to JNIHook_Detach made JVMTI's
        // GetMethodDeclaringClass dereference invalid VM metadata during
        // unload. JNIHook_Shutdown restores every cached class by name and
        // therefore does not need to touch the stale method identifier.
        swiftLog("JNIHook: Restoring cached classes and shutting down...");
        g_accept_render.store(false);
        features::visual::outline::set_hook_runtime(false);
        const ULONGLONG deadline = GetTickCount64() + 3000;
        while (g_active_render.load() && GetTickCount64() < deadline) Sleep(1);
        if (g_active_render.load()) {
            g_accept_render.store(true);
            features::visual::outline::set_hook_runtime(true);
            swiftLog("JNIHook: unload cancelled; nametag callback still active");
            return false;
        }
        const auto shutdown_result = JNIHook_Shutdown();
        swiftLog("JNIHook: Shutdown result=%d", static_cast<int>(shutdown_result));
		if (shutdown_result != JNIHOOK_OK) {
            g_accept_render.store(true);
            features::visual::outline::set_hook_runtime(true);
			swiftLog("JNIHook: Refusing DLL unload because the hooked class was not restored");
			return false;
		}
        g_render_name_method = nullptr;
        g_jnihook_initialized = false;
        g_hook_attached = false;
        features::visual::outline::set_hook_runtime(false);
        g_original_render_name = nullptr;
		return true;
    }
}
