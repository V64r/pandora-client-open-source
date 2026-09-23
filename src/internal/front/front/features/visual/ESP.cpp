#include "../features.hpp"
#include "backends/imgui.h"

#include <algorithm>
#include <cmath>
#include <cfloat>
#include <cstdio>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

extern bool gui_esp_enabled; extern int gui_esp_mode; extern int gui_esp_draw_mode;
extern int gui_esp_2d_style; extern float gui_esp_corner_size; extern float gui_esp_line_thickness;
extern float gui_esp_render_distance; extern bool gui_esp_pulse; extern float gui_esp_pulse_speed;
extern float gui_esp_pulse_min_alpha; extern float gui_esp_pulse_max_alpha;
extern bool gui_esp_healthbar; extern int gui_esp_healthbar_position; extern int gui_esp_healthbar_style;
extern float gui_esp_healthbar_thickness; extern bool gui_esp_health_number; extern float gui_esp_healthbar_segments;
extern float gui_esp_healthbar_gradient_top[4]; extern float gui_esp_healthbar_gradient_bottom[4];
extern bool gui_esp_hurt_color; extern float gui_esp_hurt_effect_color[4];
extern int gui_esp_fill_mode;
extern float gui_esp_outline_color[4]; extern float gui_esp_filled_color[4];
extern float gui_esp_fill_gradient_top[4]; extern float gui_esp_fill_gradient_bottom[4]; extern float gui_esp_friend_color[4];
extern std::atomic<bool> g_PlayerInGui;
extern std::atomic<bool> g_ChatOpen;

namespace {
struct EspPlayer { int id=0; std::string name; mapper::__vec3 position{},old_position{}; float health=0,max_health=20; int hurt_time=0; bool friendly=false; };
std::vector<EspPlayer> players; std::mutex players_mutex; float players_partial_ticks=0; int players_world_tick=-1;
std::unordered_map<int,float> hurt_fades;

ImU32 color_u32(const float c[4],float alpha=1){return ImGui::ColorConvertFloat4ToU32({std::clamp(c[0],0.f,1.f),std::clamp(c[1],0.f,1.f),std::clamp(c[2],0.f,1.f),std::clamp(c[3]*alpha,0.f,1.f)});}
void blend(const float a[4],const float b[4],float t,float out[4]){t=std::clamp(t,0.f,1.f);for(int i=0;i<3;++i)out[i]=a[i]+(b[i]-a[i])*t;out[3]=a[3]*(1.f+(b[3]-1.f)*t);}
float pulse_alpha(){if(!gui_esp_pulse)return 1;float lo=std::clamp((std::min)(gui_esp_pulse_min_alpha,gui_esp_pulse_max_alpha),0.f,1.f),hi=std::clamp((std::max)(gui_esp_pulse_min_alpha,gui_esp_pulse_max_alpha),0.f,1.f);float w=.5f+.5f*std::sin((float)ImGui::GetTime()*std::clamp(gui_esp_pulse_speed,.1f,5.f)*6.2831853f);w=w*w*(3-2*w);return lo+(hi-lo)*w;}

bool project_box(const EspPlayer& p,float partial,double cx,double cy,double cz,mapper::__vec2 out[8]){
 double x=p.old_position.x+(p.position.x-p.old_position.x)*partial-cx,y=p.old_position.y+(p.position.y-p.old_position.y)*partial-cy,z=p.old_position.z+(p.position.z-p.old_position.z)*partial-cz;
 constexpr double hw=.4,lo=-.1,hi=1.92; mapper::__vec3 c[8]={{x-hw,y+lo,z-hw},{x-hw,y+hi,z-hw},{x+hw,y+hi,z-hw},{x+hw,y+lo,z-hw},{x-hw,y+lo,z+hw},{x-hw,y+hi,z+hw},{x+hw,y+hi,z+hw},{x+hw,y+lo,z+hw}};
 for(int i=0;i<8;++i){out[i]=features::visual::world_to_screen(c[i]);if(out[i].x==FLT_MAX||out[i].y==FLT_MAX)return false;}return true;
}
void bounds(const mapper::__vec2 p[8],float& x1,float& y1,float& x2,float& y2){x1=y1=FLT_MAX;x2=y2=-FLT_MAX;for(int i=0;i<8;++i){x1=(std::min)(x1,(float)p[i].x);y1=(std::min)(y1,(float)p[i].y);x2=(std::max)(x2,(float)p[i].x);y2=(std::max)(y2,(float)p[i].y);}}
void colors(const EspPlayer& p,float hurt,float outline[4],float fill[4]){const float* o=p.friendly?gui_esp_friend_color:gui_esp_outline_color;const float* f=p.friendly?gui_esp_friend_color:gui_esp_filled_color;blend(o,gui_esp_hurt_effect_color,gui_esp_hurt_color?hurt:0,outline);blend(f,gui_esp_hurt_effect_color,gui_esp_hurt_color?hurt:0,fill);}
void gradient_colors(const EspPlayer& p,float hurt,float top[4],float bottom[4]){const float amount=gui_esp_hurt_color?hurt:0.f;const float* a=p.friendly?gui_esp_friend_color:gui_esp_fill_gradient_top;const float* b=p.friendly?gui_esp_friend_color:gui_esp_fill_gradient_bottom;blend(a,gui_esp_hurt_effect_color,amount,top);blend(b,gui_esp_hurt_effect_color,amount,bottom);}

void gradient_quad(ImDrawList* dl,const ImVec2 p[4],ImU32 top,ImU32 bottom,float y1,float y2){
 ImVec4 tc=ImGui::ColorConvertU32ToFloat4(top),bc=ImGui::ColorConvertU32ToFloat4(bottom);ImVec2 uv=ImGui::GetFontTexUvWhitePixel();ImDrawIdx base=(ImDrawIdx)dl->_VtxCurrentIdx;dl->PrimReserve(6,4);
 for(int i=0;i<4;++i){float t=y2>y1?std::clamp((p[i].y-y1)/(y2-y1),0.f,1.f):0;ImVec4 c{tc.x+(bc.x-tc.x)*t,tc.y+(bc.y-tc.y)*t,tc.z+(bc.z-tc.z)*t,tc.w+(bc.w-tc.w)*t};dl->PrimWriteVtx(p[i],uv,ImGui::ColorConvertFloat4ToU32(c));}
 dl->PrimWriteIdx(base);dl->PrimWriteIdx(base+1);dl->PrimWriteIdx(base+2);dl->PrimWriteIdx(base);dl->PrimWriteIdx(base+2);dl->PrimWriteIdx(base+3);
}
void corner_box(ImDrawList* dl,ImVec2 a,ImVec2 b,ImU32 color,float thick){float rx=(b.x-a.x)*std::clamp(gui_esp_corner_size,.1f,.5f),ry=(b.y-a.y)*std::clamp(gui_esp_corner_size,.1f,.5f);ImVec2 l[8][2]={{{a.x,a.y},{a.x+rx,a.y}},{{a.x,a.y},{a.x,a.y+ry}},{{b.x,a.y},{b.x-rx,a.y}},{{b.x,a.y},{b.x,a.y+ry}},{{a.x,b.y},{a.x+rx,b.y}},{{a.x,b.y},{a.x,b.y-ry}},{{b.x,b.y},{b.x-rx,b.y}},{{b.x,b.y},{b.x,b.y-ry}}};for(auto& v:l)dl->AddLine(v[0],v[1],IM_COL32(0,0,0,255),thick+2);for(auto& v:l)dl->AddLine(v[0],v[1],color,thick);}

void healthbar(ImDrawList* dl,float min_x,float max_x,float top,float bottom,float hp,float max_hp){
 float h=bottom-top;if(h<=1)return;float ratio=std::clamp(hp/(max_hp>0?max_hp:20),0.f,1.f),thick=std::clamp(gui_esp_healthbar_thickness,1.f,6.f);float left=gui_esp_healthbar_position==0?min_x-thick-3:max_x+3,right=left+thick,fill_top=bottom-h*ratio;
 dl->AddRectFilled({left-1,top-1},{right+1,bottom+1},IM_COL32(0,0,0,210));
 if(gui_esp_healthbar_style==1)dl->AddRectFilledMultiColor({left,fill_top},{right,bottom},color_u32(gui_esp_healthbar_gradient_top),color_u32(gui_esp_healthbar_gradient_top),color_u32(gui_esp_healthbar_gradient_bottom),color_u32(gui_esp_healthbar_gradient_bottom));
 else{float c[4]={1-ratio,ratio,.08f,1};dl->AddRectFilled({left,fill_top},{right,bottom},color_u32(c));}
 int segs=std::clamp((int)std::round(gui_esp_healthbar_segments),2,20);for(int i=1;i<segs;++i){float y=top+h*((float)i/segs);dl->AddLine({left,y},{right,y},IM_COL32(0,0,0,190),1);}
 if(gui_esp_health_number){char text[16]{};std::snprintf(text,sizeof(text),"%d",(int)std::round(hp));ImVec2 size=ImGui::CalcTextSize(text);float tx=gui_esp_healthbar_position==0?left-size.x-2:right+2;ImVec2 p{tx,top-1};dl->AddText({p.x+1,p.y+1},IM_COL32(0,0,0,230),text);dl->AddText(p,IM_COL32(255,255,255,255),text);}
}
void draw2d(ImDrawList* dl,const EspPlayer& p,const mapper::__vec2 pts[8],float hurt){
 float x1,y1,x2,y2;bounds(pts,x1,y1,x2,y2);if(x1>=x2||y1>=y2)return;float o[4],f[4];colors(p,hurt,o,f);float alpha=pulse_alpha(),round=gui_esp_2d_style==0?4.f:0.f;
 {if(gui_esp_fill_mode==1){float top[4],bottom[4];gradient_colors(p,hurt,top,bottom);dl->AddRectFilledMultiColor({x1,y1},{x2,y2},color_u32(top,alpha),color_u32(top,alpha),color_u32(bottom,alpha),color_u32(bottom,alpha));}else dl->AddRectFilled({x1,y1},{x2,y2},color_u32(f,alpha),round);}
 if(gui_esp_draw_mode!=1){float t=std::clamp(gui_esp_line_thickness,.5f,6.f);if(gui_esp_2d_style==2)corner_box(dl,{x1,y1},{x2,y2},color_u32(o,alpha),t);else{dl->AddRect({x1-1,y1-1},{x2+1,y2+1},IM_COL32(0,0,0,255),round,0,t+2);dl->AddRect({x1,y1},{x2,y2},color_u32(o,alpha),round,0,t);}}
 if(gui_esp_healthbar)healthbar(dl,x1,x2,y1,y2,p.health,p.max_health);
}
void draw3d(ImDrawList* dl,const EspPlayer& p,const mapper::__vec2 pts[8],float hurt){
 float x1,y1,x2,y2;bounds(pts,x1,y1,x2,y2);float o[4],f[4];colors(p,hurt,o,f);float alpha=pulse_alpha();
 {float top[4],bottom[4];gradient_colors(p,hurt,top,bottom);int faces[6][4]={{0,3,2,1},{4,5,6,7},{0,1,5,4},{3,7,6,2},{1,2,6,5},{0,4,7,3}};for(auto& face:faces){ImVec2 q[4];for(int i=0;i<4;++i)q[i]={(float)pts[face[i]].x,(float)pts[face[i]].y};if(gui_esp_fill_mode==1)gradient_quad(dl,q,color_u32(top,alpha),color_u32(bottom,alpha),y1,y2);else dl->AddQuadFilled(q[0],q[1],q[2],q[3],color_u32(f,alpha));}}
 if(gui_esp_draw_mode!=1){int edges[12][2]={{0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}};float t=std::clamp(gui_esp_line_thickness,.5f,6.f);for(auto& e:edges)dl->AddLine({(float)pts[e[0]].x,(float)pts[e[0]].y},{(float)pts[e[1]].x,(float)pts[e[1]].y},IM_COL32(0,0,0,255),t+2);for(auto& e:edges)dl->AddLine({(float)pts[e[0]].x,(float)pts[e[0]].y},{(float)pts[e[1]].x,(float)pts[e[1]].y},color_u32(o,alpha),t);}
 if(gui_esp_healthbar)healthbar(dl,x1,x2,y1,y2,p.health,p.max_health);
}
}

void features::visual::esp::clear(){std::lock_guard<std::mutex> lock(players_mutex);players.clear();players_partial_ticks=0;players_world_tick=-1;hurt_fades.clear();}
void features::visual::esp::run(mapper::__minecraft& minecraft){
 if(!gui_esp_enabled){clear();return;}auto world=minecraft.get_world();auto local=minecraft.get_local_player();auto timer=minecraft.get_timer();if(!world.object||!local.object||!timer.object){clear();return;}auto local_pos=local.get_position();std::vector<EspPlayer> list;
 for(auto player:world.get_players()){if(!player.object||sdk::jni->IsSameObject(player.object,local.object))continue;std::string name=player.get_name();if(name.empty())continue;float hp=player.get_health();if(hp<=0)continue;auto pos=player.get_position();if(local_pos.get_distance_to_vec3(pos)>std::clamp(gui_esp_render_distance,8.f,256.f))continue;EspPlayer e;e.id=player.get_entity_id();e.name=std::move(name);e.position=pos;e.old_position=player.get_old_position();e.health=hp;e.max_health=player.get_max_health();e.hurt_time=player.get_hurt_time();e.friendly=features::friends::is_friend(e.name)||features::friends::is_teammate(player,local);list.push_back(std::move(e));}
 std::lock_guard<std::mutex> lock(players_mutex);players=std::move(list);players_partial_ticks=std::clamp(timer.get_partial_ticks(),0.f,1.f);players_world_tick=local.get_ticks_existed();
}
void features::visual::esp::render(){
 if(!gui_esp_enabled)return;if(g_PlayerInGui.load(std::memory_order_acquire))return;thread_local std::vector<EspPlayer> list;float snapshot_partial=0;int snapshot_tick=-1;{std::lock_guard<std::mutex> lock(players_mutex);list=players;snapshot_partial=players_partial_ticks;snapshot_tick=players_world_tick;}if(list.empty())return;
 // The entity pair belongs to snapshot_tick while the camera belongs to the
 // render tick. Preserve the elapsed whole-tick offset instead of freezing at
 // 1.0 or moving the entity one tick behind the camera. Normally this stays in
 // [0, 1]; [1, 2] is a one-frame extrapolation while the async scan catches up.
 float partial=snapshot_partial;
 if(features::visual::render_frame_snapshot_valid){partial=std::clamp(features::visual::render_partial_ticks,0.f,1.f);if(features::visual::render_world_tick>=0&&snapshot_tick>=0)partial+=(float)(features::visual::render_world_tick-snapshot_tick);}
 partial=std::clamp(partial,0.f,2.f);
 float dt=std::clamp(ImGui::GetIO().DeltaTime,0.f,.1f);std::unordered_set<int> alive;std::unordered_map<int,float> fade_snapshot;
 { std::lock_guard<std::mutex> lock(players_mutex); for(auto& p:list){alive.insert(p.id);float& f=hurt_fades[p.id];float target=p.hurt_time>0?1.f:0.f;f+=(target-f)*std::clamp(dt*(target>f?14.f:4.5f),0.f,1.f);fade_snapshot.emplace(p.id,f);} for(auto it=hurt_fades.begin();it!=hurt_fades.end();)if(!alive.count(it->first))it=hurt_fades.erase(it);else++it; }
 ImDrawList* dl = ImGui::GetBackgroundDrawList();
 for (const auto& p : list) {
     mapper::__vec2 projected[8];
     if (!project_box(p, partial, features::visual::render_camera_x,
         features::visual::render_camera_y, features::visual::render_camera_z, projected)) continue;
     if (gui_esp_enabled) {
         const auto fade = fade_snapshot.find(p.id);
         const float hurt = fade != fade_snapshot.end() ? fade->second : 0.f;
         if (gui_esp_mode == 0) draw2d(dl, p, projected, hurt);
         else draw3d(dl, p, projected, hurt);
     }
 }
}
