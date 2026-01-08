#include "Graphics/PipelineState.h"
#include "Graphics/Device.h"
#include "Graphics/RootSignature.h"
#include "Core/Log.h"

namespace Sea
{
    PipelineState::~PipelineState() { m_PipelineState.Reset(); }

    Ref<PipelineState> PipelineState::CreateGraphics(Device& device, const GraphicsPipelineDesc& desc)
    {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.pRootSignature = desc.rootSignature->GetRootSignature();
        
        if (!desc.vertexShader.empty())
            psoDesc.VS = { desc.vertexShader.data(), desc.vertexShader.size() };
        if (!desc.pixelShader.empty())
            psoDesc.PS = { desc.pixelShader.data(), desc.pixelShader.size() };

        psoDesc.InputLayout = { desc.inputLayout.data(), static_cast<UINT>(desc.inputLayout.size()) };
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        
        psoDesc.RasterizerState.FillMode = static_cast<D3D12_FILL_MODE>(desc.fillMode);
        psoDesc.RasterizerState.CullMode = static_cast<D3D12_CULL_MODE>(desc.cullMode);
        psoDesc.RasterizerState.DepthClipEnable = TRUE;

        psoDesc.DepthStencilState.DepthEnable = desc.depthEnable;
        psoDesc.DepthStencilState.DepthWriteMask = desc.depthWrite ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
        psoDesc.DepthStencilState.DepthFunc = static_cast<D3D12_COMPARISON_FUNC>(desc.depthFunc);

        psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.SampleDesc = { 1, 0 };

        psoDesc.NumRenderTargets = static_cast<UINT>(desc.rtvFormats.size());
        for (size_t i = 0; i < desc.rtvFormats.size(); ++i)
            psoDesc.RTVFormats[i] = static_cast<DXGI_FORMAT>(desc.rtvFormats[i]);
        psoDesc.DSVFormat = static_cast<DXGI_FORMAT>(desc.dsvFormat);

        auto pso = Ref<PipelineState>(new PipelineState());
        if (FAILED(device.GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pso->m_PipelineState))))
        {
            SEA_CORE_ERROR("Failed to create graphics pipeline state");
            return nullptr;
        }
        return pso;
    }

    Ref<PipelineState> PipelineState::CreateCompute(Device& device, const ComputePipelineDesc& desc)
    {
        D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.pRootSignature = desc.rootSignature->GetRootSignature();
        psoDesc.CS = { desc.computeShader.data(), desc.computeShader.size() };

        auto pso = Ref<PipelineState>(new PipelineState());
        pso->m_IsCompute = true;
        if (FAILED(device.GetDevice()->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&pso->m_PipelineState))))
        {
            SEA_CORE_ERROR("Failed to create compute pipeline state");
            return nullptr;
        }
        return pso;
    }
}
