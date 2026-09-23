#include "mapper.hpp"
#include <cmath>


mapper::__vec3__::__vec3__(mapper::__vec3 vec3)
{
    // PRIMERO intentar constructor directo (1.8.9)
    mapper::__method method = mapper::classes["Vec3"].get_method("<init>", "(DDD)V");

    if (method.identifier != nullptr && mapper::classes["Vec3"].klass != nullptr) {
        this->object = sdk::jni->NewObject(mapper::classes["Vec3"].klass, method.identifier, vec3.x, vec3.y, vec3.z);
        if (sdk::jni->ExceptionCheck()) { sdk::jni->ExceptionClear(); this->object = nullptr; }
        if (this->object != nullptr) return;
    }

    if (mapper::classes["Vec3"].klass != nullptr) {
        std::string sig = "(DDD)" + mapper::classes["Vec3"].signature;

        mapper::__method factory = mapper::classes["Vec3"].get_method("createVectorHelper", sig);
        if (!factory.identifier) factory = mapper::classes["Vec3"].get_method("func_72443_a", sig);
        if (!factory.identifier) factory = mapper::classes["Vec3"].get_method("a", sig);

        if (factory.identifier) {
            this->object = sdk::jni->CallStaticObjectMethod(mapper::classes["Vec3"].klass, factory.identifier, vec3.x, vec3.y, vec3.z);
            if (sdk::jni->ExceptionCheck()) { sdk::jni->ExceptionClear(); this->object = nullptr; }
            return;
        }
    }

    this->object = nullptr;
}

mapper::__vec3__::__vec3__(jobject object)
{
    this->object = object;
}

mapper::__vec3__::__vec3__(const mapper::__vec3__& vec3)
{
    if (vec3.object != nullptr)
        this->object = sdk::jni->NewLocalRef(vec3.object);
}

mapper::__vec3__::~__vec3__()
{
    if (this->object != nullptr)
        sdk::jni->DeleteLocalRef(this->object);
}

// --- LOGICA MATEMATICA NATIVA DE C++ ---

double mapper::__vec3::get_distance_to_vec3(mapper::__vec3 vec3)
{
    double dx = this->x - vec3.x;
    double dy = this->y - vec3.y;
    double dz = this->z - vec3.z;

    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

float mapper::__vec3::get_angle_x_difference_to_vec3(mapper::__vec3 target, float angle)
{
    double dx = target.x - this->x;
    double dz = target.z - this->z;

    double target_yaw = std::atan2(dz, dx) * 180.0 / 3.14159265358979323846 - 90.0;

    float angle_x_difference = std::fmod((float)target_yaw - angle, 360.0f);

    if (angle_x_difference >= 180.0f)
        angle_x_difference -= 360.0f;

    if (angle_x_difference < -180.0f)
        angle_x_difference += 360.0f;

    return angle_x_difference;
}

float mapper::__vec3::get_angle_y_difference_to_vec3(mapper::__vec3 target, float angle)
{
    double dx = target.x - this->x;
    double dy = this->y - target.y;
    double dz = target.z - this->z;

    double dist = std::sqrt(dx * dx + dz * dz);

    double target_pitch = std::atan2(dy, dist) * 180.0 / 3.14159265358979323846;

    return (float)target_pitch - angle;
}
