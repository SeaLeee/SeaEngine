#pragma once
#include "Core/Types.h"
#include "Graphics/Device.h"
#include "Graphics/Texture.h"
#include "Graphics/Buffer.h"
#include "Graphics/PipelineState.h"
#include "Graphics/RootSignature.h"
#include "Graphics/DescriptorHeap.h"
#include "Shader/Shader.h"
#include "Scene/Mesh.h"
#include "Scene/Camera.h"
#include "Scene/OceanQuadTree.h"

#include <DirectXMath.h>

namespace Sea
{
    struct OceanParams
    {
        f32 patchSize = 1000.0f;      // 海面块大小（米）- 增大
        f32 windSpeed = 25.0f;        // 风速 (m/s) - 稍大以产生更明显的波浪
        DirectX::XMFLOAT2 windDirection = { 0.8f, 0.6f };  // 风向
        f32 amplitude = 0.0005f;      // 波浪振幅系数 - 增大
        f32 choppiness = 2.0f;        // 波浪锐度 - 增大使波浪更尖锐
        u32 resolution = 256;         // FFT分辨率 (2的幂次)
        f32 gridSize = 200.0f;        // 渲染网格大小 - 增大
        
        // Advanced rendering parameters (AAA quality)
        f32 foamIntensity = 1.5f;     // 泡沫强度 - 增大
        f32 foamScale = 0.8f;         // 泡沫纹理缩放
        f32 whitecapThreshold = 0.25f; // 白沫阈值 - 降低使更多泡沫
        f32 sssStrength = 1.2f;       // 次表面散射强度 - 增大
        f32 fogDensity = 0.0005f;     // 雾密度 - 降低
        f32 fogHeightFalloff = 0.005f; // 雾高度衰减 - 降低
        f32 sunDiskSize = 0.015f;     // 太阳盘大小 - 稍大
        f32 sunIntensity = 2.5f;      // 太阳强度 - 增大
    };

    class Ocean
    {
    public:
        Ocean(Device& device);
        ~Ocean() = default;

        bool Initialize(const OceanParams& params = OceanParams());
        
        void Update(f32 deltaTime, CommandList& cmdList);
        void Render(CommandList& cmdList, const Camera& camera);

        void SetWindSpeed(f32 speed) { m_Params.windSpeed = speed; m_NeedsSpectrumRebuild = true; }
        void SetWindDirection(f32 x, f32 z) { m_Params.windDirection = { x, z }; m_NeedsSpectrumRebuild = true; }
        void SetChoppiness(f32 chop) { m_Params.choppiness = chop; }
        void SetAmplitude(f32 amp) { m_Params.amplitude = amp; m_NeedsSpectrumRebuild = true; }
        
        const OceanParams& GetParams() const { return m_Params; }
        OceanParams& GetParams() { return m_Params; }

        // 太阳/光照参数
        void SetSunDirection(const DirectX::XMFLOAT3& dir) { m_SunDirection = dir; }
        void SetOceanColor(const DirectX::XMFLOAT4& color) { m_OceanColor = color; }
        void SetSkyColor(const DirectX::XMFLOAT4& color) { m_SkyColor = color; }
        void SetScatterColor(const DirectX::XMFLOAT4& color) { m_ScatterColor = color; }
        
        // 视图模式 (0=Lit, 1=Wireframe, 2=Normals)
        void SetViewMode(int mode) { m_ViewMode = mode; }
        int GetViewMode() const { return m_ViewMode; }
        
        // 四叉树 LOD
        void SetUseQuadTree(bool use) { m_UseQuadTree = use; }
        bool GetUseQuadTree() const { return m_UseQuadTree; }
        OceanQuadTree* GetQuadTree() const { return m_QuadTree.get(); }
        
        // 重新编译着色器
        bool RecompileShaders();

    private:
        bool CreateComputePipelines();
        bool CreateRenderPipeline();
        bool CreateQuadTreePipeline();
        bool CreateTextures();
        bool CreateOceanMesh();
        
        void RebuildSpectrum(CommandList& cmdList);
        void UpdateSpectrum(CommandList& cmdList);
        void PerformFFT(CommandList& cmdList, ID3D12Resource* input, ID3D12Resource* output);
        void GenerateNormals(CommandList& cmdList);

    private:
        Device& m_Device;
        OceanParams m_Params;
        f32 m_Time = 0.0f;
        bool m_NeedsSpectrumRebuild = true;
        bool m_Initialized = false;

        // 纹理资源
        Scope<Texture> m_H0Texture;         // 初始频谱
        Scope<Texture> m_OmegaTexture;      // 角频率
        Scope<Texture> m_HtTexture;         // 时变频谱
        Scope<Texture> m_DisplacementTexture;  // 位移贴图
        Scope<Texture> m_NormalTexture;     // 法线贴图
        
        // FFT临时纹理
        Scope<Texture> m_FFTTemp[2];        // Ping-pong缓冲
        
        // 常量缓冲
        Scope<Buffer> m_OceanCB;
        Scope<Buffer> m_FFTCB;
        
        // Render Pipeline
        Scope<RootSignature> m_RenderRootSig;
        Ref<PipelineState> m_RenderPSO;
        Ref<PipelineState> m_WireframePSO;   // Wireframe 模式
        Ref<PipelineState> m_NormalsPSO;     // Normals 可视化模式
        
        // 四叉树 LOD 渲染管线
        Scope<RootSignature> m_QuadTreeRootSig;
        Ref<PipelineState> m_QuadTreePSO;
        Ref<PipelineState> m_QuadTreeWireframePSO;
        Scope<DescriptorHeap> m_QuadTreeSRVHeap;
        
        // 海面网格
        Scope<Mesh> m_OceanMesh;
        
        // 四叉树 LOD
        Scope<OceanQuadTree> m_QuadTree;
        bool m_UseQuadTree = true;  // 默认启用四叉树 LOD
        
        // 采样器
        D3D12_CPU_DESCRIPTOR_HANDLE m_SamplerHandle;
        
        // 光照参数 - 更真实的海水颜色
        DirectX::XMFLOAT3 m_SunDirection = { -0.5f, -0.7f, -0.5f };
        DirectX::XMFLOAT4 m_OceanColor = { 0.0f, 0.05f, 0.12f, 1.0f };      // 深海蓝绿
        DirectX::XMFLOAT4 m_SkyColor = { 0.6f, 0.75f, 0.95f, 1.0f };        // 天空蓝
        DirectX::XMFLOAT4 m_ScatterColor = { 0.0f, 0.2f, 0.25f, 1.0f };     // 散射色 (SSS) - 更青绿
        
        // 视图模式
        int m_ViewMode = 0;  // 0=Lit, 1=Wireframe, 2=Normals
        
        // 描述符堆（用于Compute UAV/SRV）
        Scope<DescriptorHeap> m_UAVHeap;
        Scope<DescriptorHeap> m_SRVHeap;
    };
}
