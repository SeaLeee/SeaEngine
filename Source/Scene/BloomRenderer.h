#pragma once
#include "Core/Types.h"
#include "Graphics/Graphics.h"
#include <array>

namespace Sea
{
    // Bloom 设置 (Unreal 风格)
    struct BloomSettings
    {
        bool Enabled = false;
        float Intensity = 0.675f;
        float Threshold = 1.0f;
        float Radius = 1.0f;
        
        // Bloom 颜色 tint
        float TintR = 1.0f;
        float TintG = 1.0f;
        float TintB = 1.0f;
        
        // 每层 mip 权重 (6层)
        float Mip1Weight = 0.266f;  // 1/2 res
        float Mip2Weight = 0.232f;  // 1/4 res
        float Mip3Weight = 0.246f;  // 1/8 res
        float Mip4Weight = 0.384f;  // 1/16 res
        float Mip5Weight = 0.426f;  // 1/32 res
        float Mip6Weight = 0.060f;  // 1/64 res
    };

    // Bloom 常量缓冲区
    struct BloomConstants
    {
        float TexelSizeX;
        float TexelSizeY;
        float BloomThreshold;
        float BloomIntensity;
        
        float BloomRadius;
        float BloomTintR;
        float BloomTintG;
        float BloomTintB;
        
        float Bloom1Weight;
        float Bloom2Weight;
        float Bloom3Weight;
        float Bloom4Weight;
        float Bloom5Weight;
        float Bloom6Weight;
        
        float CurrentMipLevel;  // 当前 mip 级别 (用于 upsample shader)
        float IsLastMip;        // 是否是最小级别
    };

    class BloomRenderer : public NonCopyable
    {
    public:
        static constexpr u32 MIP_COUNT = 6;
        
        BloomRenderer(Device& device);
        ~BloomRenderer();

        bool Initialize(u32 width, u32 height);
        void Shutdown();
        void Resize(u32 width, u32 height);

        // 执行 Bloom 效果
        // inputSRV: 场景颜色的 SRV
        // outputRTV: 最终输出的 RTV (可以是 backbuffer 或中间 RT)
        void Render(CommandList& cmdList, 
                    D3D12_GPU_DESCRIPTOR_HANDLE inputSRV,
                    D3D12_CPU_DESCRIPTOR_HANDLE outputRTV,
                    ID3D12Resource* outputResource,
                    u32 outputWidth, u32 outputHeight);

        // 获取/设置参数
        BloomSettings& GetSettings() { return m_Settings; }
        const BloomSettings& GetSettings() const { return m_Settings; }
        void SetSettings(const BloomSettings& settings) { m_Settings = settings; }
        
        // 获取最终 Bloom 结果的 SRV（用于 Tonemapping 合成）
        D3D12_GPU_DESCRIPTOR_HANDLE GetBloomResultSRV() const { return m_UpsampleChain[0].SRV; }
        DescriptorHeap* GetSRVHeap() { return m_SRVHeap.get(); }
        
        // 获取最终 Bloom 结果的资源（用于外部创建 SRV）
        ID3D12Resource* GetBloomResultResource() const { return m_UpsampleChain[0].Resource.Get(); }

    private:
        bool CreatePipelines();
        bool CreateResources(u32 width, u32 height);
        bool CreateConstantBuffer();
        void ReleaseResources();

        void ThresholdPass(CommandList& cmdList, D3D12_GPU_DESCRIPTOR_HANDLE inputSRV);
        void DownsamplePass(CommandList& cmdList, u32 mipLevel);
        void UpsamplePass(CommandList& cmdList, u32 mipLevel);
        void CompositePass(CommandList& cmdList, 
                           D3D12_GPU_DESCRIPTOR_HANDLE sceneSRV,
                           D3D12_CPU_DESCRIPTOR_HANDLE outputRTV,
                           ID3D12Resource* outputResource,
                           u32 outputWidth, u32 outputHeight);

        Device& m_Device;
        BloomSettings m_Settings;
        
        u32 m_Width = 0;
        u32 m_Height = 0;

        // PSO
        Ref<PipelineState> m_ThresholdPSO;
        Ref<PipelineState> m_DownsamplePSO;
        Ref<PipelineState> m_UpsamplePSO;
        Ref<PipelineState> m_CompositePSO;
        Scope<RootSignature> m_RootSignature;
        
        // 常量缓冲区
        Scope<Buffer> m_ConstantBuffer;

        // Mip chain 纹理 (6 levels)
        struct MipLevel
        {
            ComPtr<ID3D12Resource> Resource;
            D3D12_CPU_DESCRIPTOR_HANDLE RTV = {};
            D3D12_GPU_DESCRIPTOR_HANDLE SRV = {};
            u32 Width = 0;
            u32 Height = 0;
        };
        std::array<MipLevel, MIP_COUNT> m_DownsampleChain;
        std::array<MipLevel, MIP_COUNT> m_UpsampleChain;

        // 描述符堆
        Scope<DescriptorHeap> m_RTVHeap;
        Scope<DescriptorHeap> m_SRVHeap;
        Scope<DescriptorHeap> m_UpsampleSRVHeap;  // 用于 Upsample 的双纹理 SRV 对
        std::array<D3D12_GPU_DESCRIPTOR_HANDLE, MIP_COUNT> m_UpsampleSRVPairs;  // 每层的 SRV 对起始位置
        
        // 采样器
        ComPtr<ID3D12Resource> m_SamplerResource;
    };
}
