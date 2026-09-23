// Intentionally empty.
//
// This translation unit used to define hooks::clear(JNIEnv*, jclass, jint, jlong),
// an early JNI-side matrix-capture path. It was fully superseded by
// hooks::gl_clear_hook (see hooks.cpp) plus capture_render_frame_java_state, and
// nothing referenced it anymore (no declaration in hooks.hpp, no address taken,
// no jnihook registration). The dead code was removed; the file stays in the
// project so the .vcxproj reference remains valid.
