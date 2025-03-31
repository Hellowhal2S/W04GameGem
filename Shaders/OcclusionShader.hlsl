// OcclusionShader.hlsl

cbuffer cbWorld : register(b0)
{
    row_major float4x4 World;
};

cbuffer cbViewProj : register(b1)
{
    row_major float4x4 ViewProj;
};

struct VSInput
{
    float3 Position : POSITION;
    //float4 Color : COLOR;
};

struct VSOutput
{
    float4 Pos : SV_POSITION;
    //float4 Color : COLOR;
};

VSOutput VS(VSInput input)
{
    VSOutput output;
    float4 worldPos = mul(float4(input.Position, 1.0f), World);
    output.Pos = mul(worldPos, ViewProj);
    //output.Color = input.Color;
    return output;
}

// float4 PS(VSOutput input) : SV_TARGET
// {
//     return input.Color;
// }

// Pixel Shader 생략 (사용하지 않음)
