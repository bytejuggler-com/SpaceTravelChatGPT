#include "ui.h"
#include "imgui.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>

namespace astra {
namespace {
constexpr ImU32 white=IM_COL32(235,242,247,255),muted=IM_COL32(133,151,169,255),accent=IM_COL32(159,231,211,255);
void text(ImDrawList* d,ImFont* font,float size,float x,float y,ImU32 color,const char* label){d->AddText(font,size,{x,y},color,label);}
bool hit(const char* id,float x,float y,float w,float h) {ImGui::SetCursorScreenPos({x,y});return ImGui::InvisibleButton(id,{w,h});}
void pill(ImDrawList* d,float x,float y,float w,const char* label,ImFont* font,bool active=false) {
    d->AddRectFilled({x,y},{x+w,y+30},active?IM_COL32(159,231,211,26):IM_COL32(18,27,39,180),15);
    d->AddRect({x,y},{x+w,y+30},active?IM_COL32(159,231,211,85):IM_COL32(124,149,170,48),15);
    text(d,font,12,x+12,y+7,active?accent:muted,label);
}
}
void Interface::initialize() {
    auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.LogFilename=nullptr;
    io.ConfigFlags|=ImGuiConfigFlags_NavEnableKeyboard;
    auto load=[&](const char* path){return std::filesystem::exists(path)?io.Fonts->AddFontFromFileTTF(path,18):io.Fonts->AddFontDefault();};
    regular_=load("C:/Windows/Fonts/segoeui.ttf");
    semibold_=load("C:/Windows/Fonts/seguisb.ttf");
    mono_=load("C:/Windows/Fonts/consola.ttf");
    io.FontDefault=regular_;
    ImGui::StyleColorsDark();auto& s=ImGui::GetStyle();
    s.WindowRounding=16;s.FrameRounding=6;s.PopupRounding=8;s.GrabRounding=6;s.WindowPadding={22,20};
    s.ItemSpacing={12,12};s.FramePadding={10,7};s.WindowBorderSize=1;
    s.Colors[ImGuiCol_WindowBg]={.025f,.045f,.067f,.97f};
    s.Colors[ImGuiCol_TitleBg]={.035f,.065f,.09f,1};
    s.Colors[ImGuiCol_TitleBgActive]={.055f,.095f,.12f,1};
    s.Colors[ImGuiCol_Border]={.24f,.35f,.42f,.4f};
    s.Colors[ImGuiCol_FrameBg]={.08f,.12f,.16f,1};
    s.Colors[ImGuiCol_FrameBgHovered]={.12f,.20f,.23f,1};
    s.Colors[ImGuiCol_SliderGrab]={.46f,.78f,.69f,1};
    s.Colors[ImGuiCol_SliderGrabActive]={.62f,.91f,.83f,1};
    s.Colors[ImGuiCol_CheckMark]={.62f,.91f,.83f,1};
    s.Colors[ImGuiCol_Button]={.12f,.19f,.22f,1};
    s.Colors[ImGuiCol_ButtonHovered]={.19f,.32f,.34f,1};
    s.Colors[ImGuiCol_Header]={.13f,.24f,.25f,1};
}
UiActions Interface::draw(const Timeline& timeline,double time,Settings& s,const std::string& driver,int rw,int rh,bool audioAvailable) {
    UiActions action;
    if(!s.interfaceVisible)return action;
    const ImVec2 display=ImGui::GetIO().DisplaySize;
    const float w=display.x,h=display.y,margin=w<1000?28.0f:48.0f;
    auto moment=timeline.evaluate(time);
    int cue=moment.mix>.65f?moment.nextCue:moment.cue;
    const auto& info=sceneRegistry()[timeline.cues()[cue].scene];
    ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize(display);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{0,0});
    ImGui::Begin("Flight deck",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoBackground|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoBringToFrontOnFocus);
    auto* d=ImGui::GetWindowDrawList();
    // Fades keep typography readable while preserving the uninterrupted scene.
    d->AddRectFilledMultiColor({0,0},{w,165},IM_COL32(4,9,16,230),IM_COL32(4,9,16,230),IM_COL32(4,9,16,0),IM_COL32(4,9,16,0));
    d->AddRectFilledMultiColor({0,h-310},{w,h},IM_COL32(4,9,16,0),IM_COL32(4,9,16,0),IM_COL32(4,9,16,245),IM_COL32(4,9,16,245));
    float logoY=41;
    d->AddCircle({margin+12,logoY+12},11,accent,32,1.3f);
    d->AddLine({margin+12,logoY-5},{margin+12,logoY+29},accent,1);
    d->AddLine({margin-5,logoY+12},{margin+29,logoY+12},accent,1);
    text(d,semibold_,23,margin+43,logoY-3,white,"A S T R A");
    text(d,mono_,10,margin+45,logoY+27,muted,"AN INTERSTELLAR EXPERIENCE");
    float right=w-margin;
    pill(d,right-288,40,119,s.paused?"PAUSED":"LIVE JOURNEY",mono_,!s.paused);
    pill(d,right-155,40,155,"VISUAL CONTROLS",mono_,s.settingsOpen);
    if(hit("settings",right-155,40,155,30))s.settingsOpen=!s.settingsOpen;
    if(hit("live",right-288,40,119,30))s.paused=!s.paused;
    // Minimal scientific marks, placed away from the title and controls.
    float cy=h*.43f;
    d->AddLine({margin,cy-25},{margin,cy+25},IM_COL32(139,170,191,80));
    for(int i=-2;i<=2;i++)d->AddLine({margin,cy+i*10},{margin+(i?5.0f:10.0f),cy+i*10},IM_COL32(139,170,191,80));
    text(d,mono_,10,margin+17,cy-6,muted,"DEEP SPACE / SECTOR 07");
    char counter[64];std::snprintf(counter,sizeof(counter),"%02d / %02d",cue+1,static_cast<int>(timeline.cues().size()));
    text(d,mono_,12,right-70,cy-6,muted,counter);
    float titleY=h-280;
    text(d,mono_,12,margin,titleY,accent,info.subtitle.c_str());
    text(d,regular_,w<1000?38.0f:48.0f,margin-2,titleY+24,white,info.title.c_str());
    text(d,regular_,16,margin,titleY+86,muted,info.description.c_str());
    // Scene chapters form the navigation and show the progress within each cue.
    float chapterY=h-124;
    int count=static_cast<int>(timeline.cues().size());
    float gap=18,cell=(w-2*margin-gap*(count-1))/count;
    if(count<=8)for(int i=0;i<count;i++) {
        float x=margin+i*(cell+gap);
        const auto& entry=sceneRegistry()[timeline.cues()[i].scene];
        bool current=i==moment.cue;
        ImU32 line=current?accent:IM_COL32(99,124,143,75);
        d->AddRectFilled({x,chapterY},{x+cell,chapterY+2},IM_COL32(99,124,143,65));
        if(current)d->AddRectFilled({x,chapterY},{x+cell*static_cast<float>(moment.local/timeline.cues()[i].duration),chapterY+2},accent);
        else if(i<moment.cue)d->AddRectFilled({x,chapterY},{x+cell,chapterY+2},IM_COL32(159,231,211,110));
        char label[96];std::snprintf(label,sizeof(label),"%02d   %s",i+1,entry.id.c_str());
        std::string upper=label;for(char& c:upper)if(c>='a'&&c<='z')c=static_cast<char>(c-'a'+'A');
        text(d,mono_,11,x,chapterY+13,current?white:muted,upper.c_str());
        hit(("scrub"+std::to_string(i)).c_str(),x,chapterY-8,cell,16);
        if(ImGui::IsItemActive()) {
            double fraction=std::clamp(static_cast<double>((ImGui::GetIO().MousePos.x-x)/cell),0.0,.9999);
            action.seek=timeline.start(i)+fraction*timeline.cues()[i].duration;
        }
        if(ImGui::IsItemHovered())ImGui::SetTooltip("Drag to seek within this chapter");
        if(hit(("scene"+std::to_string(i)).c_str(),x,chapterY+9,cell,30))action.seek=timeline.start(i);
        if(ImGui::IsItemHovered())d->AddRectFilled({x,chapterY},{x+cell,chapterY+2},line);
    }
    else {
        float t=static_cast<float>(timeline.wrap(time));
        ImGui::SetCursorScreenPos({margin,chapterY});ImGui::SetNextItemWidth(w-2*margin);
        if(ImGui::SliderFloat("##timeline",&t,0,static_cast<float>(timeline.length()),"%.1f s"))action.seek=t;
    }
    float y=h-57;
    d->AddLine({margin,y-13},{right,y-13},IM_COL32(103,126,146,40));
    if(hit("play",margin,y,68,28))s.paused=!s.paused;
    text(d,mono_,12,margin,y+5,white,s.paused?"PLAY":"PAUSE");
    if(hit("restart",margin+80,y,72,28))action.seek=0;
    text(d,mono_,11,margin+80,y+5,muted,"RESTART");
    char timeLabel[64];int elapsed=static_cast<int>(timeline.wrap(time)),total=static_cast<int>(timeline.length());
    std::snprintf(timeLabel,sizeof(timeLabel),"%02d:%02d  /  %02d:%02d",elapsed/60,elapsed%60,total/60,total%60);
    text(d,mono_,11,margin+172,y+5,muted,timeLabel);
    if(w>1050)text(d,mono_,10,w*.5f-90,y+6,IM_COL32(102,125,145,255),"TAB  IMMERSIVE     F  FULLSCREEN");
    if(hit("mute",right-198,y,110,28))s.muted=!s.muted;
    text(d,mono_,11,right-198,y+5,muted,!audioAvailable?"AUDIO OFF":s.muted?"SOUND OFF":"SOUND ON");
    // A small animated spectrum is a visual accompaniment to the synthesized score.
    if(audioAvailable&&!s.muted)for(int i=0;i<7;i++) {
        float level=s.paused?3:3+11*std::abs(std::sin(static_cast<float>(time)*3+i*1.7f));
        d->AddRectFilled({right-106+i*4,y+17-level},{right-104+i*4,y+17},accent,1);
    }
    char fps[32];std::snprintf(fps,sizeof(fps),"%.0f FPS",ImGui::GetIO().Framerate);
    text(d,mono_,11,right-55,y+5,muted,fps);
    ImGui::End();ImGui::PopStyleVar();
    if(s.settingsOpen) {
        ImGui::SetNextWindowPos({std::max(12.0f,w-385),88},ImGuiCond_Always);
        ImGui::SetNextWindowSize({350,std::min(h-110,655.0f)},ImGuiCond_Always);
        if(ImGui::Begin("Visual controls",&s.settingsOpen,ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoSavedSettings)) {
            settings(s,audioAvailable);
            ImGui::Separator();ImGui::TextDisabled("%s / %d x %d",driver.c_str(),rw,rh);
            if(ImGui::Button("Save screenshot"))action.screenshot=true;
            ImGui::SameLine();if(ImGui::Button("Fullscreen"))action.toggleFullscreen=true;
            ImGui::TextDisabled("1-4 scenes / arrows skip / R restart");
            ImGui::TextDisabled("Space pause / M mute / Esc close");
        }
        ImGui::End();
    }
    return action;
}
void Interface::settings(Settings& s,bool audioAvailable) {
    ImGui::TextDisabled("PRESENTATION");
    if(ImGui::Button("Cinematic")){s.crt=s.pixel=s.palette=false;s.film=true;s.bloom=.65f;s.exposure=1;}
    ImGui::SameLine();if(ImGui::Button("CRT")){s.crt=true;s.pixel=s.palette=false;s.film=true;}
    ImGui::SameLine();if(ImGui::Button("Retro")){s.pixel=s.palette=true;s.crt=false;s.film=false;}
    ImGui::Separator();
    ImGui::Checkbox("CRT phosphor",&s.crt);
    if(s.crt){ImGui::SetNextItemWidth(-1);ImGui::SliderFloat("##crt",&s.crtAmount,.1f,1,"Intensity %.2f");}
    ImGui::Checkbox("Retro pixels",&s.pixel);
    if(s.pixel){ImGui::SetNextItemWidth(-1);ImGui::SliderFloat("##pixel",&s.pixelSize,2,12,"Pixel size %.0f");}
    ImGui::Checkbox("Limited palette + dithering",&s.palette);
    if(s.palette){ImGui::SetNextItemWidth(-1);ImGui::SliderFloat("##palette",&s.paletteLevels,3,24,"Levels %.0f");}
    ImGui::Checkbox("Film grain",&s.film);
    if(s.film){ImGui::SetNextItemWidth(-1);ImGui::SliderFloat("##film",&s.filmAmount,0,1,"Grain %.2f");}
    ImGui::SetNextItemWidth(-1);ImGui::SliderFloat("##bloom",&s.bloom,0,1.8f,"Bloom %.2f");
    ImGui::SetNextItemWidth(-1);ImGui::SliderFloat("##exposure",&s.exposure,.4f,2,"Exposure %.2f");
    ImGui::Separator();ImGui::TextDisabled("PERFORMANCE");
    ImGui::SetNextItemWidth(-1);ImGui::Combo("##quality",&s.quality,"Low / 50%\0Balanced / 75%\0High / 100%\0");
    ImGui::Checkbox("Adapt resolution to frame rate",&s.autoQuality);
    ImGui::Separator();ImGui::TextDisabled("ORIGINAL SCORE / SIGNALS FROM HOME");
    if(!audioAvailable)ImGui::TextWrapped("No audio device available. Visual playback continues.");
    ImGui::BeginDisabled(!audioAvailable);
    ImGui::Checkbox("Mute soundtrack",&s.muted);
    ImGui::SetNextItemWidth(-1);ImGui::SliderFloat("##volume",&s.volume,0,1,"Volume %.2f");
    ImGui::EndDisabled();
}
}
