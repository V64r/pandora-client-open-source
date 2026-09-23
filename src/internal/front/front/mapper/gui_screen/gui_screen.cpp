#include "mapper.hpp"

mapper::__gui_screen::__gui_screen(jobject object)
{
    this->object = object;
}

mapper::__gui_screen::__gui_screen(const mapper::__gui_screen& gui_screen)
{
    if (gui_screen.object != nullptr)
        this->object = sdk::jni->NewLocalRef(gui_screen.object);
}

mapper::__gui_screen::~__gui_screen()
{
    if (this->object != nullptr)
        sdk::jni->DeleteLocalRef(this->object);
}
