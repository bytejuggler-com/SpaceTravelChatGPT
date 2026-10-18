#include "common.hlsli"
cbuffer Scene:register(b0,space3) { float4 screen; float4 params; }
float4 main(PixelIn input):SV_Target {
    float2 p=(input.uv-.5)*float2(screen.x/screen.y,1);
    float t=screen.z;
    float3 col=float3(.003,.004,.012)+stars(p,t)*.7;
    float2 q=rotate(p-float2(.13,-.025),-.30+.08*sin(t*.08));
    q.y/= .48+.04*sin(t*.07);
    float r=length(q), a=atan2(q.y,q.x);
    float winding=a-2.7*log(r+.07)-t*.025;
    float arm=pow(.5+.5*cos(winding*2),5);
    float clouds=fbm(float3(q*10,t*.03));
    float disk=exp(-r*3.6)*smoothstep(.94,.7,r);
    float dust=fbm(float3(q*25,t*.02));
    col+=float3(.16,.22,.48)*disk*(.14+arm*(.4+clouds*1.9));
    col+=float3(.32,.08,.16)*disk*pow(clouds,3)*arm;
    col*=1-saturate(dust-.43)*disk*1.5;
    col+=float3(1.6,1.15,.62)*exp(-r*21)*1.6;
    col+=float3(.50,.36,.25)*exp(-r*7)*.5;
    return float4(col,1);
}
