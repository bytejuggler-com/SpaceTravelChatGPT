#pragma once
#include "core.h"
#include <SDL3/SDL.h>
#include <string>
#include <vector>

namespace astra {
struct Soundtrack {
    static constexpr int sampleRate = 48000;
    std::vector<float> samples;
    double duration() const { return static_cast<double>(samples.size()/2)/sampleRate; }
    static Soundtrack synthesize(const Timeline& timeline);
    bool saveWav(const std::string& path) const;
};
class Audio {
public:
    ~Audio();
    bool open(Soundtrack track);
    void update(bool paused, float gain);
    void seek(double seconds);
    double position() const;
    bool active() const { return stream_ != nullptr; }
    const std::string& error() const { return error_; }
private:
    Soundtrack track_;
    SDL_AudioStream* stream_ = nullptr;
    std::string error_;
    bool paused_ = true;
    double base_ = 0;
    uint64_t submitted_ = 0;
    size_t cursor_ = 0;
    mutable uint64_t lastConsumed_ = 0, lastTick_ = 0;
    void fill();
};
}
