#include "core.h"
#include "audio.h"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace astra;
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main() {
    try {
        Timeline t;
        require(t.length()==44,"Default timeline length");
        require(t.evaluate(0).cue==0,"First cue");
        require(t.evaluate(10).cue==1,"Exact boundary");
        require(t.evaluate(44).cue==0,"Loop boundary");
        require(t.evaluate(-.1).cue==3,"Negative wrap");
        auto before=t.evaluate(9.99999),after=t.evaluate(10);
        require(before.mix>.999f,"Crossfade approaches incoming scene");
        require(std::abs(before.nextLocal-after.local)<.0001,"Incoming scene clock stays continuous");
        for(double time=-5;time<100;time+=.017){auto m=t.evaluate(time);require(m.mix>=0&&m.mix<=1,"Transition range");require(m.local>=0&&m.local<t.cues()[m.cue].duration,"Local time range");}
        auto path=std::filesystem::temp_directory_path()/"astra-timeline-test.cfg";
        {std::ofstream f(path);f<<"galaxy 4 .5\n departure 2 0\n";}
        t.load(path.string());require(t.length()==6&&t.cues()[0].scene==3,"Reordered timeline");
        for(const char* invalid:{"bad 4 .5","solar 4 3","solar 0 0","solar 4 .5 junk",""}) {
            {std::ofstream f(path);f<<invalid;}
            bool rejected=false;try{t.load(path.string());}catch(...){rejected=true;}
            require(rejected,"Invalid configuration rejected");
            require(t.length()==6,"Invalid load leaves previous timeline intact");
        }
        std::filesystem::remove(path);
        auto track=Soundtrack::synthesize(t);
        require(track.samples.size()==6*48000*2,"Soundtrack follows timeline duration");
        double sum=0,peak=0,difference=0;
        for(size_t i=0;i<track.samples.size();++i){float sample=track.samples[i];require(std::isfinite(sample)&&std::abs(sample)<=1,"Finite unclipped samples");sum+=sample*sample;peak=std::max(peak,static_cast<double>(std::abs(sample)));if(i%2==0)difference+=std::abs(sample-track.samples[i+1]);}
        require(std::sqrt(sum/track.samples.size())>.01,"Audible soundtrack");
        require(peak>.1&&difference>1,"Musical dynamics and stereo separation");
        require(std::abs(track.samples.front())<.001&&std::abs(track.samples.back())<.001,"Click-free boundaries");
        std::cout<<"Timeline, configuration validation, transition continuity, and soundtrack tests passed.\n";
        return 0;
    } catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
