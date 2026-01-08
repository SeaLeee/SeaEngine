// Simple Compute Shader Example
// Performs a simple blur operation

Texture2D<float4> g_Input : register(t0);
RWTexture2D<float4> g_Output : register(u0);

cbuffer BlurConstants : register(b0)
{
    float2 g_TexelSize;
    float g_BlurRadius;
    float g_Padding;
}

[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    float4 color = float4(0, 0, 0, 0);
    float totalWeight = 0;
    
    int radius = (int)g_BlurRadius;
    
    for (int y = -radius; y <= radius; ++y)
    {
        for (int x = -radius; x <= radius; ++x)
        {
            int2 samplePos = DTid.xy + int2(x, y);
            float weight = 1.0 / (1.0 + abs(x) + abs(y));
            color += g_Input[samplePos] * weight;
            totalWeight += weight;
        }
    }
    
    g_Output[DTid.xy] = color / totalWeight;
}
