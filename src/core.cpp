#include "core.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace astra {
const std::vector<SceneInfo>& sceneRegistry() {
    static const std::vector<SceneInfo> scenes = {
        {"departure", "Into the unknown", "01 / DEPARTURE", "A thousand points of light. One way forward."},
        {"cruise", "Drift between worlds", "02 / INTERSTELLAR", "Ride the quiet current of the interstellar sea."},
        {"solar", "At the edge of fire", "03 / SOLAR APPROACH", "Skim the corona of a restless, burning giant."},
        {"galaxy", "Everything, from afar", "04 / GALACTIC ORBIT", "A spiral of light. An ocean of distant suns."}
    };
    return scenes;
}
Timeline::Timeline() : cues_{{0,10,1.4},{1,10,1.4},{2,12,1.8},{3,12,1.8}} {}
void Timeline::load(const std::string& file) {
    std::ifstream in(file);
    if (!in) throw std::runtime_error("Cannot open timeline: " + file);
    std::vector<Cue> result;
    std::string line;
    int number = 0;
    while (std::getline(in,line)) {
        ++number;
        line = line.substr(0, line.find('#'));
        std::istringstream row(line);
        std::string id, extra;
        if (!(row >> id)) continue;
        double duration = 0, transition = 0;
        if (!(row >> duration >> transition) || (row >> extra) || !std::isfinite(duration) ||
            !std::isfinite(transition) || duration < 2 || duration > 120 || transition < 0 || transition > duration * 0.45)
            throw std::runtime_error("Invalid timeline values on line " + std::to_string(number));
        const auto& registry = sceneRegistry();
        auto found = std::find_if(registry.begin(), registry.end(), [&](const auto& s){ return s.id == id; });
        if (found == registry.end()) throw std::runtime_error("Unknown scene: " + id);
        result.push_back({static_cast<int>(found-registry.begin()), duration, transition});
    }
    if (result.empty() || result.size() > 64) throw std::runtime_error("Timeline must contain 1 to 64 cues.");
    // Bound the generated soundtrack to a reasonable memory footprint.
    double total = 0;
    for (const auto& cue : result) total += cue.duration;
    if (total > 300) throw std::runtime_error("Timeline exceeds the five minute soundtrack limit.");
    cues_ = std::move(result);
}
double Timeline::length() const { double result=0; for (const auto& c:cues_) result+=c.duration; return result; }
double Timeline::start(int index) const { double result=0; for(int i=0;i<index && i<static_cast<int>(cues_.size());++i) result+=cues_[i].duration; return result; }
double Timeline::wrap(double time) const { double t=std::fmod(time,length()); return t<0?t+length():t; }
Moment Timeline::evaluate(double time) const {
    double t=wrap(time);
    int index=0;
    while (index+1 < static_cast<int>(cues_.size()) && t >= cues_[index].duration) t-=cues_[index++].duration;
    const auto& c=cues_[index];
    int next=(index+1)%static_cast<int>(cues_.size());
    // Incoming scenes preview negative local time, then continue through zero at the cut.
    // This avoids repeating the beginning of the incoming scene after a crossfade.
    float x=c.transition>0?static_cast<float>(std::clamp((t-(c.duration-c.transition))/c.transition,0.0,1.0)):0;
    return {index,next,t,t-c.duration,x*x*(3-2*x)};
}
}
