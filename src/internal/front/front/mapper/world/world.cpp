#include "mapper.hpp"

mapper::__world::__world(jobject object)
{
    this->object = object;
}

mapper::__world::__world(const mapper::__world& world)
{
    if (world.object != nullptr)
        this->object = sdk::jni->NewLocalRef(world.object);
}

mapper::__world::~__world()
{
    if (this->object != nullptr)
        sdk::jni->DeleteLocalRef(this->object);
}

std::vector<mapper::__player> mapper::__world::get_players()
{
    std::vector<mapper::__player> players;
    mapper::__field field = mapper::classes["WorldClient"].get_field("playerEntities", mapper::classes["List"].signature);
    if (!field.identifier) field = mapper::classes["WorldClient"].get_field("field_73010_i", mapper::classes["List"].signature);
    if (!field.identifier) field = mapper::classes["WorldClient"].get_field("j", mapper::classes["List"].signature);
    if (!field.identifier) field = mapper::classes["WorldClient"].get_field("i", mapper::classes["List"].signature);
    if (!field.identifier) field = mapper::classes["WorldClient"].get_field("h", mapper::classes["List"].signature);
    if (!field.identifier) return players;

    jobject list = sdk::jni->GetObjectField(this->object, field.identifier);
    if (!list) return players;
    mapper::__method to_array = mapper::classes["List"].get_method("toArray", "()[Ljava/lang/Object;");
    if (!to_array.identifier) { sdk::jni->DeleteLocalRef(list); return players; }
    jobjectArray array = static_cast<jobjectArray>(sdk::jni->CallObjectMethod(list, to_array.identifier));
    if (sdk::jni->ExceptionCheck()) { sdk::jni->ExceptionClear(); sdk::jni->DeleteLocalRef(list); return players; }

    if (array) {
        const jsize length = sdk::jni->GetArrayLength(array);
        players.reserve(length);
        for (jsize i = 0; i < length; ++i) {
            jobject player = sdk::jni->GetObjectArrayElement(array, i);
            if (player) players.emplace_back(player);
        }
        sdk::jni->DeleteLocalRef(array);
    }
    sdk::jni->DeleteLocalRef(list);
    return players;
}
