// GBuffer Pass Vertex Shader (Standalone)
// For Deferred Rendering Pipeline

cbuffer FrameConstants : register(b0)
{
    row_major float4x4 g_ViewProjection;
    row_major float4x4 g_View;
    row_major float4x4 g_Projection;
    float3 g_CameraPosition;
    float g_Time;
};

cbuffer ObjectConstants : register(b1)
{
    row_major float4x4 g_World;
    row_major float4x4 g_WorldInvTranspose;
    float4 g_BaseColor;
    float g_Metallic;
    float g_Roughness;
    float g_AO;
    float g_EmissiveIntensity;
    float3 g_EmissiveColor;
    float _Padding;
};

struct VSInput
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float2 TexCoord : TEXCOORD;
};

struct PSInput
{
    float4 Position : SV_POSITION;
    float3 WorldPos : TEXCOORD0;
    float3 Normal : TEXCOORD1;
    float2 TexCoord : TEXCOORD2;
};

PSInput VSMain(VSInput input)
{
    PSInput output;
    
    float4 worldPos = mul(g_World, float4(input.Position, 1.0));
    output.Position = mul(g_ViewProjection, worldPos);
    output.WorldPos = worldPos.xyz;
    output.Normal = normalize(mul((float3x3)g_WorldInvTranspose, input.Normal));
    output.TexCoord = input.TexCoord;
    
    return output;
}
