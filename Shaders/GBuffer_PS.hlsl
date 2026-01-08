// GBuffer Pass Pixel Shader
#include "Common.hlsli"

// Material textures
Texture2D<float4> g_AlbedoMap : register(t0);
Texture2D<float3> g_NormalMap : register(t1);
Texture2D<float> g_RoughnessMap : register(t2);
Texture2D<float> g_MetallicMap : register(t3);
SamplerState g_Sampler : register(s0);

// GBuffer outputs
struct GBufferOutput
{
    float4 Albedo : SV_TARGET0;      // RGB: Albedo, A: unused
    float4 Normal : SV_TARGET1;      // RGB: World Normal (encoded), A: unused
    float4 Material : SV_TARGET2;    // R: Roughness, G: Metallic, B: AO, A: unused
};

GBufferOutput main(PSInput input)
{
    GBufferOutput output;
    
    // Sample textures
    float4 albedo = g_AlbedoMap.Sample(g_Sampler, input.TexCoord);
    float3 normalMap = g_NormalMap.Sample(g_Sampler, input.TexCoord);
    float roughness = g_RoughnessMap.Sample(g_Sampler, input.TexCoord);
    float metallic = g_MetallicMap.Sample(g_Sampler, input.TexCoord);
    
    // Transform normal to world space
    float3 normal = DecodeNormal(normalMap.xy);
    float3 worldNormal = TransformNormalToWorld(normal, input.Normal, input.Tangent, input.Bitangent);
    
    // Write to GBuffer
    output.Albedo = float4(albedo.rgb, 1.0);
    output.Normal = float4(worldNormal * 0.5 + 0.5, 1.0);  // Encode to [0,1]
    output.Material = float4(roughness, metallic, 1.0, 1.0);
    
    return output;
}
