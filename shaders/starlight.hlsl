struct In {float4 position:SV_Position;float2 uv:TEXCOORD0;float3 color:TEXCOORD1;};
float4 main(In input):SV_Target {
    float r=dot(input.uv,input.uv);
    float glow=exp(-r*4.5)*smoothstep(1.0,.4,r);
    return float4(input.color*glow,glow);
}
