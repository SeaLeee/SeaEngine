#pragma once

#include "Core/Types.h"
#include "RHI/RHI.h"
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>

namespace Sea
{
    // Forward declarations
    class FrameGraph;
    class FrameGraphBuilder;
    class FrameGraphPass;
    class FrameGraphResource;

    //=============================================================================
    // Resource Handle - Type-safe handle to a FrameGraph resource
    //=============================================================================
    struct FrameGraphResourceHandle
    {
        u32 id = UINT32_MAX;
        u32 version = 0;  // Version for tracking resource modifications

        bool IsValid() const { return id != UINT32_MAX; }
        bool operator==(const FrameGraphResourceHandle& other) const 
        { 
            return id == other.id && version == other.version; 
        }
        bool operator!=(const FrameGraphResourceHandle& other) const 
        { 
            return !(*this == other); 
        }
    };

    //=============================================================================
    // Resource Access Flags
    //=============================================================================
    enum class FrameGraphResourceAccess : u8
    {
        None = 0,
        Read = 1 << 0,      // Shader read (SRV)
        Write = 1 << 1,     // Render target or UAV write
        ReadWrite = Read | Write
    };
    
    inline FrameGraphResourceAccess operator|(FrameGraphResourceAccess a, FrameGraphResourceAccess b)
    {
        return static_cast<FrameGraphResourceAccess>(static_cast<u8>(a) | static_cast<u8>(b));
    }
    inline bool operator&(FrameGraphResourceAccess a, FrameGraphResourceAccess b)
    {
        return (static_cast<u8>(a) & static_cast<u8>(b)) != 0;
    }

    //=============================================================================
    // Resource Type
    //=============================================================================
    enum class FrameGraphResourceType : u8
    {
        Texture,
        Buffer,
        External     // Imported external resource
    };

    //=============================================================================
    // Texture Descriptor for FrameGraph
    //=============================================================================
    struct FrameGraphTextureDesc
    {
        u32 width = 0;
        u32 height = 0;
        u16 depth = 1;
        u16 mipLevels = 1;
        u32 sampleCount = 1;
        RHIFormat format = RHIFormat::R8G8B8A8_UNORM;
        RHITextureUsage usage = RHITextureUsage::ShaderResource;
        RHIClearValue clearValue = {};
        std::string name;

        // Use screen-relative sizing
        bool useScreenSize = false;
        f32 screenSizeScale = 1.0f;
    };

    //=============================================================================
    // Buffer Descriptor for FrameGraph  
    //=============================================================================
    struct FrameGraphBufferDesc
    {
        u64 size = 0;
        u32 stride = 0;
        bool allowUAV = false;
        std::string name;
    };

    //=============================================================================
    // FrameGraphResource - Internal resource representation
    //=============================================================================
    class FrameGraphResource
    {
    public:
        FrameGraphResource() = default;
        FrameGraphResource(u32 id, const std::string& name, FrameGraphResourceType type);

        u32 GetId() const { return m_Id; }
        u32 GetVersion() const { return m_Version; }
        const std::string& GetName() const { return m_Name; }
        FrameGraphResourceType GetType() const { return m_Type; }

        bool IsImported() const { return m_IsImported; }
        bool IsTransient() const { return !m_IsImported; }

        // Texture specific
        const FrameGraphTextureDesc& GetTextureDesc() const { return m_TextureDesc; }
        void SetTextureDesc(const FrameGraphTextureDesc& desc) { m_TextureDesc = desc; }

        // Buffer specific
        const FrameGraphBufferDesc& GetBufferDesc() const { return m_BufferDesc; }
        void SetBufferDesc(const FrameGraphBufferDesc& desc) { m_BufferDesc = desc; }

        // Physical resource (set during execution)
        void SetPhysicalTexture(RHIRenderTarget* texture) { m_PhysicalTexture = texture; }
        RHIRenderTarget* GetPhysicalTexture() const { return m_PhysicalTexture; }
        
        void SetPhysicalBuffer(RHIBuffer* buffer) { m_PhysicalBuffer = buffer; }
        RHIBuffer* GetPhysicalBuffer() const { return m_PhysicalBuffer; }

        // Lifetime tracking
        u32 GetFirstUse() const { return m_FirstUse; }
        u32 GetLastUse() const { return m_LastUse; }
        void UpdateLifetime(u32 passIndex);

        // Version management
        void IncrementVersion() { m_Version++; }
        FrameGraphResourceHandle GetHandle() const { return { m_Id, m_Version }; }

    private:
        u32 m_Id = UINT32_MAX;
        u32 m_Version = 0;
        std::string m_Name;
        FrameGraphResourceType m_Type = FrameGraphResourceType::Texture;
        bool m_IsImported = false;

        FrameGraphTextureDesc m_TextureDesc;
        FrameGraphBufferDesc m_BufferDesc;

        RHIRenderTarget* m_PhysicalTexture = nullptr;
        RHIBuffer* m_PhysicalBuffer = nullptr;

        u32 m_FirstUse = UINT32_MAX;
        u32 m_LastUse = 0;
    };

    //=============================================================================
    // Pass Type
    //=============================================================================
    enum class FrameGraphPassType : u8
    {
        Graphics,
        Compute,
        Copy,
        Present
    };

    //=============================================================================
    // Resource Binding in a Pass
    //=============================================================================
    struct FrameGraphResourceBinding
    {
        FrameGraphResourceHandle handle;
        FrameGraphResourceAccess access = FrameGraphResourceAccess::Read;
        RHIResourceState requiredState = RHIResourceState::Common;
        u32 slot = 0;  // Binding slot (for SRV/UAV/RTV index)
    };

    //=============================================================================
    // FrameGraphPass - Represents a render/compute pass
    //=============================================================================
    class FrameGraphPass
    {
    public:
        using ExecuteFunc = std::function<void(RHICommandList& cmdList, const FrameGraphPass& pass)>;

        FrameGraphPass() = default;
        FrameGraphPass(u32 id, const std::string& name, FrameGraphPassType type);

        u32 GetId() const { return m_Id; }
        const std::string& GetName() const { return m_Name; }
        FrameGraphPassType GetType() const { return m_Type; }

        // Resource bindings
        void AddInput(FrameGraphResourceHandle handle, u32 slot = 0);
        void AddOutput(FrameGraphResourceHandle handle, u32 slot = 0);
        void SetDepthStencil(FrameGraphResourceHandle handle, bool readOnly = false);

        const std::vector<FrameGraphResourceBinding>& GetInputs() const { return m_Inputs; }
        const std::vector<FrameGraphResourceBinding>& GetOutputs() const { return m_Outputs; }
        const FrameGraphResourceBinding& GetDepthStencil() const { return m_DepthStencil; }
        bool HasDepthStencil() const { return m_DepthStencil.handle.IsValid(); }

        // Execute callback
        void SetExecuteCallback(ExecuteFunc callback) { m_ExecuteCallback = std::move(callback); }
        void Execute(RHICommandList& cmdList) const;

        // Pass state
        bool IsCulled() const { return m_IsCulled; }
        void SetCulled(bool culled) { m_IsCulled = culled; }
        
        bool HasSideEffects() const { return m_HasSideEffects; }
        void SetHasSideEffects(bool hasSideEffects) { m_HasSideEffects = hasSideEffects; }

        // Reference counting for culling
        u32 GetRefCount() const { return m_RefCount; }
        void IncrementRefCount() { m_RefCount++; }
        void DecrementRefCount() { if (m_RefCount > 0) m_RefCount--; }

    private:
        u32 m_Id = UINT32_MAX;
        std::string m_Name;
        FrameGraphPassType m_Type = FrameGraphPassType::Graphics;

        std::vector<FrameGraphResourceBinding> m_Inputs;
        std::vector<FrameGraphResourceBinding> m_Outputs;
        FrameGraphResourceBinding m_DepthStencil;

        ExecuteFunc m_ExecuteCallback;
        
        bool m_IsCulled = false;
        bool m_HasSideEffects = false;
        u32 m_RefCount = 0;
    };

    //=============================================================================
    // FrameGraphBuilder - Builder pattern for constructing passes
    //=============================================================================
    class FrameGraphBuilder
    {
    public:
        FrameGraphBuilder(FrameGraph& frameGraph, FrameGraphPass& pass);

        // Create new transient resources
        FrameGraphResourceHandle CreateTexture(const FrameGraphTextureDesc& desc);
        FrameGraphResourceHandle CreateTexture(const std::string& name, const RHITextureDesc& rhiDesc);
        FrameGraphResourceHandle CreateBuffer(const FrameGraphBufferDesc& desc);
        FrameGraphResourceHandle CreateBuffer(const std::string& name, const RHIBufferDesc& rhiDesc);

        // Read resources (SRV)
        FrameGraphResourceHandle Read(FrameGraphResourceHandle input, u32 slot = 0);

        // Write resources (RTV/UAV) - returns new version of the resource
        FrameGraphResourceHandle Write(FrameGraphResourceHandle output, u32 slot = 0);

        // Read-Write (UAV)
        FrameGraphResourceHandle ReadWrite(FrameGraphResourceHandle resource, u32 slot = 0);

        // Depth stencil
        FrameGraphResourceHandle UseDepthStencil(FrameGraphResourceHandle depth, bool readOnly = false);

        // Mark pass as having side effects (won't be culled even if outputs unused)
        void SetSideEffect(bool hasSideEffect = true);

    private:
        FrameGraph& m_FrameGraph;
        FrameGraphPass& m_Pass;
    };

    //=============================================================================
    // FrameGraph - Main class for managing the render graph
    //=============================================================================
    class FrameGraph
    {
    public:
        FrameGraph();
        ~FrameGraph();

        void Initialize(RHIDevice* device);
        void Shutdown();

        // Pass creation with builder pattern - version with pass data
        template<typename PassData, typename SetupFunc, typename ExecuteFunc>
        PassData& AddPass(const std::string& name, FrameGraphPassType type,
                          SetupFunc&& setup, ExecuteFunc&& execute);

        // Pass creation with builder pattern - simple version without pass data
        template<typename SetupFunc, typename ExecuteFunc>
        FrameGraphPass& AddPassSimple(const std::string& name, FrameGraphPassType type,
                                      SetupFunc&& setup, ExecuteFunc&& execute);

        // Import external resources
        FrameGraphResourceHandle ImportTexture(const std::string& name, 
                                               RHIRenderTarget* texture,
                                               const FrameGraphTextureDesc& desc);
        FrameGraphResourceHandle ImportBuffer(const std::string& name,
                                              RHIBuffer* buffer,
                                              const FrameGraphBufferDesc& desc);

        // Get resources
        FrameGraphResource* GetResource(FrameGraphResourceHandle handle);
        const FrameGraphResource* GetResource(FrameGraphResourceHandle handle) const;

        // Compilation and execution
        bool Compile();
        void Execute(RHICommandList& cmdList);

        // Screen size for relative-sized resources
        void SetScreenSize(u32 width, u32 height);
        u32 GetScreenWidth() const { return m_ScreenWidth; }
        u32 GetScreenHeight() const { return m_ScreenHeight; }

        // Mark a resource as final output (prevents culling of its producing pass)
        void MarkOutput(FrameGraphResourceHandle handle);

        // Clear for next frame
        void Reset();

        // Friend access for builder
        friend class FrameGraphBuilder;

    private:
        // Internal resource creation
        FrameGraphResourceHandle CreateResource(const std::string& name, 
                                                FrameGraphResourceType type);
        
        // Compilation phases
        void BuildDependencies();
        void CullPasses();
        void ComputeResourceLifetimes();
        void AllocateResources();
        
        // Resource state management
        void TransitionResources(RHICommandList& cmdList, const FrameGraphPass& pass);

    private:
        RHIDevice* m_Device = nullptr;

        std::vector<std::unique_ptr<FrameGraphResource>> m_Resources;
        std::vector<std::unique_ptr<FrameGraphPass>> m_Passes;
        std::vector<u32> m_ExecutionOrder;

        // Resource aliasing/pooling
        std::unordered_map<u64, std::vector<std::unique_ptr<RHIRenderTarget>>> m_TexturePool;
        std::unordered_map<u64, std::vector<std::unique_ptr<RHIBuffer>>> m_BufferPool;

        u32 m_ScreenWidth = 1920;
        u32 m_ScreenHeight = 1080;
        u32 m_NextResourceId = 0;
        u32 m_NextPassId = 0;

        std::vector<FrameGraphResourceHandle> m_OutputResources;

        bool m_IsCompiled = false;
    };

    //=============================================================================
    // Template Implementation
    //=============================================================================
    
    // Version with pass data structure - useful for passing data between setup and execute
    template<typename PassData, typename SetupFunc, typename ExecuteFunc>
    PassData& FrameGraph::AddPass(const std::string& name, FrameGraphPassType type,
                                  SetupFunc&& setup, ExecuteFunc&& execute)
    {
        auto pass = std::make_unique<FrameGraphPass>(m_NextPassId++, name, type);
        FrameGraphPass& passRef = *pass;

        // Allocate pass data (stored in a shared_ptr to outlive the lambda)
        auto passData = std::make_shared<PassData>();
        PassData& dataRef = *passData;

        // Create builder and call setup function
        FrameGraphBuilder builder(*this, passRef);
        setup(builder, dataRef);

        // Set execute callback with captured pass data
        passRef.SetExecuteCallback([exec = std::forward<ExecuteFunc>(execute), passData]
                                   (RHICommandList& cmdList, const FrameGraphPass& p) {
            exec(cmdList, *passData);
        });

        m_Passes.push_back(std::move(pass));
        m_IsCompiled = false;

        return dataRef;
    }

    // Simple version without pass data
    template<typename SetupFunc, typename ExecuteFunc>
    FrameGraphPass& FrameGraph::AddPassSimple(const std::string& name, FrameGraphPassType type,
                                              SetupFunc&& setup, ExecuteFunc&& execute)
    {
        auto pass = std::make_unique<FrameGraphPass>(m_NextPassId++, name, type);
        FrameGraphPass& passRef = *pass;

        // Create builder and call setup function
        FrameGraphBuilder builder(*this, passRef);
        setup(builder);

        // Set execute callback
        passRef.SetExecuteCallback([exec = std::forward<ExecuteFunc>(execute)]
                                   (RHICommandList& cmdList, const FrameGraphPass& p) {
            exec(cmdList, p);
        });

        m_Passes.push_back(std::move(pass));
        m_IsCompiled = false;

        return passRef;
    }

} // namespace Sea
