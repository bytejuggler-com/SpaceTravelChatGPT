#include "audio.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <numbers>

namespace astra {
namespace {
constexpr double tau = 2 * std::numbers::pi;
double hz(int midi) { return 440.0 * std::pow(2.0, (midi-69)/12.0); }
double fade(double a, double b, double x) { double u=std::clamp((x-a)/(b-a),0.0,1.0); return u*u*(3-2*u); }
// Table lookup keeps soundtrack preparation quick even on integrated-laptop CPUs.
float oscillator(double cycles) {
    static const auto table=[] {
        std::array<float,16385> t{};
        for (size_t i=0;i<t.size();++i) t[i]=static_cast<float>(std::sin(tau*i/16384));
        return t;
    }();
    double p=(cycles-std::floor(cycles))*16384;
    int i=static_cast<int>(p);
    return table[i]+static_cast<float>(p-i)*(table[i+1]-table[i]);
}
}
Soundtrack Soundtrack::synthesize(const Timeline& timeline) {
    Soundtrack result;
    const int frames=static_cast<int>(std::lround(timeline.length()*sampleRate));
    result.samples.resize(static_cast<size_t>(frames)*2);
    constexpr int chords[4][4]={{48,55,58,63},{44,51,55,60},{41,48,51,56},{43,50,53,58}};
    uint32_t random=0xA57A2026;
    for (int i=0;i<frames;++i) {
        double t=static_cast<double>(i)/sampleRate;
        double chordLength=timeline.length()/4;
        int chord=std::min(3,static_cast<int>(t/chordLength));
        double chordTime=t-chord*chordLength;
        double mix=fade(0,1.3,chordTime);
        double padL=0,padR=0;
        for (int n=0;n<4;++n) {
            double f=hz(chords[chord][n]);
            double prev=hz(chords[(chord+3)%4][n]);
            padL+=(oscillator(t*f)+0.25*oscillator(t*f*2.002))*mix;
            padR+=(oscillator(t*f*1.0015)+0.25*oscillator(t*f*1.998)) *mix;
            padL+=(oscillator(t*prev)+0.25*oscillator(t*prev*2.002))*(1-mix);
            padR+=(oscillator(t*prev*1.0015)+0.25*oscillator(t*prev*1.998))*(1-mix);
        }
        double beat=t*2; // 120 BPM
        double pulse=(beat-std::floor(beat))*0.5;
        double sub=oscillator(t*hz(chords[chord][0]-12))*0.09;
        double kick=oscillator(48*pulse+6*(1-std::exp(-pulse*28)))*std::exp(-pulse*17)*0.18;
        double step=std::floor(beat*2);
        double noteTime=(beat*2-step)*0.25;
        int arpIndex=static_cast<int>(step)%8;
        int note=chords[chord][arpIndex%4]+12+(arpIndex>=4?12:0);
        double bell=(oscillator(noteTime*hz(note)) + .22*oscillator(noteTime*hz(note)*2))*std::exp(-noteTime*12)*fade(0,.008,noteTime)*0.055;
        random=random*1664525u+1013904223u;
        double noise=static_cast<double>(random>>8)/8388608.0-1;
        double hat=noise*std::exp(-noteTime*95)*0.022;
        double atmosphere=noise*0.002*(0.5+0.5*oscillator(t*.09));
        double env=fade(0,1.5,t)*(1-fade(result.duration()-1.5,result.duration(),t));
        result.samples[static_cast<size_t>(i)*2]=static_cast<float>((padL*.047+sub+kick+bell*(.8+.2*oscillator(t*.3))+hat+atmosphere)*env);
        result.samples[static_cast<size_t>(i)*2+1]=static_cast<float>((padR*.047+sub+kick+bell*(.8-.2*oscillator(t*.3))+hat+atmosphere)*env);
    }
    // A short stereo echo/reverb tail, rendered once. Fade boundaries for a click-free loop.
    const int delay=static_cast<int>(sampleRate*.375);
    for (int i=delay;i<frames;++i) {
        for (int ch=0;ch<2;++ch) {
            size_t j=static_cast<size_t>(i)*2+ch;
            float dry=result.samples[j];
            float echo=result.samples[static_cast<size_t>(i-delay)*2+1-ch]*.25f;
            double end=1-fade(result.duration()-.8,result.duration(),static_cast<double>(i)/sampleRate);
            result.samples[j]=static_cast<float>(std::tanh(dry+echo)*end);
        }
    }
    return result;
}
bool Soundtrack::saveWav(const std::string& path) const {
    std::ofstream out(path,std::ios::binary);
    if(!out)return false;
    auto u16=[&](uint16_t n){ char b[2]={static_cast<char>(n),static_cast<char>(n>>8)}; out.write(b,2); };
    auto u32=[&](uint32_t n){ char b[4]={static_cast<char>(n),static_cast<char>(n>>8),static_cast<char>(n>>16),static_cast<char>(n>>24)}; out.write(b,4); };
    uint32_t size=static_cast<uint32_t>(samples.size()*2);
    out.write("RIFF",4); u32(36+size); out.write("WAVEfmt ",8); u32(16); u16(1); u16(2);
    u32(sampleRate); u32(sampleRate*4); u16(4); u16(16); out.write("data",4); u32(size);
    for(float s:samples) u16(static_cast<uint16_t>(static_cast<int16_t>(std::clamp(s,-1.0f,1.0f)*32767)));
    return out.good();
}
Audio::~Audio(){ if(stream_)SDL_DestroyAudioStream(stream_); }
bool Audio::open(Soundtrack track) {
    track_=std::move(track);
    SDL_AudioSpec spec{SDL_AUDIO_F32,2,Soundtrack::sampleRate};
    stream_=SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,&spec,nullptr,nullptr);
    if(!stream_) { error_=SDL_GetError(); return false; }
    seek(0);
    return true;
}
void Audio::fill() {
    if(!stream_ || track_.samples.empty())return;
    constexpr size_t block=4800; // 100 ms queued ahead; no synthesis on the playback path.
    while(SDL_GetAudioStreamQueued(stream_) < static_cast<int>(block*2*sizeof(float))) {
        size_t count=std::min(block,track_.samples.size()/2-cursor_);
        if (!SDL_PutAudioStreamData(stream_,track_.samples.data()+cursor_*2,static_cast<int>(count*2*sizeof(float)))) {
            error_=SDL_GetError(); SDL_DestroyAudioStream(stream_); stream_=nullptr; return;
        }
        submitted_+=count; cursor_=(cursor_+count)%(track_.samples.size()/2);
    }
}
void Audio::seek(double seconds) {
    if(!stream_)return;
    SDL_PauseAudioStreamDevice(stream_);
    SDL_ClearAudioStream(stream_);
    double t=std::fmod(seconds,track_.duration()); if(t<0)t+=track_.duration();
    cursor_=static_cast<size_t>(t*Soundtrack::sampleRate)%(track_.samples.size()/2);
    base_=static_cast<double>(cursor_)/Soundtrack::sampleRate;
    submitted_=0; lastConsumed_=0; lastTick_=SDL_GetTicksNS();
    fill();
    if(stream_ && !paused_)SDL_ResumeAudioStreamDevice(stream_);
}
void Audio::update(bool paused,float gain) {
    if(!stream_)return;
    SDL_SetAudioStreamGain(stream_,gain);
    if(paused!=paused_) {
        if(paused)SDL_PauseAudioStreamDevice(stream_); else SDL_ResumeAudioStreamDevice(stream_);
        paused_=paused; lastTick_=SDL_GetTicksNS();
    }
    fill();
}
double Audio::position() const {
    if(!stream_)return base_;
    int bytes=std::max(0,SDL_GetAudioStreamQueued(stream_));
    uint64_t queued=static_cast<uint64_t>(bytes)/(2*sizeof(float));
    uint64_t consumed=submitted_>=queued?submitted_-queued:0;
    uint64_t now=SDL_GetTicksNS();
    if(consumed!=lastConsumed_) {lastConsumed_=consumed;lastTick_=now;}
    // Interpolate between device pulls, bounded to one typical device buffer.
    double extra=paused_?0:std::min(.015,static_cast<double>(now-lastTick_)*1e-9);
    return base_+static_cast<double>(consumed)/Soundtrack::sampleRate+extra;
}
}
