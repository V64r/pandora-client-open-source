#include "mapper.hpp"

mapper::__timer::__timer(jobject object)
{
    this->object = object;
}

mapper::__timer::__timer(const mapper::__timer& timer)
{
    if (timer.object != nullptr)
        this->object = sdk::jni->NewLocalRef(timer.object);
}

mapper::__timer::~__timer()
{
    if (this->object != nullptr)
        sdk::jni->DeleteLocalRef(this->object);
}

float mapper::__timer::get_partial_ticks()
{
    mapper::__field field = mapper::classes["Timer"].get_field("renderPartialTicks", "F");

    if (field.identifier == nullptr)
        field = mapper::classes["Timer"].get_field("field_74281_c", "F");

    if (field.identifier == nullptr)
        field = mapper::classes["Timer"].get_field("c", "F"); // 1.8.9 Lunar/Vanilla

    if (field.identifier == nullptr)
        field = mapper::classes["Timer"].get_field("b", "F"); // 1.7.10 Lunar/Vanilla

    if (field.identifier == nullptr) return 0.f;
    return sdk::jni->GetFloatField(this->object, field.identifier);
}
