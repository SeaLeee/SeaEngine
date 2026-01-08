#pragma once
#include "Graphics/GraphicsTypes.h"
#include "Core/Types.h"

namespace Sea
{
    class Device;
    class RootSignature;

    struct GraphicsPipelineDesc
    {
        RootSignature* rootSignature = nullptr;
        std::vector<u8> vertexShader, pixelShader, geometryShader, hullShader, domainShader;
        std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayout;
        PrimitiveTopology topology = PrimitiveTopology::TriangleList;
        FillMode fillMode = FillMode::Solid;
        CullMode cullMode = CullMode::Back;
        bool depthEnable = true, depthWrite = true;
        CompareFunc depthFunc = CompareFunc::Less;
        std::vector<Format> rtvFormats;
        Format dsvFormat = Format::D32_FLOAT;
    };

    struct ComputePipelineDesc
    {
        RootSignature* rootSignature = nullptr;
        std::vector<u8> computeShader;
    };

    class PipelineState : public NonCopyable
    {
    public:
        static Ref<PipelineState> CreateGraphics(Device& device, const GraphicsPipelineDesc& desc);
        static Ref<PipelineState> CreateCompute(Device& device, const ComputePipelineDesc& desc);

        ~PipelineState();
        ID3D12PipelineState* GetPipelineState() const { return m_PipelineState.Get(); }
        bool IsCompute() const { return m_IsCompute; }

    private:
        PipelineState() = default;
        ComPtr<ID3D12PipelineState> m_PipelineState;
        bool m_IsCompute = false;
    };
}
