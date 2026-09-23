#pragma once
#include "../features.hpp"
#include <windows.h>
#include <initializer_list>

namespace features::misc::input_support {
inline jfieldID field(const char* type, const std::string& signature,
    std::initializer_list<const char*> names) {
    const auto found = mapper::classes.find(type);
    if (found == mapper::classes.end()) return nullptr;
    for (const auto name : names) {
        auto id = found->second.get_field(name, signature).identifier;
        if (id) return id;
    }
    return nullptr;
}
inline jmethodID method(const char* type, const std::string& signature,
    std::initializer_list<const char*> names) {
    const auto found = mapper::classes.find(type);
    if (found == mapper::classes.end()) return nullptr;
    for (const auto name : names) {
        auto id = found->second.get_method(name, signature).identifier;
        if (id) return id;
    }
    return nullptr;
}
inline bool physical_key(int lwjgl_code) {
    if (lwjgl_code < 0) {
        const int mouse = lwjgl_code + 100;
        const int keys[]{VK_LBUTTON, VK_RBUTTON, VK_MBUTTON, VK_XBUTTON1, VK_XBUTTON2};
        return mouse >= 0 && mouse < 5 && (GetAsyncKeyState(keys[mouse]) & 0x8000);
    }
    if (lwjgl_code <= 0 || lwjgl_code > 255) return false;
    // LWJGL scan code 42 = Left Shift, 54 = Right Shift.
    // MapVirtualKeyW can return VK_LSHIFT/VK_RSHIFT but GetAsyncKeyState
    // with VK_SHIFT covers both sides reliably.
    if (lwjgl_code == 42 || lwjgl_code == 54)
        return (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
    const UINT scan = (lwjgl_code & 0x80) ? 0xE000 | (lwjgl_code & 0x7F) : lwjgl_code;
    const UINT vk = MapVirtualKeyW(scan, MAPVK_VSC_TO_VK_EX);
    return vk && (GetAsyncKeyState(vk) & 0x8000);
}
inline int stack_count(mapper::__item_stack& stack) {
    if (!stack.object) return 0;
    const auto id = field("ItemStack", "I", {"stackSize", "field_77994_a", "b"});
    return id ? sdk::jni->GetIntField(stack.object, id) : 0;
}
inline bool block_stack(mapper::__item_stack& stack) {
    if (!stack.object || stack_count(stack) <= 0 || sdk::jni->ExceptionCheck()) return false;
    auto item = stack.get_item();
    return item.object && !sdk::jni->ExceptionCheck() && item.is_block();
}
}
