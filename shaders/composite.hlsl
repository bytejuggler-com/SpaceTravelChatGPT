#include "common.hlsli"
Texture2D<float4> sceneA:register(t0,space2);
Texture2D<float4> sceneB:register(t1,space2);
Texture2D<float4> bloomTex:register(t2,space2);
SamplerState samplerA:register(s0,space2);
SamplerState samplerB:register(s1,space2);
SamplerState samplerBloom:register(s2,space2);
cbuffer Post:register(b0,space3) {
    float4 screen; // width, height, time, transition
    float4 style; // bloom, exposure, CRT, pixel size
    float4 film; // palette levels, grain, unused, unused
}
float3 aces(float3 x) {return saturate((x*(2.51*x+.03))/(x*(2.43*x+.59)+.14));}
float4 main(PixelIn input):SV_Target {
    float2 uv=input.uv;
    if(style.z>0){float2 p=uv*2-1;p*=1+dot(p,p)*.035*style.z;uv=p*.5+.5;}
    float mask=step(0,uv.x)*step(0,uv.y)*step(uv.x,1)*step(uv.y,1);
    if(style.w>1)uv=(floor(uv*screen.xy/style.w)+.5)*style.w/screen.xy;
    float3 a=sceneA.Sample(samplerA,uv).rgb,b=sceneB.Sample(samplerB,uv).rgb;
    float3 color=lerp(a,b,screen.w);
    color+=bloomTex.Sample(samplerBloom,uv).rgb*style.x;
    color=aces(color*style.y);
    color=pow(max(color,0),1.0/2.2);
    float vignette=1-dot((input.uv-.5)*float2(1.1,1.3),(input.uv-.5)*float2(1.1,1.3))*.50;
    color*=vignette;
    if(style.z>0){
        color*=1-style.z*.16*(.5+.5*sin(input.position.y*PI));
        float3 phosphor=.86+.14*cos(input.position.x*2.094+float3(0,2.094,4.188));
        color*=lerp(1,phosphor,style.z);
    }
    if(film.x>0){
        const float bayer[16]={0,8,2,10,12,4,14,6,3,11,1,9,15,7,13,5};
        uint2 cell=(uint2)floor(input.position.xy/max(style.w,1));
        float dither=(bayer[(cell.y%4)*4+cell.x%4]/16.0-.5)/film.x;
        color=floor(saturate(color+dither)*film.x+.5)/film.x;
    }
    color+=(hash2(input.position.xy+frac(screen.z)*193.7)-.5)*film.y*.07;
    return float4(saturate(color)*mask,1);
}
