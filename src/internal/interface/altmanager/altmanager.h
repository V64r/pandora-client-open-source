#pragma once

#include <jni.h>
#include <atomic>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

extern bool g_AltManagerMode;
extern std::atomic<int> g_AltManagerAction;
extern std::atomic<bool> g_AltManagerRefocusInput;
extern char g_AltManagerPendingName[17];
extern std::string g_AltManagerPremiumName;
extern std::string g_AltManagerCurrentName;
extern std::string g_AltManagerStatus;
extern std::mutex g_AltManagerStateMutex;
extern jobject g_AltManagerPremiumSession;
extern std::vector<std::pair<jfieldID, jobject>> g_AltManagerPremiumStringFields;

void ShutdownAltAuthWorker();
bool AltManagerFindSession(JNIEnv* env, jobject minecraft, jfieldID& sessionField, jobject& session);
bool AltManagerSetSessionName(JNIEnv* env, jobject session, const char* username);
bool AltManagerRestorePremiumSession(JNIEnv* env, jobject session);
void AltManagerUpdateOnGameThread(JNIEnv* env, jobject minecraft, jobject currentScreen);
