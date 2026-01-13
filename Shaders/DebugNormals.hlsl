// Debug Normals Shader - 显示世界空间法线作为颜色

cbuffer PerFrame : register(b0)
{
    float4x4 ViewProjection;
    float4x4 View;
    float4x4 Projection;
    float3 CameraPosition;
    float Time;
    float3 LightDirection;
    float _Padding1;
    float3 LightColor;
    float LightIntensity;
    float3 AmbientColor;
    float _Padding2;
};

cbuffer PerObject : register(b1)
{
    float4x4 World;
    float4x4 WorldInvTranspose;
    float4 BaseColor;
    float Metallic;
    float Roughness;
    float AO;
    float EmissiveIntensity;
    float3 EmissiveColor;
    float NormalScale;
    uint TextureFlags;
    float3 _Padding;
};

struct VSInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 texcoord : TEXCOORD;
    float4 color : COLOR;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float3 worldNormal : NORMAL;
};

VSOutput VSMain(VSInput input)
{
    VSOutput output;
    
    float4 worldPos = mul(float4(input.position, 1.0), World);
    output.position = mul(worldPos, ViewProjection);
    
    // 变换法线到世界空间
    output.worldNormal = normalize(mul(input.normal, (float3x3)WorldInvTranspose));
    
    return output;
}

float4 PSMain(VSOutput input) : SV_TARGET
{
    // 将法线从 [-1, 1] 映射到 [0, 1] 用于可视化
    float3 normalColor = input.worldNormal * 0.5 + 0.5;
    return float4(normalColor, 1.0);
}
