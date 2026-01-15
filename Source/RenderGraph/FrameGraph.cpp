#include "RenderGraph/FrameGraph.h"
#include <algorithm>
#include <queue>

namespace Sea
{
    //=============================================================================
    // FrameGraphResource Implementation
    //=============================================================================
    FrameGraphResource::FrameGraphResource(u32 id, const std::string& name, FrameGraphResourceType type)
        : m_Id(id), m_Name(name), m_Type(type)
    {
    }

    void FrameGraphResource::UpdateLifetime(u32 passIndex)
    {
        m_FirstUse = std::min(m_FirstUse, passIndex);
        m_LastUse = std::max(m_LastUse, passIndex);
    }

    //=============================================================================
    // FrameGraphPass Implementation
    //=============================================================================
    FrameGraphPass::FrameGraphPass(u32 id, const std::string& name, FrameGraphPassType type)
        : m_Id(id), m_Name(name), m_Type(type)
    {
    }

    void FrameGraphPass::AddInput(FrameGraphResourceHandle handle, u32 slot)
    {
        FrameGraphResourceBinding binding;
        binding.handle = handle;
        binding.access = FrameGraphResourceAccess::Read;
        binding.requiredState = RHIResourceState::ShaderResource;
        binding.slot = slot;
        m_Inputs.push_back(binding);
    }

    void FrameGraphPass::AddOutput(FrameGraphResourceHandle handle, u32 slot)
    {
        FrameGraphResourceBinding binding;
        binding.handle = handle;
        binding.access = FrameGraphResourceAccess::Write;
        binding.requiredState = RHIResourceState::RenderTarget;
        binding.slot = slot;
        m_Outputs.push_back(binding);
    }

    void FrameGraphPass::SetDepthStencil(FrameGraphResourceHandle handle, bool readOnly)
    {
        m_DepthStencil.handle = handle;
        m_DepthStencil.access = readOnly ? FrameGraphResourceAccess::Read 
                                         : FrameGraphResourceAccess::ReadWrite;
        m_DepthStencil.requiredState = readOnly ? RHIResourceState::DepthRead 
                                                : RHIResourceState::DepthWrite;
        m_DepthStencil.slot = 0;
    }

    void FrameGraphPass::Execute(RHICommandList& cmdList) const
    {
        if (m_ExecuteCallback && !m_IsCulled)
        {
            m_ExecuteCallback(cmdList, *this);
        }
    }

    //=============================================================================
    // FrameGraphBuilder Implementation
    //=============================================================================
    FrameGraphBuilder::FrameGraphBuilder(FrameGraph& frameGraph, FrameGraphPass& pass)
        : m_FrameGraph(frameGraph), m_Pass(pass)
    {
    }

    FrameGraphResourceHandle FrameGraphBuilder::CreateTexture(const FrameGraphTextureDesc& desc)
    {
        auto handle = m_FrameGraph.CreateResource(desc.name, FrameGraphResourceType::Texture);
        auto* resource = m_FrameGraph.GetResource(handle);
        if (resource)
        {
            resource->SetTextureDesc(desc);
        }
        return handle;
    }

    FrameGraphResourceHandle FrameGraphBuilder::CreateTexture(const std::string& name, const RHITextureDesc& rhiDesc)
    {
        // Convert RHITextureDesc to FrameGraphTextureDesc
        FrameGraphTextureDesc fgDesc;
        fgDesc.name = name.empty() ? rhiDesc.name : name;
        fgDesc.width = rhiDesc.width;
        fgDesc.height = rhiDesc.height;
        fgDesc.depth = rhiDesc.depth;
        fgDesc.mipLevels = rhiDesc.mipLevels;
        fgDesc.sampleCount = rhiDesc.sampleCount;
        fgDesc.format = rhiDesc.format;
        fgDesc.usage = rhiDesc.usage;
        fgDesc.clearValue = rhiDesc.clearValue;
        
        return CreateTexture(fgDesc);
    }

    FrameGraphResourceHandle FrameGraphBuilder::CreateBuffer(const FrameGraphBufferDesc& desc)
    {
        auto handle = m_FrameGraph.CreateResource(desc.name, FrameGraphResourceType::Buffer);
        auto* resource = m_FrameGraph.GetResource(handle);
        if (resource)
        {
            resource->SetBufferDesc(desc);
        }
        return handle;
    }

    FrameGraphResourceHandle FrameGraphBuilder::CreateBuffer(const std::string& name, const RHIBufferDesc& rhiDesc)
    {
        // Convert RHIBufferDesc to FrameGraphBufferDesc
        FrameGraphBufferDesc fgDesc;
        fgDesc.name = name.empty() ? rhiDesc.name : name;
        fgDesc.size = rhiDesc.size;
        fgDesc.stride = rhiDesc.structureByteStride;
        fgDesc.allowUAV = rhiDesc.allowUAV;
        
        return CreateBuffer(fgDesc);
    }

    FrameGraphResourceHandle FrameGraphBuilder::Read(FrameGraphResourceHandle input, u32 slot)
    {
        m_Pass.AddInput(input, slot);
        return input;
    }

    FrameGraphResourceHandle FrameGraphBuilder::Write(FrameGraphResourceHandle output, u32 slot)
    {
        // Writing creates a new version of the resource
        auto* resource = m_FrameGraph.GetResource(output);
        if (resource)
        {
            resource->IncrementVersion();
            auto newHandle = resource->GetHandle();
            m_Pass.AddOutput(newHandle, slot);
            return newHandle;
        }
        m_Pass.AddOutput(output, slot);
        return output;
    }

    FrameGraphResourceHandle FrameGraphBuilder::ReadWrite(FrameGraphResourceHandle resource, u32 slot)
    {
        // Read-write: add as both input and output
        m_Pass.AddInput(resource, slot);
        
        auto* res = m_FrameGraph.GetResource(resource);
        if (res)
        {
            res->IncrementVersion();
            auto newHandle = res->GetHandle();
            
            FrameGraphResourceBinding binding;
            binding.handle = newHandle;
            binding.access = FrameGraphResourceAccess::ReadWrite;
            binding.requiredState = RHIResourceState::UnorderedAccess;
            binding.slot = slot;
            // Add to outputs with UAV state
            m_Pass.AddOutput(newHandle, slot);
            return newHandle;
        }
        return resource;
    }

    FrameGraphResourceHandle FrameGraphBuilder::UseDepthStencil(FrameGraphResourceHandle depth, bool readOnly)
    {
        m_Pass.SetDepthStencil(depth, readOnly);
        
        if (!readOnly)
        {
            auto* resource = m_FrameGraph.GetResource(depth);
            if (resource)
            {
                resource->IncrementVersion();
                return resource->GetHandle();
            }
        }
        return depth;
    }

    void FrameGraphBuilder::SetSideEffect(bool hasSideEffect)
    {
        m_Pass.SetHasSideEffects(hasSideEffect);
    }

    //=============================================================================
    // FrameGraph Implementation
    //=============================================================================
    FrameGraph::FrameGraph() = default;
    FrameGraph::~FrameGraph() { Shutdown(); }

    void FrameGraph::Initialize(RHIDevice* device)
    {
        m_Device = device;
    }

    void FrameGraph::Shutdown()
    {
        Reset();
        m_TexturePool.clear();
        m_BufferPool.clear();
        m_Device = nullptr;
    }

    FrameGraphResourceHandle FrameGraph::ImportTexture(const std::string& name,
                                                        RHIRenderTarget* texture,
                                                        const FrameGraphTextureDesc& desc)
    {
        auto handle = CreateResource(name, FrameGraphResourceType::External);
        auto* resource = GetResource(handle);
        if (resource)
        {
            resource->SetTextureDesc(desc);
            resource->SetPhysicalTexture(texture);
        }
        return handle;
    }

    FrameGraphResourceHandle FrameGraph::ImportBuffer(const std::string& name,
                                                       RHIBuffer* buffer,
                                                       const FrameGraphBufferDesc& desc)
    {
        auto handle = CreateResource(name, FrameGraphResourceType::External);
        auto* resource = GetResource(handle);
        if (resource)
        {
            resource->SetBufferDesc(desc);
            resource->SetPhysicalBuffer(buffer);
        }
        return handle;
    }

    FrameGraphResourceHandle FrameGraph::CreateResource(const std::string& name,
                                                         FrameGraphResourceType type)
    {
        auto resource = std::make_unique<FrameGraphResource>(m_NextResourceId++, name, type);
        FrameGraphResourceHandle handle = resource->GetHandle();
        m_Resources.push_back(std::move(resource));
        m_IsCompiled = false;
        return handle;
    }

    FrameGraphResource* FrameGraph::GetResource(FrameGraphResourceHandle handle)
    {
        if (!handle.IsValid() || handle.id >= m_Resources.size())
            return nullptr;
        return m_Resources[handle.id].get();
    }

    const FrameGraphResource* FrameGraph::GetResource(FrameGraphResourceHandle handle) const
    {
        if (!handle.IsValid() || handle.id >= m_Resources.size())
            return nullptr;
        return m_Resources[handle.id].get();
    }

    bool FrameGraph::Compile()
    {
        if (m_IsCompiled)
            return true;

        // Phase 1: Build dependencies
        BuildDependencies();

        // Phase 2: Cull unused passes
        CullPasses();

        // Phase 3: Compute resource lifetimes
        ComputeResourceLifetimes();

        // Phase 4: Allocate physical resources
        AllocateResources();

        m_IsCompiled = true;
        return true;
    }

    void FrameGraph::BuildDependencies()
    {
        m_ExecutionOrder.clear();

        // Simple topological sort based on pass order
        // In a more complex system, we'd build a proper dependency graph
        for (size_t i = 0; i < m_Passes.size(); ++i)
        {
            m_ExecutionOrder.push_back(static_cast<u32>(i));
        }
    }

    void FrameGraph::CullPasses()
    {
        // Mark all passes as potentially culled
        for (auto& pass : m_Passes)
        {
            pass->SetCulled(!pass->HasSideEffects());
        }

        // Mark passes that produce output resources as having side effects
        for (const auto& outputHandle : m_OutputResources)
        {
            for (auto& pass : m_Passes)
            {
                for (const auto& output : pass->GetOutputs())
                {
                    if (output.handle.id == outputHandle.id)
                    {
                        pass->SetCulled(false);
                    }
                }
            }
        }

        // Work backwards from passes with side effects
        // Any pass that produces resources used by non-culled passes is kept
        bool changed = true;
        while (changed)
        {
            changed = false;
            for (auto it = m_Passes.rbegin(); it != m_Passes.rend(); ++it)
            {
                auto& pass = *it;
                if (!pass->IsCulled())
                {
                    // Keep all passes that produce our inputs
                    for (const auto& input : pass->GetInputs())
                    {
                        // Find which pass produces this resource version
                        for (auto& producerPass : m_Passes)
                        {
                            if (producerPass->IsCulled())
                            {
                                for (const auto& output : producerPass->GetOutputs())
                                {
                                    if (output.handle.id == input.handle.id)
                                    {
                                        producerPass->SetCulled(false);
                                        changed = true;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        // Remove culled passes from execution order
        m_ExecutionOrder.erase(
            std::remove_if(m_ExecutionOrder.begin(), m_ExecutionOrder.end(),
                [this](u32 idx) { return m_Passes[idx]->IsCulled(); }),
            m_ExecutionOrder.end());
    }

    void FrameGraph::ComputeResourceLifetimes()
    {
        // Reset lifetimes
        for (auto& resource : m_Resources)
        {
            // External resources have infinite lifetime
            if (resource->GetType() == FrameGraphResourceType::External)
            {
                resource->UpdateLifetime(0);
                resource->UpdateLifetime(static_cast<u32>(m_ExecutionOrder.size()));
            }
        }

        // Update lifetimes based on pass usage
        for (size_t execIdx = 0; execIdx < m_ExecutionOrder.size(); ++execIdx)
        {
            u32 passIdx = m_ExecutionOrder[execIdx];
            const auto& pass = m_Passes[passIdx];

            for (const auto& input : pass->GetInputs())
            {
                auto* resource = GetResource(input.handle);
                if (resource)
                {
                    resource->UpdateLifetime(static_cast<u32>(execIdx));
                }
            }

            for (const auto& output : pass->GetOutputs())
            {
                auto* resource = GetResource(output.handle);
                if (resource)
                {
                    resource->UpdateLifetime(static_cast<u32>(execIdx));
                }
            }

            if (pass->HasDepthStencil())
            {
                auto* resource = GetResource(pass->GetDepthStencil().handle);
                if (resource)
                {
                    resource->UpdateLifetime(static_cast<u32>(execIdx));
                }
            }
        }
    }

    void FrameGraph::AllocateResources()
    {
        if (!m_Device)
            return;

        for (auto& resource : m_Resources)
        {
            // Skip external resources (already have physical backing)
            if (resource->GetType() == FrameGraphResourceType::External)
                continue;

            // Skip resources with no usage
            if (resource->GetFirstUse() == UINT32_MAX)
                continue;

            if (resource->GetType() == FrameGraphResourceType::Texture)
            {
                auto desc = resource->GetTextureDesc();
                
                // Apply screen-relative sizing
                if (desc.useScreenSize)
                {
                    desc.width = static_cast<u32>(m_ScreenWidth * desc.screenSizeScale);
                    desc.height = static_cast<u32>(m_ScreenHeight * desc.screenSizeScale);
                }

                // Create RHI texture desc
                RHITextureDesc rhiDesc;
                rhiDesc.width = desc.width;
                rhiDesc.height = desc.height;
                rhiDesc.depth = desc.depth;
                rhiDesc.mipLevels = desc.mipLevels;
                rhiDesc.sampleCount = desc.sampleCount;
                rhiDesc.format = desc.format;
                rhiDesc.usage = desc.usage;
                rhiDesc.clearValue = desc.clearValue;
                rhiDesc.name = desc.name;

                // TODO: Implement resource pooling/aliasing
                auto texture = m_Device->CreateRenderTarget(rhiDesc);
                if (texture)
                {
                    resource->SetPhysicalTexture(texture.get());
                    
                    // Store in pool for lifetime management
                    u64 poolKey = resource->GetId();
                    m_TexturePool[poolKey].push_back(std::move(texture));
                }
            }
            else if (resource->GetType() == FrameGraphResourceType::Buffer)
            {
                auto desc = resource->GetBufferDesc();

                RHIBufferDesc rhiDesc;
                rhiDesc.size = desc.size;
                rhiDesc.usage = RHIBufferUsage::Default;
                rhiDesc.allowUAV = desc.allowUAV;
                rhiDesc.name = desc.name;

                auto buffer = m_Device->CreateBuffer(rhiDesc);
                if (buffer)
                {
                    resource->SetPhysicalBuffer(buffer.get());
                    
                    u64 poolKey = resource->GetId();
                    m_BufferPool[poolKey].push_back(std::move(buffer));
                }
            }
        }
    }

    void FrameGraph::Execute(RHICommandList& cmdList)
    {
        if (!m_IsCompiled)
        {
            if (!Compile())
                return;
        }

        // Execute passes in order
        for (u32 execIdx : m_ExecutionOrder)
        {
            const auto& pass = m_Passes[execIdx];
            
            if (pass->IsCulled())
                continue;

            // Begin debug event
            cmdList.BeginEvent(pass->GetName().c_str());

            // Transition resources to required states
            TransitionResources(cmdList, *pass);

            // Execute pass
            pass->Execute(cmdList);

            // End debug event
            cmdList.EndEvent();
        }
    }

    void FrameGraph::TransitionResources(RHICommandList& cmdList, const FrameGraphPass& pass)
    {
        // Transition inputs to shader resource state
        for (const auto& input : pass.GetInputs())
        {
            auto* resource = GetResource(input.handle);
            if (resource && resource->GetPhysicalTexture())
            {
                // TODO: Track current state and only transition if needed
                // For now, assume resources need transition
            }
        }

        // Transition outputs to render target state
        for (const auto& output : pass.GetOutputs())
        {
            auto* resource = GetResource(output.handle);
            if (resource && resource->GetPhysicalTexture())
            {
                // TODO: Track current state and only transition if needed
            }
        }

        // Transition depth stencil
        if (pass.HasDepthStencil())
        {
            auto* resource = GetResource(pass.GetDepthStencil().handle);
            if (resource && resource->GetPhysicalTexture())
            {
                // TODO: Track current state and only transition if needed
            }
        }

        // Flush all barriers
        cmdList.FlushBarriers();
    }

    void FrameGraph::SetScreenSize(u32 width, u32 height)
    {
        if (m_ScreenWidth != width || m_ScreenHeight != height)
        {
            m_ScreenWidth = width;
            m_ScreenHeight = height;
            m_IsCompiled = false;  // Need to reallocate screen-relative resources
        }
    }

    void FrameGraph::MarkOutput(FrameGraphResourceHandle handle)
    {
        if (handle.IsValid())
        {
            m_OutputResources.push_back(handle);
        }
    }

    void FrameGraph::Reset()
    {
        m_Resources.clear();
        m_Passes.clear();
        m_ExecutionOrder.clear();
        m_OutputResources.clear();
        m_NextResourceId = 0;
        m_NextPassId = 0;
        m_IsCompiled = false;

        // Note: We keep the resource pools for reuse next frame
    }

} // namespace Sea
