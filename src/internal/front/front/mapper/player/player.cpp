#include "mapper.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>

mapper::__player::__player(jobject object)
{
    this->object = object;
}

mapper::__player::__player(const mapper::__player& player)
{
    if (player.object != nullptr)
        this->object = sdk::jni->NewLocalRef(player.object);
}

mapper::__player::~__player()
{
    if (this->object != nullptr)
        sdk::jni->DeleteLocalRef(this->object);
}

mapper::__vec3 mapper::__player::get_position()
{
    mapper::__field field_0 = mapper::classes["EntityPlayerXP"].get_field("posX", "D");
    mapper::__field field_1 = mapper::classes["EntityPlayerXP"].get_field("posY", "D");
    mapper::__field field_2 = mapper::classes["EntityPlayerXP"].get_field("posZ", "D");

    if (field_0.identifier == nullptr) field_0 = mapper::classes["EntityPlayerXP"].get_field("field_70165_t", "D");
    if (field_0.identifier == nullptr) field_0 = mapper::classes["EntityPlayerXP"].get_field("s", "D");

    if (field_1.identifier == nullptr) field_1 = mapper::classes["EntityPlayerXP"].get_field("field_70163_u", "D");
    if (field_1.identifier == nullptr) field_1 = mapper::classes["EntityPlayerXP"].get_field("t", "D");

    if (field_2.identifier == nullptr) field_2 = mapper::classes["EntityPlayerXP"].get_field("field_70161_v", "D");
    if (field_2.identifier == nullptr) field_2 = mapper::classes["EntityPlayerXP"].get_field("u", "D");

    mapper::__vec3 result = {0, 0, 0};
    if (field_0.identifier == nullptr || field_1.identifier == nullptr || field_2.identifier == nullptr) return result;
    result.x = sdk::jni->GetDoubleField(this->object, field_0.identifier);
    result.y = sdk::jni->GetDoubleField(this->object, field_1.identifier);
    result.z = sdk::jni->GetDoubleField(this->object, field_2.identifier);
    return result;
}

mapper::__vec3 mapper::__player::get_old_position()
{
    // Minecraft renders from lastTickPos*. prevPos* can diverge during local
    // movement and vertical motion, making attached overlays shake on jumps.
    mapper::__field field_0 = mapper::classes["EntityPlayerXP"].get_field("lastTickPosX", "D");
    mapper::__field field_1 = mapper::classes["EntityPlayerXP"].get_field("lastTickPosY", "D");
    mapper::__field field_2 = mapper::classes["EntityPlayerXP"].get_field("lastTickPosZ", "D");

    if (field_0.identifier == nullptr) field_0 = mapper::classes["EntityPlayerXP"].get_field("field_70142_S", "D");
    if (field_0.identifier == nullptr) field_0 = mapper::classes["EntityPlayerXP"].get_field("P", "D");

    if (field_1.identifier == nullptr) field_1 = mapper::classes["EntityPlayerXP"].get_field("field_70137_T", "D");
    if (field_1.identifier == nullptr) field_1 = mapper::classes["EntityPlayerXP"].get_field("Q", "D");

    if (field_2.identifier == nullptr) field_2 = mapper::classes["EntityPlayerXP"].get_field("field_70136_U", "D");
    if (field_2.identifier == nullptr) field_2 = mapper::classes["EntityPlayerXP"].get_field("R", "D");

    // Compatibility fallback for mappings without lastTickPos fields.
    if (field_0.identifier == nullptr) field_0 = mapper::classes["EntityPlayerXP"].get_field("field_70169_q", "D");
    if (field_0.identifier == nullptr) field_0 = mapper::classes["EntityPlayerXP"].get_field("p", "D");

    if (field_1.identifier == nullptr) field_1 = mapper::classes["EntityPlayerXP"].get_field("field_70167_r", "D");
    if (field_1.identifier == nullptr) field_1 = mapper::classes["EntityPlayerXP"].get_field("q", "D");

    if (field_2.identifier == nullptr) field_2 = mapper::classes["EntityPlayerXP"].get_field("field_70166_s", "D");
    if (field_2.identifier == nullptr) field_2 = mapper::classes["EntityPlayerXP"].get_field("r", "D");

    mapper::__vec3 result = {0, 0, 0};
    if (field_0.identifier == nullptr || field_1.identifier == nullptr || field_2.identifier == nullptr) return result;
    result.x = sdk::jni->GetDoubleField(this->object, field_0.identifier);
    result.y = sdk::jni->GetDoubleField(this->object, field_1.identifier);
    result.z = sdk::jni->GetDoubleField(this->object, field_2.identifier);
    return result;
}

mapper::__vec3 mapper::__player::get_motion()
{
    mapper::__field field_0 = mapper::classes["EntityPlayerXP"].get_field("motionX", "D");
    mapper::__field field_1 = mapper::classes["EntityPlayerXP"].get_field("motionY", "D");
    mapper::__field field_2 = mapper::classes["EntityPlayerXP"].get_field("motionZ", "D");

    if (field_0.identifier == nullptr) field_0 = mapper::classes["EntityPlayerXP"].get_field("field_70159_w", "D");
    if (field_0.identifier == nullptr) field_0 = mapper::classes["EntityPlayerXP"].get_field("v", "D");

    if (field_1.identifier == nullptr) field_1 = mapper::classes["EntityPlayerXP"].get_field("field_70181_x", "D");
    if (field_1.identifier == nullptr) field_1 = mapper::classes["EntityPlayerXP"].get_field("w", "D");

    if (field_2.identifier == nullptr) field_2 = mapper::classes["EntityPlayerXP"].get_field("field_70179_y", "D");
    if (field_2.identifier == nullptr) field_2 = mapper::classes["EntityPlayerXP"].get_field("x", "D");

    mapper::__vec3 result = {0, 0, 0};
    if (field_0.identifier == nullptr || field_1.identifier == nullptr || field_2.identifier == nullptr) return result;
    result.x = sdk::jni->GetDoubleField(this->object, field_0.identifier);
    result.y = sdk::jni->GetDoubleField(this->object, field_1.identifier);
    result.z = sdk::jni->GetDoubleField(this->object, field_2.identifier);
    return result;
}

void mapper::__player::set_motion(mapper::__vec3 motion)
{
    mapper::__field field_0 = mapper::classes["EntityPlayerXP"].get_field("motionX", "D");
    mapper::__field field_1 = mapper::classes["EntityPlayerXP"].get_field("motionY", "D");
    mapper::__field field_2 = mapper::classes["EntityPlayerXP"].get_field("motionZ", "D");

    if (field_0.identifier == nullptr) field_0 = mapper::classes["EntityPlayerXP"].get_field("field_70159_w", "D");
    if (field_0.identifier == nullptr) field_0 = mapper::classes["EntityPlayerXP"].get_field("v", "D");

    if (field_1.identifier == nullptr) field_1 = mapper::classes["EntityPlayerXP"].get_field("field_70181_x", "D");
    if (field_1.identifier == nullptr) field_1 = mapper::classes["EntityPlayerXP"].get_field("w", "D");

    if (field_2.identifier == nullptr) field_2 = mapper::classes["EntityPlayerXP"].get_field("field_70179_y", "D");
    if (field_2.identifier == nullptr) field_2 = mapper::classes["EntityPlayerXP"].get_field("x", "D");

    if (field_0.identifier == nullptr || field_1.identifier == nullptr || field_2.identifier == nullptr) return;
    sdk::jni->SetDoubleField(this->object, field_0.identifier, motion.x);
    sdk::jni->SetDoubleField(this->object, field_1.identifier, motion.y);
    sdk::jni->SetDoubleField(this->object, field_2.identifier, motion.z);
}

bool mapper::__player::get_on_ground()
{
    mapper::__field field = mapper::classes["EntityPlayerXP"].get_field("onGround", "Z");
    if (field.identifier == nullptr) field = mapper::classes["EntityPlayerXP"].get_field("field_70122_E", "Z");
    if (field.identifier == nullptr) field = mapper::classes["EntityPlayerXP"].get_field("C", "Z");

    if (field.identifier == nullptr) return false;
    return sdk::jni->GetBooleanField(this->object, field.identifier);
}

void mapper::__player::jump()
{
    mapper::__method method = mapper::classes["EntityPlayer"].get_method("jump", "()V");
    if (method.identifier == nullptr) method = mapper::classes["EntityPlayer"].get_method("func_70664_aZ", "()V");
    if (method.identifier == nullptr) method = mapper::classes["EntityPlayer"].get_method("bF", "()V");
    if (method.identifier == nullptr) method = mapper::classes["EntityPlayer"].get_method("bl", "()V"); // 1.7.10

    if (method.identifier == nullptr) return;
    sdk::jni->CallVoidMethod(this->object, method.identifier);
}

bool mapper::__player::is_swing_in_progress()
{
    mapper::__field field = mapper::classes["EntityPlayerXP"].get_field("isSwingInProgress", "Z");
    if (field.identifier == nullptr) field = mapper::classes["EntityPlayerXP"].get_field("field_82175_bq", "Z");
    if (field.identifier == nullptr) field = mapper::classes["EntityPlayerXP"].get_field("ar", "Z");

    if (field.identifier == nullptr) return false;
    return sdk::jni->GetBooleanField(this->object, field.identifier);
}

__int32 mapper::__player::get_hurt_time()
{
    if (this->object == nullptr) return 0;

    mapper::__field field = mapper::classes["EntityLivingBase"].get_field("hurtTime", "I");
    if (field.identifier == nullptr) field = mapper::classes["EntityLivingBase"].get_field("field_70737_aN", "I");
    if (field.identifier == nullptr) field = mapper::classes["EntityLivingBase"].get_field("au", "I"); // 1.8.9 obf
    if (field.identifier == nullptr) field = mapper::classes["EntityLivingBase"].get_field("aw", "I"); // 1.7.10 obf

    if (field.identifier == nullptr) return 0;
    return sdk::jni->GetIntField(this->object, field.identifier);
}

mapper::__vec3 mapper::__player::get_view_position(float partial_ticks)
{
    mapper::__vec3 position = this->get_position();
    mapper::__vec3 old_position = this->get_old_position();

    mapper::__vec3 result;
    result.x = old_position.x + (position.x - old_position.x) * partial_ticks;
    result.y = old_position.y + (position.y - old_position.y) * partial_ticks;
    result.z = old_position.z + (position.z - old_position.z) * partial_ticks;
    return result;
}

mapper::__vec2 mapper::__player::get_view_angles()
{
    mapper::__field field_0 = mapper::classes["EntityPlayerXP"].get_field("rotationYaw", "F");
    mapper::__field field_1 = mapper::classes["EntityPlayerXP"].get_field("rotationPitch", "F");

    if (field_0.identifier == nullptr) field_0 = mapper::classes["EntityPlayerXP"].get_field("field_70177_z", "F");
    if (field_0.identifier == nullptr) field_0 = mapper::classes["EntityPlayerXP"].get_field("y", "F");

    if (field_1.identifier == nullptr) field_1 = mapper::classes["EntityPlayerXP"].get_field("field_70125_A", "F");
    if (field_1.identifier == nullptr) field_1 = mapper::classes["EntityPlayerXP"].get_field("z", "F");

    mapper::__vec2 result = {};
    if (field_0.identifier == nullptr || field_1.identifier == nullptr) return result;
    result.x = sdk::jni->GetFloatField(this->object, field_0.identifier);
    result.y = sdk::jni->GetFloatField(this->object, field_1.identifier);
    return result;
}

void mapper::__player::set_view_angles(mapper::__vec2 view_angles)
{
    mapper::__field field_0 = mapper::classes["EntityPlayerXP"].get_field("rotationYaw", "F");
    mapper::__field field_1 = mapper::classes["EntityPlayerXP"].get_field("rotationPitch", "F");

    if (field_0.identifier == nullptr) field_0 = mapper::classes["EntityPlayerXP"].get_field("field_70177_z", "F");
    if (field_0.identifier == nullptr) field_0 = mapper::classes["EntityPlayerXP"].get_field("y", "F");

    if (field_1.identifier == nullptr) field_1 = mapper::classes["EntityPlayerXP"].get_field("field_70125_A", "F");
    if (field_1.identifier == nullptr) field_1 = mapper::classes["EntityPlayerXP"].get_field("z", "F");

    if (field_0.identifier == nullptr || field_1.identifier == nullptr) return;
    sdk::jni->SetFloatField(this->object, field_0.identifier, view_angles.x);
    sdk::jni->SetFloatField(this->object, field_1.identifier, view_angles.y);
}

void mapper::__player::set_old_view_angles(mapper::__vec2 old_view_angles)
{
    mapper::__field field_0 = mapper::classes["EntityPlayerXP"].get_field("prevRotationYaw", "F");
    mapper::__field field_1 = mapper::classes["EntityPlayerXP"].get_field("prevRotationPitch", "F");

    if (field_0.identifier == nullptr) field_0 = mapper::classes["EntityPlayerXP"].get_field("field_70126_B", "F");
    if (field_0.identifier == nullptr) field_0 = mapper::classes["EntityPlayerXP"].get_field("A", "F");

    if (field_1.identifier == nullptr) field_1 = mapper::classes["EntityPlayerXP"].get_field("field_70127_C", "F");
    if (field_1.identifier == nullptr) field_1 = mapper::classes["EntityPlayerXP"].get_field("B", "F");

    if (field_0.identifier == nullptr || field_1.identifier == nullptr) return;
    sdk::jni->SetFloatField(this->object, field_0.identifier, old_view_angles.x);
    sdk::jni->SetFloatField(this->object, field_1.identifier, old_view_angles.y);
}

__int32 mapper::__player::get_ticks_existed()
{
    mapper::__field field = mapper::classes["EntityPlayerXP"].get_field("ticksExisted", "I");
    if (field.identifier == nullptr) field = mapper::classes["EntityPlayerXP"].get_field("field_70173_aa", "I");
    if (field.identifier == nullptr) field = mapper::classes["EntityPlayerXP"].get_field("W", "I");

    if (field.identifier == nullptr) return 0;
    return sdk::jni->GetIntField(this->object, field.identifier);
}

float mapper::__player::get_health()
{
    mapper::__method method = mapper::classes["EntityPlayerXP"].get_method("getHealth", "()F");
    if (method.identifier == nullptr) method = mapper::classes["EntityPlayerXP"].get_method("func_110143_aJ", "()F");
    if (method.identifier == nullptr) method = mapper::classes["EntityPlayerXP"].get_method("bn", "()F");
    if (method.identifier == nullptr) method = mapper::classes["EntityPlayerXP"].get_method("bm", "()F"); // 1.7.10 alt

    if (method.identifier == nullptr) return 0.f;
    float hp = sdk::jni->CallFloatMethod(this->object, method.identifier);
    if (sdk::jni->ExceptionCheck()) { sdk::jni->ExceptionClear(); return 0.f; }
    return hp;
}

float mapper::__player::get_move_foreward()
{
    mapper::__field field = mapper::classes["EntityPlayerXP"].get_field("moveForward", "F");
    if (field.identifier == nullptr) field = mapper::classes["EntityPlayerXP"].get_field("field_70701_bs", "F");
    if (field.identifier == nullptr) field = mapper::classes["EntityPlayerXP"].get_field("ba", "F");
    if (field.identifier == nullptr) field = mapper::classes["EntityPlayerXP"].get_field("be", "F"); // 1.7.10

    if (field.identifier == nullptr) return 0.f;
    return sdk::jni->GetFloatField(this->object, field.identifier);
}

float mapper::__player::get_move_strafing()
{
    mapper::__field field = mapper::classes["EntityPlayerXP"].get_field("moveStrafing", "F");
    if (field.identifier == nullptr) field = mapper::classes["EntityPlayerXP"].get_field("field_70702_br", "F");
    if (field.identifier == nullptr) field = mapper::classes["EntityPlayerXP"].get_field("aZ", "F");
    if (field.identifier == nullptr) field = mapper::classes["EntityPlayerXP"].get_field("bd", "F"); // 1.7.10

    if (field.identifier == nullptr) return 0.f;
    return sdk::jni->GetFloatField(this->object, field.identifier);
}

bool mapper::__player::can_entity_be_seen(mapper::__player player)
{
    std::string signature = "(" + mapper::classes["Entity"].signature + ")Z";

    mapper::__method method = mapper::classes["EntityPlayerXP"].get_method("canEntityBeSeen", signature);
    if (method.identifier == nullptr) method = mapper::classes["EntityPlayerXP"].get_method("func_70685_l", signature);
    if (method.identifier == nullptr) method = mapper::classes["EntityPlayerXP"].get_method("t", signature);
    if (method.identifier == nullptr) method = mapper::classes["EntityPlayerXP"].get_method("s", signature); // 1.7.10
    if (method.identifier == nullptr) method = mapper::classes["EntityPlayerXP"].get_method("u", signature); // 1.7.10 alt
    if (method.identifier == nullptr) method = mapper::classes["EntityPlayerXP"].get_method("q", signature); // 1.7.10 alt2
    if (method.identifier == nullptr) method = mapper::classes["EntityPlayerXP"].get_method("r", signature); // 1.7.10 alt3

    if (method.identifier == nullptr) return true;
    bool result = sdk::jni->CallBooleanMethod(this->object, method.identifier, player.object);
    if (sdk::jni->ExceptionCheck()) { sdk::jni->ExceptionClear(); return true; }
    return result;
}

bool mapper::__player::get_flag(__int32 flag)
{
    mapper::__method method = mapper::classes["EntityPlayerXP"].get_method("getFlag", "(I)Z");
    if (method.identifier == nullptr) method = mapper::classes["EntityPlayerXP"].get_method("func_70083_f", "(I)Z");
    if (method.identifier == nullptr) method = mapper::classes["EntityPlayerXP"].get_method("g", "(I)Z");

    if (method.identifier == nullptr) return false;
    return sdk::jni->CallBooleanMethod(this->object, method.identifier, flag);
}

void mapper::__player::set_move_foreward(float value)
{
    mapper::__field field = mapper::classes["EntityPlayerXP"].get_field("moveForward", "F");
    if (field.identifier == nullptr) field = mapper::classes["EntityPlayerXP"].get_field("field_70701_bs", "F");
    if (field.identifier == nullptr) field = mapper::classes["EntityPlayerXP"].get_field("ba", "F");
    if (field.identifier == nullptr) field = mapper::classes["EntityPlayerXP"].get_field("be", "F"); // 1.7.10

    if (field.identifier == nullptr) return;
    sdk::jni->SetFloatField(this->object, field.identifier, value);
}

void mapper::__player::set_move_strafing(float value)
{
    mapper::__field field = mapper::classes["EntityPlayerXP"].get_field("moveStrafing", "F");
    if (field.identifier == nullptr) field = mapper::classes["EntityPlayerXP"].get_field("field_70702_br", "F");
    if (field.identifier == nullptr) field = mapper::classes["EntityPlayerXP"].get_field("aZ", "F");
    if (field.identifier == nullptr) field = mapper::classes["EntityPlayerXP"].get_field("bd", "F"); // 1.7.10

    if (field.identifier == nullptr) return;
    sdk::jni->SetFloatField(this->object, field.identifier, value);
}

bool mapper::__player::is_offset_position_in_liquid(double x, double y, double z)
{
    mapper::__method method = mapper::classes["EntityPlayerXP"].get_method("isOffsetPositionInLiquid", "(DDD)Z");
    if (method.identifier == nullptr) method = mapper::classes["EntityPlayerXP"].get_method("func_70038_c", "(DDD)Z");
    if (method.identifier == nullptr) method = mapper::classes["EntityPlayerXP"].get_method("c", "(DDD)Z");

    if (method.identifier == nullptr) return true; // safe default: assume in liquid
    return !sdk::jni->CallBooleanMethod(this->object, method.identifier, x, y, z);
}

mapper::__item_stack mapper::__player::get_held_item_stack()
{
    if (this->object == nullptr) return mapper::__item_stack(nullptr);

    std::string signature = "()" + mapper::classes["ItemStack"].signature;

    mapper::__method method = mapper::classes["EntityPlayerXP"].get_method("getHeldItem", signature);
    if (method.identifier == nullptr) method = mapper::classes["EntityPlayerXP"].get_method("func_70694_bm", signature);
    if (method.identifier == nullptr) method = mapper::classes["EntityPlayerXP"].get_method("bA", signature);
    if (method.identifier == nullptr) method = mapper::classes["EntityPlayerXP"].get_method("bI", signature); // 1.7.10 alt
    if (method.identifier == nullptr) method = mapper::classes["EntityPlayerXP"].get_method("bJ", signature); // 1.7.10 alt
    if (method.identifier == nullptr) method = mapper::classes["EntityPlayerXP"].get_method("be", signature); // 1.7.10 obf (EntityLivingBase)
    if (method.identifier == nullptr) method = mapper::classes["EntityPlayerXP"].get_method("bf", signature); // 1.7.10 obf alt2

    // 1.7.10 Fallback: get inventory field, then call getCurrentItem
    if (method.identifier == nullptr && mapper::version == mapper::MINECRAFT_17) {
        mapper::__field inv_field = mapper::classes["EntityPlayerXP"].get_field("inventory", "L" + mapper::classes["InventoryPlayer"].name + ";");
        if (inv_field.identifier == nullptr) inv_field = mapper::classes["EntityPlayerXP"].get_field("field_71071_by", "L" + mapper::classes["InventoryPlayer"].name + ";");
        if (inv_field.identifier == nullptr) inv_field = mapper::classes["EntityPlayerXP"].get_field("bm", "L" + mapper::classes["InventoryPlayer"].name + ";");
        if (inv_field.identifier == nullptr) inv_field = mapper::classes["EntityPlayerXP"].get_field("bg", "L" + mapper::classes["InventoryPlayer"].name + ";");
        
        if (inv_field.identifier != nullptr) {
            jobject inv_obj = sdk::jni->GetObjectField(this->object, inv_field.identifier);
            if (inv_obj != nullptr) {
                mapper::__method get_curr = mapper::classes["InventoryPlayer"].get_method("getCurrentItem", signature);
                if (get_curr.identifier == nullptr) get_curr = mapper::classes["InventoryPlayer"].get_method("func_70448_g", signature);
                if (get_curr.identifier == nullptr) get_curr = mapper::classes["InventoryPlayer"].get_method("h", signature);
                
                if (get_curr.identifier != nullptr) {
                    jobject result = sdk::jni->CallObjectMethod(inv_obj, get_curr.identifier);
                    sdk::jni->DeleteLocalRef(inv_obj);
                    if (sdk::jni->ExceptionCheck()) { sdk::jni->ExceptionClear(); return mapper::__item_stack(nullptr); }
                    return mapper::__item_stack(result);
                }
                sdk::jni->DeleteLocalRef(inv_obj);
            }
        }
    }

    if (method.identifier == nullptr) return mapper::__item_stack(nullptr);
    jobject result = sdk::jni->CallObjectMethod(this->object, method.identifier);
    if (sdk::jni->ExceptionCheck()) { sdk::jni->ExceptionClear(); return mapper::__item_stack(nullptr); }
    return mapper::__item_stack(result);
}

std::string mapper::__player::get_name()
{
    if (this->object == nullptr) return "";

    mapper::__method method = mapper::classes["EntityPlayerXP"].get_method("getName", "()Ljava/lang/String;");
    if (method.identifier == nullptr) method = mapper::classes["EntityPlayerXP"].get_method("getCommandSenderName", "()Ljava/lang/String;");
    if (method.identifier == nullptr) method = mapper::classes["EntityPlayerXP"].get_method("func_70005_c_", "()Ljava/lang/String;");
    if (method.identifier == nullptr) method = mapper::classes["EntityPlayerXP"].get_method("e_", "()Ljava/lang/String;");
    if (method.identifier == nullptr) method = mapper::classes["EntityPlayerXP"].get_method("d_", "()Ljava/lang/String;"); // 1.7.10 alt

    if (method.identifier == nullptr) return "";

    jstring jstr = (jstring)sdk::jni->CallObjectMethod(this->object, method.identifier);
    if (sdk::jni->ExceptionCheck()) { sdk::jni->ExceptionClear(); return ""; }
    if (!jstr) return "";

    const char* name_chars = sdk::jni->GetStringUTFChars(jstr, nullptr);
    std::string fixed_name = "";

    if (name_chars != nullptr)
    {
        for (int i = 0; name_chars[i] != '\0'; ++i)
        {
            unsigned char c = (unsigned char)name_chars[i];
            
            // Skip UTF-8 section sign (0xC2 0xA7) + the color/format char after it
            if (c == 0xC2 && (unsigned char)name_chars[i + 1] == 0xA7 && name_chars[i + 2] != '\0') {
                i += 2; // skip §X (3 bytes total)
                continue;
            }

            if (c >= 32 && c <= 126)
            {
                fixed_name += (char)c;
            }
        }
        sdk::jni->ReleaseStringUTFChars(jstr, name_chars);
    }
    sdk::jni->DeleteLocalRef(jstr);

    return fixed_name;
}

std::string mapper::__player::get_uuid()
{
    if (!this->object || !sdk::jni) return "";

    mapper::__field gp_field = mapper::classes["EntityPlayer"].get_field(
        "gameProfile", "Lcom/mojang/authlib/GameProfile;");
    if (!gp_field.identifier) gp_field = mapper::classes["EntityPlayer"].get_field(
        "field_146106_i", "Lcom/mojang/authlib/GameProfile;");
    if (!gp_field.identifier) gp_field = mapper::classes["EntityPlayer"].get_field(
        "i", "Lcom/mojang/authlib/GameProfile;");
    if (!gp_field.identifier) gp_field = mapper::classes["EntityPlayer"].get_field(
        "bH", "Lcom/mojang/authlib/GameProfile;");
    if (!gp_field.identifier) gp_field = mapper::classes["EntityPlayer"].get_field(
        "cc", "Lcom/mojang/authlib/GameProfile;");
    if (!gp_field.identifier) {
        for (const auto& field : mapper::classes["EntityPlayer"].fields) {
            if (field.signature.find("GameProfile") != std::string::npos) {
                gp_field = field;
                break;
            }
        }
    }
    if (!gp_field.identifier) return "";

    jobject profile = sdk::jni->GetObjectField(this->object, gp_field.identifier);
    if (sdk::jni->ExceptionCheck()) { sdk::jni->ExceptionClear(); return ""; }
    if (!profile) return "";

    jclass profile_class = sdk::jni->GetObjectClass(profile);
    jmethodID get_id = profile_class
        ? sdk::jni->GetMethodID(profile_class, "getId", "()Ljava/util/UUID;")
        : nullptr;
    if (!get_id && sdk::jni->ExceptionCheck()) sdk::jni->ExceptionClear();

    jobject uuid_object = get_id
        ? sdk::jni->CallObjectMethod(profile, get_id)
        : nullptr;
    if (sdk::jni->ExceptionCheck()) { sdk::jni->ExceptionClear(); uuid_object = nullptr; }

    std::string result;
    if (uuid_object) {
        jclass uuid_class = sdk::jni->GetObjectClass(uuid_object);
        jmethodID to_string = uuid_class
            ? sdk::jni->GetMethodID(uuid_class, "toString", "()Ljava/lang/String;")
            : nullptr;
        if (to_string) {
            jstring text = static_cast<jstring>(
                sdk::jni->CallObjectMethod(uuid_object, to_string));
            if (!sdk::jni->ExceptionCheck() && text) {
                const char* chars = sdk::jni->GetStringUTFChars(text, nullptr);
                if (chars) {
                    result = chars;
                    sdk::jni->ReleaseStringUTFChars(text, chars);
                }
                sdk::jni->DeleteLocalRef(text);
            } else if (sdk::jni->ExceptionCheck()) {
                sdk::jni->ExceptionClear();
            }
        }
        if (uuid_class) sdk::jni->DeleteLocalRef(uuid_class);
        sdk::jni->DeleteLocalRef(uuid_object);
    }

    if (profile_class) sdk::jni->DeleteLocalRef(profile_class);
    sdk::jni->DeleteLocalRef(profile);
    result.erase(std::remove(result.begin(), result.end(), '-'), result.end());
    std::transform(result.begin(), result.end(), result.begin(),
        [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return result.size() == 32 ? result : std::string{};
}

__int32 mapper::__player::get_entity_id()
{
    if (this->object == nullptr) return -1;
    mapper::__field field = mapper::classes["EntityPlayerXP"].get_field("entityId", "I");
    if (field.identifier == nullptr) field = mapper::classes["EntityPlayerXP"].get_field("field_145783_c", "I");
    if (field.identifier == nullptr) field = mapper::classes["EntityPlayerXP"].get_field("d", "I");

    // Try on base class if not found
    if (field.identifier == nullptr) field = mapper::classes["Entity"].get_field("entityId", "I");
    if (field.identifier == nullptr) field = mapper::classes["Entity"].get_field("field_145783_c", "I");
    if (field.identifier == nullptr) field = mapper::classes["Entity"].get_field("d", "I");

    if (field.identifier == nullptr) return -1;
    return sdk::jni->GetIntField(this->object, field.identifier);
}

mapper::__item_stack mapper::__player::get_inventory_slot(int slot_id)
{
    mapper::__field field = mapper::classes["EntityPlayer"].get_field("inventory", mapper::classes["InventoryPlayer"].signature);
    if (field.identifier == nullptr) field = mapper::classes["EntityPlayer"].get_field("field_71071_by", mapper::classes["InventoryPlayer"].signature);
    if (field.identifier == nullptr) field = mapper::classes["EntityPlayer"].get_field("bg", mapper::classes["InventoryPlayer"].signature); // 1.8.9 / 1.7.10
    if (field.identifier == nullptr) field = mapper::classes["EntityPlayer"].get_field("bi", mapper::classes["InventoryPlayer"].signature);

    if (field.identifier == nullptr) return mapper::__item_stack(nullptr);

    jobject inventory_obj = sdk::jni->GetObjectField(this->object, field.identifier);
    if (inventory_obj == nullptr) return mapper::__item_stack(nullptr);

    jclass inventory_class = sdk::jni->GetObjectClass(inventory_obj);
    if (inventory_class == nullptr) {
        sdk::jni->DeleteLocalRef(inventory_obj);
        return mapper::__item_stack(nullptr);
    }

    std::string method_sig = "(I)" + mapper::classes["ItemStack"].signature;
    jmethodID get_stack = sdk::jni->GetMethodID(inventory_class, "getStackInSlot", method_sig.c_str());
    if (get_stack == nullptr) { sdk::jni->ExceptionClear(); get_stack = sdk::jni->GetMethodID(inventory_class, "func_70301_a", method_sig.c_str()); }
    if (get_stack == nullptr) { sdk::jni->ExceptionClear(); get_stack = sdk::jni->GetMethodID(inventory_class, "a", method_sig.c_str()); }

    jobject item_stack_obj = nullptr;
    if (get_stack != nullptr) {
        item_stack_obj = sdk::jni->CallObjectMethod(inventory_obj, get_stack, slot_id);
        if (sdk::jni->ExceptionCheck()) { sdk::jni->ExceptionClear(); item_stack_obj = nullptr; }
    }

    sdk::jni->DeleteLocalRef(inventory_class);
    sdk::jni->DeleteLocalRef(inventory_obj);

    return mapper::__item_stack(item_stack_obj);
}

// ==========================================
// ==========================================
int mapper::__player::get_current_slot()
{
    mapper::__field field = mapper::classes["EntityPlayer"].get_field("inventory", mapper::classes["InventoryPlayer"].signature);
    if (field.identifier == nullptr) field = mapper::classes["EntityPlayer"].get_field("field_71071_by", mapper::classes["InventoryPlayer"].signature);
    if (field.identifier == nullptr) field = mapper::classes["EntityPlayer"].get_field("bg", mapper::classes["InventoryPlayer"].signature); // 1.8.9 / 1.7.10
    if (field.identifier == nullptr) field = mapper::classes["EntityPlayer"].get_field("bi", mapper::classes["InventoryPlayer"].signature);

    if (field.identifier == nullptr) return 0;
    jobject inventory_obj = sdk::jni->GetObjectField(this->object, field.identifier);
    if (inventory_obj == nullptr) return 0;

    mapper::__field slot_field = mapper::classes["InventoryPlayer"].get_field("currentItem", "I");
    if (slot_field.identifier == nullptr) slot_field = mapper::classes["InventoryPlayer"].get_field("field_70461_c", "I");
    if (slot_field.identifier == nullptr) slot_field = mapper::classes["InventoryPlayer"].get_field("c", "I");

    if (slot_field.identifier == nullptr) {
        sdk::jni->DeleteLocalRef(inventory_obj);
        return 0;
    }

    int slot = sdk::jni->GetIntField(inventory_obj, slot_field.identifier);
    sdk::jni->DeleteLocalRef(inventory_obj);
    return slot;
}

void mapper::__player::set_current_slot(int slot)
{
    mapper::__field field = mapper::classes["EntityPlayer"].get_field("inventory", mapper::classes["InventoryPlayer"].signature);
    if (field.identifier == nullptr) field = mapper::classes["EntityPlayer"].get_field("field_71071_by", mapper::classes["InventoryPlayer"].signature);
    if (field.identifier == nullptr) field = mapper::classes["EntityPlayer"].get_field("bg", mapper::classes["InventoryPlayer"].signature); // 1.8.9 / 1.7.10
    if (field.identifier == nullptr) field = mapper::classes["EntityPlayer"].get_field("bi", mapper::classes["InventoryPlayer"].signature);

    if (field.identifier == nullptr) return;
    jobject inventory_obj = sdk::jni->GetObjectField(this->object, field.identifier);
    if (inventory_obj == nullptr) return;

    mapper::__field slot_field = mapper::classes["InventoryPlayer"].get_field("currentItem", "I");
    if (slot_field.identifier == nullptr) slot_field = mapper::classes["InventoryPlayer"].get_field("field_70461_c", "I");
    if (slot_field.identifier == nullptr) slot_field = mapper::classes["InventoryPlayer"].get_field("c", "I");

    if (slot_field.identifier == nullptr) {
        sdk::jni->DeleteLocalRef(inventory_obj);
        return;
    }

    sdk::jni->SetIntField(inventory_obj, slot_field.identifier, slot);
    sdk::jni->DeleteLocalRef(inventory_obj);
}
// ==========================================
// ==========================================

bool mapper::__player::is_invisible()
{
    mapper::__method method = mapper::classes["EntityPlayerXP"].get_method("isInvisible", "()Z");
    if (method.identifier == nullptr) method = mapper::classes["EntityPlayerXP"].get_method("func_70070_b", "()Z");
    if (method.identifier == nullptr) method = mapper::classes["EntityPlayerXP"].get_method("P", "()Z");

    if (method.identifier == nullptr) return false;
    return sdk::jni->CallBooleanMethod(this->object, method.identifier);
}

float mapper::__player::get_max_health()
{
    mapper::__method method = mapper::classes["EntityPlayerXP"].get_method("getMaxHealth", "()F");
    if (method.identifier == nullptr) method = mapper::classes["EntityPlayerXP"].get_method("func_110148_a", "()F");
    if (method.identifier == nullptr) method = mapper::classes["EntityPlayerXP"].get_method("bm", "()F");
    if (method.identifier == nullptr) method = mapper::classes["EntityPlayerXP"].get_method("bn", "()F"); // 1.7.10

    if (method.identifier == nullptr) return 20.0f;
    return sdk::jni->CallFloatMethod(this->object, method.identifier);
}

bool mapper::__player::is_using_item()
{
    if (this->object == nullptr) return false;

    mapper::__field field = mapper::classes["EntityPlayer"].get_field("itemInUseCount", "I");
    if (field.identifier == nullptr) field = mapper::classes["EntityPlayer"].get_field("field_71072_f", "I");
    if (field.identifier == nullptr) field = mapper::classes["EntityPlayer"].get_field("h", "I"); // Obfuscado 1.8.9

    if (field.identifier != nullptr) {
        int count = sdk::jni->GetIntField(this->object, field.identifier);
        if (count > 0) return true;
    }

    mapper::__method method = mapper::classes["EntityPlayer"].get_method("isUsingItem", "()Z");
    if (method.identifier == nullptr) method = mapper::classes["EntityPlayer"].get_method("func_71039_bw", "()Z");
    if (method.identifier == nullptr) method = mapper::classes["EntityPlayer"].get_method("bS", "()Z");
    if (method.identifier == nullptr) method = mapper::classes["EntityPlayer"].get_method("bQ", "()Z");
    if (method.identifier == nullptr) method = mapper::classes["EntityPlayer"].get_method("bR", "()Z");

    if (method.identifier != nullptr) {
        return sdk::jni->CallBooleanMethod(this->object, method.identifier);
    }

    return false;
}

void mapper::__player::send_packet(jobject packet)
{
    if (this->object == nullptr || packet == nullptr) return;

    std::string sig = "L" + mapper::classes["NetHandlerPlayClient"].name + ";";
    mapper::__field field = mapper::classes["EntityPlayerXP"].get_field("sendQueue", sig);
    if (!field.identifier) field = mapper::classes["EntityPlayerXP"].get_field("field_71174_a", sig);
    if (!field.identifier) field = mapper::classes["EntityPlayerXP"].get_field("a", sig);

    if (field.identifier == nullptr) return;

    jobject send_queue = sdk::jni->GetObjectField(this->object, field.identifier);
    if (send_queue == nullptr) return;

    std::string method_sig = "(" + mapper::classes["Packet"].signature + ")V";
    mapper::__method method = mapper::classes["NetHandlerPlayClient"].get_method("addToSendQueue", method_sig);
    if (!method.identifier) method = mapper::classes["NetHandlerPlayClient"].get_method("func_147297_a", method_sig);
    if (!method.identifier) method = mapper::classes["NetHandlerPlayClient"].get_method("a", method_sig);

    if (method.identifier != nullptr) {
        sdk::jni->CallVoidMethod(send_queue, method.identifier, packet);
        if (sdk::jni->ExceptionCheck()) sdk::jni->ExceptionClear();
    }
    sdk::jni->DeleteLocalRef(send_queue);
}
