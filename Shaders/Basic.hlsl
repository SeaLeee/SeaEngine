// Simple 3D rendering shaders
// Basic.hlsl

cbuffer PerFrameData : register(b0)
{
    row_major float4x4 ViewProjection;
    row_major float4x4 View;
    row_major float4x4 Projection;
    float3 CameraPosition;
    float Time;
    float3 LightDirection;
    float _Padding1;
    float3 LightColor;
    float LightIntensity;
    float3 AmbientColor;
    float _Padding2;
};

cbuffer PerObjectData : register(b1)
{
    row_major float4x4 World;
    row_major float4x4 WorldInvTranspose;
    float4 BaseColor;
    float Metallic;
    float Roughness;
    float AO;
    float EmissiveIntensity;
    float3 EmissiveColor;
    float NormalScale;
    uint TextureFlags;
    float3 _Padding3;
};

struct VSInput
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float2 TexCoord : TEXCOORD0;
    float4 Color : COLOR0;
};

struct PSInput
{
    float4 Position : SV_POSITION;
    float3 WorldPos : TEXCOORD0;
    float3 Normal : TEXCOORD1;
    float2 TexCoord : TEXCOORD2;
    float4 Color : COLOR0;
};

PSInput VSMain(VSInput input)
{
    PSInput output;
    
    float4 worldPos = mul(float4(input.Position, 1.0), World);
    output.WorldPos = worldPos.xyz;
    output.Position = mul(worldPos, ViewProjection);
    output.Normal = normalize(mul(float4(input.Normal, 0.0), WorldInvTranspose).xyz);
    output.TexCoord = input.TexCoord;
    output.Color = input.Color * BaseColor;
    
    return output;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    float3 N = normalize(input.Normal);
    float3 L = normalize(-LightDirection);
    float3 V = normalize(CameraPosition - input.WorldPos);
    float3 H = normalize(L + V);
    
    // Diffuse
    float NdotL = max(dot(N, L), 0.0);
    float3 diffuse = input.Color.rgb * NdotL * LightColor * LightIntensity;
    
    // Specular (Blinn-Phong)
    float NdotH = max(dot(N, H), 0.0);
    float shininess = lerp(8.0, 256.0, 1.0 - Roughness);
    float spec = pow(NdotH, shininess);
    float3 specular = LightColor * spec * Metallic * LightIntensity;
    
    // Ambient
    float3 ambient = input.Color.rgb * AmbientColor;
    
    // Fresnel rim
    float fresnel = pow(1.0 - max(dot(N, V), 0.0), 3.0);
    float3 rim = fresnel * 0.2 * LightColor;
    
    float3 finalColor = ambient + diffuse + specular + rim;
    
    // Simple tone mapping
    finalColor = finalColor / (finalColor + 1.0);
    
    // Gamma correction
    finalColor = pow(finalColor, 1.0 / 2.2);
    
    return float4(finalColor, input.Color.a);
}
