#include "mapper.hpp"

mapper::__render_manager::__render_manager(jobject object)
{
    this->object = object;
}

mapper::__render_manager::__render_manager(const mapper::__render_manager& render_manager)
{
    if (render_manager.object != nullptr)
        this->object = sdk::jni->NewLocalRef(render_manager.object);
}

mapper::__render_manager::~__render_manager()
{
    if (this->object != nullptr)
        sdk::jni->DeleteLocalRef(this->object);
}

bool mapper::__render_manager::render_player(mapper::__player player, float partial_ticks)
{
    if (!this->object || !player.object) return false;

    // NO usar static: en 1.7 la firma cambia y el static se inicializa una sola vez
    std::string signature = "(" + mapper::classes["Entity"].signature + "F)Z";

    mapper::__method m = mapper::classes["RenderManager"].get_method("renderEntitySimple", signature);
    if (!m.identifier) m = mapper::classes["RenderManager"].get_method("func_147937_a", signature);
    if (!m.identifier) m = mapper::classes["RenderManager"].get_method("a", signature);

    if (m.identifier != nullptr) {
        jboolean result = sdk::jni->CallBooleanMethod(this->object, m.identifier, player.object, partial_ticks);
        if (sdk::jni->ExceptionCheck()) { sdk::jni->ExceptionClear(); return false; }
        return result;
    }

    // 1.7.10 fallback: renderEntityWithPosYaw tiene firma diferente
    // o renderEntitySimple no devuelve boolean sino void
    std::string sig_void = "(" + mapper::classes["Entity"].signature + "F)V";
    mapper::__method mv = mapper::classes["RenderManager"].get_method("renderEntitySimple", sig_void);
    if (!mv.identifier) mv = mapper::classes["RenderManager"].get_method("func_147937_a", sig_void);
    if (!mv.identifier) mv = mapper::classes["RenderManager"].get_method("a", sig_void);

    if (mv.identifier != nullptr) {
        sdk::jni->CallVoidMethod(this->object, mv.identifier, player.object, partial_ticks);
        if (sdk::jni->ExceptionCheck()) { sdk::jni->ExceptionClear(); return false; }
        return true;
    }

    return false;
}

bool mapper::__render_manager::render_player(mapper::__player player, double x, double y, double z, float yaw, float partial_ticks)
{
    if (!this->object || !player.object) return false;

    std::string signature = "(" + mapper::classes["Entity"].signature + "DDDFF)Z";

    mapper::__method m = mapper::classes["RenderManager"].get_method("renderEntityWithPosYaw", signature);
    if (!m.identifier) m = mapper::classes["RenderManager"].get_method("func_147940_a", signature);
    if (!m.identifier) m = mapper::classes["RenderManager"].get_method("a", signature);

    if (m.identifier != nullptr) {
        jboolean result = sdk::jni->CallBooleanMethod(this->object, m.identifier, player.object, x, y, z, yaw, partial_ticks);
        if (sdk::jni->ExceptionCheck()) { sdk::jni->ExceptionClear(); return false; }
        return result;
    }

    // 1.7.10 fallback: void return type
    std::string sig_void = "(" + mapper::classes["Entity"].signature + "DDDFF)V";
    mapper::__method mv = mapper::classes["RenderManager"].get_method("renderEntityWithPosYaw", sig_void);
    if (!mv.identifier) mv = mapper::classes["RenderManager"].get_method("func_147940_a", sig_void);
    if (!mv.identifier) mv = mapper::classes["RenderManager"].get_method("a", sig_void);

    if (mv.identifier != nullptr) {
        sdk::jni->CallVoidMethod(this->object, mv.identifier, player.object, x, y, z, yaw, partial_ticks);
        if (sdk::jni->ExceptionCheck()) { sdk::jni->ExceptionClear(); return false; }
        return true;
    }

    return false;
}

double mapper::__render_manager::get_render_pos_x()
{
    if (!this->object) return 0.0;
    // NO static: fields varian entre versiones
    mapper::__field f = mapper::classes["RenderManager"].get_field("renderPosX", "D");
    if (!f.identifier) f = mapper::classes["RenderManager"].get_field("field_78725_b", "D");
    if (!f.identifier) f = mapper::classes["RenderManager"].get_field("o", "D"); // 1.8 obf
    if (!f.identifier) f = mapper::classes["RenderManager"].get_field("h", "D"); // 1.7.10 obf
    if (!f.identifier) return 0.0;
    return sdk::jni->GetDoubleField(this->object, f.identifier);
}

double mapper::__render_manager::get_render_pos_y()
{
    if (!this->object) return 0.0;
    mapper::__field f = mapper::classes["RenderManager"].get_field("renderPosY", "D");
    if (!f.identifier) f = mapper::classes["RenderManager"].get_field("field_78726_c", "D");
    if (!f.identifier) f = mapper::classes["RenderManager"].get_field("p", "D"); // 1.8 obf
    if (!f.identifier) f = mapper::classes["RenderManager"].get_field("i", "D"); // 1.7.10 obf
    if (!f.identifier) return 0.0;
    return sdk::jni->GetDoubleField(this->object, f.identifier);
}

double mapper::__render_manager::get_render_pos_z()
{
    if (!this->object) return 0.0;
    mapper::__field f = mapper::classes["RenderManager"].get_field("renderPosZ", "D");
    if (!f.identifier) f = mapper::classes["RenderManager"].get_field("field_78723_d", "D");
    if (!f.identifier) f = mapper::classes["RenderManager"].get_field("q", "D"); // 1.8 obf
    if (!f.identifier) f = mapper::classes["RenderManager"].get_field("j", "D"); // 1.7.10 obf
    if (!f.identifier) return 0.0;
    return sdk::jni->GetDoubleField(this->object, f.identifier);
}
