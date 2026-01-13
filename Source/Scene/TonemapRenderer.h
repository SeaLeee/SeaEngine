#pragma once
#include "Core/Types.h"
#include "Graphics/Graphics.h"

namespace Sea
{
    // Tonemapping 设置
    struct TonemapSettings
    {
        bool Enabled = true;
        int Operator = 0;           // 0: ACES, 1: Reinhard, 2: Uncharted2, 3: None
        float Exposure = 1.0f;
        float Gamma = 2.2f;
        
        // Bloom 合成设置
        bool BloomEnabled = false;
        float BloomIntensity = 1.0f;
        float BloomTintR = 1.0f;
        float BloomTintG = 1.0f;
        float BloomTintB = 1.0f;
    };

    // Tonemap 常量缓冲区
    struct TonemapConstants
    {
        float Exposure;
        float Gamma;
        int TonemapOperator;
        float BloomIntensity;
        
        float BloomTintR;
        float BloomTintG;
        float BloomTintB;
        float BloomEnabled;
    };

    class TonemapRenderer : public NonCopyable
    {
    public:
        TonemapRenderer(Device& device);
        ~TonemapRenderer();

        bool Initialize();
        void Shutdown();

        // 执行 Tonemapping
        // inputSRV: HDR 场景颜色的 SRV (GPU handle)
        // bloomSRV: Bloom 结果的 SRV (可选，如果不需要 bloom 可传 null handle)
        // outputRTV: LDR 输出的 RTV
        void Render(CommandList& cmdList,
                    D3D12_GPU_DESCRIPTOR_HANDLE inputSRV,
                    D3D12_GPU_DESCRIPTOR_HANDLE bloomSRV,
                    D3D12_CPU_DESCRIPTOR_HANDLE outputRTV,
                    ID3D12Resource* outputResource,
                    u32 outputWidth, u32 outputHeight);

        // 获取/设置参数
        TonemapSettings& GetSettings() { return m_Settings; }
        const TonemapSettings& GetSettings() const { return m_Settings; }
        void SetSettings(const TonemapSettings& settings) { m_Settings = settings; }

        // 获取描述符堆供外部绑定使用
        DescriptorHeap* GetSRVHeap() { return m_SRVHeap.get(); }

    private:
        bool CreatePipeline();
        bool CreateConstantBuffer();

        Device& m_Device;
        TonemapSettings m_Settings;

        // PSO
        Ref<PipelineState> m_PSO;
        Scope<RootSignature> m_RootSignature;
        
        // 常量缓冲区
        Scope<Buffer> m_ConstantBuffer;

        // SRV 描述符堆（用于外部 SRV）
        Scope<DescriptorHeap> m_SRVHeap;
    };
}
