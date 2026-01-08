// Deferred Lighting Pass Pixel Shader
#include "Common.hlsli"

// GBuffer inputs
Texture2D<float4> g_AlbedoBuffer : register(t0);
Texture2D<float4> g_NormalBuffer : register(t1);
Texture2D<float4> g_MaterialBuffer : register(t2);
Texture2D<float> g_DepthBuffer : register(t3);
SamplerState g_PointSampler : register(s0);

// Light data
cbuffer LightConstants : register(b1)
{
    float3 g_LightDirection;
    float g_LightIntensity;
    float3 g_LightColor;
    float g_AmbientIntensity;
}

// PBR functions
float DistributionGGX(float3 N, float3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    
    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    
    return num / max(denom, EPSILON);
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float GeometrySmith(float3 N, float3 V, float3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    return GeometrySchlickGGX(NdotV, roughness) * GeometrySchlickGGX(NdotL, roughness);
}

float3 FresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.0 - F0) * pow(saturate(1.0 - cosTheta), 5.0);
}

struct VSOutput
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
};

float4 main(VSOutput input) : SV_TARGET
{
    // Sample GBuffer
    float4 albedoSample = g_AlbedoBuffer.Sample(g_PointSampler, input.TexCoord);
    float4 normalSample = g_NormalBuffer.Sample(g_PointSampler, input.TexCoord);
    float4 materialSample = g_MaterialBuffer.Sample(g_PointSampler, input.TexCoord);
    float depth = g_DepthBuffer.Sample(g_PointSampler, input.TexCoord);
    
    // Decode
    float3 albedo = albedoSample.rgb;
    float3 N = normalize(normalSample.rgb * 2.0 - 1.0);
    float roughness = materialSample.r;
    float metallic = materialSample.g;
    
    // Reconstruct world position
    float3 worldPos = ReconstructWorldPosition(input.TexCoord, depth);
    float3 V = normalize(g_CameraPosition - worldPos);
    float3 L = normalize(-g_LightDirection);
    float3 H = normalize(V + L);
    
    // Calculate F0 (reflectance at normal incidence)
    float3 F0 = lerp(float3(0.04, 0.04, 0.04), albedo, metallic);
    
    // Cook-Torrance BRDF
    float D = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, roughness);
    float3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);
    
    float3 numerator = D * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + EPSILON;
    float3 specular = numerator / denominator;
    
    // Energy conservation
    float3 kS = F;
    float3 kD = (1.0 - kS) * (1.0 - metallic);
    
    // Final lighting
    float NdotL = max(dot(N, L), 0.0);
    float3 radiance = g_LightColor * g_LightIntensity;
    float3 Lo = (kD * albedo * INV_PI + specular) * radiance * NdotL;
    
    // Ambient
    float3 ambient = albedo * g_AmbientIntensity;
    
    float3 color = ambient + Lo;
    
    return float4(color, 1.0);
}
