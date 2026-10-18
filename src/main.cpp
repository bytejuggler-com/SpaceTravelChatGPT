#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include "core.h"
#include "audio.h"
#include "renderer.h"
#include "ui.h"
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlgpu3.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>

using namespace astra;
namespace fs=std::filesystem;
namespace {
std::mutex diagnosticMutex;
std::vector<std::string> diagnostics;
void SDLCALL logSDL(void*,int,SDL_LogPriority priority,const char* message) {
    if(priority>=SDL_LOG_PRIORITY_WARN){std::lock_guard lock(diagnosticMutex);diagnostics.emplace_back(message);}
}
struct Options {
    int frames=0,width=1440,height=900,quality=-1;
    double at=-1;
    bool noAudio=false,noUi=false,debug=false,settings=false,exercise=false;
    std::string capture,gallery,exportAudio,preset="cinematic",config;
};
Options parse(int argc,char** argv) {
    Options o;
    for(int i=1;i<argc;i++) {
        std::string arg=argv[i];
        auto value=[&](){if(i+1>=argc)throw std::runtime_error("Missing value for "+arg);return std::string(argv[++i]);};
        if(arg=="--frames")o.frames=std::stoi(value());
        else if(arg=="--at")o.at=std::stod(value());
        else if(arg=="--capture")o.capture=value();
        else if(arg=="--gallery")o.gallery=value();
        else if(arg=="--export-audio")o.exportAudio=value();
        else if(arg=="--config")o.config=value();
        else if(arg=="--preset")o.preset=value();
        else if(arg=="--width")o.width=std::stoi(value());
        else if(arg=="--height")o.height=std::stoi(value());
        else if(arg=="--quality")o.quality=std::stoi(value());
        else if(arg=="--no-audio")o.noAudio=true;
        else if(arg=="--no-ui")o.noUi=true;
        else if(arg=="--settings")o.settings=true;
        else if(arg=="--debug-gpu")o.debug=true;
        else if(arg=="--exercise-controls")o.exercise=true;
        else throw std::runtime_error("Unknown option: "+arg);
    }
    if(o.width<800||o.width>7680||o.height<600||o.height>4320||o.quality< -1||o.quality>2||o.frames<0||!std::isfinite(o.at))throw std::runtime_error("Invalid command-line values.");
    if(o.preset!="cinematic"&&o.preset!="crt"&&o.preset!="retro")throw std::runtime_error("Unknown visual preset.");
    if(!o.gallery.empty())o.frames=24;
    if(!o.capture.empty()&&o.frames==0)o.frames=5;
    if(o.exercise&&o.frames==0)o.frames=240;
    return o;
}
void preference(Settings& s,const fs::path& path,bool write) {
    if(write) {
        std::ofstream f(path);
        f<<"quality "<<s.quality<<"\nvolume "<<s.volume<<"\nbloom "<<s.bloom<<"\nexposure "<<s.exposure
         <<"\ncrt "<<s.crt<<"\npixel "<<s.pixel<<"\npalette "<<s.palette<<"\nfilm "<<s.film
         <<"\ncrtAmount "<<s.crtAmount<<"\npixelSize "<<s.pixelSize<<"\npaletteLevels "<<s.paletteLevels
         <<"\nfilmAmount "<<s.filmAmount<<"\nautoQuality "<<s.autoQuality<<"\nmuted "<<s.muted<<'\n';
        return;
    }
    std::ifstream f(path);std::string key;float value;
    while(f>>key>>value) {
        if(!std::isfinite(value))continue;
        if(key=="quality")s.quality=std::clamp(static_cast<int>(value),0,2);
        if(key=="volume")s.volume=std::clamp(value,0.0f,1.0f);
        if(key=="bloom")s.bloom=std::clamp(value,0.0f,1.8f);
        if(key=="exposure")s.exposure=std::clamp(value,.4f,2.0f);
        if(key=="crt")s.crt=value!=0;if(key=="pixel")s.pixel=value!=0;
        if(key=="palette")s.palette=value!=0;if(key=="film")s.film=value!=0;
        if(key=="crtAmount")s.crtAmount=std::clamp(value,.1f,1.0f);
        if(key=="pixelSize")s.pixelSize=std::clamp(value,2.0f,12.0f);
        if(key=="paletteLevels")s.paletteLevels=std::clamp(value,3.0f,24.0f);
        if(key=="filmAmount")s.filmAmount=std::clamp(value,0.0f,1.0f);
        if(key=="autoQuality")s.autoQuality=value!=0;if(key=="muted")s.muted=value!=0;
    }
}
struct SDLGuard{~SDLGuard(){SDL_Quit();}};
struct GuiGuard {
    bool platform=false,renderer=false;
    ~GuiGuard(){if(renderer)ImGui_ImplSDLGPU3_Shutdown();if(platform)ImGui_ImplSDL3_Shutdown();if(ImGui::GetCurrentContext())ImGui::DestroyContext();}
};
void pushKey(SDL_Keycode key){SDL_Event e{};e.type=SDL_EVENT_KEY_DOWN;e.key.key=key;SDL_PushEvent(&e);}
int run(const Options& options,std::ofstream& log) {
    const std::string base=SDL_GetBasePath();
    Timeline timeline;timeline.load(options.config.empty()?base+"/config/demo.cfg":options.config);
    if(!options.exportAudio.empty()) {
        auto track=Soundtrack::synthesize(timeline);
        if(!track.saveWav(options.exportAudio))throw std::runtime_error("Could not write soundtrack.");
        log<<"Exported "<<track.duration()<<" seconds of original stereo audio.\n";return 0;
    }
    if(!SDL_Init(SDL_INIT_VIDEO))throw std::runtime_error(SDL_GetError());
    SDLGuard sdl;
    std::unique_ptr<SDL_Window,decltype(&SDL_DestroyWindow)> window(
        SDL_CreateWindow("ASTRA / An interstellar experience",options.width,options.height,SDL_WINDOW_RESIZABLE|SDL_WINDOW_HIGH_PIXEL_DENSITY),SDL_DestroyWindow);
    if(!window)throw std::runtime_error(SDL_GetError());
    SDL_SetWindowMinimumSize(window.get(),800,600);
    SDL_SetWindowPosition(window.get(),SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED);
    Renderer renderer;renderer.initialize(window.get(),base,options.debug);
    log<<"Renderer: "<<renderer.driver()<<"\nTimeline: "<<timeline.length()<<" s, "<<timeline.cues().size()<<" cues\n";log.flush();
    log<<"GPU: "<<SDL_GetStringProperty(SDL_GetGPUDeviceProperties(renderer.device()),SDL_PROP_GPU_DEVICE_NAME_STRING,"Unknown adapter")<<'\n';
    IMGUI_CHECKVERSION();ImGui::CreateContext();GuiGuard gui;
    gui.platform=ImGui_ImplSDL3_InitForSDLGPU(window.get());
    ImGui_ImplSDLGPU3_InitInfo init{};init.Device=renderer.device();init.ColorTargetFormat=SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    gui.renderer=ImGui_ImplSDLGPU3_Init(&init);
    if(!gui.platform||!gui.renderer)throw std::runtime_error("Interface initialization failed.");
    Interface interface;interface.initialize();
    Settings settings;
    bool automation=options.frames>0||options.at>=0;
    if(!automation)preference(settings,base+"/config/preferences.cfg",false);
    else settings.autoQuality=false;
    if(options.quality>=0)settings.quality=options.quality;
    settings.interfaceVisible=!options.noUi;settings.settingsOpen=options.settings;
    settings.paused=options.at>=0;
    if(options.preset=="crt")settings.crt=true;
    if(options.preset=="retro"){settings.pixel=settings.palette=true;settings.film=false;}
    Audio audio;
    if(!options.noAudio) {
        if(SDL_InitSubSystem(SDL_INIT_AUDIO)) {
            log<<"Preparing original soundtrack...\n";log.flush();
            bool ready=audio.open(Soundtrack::synthesize(timeline));
            log<<(ready?"Audio ready.\n":"Audio unavailable: "+audio.error()+"\n");
        } else log<<"No audio subsystem: "<<SDL_GetError()<<'\n';
    }
    double time=options.at>=0?options.at:0;
    audio.seek(time);
    bool running=true,fullscreen=false;
    int frame=0;
    double last=static_cast<double>(SDL_GetTicksNS())*1e-9,statsStart=last,frameSum=0,maxFrame=0;
    int measuredFrames=0;
    float adaptive=1;
    std::string pendingCapture;
    if(!options.gallery.empty())fs::create_directories(options.gallery);
    auto seek=[&](double value){time=timeline.wrap(value);audio.seek(time);};
    while(running) {
        if(options.exercise) {
            // Integration checks pass through the real SDL event handling path.
            if(frame==10)pushKey(SDLK_SPACE);
            if(frame==20)pushKey(SDLK_SPACE);
            if(frame==30)pushKey(SDLK_2);
            if(frame==40)pushKey(SDLK_M);
            if(frame==50)pushKey(SDLK_M);
            if(frame==60)pushKey(SDLK_TAB);
            if(frame==70)pushKey(SDLK_TAB);
            if(frame==80)pushKey(SDLK_3);
            if(frame==90)SDL_SetWindowSize(window.get(),1024,768);
            if(frame==110)SDL_SetWindowSize(window.get(),1440,900);
            if(frame==120)pushKey(SDLK_V);
            if(frame==140)pushKey(SDLK_V);
            if(frame==150)pushKey(SDLK_4);
            if(frame==180)pushKey(SDLK_R);
        }
        SDL_Event event;
        while(SDL_PollEvent(&event)) {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if(event.type==SDL_EVENT_QUIT||event.type==SDL_EVENT_WINDOW_CLOSE_REQUESTED)running=false;
            if(event.type==SDL_EVENT_KEY_DOWN&&!event.key.repeat&&!ImGui::GetIO().WantTextInput) {
                auto moment=timeline.evaluate(time);
                switch(event.key.key) {
                    case SDLK_ESCAPE:if(settings.settingsOpen)settings.settingsOpen=false;else if(fullscreen){fullscreen=false;SDL_SetWindowFullscreen(window.get(),false);}else running=false;break;
                    case SDLK_SPACE:settings.paused=!settings.paused;break;
                    case SDLK_TAB:settings.interfaceVisible=!settings.interfaceVisible;break;
                    case SDLK_M:settings.muted=!settings.muted;break;
                    case SDLK_V:settings.settingsOpen=!settings.settingsOpen;break;
                    case SDLK_R:seek(0);break;
                    case SDLK_F:case SDLK_F11:fullscreen=!fullscreen;SDL_SetWindowFullscreen(window.get(),fullscreen);break;
                    case SDLK_RIGHT:seek(timeline.start(moment.nextCue));break;
                    case SDLK_LEFT:seek(timeline.start((moment.cue+static_cast<int>(timeline.cues().size())-1)%static_cast<int>(timeline.cues().size())));break;
                    case SDLK_1:case SDLK_2:case SDLK_3:case SDLK_4:{int i=static_cast<int>(event.key.key-SDLK_1);if(i<static_cast<int>(timeline.cues().size()))seek(timeline.start(i));break;}
                    case SDLK_F12:pendingCapture=(fs::path(base)/("astra-"+std::to_string(SDL_GetTicks())+".bmp")).string();break;
                }
            }
        }
        if(!running)break;
        double now=static_cast<double>(SDL_GetTicksNS())*1e-9,dt=now-last;last=now;
        bool minimized=(SDL_GetWindowFlags(window.get())&SDL_WINDOW_MINIMIZED)!=0;
        audio.update(settings.paused||minimized,settings.muted?0:settings.volume);
        if(minimized){SDL_Delay(20);continue;}
        if(!settings.paused)time=audio.active()?audio.position():time+std::min(dt,.1);
        if(options.exercise) {
            auto verify=[&](bool condition){if(!condition)throw std::runtime_error("Control integration check failed at frame "+std::to_string(frame));};
            if(frame==11)verify(settings.paused);
            if(frame==21)verify(!settings.paused);
            if(frame==31)verify(timeline.evaluate(time).cue==1);
            if(frame==41)verify(settings.muted);
            if(frame==51)verify(!settings.muted);
            if(frame==61)verify(!settings.interfaceVisible);
            if(frame==71)verify(settings.interfaceVisible);
            if(frame==81)verify(timeline.evaluate(time).cue==2);
            if(frame==91){int width=0,height=0;SDL_GetWindowSize(window.get(),&width,&height);verify(width==1024&&height==768);}
            if(frame==121)verify(settings.settingsOpen);
            if(frame==141)verify(!settings.settingsOpen);
            if(frame==151)verify(timeline.evaluate(time).cue==3);
            if(frame==181){verify(timeline.evaluate(time).cue==0);log<<"Control integration checks passed.\n";}
        }
        if(frame>10){frameSum+=dt;maxFrame=std::max(maxFrame,dt);++measuredFrames;}
        if(!automation&&settings.autoQuality&&now-statsStart>3) {
            float fps=ImGui::GetIO().Framerate;
            float previous=adaptive;
            if(fps<48)adaptive=std::max(.65f,adaptive-.1f);
            else if(fps>58)adaptive=std::min(1.0f,adaptive+.05f);
            if(adaptive!=previous)renderer.setAdaptiveScale(adaptive);
            statsStart=now;
        }
        if(!options.gallery.empty()) {
            int entry=frame/3;
            constexpr double times[]={3,13,25,37,9.4,19.4,31.2,43.2};
            time=times[std::min(entry,7)];
            if(frame%3==2)pendingCapture=(fs::path(options.gallery)/("scene-"+std::to_string(entry)+".bmp")).string();
        }
        ImGui_ImplSDLGPU3_NewFrame();ImGui_ImplSDL3_NewFrame();ImGui::NewFrame();
        UiActions action=interface.draw(timeline,time,settings,renderer.driver(),renderer.renderWidth(),renderer.renderHeight(),audio.active());
        if(action.seek>=0)seek(action.seek);
        if(action.toggleFullscreen){fullscreen=!fullscreen;SDL_SetWindowFullscreen(window.get(),fullscreen);}
        if(action.screenshot)pendingCapture=(fs::path(base)/("astra-"+std::to_string(SDL_GetTicks())+".bmp")).string();
        if(options.frames&&frame==options.frames-1&&!options.capture.empty())pendingCapture=options.capture;
        ImGui::Render();
        bool presented=renderer.draw(timeline,time,settings,ImGui::GetDrawData(),pendingCapture);
        if(presented&&!pendingCapture.empty()){log<<"Captured "<<pendingCapture<<'\n';pendingCapture.clear();}
        ++frame;
        if(options.frames&&frame>=options.frames)running=false;
    }
    SDL_WaitForGPUIdle(renderer.device());
    if(!pendingCapture.empty())throw std::runtime_error("Capture was not presented: "+pendingCapture);
    if(!automation)preference(settings,base+"/config/preferences.cfg",true);
    log<<"Frames: "<<frame<<"; average frame: "<<(measuredFrames?frameSum/measuredFrames*1000:0)<<" ms; longest: "<<maxFrame*1000<<" ms\n";
    log<<"Render resolution: "<<renderer.renderWidth()<<" x "<<renderer.renderHeight()<<"\nClean shutdown.\n";
    log<<"Transport: "<<time<<" s; completed loops: "<<static_cast<int>(time/timeline.length())<<'\n';
    return 0;
}
}
int main(int argc,char** argv) {
    std::ofstream log(fs::path(SDL_GetBasePath())/"astra.log");
    SDL_SetLogOutputFunction(logSDL,nullptr);
    try{
        int result=run(parse(argc,argv),log);
        std::lock_guard lock(diagnosticMutex);for(const auto& message:diagnostics)log<<"SDL diagnostic: "<<message<<'\n';
        return result;
    }
    catch(const std::exception& e) {
        log<<"ERROR: "<<e.what()<<'\n';log.flush();
        bool unattended=false;for(int i=1;i<argc;i++)if(std::string(argv[i])=="--frames"||std::string(argv[i])=="--gallery"||std::string(argv[i])=="--capture"||std::string(argv[i])=="--export-audio")unattended=true;
        if(!unattended)SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,"ASTRA could not start",e.what(),nullptr);
        return 1;
    }
}
