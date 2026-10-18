#pragma once
#include "core.h"
#include <string>
struct ImFont;
namespace astra {
struct UiActions { double seek = -1; bool toggleFullscreen = false; bool screenshot = false; };
class Interface {
public:
    void initialize();
    UiActions draw(const Timeline& timeline,double time,Settings& settings,const std::string& driver,int renderWidth,int renderHeight,bool audioAvailable);
private:
    ImFont *regular_=nullptr,*semibold_=nullptr,*mono_=nullptr;
    void settings(Settings& s,bool audioAvailable);
};
}
