#pragma once
#include <string>
#include <vector>

namespace astra {
struct SceneInfo { std::string id, title, subtitle, description; };
const std::vector<SceneInfo>& sceneRegistry();
struct Cue { int scene; double duration; double transition; };
struct Moment { int cue = 0; int nextCue = 0; double local = 0; double nextLocal = 0; float mix = 0; };
class Timeline {
public:
    Timeline();
    void load(const std::string& file);
    double length() const;
    double start(int index) const;
    double wrap(double time) const;
    Moment evaluate(double time) const;
    const std::vector<Cue>& cues() const { return cues_; }
private:
    std::vector<Cue> cues_;
};
struct Settings {
    bool paused = false, muted = false, interfaceVisible = true, settingsOpen = false;
    bool crt = false, pixel = false, palette = false, film = true;
    bool autoQuality = true;
    int quality = 1;
    float volume = 0.55f, bloom = 0.65f, exposure = 1.0f;
    float crtAmount = 0.45f, pixelSize = 4.0f, paletteLevels = 12.0f, filmAmount = 0.16f;
};
}
