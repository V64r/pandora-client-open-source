#include "mapper.hpp"

mapper::__item::__item(jobject object)
{
    this->object = object;
}

mapper::__item::__item(const mapper::__item& item)
{
    if (item.object != nullptr)
        this->object = sdk::jni->NewLocalRef(item.object);
}

mapper::__item::~__item()
{
    if (this->object != nullptr)
        sdk::jni->DeleteLocalRef(this->object);
}

__int32 mapper::__item::get_id()
{
    if (this->object == nullptr || mapper::classes["Item"].klass == nullptr) return 0;

    std::string signature = "(" + mapper::classes["Item"].signature + ")I";

    mapper::__method method = mapper::classes["Item"].get_method("getIdFromItem", signature);
    if (method.identifier == nullptr)
        method = mapper::classes["Item"].get_method("func_150891_b", signature);
    if (method.identifier == nullptr)
        method = mapper::classes["Item"].get_method("b", signature); // 1.8.9 Lunar/Vanilla
    if (method.identifier == nullptr)
        method = mapper::classes["Item"].get_method("a", signature); // 1.7.10 alt

    if (method.identifier == nullptr) return 0;
    jint result = sdk::jni->CallStaticIntMethod(mapper::classes["Item"].klass, method.identifier, this->object);
    if (sdk::jni->ExceptionCheck()) { sdk::jni->ExceptionClear(); return 0; }
    return result;
}

bool mapper::__item::is_sword()
{
    __int32 id = this->get_id();
    return id == 267 || id == 268 || id == 272 || id == 276 || id == 283;
}

bool mapper::__item::is_axe()
{
    __int32 id = this->get_id();
    return id == 258 || id == 271 || id == 275 || id == 279 || id == 286;
}

bool mapper::__item::is_pickaxe()
{
    __int32 id = this->get_id();
    return id == 278 || id == 257 || id == 274 || id == 270 || id == 285;
}

bool mapper::__item::is_shovel()
{
    __int32 id = this->get_id();
    return id == 277 || id == 256 || id == 284 || id == 273 || id == 269;
}

bool mapper::__item::is_block()
{
    __int32 id = this->get_id();
    return (id >= 1 && id <= 5) || id == 7 || (id >= 13 && id <= 30) || (id >= 33 && id <= 35) ||
        (id >= 41 && id <= 49) || (id >= 52 && id <= 53) || (id >= 56 && id <= 58) ||
        (id >= 60 && id <= 62) || id == 67 || (id >= 73 && id <= 74) || (id >= 78 && id <= 80) ||
        id == 82 || (id >= 84 && id <= 91) || (id >= 95 && id <= 103) || (id >= 108 && id <= 110) ||
        (id >= 112 && id <= 114) || (id >= 120 && id <= 121) || (id >= 123 && id <= 126) ||
        (id >= 128 && id <= 129) || (id >= 133 && id <= 139) || (id >= 152 && id <= 153) ||
        (id >= 155 && id <= 156) || (id >= 158 && id <= 166) || (id >= 168 && id <= 170) ||
        (id >= 172 && id <= 174) || (id >= 178 && id <= 182);
}

bool mapper::__item::is_potion()
{
    return this->get_id() == 373;
}

bool mapper::__item::is_soup()
{
    return this->get_id() == 282; // Mushroom Stew
}

bool mapper::__item::is_food()
{
    __int32 id = this->get_id();
    return id == 260 || id == 282 || id == 297 || id == 319 || id == 320 || id == 322 || 
           id == 349 || id == 350 || id == 357 || id == 360 || id == 363 || id == 364 || 
           id == 365 || id == 366 || id == 367 || id == 375 || id == 391 || id == 392 || 
           id == 393 || id == 394 || id == 400 || id == 411 || id == 412 || id == 413 || 
           id == 423 || id == 424;
}
