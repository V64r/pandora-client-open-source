#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include "../../front/back/misc/imgui/backends/imgui.h"
#ifdef IMGUI_ENABLE_FREETYPE
#include "../../front/back/misc/imgui/misc/freetype/imgui_freetype.h"
#else
#define ImGuiFreeTypeBuilderFlags_NoHinting 0
#define ImGuiFreeTypeBuilderFlags_ForceAutoHint 0
#define ImGuiFreeTypeBuilderFlags_MonoHinting 0
#define ImGuiFreeTypeBuilderFlags_Monochrome 0
#define ImGuiFreeTypeBuilderFlags_LightHinting 0
#endif

namespace math {
    struct c_vector_2d {
        union { struct { float x, y; }; struct { float m_x, m_y; }; };
        constexpr c_vector_2d() : x(0), y(0) {}
        constexpr c_vector_2d(float px, float py) : x(px), y(py) {}
        constexpr c_vector_2d(const ImVec2& v) : x(v.x), y(v.y) {}
        constexpr ImVec2 transform() const { return ImVec2(x, y); }
        constexpr operator ImVec2() const { return transform(); }
        constexpr c_vector_2d operator+(const c_vector_2d& v) const { return {x+v.x,y+v.y}; }
        constexpr c_vector_2d operator-(const c_vector_2d& v) const { return {x-v.x,y-v.y}; }
        constexpr c_vector_2d operator*(float v) const { return {x*v,y*v}; }
        constexpr c_vector_2d operator/(float v) const { return {x/v,y/v}; }
        c_vector_2d& operator+=(const c_vector_2d& v) { x+=v.x; y+=v.y; return *this; }
        c_vector_2d& operator-=(const c_vector_2d& v) { x-=v.x; y-=v.y; return *this; }
    };
    struct c_rect {
        float x{}, y{}, w{}, h{};
        constexpr c_rect() = default;
        constexpr c_rect(float px,float py,float pw,float ph):x(px),y(py),w(pw),h(ph){}
        constexpr c_rect(c_vector_2d p,c_vector_2d s):x(p.x),y(p.y),w(s.x),h(s.y){}
        constexpr c_vector_2d pos() const { return {x,y}; }
        constexpr c_vector_2d position() const { return {x,y}; }
        constexpr c_vector_2d size() const { return {w,h}; }
    };
    using rect_t = c_rect;
}

namespace hue {
    struct c_color {
        int r{255}, g{255}, b{255}, a{255};
        constexpr c_color() = default;
        constexpr c_color(int red,int green,int blue,int alpha=255):r(red),g(green),b(blue),a(alpha){}
        ImU32 transform() const { return IM_COL32(std::clamp(r,0,255),std::clamp(g,0,255),std::clamp(b,0,255),std::clamp(a,0,255)); }
        c_color modulate(float factor) const { return c_color(r,g,b,(int)std::clamp(a*factor,0.f,255.f)); }
        constexpr c_color with_alpha(int alpha) const { return c_color(r,g,b,alpha); }
        constexpr c_color alpha(int alpha) const { return with_alpha(alpha); }
        c_color lerp(const c_color& to,float t) const { t=std::clamp(t,0.f,1.f); return c_color((int)(r+(to.r-r)*t),(int)(g+(to.g-g)*t),(int)(b+(to.b-b)*t),(int)(a+(to.a-a)*t)); }
        ImU32 get_u32() const { return transform(); }
    };
}

#ifndef IMGUI_ENABLE_FREETYPE
namespace ImGuiFreeType { inline bool BuildFontAtlas(ImFontAtlas* atlas, unsigned int) { return atlas ? atlas->Build() : false; } }
#endif
