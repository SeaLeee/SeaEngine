// Ocean Vertex Shader - 海面网格顶点变换
#include "../Common.hlsli"

cbuffer OceanCB : register(b0)
{
    row_major float4x4 g_ViewProj;
    row_major float4x4 g_World;
    float4 g_CameraPos;
    float4 g_OceanParams;   // x = patch size, y = grid size, z = time, w = unused
    float4 g_SunDirection;
    float4 g_OceanColor;    // 海洋深水颜色
    float4 g_SkyColor;      // 天空颜色 (用于菲涅尔反射)
};

Texture2D<float4> g_DisplacementMap : register(t0);
Texture2D<float4> g_NormalMap : register(t1);
SamplerState g_Sampler : register(s0);

struct VSInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 texcoord : TEXCOORD;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float3 worldPos : WORLDPOS;
    float3 normal : NORMAL;
    float2 texcoord : TEXCOORD;
    float foam : FOAM;
};

VSOutput VSMain(VSInput input)
{
    VSOutput output;
    
    // 采样位移贴图
    float4 displacement = g_DisplacementMap.SampleLevel(g_Sampler, input.texcoord, 0);
    float4 normalFoam = g_NormalMap.SampleLevel(g_Sampler, input.texcoord, 0);
    
    // 应用位移
    float3 displacedPos = input.position + displacement.xyz;
    
    // 世界空间位置
    float4 worldPos = mul(float4(displacedPos, 1.0f), g_World);
    output.worldPos = worldPos.xyz;
    
    // 投影
    output.position = mul(worldPos, g_ViewProj);
    
    // 法线 (从贴图解码)
    output.normal = normalFoam.xyz * 2.0f - 1.0f;
    
    // 纹理坐标
    output.texcoord = input.texcoord;
    
    // 泡沫
    output.foam = normalFoam.w;
    
    return output;
}
