#ifndef ASTRA_COMMON
#define ASTRA_COMMON
#define PI 3.14159265359
float hash(float n) { return frac(sin(n*127.1)*43758.5453); }
float hash2(float2 p) { return frac(sin(dot(p,float2(127.1,311.7)))*43758.5453); }
float hash3(float3 p) { return frac(sin(dot(p,float3(127.1,311.7,74.7)))*43758.5453); }
float noise(float3 p) {
    float3 i=floor(p),f=frac(p); f=f*f*(3-2*f);
    return lerp(lerp(lerp(hash3(i),hash3(i+float3(1,0,0)),f.x),lerp(hash3(i+float3(0,1,0)),hash3(i+float3(1,1,0)),f.x),f.y),
                lerp(lerp(hash3(i+float3(0,0,1)),hash3(i+float3(1,0,1)),f.x),lerp(hash3(i+float3(0,1,1)),hash3(i+float3(1,1,1)),f.x),f.y),f.z);
}
float fbm(float3 p) {
    float v=0,a=.5;
    [unroll] for(int i=0;i<4;i++){v+=a*noise(p);p=p*2.03+float3(12.3,5.7,8.2);a*=.5;}
    return v;
}
float2 rotate(float2 p,float a){float s=sin(a),c=cos(a);return float2(c*p.x-s*p.y,s*p.x+c*p.y);}
float3 stars(float2 p,float time) {
    float3 color=0;
    [unroll] for(int layer=0;layer<3;layer++){
        float scale=90+layer*52;
        float2 q=p*scale+layer*48.73;
        float2 cell=floor(q), f=frac(q)-.5;
        float h=hash2(cell);
        float2 center=float2(hash2(cell+3),hash2(cell+7))*.65-.325;
        float d=length(f-center);
        float star=exp(-d*d*(layer==0?1800:3300));
        float alive=step(.955,h);
        color+=alive*star*(.4+1.5*hash2(cell+9))*(.82+.18*sin(time*.6+h*60))*lerp(float3(.55,.7,1),float3(1,.85,.65),h);
    }
    return color;
}
float3 nebula(float2 p,float t,float3 tint) {
    float n=fbm(float3(p*2.2,t*.012));
    float band=exp(-pow((p.y+.2*sin(p.x*1.4))*(2.1+n*2),2));
    return tint*pow(n,2.4)*band;
}
struct PixelIn { float4 position:SV_Position; float2 uv:TEXCOORD0; };
#endif
