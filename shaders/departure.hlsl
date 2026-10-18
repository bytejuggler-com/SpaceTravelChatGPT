#include "common.hlsli"
cbuffer Scene:register(b0,space3) { float4 screen; float4 params; }
float4 main(PixelIn input):SV_Target {
    float2 p=(input.uv-.5)*float2(screen.x/screen.y,1);
    float t=screen.z;
    p=rotate(p,sin(t*.07)*.12);
    float3 col=float3(.002,.005,.014);
    col+=nebula(p+float2(.22,-.12),t,float3(.035,.16,.22));
    col+=nebula(rotate(p,1.2)+1.6,t,float3(.12,.045,.17))*.45;
    col+=stars(p+float2(t*.0007,0),t)*.45;
    col+=float3(.03,.09,.11)*exp(-length(p-float2(.18,-.06))*6);
    return float4(col,1);
}
