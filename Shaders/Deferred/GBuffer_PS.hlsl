// GBuffer Pass Pixel Shader (Standalone)
// For Deferred Rendering Pipeline

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

struct PSInput
{
    float4 Position : SV_POSITION;
    float3 WorldPos : TEXCOORD0;
    float3 Normal : TEXCOORD1;
    float2 TexCoord : TEXCOORD2;
};

// G-Buffer outputs
struct GBufferOutput
{
    float4 AlbedoMetallic : SV_TARGET0;  // RGB: Albedo, A: Metallic
    float4 NormalRoughness : SV_TARGET1; // RGB: World Normal, A: Roughness
    float4 PositionAO : SV_TARGET2;      // RGB: World Position, A: AO
    float4 Emissive : SV_TARGET3;        // RGB: Emissive, A: unused
};

GBufferOutput PSMain(PSInput input)
{
    GBufferOutput output;
    
    // Albedo + Metallic
    output.AlbedoMetallic = float4(g_BaseColor.rgb, g_Metallic);
    
    // Normal (encoded to [0,1]) + Roughness
    float3 normal = normalize(input.Normal);
    output.NormalRoughness = float4(normal * 0.5 + 0.5, g_Roughness);
    
    // World Position + AO
    output.PositionAO = float4(input.WorldPos, g_AO);
    
    // Emissive
    output.Emissive = float4(g_EmissiveColor * g_EmissiveIntensity, 1.0);
    
    return output;
}
