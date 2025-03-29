struct VS_INPUT
{
    float4 position : POSITION;  // 12 bytes
    float2 texcoord : TEXCOORD;
};

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
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
    output.texcoord = input.texcoord;
    return output;
}
