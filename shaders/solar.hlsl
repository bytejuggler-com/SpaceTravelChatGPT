#include "common.hlsli"
cbuffer Scene:register(b0,space3) { float4 screen; float4 params; }
float4 main(PixelIn input):SV_Target {
    float2 p=(input.uv-.5)*float2(screen.x/screen.y,1);
    float t=screen.z;
    float2 center=float2(.25+.07*sin(t*.11),-.025+.025*cos(t*.15));
    float radius=.295+.018*sin(t*.12);
    float2 q=rotate(p-center,t*.025);
    float d=length(q), r=d/radius, angle=atan2(q.y,q.x);
    float3 col=float3(.008,.003,.008)+stars(p,t)*.38;
    float corona=exp(-max(r-1,0)*5.0);
    float flame=fbm(float3(cos(angle)*4,sin(angle)*4,t*.16-r*2));
    float rays=pow(saturate(sin(angle*31+flame*17-t*.45)*.5+.5),5);
    float outer=exp(-max(r-1,0)*(9-6*flame));
    col+=float3(1,.20,.025)*corona*.32;
    col+=float3(1,.36,.045)*outer*(.22+flame*.65+rays*.32);
    // Sparse curling prominence arcs rise above the limb.
    [unroll] for(int k=0;k<5;k++) {
        float a=k*1.256+sin(t*.04)*.14;
        float2 c=float2(cos(a),sin(a))*1.03;
        float2 v=q/radius-c;
        float arch=abs(length(v*float2(1,1.4))-(.13+.04*sin(t*.5+k)));
        float mask=smoothstep(1.0,1.06,r);
        col+=float3(2,.38,.035)*exp(-arch*180)*mask*(.55+.45*sin(t*.8+k)*sin(t*.8+k));
    }
    if(r<1) {
        float3 n=float3(q/radius,sqrt(saturate(1-r*r)));
        float viewZ=n.z;
        n.xz=rotate(n.xz,t*.06);
        float turbulence=fbm(n*7+float3(0,t*.22,0));
        float cells=noise(n*48+float3(t*.35,t*.17,0));
        float veins=abs(fbm(n*14+float3(t*.09,0,t*.13))-.5);
        float hot=saturate(turbulence*.9+cells*.32+veins*.7);
        float3 fire=lerp(float3(.65,.055,.004),float3(3.5,1.35,.16),hot);
        float rim=pow(viewZ,.3);
        col=fire*(.40+.60*rim)+float3(1.0,.18,.008)*pow(1-viewZ,3);
    }
    // Anamorphic scatter around the hot limb.
    col+=float3(.32,.075,.012)*exp(-abs(p.y-center.y)*80)*exp(-abs(p.x-center.x)*1.8)*smoothstep(.95,1.2,r);
    return float4(col,1);
}
