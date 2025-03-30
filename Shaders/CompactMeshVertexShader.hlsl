struct VS_INPUT
{
    float4 position : POSITION;  // 12 bytes
    float2 UV       : TEXCOORD0; // 자동으로 0.0 ~ 1.0으로 정규화된 값
};

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float2 UV       : TEXCOORD0; // 자동으로 0.0 ~ 1.0으로 정규화된 값
};
cbuffer MatrixConstants : register(b0)
{
    row_major float4x4 MVP;
};

PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT output;
    float4 localPos = input.position;
    output.position = mul(localPos, MVP);
    output.UV = input.UV * 2.0f - 1.0f;
    return output;
}
