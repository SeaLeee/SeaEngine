// Grid shader for ground visualization
// Grid.hlsl

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
    float2 TexCoord : TEXCOORD1;
};

PSInput VSMain(VSInput input)
{
    PSInput output;
    output.WorldPos = input.Position;
    output.Position = mul(float4(input.Position, 1.0), ViewProjection);
    output.TexCoord = input.Position.xz;
    return output;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    float2 coord = input.WorldPos.xz;
    
    // Grid lines
    float2 grid = abs(frac(coord - 0.5) - 0.5) / fwidth(coord);
    float gridLine = min(grid.x, grid.y);
    
    // Major grid lines every 5 units
    float2 majorGrid = abs(frac(coord / 5.0 - 0.5) - 0.5) / fwidth(coord / 5.0);
    float majorLine = min(majorGrid.x, majorGrid.y);
    
    // Axis lines
    float axisWidth = 0.05;
    float xAxis = smoothstep(axisWidth, 0.0, abs(input.WorldPos.z));
    float zAxis = smoothstep(axisWidth, 0.0, abs(input.WorldPos.x));
    
    // Colors
    float3 gridColor = float3(0.3, 0.3, 0.3);
    float3 majorGridColor = float3(0.5, 0.5, 0.5);
    float3 xAxisColor = float3(1.0, 0.2, 0.2);
    float3 zAxisColor = float3(0.2, 0.2, 1.0);
    
    // Combine
    float3 color = lerp(float3(0.15, 0.15, 0.18), gridColor, 1.0 - min(gridLine, 1.0) * 0.5);
    color = lerp(color, majorGridColor, 1.0 - min(majorLine, 1.0));
    color = lerp(color, xAxisColor, xAxis);
    color = lerp(color, zAxisColor, zAxis);
    
    // Distance fade
    float dist = length(input.WorldPos.xz - CameraPosition.xz);
    float fade = 1.0 - smoothstep(20.0, 50.0, dist);
    
    return float4(color, fade * 0.8);
}
