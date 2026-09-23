#include "mapper.hpp"
#include <jvmti.h>
#include <iostream>
#include <windows.h>
#include <unordered_set>
#include <algorithm>

extern "C" void swiftLog(const char* format, ...);

namespace mapper
{
    jvmtiEnv* jvmti = nullptr;
    static bool suppress_missing_class_log = false;
}

namespace
{
    // name + separator + signature. 0x1F never appears in JNI names/signatures.
    static std::string make_member_key(std::string_view name, std::string_view signature)
    {
        std::string key;
        key.reserve(name.size() + signature.size() + 1);
        key.append(name);
        key.push_back('\x1f');
        key.append(signature);
        return key;
    }
}

void mapper::__class::build_lookup()
{
    field_lookup.clear();
    method_lookup.clear();
    field_lookup.reserve(this->fields.size());
    method_lookup.reserve(this->methods.size());
    // emplace keeps the first entry for a duplicate key, preserving the previous
    // "first match in the vector wins" behavior (subclass fields are pushed
    // before superclass fields, so subclass shadowing is retained).
    for (size_t i = 0; i < this->fields.size(); ++i)
        field_lookup.emplace(make_member_key(this->fields[i].name, this->fields[i].signature), i);
    for (size_t i = 0; i < this->methods.size(); ++i)
        method_lookup.emplace(make_member_key(this->methods[i].name, this->methods[i].signature), i);
}

mapper::__field mapper::__class::get_field(std::string_view name, std::string_view signature) const
{
    const auto it = field_lookup.find(make_member_key(name, signature));
    if (it != field_lookup.end()) return this->fields[it->second];
    return {};
}

mapper::__method mapper::__class::get_method(std::string_view name, std::string_view signature) const
{
    const auto it = method_lookup.find(make_member_key(name, signature));
    if (it != method_lookup.end()) return this->methods[it->second];
    return {};
}

mapper::__class mapper::get_class(std::string name)
{
    mapper::__class klass;
    klass.name = name;
    klass.signature = "L" + name + ";";
    klass.klass = nullptr;
    if (!sdk::jni || !mapper::jvmti) return klass;

    if (jclass temporal_class = sdk::jni->FindClass(name.c_str()); temporal_class != nullptr)
    {
        klass.klass = (jclass)sdk::jni->NewGlobalRef(temporal_class);
        sdk::jni->DeleteLocalRef(temporal_class);
    }
    else
    {
        sdk::jni->ExceptionClear();
    }

    if (klass.klass == nullptr && mapper::jvmti != nullptr)
    {
        jint loaded_classes_count = 0;
        jclass* loaded_classes = nullptr;

        if (mapper::jvmti->GetLoadedClasses(&loaded_classes_count, &loaded_classes) != JVMTI_ERROR_NONE) {
            swiftLog("[MAPPER] GetLoadedClasses failed for %s", name.c_str());
            return klass;
        }

        for (jint x = 0; x < loaded_classes_count && klass.klass == nullptr; ++x)
        {
            char* class_signature = nullptr;
            char* class_reserved = nullptr;

            mapper::jvmti->GetClassSignature(loaded_classes[x], &class_signature, &class_reserved);

            if (class_signature != nullptr)
            {
                if (std::string(class_signature) == klass.signature)
                {
                    klass.klass = (jclass)sdk::jni->NewGlobalRef(loaded_classes[x]);
                }
                mapper::jvmti->Deallocate((unsigned char*)class_signature);
            }

            if (class_reserved != nullptr)
                mapper::jvmti->Deallocate((unsigned char*)class_reserved);
        }

        if (loaded_classes != nullptr) {
            for (jint i = 0; i < loaded_classes_count; ++i)
                if (loaded_classes[i]) sdk::jni->DeleteLocalRef(loaded_classes[i]);
            mapper::jvmti->Deallocate((unsigned char*)loaded_classes);
        }
    }

    if (klass.klass == nullptr)
    {
        if (!mapper::suppress_missing_class_log)
            swiftLog("[MAPPER] [!] Class not found -> %s", name.c_str());
        sdk::jni->ExceptionClear();
        return klass;
    }

    jint status = 0;
    if (mapper::jvmti->GetClassStatus(klass.klass, &status) != JVMTI_ERROR_NONE ||
        !(status & JVMTI_CLASS_STATUS_PREPARED) || (status & JVMTI_CLASS_STATUS_ERROR)) {
        if (!mapper::suppress_missing_class_log)
            swiftLog("[MAPPER] Class metadata not ready: %s status=%d", name.c_str(), status);
        sdk::jni->DeleteGlobalRef(klass.klass);
        klass.klass = nullptr;
        return klass;
    }

    jclass temporal_class_0 = klass.klass;

    while (temporal_class_0 != nullptr)
    {
        jint fields_count = 0;
        jfieldID* fields = nullptr;

        mapper::jvmti->GetClassFields(temporal_class_0, &fields_count, &fields);

        for (jint x = 0; x < fields_count; ++x)
        {
            mapper::__field field;
            field.identifier = fields[x];

            char* field_name = nullptr;
            char* field_signature = nullptr;
            char* field_reserved = nullptr;

            mapper::jvmti->GetFieldName(temporal_class_0, fields[x], &field_name, &field_signature, &field_reserved);

            if (field_name != nullptr) { field.name = field_name;      mapper::jvmti->Deallocate((unsigned char*)field_name); }
            if (field_signature != nullptr) { field.signature = field_signature; mapper::jvmti->Deallocate((unsigned char*)field_signature); }
            if (field_reserved != nullptr)  mapper::jvmti->Deallocate((unsigned char*)field_reserved);

            klass.fields.push_back(field);
        }

        if (fields != nullptr)
            mapper::jvmti->Deallocate((unsigned char*)fields);

        jint methods_count = 0;
        jmethodID* methods = nullptr;

        mapper::jvmti->GetClassMethods(temporal_class_0, &methods_count, &methods);

        for (jint x = 0; x < methods_count; ++x)
        {
            mapper::__method method;
            method.identifier = methods[x];

            char* method_name = nullptr;
            char* method_signature = nullptr;
            char* method_reserved = nullptr;

            mapper::jvmti->GetMethodName(methods[x], &method_name, &method_signature, &method_reserved);

            if (method_name != nullptr) { method.name = method_name;      mapper::jvmti->Deallocate((unsigned char*)method_name); }
            if (method_signature != nullptr) { method.signature = method_signature; mapper::jvmti->Deallocate((unsigned char*)method_signature); }
            if (method_reserved != nullptr)  mapper::jvmti->Deallocate((unsigned char*)method_reserved);

            klass.methods.push_back(method);
        }

        if (methods != nullptr)
            mapper::jvmti->Deallocate((unsigned char*)methods);

        jclass temporal_class_1 = sdk::jni->GetSuperclass(temporal_class_0);

        if (temporal_class_0 != klass.klass)
            sdk::jni->DeleteLocalRef(temporal_class_0);

        temporal_class_0 = temporal_class_1;
    }

    klass.build_lookup();
    sdk::jni->ExceptionClear();
    return klass;
}

//magia negra jiji 
bool mapper::try_resolve_class(const std::string& key, const std::string& jni_name)
{
    auto it = mapper::classes.find(key);
    if (it != mapper::classes.end() && it->second.klass != nullptr)
        return true;

    if (!sdk::jni || !mapper::jvmti)
        return false;

    mapper::suppress_missing_class_log = true;
    mapper::__class klass = mapper::get_class(jni_name);
    mapper::suppress_missing_class_log = false;

    if (klass.klass == nullptr)
        return false;

    mapper::classes[key] = std::move(klass);
    swiftLog("[MAPPER] Lazy-loaded class: %s -> %s", key.c_str(), jni_name.c_str());
    return true;
}

static void log_class_status(JNIEnv* env)
{
    if (!env) return;

    swiftLog("\n");
    swiftLog("============================================================\n");
    swiftLog("           DETAILED CLASS MAPPING REPORT\n");
    swiftLog("============================================================\n");

    int loaded_ok = 0;
    int not_found = 0;
    int not_ready = 0;
    int error_status = 0;
    int total = 0;

    swiftLog("\n--- CLASSES LOADED SUCCESSFULLY ---\n");
    for (auto& kv : mapper::classes) {
        total++;
        if (kv.second.klass != nullptr) {
            loaded_ok++;
            jint status = 0;
            if (mapper::jvmti) {
                mapper::jvmti->GetClassStatus(kv.second.klass, &status);
            }

            const char* status_str = "UNKNOWN";
            if (status & JVMTI_CLASS_STATUS_ERROR) status_str = "ERROR";
            else if (status & JVMTI_CLASS_STATUS_ARRAY) status_str = "ARRAY";
            else if (status & JVMTI_CLASS_STATUS_PRIMITIVE) status_str = "PRIMITIVE";
            else if ((status & JVMTI_CLASS_STATUS_PREPARED) && (status & JVMTI_CLASS_STATUS_INITIALIZED)) status_str = "PREPARED+INIT";
            else if (status & JVMTI_CLASS_STATUS_PREPARED) status_str = "PREPARED";
            else if (status & JVMTI_CLASS_STATUS_VERIFIED) status_str = "VERIFIED";
            else status_str = "LOADED";

            swiftLog("[OK]    %-40s -> %s (%s)\n",
                kv.first.c_str(),
                kv.second.name.c_str(),
                status_str);
        }
    }

    swiftLog("\n--- CLASSES NOT FOUND ---\n");
    for (auto& kv : mapper::classes) {
        if (kv.second.klass == nullptr && !kv.second.name.empty()) {
            not_found++;
            swiftLog("[MISS]  %-40s -> %s\n",
                kv.first.c_str(),
                kv.second.name.c_str());
        }
    }

    swiftLog("\n--- CLASSES NOT READY (status=0) ---\n");
    swiftLog("\n--- SUMMARY ---\n");
    swiftLog("Total classes:          %d\n", total);
    swiftLog("Loaded successfully:    %d\n", loaded_ok);
    swiftLog("Not found:              %d\n", not_found);
    swiftLog("============================================================\n\n");
}

//class lunar 1.8.9
static void load_lunar_1_8_9_classes()
{
    swiftLog("[MAPPER] Loading Lunar 1.8.9 classes...\n");
    mapper::version = mapper::MINECRAFT_18;

    mapper::classes["Minecraft"] = mapper::get_class("net/minecraft/client/Minecraft");
    mapper::classes["GameSettings"] = mapper::get_class("net/minecraft/client/settings/GameSettings");
    mapper::classes["KeyBinding"] = mapper::get_class("net/minecraft/client/settings/KeyBinding");
    mapper::classes["Timer"] = mapper::get_class("net/minecraft/util/Timer");
    mapper::classes["GuiScreen"] = mapper::get_class("net/minecraft/client/gui/GuiScreen");
    mapper::classes["RenderManager"] = mapper::get_class("net/minecraft/client/renderer/entity/RenderManager");
    mapper::classes["RenderItem"] = mapper::get_class("net/minecraft/client/renderer/entity/RenderItem");
    mapper::classes["WorldClient"] = mapper::get_class("net/minecraft/client/multiplayer/WorldClient");
    mapper::classes["Entity"] = mapper::get_class("net/minecraft/entity/Entity");
    mapper::classes["EntityPlayer"] = mapper::get_class("net/minecraft/entity/player/EntityPlayer");
    mapper::classes["EntityPlayerXP"] = mapper::get_class("net/minecraft/client/entity/EntityPlayerSP");
    mapper::classes["AxisAlignedBB"] = mapper::get_class("net/minecraft/util/AxisAlignedBB");
    mapper::classes["ItemStack"] = mapper::get_class("net/minecraft/item/ItemStack");
    mapper::classes["Item"] = mapper::get_class("net/minecraft/item/Item");
    mapper::classes["Vec3"] = mapper::get_class("net/minecraft/util/Vec3");
    mapper::classes["InventoryPlayer"] = mapper::get_class("net/minecraft/entity/player/InventoryPlayer");
    mapper::classes["ItemPotion"] = mapper::get_class("net/minecraft/item/ItemPotion");
    mapper::classes["EntityLivingBase"] = mapper::get_class("net/minecraft/entity/EntityLivingBase");
    mapper::classes["Block"] = mapper::get_class("net/minecraft/block/Block");
    mapper::classes["IBlockState"] = mapper::get_class("net/minecraft/block/state/IBlockState");
    mapper::classes["BlockPos"] = mapper::get_class("net/minecraft/util/BlockPos");
    mapper::classes["ItemSword"] = mapper::get_class("net/minecraft/item/ItemSword");
    mapper::classes["ItemBow"] = mapper::get_class("net/minecraft/item/ItemBow");
    mapper::classes["ItemBlock"] = mapper::get_class("net/minecraft/item/ItemBlock");
    mapper::classes["Potion"] = mapper::get_class("net/minecraft/potion/Potion");
    mapper::classes["PotionEffect"] = mapper::get_class("net/minecraft/potion/PotionEffect");
    mapper::classes["EntityRenderer"] = mapper::get_class("net/minecraft/client/renderer/EntityRenderer");
    mapper::classes["RendererLivingEntity"] = mapper::get_class("net/minecraft/client/renderer/entity/RendererLivingEntity");
    mapper::classes["FontRenderer"] = mapper::get_class("net/minecraft/client/gui/FontRenderer");
    mapper::classes["ScaledResolution"] = mapper::get_class("net/minecraft/client/gui/ScaledResolution");
    mapper::classes["NetHandlerPlayClient"] = mapper::get_class("net/minecraft/client/network/NetHandlerPlayClient");
    mapper::classes["NetworkManager"] = mapper::get_class("net/minecraft/network/NetworkManager");
    mapper::classes["GuiContainer"] = mapper::get_class("net/minecraft/client/gui/inventory/GuiContainer");
    mapper::classes["RenderPlayer"] = mapper::get_class("net/minecraft/client/renderer/entity/RenderPlayer");

    //classes cargadas en runtime
    //    - PlayerControllerMP                    (se instancia al entrar a un mundo)
    //    - ActiveRenderInfo                      (static init al renderizar mundo)
    //    - MovingObjectPosition                  (ray tracing en mundo)
    //    - MovingObjectPosition$MovingObjectType (enum anidado, al usar el padre)
    //    - Packet                                (solo con red activa)
    //    - C16PacketClientStatus                 (solo con red activa)
    //    - C16PacketClientStatus$EnumState       (solo con red activa)
    //    - C0DPacketCloseWindow                  (solo con red activa)
    //    - C08PacketPlayerBlockPlacement         (solo con red activa)
    //    - GuiInventory                          (solo al abrir inventario)
    //    - GuiChest                              (solo al abrir cofre)

    swiftLog("[MAPPER] Lunar 1.8.9 classes loaded.\n");
}
//class lunar 1.7.10 testing
static void load_lunar_1_7_10_classes()
{
    swiftLog("[MAPPER] Loading Lunar 1.7.10 classes...\n");
    mapper::version = mapper::MINECRAFT_17;

    mapper::classes["Minecraft"] = mapper::get_class("net/minecraft/client/Minecraft");
    mapper::classes["GameSettings"] = mapper::get_class("net/minecraft/client/settings/GameSettings");
    mapper::classes["KeyBinding"] = mapper::get_class("net/minecraft/client/settings/KeyBinding");
    mapper::classes["Timer"] = mapper::get_class("net/minecraft/util/Timer");
    mapper::classes["GuiScreen"] = mapper::get_class("net/minecraft/client/gui/GuiScreen");
    mapper::classes["RenderManager"] = mapper::get_class("net/minecraft/client/renderer/entity/RenderManager");
    mapper::classes["WorldClient"] = mapper::get_class("net/minecraft/client/multiplayer/WorldClient");
    mapper::classes["Entity"] = mapper::get_class("net/minecraft/entity/Entity");
    mapper::classes["EntityPlayer"] = mapper::get_class("net/minecraft/entity/player/EntityPlayer");
    mapper::classes["EntityPlayerXP"] = mapper::get_class("net/minecraft/client/entity/EntityClientPlayerMP");
    mapper::classes["AxisAlignedBB"] = mapper::get_class("net/minecraft/util/AxisAlignedBB");
    mapper::classes["ItemStack"] = mapper::get_class("net/minecraft/item/ItemStack");
    mapper::classes["Item"] = mapper::get_class("net/minecraft/item/Item");
    mapper::classes["Vec3"] = mapper::get_class("net/minecraft/util/Vec3");
    mapper::classes["InventoryPlayer"] = mapper::get_class("net/minecraft/entity/player/InventoryPlayer");
    mapper::classes["ItemPotion"] = mapper::get_class("net/minecraft/item/ItemPotion");
    mapper::classes["EntityLivingBase"] = mapper::get_class("net/minecraft/entity/EntityLivingBase");
    mapper::classes["Block"] = mapper::get_class("net/minecraft/block/Block");
    mapper::classes["ItemSword"] = mapper::get_class("net/minecraft/item/ItemSword");
    mapper::classes["ItemBow"] = mapper::get_class("net/minecraft/item/ItemBow");
    mapper::classes["ItemBlock"] = mapper::get_class("net/minecraft/item/ItemBlock");
    mapper::classes["Potion"] = mapper::get_class("net/minecraft/potion/Potion");
    mapper::classes["PotionEffect"] = mapper::get_class("net/minecraft/potion/PotionEffect");
    mapper::classes["EntityRenderer"] = mapper::get_class("net/minecraft/client/renderer/EntityRenderer");
    mapper::classes["RendererLivingEntity"] = mapper::get_class("net/minecraft/client/renderer/entity/RendererLivingEntity");
    mapper::classes["FontRenderer"] = mapper::get_class("net/minecraft/client/gui/FontRenderer");
    mapper::classes["ScaledResolution"] = mapper::get_class("net/minecraft/client/gui/ScaledResolution");
    mapper::classes["NetHandlerPlayClient"] = mapper::get_class("net/minecraft/client/network/NetHandlerPlayClient");
    mapper::classes["NetworkManager"] = mapper::get_class("net/minecraft/network/NetworkManager");
    mapper::classes["GuiContainer"] = mapper::get_class("net/minecraft/client/gui/inventory/GuiContainer");
    mapper::classes["RenderPlayer"] = mapper::get_class("net/minecraft/client/renderer/entity/RenderPlayer");



    swiftLog("[MAPPER] Lunar 1.7.10 classes loaded.\n");
}

__int32 mapper::initialize()
{
    swiftLog("[MAPPER] Starting class lookup engine (JVMTI)...\n");

    mapper::classes.clear();
    mapper::classes.reserve(64);

    if (sdk::jvm->GetEnv((void**)&mapper::jvmti, JVMTI_VERSION_1_2) != JNI_OK) {
        swiftLog("[MAPPER] [!] ERROR: Failed to connect to JVMTI.\n");
        return -1;
    }

    swiftLog("[MAPPER] JVMTI connected. Mapping base utilities...\n");
    mapper::classes["List"] = mapper::get_class("java/util/List");
    mapper::classes["Enum"] = mapper::get_class("java/lang/Enum");

    JNIEnv* env = sdk::jni;
    bool isLunar = false;
    bool isForge = false;
    bool isFeather = false;
    bool is1_7 = false;

    HWND currentWindowHandle = nullptr;
    for (currentWindowHandle = GetTopWindow(NULL); currentWindowHandle != NULL; currentWindowHandle = GetNextWindow(currentWindowHandle, GW_HWNDNEXT))
    {
        if (!IsWindowVisible(currentWindowHandle))
            continue;

        int length = GetWindowTextLength(currentWindowHandle);
        if (length == 0)
            continue;

        CHAR cName[MAX_PATH];
        GetClassNameA(currentWindowHandle, cName, _countof(cName));
        if (strcmp(cName, "LWJGL") != 0) {
            continue;
        }

        DWORD pid;
        GetWindowThreadProcessId(currentWindowHandle, &pid);
        if (pid == GetCurrentProcessId()) {
            break;
        }
    }

    char windowTitle[256] = { 0 };
    if (currentWindowHandle) {
        GetWindowTextA(currentWindowHandle, windowTitle, sizeof(windowTitle));
        swiftLog("[MAPPER] Window title: %s\n", windowTitle);
    }

    is1_7 = strstr(windowTitle, "1.7.10") != nullptr;
    bool vanillaMappings = strstr(windowTitle, "Badlion Minecraft") != nullptr;

    if (strstr(windowTitle, "Lunar Client") != nullptr) {
        isLunar = true;
        swiftLog("[MAPPER] Lunar Client detected! Version: %s\n", is1_7 ? "1.7.10" : "1.8.9");
    }

    if (!isLunar) {
        jclass launchWrapper = env->FindClass("net/minecraft/launchwrapper/LaunchClassLoader");
        jclass launchClazz = env->FindClass("net/minecraft/launchwrapper/Launch");
        isForge = (launchWrapper != nullptr && launchClazz != nullptr);
        if (launchWrapper != nullptr) env->DeleteLocalRef(launchWrapper);
        if (launchClazz != nullptr) env->DeleteLocalRef(launchClazz);
        env->ExceptionClear();

        if (isForge && !vanillaMappings) {
            swiftLog("[MAPPER] Forge detected! Version: %s\n", is1_7 ? "1.7.10" : "1.8");
        }
        else {
            swiftLog("[MAPPER] Vanilla/Casual detected! Version: %s\n", is1_7 ? "1.7.10" : "1.8");
        }

        jclass featherClass = env->FindClass("net/digitalingot/featheropt/FeatherCoreMod");
        if (featherClass != nullptr) {
            env->DeleteLocalRef(featherClass);
            isFeather = true;
            swiftLog("[MAPPER] Feather Client detected!\n");
        }
        else {
            env->ExceptionClear();
        }
    }

    if (isLunar) {
        if (is1_7) load_lunar_1_7_10_classes();
        else load_lunar_1_8_9_classes();
    }
    else if (isFeather) {
        load_lunar_1_8_9_classes();
    }
    else if (isForge) {
        if (is1_7) load_lunar_1_7_10_classes();
        else load_lunar_1_8_9_classes();
    }
    else {
        if (is1_7) load_lunar_1_7_10_classes();
        else load_lunar_1_8_9_classes();
    }

    log_class_status(env);

    swiftLog("[MAPPER] Initialization completed.\n");
    sdk::jni->ExceptionClear();
    return 0;
}

__int32 mapper::uninitialize()
{
    for (auto& klass : mapper::classes) {
        if (klass.second.klass != nullptr) {
            sdk::jni->DeleteGlobalRef(klass.second.klass);
        }
    }
    mapper::classes.clear();
    mapper::jvmti = nullptr;
    sdk::jni->ExceptionClear();
    return 0;
}