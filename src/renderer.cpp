#include "renderer.h"
#include "imgui_impl_sdlgpu3.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <stdexcept>
#include <vector>

namespace astra {
namespace {
constexpr auto hdr=SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
constexpr auto sdr=SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
void check(bool ok,const char* what){if(!ok)throw std::runtime_error(std::string(what)+": "+SDL_GetError());}
struct SceneUniform { float screen[4]; float params[4]; };
struct PostUniform { float screen[4]; float style[4]; float film[4]; };
}
Renderer::~Renderer() {
    if(!gpu_)return;
    SDL_WaitForGPUIdle(gpu_);
    releaseTargets();
    for(auto& s:scenes_)if(s.background)SDL_ReleaseGPUGraphicsPipeline(gpu_,s.background);
    if(particles_)SDL_ReleaseGPUGraphicsPipeline(gpu_,particles_);
    if(bloom_)SDL_ReleaseGPUGraphicsPipeline(gpu_,bloom_);
    if(composite_)SDL_ReleaseGPUGraphicsPipeline(gpu_,composite_);
    if(linear_)SDL_ReleaseGPUSampler(gpu_,linear_);
    if(window_)SDL_ReleaseWindowFromGPUDevice(gpu_,window_);
    SDL_DestroyGPUDevice(gpu_);
}
std::string Renderer::driver() const {return gpu_?SDL_GetGPUDeviceDriver(gpu_):"unavailable";}
SDL_GPUShader* Renderer::shader(const std::string& name,SDL_GPUShaderStage stage,int uniforms,int samplers) {
    std::ifstream file(assets_+"/shaders/"+name+".dxil",std::ios::binary|std::ios::ate);
    if(!file)throw std::runtime_error("Missing compiled shader: "+name);
    size_t size=static_cast<size_t>(file.tellg());
    std::vector<Uint8> bytes(size); file.seekg(0);file.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(size));
    SDL_GPUShaderCreateInfo info{};
    info.code=bytes.data();info.code_size=size;info.entrypoint="main";info.format=SDL_GPU_SHADERFORMAT_DXIL;
    info.stage=stage;info.num_uniform_buffers=uniforms;info.num_samplers=samplers;
    auto* result=SDL_CreateGPUShader(gpu_,&info);check(result!=nullptr,name.c_str());return result;
}
SDL_GPUGraphicsPipeline* Renderer::pipeline(const std::string& vertex,const std::string& fragment,SDL_GPUTextureFormat format,int vu,int fu,int samplers,bool additive) {
    SDL_GPUShader* vs=shader(vertex,SDL_GPU_SHADERSTAGE_VERTEX,vu,0);
    SDL_GPUShader* fs=nullptr;
    try {fs=shader(fragment,SDL_GPU_SHADERSTAGE_FRAGMENT,fu,samplers);} catch(...){SDL_ReleaseGPUShader(gpu_,vs);throw;}
    SDL_GPUColorTargetDescription color{};color.format=format;
    if(additive) {
        color.blend_state.enable_blend=true;
        color.blend_state.src_color_blendfactor=SDL_GPU_BLENDFACTOR_ONE;
        color.blend_state.dst_color_blendfactor=SDL_GPU_BLENDFACTOR_ONE;
        color.blend_state.color_blend_op=SDL_GPU_BLENDOP_ADD;
        color.blend_state.src_alpha_blendfactor=SDL_GPU_BLENDFACTOR_ONE;
        color.blend_state.dst_alpha_blendfactor=SDL_GPU_BLENDFACTOR_ONE;
        color.blend_state.alpha_blend_op=SDL_GPU_BLENDOP_ADD;
    }
    SDL_GPUGraphicsPipelineCreateInfo info{};
    info.vertex_shader=vs;info.fragment_shader=fs;
    info.primitive_type=SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    info.rasterizer_state.fill_mode=SDL_GPU_FILLMODE_FILL;
    info.rasterizer_state.cull_mode=SDL_GPU_CULLMODE_NONE;
    info.multisample_state.sample_count=SDL_GPU_SAMPLECOUNT_1;
    info.target_info.color_target_descriptions=&color;info.target_info.num_color_targets=1;
    auto* result=SDL_CreateGPUGraphicsPipeline(gpu_,&info);
    SDL_ReleaseGPUShader(gpu_,vs);SDL_ReleaseGPUShader(gpu_,fs);
    check(result!=nullptr,("Pipeline "+fragment).c_str());return result;
}
void Renderer::initialize(SDL_Window* window,const std::string& assets,bool debug) {
    window_=window;assets_=assets;
    gpu_=SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_DXIL,debug,"direct3d12");
    check(gpu_!=nullptr,"Direct3D 12 initialization (update your graphics driver if needed)");
    check(SDL_ClaimWindowForGPUDevice(gpu_,window_),"Claim window");
    check(SDL_SetGPUSwapchainParameters(gpu_,window_,SDL_GPU_SWAPCHAINCOMPOSITION_SDR,SDL_GPU_PRESENTMODE_VSYNC),"Vsync");
    SDL_GPUSamplerCreateInfo sampler{};
    sampler.min_filter=SDL_GPU_FILTER_LINEAR;sampler.mag_filter=SDL_GPU_FILTER_LINEAR;
    sampler.mipmap_mode=SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    sampler.address_mode_u=sampler.address_mode_v=sampler.address_mode_w=SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    linear_=SDL_CreateGPUSampler(gpu_,&sampler);check(linear_!=nullptr,"Sampler");
    for(const auto& s:sceneRegistry()) {
        SceneRenderer scene;
        scene.background=pipeline("fullscreen",s.id,hdr,0,1,0);
        scene.particles=s.id=="solar"?0:(s.id=="galaxy"?28000:5500);
        scenes_.push_back(scene);
    }
    particles_=pipeline("particles","starlight",hdr,1,0,0,true);
    bloom_=pipeline("fullscreen","bloom",hdr,0,1,2);
    composite_=pipeline("fullscreen","composite",sdr,0,1,3);
}
SDL_GPUTexture* Renderer::texture(int w,int h,SDL_GPUTextureFormat format) {
    SDL_GPUTextureCreateInfo info{};info.type=SDL_GPU_TEXTURETYPE_2D;
    info.format=format;info.usage=SDL_GPU_TEXTUREUSAGE_COLOR_TARGET|SDL_GPU_TEXTUREUSAGE_SAMPLER;
    info.width=w;info.height=h;info.layer_count_or_depth=1;info.num_levels=1;info.sample_count=SDL_GPU_SAMPLECOUNT_1;
    auto* result=SDL_CreateGPUTexture(gpu_,&info);check(result!=nullptr,"Render texture");return result;
}
void Renderer::releaseTargets() {
    for(auto*& t:scene_){if(t)SDL_ReleaseGPUTexture(gpu_,t);t=nullptr;}
    for(auto*& t:blur_){if(t)SDL_ReleaseGPUTexture(gpu_,t);t=nullptr;}
    if(output_)SDL_ReleaseGPUTexture(gpu_,output_);output_=nullptr;
}
void Renderer::resize(int w,int h,float scale) {
    int rw=std::max(320,static_cast<int>(w*scale)),rh=std::max(180,static_cast<int>(h*scale));
    if(w==width_&&h==height_&&rw==rw_&&rh==rh_)return;
    SDL_WaitForGPUIdle(gpu_);releaseTargets();
    width_=w;height_=h;rw_=rw;rh_=rh;bw_=std::max(1,rw/4);bh_=std::max(1,rh/4);
    for(auto*& t:scene_)t=texture(rw_,rh_,hdr);
    for(auto*& t:blur_)t=texture(bw_,bh_,hdr);
    output_=texture(w,h,sdr);
}
SDL_GPURenderPass* Renderer::pass(SDL_GPUCommandBuffer* cmd,SDL_GPUTexture* target,bool clear) {
    SDL_GPUColorTargetInfo info{};info.texture=target;info.clear_color={0,0,0,1};
    info.load_op=clear?SDL_GPU_LOADOP_CLEAR:SDL_GPU_LOADOP_LOAD;info.store_op=SDL_GPU_STOREOP_STORE;info.cycle=clear;
    auto* result=SDL_BeginGPURenderPass(cmd,&info,1,nullptr);check(result!=nullptr,"Render pass");return result;
}
void Renderer::renderScene(SDL_GPUCommandBuffer* cmd,SDL_GPUTexture* target,int scene,double time,double duration,int quality) {
    SceneUniform data{{static_cast<float>(rw_),static_cast<float>(rh_),static_cast<float>(time+30),static_cast<float>(time/duration)},
                      {static_cast<float>(scene),static_cast<float>(quality),0,0}};
    SDL_PushGPUFragmentUniformData(cmd,0,&data,sizeof(data));
    SDL_PushGPUVertexUniformData(cmd,0,&data,sizeof(data));
    auto* p=pass(cmd,target);
    SDL_BindGPUGraphicsPipeline(p,scenes_.at(scene).background);SDL_DrawGPUPrimitives(p,3,1,0,0);
    if(scenes_[scene].particles) {
        int count=scenes_[scene].particles*(quality==0?1:(quality==2?3:2))/2;
        SDL_BindGPUGraphicsPipeline(p,particles_);SDL_DrawGPUPrimitives(p,6,count,0,0);
    }
    SDL_EndGPURenderPass(p);
}
bool Renderer::draw(const Timeline& timeline,double time,const Settings& settings,ImDrawData* gui,const std::string& capture) {
    auto* cmd=SDL_AcquireGPUCommandBuffer(gpu_);check(cmd!=nullptr,"Command buffer");
    SDL_GPUTexture* swapchain=nullptr;Uint32 w=0,h=0;
    if(!SDL_WaitAndAcquireGPUSwapchainTexture(cmd,window_,&swapchain,&w,&h)){SDL_CancelGPUCommandBuffer(cmd);check(false,"Swapchain");}
    if(!swapchain){SDL_SubmitGPUCommandBuffer(cmd);return false;}
    const float scales[]={.5f,.75f,1.0f};
    resize(static_cast<int>(w),static_cast<int>(h),scales[settings.quality]*(settings.autoQuality?adaptiveScale_:1));
    auto moment=timeline.evaluate(time);
    const auto& a=timeline.cues()[moment.cue]; const auto& b=timeline.cues()[moment.nextCue];
    renderScene(cmd,scene_[0],a.scene,moment.local,a.duration,settings.quality);
    auto* incoming=scene_[0];
    if(moment.mix>0){renderScene(cmd,scene_[1],b.scene,moment.nextLocal,b.duration,settings.quality);incoming=scene_[1];}
    for(int i=0;i<2;++i) {
        float data[4]={1.0f/(i?bw_:rw_),1.0f/(i?bh_:rh_),i?0.0f:1.0f,i?0:moment.mix};
        SDL_PushGPUFragmentUniformData(cmd,0,data,sizeof(data));
        auto* p=pass(cmd,blur_[i]);SDL_BindGPUGraphicsPipeline(p,bloom_);
        SDL_GPUTextureSamplerBinding bindings[2]={{i?blur_[0]:scene_[0],linear_},{i?blur_[0]:incoming,linear_}};
        SDL_BindGPUFragmentSamplers(p,0,bindings,2);SDL_DrawGPUPrimitives(p,3,1,0,0);SDL_EndGPURenderPass(p);
    }
    PostUniform post{{static_cast<float>(w),static_cast<float>(h),static_cast<float>(time),moment.mix},
                     {settings.bloom,settings.exposure,settings.crt?settings.crtAmount:0,settings.pixel?settings.pixelSize:0},
                     {settings.palette?settings.paletteLevels:0,settings.film?settings.filmAmount:0,0,0}};
    SDL_PushGPUFragmentUniformData(cmd,0,&post,sizeof(post));
    auto* p=pass(cmd,output_);SDL_BindGPUGraphicsPipeline(p,composite_);
    SDL_GPUTextureSamplerBinding bindings[3]={{scene_[0],linear_},{incoming,linear_},{blur_[1],linear_}};
    SDL_BindGPUFragmentSamplers(p,0,bindings,3);SDL_DrawGPUPrimitives(p,3,1,0,0);SDL_EndGPURenderPass(p);
    if(gui) {
        ImGui_ImplSDLGPU3_PrepareDrawData(gui,cmd);
        p=pass(cmd,output_,false);ImGui_ImplSDLGPU3_RenderDrawData(gui,cmd,p);SDL_EndGPURenderPass(p);
    }
    SDL_GPUBlitInfo blit{};blit.source.texture=output_;blit.source.w=w;blit.source.h=h;
    blit.destination.texture=swapchain;blit.destination.w=w;blit.destination.h=h;
    blit.load_op=SDL_GPU_LOADOP_DONT_CARE;blit.filter=SDL_GPU_FILTER_NEAREST;
    SDL_BlitGPUTexture(cmd,&blit);
    if(capture.empty()) {check(SDL_SubmitGPUCommandBuffer(cmd),"Submit frame");return true;}
    Uint32 pitch=(w*4+255)&~255u;
    SDL_GPUTransferBufferCreateInfo ti{};ti.usage=SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;ti.size=pitch*h;
    auto* transfer=SDL_CreateGPUTransferBuffer(gpu_,&ti);check(transfer!=nullptr,"Capture buffer");
    SDL_GPUTextureRegion region{};region.texture=output_;region.w=w;region.h=h;region.d=1;
    SDL_GPUTextureTransferInfo dest{transfer,0,pitch/4,h};
    auto* copy=SDL_BeginGPUCopyPass(cmd);SDL_DownloadFromGPUTexture(copy,&region,&dest);SDL_EndGPUCopyPass(copy);
    auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);check(fence!=nullptr,"Capture fence");
    check(SDL_WaitForGPUFences(gpu_,true,&fence,1),"Wait capture");SDL_ReleaseGPUFence(gpu_,fence);
    void* pixels=SDL_MapGPUTransferBuffer(gpu_,transfer,false);
    check(pixels!=nullptr,"Map capture");
    auto* surface=SDL_CreateSurfaceFrom(w,h,SDL_PIXELFORMAT_RGBA32,pixels,pitch);
    bool saved=surface&&SDL_SaveBMP(surface,capture.c_str());
    if(surface)SDL_DestroySurface(surface);
    SDL_UnmapGPUTransferBuffer(gpu_,transfer);SDL_ReleaseGPUTransferBuffer(gpu_,transfer);
    check(saved,"Save capture");return true;
}
}
