#pragma once
#include "core.h"
#include <SDL3/SDL.h>
#include <array>
#include <string>
#include <vector>

struct ImDrawData;
namespace astra {
// Every registered scene has its own background pipeline and optional shared GPU particles.
// Scene timing, resolution and quality are supplied without touching the application shell.
struct SceneRenderer {
    SDL_GPUGraphicsPipeline* background = nullptr;
    int particles = 0;
};
class Renderer {
public:
    ~Renderer();
    void initialize(SDL_Window* window, const std::string& assets, bool debug);
    bool draw(const Timeline& timeline, double time, const Settings& settings, ImDrawData* gui, const std::string& capture = {});
    SDL_GPUDevice* device() const { return gpu_; }
    std::string driver() const;
    int renderWidth() const { return rw_; }
    int renderHeight() const { return rh_; }
    void setAdaptiveScale(float scale) { adaptiveScale_ = scale; }
private:
    SDL_Window* window_ = nullptr;
    SDL_GPUDevice* gpu_ = nullptr;
    SDL_GPUSampler* linear_ = nullptr;
    std::vector<SceneRenderer> scenes_;
    SDL_GPUGraphicsPipeline *particles_=nullptr,*bloom_=nullptr,*composite_=nullptr;
    std::array<SDL_GPUTexture*,2> scene_{}, blur_{};
    SDL_GPUTexture* output_=nullptr;
    int width_=0,height_=0,rw_=0,rh_=0,bw_=0,bh_=0;
    float adaptiveScale_=1;
    std::string assets_;
    SDL_GPUShader* shader(const std::string& name, SDL_GPUShaderStage stage, int uniforms, int samplers);
    SDL_GPUGraphicsPipeline* pipeline(const std::string& vertex,const std::string& fragment,SDL_GPUTextureFormat format,int vertexUniforms,int fragmentUniforms,int samplers,bool additive=false);
    SDL_GPUTexture* texture(int w,int h,SDL_GPUTextureFormat format);
    void resize(int w,int h,float scale);
    void releaseTargets();
    void renderScene(SDL_GPUCommandBuffer* cmd,SDL_GPUTexture* target,int scene,double time,double duration,int quality);
    SDL_GPURenderPass* pass(SDL_GPUCommandBuffer* cmd,SDL_GPUTexture* target,bool clear=true);
};
}
