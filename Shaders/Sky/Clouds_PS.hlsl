// Advanced Volumetric Clouds Pixel Shader
// Based on GPU Pro 7 and VK_PROJ_Turbo Implementation
// Features: Perlin-Worley noise combination, Height gradients, Multi-scattering, Beer-Powder
// Reference: https://github.com/msnh2012/VK_PROJ_Turbo/blob/master/docs/VolumetricCloud.md

// ========== Constants ==========
static const float PI = 3.14159265359f;
static const float MIE_G = 0.76f;

// Cloud layer heights (meters)
static const float CLOUD_LAYER_BOTTOM = 1500.0f;
static const float CLOUD_LAYER_TOP = 4500.0f;
static const float CLOUD_LAYER_THICKNESS = CLOUD_LAYER_TOP - CLOUD_LAYER_BOTTOM;

// Rayleigh phase function
float RayleighPhase(float cosTheta)
{
    return 3.0f / (16.0f * PI) * (1.0f + cosTheta * cosTheta);
}

// Henyey-Greenstein phase function
float HenyeyGreenstein(float cosTheta, float g)
{
    float g2 = g * g;
    float denom = 1.0f + g2 - 2.0f * g * cosTheta;
    return (1.0f - g2) / (4.0f * PI * pow(max(denom, 0.0001f), 1.5f));
}

// Dual lobe phase function - realistic cloud forward/backward scattering
float DualLobePhase(float cosTheta)
{
    float forward = HenyeyGreenstein(cosTheta, 0.8f);
    float backward = HenyeyGreenstein(cosTheta, -0.3f);
    return lerp(backward, forward, 0.7f);
}

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

struct PSInput
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
    float3 ViewDir : TEXCOORD1;
};

// ========== GPU Pro 7 Hash Functions ==========
#define UI0 1597334673U
#define UI1 3812015801U
#define UI2 uint2(UI0, UI1)
#define UI3 uint3(UI0, UI1, 2798796415U)
#define UIF (1.0f / float(0xffffffffU))

// High quality hash function - fixed for large coordinates
float3 hash33(float3 p)
{
    // Use frac to avoid integer overflow with large world coordinates
    p = frac(p * 0.1031f) + frac(p * 0.1030f);
    p += dot(p, p.yxz + 33.33f);
    return frac((p.xxy + p.yxx) * p.zyx) * 2.0f - 1.0f;
}

float hash(float n)
{
    return frac(sin(n) * 43758.5453123f);
}

float3 hash3Simple(float3 p)
{
    // Safer hash that handles large coordinates
    p = frac(p * float3(0.1031f, 0.1030f, 0.0973f));
    p += dot(p, p.yxz + 33.33f);
    return frac((p.xxy + p.yxx) * p.zyx) * 2.0f - 1.0f;
}

// ========== Remap Function (Key to Perlin-Worley) ==========
float remap(float x, float xMin, float xMax, float yMin, float yMax)
{
    return yMin + (saturate((x - xMin) / (xMax - xMin)) * (yMax - yMin));
}

// ========== Gradient Noise (Perlin) ==========
float gradientNoise(float3 x, float freq)
{
    // Wrap coordinates to avoid precision issues
    float3 xWrapped = frac(x / freq) * freq;
    
    float3 p = floor(xWrapped);
    float3 w = frac(xWrapped);
    
    // Quintic interpolation (smoother than cubic hermite)
    float3 u = w * w * w * (w * (w * 6.0f - 15.0f) + 10.0f);
    
    // Get gradient vectors at cube corners
    float3 ga = hash33(p + float3(0, 0, 0));
    float3 gb = hash33(p + float3(1, 0, 0));
    float3 gc = hash33(p + float3(0, 1, 0));
    float3 gd = hash33(p + float3(1, 1, 0));
    float3 ge = hash33(p + float3(0, 0, 1));
    float3 gf = hash33(p + float3(1, 0, 1));
    float3 gg = hash33(p + float3(0, 1, 1));
    float3 gh = hash33(p + float3(1, 1, 1));
    
    // Dot products with distance vectors
    float va = dot(ga, w - float3(0, 0, 0));
    float vb = dot(gb, w - float3(1, 0, 0));
    float vc = dot(gc, w - float3(0, 1, 0));
    float vd = dot(gd, w - float3(1, 1, 0));
    float ve = dot(ge, w - float3(0, 0, 1));
    float vf = dot(gf, w - float3(1, 0, 1));
    float vg = dot(gg, w - float3(0, 1, 1));
    float vh = dot(gh, w - float3(1, 1, 1));
    
    // Trilinear interpolation
    return va + u.x * (vb - va) + u.y * (vc - va) + u.z * (ve - va)
         + u.x * u.y * (va - vb - vc + vd)
         + u.y * u.z * (va - vc - ve + vg)
         + u.z * u.x * (va - vb - ve + vf)
         + u.x * u.y * u.z * (-va + vb + vc - vd + ve - vf - vg + vh);
}

// ========== Worley Noise (Cellular) ==========
float worleyNoise(float3 uv, float freq)
{
    // Wrap coordinates for tiling
    float3 uvWrapped = frac(uv / freq) * freq;
    
    float3 id = floor(uvWrapped);
    float3 p = frac(uvWrapped);
    float minDist = 10000.0f;
    
    // Search 3x3x3 neighborhood
    for (int x = -1; x <= 1; x++)
    {
        for (int y = -1; y <= 1; y++)
        {
            for (int z = -1; z <= 1; z++)
            {
                float3 offset = float3(x, y, z);
                // Feature point in cell using hash
                float3 cellId = id + offset;
                float3 h = hash3Simple(cellId) * 0.5f + 0.5f;
                h += offset;
                float3 d = p - h;
                float dist = dot(d, d);
                minDist = min(minDist, dist);
            }
        }
    }
    
    // Return inverted: cell centers bright, edges dark
    return 1.0f - saturate(minDist);
}

// ========== FBM Functions (Fractal Brownian Motion) ==========
float perlinFBM(float3 p, float freq, int octaves)
{
    float G = exp2(-0.85f); // Hurst exponent for smoothness
    float amp = 1.0f;
    float noise = 0.0f;
    float totalAmp = 0.0f;
    
    for (int i = 0; i < octaves; i++)
    {
        noise += amp * gradientNoise(p * freq, freq);
        totalAmp += amp;
        freq *= 2.0f;
        amp *= G;
    }
    
    return noise / totalAmp;
}

float worleyFBM(float3 p, float freq)
{
    // Weighted FBM with specific octave weights (sum = 1.0)
    return worleyNoise(p * freq, freq) * 0.625f
         + worleyNoise(p * freq * 2.0f, freq * 2.0f) * 0.25f
         + worleyNoise(p * freq * 4.0f, freq * 4.0f) * 0.125f;
}

// ========== Perlin-Worley Noise Combination (GPU Pro 7 Technique) ==========
// This is the key technique: combine Perlin and Worley using remap
float4 samplePerlinWorley(float3 uv)
{
    float freq = 4.0f;
    
    // Perlin FBM for base shape (range [-1,1] -> [0,1])
    float perlin_fbm = lerp(1.0f, perlinFBM(uv, freq, 7), 0.5f);
    
    // Make Perlin more billowy (GPU Pro 7 technique)
    perlin_fbm = abs(perlin_fbm * 2.0f - 1.0f);
    
    // Worley FBM at different frequencies for detail
    float worley1 = worleyFBM(uv, freq);
    float worley2 = worleyFBM(uv, freq * 2.0f);
    float worley3 = worleyFBM(uv, freq * 4.0f);
    
    // Combine Worley channels
    float worley_fbm = worley1 * 0.625f + worley2 * 0.25f + worley3 * 0.125f;
    
    // KEY: Combine Perlin and Worley using remap
    // This creates the classic Perlin-Worley cloud texture
    float perlin_worley = remap(perlin_fbm, 0.0f, 1.0f, worley_fbm, 1.0f);
    
    return float4(perlin_worley, worley1, worley2, worley3);
}

// ========== Height Gradient Functions ==========
float GetHeightFraction(float3 pos, float cloudBottom, float cloudTop)
{
    return saturate((pos.y - cloudBottom) / (cloudTop - cloudBottom));
}

// Realistic cloud shape gradient based on cloud type
// Reference: Real-time Rendering of Volumetric Clouds (SIGGRAPH 2017)
float GetDensityHeightGradient(float heightFraction, float cloudType)
{
    // cloudType: 0 = stratus (flat), 0.5 = stratocumulus, 1 = cumulus (puffy)
    float stratusPeak = 0.15f;
    float cumulusPeak = 0.35f;
    float peakHeight = lerp(stratusPeak, cumulusPeak, cloudType);
    
    // Smooth gradient: rounded bottom, fluffy top
    float bottom = saturate(remap(heightFraction, 0.0f, peakHeight, 0.0f, 1.0f));
    float top = saturate(remap(heightFraction, peakHeight, 1.0f, 1.0f, 0.0f));
    
    // Apply smoothing curves
    bottom = pow(bottom, 0.5f);
    top = pow(top, 0.7f);
    
    return bottom * top;
}

// ========== Cloud Density Sampling (GPU Pro 7 Method) ==========
float SampleCloudDensity(float3 pos, float heightFraction)
{
    float cloudBottom = CloudHeight + CLOUD_LAYER_BOTTOM;
    float cloudTop = CloudHeight + CLOUD_LAYER_TOP;
    
    // Early exit for out of bounds
    if (heightFraction <= 0.0f || heightFraction >= 1.0f)
        return 0.0f;
    
    // Wind animation
    float3 windDir = normalize(float3(1.0f, 0.0f, 0.3f));
    float windSpeed = 20.0f;
    float3 windOffset = windDir * Time * windSpeed;
    float3 samplePos = pos + windOffset;
    
    // Sample base shape noise (low frequency)
    float3 baseUV = samplePos * 0.00015f;
    float4 noise = samplePerlinWorley(baseUV);
    float baseShape = noise.x;
    
    // Combine worley channels for detail
    float worleyFBMValue = noise.y * 0.625f + noise.z * 0.25f + noise.w * 0.125f;
    
    // Sample cloud type from low frequency noise
    float cloudType = perlinFBM(baseUV * 0.5f + float3(100, 0, 0), 2.0f, 3) * 0.5f + 0.5f;
    
    // Apply height gradient based on cloud type
    float heightGradient = GetDensityHeightGradient(heightFraction, cloudType);
    
    // Base cloud shape with remap
    float baseCloud = remap(baseShape, worleyFBMValue - 1.0f, 1.0f, 0.0f, 1.0f);
    baseCloud = saturate(baseCloud) * heightGradient;
    
    // Apply coverage with remap (key GPU Pro 7 technique)
    float coverage = CloudCoverage;
    baseCloud = remap(baseCloud, 1.0f - coverage, 1.0f, 0.0f, 1.0f);
    baseCloud = saturate(baseCloud);
    
    if (baseCloud <= 0.01f)
        return 0.0f;
    
    // Detail erosion (high frequency Worley noise)
    float3 detailUV = samplePos * 0.0008f;
    float detailWorley = worleyFBM(detailUV, 8.0f);
    float detailNoise = detailWorley * 0.35f;
    
    // Erode edges more, preserve dense cloud centers
    float detailModifier = lerp(detailNoise, 1.0f - detailNoise, saturate(heightFraction * 5.0f));
    float finalDensity = saturate(baseCloud - detailModifier * 0.25f);
    
    return finalDensity * CloudDensity * 2.0f;
}

// ========== Beer-Lambert with Powder Effect ==========
float BeerLambert(float density)
{
    return exp(-density);
}

float BeerPowder(float density, float cosTheta)
{
    float beer = BeerLambert(density);
    // Powder effect: brightening at cloud edges when looking toward light
    float powder = 1.0f - exp(-density * 2.0f);
    // More powder effect when looking toward sun
    float powderFactor = lerp(0.15f, 0.05f, saturate(cosTheta * 0.5f + 0.5f));
    return lerp(beer, beer * powder, powderFactor);
}

// ========== Light March toward Sun ==========
float LightMarch(float3 pos, float3 sunDir, float cloudBottom, float cloudTop)
{
    const int LIGHT_STEPS = 6;
    float stepSize = CLOUD_LAYER_THICKNESS / float(LIGHT_STEPS) * 0.5f;
    
    float totalDensity = 0.0f;
    float3 lightPos = pos;
    
    for (int i = 0; i < LIGHT_STEPS; i++)
    {
        lightPos += sunDir * stepSize;
        float heightFrac = GetHeightFraction(lightPos, cloudBottom, cloudTop);
        if (heightFrac > 0.0f && heightFrac < 1.0f)
        {
            totalDensity += SampleCloudDensity(lightPos, heightFrac) * stepSize * 0.003f;
        }
    }
    
    return BeerLambert(totalDensity);
}

// ========== Cloud Lighting Computation ==========
float3 ComputeCloudLighting(float3 pos, float3 viewDir, float3 sunDir, float density, float heightFrac)
{
    float cloudBottom = CloudHeight + CLOUD_LAYER_BOTTOM;
    float cloudTop = CloudHeight + CLOUD_LAYER_TOP;
    float cosTheta = dot(viewDir, sunDir);
    
    // Phase function for realistic scattering
    float phase = DualLobePhase(cosTheta);
    
    // Light attenuation towards sun
    float lightAtten = LightMarch(pos, sunDir, cloudBottom, cloudTop);
    
    // Base sun lighting
    float3 sunLight = SunColor * SunIntensity * lightAtten * phase;
    
    // Multiple scattering approximation (octaves of scattering)
    float3 multiScatter = SunColor * SunIntensity * 0.25f * lightAtten;
    
    // Ambient from sky (varies with height)
    float3 skyAmbient = lerp(
        float3(0.4f, 0.45f, 0.55f) * 0.4f,
        float3(0.6f, 0.7f, 0.9f) * 0.6f,
        heightFrac
    );
    
    // Ground bounce light
    float3 groundLight = GroundColor * 0.15f * (1.0f - heightFrac);
    
    // Silver lining effect when looking toward sun
    float silverLining = pow(saturate(1.0f - abs(cosTheta - 0.95f) * 10.0f), 2.0f);
    float3 silver = SunColor * silverLining * lightAtten * 2.0f;
    
    return sunLight + multiScatter + skyAmbient + groundLight + silver;
}

// ========== Ray-Cloud Intersection ==========
bool RayCloudIntersect(float3 rayOrigin, float3 rayDir, out float tMin, out float tMax)
{
    float cloudBottom = CloudHeight + CLOUD_LAYER_BOTTOM;
    float cloudTop = CloudHeight + CLOUD_LAYER_TOP;
    
    // Camera below cloud layer
    if (rayOrigin.y < cloudBottom)
    {
        if (rayDir.y <= 0.0f) return false;
        tMin = (cloudBottom - rayOrigin.y) / rayDir.y;
        tMax = (cloudTop - rayOrigin.y) / rayDir.y;
    }
    // Camera inside cloud layer
    else if (rayOrigin.y < cloudTop)
    {
        tMin = 0.0f;
        if (rayDir.y > 0.0f)
            tMax = (cloudTop - rayOrigin.y) / rayDir.y;
        else
            tMax = (cloudBottom - rayOrigin.y) / rayDir.y;
    }
    // Camera above cloud layer
    else
    {
        if (rayDir.y >= 0.0f) return false;
        tMin = (cloudTop - rayOrigin.y) / rayDir.y;
        tMax = (cloudBottom - rayOrigin.y) / rayDir.y;
    }
    
    // Clamp maximum distance for performance
    tMax = min(tMax, 80000.0f);
    
    return tMin < tMax && tMax > 0.0f;
}

// ========== Main Shader ==========
float4 PSMain(PSInput input) : SV_TARGET
{
    float3 viewDir = normalize(input.ViewDir);
    float3 sunDir = normalize(SunDirection);
    float cosTheta = dot(viewDir, sunDir);
    
    // ===== Atmospheric Sky =====
    float3 skyZenith = float3(0.15f, 0.4f, 0.75f);
    float3 skyHorizon = float3(0.55f, 0.7f, 0.9f);
    float3 sunsetColor = float3(1.2f, 0.5f, 0.2f);
    
    float horizon = saturate(viewDir.y);
    float horizonFade = pow(1.0f - horizon, 4.0f);
    
    // Sky gradient
    float3 skyColor = lerp(skyHorizon, skyZenith, saturate(pow(viewDir.y, 0.5f)));
    
    // Sunset coloring
    float sunHeight = sunDir.y;
    float sunsetFactor = saturate(1.0f - abs(sunHeight) * 2.5f);
    float sunAngle = saturate((cosTheta * 0.5f + 0.5f));
    float3 sunsetGlow = sunsetColor * sunsetFactor * pow(sunAngle, 3.0f) * horizonFade;
    
    // Scattering
    float rayleigh = RayleighPhase(cosTheta);
    float3 rayleighColor = skyColor * (1.0f + rayleigh * 0.2f);
    
    float mie = HenyeyGreenstein(cosTheta, MIE_G);
    float3 mieColor = SunColor * mie * 0.2f;
    
    float3 atmosphereColor = (rayleighColor + mieColor + sunsetGlow) * AtmosphereScale;
    
    // Sun disk
    float sunDisk = smoothstep(0.9995f, 0.9998f, cosTheta);
    float sunGlow = smoothstep(0.99f, 0.9995f, cosTheta) * 0.3f;
    atmosphereColor += SunColor * (sunDisk + sunGlow) * SunIntensity;
    
    // Ground
    if (viewDir.y < 0.0f)
    {
        float groundBlend = saturate(-viewDir.y * 8.0f);
        float3 groundSky = lerp(atmosphereColor, GroundColor * 0.4f, groundBlend);
        return float4(groundSky, 1.0f);
    }
    
    // ===== Volumetric Clouds =====
    if (CloudCoverage <= 0.01f)
    {
        return float4(atmosphereColor, 1.0f);
    }
    
    float3 rayOrigin = CameraPosition;
    rayOrigin.y += 100.0f;
    
    float tMin, tMax;
    if (!RayCloudIntersect(rayOrigin, viewDir, tMin, tMax))
    {
        return float4(atmosphereColor, 1.0f);
    }
    
    float cloudBottom = CloudHeight + CLOUD_LAYER_BOTTOM;
    float cloudTop = CloudHeight + CLOUD_LAYER_TOP;
    
    // Adaptive step count based on ray length
    float rayLength = tMax - tMin;
    int numSteps = 64;
    float stepSize = rayLength / float(numSteps);
    
    // Blue noise dithering to reduce banding
    float blueNoise = frac(sin(dot(input.TexCoord, float2(12.9898f, 78.233f))) * 43758.5453f);
    tMin += stepSize * blueNoise * 0.5f;
    
    float3 cloudColor = float3(0, 0, 0);
    float transmittance = 1.0f;
    float3 pos = rayOrigin + viewDir * tMin;
    
    // Zero-density optimization counter
    int zeroCount = 0;
    
    for (int i = 0; i < numSteps; i++)
    {
        if (transmittance < 0.01f) break;
        
        float heightFrac = GetHeightFraction(pos, cloudBottom, cloudTop);
        float density = SampleCloudDensity(pos, heightFrac);
        
        if (density > 0.001f)
        {
            zeroCount = 0;
            
            // Compute lighting
            float3 lighting = ComputeCloudLighting(pos, viewDir, sunDir, density, heightFrac);
            
            // Beer-Powder transmittance
            float sampleTransmittance = BeerPowder(density * stepSize * 0.01f, cosTheta);
            
            // Integrate scattering
            float3 scatteringIntegral = lighting * (1.0f - sampleTransmittance);
            cloudColor += scatteringIntegral * transmittance;
            transmittance *= sampleTransmittance;
        }
        else
        {
            zeroCount++;
            // Skip extra steps in empty space
            if (zeroCount > 4)
            {
                pos += viewDir * stepSize;
            }
        }
        
        pos += viewDir * stepSize;
    }
    
    // Blend clouds with atmosphere
    float3 finalColor = cloudColor + atmosphereColor * transmittance;
    
    // Atmospheric perspective - fade distant clouds
    float dist = length(pos - rayOrigin);
    float atmosphericFade = exp(-dist * 0.000008f);
    finalColor = lerp(atmosphereColor, finalColor, atmosphericFade);
    
    return float4(finalColor, 1.0f);
}
