Texture2D Textures : register(t0);
SamplerState Sampler : register(s0);

cbuffer TextureConstants : register(b5)
{
    float2 UVOffset;
};
cbuffer MatrixConstants : register(b0)
{
    row_major float4x4 MVP;
    row_major float4x4 MInverseTranspose;
    float4 UUID;
    bool isSelected;
    float3 MatrixPad0;
};
struct PS_INPUT
{
    float4 position : SV_POSITION;
    float2 UV       : TEXCOORD0;
};

struct PS_OUTPUT
{
    float4 color : SV_Target0;
};

PS_OUTPUT mainPS(PS_INPUT input)
{
    PS_OUTPUT output;
    output.color = Textures.Sample(Sampler, input.UV + UVOffset);
    if (isSelected)output.color+=float4(0.3,0.3,0.3,1);
    return output;
}
