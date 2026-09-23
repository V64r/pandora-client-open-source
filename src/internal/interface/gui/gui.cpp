#include "gui.h"
#include "../../front/front/features/combat/clicksounds/ClickSounds.hpp"
#include <unordered_set>

// ============================================================
// IN-GAME MENU STATE
// ============================================================
std::atomic<bool> g_MenuVisible{ false };
std::atomic<bool> g_PlayerInGui{ false };
std::atomic<bool> g_OnMinecraftMainMenu{ false };
std::atomic<bool> g_MinecraftWorldLoaded{ false };
std::atomic<bool> g_OnMultiplayerScreen{ false };
std::atomic<bool> g_ChatOpen{ false };
bool         g_ArrayListDragMode = false;
std::atomic<bool> g_ImGuiReady{ false };
WNDPROC      g_OrigWndProc = nullptr;
static ImGuiContext* g_OurImGuiCtx = nullptr; // nuestro contexto ImGui, guardado al init
namespace swift_w_port { void invalidate_context(); }
void InvalidateInGameImGuiContext(ImGuiContext* dyingContext) {
    if (g_OurImGuiCtx == dyingContext) g_OurImGuiCtx = nullptr;
    swift_w_port::invalidate_context();
}
namespace hooks { extern ImGuiContext* g_GameImGuiContext; }
float        g_MenuOpenAnim = 0.0f;
float        g_PendingWheelDelta = 0.0f;
int   g_SelectedMod = -1;

bool           g_IsBinding = false;
int* g_BindingPtr = nullptr;
bool           g_BindMouseArmed = false;
namespace framework { extern bool g_keybind_capturing; } // set while a keybind waits for a key
std::map<int, bool> keyStates;
std::unordered_map<std::string, ULONGLONG> g_KeybindBlockedUntil;
std::unordered_map<std::string, double> g_NeedBindNoticeStarted;
std::map<ImGuiID, float> g_AnimStates;

float ImLerp(float a, float b, float t) { return a + (b - a) * t; }
ImVec4 LerpColor(ImVec4 a, ImVec4 b, float t) {
    return ImVec4(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t);
}
float GetAnim(const char* label, bool active, float speed = 0.11f) {
    ImGuiID id = ImGui::GetID(label);
    float target = active ? 1.0f : 0.0f;
    if (g_AnimStates.find(id) == g_AnimStates.end()) g_AnimStates[id] = 0.0f;
    
    // Smooth frame-rate independent interpolation (approximate)
    float dt = ImGui::GetIO().DeltaTime;
    float lerpAmount = 1.0f - expf(-speed * 60.0f * dt);
    
    g_AnimStates[id] = ImLerp(g_AnimStates[id], target, lerpAmount);
    return g_AnimStates[id];
}
int          g_MenuKey = VK_INSERT;
bool         g_ConfigInputActive = false;
bool         g_NumericEditActive = false;

float g_AccentColor[4] = { 156.f / 255.f, 156.f / 255.f, 213.f / 255.f, 1.f };
float gui_guicolor_custom[3] = { 156.f / 255.f, 156.f / 255.f, 213.f / 255.f };
bool g_MenuHeaderAnim = true;
float g_MenuHeaderAnimColor[3] = {1.0f, 1.0f, 1.0f};
float g_MenuHeaderBgColor[3] = {0.302f, 0.235f, 0.420f};

float gui_min_cps = 12.0f;
float gui_max_cps = 14.0f;
float gui_inv_cps = 20.0f;
float gui_aa_min_dist = 1.0f;
float gui_aa_max_dist = 4.0f;
float gui_aa_min_fov = 30.0f;
float gui_aa_max_fov = 180.0f;
float gui_aa_horizontal_speed = 10.0f;
float gui_aa_vertical_speed = 10.0f;
bool  gui_aa_silent = false;

float gui_refill_delay = 80.0f;
float gui_refill_silent_delay = 80.0f;
float gui_refill_silent_ticks = 2.0f;

bool gui_armorswitcher_enabled = false;
int  gui_armorswitcher_kit1 = 0;
int  gui_armorswitcher_kit2 = 1;
int  gui_armorswitcher_bind = 0;
float gui_armorswitcher_delay = 80.0f;

// -- Macros ---------------------------------------------------
bool  gui_macros_enabled = false;
int   gui_macros_mode = 0;               // 0=Bow, 1=Fireball, 2=Gap, 3=Pot
int   gui_macros_bind = 0;
float gui_macros_switch_delay = 50.0f;
float gui_macros_use_delay = 50.0f;
bool  gui_macros_auto_switch_back = true;


bool gui_friends_enabled = true;
int  gui_friends_add_bind = VK_MBUTTON;

std::vector<std::string> g_FriendsList;
std::vector<std::string> g_FriendUUIDs;
namespace features::friends {
    std::vector<std::string>* list = &g_FriendsList;
    std::vector<std::string>* uuids = &g_FriendUUIDs;
}

bool  gui_velo_enabled = false;
int   gui_velo_mode = 0;               // 0=Blatant
bool  gui_velo_air_only = false;
bool  gui_velo_moving_only = false;
bool  gui_velo_weapon_only = false;
bool  gui_velo_push_back = false;
bool  gui_velo_clicking_only = false;
float gui_velo_horizontal = 100.0f;
float gui_velo_vertical = 100.0f;
float gui_velo_chance = 100.0f;
float gui_velo_delay = 0.0f;

bool  gui_reach_enabled = false;
bool  gui_reach_ground_only = false;
bool  gui_reach_weapon_only = false;
bool  gui_reach_liquid_check = false;
bool  gui_reach_combo_mode = false;
bool  gui_reach_hit_through_walls = false;
float gui_reach_min_distance = 3.1f; float gui_reach_max_distance = 3.15f;
bool gui_reach_hitbox_enabled = false; float gui_reach_hitbox_size = 0.20f;
float gui_reach_chance = 100.0f;


bool  gui_nohitdelay_enabled = false;

std::string g_CachedPlayerName = "";
std::mutex g_CachedIdentityMutex;
GLuint g_PlayerHeadTexture = 0;
static std::mutex g_PlayerHeadMutex;
static std::vector<unsigned char> g_PlayerHeadPendingPng;
static std::string g_PlayerHeadRequestedName;
static std::string g_PlayerHeadPendingName;
static std::atomic<bool> g_PlayerHeadDownloading{ false };
static HANDLE g_PlayerHeadThread = nullptr;
static std::atomic<bool> g_PlayerHeadRefreshRequested{true};

static bool IsValidMinecraftName(const std::string& name) {
    if (name.empty() || name.size() > 16) return false;
    for (unsigned char c : name)
        if (!(std::isalnum(c) || c == '_')) return false;
    return true;
}

static DWORD WINAPI DownloadPlayerHeadThread(LPVOID parameter) {
    std::unique_ptr<std::string> name(static_cast<std::string*>(parameter));
    std::vector<unsigned char> bytes;
    HINTERNET session = WinHttpOpen(L"Swift/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (session) {
        WinHttpSetTimeouts(session, 2500, 2500, 3500, 3500);
        HINTERNET connection = WinHttpConnect(session, L"mc-heads.net",
            INTERNET_DEFAULT_HTTPS_PORT, 0);
        if (connection) {
            std::wstring wide_name(name->begin(), name->end());
            std::wstring path = L"/skin/" + wide_name + L".png";
            HINTERNET request = WinHttpOpenRequest(connection, L"GET", path.c_str(),
                nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                WINHTTP_FLAG_SECURE);
            if (request && WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                    WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
                WinHttpReceiveResponse(request, nullptr)) {
                DWORD status = 0, status_size = sizeof(status);
                WinHttpQueryHeaders(request,
                    WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                    WINHTTP_HEADER_NAME_BY_INDEX, &status, &status_size,
                    WINHTTP_NO_HEADER_INDEX);
                if (status == 200) {
                    DWORD available = 0;
                    while (WinHttpQueryDataAvailable(request, &available) && available) {
                        if (bytes.size() + available > 1024 * 1024) { bytes.clear(); break; }
                        const size_t old_size = bytes.size();
                        bytes.resize(old_size + available);
                        DWORD received = 0;
                        if (!WinHttpReadData(request, bytes.data() + old_size,
                                available, &received)) { bytes.clear(); break; }
                        bytes.resize(old_size + received);
                    }
                }
            }
            if (request) WinHttpCloseHandle(request);
            WinHttpCloseHandle(connection);
        }
        WinHttpCloseHandle(session);
    }
    {
        std::lock_guard<std::mutex> lock(g_PlayerHeadMutex);
        g_PlayerHeadPendingPng.swap(bytes);
        g_PlayerHeadPendingName = *name;
    }
    g_PlayerHeadDownloading.store(false, std::memory_order_release);
    return 0;
}

static void UploadPlayerHead(const unsigned char* pixels, int width) {
    std::vector<unsigned char> head_pixels(8 * 8 * 4, 0);
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            const unsigned char* base = pixels + (((8 + y) * (width / 64)) * width + ((8 + x) * (width / 64))) * 4;
            const unsigned char* hat = pixels + (((8 + y) * (width / 64)) * width + ((40 + x) * (width / 64))) * 4;
            unsigned char* destination = head_pixels.data() + (y * 8 + x) * 4;
            const float alpha = hat[3] / 255.0f;
            for (int channel = 0; channel < 3; ++channel)
                destination[channel] = static_cast<unsigned char>(
                    std::clamp(hat[channel] * alpha + base[channel] * (1.0f - alpha), 0.0f, 255.0f));
            destination[3] = 255;
        }
    }
    static std::vector<unsigned char> previous_head;
    if (g_PlayerHeadTexture && previous_head == head_pixels) return;
    GLuint head_texture = 0;
    GLint previous_texture = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous_texture);
    glGenTextures(1, &head_texture);
    glBindTexture(GL_TEXTURE_2D, head_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    using BindBufferFn = void (APIENTRY*)(GLenum, GLuint);
    auto bind_buffer = reinterpret_cast<BindBufferFn>(wglGetProcAddress("glBindBuffer"));
    GLint unpack_buffer = 0;
    if (bind_buffer) {
        glGetIntegerv(0x88EF, &unpack_buffer);
        bind_buffer(0x88EC, 0);
    }
    glPushClientAttrib(GL_CLIENT_PIXEL_STORE_BIT);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 8, 8, 0, GL_RGBA,
        GL_UNSIGNED_BYTE, head_pixels.data());
    glPopClientAttrib();
    if (bind_buffer) bind_buffer(0x88EC, unpack_buffer);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previous_texture));

    if (!head_texture) return;
    if (g_PlayerHeadTexture) glDeleteTextures(1, &g_PlayerHeadTexture);
    g_PlayerHeadTexture = head_texture;
    previous_head = std::move(head_pixels);
}

static jmethodID FindSkinMethod(JNIEnv* env, jobject object,
    const char* readable, const char* mapped, const std::string& prefix,
    std::string* signature = nullptr) {
    jclass cls = env->GetObjectClass(object);
    jmethodID result = nullptr;
    while (cls && !result) {
        jint count = 0; jmethodID* methods = nullptr;
        if (sdk::jvmti->GetClassMethods(cls, &count, &methods) == JVMTI_ERROR_NONE) {
            for (int i = 0; i < count && !result; ++i) {
                char* name = nullptr; char* sig = nullptr;
                if (sdk::jvmti->GetMethodName(methods[i], &name, &sig, nullptr) == JVMTI_ERROR_NONE &&
                    name && sig && (!std::strcmp(name, readable) || !std::strcmp(name, mapped)) &&
                    std::string(sig).rfind(prefix, 0) == 0) {
                    result = methods[i];
                    if (signature) *signature = sig;
                }
                if (name) sdk::jvmti->Deallocate(reinterpret_cast<unsigned char*>(name));
                if (sig) sdk::jvmti->Deallocate(reinterpret_cast<unsigned char*>(sig));
            }
            if (methods) sdk::jvmti->Deallocate(reinterpret_cast<unsigned char*>(methods));
        }
        jclass parent = result ? nullptr : env->GetSuperclass(cls);
        env->DeleteLocalRef(cls);
        cls = parent;
    }
    return result;
}

static bool UpdateHeadFromMinecraft() {
    JNIEnv* env = nullptr;
    if (!sdk::jvm || sdk::jvm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK ||
        !env || !sdk::jvmti || env->ExceptionCheck() || !wglGetCurrentContext()) return false;
    struct JniScope {
        JNIEnv* previous;
        explicit JniScope(JNIEnv* current) : previous(sdk::jni) { sdk::jni = current; }
        ~JniScope() { sdk::jni = previous; }
    } jni_scope(env);
    mapper::__minecraft mc;
    if (!mc.object || env->ExceptionCheck()) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        return false;
    }
    auto player = mc.get_local_player();
    if (!mc.object || !player.object || env->ExceptionCheck()) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        return false;
    }
    if (env->PushLocalFrame(24) < 0) { env->ExceptionClear(); return false; }
    std::string skin_signature;
    jmethodID skin_method = FindSkinMethod(env, player.object, "getLocationSkin", "func_110306_p", "()L", &skin_signature);
    jobject resource = skin_method ? env->CallObjectMethod(player.object, skin_method) : nullptr;
    jmethodID manager_method = !env->ExceptionCheck() ? FindSkinMethod(env, mc.object,
        "getTextureManager", "func_110434_K", "()L") : nullptr;
    jobject manager = manager_method ? env->CallObjectMethod(mc.object, manager_method) : nullptr;
    jobject texture = nullptr;
    if (!env->ExceptionCheck() && resource && manager) {
        jmethodID method = FindSkinMethod(env, manager, "getTexture", "func_110581_b",
            "(" + skin_signature.substr(2) + ")L");
        if (method) texture = env->CallObjectMethod(manager, method, resource);
    }
    jint texture_id = 0;
    if (!env->ExceptionCheck() && texture) {
        jmethodID method = FindSkinMethod(env, texture, "getGlTextureId", "func_110552_b", "()I");
        if (method) texture_id = env->CallIntMethod(texture, method);
    }
    if (env->ExceptionCheck()) { env->ExceptionClear(); texture_id = 0; }
    env->PopLocalFrame(nullptr);
    if (texture_id <= 0 || !glIsTexture(static_cast<GLuint>(texture_id))) return false;

    GLint previous = 0, width = 0, height = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(texture_id));
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &height);
    if (width < 64 || width > 1024 || width % 64 != 0 ||
        (height != width && height != width / 2)) {
        glBindTexture(GL_TEXTURE_2D, previous);
        return false;
    }
    using BindBufferFn = void (APIENTRY*)(GLenum, GLuint);
    auto bind_buffer = reinterpret_cast<BindBufferFn>(wglGetProcAddress("glBindBuffer"));
    GLint pack_buffer = 0;
    if (bind_buffer) {
        glGetIntegerv(0x88ED, &pack_buffer);
        bind_buffer(0x88EB, 0);
    }
    glPushClientAttrib(GL_CLIENT_PIXEL_STORE_BIT);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glPixelStorei(GL_PACK_ROW_LENGTH, 0);
    glPixelStorei(GL_PACK_SKIP_ROWS, 0);
    glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
    std::vector<unsigned char> pixels(static_cast<size_t>(width) * height * 4);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    glPopClientAttrib();
    if (bind_buffer) bind_buffer(0x88EB, pack_buffer);
    glBindTexture(GL_TEXTURE_2D, previous);
    UploadPlayerHead(pixels.data(), width);
    return true;
}

void UpdatePlayerHeadTextureOnRenderThread() {
    std::string player_name;
    {
        std::lock_guard<std::mutex> lock(g_CachedIdentityMutex);
        player_name = g_CachedPlayerName;
    }
    static std::string displayed_name;
    const bool identity_changed = player_name != displayed_name;
    if (identity_changed) {
        displayed_name = player_name;
        // Never display the previous account's face next to a new name.
        if (g_PlayerHeadTexture) {
            glDeleteTextures(1, &g_PlayerHeadTexture);
            g_PlayerHeadTexture = 0;
        }
        std::lock_guard<std::mutex> lock(g_PlayerHeadMutex);
        g_PlayerHeadPendingPng.clear();
        g_PlayerHeadPendingName.clear();
    }

    static bool live_skin_available = false;
    static bool was_visible = false, was_in_world = false;
    const bool visible = g_MenuVisible.load();
    const bool in_world = g_MinecraftWorldLoaded.load();
    const bool refresh = g_PlayerHeadRefreshRequested.exchange(false) || identity_changed ||
        (visible && !was_visible) || (in_world && !was_in_world);
    was_visible = visible;
    was_in_world = in_world;
    if (refresh) live_skin_available = UpdateHeadFromMinecraft();
    if (!in_world) live_skin_available = false;
    if (live_skin_available) return;
    bool should_download = false;
    {
        std::lock_guard<std::mutex> lock(g_PlayerHeadMutex);
        should_download = IsValidMinecraftName(player_name) &&
            (player_name != g_PlayerHeadRequestedName || refresh);
    }
    if (should_download &&
        !g_PlayerHeadDownloading.exchange(true, std::memory_order_acq_rel)) {
        if (g_PlayerHeadThread && WaitForSingleObject(g_PlayerHeadThread, 0) == WAIT_OBJECT_0) {
            CloseHandle(g_PlayerHeadThread);
            g_PlayerHeadThread = nullptr;
        }
        {
            std::lock_guard<std::mutex> lock(g_PlayerHeadMutex);
            g_PlayerHeadRequestedName = player_name;
        }
        auto* argument = new std::string(player_name);
        g_PlayerHeadThread = CreateThread(nullptr, 0, DownloadPlayerHeadThread, argument, 0, nullptr);
        if (!g_PlayerHeadThread) { delete argument; g_PlayerHeadDownloading.store(false); }
    }

    std::vector<unsigned char> png;
    std::string loaded_name;
    {
        std::lock_guard<std::mutex> lock(g_PlayerHeadMutex);
        if (!g_PlayerHeadPendingPng.empty()) {
            png.swap(g_PlayerHeadPendingPng);
            loaded_name = g_PlayerHeadPendingName;
            g_PlayerHeadPendingName.clear();
        }
    }
    // A slow response from the previous account may arrive after Alt Manager
    // changed the visible name. Discard it instead of flashing the stale head.
    if (png.empty() || loaded_name != player_name) return;
    int width = 0, height = 0, channels = 0;
    if (!stbi_info_from_memory(png.data(), static_cast<int>(png.size()),
            &width, &height, &channels) || width < 64 || height < 32 ||
            width > 600 || height > 600)
        return;
    unsigned char* pixels = stbi_load_from_memory(png.data(), (int)png.size(),
        &width, &height, &channels, 4);
    if (!pixels || width < 64 || height < 32 || width > 600 || height > 600) {
        if (pixels) stbi_image_free(pixels);
        return;
    }
    UploadPlayerHead(pixels, width);
    stbi_image_free(pixels);
}

struct FriendHeadResult { std::string name; std::vector<unsigned char> png; };
static std::mutex g_FriendHeadMutex;
static std::unordered_map<std::string, GLuint> g_FriendHeadTextures;
static std::unordered_set<std::string> g_FriendHeadRequested;
static std::vector<FriendHeadResult> g_FriendHeadPending;
static std::vector<HANDLE> g_FriendHeadThreads;

static void ReapFriendHeadThreadsLocked() {
    for (auto it = g_FriendHeadThreads.begin(); it != g_FriendHeadThreads.end();) {
        HANDLE thread = *it;
        if (!thread || WaitForSingleObject(thread, 0) == WAIT_OBJECT_0) {
            if (thread) CloseHandle(thread);
            it = g_FriendHeadThreads.erase(it);
        } else {
            ++it;
        }
    }
}

void InvalidatePlayerSkinTexturesAfterContextLoss() {
    // The old GL context owns these names.  Never delete or reuse them from the
    // replacement context; request a fresh upload instead.
    g_PlayerHeadTexture = 0;
    g_PlayerHeadRefreshRequested.store(true);
    std::lock_guard<std::mutex> lock(g_PlayerHeadMutex);
    g_PlayerHeadRequestedName.clear();
    std::lock_guard<std::mutex> friend_lock(g_FriendHeadMutex);
    g_FriendHeadTextures.clear();
    g_FriendHeadRequested.clear();
}

void ShutdownPlayerSkinLoaders() {
    if (g_PlayerHeadThread) {
        WaitForSingleObject(g_PlayerHeadThread, INFINITE);
        CloseHandle(g_PlayerHeadThread);
        g_PlayerHeadThread = nullptr;
    }
    std::vector<HANDLE> threads;
    {
        std::lock_guard<std::mutex> lock(g_FriendHeadMutex);
        threads.swap(g_FriendHeadThreads);
    }
    for (HANDLE thread : threads) {
        if (!thread) continue;
        WaitForSingleObject(thread, INFINITE);
        CloseHandle(thread);
    }
}

static DWORD WINAPI DownloadFriendHeadThread(LPVOID parameter) {
    std::unique_ptr<std::string> name(static_cast<std::string*>(parameter));
    std::vector<unsigned char> bytes;
    HINTERNET session = WinHttpOpen(L"Swift/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (session) {
        WinHttpSetTimeouts(session, 2500, 2500, 3500, 3500);
        HINTERNET connection = WinHttpConnect(session, L"mc-heads.net", INTERNET_DEFAULT_HTTPS_PORT, 0);
        if (connection) {
            std::wstring wide(name->begin(), name->end());
            std::wstring path = L"/avatar/" + wide + L"/32";
            HINTERNET request = WinHttpOpenRequest(connection, L"GET", path.c_str(), nullptr,
                WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
            if (request && WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                    WINHTTP_NO_REQUEST_DATA, 0, 0, 0) && WinHttpReceiveResponse(request, nullptr)) {
                DWORD status = 0, size = sizeof(status);
                WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                    WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX);
                DWORD available = 0;
                while (status == 200 && WinHttpQueryDataAvailable(request, &available) && available) {
                    if (bytes.size() + available > 256 * 1024) { bytes.clear(); break; }
                    size_t old = bytes.size(); bytes.resize(old + available); DWORD received = 0;
                    if (!WinHttpReadData(request, bytes.data() + old, available, &received)) { bytes.clear(); break; }
                    bytes.resize(old + received);
                }
            }
            if (request) WinHttpCloseHandle(request);
            WinHttpCloseHandle(connection);
        }
        WinHttpCloseHandle(session);
    }
    std::lock_guard<std::mutex> lock(g_FriendHeadMutex);
    g_FriendHeadPending.push_back({*name, std::move(bytes)});
    return 0;
}

static ImTextureID GetFriendHeadTexture(const std::string& name) {
    std::vector<FriendHeadResult> pending;
    {
        std::lock_guard<std::mutex> lock(g_FriendHeadMutex);
        pending.swap(g_FriendHeadPending);
    }
    for (auto& result : pending) {
        int width = 0, height = 0, channels = 0;
        const bool valid_header = !result.png.empty() && stbi_info_from_memory(
            result.png.data(), static_cast<int>(result.png.size()),
            &width, &height, &channels) != 0 && width >= 8 && height >= 8 &&
            width <= 128 && height <= 128;
        unsigned char* pixels = valid_header ? stbi_load_from_memory(
            result.png.data(), static_cast<int>(result.png.size()), &width, &height, &channels, 4) : nullptr;
        if (!pixels || width < 8 || height < 8 || width > 128 || height > 128) {
            if (pixels) stbi_image_free(pixels);
            std::lock_guard<std::mutex> lock(g_FriendHeadMutex);
            g_FriendHeadRequested.erase(result.name);
            continue;
        }
        GLint previous_texture = 0;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous_texture);
        GLuint texture = 0; glGenTextures(1, &texture); glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previous_texture));
        stbi_image_free(pixels);
        std::lock_guard<std::mutex> lock(g_FriendHeadMutex);
        auto old = g_FriendHeadTextures.find(result.name);
        if (old != g_FriendHeadTextures.end() && old->second) glDeleteTextures(1, &old->second);
        g_FriendHeadTextures[result.name] = texture;
    }
    {
        std::lock_guard<std::mutex> lock(g_FriendHeadMutex);
        ReapFriendHeadThreadsLocked();
        auto found = g_FriendHeadTextures.find(name);
        if (found != g_FriendHeadTextures.end()) return reinterpret_cast<ImTextureID>(static_cast<intptr_t>(found->second));
        if (IsValidMinecraftName(name) && g_FriendHeadRequested.insert(name).second) {
            auto* argument = new std::string(name);
            HANDLE thread = CreateThread(nullptr, 0, DownloadFriendHeadThread, argument, 0, nullptr);
            if (thread) g_FriendHeadThreads.push_back(thread);
            else {
                delete argument;
                g_FriendHeadRequested.erase(name);
            }
        }
    }
    return nullptr;
}

std::string g_CachedServerIP = "";

namespace font { extern ImFont* default_icon; }

bool  gui_watermark_enabled = false;
float gui_watermark_pos_x = 0.015f;
float gui_watermark_pos_y = 0.015f;
bool  gui_watermark_blur = true;
float gui_watermark_blur_opacity = 1.00f;
float gui_watermark_color[3] = { 1.0f, 1.0f, 1.0f };
float gui_watermark_color_b[3] = { 156.f / 255.f, 156.f / 255.f, 213.f / 255.f };
int   gui_watermark_color_mode = 1;
bool  gui_watermark_show_player = false;
bool  gui_watermark_show_server = true;
bool  gui_watermark_show_fps = false;
bool  gui_watermark_show_name = true;
bool  gui_watermark_show_time = false;
bool  gui_watermark_background = true;
bool  gui_watermark_background_shadow = true;
bool  gui_watermark_split_background = false;
bool  gui_watermark_text_shadow = true;

bool  gui_arraylist_enabled = false;
bool  gui_arraylist_watermark = false;
bool  gui_arraylist_background = true;
bool  gui_arraylist_colorbar = false;
float gui_arraylist_color[3] = { 228.f / 255.f, 228.f / 255.f, 231.f / 255.f };
float gui_arraylist_scale = 1.30f;
float gui_arraylist_speed = 0.56f;
float gui_arraylist_pos_x = 0.99f;
float gui_arraylist_pos_y = 0.02f;
float gui_arraylist_pad_x = 0.0f;
float gui_arraylist_pad_y = 0.0f;
float gui_arraylist_radius = 0.9f;
float gui_arraylist_info_color[3] = { 0.678431f, 0.678431f, 0.678431f };
float gui_arraylist_color_b[3] = { 1.0f, 1.0f, 1.0f };
float gui_arraylist_bar_width = 2.0f;
bool  gui_arraylist_bracket_flags = false;
float gui_arraylist_color_c[3] = {156.f / 255.f, 156.f / 255.f, 213.f / 255.f};
float gui_arraylist_color_space = 100.f;
int   gui_arraylist_color_mode = 2; // Gradient
bool gui_arraylist_show_info = true;
float gui_arraylist_bg_color_4[4] = { 0.0f, 0.0f, 0.0f, 0.85f };
bool  gui_arraylist_lowercase = true;
bool  gui_arraylist_shadows = true;
bool  gui_arraylist_background_shadow = true;
float gui_arraylist_shadow_strength = 55.0f;
bool  gui_arraylist_blur = true;
float gui_arraylist_blur_opacity = 0.90f;

#include <set>
std::set<std::string> g_arraylist_hidden_modules;

bool  gui_esp_enabled = false;
int   gui_esp_mode = 0;
int   gui_esp_draw_mode = 0;
int   gui_esp_2d_style = 0;
float gui_esp_corner_size = 0.25f;
float gui_esp_line_thickness = 2.0f;
float gui_esp_render_distance = 128.0f;
bool  gui_esp_pulse = false;
float gui_esp_pulse_speed = 1.50f;
float gui_esp_pulse_min_alpha = 0.40f;
float gui_esp_pulse_max_alpha = 1.00f;
bool  gui_esp_healthbar = false;
int   gui_esp_healthbar_position = 0;
int   gui_esp_healthbar_style = 0;
float gui_esp_healthbar_thickness = 3.0f;
bool  gui_esp_health_number = false;
float gui_esp_healthbar_segments = 10.0f;
float gui_esp_healthbar_gradient_top[4] = { 0.15f, 1.0f, 0.20f, 1.0f };
float gui_esp_healthbar_gradient_bottom[4] = { 1.0f, 0.12f, 0.08f, 1.0f };
bool  gui_esp_hurt_color = false;
float gui_esp_hurt_effect_color[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
int   gui_esp_fill_mode = 0;
float gui_esp_outline_color[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
float gui_esp_filled_color[4] = { 1.0f, 1.0f, 1.0f, 0.15f };
float gui_esp_fill_gradient_top[4] = { 1.0f, 1.0f, 1.0f, 0.08f };
float gui_esp_fill_gradient_bottom[4] = { 156.f/255.f, 156.f/255.f, 213.f/255.f, 1.f };
float gui_esp_friend_color[4] = { 0.35f, 1.0f, 0.45f, 0.72f };

bool  gui_nametags_enabled = false;
bool  gui_nametags_draw_health = false;
int   gui_nametags_health_format = 1;
float gui_nametags_health_segments = 10.0f;
bool  gui_nametags_show_name = true;
bool  gui_nametags_show_own = false;
bool  gui_nametags_hide_vanilla = false;
bool  gui_nametags_show_equipment = false;
bool  gui_nametags_show_enchantments = false;
bool  gui_nametags_draw_distance = false;
bool  gui_nametags_draw_hurt_time = false;
bool  gui_nametags_draw_invisible = true;
bool  gui_nametags_use_fake_name = false;
bool  gui_nametags_background = false;
char  gui_nametags_fake_name[64] = "Jugador";
float gui_nametags_color[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
float gui_nametags_scale = 1.30f;
bool  gui_nametags_distance_scaling = true;

bool  gui_tracers_enabled = false;
bool  gui_tracers_draw_distance = false;
bool  gui_tracers_draw_hurt_time = false;
bool  gui_tracers_draw_invisible = false;
float gui_tracers_color_4[4] = { 0.2f, 0.6f, 1.0f, 1.0f };
float gui_tracers_thickness = 1.5f;



bool  gui_sprint_enabled = false;

bool  gui_noslow_enabled = false;
bool  gui_noitemrelease_enabled = false;
bool  gui_noitemrelease_food = false;



bool gui_nojumpdelay_enabled = false;


// -- Fly -----------------------------------------------------

bool  gui_autoarmor_enabled = false;
float gui_autoarmor_delay = 100.0f;
bool  gui_autoarmor_only_better = true;



bool  gui_blink_enabled = false;
bool  gui_blink_show_path = false;
bool  gui_blink_show_timer = false;
float gui_blink_path_color[3] = { 1.0f, 1.0f, 1.0f };
float gui_blink_timer_limit = 10.0f;

// -- ESP unificado -------------------------------------------

// -- BlockHit ------------------------------------------------
bool  gui_blockhit_enabled = false;
int   gui_blockhit_mode = 2;        // 0=Manual, 1=Predict, 2=Auto, 3=Lag
bool  gui_blockhit_require_mouse_down = false;
float gui_blockhit_block_ticks = 2.0f;
float gui_blockhit_unblock_ticks = 3.0f;
float gui_blockhit_chance = 100.0f;
bool  gui_blockhit_only_sword = true;
bool  gui_blockhit_visual_only = false;

// -- FastPlace ------------------------------------------------
bool  gui_fastplace_enabled = false;
int   gui_fastplace_held_item = 0;  // 0=All, 1=Blocks, 2=Projectiles

// -- AutoTool -------------------------------------------------
bool  gui_autotool_enabled = false;
float gui_autotool_swap_delay = 0.0f;
bool  gui_autotool_swap_weapon = true;
bool  gui_autotool_instant_swap = true;
bool  gui_autotool_swap_back = false;
bool  gui_autotool_require_mouse_down = true;
bool  gui_autotool_only_sneaking = false;


// -- AutoClick extra ------------------------------------------
bool  gui_ac_break_blocks = false;
int   gui_ac_click_method = 0; // 0=Normal, 1=Jitter, 2=Butterfly

std::vector<std::string> g_ConfigList;
char  g_NewConfigName[64] = "";
bool  g_InConfigMenu = false;
int   g_SelectedConfig = 0;

// ============================================================
// CONFIG CARD SYSTEM (Drip Lite style)
// ============================================================
enum ConfigType { CFG_LEGIT = 0, CFG_SEMI_LEGIT = 1, CFG_BLATANT = 2 };
static std::map<std::string, int> g_ConfigTypeMap; // name -> ConfigType
static std::map<std::string, std::string> g_ConfigDateMap;
static int g_GuiScaleIndex = 0; // 0=Default, 1=150%, 2=175%, 3=200%
static float g_GuiScale = 1.0f;
static float g_GuiScaleTarget = 1.0f;
static bool g_BackgroundDim = true;
static bool g_ShowNewConfigModal = false;
static bool g_ModalJustOpened = false;
static char g_ModalConfigName[64] = "";
static int  g_ModalConfigType = 0;
static float g_ModalAnim = 0.0f;
static float g_ModalBgAnim = 0.0f;
// Button action animations: key -> { progress, actionType }
// actionType: 0=none, 1=load, 2=export, 3=delete
static std::map<std::string, int>   g_CfgBtnAction;
static std::string g_CfgPendingAction; // name of config being acted on


static int   g_CfgPendingType = 0;     // action type pending
static float g_CfgActionTimer = 0.0f;
static float g_NewConfigBtnAnim = 0.0f;
static std::map<std::string, float> g_CfgCardHoverAnim;
static float g_BtnImportAnim = 0.0f;
static float g_BtnResetAnim = 0.0f;

char g_LoggedUserName[64] = "Swift";
std::string g_LoggedUserExpire = "Lifetime";

// ============================================================
// ============================================================
// NOTIFICATION TOAST SYSTEM v4  Ultra Premium Minimalist
// ============================================================
struct NotificationToast {
    std::string title;
    std::string body;
    std::string tag;
    float       timer    = 0.0f;
    float       duration = 3.5f;
    float       slideX   = 0.0f;   // Raid0-style horizontal slide position
    float       slideY   = 0.0f;   // For vertical stacking animation
    float       alpha    = 0.0f;
    bool        leaving  = false;
};
static std::vector<NotificationToast> g_Toasts;
static constexpr int MAX_TOASTS = 6;
std::mutex g_ToastMutex;

// Shared with arraylist toasts so they can stack above system notifications
float g_system_toast_height = 0.0f;

void TriggerNotification(const char* title,
    const char* body = "",
    const char* tag  = "SYSTEM") {
    if (!features::misc::notifications::enabled)
        return;

    std::lock_guard<std::mutex> lock(g_ToastMutex);
    NotificationToast t;
    t.title = title;
    t.body  = body;
    t.tag   = tag;
    if (g_Toasts.size() >= MAX_TOASTS)
        g_Toasts.erase(g_Toasts.begin());
    g_Toasts.push_back(std::move(t));
}

void RenderNotifications() {
    std::lock_guard<std::mutex> lock(g_ToastMutex);
    if (!features::misc::notifications::enabled) {
        g_Toasts.clear();
        g_system_toast_height = 0.0f;
        return;
    }
    if (g_Toasts.empty()) { g_system_toast_height = 0.0f; return; }

    ImGuiIO& io = ImGui::GetIO();
    const float dt = io.DeltaTime > 0.05f ? 0.05f : io.DeltaTime;
    const float screenW = io.DisplaySize.x;
    const float screenH = io.DisplaySize.y;
    ImDrawList* dl = ImGui::GetForegroundDrawList();

    ImFont* font = FONT_MANAGER.get_watermark_font();
    if (!font) font = ImGui::GetFont();

    float fsz_title = 19.0f;
    float fsz_body = 16.5f;
    float pad_x = 17.f;
    float pad_y = 13.f;
    float margin_x = 16.f;
    float margin_y = 16.f;
    float spacing = 10.f;
    float rounding = 7.0f;
    float bar_height = 3.0f;
    float gap_text_bar = 8.0f;

    float line_h1 = font->CalcTextSizeA(fsz_title, FLT_MAX, 0.f, "A").y;
    float line_h2 = font->CalcTextSizeA(fsz_body, FLT_MAX, 0.f, "A").y;

    int pos_mode = features::misc::notifications::position;
    bool isTop = (pos_mode == 0 || pos_mode == 1);
    bool isLeft = (pos_mode == 0 || pos_mode == 2);

    float curTargetY = isTop ? margin_y : (screenH - margin_y);

    for (int i = (int)g_Toasts.size() - 1; i >= 0; i--) {
        auto& t = g_Toasts[i];
        t.timer += dt;

		constexpr float exit_duration = 0.35f;
        float leaveStart = t.duration - exit_duration;
        if (t.timer >= leaveStart) t.leaving = true;

        // Raid0-style alpha: lerp toward 0 on exit, 255 on enter
        if (!t.leaving) {
            float fi = t.timer / 0.2f;
            float want_alpha = fi < 1.f ? fi : 1.f;
            t.alpha += (want_alpha - t.alpha) * dt * 7.0f;
        } else {
            t.alpha += (0.0f - t.alpha) * dt * 7.0f;
        }
        float a = t.alpha;

        bool hasBody = !t.body.empty();

        float H_total = pad_y + line_h1 + (hasBody ? (line_h2 + 2.f) : 0.f) + gap_text_bar + bar_height + pad_y;

        if (isTop) {
            if (t.slideY == 0.0f) t.slideY = curTargetY - 12.f;
            t.slideY += (curTargetY - t.slideY) * dt * 7.0f;
            curTargetY += H_total + spacing;
        } else {
            curTargetY -= H_total;
            if (t.slideY == 0.0f) t.slideY = curTargetY + 12.f;
            t.slideY += (curTargetY - t.slideY) * dt * 7.0f;
            curTargetY -= spacing;
        }

        ImVec2 titleSz = font->CalcTextSizeA(fsz_title, FLT_MAX, 0.f, t.title.c_str());
        ImVec2 bodySz = hasBody ? font->CalcTextSizeA(fsz_body, FLT_MAX, 0.f, t.body.c_str()) : ImVec2(0, 0);

        float max_text_w = (titleSz.x > bodySz.x) ? titleSz.x : bodySz.x;
        float W = max_text_w + pad_x * 2.f;
        
        if (W < 235.f) W = 235.f;

        // Raid0-style slide: on exit, lerp X position off-screen
        float restX = isLeft ? margin_x : (screenW - margin_x - W);
        float offscreenX = isLeft ? (-W - margin_x) : (screenW + margin_x);
        float targetX = t.leaving ? offscreenX : restX;

        // Initialize slideX on first frame
        if (t.slideX == 0.0f && t.timer <= dt * 2.0f) {
            t.slideX = isLeft ? (-W - margin_x) : (screenW + margin_x);
        }

        // Smooth lerp toward target (Raid0 uses DeltaTime * 7.F)
        t.slideX += (targetX - t.slideX) * dt * 7.0f;

        float x = t.slideX;
        float y = t.slideY;

        ImVec2 bmin = { x, y };
        ImVec2 bmax = { x + W, y + H_total };

        // Background shadow
        for (int shadowLayer = 3; shadowLayer >= 1; --shadowLayer) {
            const float spread = 1.0f + shadowLayer * 1.5f;
            dl->AddRectFilled(ImVec2(bmin.x - spread, bmin.y - spread),
                ImVec2(bmax.x + spread, bmax.y + spread),
                IM_COL32(0, 0, 0, (int)(a * (4 - shadowLayer) * 15)), rounding + spread);
        }

        // Background
        dl->AddRectFilled(bmin, bmax, IM_COL32(20, 20, 20, (int)(255 * a)), rounding);

        // Text
        float tx = x + pad_x;
        float ty = y + pad_y;
        
        dl->AddText(font, fsz_title, { tx, ty }, IM_COL32(235, 235, 235, (int)(255 * a)), t.title.c_str());
        ty += line_h1 + 2.f;
        
        if (hasBody) {
            dl->AddText(font, fsz_body, { tx, ty }, IM_COL32(170, 170, 170, (int)(255 * a)), t.body.c_str());
            ty += line_h2 + 2.f;
        }
        
        // Progress bar
        float drainProg = 1.0f - (t.timer / t.duration);
        if (drainProg < 0.f) drainProg = 0.f;
        if (drainProg > 1.f) drainProg = 1.f;
        
        float barY = bmax.y - pad_y - bar_height;
        float barMinX = x + pad_x;
        float barMaxX = x + W - pad_x;
        float barFullW = barMaxX - barMinX;
        float prog_w = barFullW * drainProg;

        // Bar track
        dl->AddRectFilled(ImVec2(barMinX, barY), ImVec2(barMaxX, barY + bar_height),
            IM_COL32(40, 40, 40, (int)(255 * a)), rounding);

        // Bar fill
        if (prog_w > 0.01f) {
            const float* bc = features::misc::notifications::bar_color;
            const float bar_alpha = a * std::clamp(bc[3], 0.f, 1.f);
            
            // Glow
            for (int glowLayer = 6; glowLayer >= 1; --glowLayer) {
                const float spread = 0.8f + glowLayer * 1.15f;
                const float falloff = 1.f - (float)(glowLayer - 1) / 6.f;
                dl->AddRectFilled(ImVec2(barMinX - spread, barY - spread),
                    ImVec2(barMinX + prog_w + spread, barY + bar_height + spread),
                    IM_COL32((int)(bc[0]*255), (int)(bc[1]*255), (int)(bc[2]*255),
                        (int)(bar_alpha * 70.f * falloff * falloff)), rounding + spread);
            }

            ImU32 prog_color = IM_COL32((int)(bc[0]*255), (int)(bc[1]*255), (int)(bc[2]*255), (int)(255 * bar_alpha));
            dl->AddRectFilled(ImVec2(barMinX, barY), ImVec2(barMinX + prog_w, barY + bar_height),
                prog_color, rounding);
        }
    }

    g_Toasts.erase(
        std::remove_if(g_Toasts.begin(), g_Toasts.end(),
            [](const NotificationToast& t) { return t.timer >= t.duration; }),
        g_Toasts.end());

    g_system_toast_height = isTop ? (curTargetY - margin_y) : ((screenH - margin_y) - curTargetY);
}

#include "../../userconfig/userconfig.cpp"

static std::vector<std::function<void()>> restore_module_defaults;
void ResetAllSettings() {
    features::misc::bard_helper::cancel();
    features::misc::bard_helper::bind = 0;
    features::friends::nearby_bind = 0; features::friends::nearby_distance = 10.f;
    features::friends::nearby_requested = false;
    gui_arraylist_color_space = 100.f;
    gui_arraylist_color_c[0] = gui_arraylist_color_c[1] = 156.f / 255.f;
    gui_arraylist_color_c[2] = 213.f / 255.f;
    features::movement::timer_speed::enabled = false;
    features::movement::timer_speed::speed = 1.f;
    features::movement::timer_speed::require_damage = false;
    features::movement::timer_speed::only_weapon = false;
    features::movement::timer_speed::moving = false;
    features::movement::bunnyhop::enabled = false;
    features::movement::bunnyhop::liquid_check = true;
    features::movement::bunnyhop::only_moving = true;
    features::movement::bunnyhop::jump_delay = 0.f;
    features::movement::bunnyhop::jump_height = 0.42f;
    features::movement::bunnyhop::power = 1.45f;
    features::movement::bunnyhop::speed_multiplier = 1.6f;
    features::movement::bunnyhop::slowdown_factor = 0.66f;
    features::movement::bunnyhop::friction = 159.f;
    features::movement::bunnyhop::direction_threshold = 10.f;
    features::combat::auto_click::click_sound = false;
    features::combat::auto_click::sound = 0;
    features::combat::auto_click::click_volume = 50.f;
    features::misc::right_clicker::enabled = false;
    features::misc::right_clicker::min_cps = 10.f;
    features::misc::right_clicker::max_cps = 12.f;
    features::misc::right_clicker::only_click = true;
    features::misc::right_clicker::ignore_obsidian = false;
    features::misc::right_clicker::only_blocks = false;
    features::misc::bridge_assist::enabled = false;
    features::misc::bridge_assist::edge_offset = 0.15f;
    features::misc::bridge_assist::unsneak_delay = 60.f;
    features::misc::bridge_assist::select_blocks = 0;
    features::misc::bridge_assist::randomize = false;
    features::misc::bridge_assist::sneak_on_jump = false;
    features::misc::bridge_assist::avoid_double_sneaking = false;
    features::misc::bridge_assist::require_sneak = false;
    features::misc::bridge_assist::holding_blocks = true;
    features::misc::bridge_assist::looking_down = true;
    features::misc::bridge_assist::not_forward = true;
    features::visual::outline::reset();
    gui_guicolor_custom[0] = 156.f / 255.f;
    gui_guicolor_custom[1] = 156.f / 255.f;
    gui_guicolor_custom[2] = 213.f / 255.f;
    for (int i = 0; i < 3; ++i) g_AccentColor[i] = gui_guicolor_custom[i];
    features::combat::auto_click::enabled = false;
    features::combat::aim_assist::enabled = false;
    features::combat::aim_assist::mode = 0;
    features::combat::aim_assist::clicking_only = false;
    features::combat::aim_assist::weapons_only = false;
    features::combat::aim_assist::break_blocks = false;
    features::combat::aim_assist::through_walls = false;
    features::combat::aim_assist::ignore_invisible = false;
    features::combat::aim_assist::aim_mode = 2; features::combat::aim_assist::priority = 0; features::combat::aim_assist::target_mode = 1;
    gui_aa_min_dist = 1.0f; gui_aa_max_dist = 4.0f; gui_aa_min_fov = 30.0f; gui_aa_max_fov = 180.0f;
    gui_aa_horizontal_speed = 10.0f; gui_aa_vertical_speed = 10.0f;
    gui_aa_silent = false;
    features::combat::refill::enabled = false;
    gui_reach_enabled = false; gui_reach_ground_only = false; gui_reach_weapon_only = false;
    gui_reach_liquid_check = false; gui_reach_combo_mode = false; gui_reach_hit_through_walls = false;
    gui_reach_min_distance = 3.1f; gui_reach_max_distance = 3.1f; gui_reach_hitbox_enabled = false; gui_reach_hitbox_size = 0.1f; gui_reach_chance = 100.0f;
    gui_velo_enabled = false; gui_velo_air_only = false; gui_velo_moving_only = false;
    gui_velo_weapon_only = false; gui_velo_push_back = false; gui_velo_clicking_only = false;
    gui_velo_horizontal = 100.0f; gui_velo_vertical = 100.0f; gui_velo_chance = 100.0f; gui_velo_delay = 0.0f;
    gui_nohitdelay_enabled = false;
    gui_sprint_enabled = false;
    gui_noslow_enabled = false;
    gui_noitemrelease_enabled = false; gui_noitemrelease_food = false;
    gui_nojumpdelay_enabled = false;
    features::movement::snap_tap::enabled = false;
    features::movement::instant_stop::enabled = false;
    features::movement::instant_stop::only_on_ground = true;
    features::movement::instant_stop::stop_on_sneak = false;
    features::movement::instant_stop::stop_strength = 1.0f;
    gui_watermark_color[0] = 1.0f; gui_watermark_color[1] = 1.0f; gui_watermark_color[2] = 1.0f;
    gui_watermark_color_b[0] = 156.f / 255.f; gui_watermark_color_b[1] = 156.f / 255.f; gui_watermark_color_b[2] = 213.f / 255.f;
    gui_autoarmor_enabled = false; gui_autoarmor_delay = 100.0f; gui_autoarmor_only_better = true;
    gui_blink_enabled = false; gui_blink_show_path = false; gui_blink_show_timer = false;
    gui_blink_path_color[0] = 1.0f; gui_blink_path_color[1] = 1.0f; gui_blink_path_color[2] = 1.0f;
    gui_blink_timer_limit = 10.0f;
    gui_esp_enabled = false;
    gui_esp_mode = 0; gui_esp_draw_mode = 0; gui_esp_2d_style = 0; gui_esp_corner_size = 0.25f;
    gui_esp_line_thickness = 2.0f; gui_esp_render_distance = 128.0f;
    gui_esp_pulse = false; gui_esp_pulse_speed = 1.50f; gui_esp_pulse_min_alpha = 0.40f; gui_esp_pulse_max_alpha = 1.00f;
    gui_esp_healthbar = false; gui_esp_healthbar_position = 0; gui_esp_healthbar_style = 0;
    gui_esp_healthbar_thickness = 3.0f; gui_esp_health_number = false; gui_esp_healthbar_segments = 10.0f;
    gui_esp_hurt_color = false; gui_esp_fill_mode = 0;
    gui_esp_outline_color[0] = 1.0f; gui_esp_outline_color[1] = 1.0f; gui_esp_outline_color[2] = 1.0f; gui_esp_outline_color[3] = 1.0f;
    gui_esp_filled_color[0] = 1.0f; gui_esp_filled_color[1] = 1.0f; gui_esp_filled_color[2] = 1.0f; gui_esp_filled_color[3] = 0.15f;
    gui_esp_hurt_effect_color[0] = 1.0f; gui_esp_hurt_effect_color[1] = 0.0f; gui_esp_hurt_effect_color[2] = 0.0f; gui_esp_hurt_effect_color[3] = 1.0f;
    gui_esp_healthbar_gradient_top[0] = 0.15f; gui_esp_healthbar_gradient_top[1] = 1.0f; gui_esp_healthbar_gradient_top[2] = 0.20f; gui_esp_healthbar_gradient_top[3] = 1.0f;
    gui_esp_healthbar_gradient_bottom[0] = 1.0f; gui_esp_healthbar_gradient_bottom[1] = 0.12f; gui_esp_healthbar_gradient_bottom[2] = 0.08f; gui_esp_healthbar_gradient_bottom[3] = 1.0f;
    gui_esp_fill_gradient_top[0] = 1.0f; gui_esp_fill_gradient_top[1] = 1.0f; gui_esp_fill_gradient_top[2] = 1.0f; gui_esp_fill_gradient_top[3] = 0.08f;
    gui_esp_fill_gradient_bottom[0] = 156.f/255.f; gui_esp_fill_gradient_bottom[1] = 156.f/255.f; gui_esp_fill_gradient_bottom[2] = 213.f/255.f; gui_esp_fill_gradient_bottom[3] = 1.f;
    gui_esp_friend_color[0] = 0.35f; gui_esp_friend_color[1] = 1.0f; gui_esp_friend_color[2] = 0.45f; gui_esp_friend_color[3] = 0.72f;
    gui_nametags_enabled = false; gui_nametags_health_format = 1; gui_nametags_health_segments = 10.0f; gui_nametags_draw_health = false; gui_nametags_show_name = true; gui_nametags_show_own = false; gui_nametags_hide_vanilla = false; gui_nametags_show_equipment = false; gui_nametags_show_enchantments = false; gui_nametags_draw_distance = false;
    features::visual::hit_markers::enabled = false; features::visual::hit_markers::mode = 0;
    features::visual::hit_markers::size = 10.f; features::visual::hit_markers::line_width = 2.f;
    features::visual::hit_markers::duration = 0.5f; features::visual::hit_markers::fade_out = true;
    features::visual::hit_markers::scale_animation = true; features::visual::hit_markers::scale_amount = 1.5f;
    features::visual::hit_markers::outline = true; features::visual::hit_markers::outline_width = 1.f;
    gui_nametags_draw_hurt_time = true; gui_nametags_draw_invisible = false;
    gui_nametags_scale = 1.30f; gui_nametags_distance_scaling = true;
    gui_tracers_enabled = false; gui_tracers_draw_distance = true; gui_tracers_draw_hurt_time = true;
    gui_tracers_draw_invisible = false; gui_tracers_thickness = 1.5f;
    gui_watermark_enabled = false;
    gui_arraylist_enabled = false; gui_arraylist_watermark = false; gui_arraylist_background = true;
    gui_arraylist_colorbar = false; features::misc::notifications::enabled = true;
    gui_arraylist_scale = 1.30f; gui_arraylist_speed = 0.56f;
    gui_arraylist_pos_x = 1.0f;  gui_arraylist_pos_y = 0.0f;
    gui_arraylist_pad_x = 0.0f;  gui_arraylist_pad_y = 0.0f;   gui_arraylist_radius = 0.9f;
    gui_arraylist_color_mode = 2; gui_arraylist_show_info = true;
    gui_arraylist_lowercase = true; gui_arraylist_shadows = true;
    gui_arraylist_background_shadow = true; gui_arraylist_shadow_strength = 55.0f; gui_arraylist_blur = true;
    gui_arraylist_bracket_flags = false;
    gui_arraylist_color[0] = 228.f / 255.f; gui_arraylist_color[1] = 228.f / 255.f; gui_arraylist_color[2] = 231.f / 255.f;
    gui_arraylist_color_b[0] = 1.0f; gui_arraylist_color_b[1] = 1.0f; gui_arraylist_color_b[2] = 1.0f;
    gui_arraylist_info_color[0] = 0.678431f; gui_arraylist_info_color[1] = 0.678431f; gui_arraylist_info_color[2] = 0.678431f;
    features::misc::notifications::bar_color[0] = 228.f / 255.f;
    features::misc::notifications::bar_color[1] = 228.f / 255.f;
    features::misc::notifications::bar_color[2] = 231.f / 255.f;
    features::misc::notifications::bar_color[3] = 1.0f;
    gui_arraylist_bg_color_4[0] = 0.0f; gui_arraylist_bg_color_4[1] = 0.0f;
    gui_arraylist_bg_color_4[2] = 0.0f; gui_arraylist_bg_color_4[3] = 0.85f;
    g_arraylist_hidden_modules.clear();
    

    for (auto& mod : modules) mod.keybind = 0;
    g_KeybindMap.clear();
    g_KeybindBlockedUntil.clear();

    gui_blockhit_enabled = false; gui_blockhit_mode = 2; gui_blockhit_require_mouse_down = false; gui_blockhit_block_ticks = 2.0f; gui_blockhit_unblock_ticks = 3.0f; gui_blockhit_chance = 100.0f; gui_blockhit_only_sword = true; gui_blockhit_visual_only = false;
    gui_fastplace_enabled = false; gui_fastplace_held_item = 0;
    gui_autotool_enabled = false; gui_autotool_swap_delay = 0.0f; gui_autotool_swap_weapon = true;
    gui_autotool_instant_swap = true;  gui_autotool_swap_back = false; gui_autotool_require_mouse_down = true; gui_autotool_only_sneaking = false;
    gui_ac_break_blocks = false;
    gui_min_cps = 12.0f; gui_max_cps = 14.0f; gui_inv_cps = 20.0f;
    gui_aa_min_dist = 1.0f; gui_aa_max_dist = 4.0f; gui_aa_min_fov = 30.0f; gui_aa_max_fov = 180.0f;
    gui_aa_horizontal_speed = 10.0f; gui_aa_vertical_speed = 10.0f;
    gui_aa_silent = false; gui_ac_click_method = 0;
    gui_refill_delay = 80.0f; gui_refill_silent_delay = 80.0f; gui_refill_silent_ticks = 2.0f;
    gui_armorswitcher_enabled = false; gui_friends_enabled = true;
    gui_friends_add_bind = VK_MBUTTON;
    features::friends::clear();
    gui_armorswitcher_kit1 = 0; gui_armorswitcher_kit2 = 1;
    gui_armorswitcher_bind = 0; gui_armorswitcher_delay = 80.0f;
    gui_macros_enabled = false; gui_macros_mode = 0; gui_macros_bind = 0;
    gui_macros_switch_delay = 50.0f; gui_macros_use_delay = 50.0f; gui_macros_auto_switch_back = true;
    for (const auto& restore : restore_module_defaults) restore();
    for (auto& module : modules) {
        if (module.enabledPtr) *module.enabledPtr = false;
        module.keybind = 0;
    }
    gui_friends_enabled = false;
    g_arraylist_hidden_modules.clear();
    ++g_ConfigLoadRevision;
    TriggerNotification("All settings have been reset.", "Every module restored to its default values.", "RESET");
}

enum Category {
    CAT_COMBAT = 0,
    CAT_VISUALS,
    CAT_MOVEMENT,
    CAT_LATENCY,
    CAT_MISC,
    CAT_SETTINGS,
    CAT_CONFIGS
};


std::string GetKeyName(int key) {
    if (key == 0) return "NONE";
    if (key >= 'A' && key <= 'Z') return std::string(1, (char)key);
    if (key >= '0' && key <= '9') return std::string(1, (char)key);
    switch (key) {
    case VK_LBUTTON: return "M1"; case VK_RBUTTON: return "M2"; case VK_MBUTTON: return "M3";
    case VK_XBUTTON1: return "M4"; case VK_XBUTTON2: return "M5";
    case VK_SHIFT: return "Shift"; case VK_CONTROL: return "Ctrl"; case VK_MENU: return "Alt";
    case VK_SPACE: return "Space"; case VK_ESCAPE: return "Esc"; case VK_RETURN: return "Enter";
    case VK_INSERT: return "Insert"; case VK_DELETE: return "Del";
    default: return "K" + std::to_string(key);
    }
}

void InitMenuData();

// ============================================================
// HOOKED WNDPROC
// ============================================================
static int s_CursorIncrements = 0;
static std::atomic<bool> s_DeferredCursorRelease{ false };

static void ReleaseMenuCursorForGameplay() {
    s_DeferredCursorRelease.store(false, std::memory_order_release);
    // ImGui may still own the mouse when the menu is closed while a control is
    // hovered/pressed. Release that capture before giving input back to LWJGL.
    if (GetCapture() == g_GameWindow)
        ReleaseCapture();

    // The menu lets the native cursor move freely. If gameplay resumes while
    // it is left away from the client centre, Minecraft interprets that old
    // absolute displacement as a fresh relative mouse delta and the camera
    // jumps by itself. Re-centre first, while the cursor is still visible.
    if (g_GameWindow && IsWindow(g_GameWindow)) {
        RECT clientRect{};
        if (GetClientRect(g_GameWindow, &clientRect)) {
            POINT centre{
                (clientRect.left + clientRect.right) / 2,
                (clientRect.top + clientRect.bottom) / 2
            };
            ClientToScreen(g_GameWindow, &centre);
            SetCursorPos(centre.x, centre.y);
        }
    }

    // Do not leave a held ImGui button active across the menu transition.
    ImGuiContext* previousContext = ImGui::GetCurrentContext();
    if (g_OurImGuiCtx) {
        ImGui::SetCurrentContext(g_OurImGuiCtx);
        ImGuiIO& io = ImGui::GetIO();
        for (bool& buttonDown : io.MouseDown)
            buttonDown = false;
        ImGui::SetCurrentContext(previousContext);
    }

    for (int i = 0; i < s_CursorIncrements; i++) ShowCursor(FALSE);
    s_CursorIncrements = 0;

    // ShowCursor changes the display counter, but Windows may keep painting the
    // last cursor shape until the next mouse message. Clear that cached shape
    // now so closing with Insert never leaves a frozen arrow on screen.
    SetCursor(nullptr);
}

static void MenuOpen(HWND hWnd) {
    g_PlayerHeadRefreshRequested.store(true);
    ClipCursor(nullptr);
    SetFocus(hWnd);
    RECT rc;
    GetClientRect(hWnd, &rc);
    POINT center = { (rc.right - rc.left) / 2, (rc.bottom - rc.top) / 2 };
    ClientToScreen(hWnd, &center);
    SetCursorPos(center.x, center.y);
    // Alt Manager closes back into Minecraft's Multiplayer GUI, where Windows
    // must keep the cursor visible. Reopening it must reuse that ownership,
    // otherwise every open/restore cycle increments ShowCursor again.
    if (s_DeferredCursorRelease.exchange(false, std::memory_order_acq_rel) &&
        s_CursorIncrements > 0) {
        SetCursor(LoadCursor(NULL, IDC_ARROW));
        return;
    }
    // Minecraft/LWJGL can decrement Windows' cursor display counter while a
    // deferred Alt Manager reference still exists. Always verify the real
    // counter instead of trusting only our bookkeeping; every increment is
    // tracked and restored when gameplay resumes.
    int sc;
    do {
        sc = ShowCursor(TRUE);
        ++s_CursorIncrements;
    } while (sc < 0);
    s_DeferredCursorRelease.store(false, std::memory_order_release);
    SetCursor(LoadCursor(NULL, IDC_ARROW));
}

static void MenuClose(bool keepCursorVisible = false) {
    g_ConfigInputActive = false;
    // Config profiles are explicit snapshots. Closing the menu must never
    // overwrite the last manually saved state.
    // Closing the regular menu returns to gameplay and must release the cursor.
    // Closing Alt Manager returns to Minecraft's multiplayer GUI, where the
    // cursor must remain visible. Keep the same native cursor reference instead
    // of replacing it with a second software cursor.
    if (!keepCursorVisible) {
        ReleaseMenuCursorForGameplay();
    } else {
        s_DeferredCursorRelease.store(true, std::memory_order_release);
        ClipCursor(nullptr);
        SetCursor(LoadCursor(NULL, IDC_ARROW));
    }
}

static LRESULT WINAPI HookedWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (!g_Running) {
        if (msg == WM_NULL) {
            WNDPROC orig = nullptr;
            {
                std::lock_guard<std::recursive_mutex> imguiContextGuard(hooks::render_mutex);
                if (g_OrigWndProc) {
                    orig = g_OrigWndProc;
                    SetWindowLongPtrA(hWnd, GWLP_WNDPROC, (LONG_PTR)orig);
                    g_OrigWndProc = nullptr;
                }
            }
            if (orig) return CallWindowProc(orig, hWnd, msg, wParam, lParam);
        }
        
        // Ensure ALL input and ImGui processing is completely bypassed during unload.
        return g_OrigWndProc ? CallWindowProc(g_OrigWndProc, hWnd, msg, wParam, lParam) : DefWindowProc(hWnd, msg, wParam, lParam);
    }

    // ImGui uses a process-global current-context pointer. Serialize all window
    // messages with render/reinitialization so F11 cannot dispatch into a freed context.
    std::lock_guard<std::recursive_mutex> imguiContextGuard(hooks::render_mutex);
    extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

    // Losing focus starts the exact same reverse animation as Insert. Keep the
    // native cursor visible for the application that just received focus, but
    // stop ImGui from drawing its own cursor immediately.
    if ((msg == WM_KILLFOCUS ||
         (msg == WM_ACTIVATE && LOWORD(wParam) == WA_INACTIVE) ||
         (msg == WM_ACTIVATEAPP && wParam == FALSE)) && g_MenuVisible) {
        g_MenuVisible = false;
        g_AltManagerMode = false;
        MenuClose(true);
    }

    // If Minecraft regains focus after the focus-triggered close, restore its
    // gameplay cursor state without waiting for camera movement.
    if (msg == WM_SETFOCUS && !g_MenuVisible && s_CursorIncrements > 0) {
        ReleaseMenuCursorForGameplay();
    }

    if (msg == WM_SYSCOMMAND && wParam == SC_KEYMENU) {
        return 0;
    }


    if (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN) {
        const UINT keyScanCode = (UINT)((lParam >> 16) & 0xFF);
        const bool rightShiftPressed =
            (int)wParam == VK_RSHIFT ||
            ((int)wParam == VK_SHIFT && keyScanCode == 0x36);
        // Capture the next real key event instead of polling every frame.
        // Right Shift is reserved for the Alt Manager and is accepted only
        // while Minecraft is showing its multiplayer/server-selection screen.
        if (rightShiftPressed && (lParam & (1LL << 30)) == 0 &&
            g_OnMultiplayerScreen.load(std::memory_order_acquire)) {
            if (!g_MenuVisible) {
                g_AltManagerMode = true;
                g_AltManagerRefocusInput.store(true, std::memory_order_release);
                g_MenuVisible = true;
                MenuOpen(hWnd);
            }
            return 0;
        }
        static ULONGLONG lastMenuToggle = 0;
        const ULONGLONG menuToggleNow = GetTickCount64();
        if ((int)wParam == g_MenuKey && (lParam & (1LL << 30)) == 0 &&
            !g_ChatOpen.load(std::memory_order_acquire) &&
            menuToggleNow - lastMenuToggle >= 220) {
            lastMenuToggle = menuToggleNow;
            if (!g_MenuVisible)
                g_AltManagerMode = false;
            g_MenuVisible = !g_MenuVisible;
            if (g_MenuVisible) MenuOpen(hWnd);
            else               MenuClose();
            return 0;
        }
        // A keybind is actively capturing a key: Escape must clear that bind to
        // "None" (handled by the keybind itself), NOT close the menu.
        if (wParam == VK_ESCAPE && g_MenuVisible && framework::g_keybind_capturing) {
            return 0;
        }
        if (wParam == VK_ESCAPE && g_MenuVisible && g_AltManagerMode) {
            g_MenuVisible = false;
            g_AltManagerMode = false;
            g_MenuOpenAnim = 0.0f;
            MenuClose(true);
            return 0;
        }
        if (wParam == VK_ESCAPE && g_MenuVisible) {
            bool editingText = g_ConfigInputActive || g_NumericEditActive;
            ImGuiContext* previousContext = ImGui::GetCurrentContext();
            if (hooks::g_GameImGuiContext) {
                ImGui::SetCurrentContext(hooks::g_GameImGuiContext);
                editingText = editingText || ImGui::GetIO().WantTextInput;
            }
            ImGui::SetCurrentContext(previousContext);
            if (editingText) {
                if (hooks::g_GameImGuiContext) {
                    ImGui::SetCurrentContext(hooks::g_GameImGuiContext);
                    ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);
                    ImGui::SetCurrentContext(previousContext);
                }
                return 0;
            }
            g_MenuVisible = false;
            MenuClose();
            return 0;
        }
    }
    if (g_MenuVisible || g_AltManagerMode) {
        if (msg == WM_SETCURSOR) { SetCursor(LoadCursor(NULL, IDC_ARROW)); return TRUE; }
        
        std::lock_guard<std::recursive_mutex> lock(hooks::render_mutex);
        
        if (msg == WM_LBUTTONDOWN || msg == WM_LBUTTONUP ||
            msg == WM_RBUTTONDOWN || msg == WM_RBUTTONUP ||
            msg == WM_MOUSEMOVE || msg == WM_INPUT) {
            ImGuiContext* prev = ImGui::GetCurrentContext();
            if (g_OurImGuiCtx) ImGui::SetCurrentContext(g_OurImGuiCtx);
            ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);
            if (g_OurImGuiCtx) ImGui::SetCurrentContext(prev);
            return 0;
        }
        if (msg == WM_MOUSEWHEEL) {
            ImGuiContext* prev = ImGui::GetCurrentContext();
            if (g_OurImGuiCtx) ImGui::SetCurrentContext(g_OurImGuiCtx);
            ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);
            if (g_OurImGuiCtx) ImGui::SetCurrentContext(prev);
            return 0;
        }
        // pueda seguir moviendose con el menu visible.
        if (msg == WM_CHAR || msg == WM_KEYDOWN || msg == WM_KEYUP ||
            msg == WM_SYSKEYDOWN || msg == WM_SYSKEYUP) {
            if (g_ConfigInputActive || g_NumericEditActive || g_AltManagerMode) {
                // InputText activo: enviar WM_CHAR a NUESTRO contexto ImGui (no el del juego)
                ImGuiContext* prev = ImGui::GetCurrentContext();
                if (g_OurImGuiCtx) ImGui::SetCurrentContext(g_OurImGuiCtx);
                ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);
                if (g_OurImGuiCtx) ImGui::SetCurrentContext(prev);
                return 0;
            }
            ImGuiContext* prev = ImGui::GetCurrentContext();
            if (hooks::g_GameImGuiContext)
                ImGui::SetCurrentContext(hooks::g_GameImGuiContext);
            ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);
            ImGui::SetCurrentContext(prev);
            bool isMovement = (wParam == 'W' || wParam == 'A' || wParam == 'S' || wParam == 'D' ||
                wParam == VK_SPACE || wParam == VK_SHIFT || wParam == VK_CONTROL ||
                wParam == VK_LSHIFT || wParam == VK_RSHIFT ||
                wParam == VK_LCONTROL || wParam == VK_RCONTROL);
            if (isMovement)
                return g_OrigWndProc ? CallWindowProcA(g_OrigWndProc, hWnd, msg, wParam, lParam) : DefWindowProcA(hWnd, msg, wParam, lParam);
            return 0;
        }
        if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) return 1;
    }
    return g_OrigWndProc ? CallWindowProcA(g_OrigWndProc, hWnd, msg, wParam, lParam) : DefWindowProcA(hWnd, msg, wParam, lParam);
}

// ============================================================
// INIT IN-GAME IMGUI
// ============================================================
void InitInGameImGui(HWND hwnd) {
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    io.FontGlobalScale = 1.0f;
    io.Fonts->Clear();

    // ============================================================
    // INICIALIZAR FONT MANAGER
    // ============================================================
    FONT_MANAGER.initialize(io, myModule);

    io.FontDefault = FONT_MANAGER.get_default();

    io.FontDefault = FONT_MANAGER.get_default();

    // ============================================================
    // ============================================================
    ImGuiStyle& style = ImGui::GetStyle();
    style.AntiAliasedLines = true;
    style.AntiAliasedLinesUseTex = true;
    style.AntiAliasedFill = true;
    style.CurveTessellationTol = 0.5f;
    style.CircleTessellationMaxError = 0.3f;
    style.WindowRounding = 10.f; style.ChildRounding = 6.f;  style.FrameRounding = 5.f;
    style.PopupRounding = 6.f;  style.ScrollbarRounding = 6.f; style.GrabRounding = 6.f;
    style.WindowBorderSize = 0.f; style.ChildBorderSize = 0.f; style.FrameBorderSize = 0.f;
    style.ItemSpacing = ImVec2(8, 10); style.ItemInnerSpacing = ImVec2(6, 4);
    style.WindowPadding = ImVec2(0, 0);  style.FramePadding = ImVec2(10, 6);
    style.ScrollbarSize = 4.f;           style.GrabMinSize = 4.f;
    ImVec4* c = style.Colors;
    c[ImGuiCol_WindowBg] = ImVec4(0.032f, 0.035f, 0.043f, 0.98f);
    c[ImGuiCol_ChildBg] = ImVec4(0.045f, 0.048f, 0.058f, 0.98f);
    c[ImGuiCol_PopupBg] = ImVec4(0.055f, 0.058f, 0.068f, 1.f);
    c[ImGuiCol_Border] = ImVec4(0.11f, 0.115f, 0.13f, 1.f);
    c[ImGuiCol_FrameBg] = ImVec4(0.050f, 0.054f, 0.066f, 1.f);
    c[ImGuiCol_FrameBgHovered] = ImVec4(0.078f, 0.082f, 0.098f, 1.f);
    c[ImGuiCol_FrameBgActive] = ImVec4(0.095f, 0.10f, 0.12f, 1.f);
    c[ImGuiCol_ScrollbarBg] = ImVec4(0.04f, 0.04f, 0.05f, 1.f);
    c[ImGuiCol_ScrollbarGrab] = ImVec4(0.20f, 0.20f, 0.22f, 1.f);
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(g_AccentColor[0], g_AccentColor[1], g_AccentColor[2], 0.6f);
    c[ImGuiCol_ScrollbarGrabActive] = ImVec4(g_AccentColor[0], g_AccentColor[1], g_AccentColor[2], 1.f);
    c[ImGuiCol_CheckMark] = ImVec4(g_AccentColor[0], g_AccentColor[1], g_AccentColor[2], 1.f);
    c[ImGuiCol_SliderGrab] = ImVec4(g_AccentColor[0], g_AccentColor[1], g_AccentColor[2], 1.f);
    c[ImGuiCol_SliderGrabActive] = ImVec4(0.98f, 0.98f, 0.99f, 1.f);
    c[ImGuiCol_Button] = ImVec4(0.050f, 0.052f, 0.062f, 1.f);
    c[ImGuiCol_ButtonHovered] = ImVec4(0.080f, 0.083f, 0.098f, 1.f);
    c[ImGuiCol_ButtonActive] = ImVec4(g_AccentColor[0], g_AccentColor[1], g_AccentColor[2], 0.5f);
    c[ImGuiCol_Header] = ImVec4(0.f, 0.f, 0.f, 0.f);
    c[ImGuiCol_HeaderHovered] = ImVec4(0.075f, 0.080f, 0.095f, 1.f);
    c[ImGuiCol_HeaderActive] = ImVec4(0.095f, 0.10f, 0.12f, 1.f);
    c[ImGuiCol_Separator] = ImVec4(0.27f, 0.27f, 0.30f, 1.f);
    c[ImGuiCol_Text] = ImVec4(0.90f, 0.90f, 0.92f, 1.f);
    c[ImGuiCol_TextDisabled] = ImVec4(0.38f, 0.38f, 0.43f, 1.f);
#if IMGUI_VERSION_NUM >= 19200
    c[ImGuiCol_InputTextCursor] = ImVec4(g_AccentColor[0], g_AccentColor[1], g_AccentColor[2], 1.f);
#endif
    // Only hook WndProc if we haven't already (prevents infinite recursion on F11 reinit)
    if (!g_OrigWndProc) {
        g_OrigWndProc = (WNDPROC)SetWindowLongPtrA(hwnd, GWLP_WNDPROC, (LONG_PTR)HookedWndProc);
    }
    g_render->setup(); // W_AUTHENTIC_SETUP
    g_OurImGuiCtx = ImGui::GetCurrentContext();

    static bool s_MenuDataInited = false;
    if (!s_MenuDataInited) {
        InitMenuData();
        ReloadConfigsFromDisk();
        LoadConfigTypes();
        s_MenuDataInited = true;
    }

    g_ImGuiReady = true;
}

// ============================================================
// PROCESS KEYBINDS
// ============================================================
void ProcessKeybinds() {
    // Module binds are local to Minecraft. GetAsyncKeyState is global, so
    // without this guard a bind pressed in Explorer, Discord, etc. toggles a
    // module in the background. Keep the edge state synchronized while the
    // game is unfocused so holding a key during Alt+Tab cannot trigger it when
    // focus returns either.
    const HWND foregroundWindow = GetForegroundWindow();
    const bool gameHasFocus = g_GameWindow && IsWindow(g_GameWindow) &&
        (foregroundWindow == g_GameWindow ||
         GetAncestor(foregroundWindow, GA_ROOT) == GetAncestor(g_GameWindow, GA_ROOT));
    if (!gameHasFocus) {
        for (const auto& mod : modules) {
            if (mod.keybind != 0)
                keyStates[mod.keybind] = (GetAsyncKeyState(mod.keybind) & 0x8000) != 0;
        }
        return;
    }

    // The click that opens a bind editor must be released before mouse buttons
    // become eligible as the new bind. Keyboard events remain immediately valid.
    if (g_IsBinding && g_BindingPtr && !g_BindMouseArmed) {
        const bool anyMouseDown =
            (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0 ||
            (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0 ||
            (GetAsyncKeyState(VK_MBUTTON) & 0x8000) != 0 ||
            (GetAsyncKeyState(VK_XBUTTON1) & 0x8000) != 0 ||
            (GetAsyncKeyState(VK_XBUTTON2) & 0x8000) != 0;
        if (!anyMouseDown) g_BindMouseArmed = true;
    } else if (!g_IsBinding) {
        g_BindMouseArmed = false;
    }

    // g_PlayerInGui se actualiza desde el hilo principal (get_time) donde JNI es valido
    if (!g_MenuVisible && !g_PlayerInGui) {
        std::vector<int> checkedKeys;
        for (auto& mod : modules) {
            if (mod.keybind != 0 && mod.enabledPtr) {
                bool pressed = (GetAsyncKeyState(mod.keybind) & 0x8000) != 0;
                const auto blocked = g_KeybindBlockedUntil.find(mod.name);
                if (blocked != g_KeybindBlockedUntil.end()) {
                    if (GetTickCount64() < blocked->second) {
                        // Keep the edge state synchronized while the confirmation
                        // toast is visible. Holding the new key cannot trigger the
                        // module as soon as the toast finishes; it must be released
                        // and pressed again.
                        checkedKeys.push_back(mod.keybind);
                        continue;
                    }
                    g_KeybindBlockedUntil.erase(blocked);
                }
                if (pressed && !keyStates[mod.keybind]) {
                    *mod.enabledPtr = !(*mod.enabledPtr);
                    extern void PushModuleToast(const std::string& name, bool state, int keybind);
                    PushModuleToast(mod.name, *mod.enabledPtr, mod.keybind);
                }
                checkedKeys.push_back(mod.keybind);
            }
        }
        for (int k : checkedKeys) {
            keyStates[k] = (GetAsyncKeyState(k) & 0x8000) != 0;
        }
    }
}


// ============================================================
// INIT MENU DATA
// ============================================================
void SelectFirstModuleInCategory(int category) {
    g_SelectedMod = -1;
    for (int i = 0; i < (int)modules.size(); ++i) {
        if (modules[i].category == category && !modules[i].hidden) {
            g_SelectedMod = i;
            break;
        }
    }
}

void InitMenuData() {
    modules.clear();

    // -- Left Clicker --------------------------------------------
    Module lc = CreateMod("Left Clicker", "Automatically clicks for you at the desired CPS rate.", CAT_COMBAT, &features::combat::auto_click::enabled);
    // Replaced standard CPS slider with Custom Range Slider inside Render code
    Setting s_inv_cps; s_inv_cps.type = Setting::SLIDER; s_inv_cps.name = "Inventory CPS"; s_inv_cps.floatPtr = &gui_inv_cps; s_inv_cps.min = 1.0f; s_inv_cps.max = 25.0f; s_inv_cps.format = "%.1f";
    s_inv_cps.visibleCondition = [](){ return features::combat::auto_click::inventory_enabled; };
    lc.settings.push_back(s_inv_cps);
    { Setting s; s.type = Setting::MULTIBOX; s.name = "Conditions"; s.multiboxItems = {
        { "Break Blocks", &gui_ac_break_blocks },
        { "Inventory Fill", &features::combat::auto_click::inventory_enabled },
        { "Weapons Only", &features::combat::auto_click::weapons_only },
        { "Target Only", &features::combat::auto_click::target_only }
    }; lc.settings.push_back(std::move(s)); }
    { Setting s{Setting::TOGGLE,"Prevent Unrefill",&features::combat::auto_click::prevent_unrefill}; s.visibleCondition=[](){return features::combat::auto_click::inventory_enabled;}; lc.settings.push_back(s); }
    lc.settings.push_back({ Setting::TOGGLE, "Randomization",    &features::combat::auto_click::randomization });

    Setting s_drop; s_drop.type = Setting::SLIDER; s_drop.name = "Drop Chance"; s_drop.floatPtr = &features::combat::auto_click::drop_chance; s_drop.min = 0.0f; s_drop.max = 100.0f; s_drop.format = "%.1f%%";
    s_drop.visibleCondition = [](){ return features::combat::auto_click::randomization; };
    lc.settings.push_back(s_drop);

    Setting s_spike; s_spike.type = Setting::SLIDER; s_spike.name = "Spike Chance"; s_spike.floatPtr = &features::combat::auto_click::spike_chance; s_spike.min = 0.0f; s_spike.max = 100.0f; s_spike.format = "%.1f%%";
    s_spike.visibleCondition = [](){ return features::combat::auto_click::randomization; };
    lc.settings.push_back(s_spike);

    {
        Setting s_cm; s_cm.type = Setting::DROPDOWN; s_cm.name = "Click Mode";
        s_cm.intPtr = &gui_ac_click_method;
        s_cm.dropdownItems = { "Smart", "Jitter", "Butterfly" };
        s_cm.dropdownValues = { 0, 1, 2 };
        lc.settings.push_back(s_cm);
    }
    


    lc.settings.push_back({Setting::TOGGLE, "Click sound", &features::combat::auto_click::click_sound});
    {
        Setting sounds; sounds.type = Setting::DROPDOWN; sounds.name = "Sounds";
        sounds.intPtr = &features::combat::auto_click::sound;
        for (const char* name : features::combat::auto_click::clicksounds::names) sounds.dropdownItems.emplace_back(name);
        sounds.visibleCondition = [] { return features::combat::auto_click::click_sound; };
        lc.settings.push_back(std::move(sounds));
        Setting volume{Setting::SLIDER, "Click Volume", nullptr, &features::combat::auto_click::click_volume, 0.f, 100.f, "%.0f%%"};
        volume.visibleCondition = [] { return features::combat::auto_click::click_sound; };
        lc.settings.push_back(std::move(volume));
    }
    modules.push_back(lc);

    Module aa = CreateMod("Aim Assist", "Aims at targets using customized assistance modes.", CAT_COMBAT, &features::combat::aim_assist::enabled);
    {
        Setting s_mode; s_mode.type = Setting::DROPDOWN; s_mode.name = "Mode";
        s_mode.intPtr = &features::combat::aim_assist::mode;
        s_mode.dropdownItems = { "Regular", "Lock On" };
        aa.settings.push_back(s_mode);
    }
    { Setting s; s.type = Setting::DROPDOWN; s.name = "Aim Mode"; s.intPtr = &features::combat::aim_assist::aim_mode; s.dropdownItems = {"Horizontal","Vertical","Both"}; s.visibleCondition=[](){return features::combat::aim_assist::mode==0;}; aa.settings.push_back(s); }
    { Setting s; s.type = Setting::DROPDOWN; s.name = "Priority"; s.intPtr = &features::combat::aim_assist::priority; s.dropdownItems = {"FOV","Distance","Health"}; aa.settings.push_back(s); }
    { Setting s; s.type = Setting::DROPDOWN; s.name = "Target"; s.intPtr = &features::combat::aim_assist::target_mode; s.dropdownItems = {"Single","Switch"}; s.visibleCondition=[](){return features::combat::aim_assist::mode==0;}; aa.settings.push_back(s); }
    aa.settings.push_back({Setting::SLIDER,"Distance",nullptr,&gui_aa_min_dist,0.f,10.f,"%.1f"});
    aa.settings.push_back({Setting::SLIDER,"Field Of View",nullptr,&gui_aa_min_fov,0.f,360.f,"%.1f"});
    { Setting s{Setting::SLIDER,"Horizontal Speed",nullptr,&gui_aa_horizontal_speed,1.f,100.f,"%.1f"}; s.visibleCondition=[](){return features::combat::aim_assist::mode==0&&(features::combat::aim_assist::aim_mode==0||features::combat::aim_assist::aim_mode==2);}; aa.settings.push_back(s); }
    { Setting s{Setting::SLIDER,"Vertical Speed",nullptr,&gui_aa_vertical_speed,1.f,100.f,"%.1f"}; s.visibleCondition=[](){return features::combat::aim_assist::mode==0&&(features::combat::aim_assist::aim_mode==1||features::combat::aim_assist::aim_mode==2);}; aa.settings.push_back(s); }

    { Setting s; s.type = Setting::MULTIBOX; s.name = "Conditions"; s.multiboxItems = {
        { "Only Click", &features::combat::aim_assist::clicking_only },
        { "Break Blocks", &features::combat::aim_assist::break_blocks },
        { "Weapons Only", &features::combat::aim_assist::weapons_only },
        { "Ignore Invisible", &features::combat::aim_assist::ignore_invisible },
        { "Through Wall", &features::combat::aim_assist::through_walls }
    }; aa.settings.push_back(std::move(s)); }

    modules.push_back(aa);

    Module reach = CreateMod("Reach", "Extends your attack reach distance against players.", CAT_COMBAT, &gui_reach_enabled);
    { Setting s; s.type = Setting::MULTIBOX; s.name = "Conditions"; s.multiboxItems = {
        { "Only on Ground", &gui_reach_ground_only },
        { "Only with Weapon", &gui_reach_weapon_only },
        { "Liquid Check", &gui_reach_liquid_check }
    }; reach.settings.push_back(std::move(s)); }
    reach.settings.push_back({ Setting::TOGGLE, "Combo Mode",        &gui_reach_combo_mode });
    reach.settings.push_back({ Setting::TOGGLE, "Hit Through Walls", &gui_reach_hit_through_walls });
    reach.settings.push_back({ Setting::SLIDER, "Max Reach", nullptr, &gui_reach_max_distance, 3.0f, 6.0f,   "%.2f" });
    reach.settings.push_back({ Setting::SLIDER, "Min Reach", nullptr, &gui_reach_min_distance, 3.0f, 6.0f,   "%.2f" });
    reach.settings.push_back({ Setting::TOGGLE, "Hitbox",   &gui_reach_hitbox_enabled });
    {
        Setting hbs;
        hbs.type = Setting::SLIDER;
        hbs.name = "Hitbox Size";
        hbs.floatPtr = &gui_reach_hitbox_size;
        hbs.min = 0.1f;
        hbs.max = 1.0f;
        hbs.format = "%.2f";
        hbs.visibleCondition = [](){ return gui_reach_hitbox_enabled; };
        reach.settings.push_back(hbs);
    }
    reach.settings.push_back({ Setting::SLIDER, "Chance",   nullptr, &gui_reach_chance,   0.0f, 100.0f, "%.0f%%" });
    modules.push_back(reach);

    Module velo = CreateMod("Velocity", "Reduces or removes incoming knockback force.", CAT_COMBAT, &gui_velo_enabled);
    {
        Setting s_mode; s_mode.type = Setting::DROPDOWN; s_mode.name = "Mode";
        s_mode.intPtr = &gui_velo_mode;
        s_mode.dropdownItems = { "Blatant" };
        velo.settings.push_back(s_mode);
    }
    Setting s_vhor; s_vhor.type = Setting::SLIDER; s_vhor.name = "Horizontal %"; s_vhor.floatPtr = &gui_velo_horizontal; s_vhor.min = 0.0f; s_vhor.max = 100.0f; s_vhor.format = "%.2f";
    s_vhor.visibleCondition = [](){ return gui_velo_mode == 0; };
    velo.settings.push_back(s_vhor);

    Setting s_vver; s_vver.type = Setting::SLIDER; s_vver.name = "Vertical %"; s_vver.floatPtr = &gui_velo_vertical; s_vver.min = 0.0f; s_vver.max = 100.0f; s_vver.format = "%.2f";
    s_vver.visibleCondition = [](){ return gui_velo_mode == 0; };
    velo.settings.push_back(s_vver);

    Setting s_vdel; s_vdel.type = Setting::SLIDER; s_vdel.name = "Delay"; s_vdel.floatPtr = &gui_velo_delay; s_vdel.min = 0.0f; s_vdel.max = 9.0f; s_vdel.format = "%.2f";
    s_vdel.visibleCondition = [](){ return gui_velo_mode == 0; };
    velo.settings.push_back(s_vdel);

    Setting s_vcha; s_vcha.type = Setting::SLIDER; s_vcha.name = "Chance %"; s_vcha.floatPtr = &gui_velo_chance; s_vcha.min = 0.0f; s_vcha.max = 100.0f; s_vcha.format = "%.2f";
    s_vcha.visibleCondition = [](){ return gui_velo_mode == 0; };
    velo.settings.push_back(s_vcha);

    { Setting s; s.type = Setting::MULTIBOX; s.name = "Conditions"; s.multiboxItems = {
        { "Clicking Only", &gui_velo_clicking_only },
        { "Weapons Only", &gui_velo_weapon_only }
    }; velo.settings.push_back(std::move(s)); }
    velo.settings.push_back({ Setting::TOGGLE, "Push Back",     &gui_velo_push_back });
    modules.push_back(velo);



    // -- Sprint movido a Combat --------------------------------
    {
        namespace ts = features::movement::timer_speed;
        Module mod = CreateMod("Timer Speed", "Changes game tick speed while the selected conditions are met.", CAT_MOVEMENT, &ts::enabled);
        mod.settings.push_back({Setting::SLIDER, "Speed", nullptr, &ts::speed, 0.1f, 3.f, "%.2f"});
        mod.settings.push_back({Setting::TOGGLE, "Require Damage", &ts::require_damage});
        mod.settings.push_back({Setting::TOGGLE, "Only Weapon", &ts::only_weapon});
        mod.settings.push_back({Setting::TOGGLE, "Moving", &ts::moving});
        modules.push_back(std::move(mod));
    }
    {
        namespace bh = features::movement::bunnyhop;
        Module mod = CreateMod("BunnyHop", "Automatically hops with configurable horizontal motion.", CAT_MOVEMENT, &bh::enabled);
        mod.settings.push_back({Setting::SLIDER, "Jump Delay", nullptr, &bh::jump_delay, 0.f, 1000.f, "%.0f ms"});
        mod.settings.push_back({Setting::SLIDER, "Jump Height", nullptr, &bh::jump_height, 0.f, 1.f, "%.2f"});
        mod.settings.push_back({Setting::SLIDER, "Power", nullptr, &bh::power, 0.1f, 4.f, "%.2f"});
        mod.settings.push_back({Setting::SLIDER, "Speed Multiplier", nullptr, &bh::speed_multiplier, 0.1f, 3.f, "%.2f"});
        mod.settings.push_back({Setting::SLIDER, "Slow Down Factor", nullptr, &bh::slowdown_factor, 0.f, 1.f, "%.2f"});
        mod.settings.push_back({Setting::SLIDER, "Friction", nullptr, &bh::friction, 2.f, 200.f, "%.1f"});
        mod.settings.push_back({Setting::SLIDER, "Direction Threshold", nullptr, &bh::direction_threshold, 0.1f, 10.f, "%.2f"});
        mod.settings.push_back({Setting::TOGGLE, "Liquid Check", &bh::liquid_check});
        mod.settings.push_back({Setting::TOGGLE, "Only Moving", &bh::only_moving});
        modules.push_back(std::move(mod));
    }

    Module sprint = CreateMod("Sprint", "Automatically sprints for you at all times.", CAT_MOVEMENT, &gui_sprint_enabled);
    modules.push_back(sprint);

    
    // -- ArmorSwitcher (Combat/Player) -- FULL MODULE
    {
        Module as = CreateMod("ArmorSwitcher", "Quickly swaps between armor sets (Diamond/Iron...).", CAT_MISC, &gui_armorswitcher_enabled);
        Setting kit1; kit1.type = Setting::DROPDOWN; kit1.name = "Kit 1";
        kit1.intPtr = &gui_armorswitcher_kit1;
        kit1.dropdownItems = { "Diamond", "Iron", "Gold", "Chain", "Leather" };
        as.settings.push_back(kit1);
        Setting kit2 = kit1;
        kit2.name = "Kit 2";
        kit2.intPtr = &gui_armorswitcher_kit2;
        as.settings.push_back(kit2);
        as.settings.push_back({ Setting::SLIDER, "Delay (ms)", nullptr, &gui_armorswitcher_delay, 20.0f, 300.0f, "%.0f ms" });

        modules.push_back(as);
    }

    // -- Macros (Player) --------------------------------------
    {
        Module mc = CreateMod("Macros", "Automated hotbar macros for Bow, Fireball, Gaps and Pots.", CAT_MISC, &gui_macros_enabled);
        Setting s_mode; s_mode.type = Setting::DROPDOWN; s_mode.name = "Mode";
        s_mode.intPtr = &gui_macros_mode;
        s_mode.dropdownItems = { "Bow", "Fireball", "Gap", "Pot" };
        mc.settings.push_back(s_mode);
        mc.settings.push_back({ Setting::SLIDER, "Switch Delay", nullptr, &gui_macros_switch_delay, 0.0f, 200.0f, "%.0f ms" });
        mc.settings.push_back({ Setting::SLIDER, "Use Delay",    nullptr, &gui_macros_use_delay,    0.0f, 200.0f, "%.0f ms" });
        mc.settings.push_back({ Setting::TOGGLE, "Auto Switch Back", &gui_macros_auto_switch_back });
        modules.push_back(mc);
    }

    {
        namespace rc = features::misc::right_clicker;
        Module mod = CreateMod("Right Clicker", "Clicks the right mouse button at the selected CPS range.", CAT_MISC, &rc::enabled);
        Setting conditions; conditions.type = Setting::MULTIBOX; conditions.name = "Conditions";
        conditions.multiboxItems = {{"Only Click", &rc::only_click}, {"Ignore Obsidian", &rc::ignore_obsidian}, {"Only Blocks", &rc::only_blocks}};
        mod.settings.push_back(std::move(conditions));
        modules.push_back(std::move(mod));
    }
    {
        namespace ba = features::misc::bridge_assist;
        Module mod = CreateMod("Bridge Assist", "Sneaks near block edges. Higher offset allows more overhang.", CAT_MISC, &ba::enabled);
        mod.settings.push_back({Setting::SLIDER, "Edge offset", nullptr, &ba::edge_offset, 0.f, 0.29f, "%.2f"});
        mod.settings.push_back({Setting::SLIDER, "Unsneak delay", nullptr, &ba::unsneak_delay, 0.f, 500.f, "%.0f ms"});
        Setting select; select.type = Setting::DROPDOWN; select.name = "Select blocks"; select.intPtr = &ba::select_blocks;
        select.dropdownItems = {"No", "On depletion", "Always"}; mod.settings.push_back(std::move(select));
        mod.settings.push_back({Setting::TOGGLE, "Randomize", &ba::randomize});
        mod.settings.push_back({Setting::TOGGLE, "Sneak on jump", &ba::sneak_on_jump});
        mod.settings.push_back({Setting::TOGGLE, "Avoid double-sneaking", &ba::avoid_double_sneaking});
        Setting conditions; conditions.type = Setting::MULTIBOX; conditions.name = "Conditions";
        conditions.multiboxItems = {{"Sneak key pressed", &ba::require_sneak}, {"Holding blocks", &ba::holding_blocks},
            {"Looking down", &ba::looking_down}, {"Not moving forward", &ba::not_forward}};
        mod.settings.push_back(std::move(conditions));
        modules.push_back(std::move(mod));
    }

    {
        namespace bard = features::misc::bard_helper;
        Module mod = CreateMod("BardHelper", "Cycles through selected effects in your hotbar once.", CAT_MISC, &bard::enabled);
        mod.settings.push_back({Setting::SLIDER, "Switch Delay", nullptr, &bard::switch_delay, 20.f, 2000.f, "%.0f ms"});
        mod.settings.push_back({Setting::TOGGLE, "Return Last Slot", &bard::return_last_slot});
        Setting effects; effects.type = Setting::MULTIBOX; effects.name = "Effects";
        effects.multiboxItems = {{"Resistance", &bard::resistance}, {"Strength", &bard::strength},
            {"Regeneration", &bard::regeneration}, {"Speed", &bard::speed}, {"Jump", &bard::jump}};
        mod.settings.push_back(std::move(effects));
        modules.push_back(std::move(mod));
    }
    Module refill = CreateMod("Refill", "Automatically refills your hotbar items and soups.", CAT_MISC, &features::combat::refill::enabled);
    refill.settings.push_back({ Setting::SLIDER, "Delay (ms)",    nullptr, &gui_refill_delay,        10.0f, 300.0f, "%.0f ms" });
    modules.push_back(refill);


    // -- BlockHit (Combat) -------------------------------------
    {
        Module bh = CreateMod("BlockHit", "Automatically blocks before hitting for damage reduction and kb.", CAT_COMBAT, &gui_blockhit_enabled);
        Setting s_bmode; s_bmode.type = Setting::DROPDOWN; s_bmode.name = "Mode";
        s_bmode.intPtr = &gui_blockhit_mode;
        s_bmode.dropdownItems = { "Manual", "Predict", "Auto", "Lag" };
        bh.settings.push_back(s_bmode);
        bh.settings.push_back({ Setting::TOGGLE, "Require Mouse Down", &gui_blockhit_require_mouse_down });
        modules.push_back(bh);
    }

    {
    }

    {
        Module esp = CreateMod("ESP", "Renders the adapted player ESP.", CAT_VISUALS, &gui_esp_enabled);
        Setting mode; mode.type = Setting::DROPDOWN; mode.name = "Mode";
        mode.intPtr = &gui_esp_mode;
        mode.dropdownItems = { "2D", "3D" };
        esp.settings.push_back(mode);
        { Setting s; s.type = Setting::DROPDOWN; s.name = "2D Style"; s.intPtr = &gui_esp_2d_style; s.dropdownItems = { "Rounded", "Normal", "Corners" }; s.visibleCondition = [] { return gui_esp_mode == 0; }; esp.settings.push_back(s); }
        { Setting s{ Setting::SLIDER, "Corners", nullptr, &gui_esp_corner_size, 0.10f, 0.50f, "%.2f" }; s.visibleCondition = [] { return gui_esp_mode == 0 && gui_esp_2d_style == 2; }; esp.settings.push_back(s); }
        Setting draw_mode; draw_mode.type = Setting::DROPDOWN; draw_mode.name = "Draw Mode";
        draw_mode.intPtr = &gui_esp_draw_mode;
        draw_mode.dropdownItems = { "Outline", "Fill", "Both" };
        esp.settings.push_back(draw_mode);
        { Setting s{ Setting::COLOR4, "Outline Color", nullptr, nullptr, 0.f, 0.f, "", gui_esp_outline_color }; s.inlineWithPrevious = true; esp.settings.push_back(s); }
        { Setting s; s.type = Setting::DROPDOWN; s.name = "Fill Style"; s.intPtr = &gui_esp_fill_mode; s.dropdownItems = { "Solid", "Gradient" }; esp.settings.push_back(s); }
        { Setting s{ Setting::COLOR4, "Fill Color", nullptr, nullptr, 0.f, 0.f, "", gui_esp_filled_color }; s.visibleCondition = [] { return gui_esp_fill_mode == 0; }; s.inlineWithPrevious = true; esp.settings.push_back(s); }
        { Setting s{ Setting::COLOR4, "Top Color", nullptr, nullptr, 0.f, 0.f, "", gui_esp_fill_gradient_top }; s.visibleCondition = [] { return gui_esp_fill_mode == 1; }; esp.settings.push_back(s); }
        { Setting s{ Setting::COLOR4, "Bottom Color", nullptr, nullptr, 0.f, 0.f, "", gui_esp_fill_gradient_bottom }; s.visibleCondition = [] { return gui_esp_fill_mode == 1; }; esp.settings.push_back(s); }
        esp.settings.push_back({ Setting::COLOR4, "Friend color", nullptr, nullptr, 0.f, 0.f, "", gui_esp_friend_color });
        esp.settings.push_back({ Setting::SLIDER, "Line Thickness", nullptr, &gui_esp_line_thickness, 0.5f, 6.0f, "%.1f" });
        esp.settings.push_back({ Setting::SLIDER, "Render Distance", nullptr, &gui_esp_render_distance, 8.0f, 256.0f, "%.0f" });
        esp.settings.push_back({ Setting::TOGGLE, "Pulse", &gui_esp_pulse });
        { Setting s{ Setting::SLIDER, "Pulse Speed", nullptr, &gui_esp_pulse_speed, 0.10f, 5.0f, "%.2f" }; s.visibleCondition = [] { return gui_esp_pulse; }; esp.settings.push_back(s); }
        { Setting s{ Setting::SLIDER, "Pulse Min Alpha", nullptr, &gui_esp_pulse_min_alpha, 0.0f, 1.0f, "%.2f" }; s.visibleCondition = [] { return gui_esp_pulse; }; esp.settings.push_back(s); }
        { Setting s{ Setting::SLIDER, "Pulse Max Alpha", nullptr, &gui_esp_pulse_max_alpha, 0.0f, 1.0f, "%.2f" }; s.visibleCondition = [] { return gui_esp_pulse; }; esp.settings.push_back(s); }
        esp.settings.push_back({ Setting::TOGGLE, "Health Bar", &gui_esp_healthbar });
        { Setting s{ Setting::COLOR4, "Health Top", nullptr, nullptr, 0.f, 0.f, "", gui_esp_healthbar_gradient_top }; s.visibleCondition = [] { return gui_esp_healthbar && gui_esp_healthbar_style == 1; }; s.inlineWithPrevious = true; esp.settings.push_back(s); }
        { Setting s{ Setting::COLOR4, "Health Bottom", nullptr, nullptr, 0.f, 0.f, "", gui_esp_healthbar_gradient_bottom }; s.visibleCondition = [] { return gui_esp_healthbar && gui_esp_healthbar_style == 1; }; s.inlineWithPrevious = true; esp.settings.push_back(s); }
        { Setting s; s.type = Setting::DROPDOWN; s.name = "Health Bar Position"; s.intPtr = &gui_esp_healthbar_position; s.dropdownItems = { "Left", "Right" }; s.visibleCondition = [] { return gui_esp_healthbar; }; esp.settings.push_back(s); }
        { Setting s; s.type = Setting::DROPDOWN; s.name = "Health Bar Style"; s.intPtr = &gui_esp_healthbar_style; s.dropdownItems = { "HP Based", "Gradient" }; s.visibleCondition = [] { return gui_esp_healthbar; }; esp.settings.push_back(s); }
        { Setting s{ Setting::SLIDER, "Health Bar Thickness", nullptr, &gui_esp_healthbar_thickness, 1.0f, 6.0f, "%.0f" }; s.visibleCondition = [] { return gui_esp_healthbar; }; esp.settings.push_back(s); }
        { Setting s; s.type = Setting::TOGGLE; s.name = "Health Number"; s.boolPtr = &gui_esp_health_number; s.visibleCondition = [] { return gui_esp_healthbar; }; esp.settings.push_back(s); }
        { Setting s{ Setting::SLIDER, "Segments", nullptr, &gui_esp_healthbar_segments, 2.0f, 20.0f, "%.0f" }; s.visibleCondition = [] { return gui_esp_healthbar; }; esp.settings.push_back(s); }
        esp.settings.push_back({ Setting::TOGGLE, "Hurt Effect", &gui_esp_hurt_color });
        { Setting s{ Setting::COLOR4, "Hurt Color", nullptr, nullptr, 0.f, 0.f, "", gui_esp_hurt_effect_color }; s.visibleCondition = [] { return gui_esp_hurt_color; }; s.inlineWithPrevious = true; esp.settings.push_back(s); }
        modules.push_back(esp);
    }

    Module nametags = CreateMod("Nametags", "Displays enhanced custom nametags and health armor stats above players.", CAT_VISUALS, &gui_nametags_enabled);
    nametags.settings.push_back({ Setting::TOGGLE, "Show Name", &gui_nametags_show_name });
    nametags.settings.push_back({ Setting::TOGGLE, "Show Health", &gui_nametags_draw_health });
    { Setting s; s.type = Setting::DROPDOWN; s.name = "Health Format"; s.intPtr = &gui_nametags_health_format; s.dropdownItems = { "HP", "Hearts", "Bar" }; s.visibleCondition = [] { return gui_nametags_draw_health; }; nametags.settings.push_back(s); }
    { Setting s; s.type = Setting::SLIDER; s.name = "Segments"; s.floatPtr = &gui_nametags_health_segments; s.min = 1.f; s.max = 20.f; s.format = "%.0f"; s.visibleCondition = [] { return gui_nametags_draw_health && gui_nametags_health_format == 2; }; nametags.settings.push_back(s); }
    nametags.settings.push_back({ Setting::TOGGLE, "Show Own Nametag", &gui_nametags_show_own });
    nametags.settings.push_back({ Setting::TOGGLE, "Hide Vanilla Nametags", &gui_nametags_hide_vanilla });
    nametags.settings.push_back({ Setting::TOGGLE, "Show Distance", &gui_nametags_draw_distance });
    nametags.settings.push_back({ Setting::TOGGLE, "Show Equipment", &gui_nametags_show_equipment });
    { Setting s; s.type = Setting::TOGGLE; s.name = "Show Enchantments"; s.boolPtr = &gui_nametags_show_enchantments; s.visibleCondition = [] { return gui_nametags_show_equipment; }; nametags.settings.push_back(s); }
    nametags.settings.push_back({ Setting::TOGGLE, "Background",             &gui_nametags_background });    nametags.settings.push_back({ Setting::TOGGLE, "Use Fake Name",          &gui_nametags_use_fake_name });
    nametags.settings.push_back({ Setting::TOGGLE, "Distance Scaling", &gui_nametags_distance_scaling });
    nametags.settings.push_back({ Setting::SLIDER, "Scale", nullptr, &gui_nametags_scale, 0.85f, 4.00f, "%.2f" });
    nametags.settings.push_back({ Setting::COLOR4, "Color", nullptr, nullptr, 0, 0, "", gui_nametags_color });
    modules.push_back(nametags);
    {
        namespace outline = features::visual::outline;
        Module mod = CreateMod("Outline", "Traces player model silhouettes through walls, without filling the model.", CAT_VISUALS, &outline::enabled);
        mod.settings.push_back({ Setting::COLOR4, "Color", nullptr, nullptr, 0.f, 0.f, "", outline::color });
        mod.settings.push_back({ Setting::SLIDER, "Thickness", nullptr, &outline::thickness, 0.5f, 6.f, "%.1f px" });
        mod.settings.push_back({ Setting::TOGGLE, "Glow", &outline::glow });
        Setting intensity{ Setting::SLIDER, "Intensity", nullptr, &outline::strength, 0.f, 1.f, "%.2f" };
        intensity.visibleCondition = [] { return features::visual::outline::glow; };
        mod.settings.push_back(intensity);
        modules.push_back(mod);
    }


    Module tracers = CreateMod("Tracers", "Draws visual indicator tracer lines to every active player.", CAT_VISUALS, &gui_tracers_enabled);
    tracers.settings.push_back({ Setting::TOGGLE, "Draw Distance",          &gui_tracers_draw_distance });
    tracers.settings.push_back({ Setting::SLIDER, "Thickness", nullptr, &gui_tracers_thickness, 0.1f, 5.0f, "%.1f" });
    tracers.settings.push_back({ Setting::COLOR4, "Color", nullptr, nullptr, 0, 0, "", gui_tracers_color_4 });
    modules.push_back(tracers);

    Module hitmarkers = CreateMod("Hit Markers", "Shows a configurable marker only when an attack actually damages a player.", CAT_VISUALS, &features::visual::hit_markers::enabled);
    { Setting s; s.type = Setting::DROPDOWN; s.name = "Mode"; s.intPtr = &features::visual::hit_markers::mode; s.dropdownItems = { "2D", "3D" }; hitmarkers.settings.push_back(s); }
    hitmarkers.settings.push_back({ Setting::COLOR4, "Color", nullptr, nullptr, 0.f, 0.f, "", features::visual::hit_markers::color });
    hitmarkers.settings.push_back({ Setting::SLIDER, "Size", nullptr, &features::visual::hit_markers::size, 3.f, 24.f, "%.0f" });
    hitmarkers.settings.push_back({ Setting::SLIDER, "Line Width", nullptr, &features::visual::hit_markers::line_width, 0.5f, 5.f, "%.1f" });
    hitmarkers.settings.push_back({ Setting::SLIDER, "Duration", nullptr, &features::visual::hit_markers::duration, 0.1f, 2.f, "%.1fs" });
    hitmarkers.settings.push_back({ Setting::TOGGLE, "Fade Out", &features::visual::hit_markers::fade_out });
    hitmarkers.settings.push_back({ Setting::TOGGLE, "Scale Animation", &features::visual::hit_markers::scale_animation });
    { Setting s; s.type = Setting::SLIDER; s.name = "Scale Amount"; s.floatPtr = &features::visual::hit_markers::scale_amount; s.min = 0.5f; s.max = 2.5f; s.format = "%.1fx"; s.visibleCondition = [] { return features::visual::hit_markers::scale_animation; }; hitmarkers.settings.push_back(s); }
    hitmarkers.settings.push_back({ Setting::TOGGLE, "Outline", &features::visual::hit_markers::outline });
    { Setting s; s.type = Setting::COLOR4; s.name = "Outline Color"; s.colorPtr = features::visual::hit_markers::outline_color; s.visibleCondition = [] { return features::visual::hit_markers::outline; }; hitmarkers.settings.push_back(s); }
    { Setting s; s.type = Setting::SLIDER; s.name = "Outline Width"; s.floatPtr = &features::visual::hit_markers::outline_width; s.min = 0.5f; s.max = 3.f; s.format = "%.1f"; s.visibleCondition = [] { return features::visual::hit_markers::outline; }; hitmarkers.settings.push_back(s); }
    modules.push_back(hitmarkers);

    Module arr = CreateMod("Array List", "Displays a sleek minimalist HUD list of enabled modules.", CAT_VISUALS, &gui_arraylist_enabled);
    // --- General ---
    {
        Setting s_render; s_render.type = Setting::DROPDOWN; s_render.name = "Mode";
        s_render.intPtr = &gui_arraylist_color_mode;
        s_render.dropdownItems = { "Single", "Rainbow", "Gradient", "Flow", "GUI Based" };
        arr.settings.push_back(s_render);
    }
    {
        Setting s; s.type = Setting::COLOR; s.name = "Color 1"; s.colorPtr = gui_arraylist_color;
        s.visibleCondition = [] { return gui_arraylist_color_mode != 4; }; // hide on GUI Based
        arr.settings.push_back(s);
    }
    {
        Setting s; s.type = Setting::COLOR; s.name = "Color 2"; s.colorPtr = gui_arraylist_color_b;
        s.visibleCondition = [] { return gui_arraylist_color_mode == 2 || gui_arraylist_color_mode == 3; };
        arr.settings.push_back(s);
    }
    {
        Setting color; color.type = Setting::COLOR; color.name = "Color 3"; color.colorPtr = gui_arraylist_color_c;
        color.visibleCondition = [] { return gui_arraylist_color_mode == 2; };
        arr.settings.push_back(color);
        Setting space{Setting::SLIDER, "Color Space", nullptr, &gui_arraylist_color_space, 20.f, 300.f, "%.0f"};
        space.visibleCondition = [] { return gui_arraylist_color_mode == 2; };
        arr.settings.push_back(space);
    }
    arr.settings.push_back({ Setting::COLOR, "Flag Color", nullptr, nullptr, 0, 0, "", gui_arraylist_info_color });
    {
        Setting s; s.type = Setting::SLIDER; s.name = "Wave Speed"; s.floatPtr = &gui_arraylist_speed;
        s.min = 0.1f; s.max = 3.0f; s.format = "%.2f";
        s.visibleCondition = [] { return gui_arraylist_color_mode != 0 && gui_arraylist_color_mode != 4; }; // hide on Single & GUI Based
        arr.settings.push_back(s);
    }
    // --- Style ---
    // Keep module visibility reachable without scrolling to the panel bottom.
    {
        static int hideModulesIdx = 0;
        Setting s; s.type = Setting::DROPDOWN; s.name = "Hide Modules";
        s.intPtr = &hideModulesIdx;
        s.dropdownItems = { "placeholder" }; // dynamically replaced at render time
        arr.settings.push_back(s);
    }
    arr.settings.push_back({ Setting::SLIDER, "Scale", nullptr, &gui_arraylist_scale, 0.5f, 3.0f, "%.2fx" });
    arr.settings.push_back({ Setting::TOGGLE, "Background", &gui_arraylist_background });
    {
        Setting s; s.type = Setting::TOGGLE; s.name = "Shadow"; s.boolPtr = &gui_arraylist_background_shadow;
        s.visibleCondition = [] { return gui_arraylist_background; };
        arr.settings.push_back(s);
    }
    {
        Setting s; s.type = Setting::SLIDER; s.name = "Shadow Strength"; s.floatPtr = &gui_arraylist_shadow_strength;
        s.min = 0.0f; s.max = 100.0f; s.format = "%.0f%%";
        s.visibleCondition = [] { return gui_arraylist_background && gui_arraylist_background_shadow; };
        arr.settings.push_back(s);
    }
    {
        Setting s; s.type = Setting::TOGGLE; s.name = "Blur"; s.boolPtr = &gui_arraylist_blur;
        s.visibleCondition = [] { return gui_arraylist_background; };
        arr.settings.push_back(s);
    }
    {
        Setting s; s.type = Setting::COLOR4; s.name = "Background Color"; s.colorPtr = gui_arraylist_bg_color_4;
        s.visibleCondition = [] { return gui_arraylist_background; };
        arr.settings.push_back(s);
    }
    arr.settings.push_back({ Setting::TOGGLE, "Side Bar", &gui_arraylist_colorbar });
    {
        Setting s; s.type = Setting::SLIDER; s.name = "Bar Width"; s.floatPtr = &gui_arraylist_bar_width;
        s.min = 0.5f; s.max = 5.0f; s.format = "%.1f";
        s.visibleCondition = [] { return gui_arraylist_colorbar; };
        arr.settings.push_back(s);
    }
    // --- Layout ---
    arr.settings.push_back({ Setting::SLIDER, "Rounding", nullptr, &gui_arraylist_radius, 0.0f, 5.0f, "%.1f" });
    arr.settings.push_back({ Setting::SLIDER, "Horizontal Spacing", nullptr, &gui_arraylist_pad_x, 0.0f, 10.0f, "%.1f" });
    arr.settings.push_back({ Setting::SLIDER, "Vertical Spacing", nullptr, &gui_arraylist_pad_y, 0.0f, 5.0f, "%.1f" });
    // --- Text ---
    arr.settings.push_back({ Setting::TOGGLE, "Text Shadows", &gui_arraylist_shadows });

    arr.settings.push_back({ Setting::TOGGLE, "Lowercase", &gui_arraylist_lowercase });
    arr.settings.push_back({ Setting::TOGGLE, "Show Flags", &gui_arraylist_show_info });
    {
        Setting s; s.type = Setting::TOGGLE; s.name = "Bracket Flags"; s.boolPtr = &gui_arraylist_bracket_flags;
        s.visibleCondition = [] { return gui_arraylist_show_info; };
        arr.settings.push_back(s);
    }
    modules.push_back(arr);


    Module wm = CreateMod("Watermark", "Displays client version and build information on screen.", CAT_VISUALS, &gui_watermark_enabled);
    { Setting s; s.type = Setting::DROPDOWN; s.name = "Elements"; wm.settings.push_back(s); }
    { Setting s; s.type = Setting::DROPDOWN; s.name = "Color Mode"; s.intPtr = &gui_watermark_color_mode; s.dropdownItems = { "Shadow", "Gradient" }; wm.settings.push_back(s); }
    wm.settings.push_back({ Setting::TOGGLE, "Background",   &gui_watermark_background });
    { Setting s{ Setting::TOGGLE, "Blur", &gui_watermark_blur }; s.visibleCondition = [] { return gui_watermark_background; }; wm.settings.push_back(s); }
    { Setting s{ Setting::TOGGLE, "Background Shadow", &gui_watermark_background_shadow }; s.visibleCondition = [] { return gui_watermark_background; }; wm.settings.push_back(s); }
    { Setting s{ Setting::TOGGLE, "Split Background", &gui_watermark_split_background }; s.visibleCondition = [] { return gui_watermark_background; }; wm.settings.push_back(s); }
    wm.settings.push_back({ Setting::TOGGLE, "Text Shadow",  &gui_watermark_text_shadow });
    wm.settings.push_back({ Setting::COLOR, "Color 1", nullptr, nullptr, 0.f, 0.f, "", gui_watermark_color });
    { Setting s; s.type = Setting::COLOR; s.name = "Color 2"; s.colorPtr = gui_watermark_color_b; s.visibleCondition = [] { return gui_watermark_color_mode == 1; }; wm.settings.push_back(s); }
    modules.push_back(wm);

    Module noSlow = CreateMod("NoSlowdown", "Removes movement deceleration while using or consuming items.", CAT_MOVEMENT, &gui_noslow_enabled);
    modules.push_back(noSlow);

    Module noItemRelease = CreateMod("NoItemRelease", "Keeps items actively used without holding right click.", CAT_MOVEMENT, &gui_noitemrelease_enabled);
    { Setting items; items.type = Setting::MULTIBOX; items.name = "Items";
      items.multiboxItems = {{"Food", &gui_noitemrelease_food}, {"Potions", &features::movement::no_item_release::potions},
        {"Sword", &features::movement::no_item_release::sword}, {"Bow", &features::movement::no_item_release::bow}};
      noItemRelease.settings.push_back(std::move(items)); }
    modules.push_back(noItemRelease);

    Module snapTap = CreateMod("SnapTap", "Keeps the most recently pressed direction active when opposite movement keys overlap.", CAT_MOVEMENT, &features::movement::snap_tap::enabled);
    modules.push_back(snapTap);

    Module instantStop = CreateMod("InstantStop", "Reduces horizontal momentum as soon as movement input is released.", CAT_MOVEMENT, &features::movement::instant_stop::enabled);
    instantStop.settings.push_back({ Setting::TOGGLE, "Only On Ground", &features::movement::instant_stop::only_on_ground });
    instantStop.settings.push_back({ Setting::TOGGLE, "Stop On Sneak", &features::movement::instant_stop::stop_on_sneak });
    instantStop.settings.push_back({ Setting::SLIDER, "Stop Strength", nullptr, &features::movement::instant_stop::stop_strength, 0.0f, 1.0f, "%.2f" });
    modules.push_back(instantStop);


    // -- FastPlace (Movement) ---------------------------------
    {
        Module fp = CreateMod("FastPlace", "Places blocks at supercharged speed with zero tick delay.", CAT_MISC, &gui_fastplace_enabled);
        Setting s_held; s_held.type = Setting::DROPDOWN; s_held.name = "Held Item";
        s_held.intPtr = &gui_fastplace_held_item;
        s_held.dropdownItems = { "All", "Blocks", "Projectiles" };
        fp.settings.push_back(s_held);
        modules.push_back(fp);
    }

    {
        Module aa = CreateMod("AutoArmor", "Automatically equips the highest defense armor in inventory.", CAT_MISC, &gui_autoarmor_enabled);
        aa.settings.push_back({ Setting::SLIDER, "Delay (ms)", nullptr, &gui_autoarmor_delay, 0.0f, 500.0f, "%.0f ms" });
        aa.settings.push_back({ Setting::TOGGLE, "Only Better",  &gui_autoarmor_only_better });
        modules.push_back(aa);
    }

    Module blink = CreateMod("Blink", "Simulates temporary network freeze to instantly teleport.", CAT_LATENCY, &gui_blink_enabled);
    {
        Setting s_mode; s_mode.type = Setting::DROPDOWN; s_mode.name = "Mode";
        s_mode.intPtr = &features::latency::blink::mode;
        s_mode.dropdownItems = { "Smooth", "Freeze" };
        blink.settings.push_back(s_mode);
    }
    blink.settings.push_back({ Setting::TOGGLE, "Show Path",  &gui_blink_show_path });
    blink.settings.push_back({ Setting::TOGGLE, "Show Timer", &gui_blink_show_timer });
    blink.settings.push_back({ Setting::COLOR,  "Path Color", nullptr, nullptr, 0, 0, "", gui_blink_path_color });
    blink.settings.push_back({ Setting::SLIDER, "Timer Limit (s)", nullptr, &gui_blink_timer_limit, 1.0f, 20.0f, "%.1f" });
    modules.push_back(blink);

    {
        Module dr = CreateMod("Delay Remover", "Removes various game delays.", CAT_MISC, nullptr);
        dr.settings.push_back({ Setting::TOGGLE, "No Hit Delay", &gui_nohitdelay_enabled });
        dr.settings.push_back({ Setting::TOGGLE, "No Jump Delay", &gui_nojumpdelay_enabled });
        modules.push_back(dr);


        Module friends = CreateMod("Friends", "Manages players excluded by combat and visual modules.", CAT_MISC, &gui_friends_enabled);
        modules.push_back(friends);
        
    }

    for (auto& module : modules) {
        if (module.name == "Timer Speed" || module.name == "BunnyHop" || module.name == "InstantStop" || module.name == "instantStop") {
            Setting conditions; conditions.type = Setting::MULTIBOX; conditions.name = "Conditions";
            for (auto it = module.settings.begin(); it != module.settings.end();) {
                if (it->type == Setting::TOGGLE) {
                    conditions.multiboxItems.emplace_back(it->name == "Moving" ? "Only While Moving" : it->name, it->boolPtr);
                    it = module.settings.erase(it);
                } else ++it;
            }
            module.settings.push_back(std::move(conditions));
        }
        std::stable_partition(module.settings.begin(), module.settings.end(),
            [](const Setting& setting) { return setting.name != "Conditions"; });
    }
    if (restore_module_defaults.empty()) {
        for (const auto& module : modules) for (const auto& setting : module.settings) {
            if (setting.boolPtr) { auto p=setting.boolPtr; const bool v=*p; restore_module_defaults.push_back([p,v]{*p=v;}); }
            if (setting.floatPtr) { auto p=setting.floatPtr; const float v=*p; restore_module_defaults.push_back([p,v]{*p=v;}); }
            if (setting.intPtr) { auto p=setting.intPtr; const int v=*p; restore_module_defaults.push_back([p,v]{*p=v;}); }
            if (setting.colorPtr) for (int i=0;i<(setting.type==Setting::COLOR4?4:3);++i) {
                auto p=setting.colorPtr+i; const float v=*p; restore_module_defaults.push_back([p,v]{*p=v;});
            }
            for (const auto& item : setting.multiboxItems) {
                auto p=item.second; if (!p) continue; const bool v=*p;
                restore_module_defaults.push_back([p,v]{*p=v;});
            }
        }
    }
    for (auto& m : modules) g_KeybindMap[m.name] = m.keybind;
    SelectFirstModuleInCategory(CAT_COMBAT);
}

// ============================================================
// RENDER MENU CONTENTS
// ============================================================
#include "../altmanager/altmanager.cpp"

#include "../w_imgui_port/swift_adapter.inc"
#include "../w_imgui_port/swift_adapter_extras.inc"

void RenderMenuContents()
{
    const float step = ImClamp(ImGui::GetIO().DeltaTime * 5.0f, 0.0f, 1.0f);
    g_MenuOpenAnim = ImClamp(g_MenuOpenAnim + (g_MenuVisible ? step : -step), 0.0f, 1.0f);
    if (g_AltManagerMode) RenderSwiftAltManager();
    else swift_w_port::render();
    return;
}
