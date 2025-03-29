Texture2D Textures : register(t0);
SamplerState Sampler : register(s0);

cbuffer TextureConstants : register(b5)
{
    float2 UVOffset;
};
struct FMaterial
{
    float3 DiffuseColor;
    float TransparencyScalar;
    float3 AmbientColor;
    float DensityScalar;
    float3 SpecularColor;
    float SpecularScalar;
    float3 EmissiveColor;
    float MaterialPad0;
};
cbuffer MaterialConstants : register(b1)
{
    FMaterial Material;
}
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
    float4 texColor = Textures.Sample(Sampler, input.UV + UVOffset);
    //건너뛰기 코드 
    texColor = float4(texColor + Material.DiffuseColor, 1.0f);
    output.color = texColor;
    return output;
}
