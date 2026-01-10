// Ocean Gerstner Vertex Shader - 使用Gerstner波模拟海面
// 这是一个在GPU上直接计算波浪的简化方案，不需要FFT

cbuffer OceanCB : register(b0)
{
    row_major float4x4 g_ViewProj;
    row_major float4x4 g_World;
    float4 g_CameraPos;
    float4 g_OceanParams;   // x = patch size, y = grid size, z = time, w = choppiness
    float4 g_SunDirection;
    float4 g_OceanColor;
    float4 g_SkyColor;
};

// Gerstner 波参数
static const int NUM_WAVES = 8;

// 预定义的波浪参数 
// x = 方向x, y = 方向z, z = 波长, w = 振幅
static const float4 WaveParams[NUM_WAVES] = {
    float4(0.7, 0.7, 60.0, 2.0),     // 主浪
    float4(-0.8, 0.6, 35.0, 1.2),    // 次浪1
    float4(0.5, -0.8, 25.0, 0.8),    // 次浪2
    float4(-0.3, 0.9, 18.0, 0.5),    // 细浪1
    float4(0.9, 0.4, 12.0, 0.35),    // 细浪2
    float4(-0.6, -0.7, 8.0, 0.25),   // 细浪3
    float4(0.2, 0.95, 5.0, 0.15),    // 涟漪1
    float4(-0.9, 0.3, 3.0, 0.1)      // 涟漪2
};

// 波浪速度系数
static const float WaveSpeeds[NUM_WAVES] = {
    1.0, 1.2, 0.9, 1.1, 1.3, 1.5, 1.8, 2.0
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
    float3 worldPos : WORLDPOS;
    float3 normal : NORMAL;
    float2 texcoord : TEXCOORD;
    float foam : FOAM;
};

// Gerstner波计算
// 返回: xyz = 位移, w = 泡沫因子
float4 GerstnerWave(float4 wave, float2 pos, float time, inout float3 tangent, inout float3 binormal)
{
    float2 dir = normalize(wave.xy);
    float wavelength = wave.z;
    float amplitude = wave.w * g_OceanParams.w;  // 乘以全局振幅系数
    
    float k = 2.0 * 3.14159265 / wavelength;  // 波数
    float c = sqrt(9.81 / k);                  // 相速度
    float steepness = 0.5;                     // 陡度 (0-1)
    float f = k * (dot(dir, pos) - c * time);
    
    float a = steepness / k;
    
    float3 displacement;
    displacement.x = dir.x * a * cos(f);
    displacement.y = amplitude * sin(f);
    displacement.z = dir.y * a * cos(f);
    
    // 切线和副切线（用于法线计算）
    tangent += float3(
        -dir.x * dir.x * steepness * sin(f),
        dir.x * steepness * cos(f),
        -dir.x * dir.y * steepness * sin(f)
    );
    
    binormal += float3(
        -dir.x * dir.y * steepness * sin(f),
        dir.y * steepness * cos(f),
        -dir.y * dir.y * steepness * sin(f)
    );
    
    // 泡沫基于波峰（y位移较大时）
    float foam = saturate(displacement.y * 2.0 - 0.5);
    
    return float4(displacement, foam);
}

VSOutput VSMain(VSInput input)
{
    VSOutput output;
    
    float time = g_OceanParams.z;
    float2 worldXZ = input.position.xz;
    
    // 初始化切线和副切线
    float3 tangent = float3(1, 0, 0);
    float3 binormal = float3(0, 0, 1);
    
    // 累加所有波浪的位移
    float3 totalDisplacement = float3(0, 0, 0);
    float totalFoam = 0;
    
    [unroll]
    for (int i = 0; i < NUM_WAVES; i++)
    {
        float4 result = GerstnerWave(WaveParams[i], worldXZ, time * WaveSpeeds[i], tangent, binormal);
        totalDisplacement += result.xyz;
        totalFoam += result.w;
    }
    
    // 应用位移
    float3 displacedPos = input.position + totalDisplacement;
    
    // 计算法线
    float3 normal = normalize(cross(binormal, tangent));
    
    // 世界空间位置
    float4 worldPos = mul(float4(displacedPos, 1.0), g_World);
    output.worldPos = worldPos.xyz;
    
    // 投影
    output.position = mul(worldPos, g_ViewProj);
    
    // 法线
    output.normal = normalize(mul(normal, (float3x3)g_World));
    
    // 纹理坐标
    output.texcoord = input.texcoord;
    
    // 泡沫
    output.foam = saturate(totalFoam / float(NUM_WAVES));
    
    return output;
}
