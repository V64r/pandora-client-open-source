#include "mapper.hpp"

mapper::__settings::__settings(jobject object)
{
    this->object = object;
}

mapper::__settings::__settings(const mapper::__settings& settings)
{
    if (settings.object != nullptr)
        this->object = sdk::jni->NewLocalRef(settings.object);
}

mapper::__settings::~__settings()
{
    if (this->object != nullptr)
        sdk::jni->DeleteLocalRef(this->object);
}

static int s_original_use_item_keycode = 0;

void mapper::__settings::set_virtual_right_click(bool enable)
{
    if (this->object == nullptr) return;
    std::string sig = "L" + mapper::classes["KeyBinding"].name + ";";

    mapper::__field kb_fid = mapper::classes["GameSettings"].get_field("keyBindUseItem", sig);
    if (!kb_fid.identifier) kb_fid = mapper::classes["GameSettings"].get_field("field_74313_G", sig);
    if (!kb_fid.identifier) kb_fid = mapper::classes["GameSettings"].get_field("ag", sig);
    if (!kb_fid.identifier) kb_fid = mapper::classes["GameSettings"].get_field("Y", sig); // 1.7.10

    if (kb_fid.identifier) {
        jobject kb_obj = sdk::jni->GetObjectField(this->object, kb_fid.identifier);
        if (kb_obj) {
            mapper::__field pr_fid = mapper::classes["KeyBinding"].get_field("pressed", "Z");
            if (!pr_fid.identifier) pr_fid = mapper::classes["KeyBinding"].get_field("field_74513_e", "Z");
            if (!pr_fid.identifier) pr_fid = mapper::classes["KeyBinding"].get_field("i", "Z");
            if (!pr_fid.identifier) pr_fid = mapper::classes["KeyBinding"].get_field("h", "Z");
            if (!pr_fid.identifier) pr_fid = mapper::classes["KeyBinding"].get_field("g", "Z"); // 1.7.10

            mapper::__field code_fid = mapper::classes["KeyBinding"].get_field("keyCode", "I");
            if (!code_fid.identifier) code_fid = mapper::classes["KeyBinding"].get_field("field_151469_d", "I");
            if (!code_fid.identifier) code_fid = mapper::classes["KeyBinding"].get_field("d", "I");
            if (!code_fid.identifier) code_fid = mapper::classes["KeyBinding"].get_field("e", "I"); // 1.7.10

            if (pr_fid.identifier && code_fid.identifier) {
                int current_code = sdk::jni->GetIntField(kb_obj, code_fid.identifier);
                if (enable) {
                    if (current_code != 0) {
                        s_original_use_item_keycode = current_code;
                        sdk::jni->SetIntField(kb_obj, code_fid.identifier, 0);
                    }
                    sdk::jni->SetBooleanField(kb_obj, pr_fid.identifier, true);
                }
                else {
                    if (current_code == 0 && s_original_use_item_keycode != 0) {
                        sdk::jni->SetIntField(kb_obj, code_fid.identifier, s_original_use_item_keycode);
                    }
                    sdk::jni->SetBooleanField(kb_obj, pr_fid.identifier, false);
                }
            }
            sdk::jni->DeleteLocalRef(kb_obj);
        }
    }
}
