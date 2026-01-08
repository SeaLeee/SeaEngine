// GBuffer Pass Vertex Shader
#include "Common.hlsli"

cbuffer ObjectConstants : register(b1)
{
    float4x4 g_WorldMatrix;
    float4x4 g_WorldInvTranspose;
}

PSInput main(VSInput input)
{
    PSInput output;
    
    float4 worldPos = mul(g_WorldMatrix, float4(input.Position, 1.0));
    output.Position = mul(g_ViewProjMatrix, worldPos);
    output.WorldPos = worldPos.xyz;
    
    output.Normal = normalize(mul((float3x3)g_WorldInvTranspose, input.Normal));
    output.Tangent = normalize(mul((float3x3)g_WorldMatrix, input.Tangent.xyz));
    output.Bitangent = cross(output.Normal, output.Tangent) * input.Tangent.w;
    
    output.TexCoord = input.TexCoord;
    
    return output;
}
