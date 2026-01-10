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

#include <DirectXMath.h>

namespace Sea
{
    struct OceanParams
    {
        f32 patchSize = 500.0f;       // 海面块大小（米）
        f32 windSpeed = 20.0f;        // 风速 (m/s)
        DirectX::XMFLOAT2 windDirection = { 0.8f, 0.6f };  // 风向
        f32 amplitude = 0.0002f;      // 波浪振幅系数
        f32 choppiness = 1.5f;        // 波浪锐度
        u32 resolution = 256;         // FFT分辨率 (2的幂次)
        f32 gridSize = 100.0f;        // 渲染网格大小
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

    private:
        bool CreateComputePipelines();
        bool CreateRenderPipeline();
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
        
        // 海面网格
        Scope<Mesh> m_OceanMesh;
        
        // 采样器
        D3D12_CPU_DESCRIPTOR_HANDLE m_SamplerHandle;
        
        // 光照参数
        DirectX::XMFLOAT3 m_SunDirection = { -0.5f, -0.7f, -0.5f };
        DirectX::XMFLOAT4 m_OceanColor = { 0.0f, 0.05f, 0.15f, 1.0f };
        DirectX::XMFLOAT4 m_SkyColor = { 0.5f, 0.7f, 0.9f, 1.0f };
        
        // 描述符堆（用于Compute UAV/SRV）
        Scope<DescriptorHeap> m_UAVHeap;
        Scope<DescriptorHeap> m_SRVHeap;
    };
}
