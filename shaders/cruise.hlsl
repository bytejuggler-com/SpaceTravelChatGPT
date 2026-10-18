#include "common.hlsli"
cbuffer Scene:register(b0,space3) { float4 screen; float4 params; }
float4 main(PixelIn input):SV_Target {
    float2 p=(input.uv-.5)*float2(screen.x/screen.y,1);
    float t=screen.z;
    float2 drift=float2(t*.012,0);
    float3 col=float3(.004,.005,.018);
    col+=nebula(rotate(p+drift,-.3),t,float3(.09,.08,.3));
    col+=nebula(rotate(p*.8+drift+1.4,.8),t,float3(.045,.18,.23));
    col+=stars(p+drift*.15,t)*.7;
    // A quiet crescent planet gives the side-scrolling field a sense of scale.
    float2 center=float2(.52-.035*sin(t*.09),-.05);
    float2 q=p-center; float radius=.205;
    float d=length(q);
    float halo=exp(-abs(d-radius)*95)*.10;
    col+=float3(.2,.45,1)*halo;
    if(d<radius){
        float3 n=float3(q/radius,sqrt(saturate(1-dot(q,q)/(radius*radius))));
        float light=pow(saturate(dot(n,normalize(float3(-.9,-.4,-.2)))),1.5);
        float surface=fbm(n*6+float3(t*.01,0,0));
        col=float3(.003,.006,.018)+float3(.13,.27,.50)*light*(.3+surface);
        col+=float3(.08,.28,.65)*pow(1-n.z,5)*saturate(dot(n.xy,normalize(float2(-1,-.4))));
    }
    return float4(col,1);
}
