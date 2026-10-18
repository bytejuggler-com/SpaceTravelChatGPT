#include "common.hlsli"
cbuffer Scene:register(b0,space1) { float4 screen; float4 params; }
struct Out {float4 position:SV_Position;float2 uv:TEXCOORD0;float3 color:TEXCOORD1;};
Out main(uint vertex:SV_VertexID,uint id:SV_InstanceID) {
    float2 corners[6]={float2(-1,-1),float2(1,-1),float2(1,1),float2(-1,-1),float2(1,1),float2(-1,1)};
    float2 corner=corners[vertex];
    float h=hash(id*3+1),j=hash(id*3+2),k=hash(id*3+3);
    float t=screen.z, aspect=screen.x/screen.y;
    float2 pos,axis=float2(1,0); float width,height,brightness;
    int scene=(int)params.x;
    if(scene==3) {
        float radius=pow(h,.66)*.87;
        float angle=2.7*log(radius+.07)+t*.025+(id%2)*PI+(j-.5)*.72;
        float2 disk=float2(cos(angle),sin(angle))*radius;
        disk.y*=.48+.04*sin(t*.07);
        pos=rotate(disk,.30-.08*sin(t*.08))+float2(.13,-.025);
        width=(.0005+.0009*k)*(screen.y/800);
        height=width;
        brightness=(.1+.8*pow(k,6))*smoothstep(.87,.65,radius);
    } else {
        float z=.4+frac(k-t*(scene==0?.065:.009))*12;
        float3 location=float3((h-.5)*22,(j-.5)*14,z);
        if(scene==1)location.x=(frac(h-t*.06)-.5)*22;
        pos=location.xy/z;
        if(scene==0)pos=rotate(pos,sin(t*.07)*.12)+float2(.18,-.06);
        else pos=rotate(pos,.025*sin(t*.1));
        axis=scene==0?normalize(pos-float2(.18,-.06)+.00001):float2(1,0);
        width=.0006+.0018/z;
        height=width;
        // Mild streaking suggests speed without turning the scene into a tunnel.
        width+=scene==0?length(pos)*.018/z:.018/z;
        brightness=(.3+1.7*h)*smoothstep(.4,.9,z)*smoothstep(12.4,9,z);
        if(scene==1){
            float2 planet=float2(.52-.035*sin(t*.09),-.05);
            brightness*=smoothstep(.205,.209,length(pos-planet));
        }
    }
    float2 perpendicular=float2(-axis.y,axis.x);
    float2 pixel=pos+axis*corner.x*width+perpendicular*corner.y*height;
    Out o;
    o.position=float4(pixel*float2(2/aspect,-2),0,1);
    o.uv=corner;
    o.color=lerp(float3(.40,.65,1.0),float3(1.0,.86,.64),hash(id+94))*brightness;
    return o;
}
