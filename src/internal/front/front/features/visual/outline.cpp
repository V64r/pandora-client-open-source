#include "../features.hpp"
#include "../../hooks/hooks.hpp"
#include "backends/imgui.h"
#include <jnihook.h>
#include <gl/GL.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <cstdint>

extern "C" void swiftLog(const char*, ...);
extern std::atomic<bool> g_Running;

namespace features::visual::outline {
namespace {
    constexpr GLenum framebuffer=0x8D40, draw_framebuffer=0x8CA9, read_framebuffer=0x8CA8;
    constexpr GLenum attachment=0x8CE0, complete=0x8CD5, texture0=0x84C0, texture1=0x84C1;
    constexpr int max_hooks=8;
    struct ModelHook { jclass klass=nullptr; jmethodID original=nullptr; };
    std::array<ModelHook,max_hooks> model_hooks{};
    int hook_count=0;
    std::atomic<bool> runtime_ready{false}, hooks_ready{false};
    std::atomic<unsigned long> active_models{0};
    struct ModelCall {
        ModelCall() { active_models.fetch_add(1); }
        ~ModelCall() { active_models.fetch_sub(1); }
    };
    jclass player_class=nullptr;
    jobject minecraft_instance=nullptr;
    jobject render_manager_instance=nullptr;
    jmethodID entity_renderer_method=nullptr;
    jfieldID main_model_field=nullptr;
    jfieldID local_player_field=nullptr;
    thread_local int model_depth=0;
    bool attempted_hooks=false;

#define GL_FUNCTIONS(X) \
    X(void, GenFramebuffers, (GLsizei, GLuint*)) \
    X(void, DeleteFramebuffers, (GLsizei, const GLuint*)) \
    X(void, BindFramebuffer, (GLenum, GLuint)) \
    X(void, FramebufferTexture2D, (GLenum, GLenum, GLenum, GLuint, GLint)) \
    X(GLenum, CheckFramebufferStatus, (GLenum)) \
    X(GLuint, CreateShader, (GLenum)) \
    X(void, ShaderSource, (GLuint, GLsizei, const char* const*, const GLint*)) \
    X(void, CompileShader, (GLuint)) \
    X(void, GetShaderiv, (GLuint, GLenum, GLint*)) \
    X(void, GetShaderInfoLog, (GLuint, GLsizei, GLsizei*, char*)) \
    X(void, DeleteShader, (GLuint)) \
    X(GLuint, CreateProgram, ()) \
    X(void, AttachShader, (GLuint, GLuint)) \
    X(void, LinkProgram, (GLuint)) \
    X(void, GetProgramiv, (GLuint, GLenum, GLint*)) \
    X(void, GetProgramInfoLog, (GLuint, GLsizei, GLsizei*, char*)) \
    X(void, DeleteProgram, (GLuint)) \
    X(void, UseProgram, (GLuint)) \
    X(GLint, GetUniformLocation, (GLuint, const char*)) \
    X(void, Uniform1i, (GLint, GLint)) \
    X(void, Uniform1f, (GLint, GLfloat)) \
    X(void, Uniform2f, (GLint, GLfloat, GLfloat)) \
    X(void, Uniform4f, (GLint, GLfloat, GLfloat, GLfloat, GLfloat)) \
    X(void, ActiveTexture, (GLenum))
#define DECLARE_GL(ret, name, args) ret (APIENTRY* name) args=nullptr;
    GL_FUNCTIONS(DECLARE_GL)
#undef DECLARE_GL
    HGLRC owner=nullptr;
    GLuint fbo=0, textures[3]{}, mask_program=0, effect_program=0;
    int width=0, height=0;
    bool gl_failed=false, captured=false;
    GLint u_source=-1, u_mask=-1, u_step=-1, u_pass=-1;
    GLint u_outer=-1, u_glow=-1, u_color=-1;

    bool load_gl() {
#define LOAD_GL(ret, name, args) \
        { const auto p=wglGetProcAddress("gl" #name); \
          const auto n=reinterpret_cast<std::intptr_t>(p); \
          if(n==0 || n==1 || n==2 || n==3 || n==-1) return false; \
          name=reinterpret_cast<decltype(name)>(p); }
        GL_FUNCTIONS(LOAD_GL)
#undef LOAD_GL
        return true;
    }
#undef GL_FUNCTIONS

    // These bindings are outside the state covered by glPushAttrib.
    struct GLState {
        GLint draw=0, read=0, program=0, active=0;
        GLint matrix_mode=GL_MODELVIEW, model_depth_saved=0, projection_depth_saved=0;
        GLdouble model_matrix[16]{}, projection_matrix[16]{};
        GLState() {
            glGetIntegerv(0x8CA6,&draw); glGetIntegerv(0x8CAA,&read);
            glGetIntegerv(0x8B8D,&program); glGetIntegerv(0x84E0,&active);
            glGetIntegerv(GL_MATRIX_MODE,&matrix_mode);
            glGetIntegerv(GL_MODELVIEW_STACK_DEPTH,&model_depth_saved);
            glGetIntegerv(GL_PROJECTION_STACK_DEPTH,&projection_depth_saved);
            glGetDoublev(GL_MODELVIEW_MATRIX,model_matrix);
            glGetDoublev(GL_PROJECTION_MATRIX,projection_matrix);
            glPushAttrib(GL_ALL_ATTRIB_BITS);
            glPushClientAttrib(GL_CLIENT_ALL_ATTRIB_BITS);
        }
        ~GLState() {
            BindFramebuffer(draw_framebuffer,draw); BindFramebuffer(read_framebuffer,read);
            UseProgram(program);
            glPopClientAttrib(); glPopAttrib(); ActiveTexture(active);
            restore_matrix(GL_MODELVIEW,GL_MODELVIEW_STACK_DEPTH,model_depth_saved,model_matrix);
            restore_matrix(GL_PROJECTION,GL_PROJECTION_STACK_DEPTH,projection_depth_saved,projection_matrix);
            glMatrixMode(matrix_mode);
        }
        static void restore_matrix(GLenum mode,GLenum query,GLint expected,const GLdouble* matrix) {
            glMatrixMode(mode);
            GLint depth=0; glGetIntegerv(query,&depth);
            while(depth>expected) { glPopMatrix(); --depth; }
            while(depth<expected) { glPushMatrix(); ++depth; }
            glLoadMatrixd(matrix);
        }
    };
    void destroy_targets() {
        if(fbo) DeleteFramebuffers(1,&fbo);
        glDeleteTextures(3,textures);
        fbo=0;
        for(auto& texture:textures) texture=0;
        width=height=0; captured=false;
    }
    void destroy_resources() {
        destroy_targets();
        if(mask_program) DeleteProgram(mask_program);
        if(effect_program) DeleteProgram(effect_program);
        mask_program=effect_program=0;
    }
    GLuint compile(GLenum kind,const char* source) {
        GLuint shader=CreateShader(kind);
        if(!shader) return 0;
        ShaderSource(shader,1,&source,nullptr); CompileShader(shader);
        GLint ok=0; GetShaderiv(shader,0x8B81,&ok);
        if(!ok) {
            char message[1024]{};
            GetShaderInfoLog(shader,sizeof(message),nullptr,message);
            swiftLog("Outline: shader compilation failed: %s",message);
            DeleteShader(shader); return 0;
        }
        return shader;
    }
    GLuint link(const char* vertex,const char* fragment) {
        GLuint vs=compile(0x8B31,vertex),fs=compile(0x8B30,fragment);
        if(!vs || !fs) { if(vs) DeleteShader(vs); if(fs) DeleteShader(fs); return 0; }
        GLuint program=CreateProgram();
        if(program) {
            AttachShader(program,vs); AttachShader(program,fs); LinkProgram(program);
            GLint ok=0; GetProgramiv(program,0x8B82,&ok);
            if(!ok) {
                char message[1024]{};
                GetProgramInfoLog(program,sizeof(message),nullptr,message);
                swiftLog("Outline: shader link failed: %s",message);
                DeleteProgram(program); program=0;
            }
        }
        DeleteShader(vs); DeleteShader(fs); return program;
    }
    constexpr const char* effect_source=R"GLSL(
#version 120
uniform sampler2D sourceTex, maskTex;
uniform vec2 pixelStep;
uniform int verticalPass;
uniform float outerPx, glowStrength;
uniform vec4 tint;
void main() {
    vec2 uv=gl_TexCoord[0].xy;
    float outerMask=0.0, blur=0.0, weights=0.0;
    float sigma=1.0+glowStrength*3.0;
    float blurRadius=glowStrength>0.0 ? ceil(sigma*3.0) : 0.0;
    float radius=max(ceil(outerPx),blurRadius);
    for(int i=-16;i<=16;++i) {
        float distancePx=abs(float(i));
        if(distancePx>radius) continue;
        vec4 sampleValue=texture2D(sourceTex,uv+pixelStep*float(i));
        float blurValue=verticalPass==0 ? sampleValue.r : sampleValue.b;
        outerMask=max(outerMask,sampleValue.r*clamp(outerPx+1.0-distancePx,0.0,1.0));
        if(glowStrength>0.0 && distancePx<=blurRadius) {
            float weight=exp(-float(i*i)/(2.0*sigma*sigma));
            blur+=blurValue*weight; weights+=weight;
        }
    }
    blur/=max(weights,1.0);
    if(verticalPass==0) gl_FragColor=vec4(outerMask,0.0,blur,1.0);
    else {
        float body=texture2D(maskTex,uv).r;
        float edge=max(outerMask-body,0.0);
        float halo=blur*glowStrength*2.5*(1.0-body);
        float alpha=max(edge,halo)*(1.0-body)*tint.a;
        gl_FragColor=vec4(tint.rgb,clamp(alpha,0.0,1.0));
    }
}
)GLSL";

    bool prepare(int w,int h) {
        HGLRC current=wglGetCurrentContext();
        if(!current) return false;
        if(owner!=current) {
            // A context loss invalidates names; never delete them in another context.
            owner=current; fbo=mask_program=effect_program=0;
            for(auto& texture:textures) texture=0;
            width=height=0; gl_failed=captured=false;
            if(!load_gl()) {
                gl_failed=true; swiftLog("Outline: required OpenGL functions unavailable");
            }
        }
        if(gl_failed) return false;
        if(fbo && width==w && height==h) return true;
        GLint limit=0; glGetIntegerv(GL_MAX_TEXTURE_SIZE,&limit);
        if(w<=0 || h<=0 || w>limit || h>limit) return false;
        GLState restore;
        destroy_targets();
        if(!mask_program) mask_program=link("#version 120\nvoid main(){gl_Position=gl_ModelViewProjectionMatrix*gl_Vertex;}",
            "#version 120\nvoid main(){gl_FragColor=vec4(1.0);}");
        if(!effect_program) effect_program=link("#version 120\nvoid main(){gl_Position=gl_Vertex;gl_TexCoord[0]=gl_MultiTexCoord0;}",effect_source);
        if(!mask_program || !effect_program) {
            destroy_resources(); gl_failed=true; return false;
        }
        GenFramebuffers(1,&fbo); glGenTextures(3,textures);
        if(!fbo) { destroy_resources(); gl_failed=true; return false; }
        ActiveTexture(texture0); BindFramebuffer(framebuffer,fbo);
        glDrawBuffer(attachment); glReadBuffer(attachment);
        for(GLuint texture:textures) {
            glBindTexture(GL_TEXTURE_2D,texture);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,0x812F);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,0x812F);
            glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
            FramebufferTexture2D(framebuffer,attachment,GL_TEXTURE_2D,texture,0);
            if(!texture || CheckFramebufferStatus(framebuffer)!=complete) {
                swiftLog("Outline: incomplete framebuffer (%dx%d); effect skipped",w,h);
                destroy_resources(); gl_failed=true; return false;
            }
        }
        u_source=GetUniformLocation(effect_program,"sourceTex");
        u_mask=GetUniformLocation(effect_program,"maskTex");
        u_step=GetUniformLocation(effect_program,"pixelStep");
        u_pass=GetUniformLocation(effect_program,"verticalPass");
        u_outer=GetUniformLocation(effect_program,"outerPx");
        u_glow=GetUniformLocation(effect_program,"glowStrength");
        u_color=GetUniformLocation(effect_program,"tint");
        width=w; height=h; return true;
    }
    float bounded(float value,float lo,float hi,float fallback) {
        return std::isfinite(value) ? std::clamp(value,lo,hi) : fallback;
    }
    void mask_state() {
        glDisable(GL_DEPTH_TEST); glDepthMask(GL_FALSE);
        glDisable(GL_STENCIL_TEST); glDisable(GL_SCISSOR_TEST);
        glDisable(GL_ALPHA_TEST); glDisable(GL_BLEND); glDisable(GL_CULL_FACE);
        glDisable(GL_COLOR_LOGIC_OP);
        glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
        glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
        for(int i=0;i<6;++i) glDisable(GL_CLIP_PLANE0+i);
        glViewport(0,0,width,height);
    }
    template<int Index>
    void JNICALL model_hook(JNIEnv* env,jobject model,jobject entity,
        jfloat a,jfloat b,jfloat c,jfloat d,jfloat e,jfloat scale) {
        hooks::callback_guard callback;
        ModelCall active;
        if(!runtime_ready.load()) return;
        const auto& hook=model_hooks[Index];
        if(!hook.original) return;
        const bool outer=model_depth++==0;
        env->CallNonvirtualVoidMethod(model,hook.klass,hook.original,entity,a,b,c,d,e,scale);
        --model_depth;
        // Do not hide exceptions raised by the original game render.
        if(env->ExceptionCheck() || !outer || !enabled || !runtime_ready.load() || !hooks_ready.load() ||
            !g_Running.load() || !entity || !env->IsInstanceOf(entity,player_class)) return;
        std::lock_guard<std::recursive_mutex> lock(hooks::render_mutex);
        if(!fbo || owner!=wglGetCurrentContext()) return;
        // Read the live local player, so respawns/world changes cannot leave a stale identity.
        if(!minecraft_instance || !local_player_field) return;
        jobject self=env->GetObjectField(minecraft_instance,local_player_field);
        if(env->ExceptionCheck()) {
            env->ExceptionClear();
            if(self) env->DeleteLocalRef(self);
            return;
        }
        const bool skip=!self || env->IsSameObject(entity,self);
        if(self) env->DeleteLocalRef(self);
        if(skip) return;
        // Minecraft selects the Steve/Slim renderer for this entity. Only its
        // actual main model contributes: armor models have inflated geometry.
        if(render_manager_instance && entity_renderer_method && main_model_field) {
            jobject renderer=env->CallObjectMethod(render_manager_instance,entity_renderer_method,entity);
            jobject selected_model=(!env->ExceptionCheck() && renderer)
                ? env->GetObjectField(renderer,main_model_field) : nullptr;
            const bool matches=!env->ExceptionCheck() && selected_model &&
                env->IsSameObject(model,selected_model);
            if(selected_model) env->DeleteLocalRef(selected_model);
            if(renderer) env->DeleteLocalRef(renderer);
            if(env->ExceptionCheck()) env->ExceptionClear();
            if(!matches) return;
        }
        GLint vp[4]{},current_program=0;
        glGetIntegerv(GL_VIEWPORT,vp); glGetIntegerv(0x8B8D,&current_program);
        GLdouble projection[16]{}; glGetDoublev(GL_PROJECTION_MATRIX,projection);
        // Skip GUI previews and shader-pack passes with a different vertex pipeline.
        if(vp[0]!=0 || vp[1]!=0 || vp[2]!=width || vp[3]!=height ||
            std::abs(projection[15])>0.001 || current_program!=0) return;
        GLState restore;
        BindFramebuffer(framebuffer,fbo);
        FramebufferTexture2D(framebuffer,attachment,GL_TEXTURE_2D,textures[0],0);
        mask_state();
        if(!captured) { glClearColor(0,0,0,0); glClear(GL_COLOR_BUFFER_BIT); }
        UseProgram(mask_program);
        // Model-only replay excludes name labels, shadows and fire.
        ++model_depth;
        env->CallNonvirtualVoidMethod(model,hook.klass,hook.original,entity,a,b,c,d,e,scale);
        --model_depth;
        if(env->ExceptionCheck()) {
            env->ExceptionClear();
            captured=false;
            static ULONGLONG last_log=0;
            if(GetTickCount64()-last_log>5000) {
                last_log=GetTickCount64(); swiftLog("Outline: model mask Java exception");
            }
        } else captured=true;
    }
    const std::array<void*,max_hooks> callbacks={
        (void*)model_hook<0>,(void*)model_hook<1>,(void*)model_hook<2>,(void*)model_hook<3>,
        (void*)model_hook<4>,(void*)model_hook<5>,(void*)model_hook<6>,(void*)model_hook<7>
    };
    jmethodID declared_render(jclass klass,const std::string& signature) {
        jint count=0; jmethodID* methods=nullptr; jmethodID result=nullptr;
        if(sdk::jvmti->GetClassMethods(klass,&count,&methods)!=JVMTI_ERROR_NONE) return nullptr;
        for(int i=0;i<count;++i) {
            char *name=nullptr,*sig=nullptr;
            if(sdk::jvmti->GetMethodName(methods[i],&name,&sig,nullptr)==JVMTI_ERROR_NONE &&
                name && sig && signature==sig &&
                (!std::strcmp(name,"render") || !std::strcmp(name,"func_78088_a") || !std::strcmp(name,"a")))
                result=methods[i];
            if(name) sdk::jvmti->Deallocate((unsigned char*)name);
            if(sig) sdk::jvmti->Deallocate((unsigned char*)sig);
        }
        if(methods) sdk::jvmti->Deallocate((unsigned char*)methods);
        return result;
    }
}

void set_hook_runtime(bool available) {
    runtime_ready.store(available);
    // No new wrapper may call a copied method while JNIHook restores its class.
    if(!available) while(active_models.load()!=0) Sleep(1);
}

void sync_hooks(mapper::__minecraft& minecraft) {
    // Install once as the world becomes ready, not on the first toggle.
    if(attempted_hooks || !runtime_ready.load() || !sdk::jni || !sdk::jvmti) return;
    JNIEnv* env=sdk::jni;
    auto local=minecraft.get_local_player();
    auto manager=minecraft.get_render_manager();
    if(!local.object || !manager.object) return;
    if(!minecraft_instance) {
        const auto& mc_class=mapper::classes["Minecraft"];
        const auto& local_type=mapper::classes["EntityPlayerXP"].signature;
        for(const char* name:{"thePlayer","field_71439_g","h"}) {
            local_player_field=mc_class.get_field(name,local_type).identifier;
            if(local_player_field) break;
        }
        if(!local_player_field) {
            attempted_hooks=true;
            swiftLog("Outline: local player identity field unavailable; effect skipped");
            return;
        }
        minecraft_instance=env->NewGlobalRef(minecraft.object);
        if(!minecraft_instance) { if(env->ExceptionCheck()) env->ExceptionClear(); return; }
    }
    if(env->PushLocalFrame(32)<0) { env->ExceptionClear(); return; }
    jobject renderer=nullptr,model=nullptr;
    const auto& entity=mapper::classes["Entity"];
    for(const auto& method:mapper::classes["RenderManager"].methods) {
        if((method.name=="getEntityRenderObject" || method.name=="func_78713_a" || method.name=="a") &&
            method.signature.rfind("("+entity.signature+")L",0)==0) {
            renderer=env->CallObjectMethod(manager.object,method.identifier,local.object);
            entity_renderer_method=method.identifier;
            break;
        }
    }
    if(!env->ExceptionCheck() && renderer) {
        const auto& living=mapper::classes["RendererLivingEntity"];
        for(const auto& field:living.fields) {
            if((field.name=="mainModel" || field.name=="field_77045_g" || field.name=="f") &&
                !field.signature.empty() && field.signature[0]=='L') {
                jfieldID id=env->GetFieldID(living.klass,field.name.c_str(),field.signature.c_str());
                if(id) {
                    model=env->GetObjectField(renderer,id);
                    main_model_field=id;
                }
                break;
            }
        }
    }
    attempted_hooks=true;
    if(env->ExceptionCheck()) env->ExceptionClear();
    if(model && mapper::classes["EntityPlayer"].klass) {
        render_manager_instance=env->NewGlobalRef(manager.object);
        player_class=(jclass)env->NewGlobalRef(mapper::classes["EntityPlayer"].klass);
        jclass cls=env->GetObjectClass(model);
        const std::string signature="("+entity.signature+"FFFFFF)V";
        while(player_class && cls && hook_count<max_hooks) {
            jmethodID method=declared_render(cls,signature);
            if(method) {
                auto& hook=model_hooks[hook_count];
                hook.klass=(jclass)env->NewGlobalRef(cls);
                if(!hook.klass) break;
                const auto result=JNIHook_Attach(method,callbacks[hook_count],&hook.original);
                if(result==JNIHOOK_OK) ++hook_count;
                else {
                    env->DeleteGlobalRef(hook.klass); hook={};
                    swiftLog("Outline: model hook failed (%d)",(int)result); break;
                }
            }
            jclass parent=env->GetSuperclass(cls);
            env->DeleteLocalRef(cls); cls=parent;
        }
        if(cls) env->DeleteLocalRef(cls);
    }
    if(env->ExceptionCheck()) env->ExceptionClear();
    env->PopLocalFrame(nullptr);
    hooks_ready.store(hook_count>0);
    swiftLog("Outline: attached %d model hooks",hook_count);
}

static void process_mask() {
    {
        GLState restore;
        BindFramebuffer(framebuffer,fbo); mask_state(); UseProgram(effect_program);
        Uniform1i(u_source,0); Uniform1i(u_mask,1);
        Uniform1f(u_outer,bounded(thickness,0.5f,6.f,2.f));
        Uniform1f(u_glow,glow ? bounded(strength,0.f,1.f,0.5f) : 0.f);
        Uniform4f(u_color,bounded(color[0],0,1,1),bounded(color[1],0,1,1),
            bounded(color[2],0,1,1),bounded(color[3],0,1,1));
        ActiveTexture(texture1); glBindTexture(GL_TEXTURE_2D,textures[0]);
        ActiveTexture(texture0);
        for(int pass=0;pass<2;++pass) {
            FramebufferTexture2D(framebuffer,attachment,GL_TEXTURE_2D,textures[pass+1],0);
            glBindTexture(GL_TEXTURE_2D,textures[pass]); Uniform1i(u_pass,pass);
            Uniform2f(u_step,pass==0 ? 1.f/width : 0.f,pass==1 ? 1.f/height : 0.f);
            glBegin(GL_QUADS);
            glTexCoord2f(0,0); glVertex2f(-1,-1);
            glTexCoord2f(1,0); glVertex2f(1,-1);
            glTexCoord2f(1,1); glVertex2f(1,1);
            glTexCoord2f(0,1); glVertex2f(-1,1);
            glEnd();
        }
        captured=false;
    }
}
void render() {
    std::lock_guard<std::recursive_mutex> lock(hooks::render_mutex);
    const ImVec2 display=ImGui::GetIO().DisplaySize;
    if(!features::visual::render_valid.load()) { captured=false; return; }
    // Warm the GL resources once on their owning thread, then reuse them.
    // Toggling does not compile shaders or allocate full-screen textures.
    if(!hooks_ready.load() || !prepare((int)display.x,(int)display.y)) return;
    if(!enabled) { captured=false; return; }
    if(!captured) return;
    process_mask();
    ImGui::GetBackgroundDrawList()->AddImage((ImTextureID)(intptr_t)textures[2],
        {0,0},display,{0,1},{1,0});
}
void release_gl() {
    std::lock_guard<std::recursive_mutex> lock(hooks::render_mutex);
    if(owner && owner==wglGetCurrentContext()) destroy_resources();
    owner=nullptr; fbo=mask_program=effect_program=0;
    for(auto& texture:textures) texture=0;
    width=height=0; captured=false; gl_failed=false;
}
void release_jni() {
    // Called after JNIHook_Shutdown and the callback barrier.
    hooks_ready.store(false);
    for(auto& hook:model_hooks) {
        if(hook.klass) sdk::jni->DeleteGlobalRef(hook.klass);
        hook={};
    }
    if(player_class) sdk::jni->DeleteGlobalRef(player_class);
    if(minecraft_instance) sdk::jni->DeleteGlobalRef(minecraft_instance);
    if(render_manager_instance) sdk::jni->DeleteGlobalRef(render_manager_instance);
    render_manager_instance=nullptr; entity_renderer_method=nullptr; main_model_field=nullptr;
    minecraft_instance=nullptr; local_player_field=nullptr;
    player_class=nullptr; hook_count=0; attempted_hooks=false;
}
void reset() {
    enabled=false; thickness=2.f; glow=false; strength=0.5f;
    color[0]=228.f/255.f; color[1]=228.f/255.f; color[2]=231.f/255.f; color[3]=1.f;
}
}
