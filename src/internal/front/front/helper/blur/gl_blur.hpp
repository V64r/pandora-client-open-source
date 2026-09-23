#pragma once
// GL Blur implementation using ImGui's OpenGL3 backend infrastructure.
// Uses the same GL loader that ImGui uses (imgl3w) plus manually loaded FBO functions.
// Works inside ImGui's AddCallback system by properly saving/restoring ImGui's GL state.

#include <windows.h>
#include <gl/GL.h>
#include "backends/imgui.h"

// ---- Types for functions NOT in ImGui's gl3w loader ----
typedef void   (APIENTRY* PFN_glGenFramebuffers)(GLsizei n, GLuint* framebuffers);
typedef void   (APIENTRY* PFN_glBindFramebuffer)(GLenum target, GLuint framebuffer);
typedef void   (APIENTRY* PFN_glFramebufferTexture2D)(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
typedef void   (APIENTRY* PFN_glDeleteFramebuffers)(GLsizei n, const GLuint* framebuffers);
typedef void   (APIENTRY* PFN_glUniform1f)(GLint location, GLfloat v0);
typedef void   (APIENTRY* PFN_glUniform2f)(GLint location, GLfloat v0, GLfloat v1);
typedef void   (APIENTRY* PFN_glCopyTexImage2D)(GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border);

// ---- GL constants not in base headers ----
#ifndef GL_FRAMEBUFFER
#define GL_FRAMEBUFFER 0x8D40
#endif
#ifndef GL_COLOR_ATTACHMENT0
#define GL_COLOR_ATTACHMENT0 0x8CE0
#endif
#ifndef GL_FRAMEBUFFER_BINDING
#define GL_FRAMEBUFFER_BINDING 0x8CA6
#endif
#ifndef GL_CURRENT_PROGRAM
#define GL_CURRENT_PROGRAM 0x8B8D
#endif
#ifndef GL_FRAGMENT_SHADER
#define GL_FRAGMENT_SHADER 0x8B30
#endif
#ifndef GL_VERTEX_SHADER
#define GL_VERTEX_SHADER 0x8B31
#endif
#ifndef GL_COMPILE_STATUS
#define GL_COMPILE_STATUS 0x8B81
#endif
#ifndef GL_LINK_STATUS
#define GL_LINK_STATUS 0x8B82
#endif
#ifndef GL_TEXTURE0
#define GL_TEXTURE0 0x84C0
#endif
#ifndef GL_ARRAY_BUFFER
#define GL_ARRAY_BUFFER 0x8892
#endif
#ifndef GL_ELEMENT_ARRAY_BUFFER
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#endif
#ifndef GL_ARRAY_BUFFER_BINDING
#define GL_ARRAY_BUFFER_BINDING 0x8894
#endif
#ifndef GL_ELEMENT_ARRAY_BUFFER_BINDING
#define GL_ELEMENT_ARRAY_BUFFER_BINDING 0x8895
#endif
#ifndef GL_VERTEX_ARRAY_BINDING
#define GL_VERTEX_ARRAY_BINDING 0x85B5
#endif
#ifndef GL_ACTIVE_TEXTURE
#define GL_ACTIVE_TEXTURE 0x84E0
#endif
#ifndef GL_TEXTURE_BINDING_2D
#define GL_TEXTURE_BINDING_2D 0x8069
#endif

namespace GLBlur {

    // ---- Manually loaded function pointers (not in ImGui's loader) ----
    inline PFN_glGenFramebuffers       pglGenFramebuffers = nullptr;
    inline PFN_glBindFramebuffer       pglBindFramebuffer = nullptr;
    inline PFN_glFramebufferTexture2D  pglFramebufferTexture2D = nullptr;
    inline PFN_glDeleteFramebuffers    pglDeleteFramebuffers = nullptr;
    inline PFN_glUniform1f             pglUniform1f = nullptr;
    inline PFN_glUniform2f             pglUniform2f = nullptr;
    inline PFN_glCopyTexImage2D        pglCopyTexImage2D = nullptr;

    // ---- ImGui's loader provides these (via imgl3w macros in imgui_impl_opengl3.cpp) ----
    // We load them ourselves via wglGetProcAddress since we're in a different compilation unit.
    typedef GLuint (APIENTRY* PFN_glCreateShader)(GLenum);
    typedef void   (APIENTRY* PFN_glShaderSource)(GLuint, GLsizei, const char* const*, const GLint*);
    typedef void   (APIENTRY* PFN_glCompileShader)(GLuint);
    typedef void   (APIENTRY* PFN_glGetShaderiv)(GLuint, GLenum, GLint*);
    typedef GLuint (APIENTRY* PFN_glCreateProgram)(void);
    typedef void   (APIENTRY* PFN_glAttachShader)(GLuint, GLuint);
    typedef void   (APIENTRY* PFN_glLinkProgram)(GLuint);
    typedef void   (APIENTRY* PFN_glGetProgramiv)(GLuint, GLenum, GLint*);
    typedef void   (APIENTRY* PFN_glUseProgram)(GLuint);
    typedef GLint  (APIENTRY* PFN_glGetUniformLocation)(GLuint, const char*);
    typedef void   (APIENTRY* PFN_glUniform1i)(GLint, GLint);
    typedef void   (APIENTRY* PFN_glDeleteShader)(GLuint);
    typedef void   (APIENTRY* PFN_glDeleteProgram)(GLuint);
    typedef void   (APIENTRY* PFN_glActiveTexture)(GLenum);
    typedef void   (APIENTRY* PFN_glBindBuffer)(GLenum, GLuint);
    typedef GLint  (APIENTRY* PFN_glGetAttribLocation)(GLuint, const char*);

    inline PFN_glCreateShader       pglCreateShader = nullptr;
    inline PFN_glShaderSource       pglShaderSource = nullptr;
    inline PFN_glCompileShader      pglCompileShader = nullptr;
    inline PFN_glGetShaderiv        pglGetShaderiv = nullptr;
    inline PFN_glCreateProgram      pglCreateProgram = nullptr;
    inline PFN_glAttachShader       pglAttachShader = nullptr;
    inline PFN_glLinkProgram        pglLinkProgram = nullptr;
    inline PFN_glGetProgramiv       pglGetProgramiv = nullptr;
    inline PFN_glUseProgram         pglUseProgram = nullptr;
    inline PFN_glGetUniformLocation pglGetUniformLocation = nullptr;
    inline PFN_glGetAttribLocation  pglGetAttribLocation = nullptr;
    inline PFN_glUniform1i          pglUniform1i = nullptr;
    inline PFN_glDeleteShader       pglDeleteShader = nullptr;
    inline PFN_glDeleteProgram      pglDeleteProgram = nullptr;
    inline PFN_glActiveTexture      pglActiveTexture = nullptr;
    inline PFN_glBindBuffer         pglBindBuffer = nullptr;

    // ---- State ----
    inline bool  Initialized = false;
    inline bool  InitFailed = false;
    inline GLuint BlurProgram = 0;
    inline GLint  LocImage = -1;
    inline GLint  LocDir = -1;
    inline GLint  LocRadius = -1;
    inline GLint  LocTexOffset = -1;
    inline GLint  AttrPosition = -1;
    inline GLint  AttrUV = -1;
    inline GLuint PingPongFBO[2] = {0, 0};
    inline GLuint PingPongTex[2] = {0, 0};
    inline int    FBOWidth = 0;
    inline int    FBOHeight = 0;

    // ---- Shaders (GLSL 130 to match ImGui's default) ----
    inline const char* VS = R"(
#version 130
in vec2 Position;
in vec2 UV;
out vec2 Frag_UV;
void main() {
    Frag_UV = UV;
    gl_Position = vec4(Position, 0.0, 1.0);
}
)";

    inline const char* FS = R"(
#version 130
uniform sampler2D image;
uniform vec2 dir;
uniform float radius;
uniform vec2 tex_offset;
in vec2 Frag_UV;
out vec4 Out_Color;
void main() {
    vec4 result = texture(image, Frag_UV) * 0.227027;
    for (int i = 1; i < 5; ++i) {
        float w = 0.0;
        if (i == 1) w = 0.1945946;
        else if (i == 2) w = 0.1216216;
        else if (i == 3) w = 0.054054;
        else if (i == 4) w = 0.016216;
        result += texture(image, Frag_UV + vec2(float(i)) * tex_offset * dir * radius) * w;
        result += texture(image, Frag_UV - vec2(float(i)) * tex_offset * dir * radius) * w;
    }
    Out_Color = vec4(result.rgb, 1.0);
}
)";

    // ---- Fullscreen quad VBO (NDC coords + UVs) ----
    inline GLuint QuadVBO = 0;

    inline void LoadFunctions() {
        if (pglCreateShader) return;
        #define LOAD(name) p##name = (PFN_##name)wglGetProcAddress(#name)
        #define LOAD_FALLBACK(name, ext) do { \
            p##name = (PFN_##name)wglGetProcAddress(#name); \
            if (!p##name) p##name = (PFN_##name)wglGetProcAddress(#name #ext); \
        } while(0)
        
        LOAD(glCreateShader);
        LOAD(glShaderSource);
        LOAD(glCompileShader);
        LOAD(glGetShaderiv);
        LOAD(glCreateProgram);
        LOAD(glAttachShader);
        LOAD(glLinkProgram);
        LOAD(glGetProgramiv);
        LOAD(glUseProgram);
        LOAD(glGetUniformLocation);
        LOAD(glGetAttribLocation);
        LOAD(glUniform1i);
        LOAD(glUniform1f);
        LOAD(glUniform2f);
        LOAD(glDeleteShader);
        LOAD(glDeleteProgram);
        LOAD(glActiveTexture);
        
        LOAD_FALLBACK(glBindBuffer, ARB);
        LOAD_FALLBACK(glGenFramebuffers, EXT);
        LOAD_FALLBACK(glBindFramebuffer, EXT);
        LOAD_FALLBACK(glFramebufferTexture2D, EXT);
        LOAD_FALLBACK(glDeleteFramebuffers, EXT);
        
        LOAD(glCopyTexImage2D); // Base GL 1.1, but might need loading depending on header
        if (!pglCopyTexImage2D) {
            HMODULE hMod = GetModuleHandleA("opengl32.dll");
            if (hMod) pglCopyTexImage2D = (PFN_glCopyTexImage2D)GetProcAddress(hMod, "glCopyTexImage2D");
        }
        #undef LOAD
        #undef LOAD_FALLBACK
    }

    typedef void (APIENTRY* PFN_glGenBuffers)(GLsizei, GLuint*);
    typedef void (APIENTRY* PFN_glBufferData)(GLenum, ptrdiff_t, const void*, GLenum);
    typedef void (APIENTRY* PFN_glEnableVertexAttribArray)(GLuint);
    typedef void (APIENTRY* PFN_glVertexAttribPointer)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*);

    inline PFN_glGenBuffers             pglGenBuffers = nullptr;
    inline PFN_glBufferData             pglBufferData = nullptr;
    inline PFN_glEnableVertexAttribArray pglEnableVertexAttribArray = nullptr;
    inline PFN_glVertexAttribPointer    pglVertexAttribPointer = nullptr;

    inline void LoadFunctions2() {
        if (pglGenBuffers) return;
        #define LOAD_FALLBACK(name, ext) do { \
            p##name = (PFN_##name)wglGetProcAddress(#name); \
            if (!p##name) p##name = (PFN_##name)wglGetProcAddress(#name #ext); \
        } while(0)
        
        LOAD_FALLBACK(glGenBuffers, ARB);
        LOAD_FALLBACK(glBufferData, ARB);
        LOAD_FALLBACK(glEnableVertexAttribArray, ARB);
        LOAD_FALLBACK(glVertexAttribPointer, ARB);
        
        #undef LOAD_FALLBACK
    }

    inline void CreateQuadVBO() {
        if (QuadVBO) return;
        // Fullscreen quad: Position (x,y) + UV (u,v)
        float vertices[] = {
            // pos        // uv
            -1.0f,  1.0f, 0.0f, 1.0f,
            -1.0f, -1.0f, 0.0f, 0.0f,
             1.0f, -1.0f, 1.0f, 0.0f,

            -1.0f,  1.0f, 0.0f, 1.0f,
             1.0f, -1.0f, 1.0f, 0.0f,
             1.0f,  1.0f, 1.0f, 1.0f,
        };
        pglGenBuffers(1, &QuadVBO);
        pglBindBuffer(GL_ARRAY_BUFFER, QuadVBO);
        pglBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, 0x88E4 /*GL_STATIC_DRAW*/);
        pglBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    inline void EnsureFBOs(int w, int h) {
        if (FBOWidth == w && FBOHeight == h && PingPongFBO[0]) return;
        if (PingPongFBO[0]) pglDeleteFramebuffers(2, PingPongFBO);
        if (PingPongTex[0]) glDeleteTextures(2, PingPongTex);
        pglGenFramebuffers(2, PingPongFBO);
        glGenTextures(2, PingPongTex);
        for (int i = 0; i < 2; i++) {
            pglBindFramebuffer(GL_FRAMEBUFFER, PingPongFBO[i]);
            glBindTexture(GL_TEXTURE_2D, PingPongTex[i]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, 0x812F /*GL_CLAMP_TO_EDGE*/);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, 0x812F /*GL_CLAMP_TO_EDGE*/);
            pglFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, PingPongTex[i], 0);
        }
        FBOWidth = w;
        FBOHeight = h;
    }

    inline void Init() {
        if (Initialized || InitFailed) return;
        LoadFunctions();
        LoadFunctions2();
        if (!pglCreateShader || !pglGenBuffers || !pglGenFramebuffers) {
            InitFailed = true;
            return;
        }

        // Compile vertex shader
        GLuint vs = pglCreateShader(GL_VERTEX_SHADER);
        pglShaderSource(vs, 1, &VS, NULL);
        pglCompileShader(vs);
        GLint ok;
        pglGetShaderiv(vs, GL_COMPILE_STATUS, &ok);
        if (!ok) { pglDeleteShader(vs); InitFailed = true; return; }

        // Compile fragment shader
        GLuint fs = pglCreateShader(GL_FRAGMENT_SHADER);
        pglShaderSource(fs, 1, &FS, NULL);
        pglCompileShader(fs);
        pglGetShaderiv(fs, GL_COMPILE_STATUS, &ok);
        if (!ok) { pglDeleteShader(vs); pglDeleteShader(fs); InitFailed = true; return; }

        // Link program
        BlurProgram = pglCreateProgram();
        pglAttachShader(BlurProgram, vs);
        pglAttachShader(BlurProgram, fs);
        pglLinkProgram(BlurProgram);
        pglGetProgramiv(BlurProgram, GL_LINK_STATUS, &ok);
        if (!ok) { pglDeleteProgram(BlurProgram); BlurProgram = 0; InitFailed = true; return; }
        pglDeleteShader(vs);
        pglDeleteShader(fs);

        // Get uniform locations
        pglUseProgram(BlurProgram);
        LocImage     = pglGetUniformLocation(BlurProgram, "image");
        LocDir       = pglGetUniformLocation(BlurProgram, "dir");
        LocRadius    = pglGetUniformLocation(BlurProgram, "radius");
        LocTexOffset = pglGetUniformLocation(BlurProgram, "tex_offset");
        AttrPosition = pglGetAttribLocation(BlurProgram, "Position");
        AttrUV       = pglGetAttribLocation(BlurProgram, "UV");
        pglUniform1i(LocImage, 0);
        pglUseProgram(0);

        // Create fullscreen quad
        CreateQuadVBO();

        Initialized = true;
    }

    struct BlurData {
        float radius;
    };

    typedef void (APIENTRY* PFN_glDisableVertexAttribArray)(GLuint);
    inline PFN_glDisableVertexAttribArray pglDisableVertexAttribArray = nullptr;

    inline void DrawBlurCallback(const ImDrawList* parent_list, const ImDrawCmd* cmd) {
        if (!Initialized || !BlurProgram) return;
        BlurData* data = (BlurData*)cmd->UserCallbackData;
        if (!data) return;

        ImGuiIO& io = ImGui::GetIO();
        int fbW = (int)(io.DisplaySize.x * io.DisplayFramebufferScale.x);
        int fbH = (int)(io.DisplaySize.y * io.DisplayFramebufferScale.y);
        if (fbW <= 0 || fbH <= 0) return;

        // ---- Save ALL ImGui GL state (matching imgui_impl_opengl3.cpp) ----
        GLenum last_active_texture; glGetIntegerv(GL_ACTIVE_TEXTURE, (GLint*)&last_active_texture);
        GLuint last_program;        glGetIntegerv(GL_CURRENT_PROGRAM, (GLint*)&last_program);
        GLuint last_texture;        glGetIntegerv(GL_TEXTURE_BINDING_2D, (GLint*)&last_texture);
        GLuint last_array_buffer;   glGetIntegerv(GL_ARRAY_BUFFER_BINDING, (GLint*)&last_array_buffer);
        GLuint last_element_buffer; glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, (GLint*)&last_element_buffer);
        GLint  last_fbo;            glGetIntegerv(GL_FRAMEBUFFER_BINDING, (GLint*)&last_fbo);
        GLint  last_viewport[4];    glGetIntegerv(GL_VIEWPORT, last_viewport);
        GLint  last_scissor_box[4]; glGetIntegerv(0x0C10 /*GL_SCISSOR_BOX*/, last_scissor_box);
        GLboolean last_scissor = glIsEnabled(GL_SCISSOR_TEST);
        GLboolean last_blend = glIsEnabled(GL_BLEND);

        if (!pglDisableVertexAttribArray) {
            pglDisableVertexAttribArray = (PFN_glDisableVertexAttribArray)wglGetProcAddress("glDisableVertexAttribArray");
            if (!pglDisableVertexAttribArray) pglDisableVertexAttribArray = (PFN_glDisableVertexAttribArray)wglGetProcAddress("glDisableVertexAttribArrayARB");
        }

        // ---- Setup our state ----
        glDisable(GL_SCISSOR_TEST);
        glDisable(GL_BLEND);
        glViewport(0, 0, fbW, fbH);
        pglActiveTexture(GL_TEXTURE0);

        EnsureFBOs(fbW, fbH);

        // Copy screen to PingPongTex[0]
        pglBindFramebuffer(GL_FRAMEBUFFER, last_fbo);
        glBindTexture(GL_TEXTURE_2D, PingPongTex[0]);
        pglCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 0, 0, fbW, fbH, 0);

        // Run blur passes using our shader + VBO
        pglUseProgram(BlurProgram);
        pglUniform1f(LocRadius, data->radius);
        pglUniform2f(LocTexOffset, 1.0f / fbW, 1.0f / fbH);

        pglBindBuffer(GL_ARRAY_BUFFER, QuadVBO);
        if (AttrPosition >= 0) {
            pglEnableVertexAttribArray(AttrPosition);
            pglVertexAttribPointer(AttrPosition, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
        }
        if (AttrUV >= 0) {
            pglEnableVertexAttribArray(AttrUV);
            pglVertexAttribPointer(AttrUV, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
        }

        bool horizontal = true;
        for (int i = 0; i < 4; i++) {
            pglBindFramebuffer(GL_FRAMEBUFFER, PingPongFBO[horizontal ? 1 : 0]);
            pglUniform2f(LocDir, horizontal ? 1.0f : 0.0f, horizontal ? 0.0f : 1.0f);
            glBindTexture(GL_TEXTURE_2D, PingPongTex[horizontal ? 0 : 1]);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            horizontal = !horizontal;
        }

        if (AttrPosition >= 0 && pglDisableVertexAttribArray) pglDisableVertexAttribArray(AttrPosition);
        if (AttrUV >= 0 && pglDisableVertexAttribArray) pglDisableVertexAttribArray(AttrUV);

        // ---- Draw blurred region onto screen ----
        pglBindFramebuffer(GL_FRAMEBUFFER, last_fbo);
        pglUseProgram(0); // Use fixed-function for final blit

        // Restore blend for alpha compositing
        if (last_blend) glEnable(GL_BLEND);
        if (last_scissor) glEnable(GL_SCISSOR_TEST);
        glViewport(last_viewport[0], last_viewport[1], last_viewport[2], last_viewport[3]);
        glScissor(last_scissor_box[0], last_scissor_box[1], last_scissor_box[2], last_scissor_box[3]);

        // ---- Restore ImGui's GL state exactly ----
        pglUseProgram(last_program);
        glBindTexture(GL_TEXTURE_2D, last_texture);
        pglActiveTexture(last_active_texture);
        pglBindBuffer(GL_ARRAY_BUFFER, last_array_buffer);
        pglBindBuffer(GL_ELEMENT_ARRAY_BUFFER, last_element_buffer);
    }

    // Adds the GL callback to perform the blur pass (call this ONCE before drawing blurred elements)
    inline void AddBlurPass(ImDrawList* dl, float radius)
    {
        if (!Initialized || !BlurProgram || InitFailed) return;

        static BlurData frameData;
        frameData.radius = radius;

        // 1. Callback to execute the blur
        dl->AddCallback(DrawBlurCallback, &frameData);

        // 2. Callback to reset ImGui render state after our GL changes
        dl->AddCallback(ImDrawCallback_ResetRenderState, nullptr);
    }

    // Draws a portion of the blurred texture (call this for each element)
    inline void DrawBlurredImage(ImDrawList* dl, const ImVec2& p_min, const ImVec2& p_max,
                                 float alpha, float rounding = 0.0f, ImDrawFlags flags = 0)
    {
        if (!Initialized || !BlurProgram || InitFailed) return;

        ImGuiIO& io = ImGui::GetIO();
        if (io.DisplaySize.x <= 0 || io.DisplaySize.y <= 0) return;

        // UV coords mapping the screen region to the blur texture (OpenGL texture Y is flipped)
        ImVec2 uv_min(p_min.x / io.DisplaySize.x, 1.0f - p_min.y / io.DisplaySize.y);
        ImVec2 uv_max(p_max.x / io.DisplaySize.x, 1.0f - p_max.y / io.DisplaySize.y);

        dl->AddImageRounded(
            (ImTextureID)(intptr_t)PingPongTex[0],
            p_min, p_max,
            ImVec2(uv_min.x, uv_min.y), ImVec2(uv_max.x, uv_max.y),
            IM_COL32(255, 255, 255, (int)(255 * alpha)),
            rounding, flags);
    }
}
