
cbuffer cbViewProj : register(b0) {
    float4x4 MVP;
};

struct VSInput {
    float3 pos : POSITION;
};

struct VSOutput {
    float4 pos : SV_POSITION;
};

VSOutput VS(VSInput input) {
    VSOutput o;
    o.pos = mul(input.pos, MVP);
    return o;
}