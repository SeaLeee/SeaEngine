// Deferred Lighting Pass Pixel Shader
// PBR Lighting using G-Buffer data

static const float PI = 3.14159265359;

cbuffer LightingConstants : register(b0)
{
    row_major float4x4 g_InvViewProjection;
    float3 g_CameraPosition;
    float g_Time;
    float3 g_LightDirection;
    float g_LightIntensity;
    float3 g_LightColor;
    float g_AmbientIntensity;
    float3 g_AmbientColor;
    float _Padding;
};

// G-Buffer textures
Texture2D<float4> g_AlbedoMetallic : register(t0);    // RGB: Albedo, A: Metallic
Texture2D<float4> g_NormalRoughness : register(t1);   // RGB: Normal, A: Roughness
Texture2D<float4> g_PositionAO : register(t2);        // RGB: Position, A: AO
Texture2D<float4> g_Emissive : register(t3);          // RGB: Emissive

SamplerState g_PointSampler : register(s0);

struct VSOutput
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
};

// PBR Functions
float DistributionGGX(float3 N, float3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    
    float nom = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    
    return nom / max(denom, 0.0001);
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    
    float nom = NdotV;
    float denom = NdotV * (1.0 - k) + k;
    
    return nom / max(denom, 0.0001);
}

float GeometrySmith(float3 N, float3 V, float3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    
    return ggx1 * ggx2;
}

float3 FresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.0 - F0) * pow(saturate(1.0 - cosTheta), 5.0);
}

float4 PSMain(VSOutput input) : SV_TARGET
{
    // Sample G-Buffer
    float4 albedoMetallic = g_AlbedoMetallic.Sample(g_PointSampler, input.TexCoord);
    float4 normalRoughness = g_NormalRoughness.Sample(g_PointSampler, input.TexCoord);
    float4 positionAO = g_PositionAO.Sample(g_PointSampler, input.TexCoord);
    float4 emissive = g_Emissive.Sample(g_PointSampler, input.TexCoord);
    
    // Extract data
    float3 albedo = albedoMetallic.rgb;
    float metallic = albedoMetallic.a;
    float3 normal = normalize(normalRoughness.rgb * 2.0 - 1.0);  // Decode from [0,1]
    float roughness = normalRoughness.a;
    float3 worldPos = positionAO.rgb;
    float ao = positionAO.a;
    
    // Early out for background (no geometry)
    if (length(worldPos) < 0.001)
    {
        return float4(0, 0, 0, 1);
    }
    
    // View direction
    float3 V = normalize(g_CameraPosition - worldPos);
    float3 L = normalize(-g_LightDirection);
    float3 H = normalize(V + L);
    
    // PBR Lighting
    float3 F0 = lerp(float3(0.04, 0.04, 0.04), albedo, metallic);
    
    // Cook-Torrance BRDF
    float NDF = DistributionGGX(normal, H, roughness);
    float G = GeometrySmith(normal, V, L, roughness);
    float3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);
    
    float3 kS = F;
    float3 kD = (1.0 - kS) * (1.0 - metallic);
    
    float3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(normal, V), 0.0) * max(dot(normal, L), 0.0) + 0.0001;
    float3 specular = numerator / denominator;
    
    // Direct lighting
    float NdotL = max(dot(normal, L), 0.0);
    float3 radiance = g_LightColor * g_LightIntensity;
    float3 directLighting = (kD * albedo / PI + specular) * radiance * NdotL;
    
    // Ambient lighting
    float3 ambient = g_AmbientColor * g_AmbientIntensity * albedo * ao;
    
    // Final color
    float3 color = directLighting + ambient + emissive.rgb;
    
    return float4(color, 1.0);
}
