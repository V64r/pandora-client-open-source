#include "altmanager.h"
#include "alt_auth.hh"

bool g_AltManagerMode = false;
std::atomic<int> g_AltManagerAction{ 0 };
std::atomic<bool> g_AltManagerRefocusInput{ false };
char g_AltManagerPendingName[17] = {};
std::string g_AltManagerPremiumName = "Unknown";
std::string g_AltManagerCurrentName = "Unknown";
std::string g_AltManagerStatus;
std::mutex g_AltManagerStateMutex;
jobject g_AltManagerPremiumSession = nullptr;
std::vector<std::pair<jfieldID, jobject>> g_AltManagerPremiumStringFields;
static std::atomic<bool> g_AltAuthBusy{ false };
static HANDLE g_AltAuthThread = nullptr;
static alt_auth::account g_AltManagerPendingAccount;

void ShutdownAltAuthWorker() {
    if (!g_AltAuthThread) return;
    WaitForSingleObject(g_AltAuthThread, INFINITE);
    CloseHandle(g_AltAuthThread);
    g_AltAuthThread = nullptr;
}

static std::string AltManagerReadJavaString(JNIEnv* env, jstring value) {
    if (!env || !value) return {};
    const char* utf = env->GetStringUTFChars(value, nullptr);
    if (!utf) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        return {};
    }
    std::string result(utf);
    env->ReleaseStringUTFChars(value, utf);
    return result;
}

static std::string AltManagerGetClassName(JNIEnv* env, jobject object) {
    if (!env || !object) return {};
    jclass objectClass = env->GetObjectClass(object);
    jclass classClass = env->FindClass("java/lang/Class");
    if (!objectClass || !classClass) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (objectClass) env->DeleteLocalRef(objectClass);
        if (classClass) env->DeleteLocalRef(classClass);
        return {};
    }
    jmethodID getName = env->GetMethodID(classClass, "getName", "()Ljava/lang/String;");
    jstring name = getName ? (jstring)env->CallObjectMethod(objectClass, getName) : nullptr;
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        name = nullptr;
    }
    std::string result = AltManagerReadJavaString(env, name);
    if (name) env->DeleteLocalRef(name);
    env->DeleteLocalRef(classClass);
    env->DeleteLocalRef(objectClass);
    return result;
}

static bool AltManagerIsSessionClass(JNIEnv* env, jclass klass) {
    if (!env || !klass || !mapper::jvmti) return false;
    jmethodID ctor = env->GetMethodID(klass, "<init>",
        "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V");
    if (!ctor) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        ctor = env->GetMethodID(klass, "<init>",
            "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V");
    }
    if (!ctor) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        return false;
    }
    jint count = 0;
    jfieldID* fields = nullptr;
    if (mapper::jvmti->GetClassFields(klass, &count, &fields) != JVMTI_ERROR_NONE || !fields)
        return false;
    int stringFields = 0;
    for (jint i = 0; i < count; ++i) {
        char* name = nullptr;
        char* signature = nullptr;
        char* generic = nullptr;
        mapper::jvmti->GetFieldName(klass, fields[i], &name, &signature, &generic);
        if (signature && strcmp(signature, "Ljava/lang/String;") == 0)
            ++stringFields;
        if (name) mapper::jvmti->Deallocate((unsigned char*)name);
        if (signature) mapper::jvmti->Deallocate((unsigned char*)signature);
        if (generic) mapper::jvmti->Deallocate((unsigned char*)generic);
    }
    mapper::jvmti->Deallocate((unsigned char*)fields);
    return stringFields >= 3 && stringFields <= 5;
}

bool AltManagerFindSession(JNIEnv* env, jobject minecraft,
                                  jfieldID& outField, jobject& outSession) {
    outField = nullptr;
    outSession = nullptr;
    if (!env || !minecraft || !mapper::jvmti) return false;
    jclass mcClass = env->GetObjectClass(minecraft);
    if (!mcClass) return false;
    jint count = 0;
    jfieldID* fields = nullptr;
    if (mapper::jvmti->GetClassFields(mcClass, &count, &fields) != JVMTI_ERROR_NONE || !fields) {
        env->DeleteLocalRef(mcClass);
        return false;
    }
    for (jint i = 0; i < count && !outSession; ++i) {
        char* name = nullptr;
        char* signature = nullptr;
        char* generic = nullptr;
        mapper::jvmti->GetFieldName(mcClass, fields[i], &name, &signature, &generic);
        jint modifiers = 0;
        mapper::jvmti->GetFieldModifiers(mcClass, fields[i], &modifiers);
        const bool objectField = signature && signature[0] == 'L' && (modifiers & 0x0008) == 0;
        if (objectField) {
            jobject candidate = env->GetObjectField(minecraft, fields[i]);
            if (env->ExceptionCheck()) {
                env->ExceptionClear();
                candidate = nullptr;
            }
            if (candidate) {
                jclass candidateClass = env->GetObjectClass(candidate);
                const bool knownName = name &&
                    (strcmp(name, "session") == 0 || strcmp(name, "field_71449_j") == 0);
                const std::string candidateClassName = AltManagerGetClassName(env, candidate);
                const bool namedSessionClass =
                    candidateClassName.find("Session") != std::string::npos ||
                    candidateClassName.find("session") != std::string::npos;
                // The old constructor-only heuristic also matched unrelated
                // Lunar objects and could display arbitrary strings as the IGN.
                if (knownName || (namedSessionClass && AltManagerIsSessionClass(env, candidateClass))) {
                    outField = fields[i];
                    outSession = candidate;
                } else {
                    env->DeleteLocalRef(candidate);
                }
                if (candidateClass) env->DeleteLocalRef(candidateClass);
            }
        }
        if (name) mapper::jvmti->Deallocate((unsigned char*)name);
        if (signature) mapper::jvmti->Deallocate((unsigned char*)signature);
        if (generic) mapper::jvmti->Deallocate((unsigned char*)generic);
    }
    mapper::jvmti->Deallocate((unsigned char*)fields);
    env->DeleteLocalRef(mcClass);
    return outField != nullptr && outSession != nullptr;
}

static std::string AltManagerGetSessionName(JNIEnv* env, jobject session) {
    if (!env || !session || !mapper::jvmti) return {};
    jclass sessionClass = env->GetObjectClass(session);
    if (!sessionClass) return {};
    const char* methodNames[] = { "getUsername", "func_111285_a", "a" };
    for (const char* methodName : methodNames) {
        jmethodID method = env->GetMethodID(sessionClass, methodName, "()Ljava/lang/String;");
        if (!method) {
            if (env->ExceptionCheck()) env->ExceptionClear();
            continue;
        }
        jstring value = (jstring)env->CallObjectMethod(session, method);
        if (!env->ExceptionCheck() && value) {
            std::string result = AltManagerReadJavaString(env, value);
            env->DeleteLocalRef(value);
            env->DeleteLocalRef(sessionClass);
            if (!result.empty()) return result;
        } else if (env->ExceptionCheck()) {
            env->ExceptionClear();
        }
    }

    jint count = 0;
    jfieldID* fields = nullptr;
    if (mapper::jvmti->GetClassFields(sessionClass, &count, &fields) == JVMTI_ERROR_NONE && fields) {
        for (jint i = 0; i < count; ++i) {
            char* name = nullptr;
            char* signature = nullptr;
            char* generic = nullptr;
            mapper::jvmti->GetFieldName(sessionClass, fields[i], &name, &signature, &generic);
            const bool isString = signature && strcmp(signature, "Ljava/lang/String;") == 0;
            if (isString) {
                jstring value = (jstring)env->GetObjectField(session, fields[i]);
                if (!env->ExceptionCheck() && value) {
                    std::string result = AltManagerReadJavaString(env, value);
                    env->DeleteLocalRef(value);
                    if (name) mapper::jvmti->Deallocate((unsigned char*)name);
                    if (signature) mapper::jvmti->Deallocate((unsigned char*)signature);
                    if (generic) mapper::jvmti->Deallocate((unsigned char*)generic);
                    mapper::jvmti->Deallocate((unsigned char*)fields);
                    env->DeleteLocalRef(sessionClass);
                    return result;
                }
                if (env->ExceptionCheck()) env->ExceptionClear();
            }
            if (name) mapper::jvmti->Deallocate((unsigned char*)name);
            if (signature) mapper::jvmti->Deallocate((unsigned char*)signature);
            if (generic) mapper::jvmti->Deallocate((unsigned char*)generic);
        }
        mapper::jvmti->Deallocate((unsigned char*)fields);
    }
    env->DeleteLocalRef(sessionClass);
    return {};
}

bool AltManagerSetSessionName(JNIEnv* env, jobject session, const char* username) {
    if (!env || !session || !username || !username[0] || !mapper::jvmti) return false;
    jclass sessionClass = env->GetObjectClass(session);
    if (!sessionClass) return false;

    const std::string oldName = AltManagerGetSessionName(env, session);
    jint count = 0;
    jfieldID* fields = nullptr;
    if (mapper::jvmti->GetClassFields(sessionClass, &count, &fields) != JVMTI_ERROR_NONE || !fields) {
        env->DeleteLocalRef(sessionClass);
        return false;
    }

    jfieldID nameField = nullptr;
    for (jint i = 0; i < count && !nameField; ++i) {
        char* name = nullptr;
        char* signature = nullptr;
        char* generic = nullptr;
        mapper::jvmti->GetFieldName(sessionClass, fields[i], &name, &signature, &generic);
        if (signature && strcmp(signature, "Ljava/lang/String;") == 0) {
            jstring value = (jstring)env->GetObjectField(session, fields[i]);
            if (env->ExceptionCheck()) {
                env->ExceptionClear();
                value = nullptr;
            }
            const std::string text = AltManagerReadJavaString(env, value);
            if (value) env->DeleteLocalRef(value);
            const bool knownField = name &&
                (strcmp(name, "username") == 0 || strcmp(name, "field_74286_b") == 0);
            if (knownField || (!oldName.empty() && text == oldName))
                nameField = fields[i];
        }
        if (name) mapper::jvmti->Deallocate((unsigned char*)name);
        if (signature) mapper::jvmti->Deallocate((unsigned char*)signature);
        if (generic) mapper::jvmti->Deallocate((unsigned char*)generic);
    }
    mapper::jvmti->Deallocate((unsigned char*)fields);

    bool success = false;
    if (nameField) {
        jstring newName = env->NewStringUTF(username);
        env->SetObjectField(session, nameField, newName);
        success = !env->ExceptionCheck();
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (newName) env->DeleteLocalRef(newName);
    }
    env->DeleteLocalRef(sessionClass);
    return success;
}

static bool AltManagerApplyAuthenticatedSession(JNIEnv* env, jobject minecraft,
                                                jfieldID sessionField, jobject currentSession,
                                                const alt_auth::account& account) {
    if (!env || !minecraft || !sessionField || !currentSession ||
        account.username.empty() || account.uuid.empty() || account.access_token.empty()) return false;
    jclass sessionClass = env->GetObjectClass(currentSession);
    if (!sessionClass) return false;
    jmethodID ctor = env->GetMethodID(sessionClass, "<init>",
        "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V");
    if (!ctor && env->ExceptionCheck()) env->ExceptionClear();
    if (!ctor) { env->DeleteLocalRef(sessionClass); return false; }

    jstring username = env->NewStringUTF(account.username.c_str());
    jstring uuid = env->NewStringUTF(account.uuid.c_str());
    jstring token = env->NewStringUTF(account.access_token.c_str());
    jstring type = env->NewStringUTF("mojang");
    jobject replacement = env->NewObject(sessionClass, ctor, username, uuid, token, type);
    bool success = replacement && !env->ExceptionCheck();
    if (success) {
        env->SetObjectField(minecraft, sessionField, replacement);
        success = !env->ExceptionCheck();
    }
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (replacement) env->DeleteLocalRef(replacement);
    if (username) env->DeleteLocalRef(username);
    if (uuid) env->DeleteLocalRef(uuid);
    if (token) env->DeleteLocalRef(token);
    if (type) env->DeleteLocalRef(type);
    env->DeleteLocalRef(sessionClass);
    return success;
}

static bool AltManagerCapturePremiumSession(JNIEnv* env, jobject session) {
    if (!env || !session || !mapper::jvmti || !g_AltManagerPremiumStringFields.empty()) return false;
    jclass sessionClass = env->GetObjectClass(session);
    if (!sessionClass) return false;

    jint count = 0;
    jfieldID* fields = nullptr;
    if (mapper::jvmti->GetClassFields(sessionClass, &count, &fields) != JVMTI_ERROR_NONE || !fields) {
        env->DeleteLocalRef(sessionClass);
        return false;
    }

    for (jint i = 0; i < count; ++i) {
        char* name = nullptr;
        char* signature = nullptr;
        char* generic = nullptr;
        mapper::jvmti->GetFieldName(sessionClass, fields[i], &name, &signature, &generic);
        if (signature && strcmp(signature, "Ljava/lang/String;") == 0) {
            jobject value = env->GetObjectField(session, fields[i]);
            if (env->ExceptionCheck()) {
                env->ExceptionClear();
                value = nullptr;
            }
            jobject savedValue = value ? env->NewGlobalRef(value) : nullptr;
            g_AltManagerPremiumStringFields.emplace_back(fields[i], savedValue);
            if (value) env->DeleteLocalRef(value);
        }
        if (name) mapper::jvmti->Deallocate((unsigned char*)name);
        if (signature) mapper::jvmti->Deallocate((unsigned char*)signature);
        if (generic) mapper::jvmti->Deallocate((unsigned char*)generic);
    }
    mapper::jvmti->Deallocate((unsigned char*)fields);
    env->DeleteLocalRef(sessionClass);
    return !g_AltManagerPremiumStringFields.empty();
}

bool AltManagerRestorePremiumSession(JNIEnv* env, jobject session) {
    if (!env || !session || g_AltManagerPremiumStringFields.empty()) return false;
    bool success = true;
    for (const auto& savedField : g_AltManagerPremiumStringFields) {
        env->SetObjectField(session, savedField.first, savedField.second);
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            success = false;
        }
    }
    return success;
}

void AltManagerUpdateOnGameThread(JNIEnv* env, jobject minecraft, jobject currentScreen) {
    static ULONGLONG nextScreenCheck = 0;
    static std::string hostScreenName;
    const ULONGLONG now = GetTickCount64();
    const std::string currentScreenName = AltManagerGetClassName(env, currentScreen);
    g_OnMinecraftMainMenu.store(
        currentScreenName.find("GuiMainMenu") != std::string::npos ||
        currentScreenName.find("TitleScreen") != std::string::npos ||
        currentScreenName.find("MainMenuScreen") != std::string::npos,
        std::memory_order_release);
    g_ChatOpen.store(currentScreenName.find("GuiChat") != std::string::npos ||
                     currentScreenName.find("ChatScreen") != std::string::npos,
                     std::memory_order_release);

    // Escape closes Alt Manager while still on GuiMultiplayer and deliberately
    // leaves the native cursor available. Once Minecraft enters a world there
    // is no screen anymore, so return cursor ownership to LWJGL even though Alt
    // Manager mode was already cleared earlier.
    if (!currentScreen && !g_MenuVisible &&
        s_DeferredCursorRelease.load(std::memory_order_acquire)) {
        ReleaseMenuCursorForGameplay();
    }
    if (g_AltManagerMode && g_MenuVisible) {
        if (hostScreenName.empty()) {
            hostScreenName = currentScreenName;
        } else if (!currentScreen || currentScreenName != hostScreenName) {
            g_MenuVisible = false;
            g_AltManagerMode = false;
            hostScreenName.clear();
            MenuClose(false);
        }
    } else if (!g_AltManagerMode) {
        hostScreenName.clear();
    }
    if (now >= nextScreenCheck) {
        nextScreenCheck = now + 250;
        g_OnMultiplayerScreen.store(currentScreenName.find("GuiMultiplayer") != std::string::npos ||
                                    currentScreenName.find("MultiplayerScreen") != std::string::npos ||
                                    currentScreenName.find("ServerSelection") != std::string::npos ||
                                    currentScreenName.find("SelectServer") != std::string::npos,
                                    std::memory_order_release);
    }

    if (g_AltManagerPremiumSession &&
        g_AltManagerAction.load(std::memory_order_acquire) == 0)
        return;

    static jfieldID cachedSessionField = nullptr;
    jfieldID sessionField = nullptr;
    jobject currentSession = nullptr;
    if (cachedSessionField) {
        sessionField = cachedSessionField;
        currentSession = env->GetObjectField(minecraft, sessionField);
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            currentSession = nullptr;
            cachedSessionField = nullptr;
        }
    }
    if (!currentSession) {
        if (!AltManagerFindSession(env, minecraft, sessionField, currentSession)) return;
        cachedSessionField = sessionField;
    }
    if (!g_AltManagerPremiumSession) {
        g_AltManagerPremiumSession = env->NewGlobalRef(currentSession);
        AltManagerCapturePremiumSession(env, currentSession);
        const std::string premiumName = AltManagerGetSessionName(env, currentSession);
        if (!premiumName.empty()) {
            std::lock_guard<std::mutex> lock(g_AltManagerStateMutex);
            g_AltManagerPremiumName = premiumName;
            g_AltManagerCurrentName = premiumName;
        }
    }

    const int action = g_AltManagerAction.exchange(0, std::memory_order_acq_rel);
    if (action == 1) {
        // Keep Lunar's authenticated session object and token intact. Replacing
        // it with an empty legacy session makes Lunar start Microsoft sign-in.
        if (AltManagerSetSessionName(env, currentSession, g_AltManagerPendingName)) {
            std::lock_guard<std::mutex> lock(g_AltManagerStateMutex);
            g_AltManagerCurrentName = g_AltManagerPendingName;
            g_AltManagerStatus = "Added successfully.";
        } else {
            std::lock_guard<std::mutex> lock(g_AltManagerStateMutex);
            g_AltManagerStatus = "Could not apply the offline session.";
        }
    } else if (action == 2 && g_AltManagerPremiumSession) {
        // Restore every captured authenticated field, then ALWAYS force the
        // original premium username. The previous short-circuit (`fields ||
        // name`) skipped the name write whenever restoring the other fields
        // succeeded, which could leave a previously selected offline IGN.
        AltManagerRestorePremiumSession(env, currentSession);
        AltManagerSetSessionName(env, currentSession, g_AltManagerPremiumName.c_str());
        const std::string restoredName = AltManagerGetSessionName(env, currentSession);
        const bool restored = !g_AltManagerPremiumName.empty() &&
            g_AltManagerPremiumName != "Unknown" &&
            restoredName == g_AltManagerPremiumName;
        if (!restored) {
            std::lock_guard<std::mutex> lock(g_AltManagerStateMutex);
            g_AltManagerStatus = "Could not restore the premium session.";
        } else {
            std::lock_guard<std::mutex> lock(g_AltManagerStateMutex);
            g_AltManagerCurrentName = g_AltManagerPremiumName;
            g_AltManagerStatus = "Restored successfully.";
        }
        g_AltManagerRefocusInput.store(true, std::memory_order_release);
    } else if (action == 3) {
        alt_auth::account pending;
        {
            std::lock_guard<std::mutex> lock(g_AltManagerStateMutex);
            pending = g_AltManagerPendingAccount;
        }
        const bool applied = AltManagerApplyAuthenticatedSession(
            env, minecraft, sessionField, currentSession, pending);
        {
            std::lock_guard<std::mutex> lock(g_AltManagerStateMutex);
            if (applied) {
                g_AltManagerCurrentName = pending.username;
                g_AltManagerStatus = "Logged in successfully as " + pending.username + ".";
            } else {
                g_AltManagerStatus = "Authenticated, but the Minecraft session could not be replaced.";
            }
        }
    }
    env->DeleteLocalRef(currentSession);
}


static bool AltManagerIsValidOfflineName(const char* name) {
    if (!name) return false;
    const size_t length = strlen(name);
    if (length < 3 || length > 16) return false;
    for (size_t i = 0; i < length; ++i) {
        const unsigned char ch = (unsigned char)name[i];
        if (!std::isalnum(ch) && ch != '_') return false;
    }
    return true;
}

struct AltAuthRequest {
    alt_auth::method method;
    std::string input;
};

static void AltManagerSetStatus(const std::string& status) {
    std::lock_guard<std::mutex> lock(g_AltManagerStateMutex);
    g_AltManagerStatus = status;
}

static DWORD WINAPI AltManagerAuthThread(LPVOID parameter) {
    std::unique_ptr<AltAuthRequest> request(static_cast<AltAuthRequest*>(parameter));
    std::vector<alt_auth::result> results;
    const auto progress = [](const std::string& text) { AltManagerSetStatus(text); };
    if (request->method == alt_auth::method::cookie) {
        results.push_back(alt_auth::login_cookie_text(request->input, progress));
    } else {
        const auto credentials = alt_auth::split_credentials(request->input, request->method);
        for (size_t i = 0; i < credentials.size(); ++i) {
            AltManagerSetStatus("Processing account " + std::to_string(i + 1) + " of " +
                std::to_string(credentials.size()) + "...");
            results.push_back(request->method == alt_auth::method::access_token
                ? alt_auth::login_access_token(credentials[i], progress)
                : alt_auth::login_refresh_token(credentials[i], progress));
        }
    }
    alt_auth::secure_clear(request->input);

    int succeeded = 0;
    std::string last_error;
    alt_auth::account last_account;
    {
        std::lock_guard<std::mutex> lock(g_AltManagerStateMutex);
        for (auto& result : results) {
            if (!result.success) { last_error = result.error; continue; }
            ++succeeded; last_account = result.value;
        }
        if (succeeded) {
            g_AltManagerPendingAccount = last_account;
            g_AltManagerStatus = succeeded == 1 ? "Authentication completed." :
                std::to_string(succeeded) + " accounts imported. Applying the last one...";
        } else {
            g_AltManagerStatus = last_error.empty() ? "No valid credentials were found." : last_error;
        }
    }
    if (succeeded) {
        g_AltManagerAction.store(3, std::memory_order_release);
    }
    g_AltAuthBusy.store(false, std::memory_order_release);
    return 0;
}

static bool AltManagerStartAuthentication(alt_auth::method method,
                                          const std::string& input) {
    if (g_AltAuthBusy.exchange(true, std::memory_order_acq_rel)) return false;
    if (g_AltAuthThread && WaitForSingleObject(g_AltAuthThread, 0) == WAIT_OBJECT_0) {
        CloseHandle(g_AltAuthThread); g_AltAuthThread = nullptr;
    }
    auto* request = new AltAuthRequest{ method, input };
    g_AltAuthThread = CreateThread(nullptr, 0, AltManagerAuthThread, request, 0, nullptr);
    if (!g_AltAuthThread) {
        alt_auth::secure_clear(request->input); delete request;
        g_AltAuthBusy.store(false, std::memory_order_release);
        return false;
    }
    return true;
}

static std::wstring AltManagerChooseCookieFile() {
    wchar_t path[MAX_PATH]{};
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = g_GameWindow;
    dialog.lpstrFilter = L"Cookie files (*.txt;*.json;*.cookies)\0*.txt;*.json;*.cookies\0All files\0*.*\0";
    dialog.lpstrFile = path;
    dialog.nMaxFile = MAX_PATH;
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    return GetOpenFileNameW(&dialog) ? std::wstring(path) : std::wstring();
}

static bool AltManagerSubmitInput(std::string& value) {
    const size_t first = value.find_first_not_of(" \t\r\n");
    const size_t last = value.find_last_not_of(" \t\r\n");
    value = first == std::string::npos ? std::string() : value.substr(first, last - first + 1);

    const bool offlineName = AltManagerIsValidOfflineName(value.c_str());
    const bool refreshToken = value.rfind("M.C", 0) == 0 || value.find("\nM.C") != std::string::npos;
    const bool jwtToken = value.rfind("eyJ", 0) == 0 &&
        std::count(value.begin(), value.end(), '.') >= 2;
    const bool cookieText = value.find("# Netscape HTTP Cookie File") != std::string::npos ||
        value.find("#HttpOnly_") != std::string::npos ||
        (value.find("\"name\"") != std::string::npos && value.find("\"value\"") != std::string::npos) ||
        (value.find('\t') != std::string::npos && value.find("live.com") != std::string::npos);

    bool accepted = false;
    if (value.empty()) {
        AltManagerSetStatus("Enter a username, token, or cookie data.");
    } else if (offlineName) {
        strncpy_s(g_AltManagerPendingName, value.c_str(), _TRUNCATE);
        g_AltManagerAction.store(1, std::memory_order_release);
        AltManagerSetStatus("Applying offline session...");
        accepted = true;
    } else if (cookieText || refreshToken || jwtToken) {
        const alt_auth::method method = cookieText ? alt_auth::method::cookie :
            (refreshToken ? alt_auth::method::refresh_token : alt_auth::method::access_token);
        accepted = AltManagerStartAuthentication(method, value);
    } else {
        AltManagerSetStatus("Input was not recognized as a username or token.");
    }

    alt_auth::secure_clear(value);
    if (accepted) value.clear();
    return accepted;
}

static void AltManagerSelectCookie() {
    if (g_AltAuthBusy.load(std::memory_order_acquire)) return;
    const std::wstring cookiePath = AltManagerChooseCookieFile();
    if (cookiePath.empty()) return;

    std::ifstream cookieFile(cookiePath, std::ios::binary);
    if (!cookieFile) {
        AltManagerSetStatus("Could not open the selected cookie file.");
        return;
    }
    std::string cookieContent((std::istreambuf_iterator<char>(cookieFile)), {});
    if (cookieContent.empty()) AltManagerSetStatus("The selected cookie file is empty.");
    else AltManagerStartAuthentication(alt_auth::method::cookie, cookieContent);
    alt_auth::secure_clear(cookieContent);
}

static void RenderSwiftAltManager() {
    ImGuiIO& io = ImGui::GetIO();
    g_ConfigInputActive = false;

    constexpr float width = 520.f;
    constexpr float height = 205.f;
    constexpr float rounding = 15.f;
    ImGui::GetBackgroundDrawList()->AddRectFilled(
        ImVec2(0.f, 0.f), io.DisplaySize, IM_COL32(0, 0, 0, 255));
    ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_Always);
    ImGui::SetNextWindowPos(ImVec2((io.DisplaySize.x - width) * 0.5f,
        (io.DisplaySize.y - height) * 0.5f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 1.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20.f, 20.f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, rounding);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 7.f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10.f, 10.f));
    FONT_MANAGER.push_ui_font();

    ImGui::Begin("SwiftAltManager", nullptr, ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBackground);
    ImGui::SetMouseCursor(ImGuiMouseCursor_Arrow);

    const ImVec2 windowPos = ImGui::GetWindowPos();
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const ImVec2 panelMin(windowPos.x + 1.f, windowPos.y + 1.f);
    const ImVec2 panelMax(windowPos.x + width - 1.f, windowPos.y + height - 1.f);
    draw->AddRectFilled(panelMin, panelMax, IM_COL32(17, 17, 17, 255), rounding);
    draw->AddRect(panelMin, panelMax, IM_COL32(7, 7, 7, 255), rounding, 0, 1.5f);

    std::string currentName;
    std::string status;
    {
        std::lock_guard<std::mutex> lock(g_AltManagerStateMutex);
        currentName = g_AltManagerCurrentName;
        status = g_AltManagerStatus;
    }

    const float contentWidth = width - 40.f;
    static char unifiedInput[65536]{};
    ImGui::SetCursorPos(ImVec2(20.f, 20.f));
    if (g_AltManagerRefocusInput.exchange(false, std::memory_order_acq_rel))
        ImGui::SetKeyboardFocusHere();
    ImGui::SetNextItemWidth(contentWidth);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.f, 6.f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.f);
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(224, 224, 228, 255));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(13, 13, 14, 255));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, IM_COL32(17, 17, 18, 255));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, IM_COL32(17, 17, 18, 255));
    ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(45, 45, 48, 255));
    ImGui::PushStyleColor(ImGuiCol_TextSelectedBg,
        IM_COL32((int)(g_AccentColor[0] * 180.f), (int)(g_AccentColor[1] * 180.f),
            (int)(g_AccentColor[2] * 180.f), 120));
    const bool submitted = ImGui::InputTextWithHint("##alt_unified_input", "...", unifiedInput,
        sizeof(unifiedInput), ImGuiInputTextFlags_EnterReturnsTrue);
    const bool inputActive = ImGui::IsItemActive();
    const bool inputHovered = ImGui::IsItemHovered();
    const ImVec2 inputMin = ImGui::GetItemRectMin();
    const ImVec2 inputMax = ImGui::GetItemRectMax();
    ImGui::PopStyleColor(6);
    ImGui::PopStyleVar();
    ImGui::PopStyleVar();
    g_ConfigInputActive = inputActive;
    const ImU32 inputBorder = inputActive
        ? IM_COL32((int)(g_AccentColor[0] * 255.f), (int)(g_AccentColor[1] * 255.f),
            (int)(g_AccentColor[2] * 255.f), 210)
        : (inputHovered ? IM_COL32(50, 50, 54, 255) : IM_COL32(31, 31, 34, 255));
    draw->AddRect(inputMin, inputMax, inputBorder, 7.f, 0, 1.f);

    const bool busy = g_AltAuthBusy.load(std::memory_order_acquire);
    ImGui::SetCursorPos(ImVec2(20.f, 62.f));
    ImGui::BeginDisabled(busy);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.f);
    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(18, 18, 19, 255));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(28, 28, 30, 255));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(34, 34, 36, 255));
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(225, 225, 229, 255));
    if (ImGui::Button("Restore", ImVec2((contentWidth - 10.f) * 0.5f, 30.f))) {
        g_AltManagerAction.store(2, std::memory_order_release);
        AltManagerSetStatus("Restoring premium session...");
    }
    ImGui::SameLine(0.f, 10.f);
    const bool cookieClicked = ImGui::Button("Cookie", ImVec2((contentWidth - 10.f) * 0.5f, 30.f));
    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar();
    ImGui::EndDisabled();
    if (cookieClicked) AltManagerSelectCookie();

    if (submitted && !busy) {
        std::string value(unifiedInput);
        if (AltManagerSubmitInput(value))
            SecureZeroMemory(unifiedInput, sizeof(unifiedInput));
    }
    if (!status.empty()) {
        ImGui::SetCursorPos(ImVec2(20.f, 108.f));
        ImGui::TextColored(ImVec4(0.62f, 0.62f, 0.66f, 1.f), "%s", status.c_str());
    }
    const std::string currentLabel = "Current  " + currentName;
    draw->AddText(ImVec2(windowPos.x + 20.f, windowPos.y + height - 28.f),
        IM_COL32(172, 172, 178, 255), currentLabel.c_str());

    ImGui::End();
    ImGui::PopFont();
    ImGui::PopStyleVar(5);
}
