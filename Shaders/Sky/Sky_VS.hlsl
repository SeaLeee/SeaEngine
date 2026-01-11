// Sky Vertex Shader
// Renders a fullscreen quad for sky rendering

cbuffer SkyConstants : register(b0)
{
    row_major float4x4 InvViewProj;
    float3 CameraPosition;
    float Time;
    float3 SunDirection;
    float SunIntensity;
    float3 SunColor;
    float AtmosphereScale;
    float3 GroundColor;
    float CloudCoverage;
    float CloudDensity;
    float CloudHeight;
    float2 Padding;
};

struct VSOutput
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
    float3 ViewDir : TEXCOORD1;
};

// 全屏三角形
VSOutput VSMain(uint vertexID : SV_VertexID)
{
    VSOutput output;
    
    // 生成覆盖整个屏幕的三角形
    output.TexCoord = float2((vertexID << 1) & 2, vertexID & 2);
    output.Position = float4(output.TexCoord * 2.0f - 1.0f, 1.0f, 1.0f);  // Z = 1.0 (最远)
    output.Position.y = -output.Position.y;
    
    // 计算视线方向 (世界空间)
    float4 worldPos = mul(InvViewProj, float4(output.Position.xy, 1.0f, 1.0f));
    output.ViewDir = normalize(worldPos.xyz / worldPos.w - CameraPosition);
    
    return output;
}
