// OceanFFT.h
// FFT-based Ocean Wave Simulation System
// Based on: Jerry Tessendorf's "Simulating Ocean Water"
//           GodotOceanWaves by 2Retr0
//           Christopher J. Horvath's "Empirical Directional Wave Spectra for Computer Graphics"

#pragma once
#include "Core/Types.h"
#include "Graphics/Device.h"
#include "Graphics/Texture.h"
#include "Graphics/Buffer.h"
#include "Graphics/PipelineState.h"
#include "Graphics/RootSignature.h"
#include "Graphics/DescriptorHeap.h"
#include "Graphics/CommandList.h"
#include "Scene/Mesh.h"
#include "Scene/Camera.h"

#include <DirectXMath.h>
#include <array>

namespace Sea
{
    using namespace DirectX;

    // ========================================================================
    // Wave Cascade Parameters - 每个级联的独立参数
    // ========================================================================
    struct WaveCascadeParams
    {
        // 频谱参数
        f32 tileLength = 250.0f;           // 平铺大小 (米)
        f32 windSpeed = 20.0f;             // 风速 (m/s)
        f32 windDirection = 0.0f;          // 风向 (角度)
        f32 fetchLength = 550.0f;          // 风区长度 (km)
        
        // 形状控制
        f32 swell = 0.8f;                  // 涌浪 - 波浪延长
        f32 spread = 0.2f;                 // 扩展 - 方向性
        f32 detail = 1.0f;                 // 细节 - 高频衰减
        
        // 位移/法线缩放
        f32 displacementScale = 1.0f;      // 位移缩放
        f32 normalScale = 1.0f;            // 法线强度
        
        // 泡沫参数
        f32 whitecap = 0.5f;               // 白沫阈值
        f32 foamAmount = 5.0f;             // 泡沫数量
        
        // 运行时状态
        f32 time = 0.0f;
        f32 foamGrowRate = 0.0f;
        f32 foamDecayRate = 0.0f;
        bool needsSpectrumRebuild = true;
        i32 spectrumSeedX = 0;
        i32 spectrumSeedY = 0;
    };

    // ========================================================================
    // Ocean FFT Parameters - 全局系统参数
    // ========================================================================
    struct OceanFFTParams
    {
        static constexpr u32 MAX_CASCADES = 4;
        
        u32 mapSize = 256;                 // FFT 分辨率 (2的幂次)
        u32 numCascades = 3;               // 级联数量
        f32 depth = 20.0f;                 // 水深 (米)
        f32 updatesPerSecond = 50.0f;      // 更新频率
        
        // 渲染参数
        f32 roughness = 0.4f;              // 表面粗糙度
        f32 normalStrength = 1.0f;         // 法线强度
        
        // 颜色
        XMFLOAT4 waterColor = { 0.1f, 0.15f, 0.18f, 1.0f };
        XMFLOAT4 foamColor = { 0.73f, 0.67f, 0.62f, 1.0f };
        
        // 级联参数
        std::array<WaveCascadeParams, MAX_CASCADES> cascades;
    };

    // ========================================================================
    // Compute Pipeline Push Constants
    // ========================================================================
    
    // 频谱生成 Push Constants
    struct SpectrumComputePushConstants
    {
        i32 seedX, seedY;
        f32 tileLengthX, tileLengthY;
        f32 alpha;                          // JONSWAP alpha
        f32 peakFrequency;                  // 峰值角频率
        f32 windSpeed;
        f32 windAngle;
        f32 depth;
        f32 swell;
        f32 detail;
        f32 spread;
        u32 cascadeIndex;
        f32 _padding[3];
    };

    // 频谱调制 Push Constants
    struct SpectrumModulatePushConstants
    {
        f32 tileLengthX, tileLengthY;
        f32 depth;
        f32 time;
        u32 cascadeIndex;
        f32 _padding[3];
    };

    // FFT Push Constants
    struct FFTPushConstants
    {
        u32 cascadeIndex;
        f32 _padding[3];
    };

    // Unpack Push Constants
    struct UnpackPushConstants
    {
        u32 cascadeIndex;
        f32 whitecap;
        f32 foamGrowRate;
        f32 foamDecayRate;
    };

    // ========================================================================
    // Ocean Render Constants
    // ========================================================================
    struct OceanRenderCB
    {
        XMFLOAT4X4 viewProj;
        XMFLOAT4X4 world;
        XMFLOAT3 cameraPos;
        f32 time;
        XMFLOAT3 sunDirection;
        f32 sunIntensity;
        XMFLOAT4 waterColor;
        XMFLOAT4 foamColor;
        f32 roughness;
        f32 normalStrength;
        u32 numCascades;
        f32 _padding1;
        // Map scales for each cascade: [uvScale.x, uvScale.y, dispScale, normalScale]
        XMFLOAT4 mapScales[OceanFFTParams::MAX_CASCADES];
    };

    // ========================================================================
    // OceanFFT Class - FFT-based Ocean Simulation
    // ========================================================================
    class OceanFFT
    {
    public:
        OceanFFT(Device& device);
        ~OceanFFT();

        bool Initialize(const OceanFFTParams& params = OceanFFTParams());
        void Shutdown();

        // 更新波浪模拟
        void Update(f32 deltaTime, CommandList& cmdList);
        
        // 渲染海面
        void Render(CommandList& cmdList, const Camera& camera);

        // 参数访问
        OceanFFTParams& GetParams() { return m_Params; }
        const OceanFFTParams& GetParams() const { return m_Params; }
        
        // 设置级联参数
        void SetCascadeParams(u32 index, const WaveCascadeParams& params);
        WaveCascadeParams& GetCascadeParams(u32 index);
        
        // 强制重新生成频谱
        void RebuildAllSpectra();
        
        // 获取位移/法线贴图 (用于其他系统)
        Texture* GetDisplacementMap() const { return m_DisplacementMaps.get(); }
        Texture* GetNormalMap() const { return m_NormalMaps.get(); }
        
        // 视图模式
        void SetViewMode(int mode) { m_ViewMode = mode; }
        int GetViewMode() const { return m_ViewMode; }
        
        // 太阳方向
        void SetSunDirection(const XMFLOAT3& dir) { m_SunDirection = dir; }
        const XMFLOAT3& GetSunDirection() const { return m_SunDirection; }

    private:
        // 初始化
        bool CreateComputePipelines();
        bool CreateRenderPipeline();
        bool CreateTextures();
        bool CreateBuffers();
        bool CreateMesh();
        bool CreateDescriptorHeaps();

        // Compute Pipeline 阶段
        void GenerateButterflyFactors(CommandList& cmdList);
        void UpdateCascade(CommandList& cmdList, u32 cascadeIndex, f32 delta);
        void GenerateSpectrum(CommandList& cmdList, u32 cascadeIndex);
        void ModulateSpectrum(CommandList& cmdList, u32 cascadeIndex);
        void PerformFFT(CommandList& cmdList, u32 cascadeIndex);
        void TransposeFFTBuffer(CommandList& cmdList, u32 cascadeIndex);
        void UnpackFFTResults(CommandList& cmdList, u32 cascadeIndex);

        // 工具函数
        static f32 JONSWAPAlpha(f32 windSpeed, f32 fetchLength);
        static f32 JONSWAPPeakFrequency(f32 windSpeed, f32 fetchLength);

    private:
        Device& m_Device;
        OceanFFTParams m_Params;
        bool m_Initialized = false;
        
        // 时间/更新控制
        f32 m_Time = 0.0f;
        f32 m_NextUpdateTime = 0.0f;
        u32 m_CurrentCascade = 0;  // 用于负载均衡
        bool m_ButterflyGenerated = false;
        bool m_TexturesReadyForRender = false;  // 纹理是否已转换为 SRV 状态
        
        // ========== Compute Resources ==========
        
        // 频谱纹理 (2D Array - 每层一个级联)
        Scope<Texture> m_SpectrumTexture;         // 初始频谱 h0(k)
        
        // FFT 缓冲区 (大型 Structured Buffer)
        Scope<Buffer> m_FFTBuffer;                // FFT ping-pong 缓冲
        Scope<Buffer> m_ButterflyFactors;         // 蝶形因子
        
        // 输出贴图 (2D Array - 每层一个级联)
        Scope<Texture> m_DisplacementMaps;        // 位移贴图
        Scope<Texture> m_NormalMaps;              // 法线/泡沫贴图
        
        // Compute Root Signatures
        Scope<RootSignature> m_SpectrumComputeRS;
        Scope<RootSignature> m_SpectrumModulateRS;
        Scope<RootSignature> m_FFTButterflyRS;
        Scope<RootSignature> m_FFTComputeRS;
        Scope<RootSignature> m_TransposeRS;
        Scope<RootSignature> m_UnpackRS;
        
        // Compute PSOs
        Ref<PipelineState> m_SpectrumComputePSO;
        Ref<PipelineState> m_SpectrumModulatePSO;
        Ref<PipelineState> m_FFTButterflyPSO;
        Ref<PipelineState> m_FFTComputePSO;
        Ref<PipelineState> m_TransposePSO;
        Ref<PipelineState> m_UnpackPSO;
        
        // Descriptor Heaps for Compute
        Scope<DescriptorHeap> m_ComputeUAVHeap;
        Scope<DescriptorHeap> m_ComputeSRVHeap;
        
        // Descriptor indices for UAV/SRV access
        u32 m_SpectrumUAVIndex = 0;
        u32 m_DisplacementUAVIndex = 0;
        u32 m_NormalUAVIndex = 0;
        u32 m_FFTBufferUAVIndex = 0;
        u32 m_ButterflyUAVIndex = 0;
        u32 m_UnpackUAVStartIndex = 0;  // Start of consecutive UAVs for unpack shader
        u32 m_SpectrumSRVIndex = 0;
        u32 m_FFTBufferSRVIndex = 0;
        u32 m_ButterflySRVIndex = 0;
        
        // ========== Render Resources ==========
        
        Scope<Mesh> m_OceanMesh;
        Scope<Buffer> m_RenderCB;
        
        Scope<RootSignature> m_RenderRS;
        Ref<PipelineState> m_RenderPSO;
        Ref<PipelineState> m_WireframePSO;
        
        Scope<DescriptorHeap> m_RenderSRVHeap;
        
        // 光照参数
        XMFLOAT3 m_SunDirection = { -0.5f, -0.7f, -0.5f };
        f32 m_SunIntensity = 2.5f;
        
        // 视图模式
        int m_ViewMode = 0;  // 0=Lit, 1=Wireframe
    };
}
