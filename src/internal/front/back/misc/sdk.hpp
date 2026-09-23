#pragma once
#include "jni.h"
#include "jvmti.h"
#include <windows.h>

namespace sdk
{
    extern JavaVM* jvm;
    extern thread_local JNIEnv* jni;
    extern jvmtiEnv* jvmti;

    bool init();
}
