#include "sdk.hpp"

namespace sdk
{
    JavaVM* jvm = nullptr;
    thread_local JNIEnv* jni = nullptr;
    jvmtiEnv* jvmti = nullptr;

    typedef jint(JNICALL* GetCreatedJavaVMs_t)(JavaVM**, jsize, jsize*);

    bool init()
    {
        HMODULE jvm_dll = GetModuleHandleA("jvm.dll");
        if (!jvm_dll) return false;

        GetCreatedJavaVMs_t JNI_GetCreatedJavaVMs = (GetCreatedJavaVMs_t)GetProcAddress(jvm_dll, "JNI_GetCreatedJavaVMs");
        if (!JNI_GetCreatedJavaVMs) return false;

        JavaVM* vms[1];
        jsize vm_count;

        // 3. Extraer la JavaVM
        if (JNI_GetCreatedJavaVMs(vms, 1, &vm_count) != JNI_OK || vm_count == 0)
            return false;

        jvm = vms[0];

        jint env_stat = jvm->GetEnv((void**)&jni, JNI_VERSION_1_8);
        if (env_stat == JNI_EDETACHED)
        {
            if (jvm->AttachCurrentThread((void**)&jni, nullptr) != JNI_OK)
                return false;
        }
        else if (env_stat != JNI_OK)
        {
            return false;
        }

        jvm->GetEnv((void**)&jvmti, JVMTI_VERSION_1_2);

        return jni != nullptr;
    }
}
