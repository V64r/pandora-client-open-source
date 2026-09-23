#include "mapper.hpp"

mapper::__item_stack::__item_stack(jobject object)
{
    this->object = object;
}

mapper::__item_stack::__item_stack(const mapper::__item_stack& item_stack)
{
    if (item_stack.object != nullptr)
        this->object = sdk::jni->NewLocalRef(item_stack.object);
}

mapper::__item_stack::~__item_stack()
{
    if (this->object != nullptr)
        sdk::jni->DeleteLocalRef(this->object);
}

mapper::__item mapper::__item_stack::get_item()
{
    mapper::__field field = mapper::classes["ItemStack"].get_field("item", "L" + mapper::classes["Item"].name + ";");

    if (field.identifier == nullptr)
        field = mapper::classes["ItemStack"].get_field("field_151002_e", "L" + mapper::classes["Item"].name + ";");

    if (field.identifier == nullptr)
        field = mapper::classes["ItemStack"].get_field("d", "L" + mapper::classes["Item"].name + ";"); // 1.8.9 obf

    if (field.identifier == nullptr)
        field = mapper::classes["ItemStack"].get_field("e", "L" + mapper::classes["Item"].name + ";"); // 1.7.10 obf

    if (field.identifier != nullptr) {
        jobject item_obj = sdk::jni->GetObjectField(this->object, field.identifier);
        if (item_obj != nullptr) return mapper::__item(item_obj);
    }
    
    std::string method_sig = "()L" + mapper::classes["Item"].name + ";";
    mapper::__method method = mapper::classes["ItemStack"].get_method("getItem", method_sig);
    if (method.identifier == nullptr) method = mapper::classes["ItemStack"].get_method("func_77973_b", method_sig);
    if (method.identifier == nullptr) method = mapper::classes["ItemStack"].get_method("b", method_sig); // 1.7.10 / 1.8.9

    if (method.identifier != nullptr) {
        jobject result = sdk::jni->CallObjectMethod(this->object, method.identifier);
        if (sdk::jni->ExceptionCheck()) sdk::jni->ExceptionClear();
        return mapper::__item(result);
    }

    return mapper::__item(nullptr);
}

bool mapper::__item_stack::is_potion()
{
    auto item = get_item();
    if (!item.object) return false;
    return item.is_potion();
}
int mapper::__item_stack::get_item_damage() {
    if (!this->object) return 0;
    jclass cls = sdk::jni->GetObjectClass(this->object);
    if (!cls) return 0;
    jfieldID fid = sdk::jni->GetFieldID(cls, "itemDamage", "I");
    if (!fid) { sdk::jni->ExceptionClear(); fid = sdk::jni->GetFieldID(cls, "field_77991_e", "I"); }
    if (!fid) { sdk::jni->ExceptionClear(); fid = sdk::jni->GetFieldID(cls, "e", "I"); }
    if (!fid) { sdk::jni->ExceptionClear(); sdk::jni->DeleteLocalRef(cls); return 0; }
    int result = sdk::jni->GetIntField(this->object, fid);
    sdk::jni->DeleteLocalRef(cls);
    return result;
}

int mapper::__item_stack::get_max_damage() {
    if (!this->object) return 0;
    std::string sig = "()I";
    mapper::__method method = mapper::classes["ItemStack"].get_method("getMaxDamage", sig);
    if (!method.identifier) method = mapper::classes["ItemStack"].get_method("func_77976_d", sig);
    if (!method.identifier) method = mapper::classes["ItemStack"].get_method("j", sig);
    if (method.identifier) {
        int res = sdk::jni->CallIntMethod(this->object, method.identifier);
        if (sdk::jni->ExceptionCheck()) sdk::jni->ExceptionClear();
        return res;
    }
    return 0;
}
