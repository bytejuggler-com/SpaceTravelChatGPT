Texture2D<float4> source:register(t0,space2);
Texture2D<float4> incoming:register(t1,space2);
SamplerState linearSampler:register(s0,space2);
SamplerState incomingSampler:register(s1,space2);
cbuffer Blur:register(b0,space3) {float4 data;}
struct In {float4 position:SV_Position;float2 uv:TEXCOORD0;};
float4 main(In input):SV_Target {
    float3 col=0;
    [unroll] for(int y=-2;y<=2;y++) {
        [unroll] for(int x=-2;x<=2;x++) {
            float2 uv=input.uv+float2(x,y)*data.xy*2;
            float3 s=lerp(source.Sample(linearSampler,uv).rgb,incoming.Sample(incomingSampler,uv).rgb,data.w);
            if(data.z>0.5)s=max(s-.50,0);
            float w=(3-abs(x))*(3-abs(y));
            col+=s*w/81.0;
        }
    }
    return float4(col,1);
}
