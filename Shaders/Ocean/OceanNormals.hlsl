// Ocean Debug Normals Shader - 显示世界空间法线作为颜色
// 专门为 Ocean 的 root signature 设计

cbuffer OceanCB : register(b0)
{
    row_major float4x4 g_ViewProj;
    row_major float4x4 g_World;
    float4 g_CameraPos;
    float4 g_OceanParams;       // x = patch size, y = grid size, z = time, w = choppiness
    float4 g_SunDirection;
    float4 g_OceanColor;
    float4 g_SkyColor;
    float4 g_ScatterColor;
    float4 g_FoamParams;
    float4 g_AtmosphereParams;
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
    
    float4 worldPos = mul(float4(input.position, 1.0), g_World);
    output.position = mul(worldPos, g_ViewProj);
    
    // 变换法线到世界空间 (假设 World 没有非均匀缩放)
    output.worldNormal = normalize(mul(float4(input.normal, 0.0), g_World).xyz);
    
    return output;
}

float4 PSMain(VSOutput input) : SV_TARGET
{
    // 将法线从 [-1, 1] 映射到 [0, 1] 用于可视化
    float3 normalColor = normalize(input.worldNormal) * 0.5 + 0.5;
    return float4(normalColor, 1.0);
}
