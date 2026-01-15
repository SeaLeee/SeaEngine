#pragma once

#include "RHI/RHITypes.h"
#include <string>

namespace Sea
{
    //=============================================================================
    // RHIResource - Base class for all RHI resources
    //=============================================================================
    class RHIResource
    {
    public:
        virtual ~RHIResource() = default;
        
        //! Get resource debug name
        const std::string& GetName() const { return m_Name; }
        
        //! Set resource debug name
        void SetName(const std::string& name) { m_Name = name; OnNameChanged(); }
        
        //! Check if resource is valid
        virtual bool IsValid() const = 0;

    protected:
        virtual void OnNameChanged() {}
        
        std::string m_Name;
    };

    //=============================================================================
    // RHIBuffer - Abstract buffer resource
    //=============================================================================
    class RHIBuffer : public RHIResource
    {
    public:
        virtual ~RHIBuffer() = default;
        
        //! Get buffer description
        const RHIBufferDesc& GetDesc() const { return m_Desc; }
        
        //! Get buffer size in bytes
        u64 GetSize() const { return m_Desc.size; }
        
        //! Get GPU virtual address
        virtual u64 GetGPUVirtualAddress() const = 0;
        
        //! Map buffer for CPU access
        virtual void* Map() = 0;
        
        //! Unmap buffer
        virtual void Unmap() = 0;
        
        //! Update buffer data
        virtual void Update(const void* data, u64 size, u64 offset = 0) = 0;
        
    protected:
        RHIBufferDesc m_Desc;
    };

    //=============================================================================
    // RHITexture - Abstract texture resource
    //=============================================================================
    class RHITexture : public RHIResource
    {
    public:
        virtual ~RHITexture() = default;
        
        //! Get texture description
        const RHITextureDesc& GetDesc() const { return m_Desc; }
        
        //! Get texture width
        u32 GetWidth() const { return m_Desc.width; }
        
        //! Get texture height
        u32 GetHeight() const { return m_Desc.height; }
        
        //! Get texture format
        RHIFormat GetFormat() const { return m_Desc.format; }
        
        //! Get mip levels
        u16 GetMipLevels() const { return m_Desc.mipLevels; }
        
        //! Get array size
        u16 GetArraySize() const { return m_Desc.depth; }
        
        //! Get texture dimension
        RHITextureDimension GetDimension() const { return m_Desc.dimension; }
        
    protected:
        RHITextureDesc m_Desc;
    };

    //=============================================================================
    // RHIRenderTarget - Render target (color or depth)
    //=============================================================================
    class RHIRenderTarget : public RHITexture
    {
    public:
        virtual ~RHIRenderTarget() = default;
        
        //! Is this a depth stencil target
        bool IsDepthStencil() const { return IsDepthStencilFormat(m_Desc.format); }
        
        //! Get RTV descriptor handle
        virtual RHIDescriptorHandle GetRTV() const = 0;
        
        //! Get DSV descriptor handle (if depth stencil)
        virtual RHIDescriptorHandle GetDSV() const = 0;
        
        //! Get SRV descriptor handle
        virtual RHIDescriptorHandle GetSRV() const = 0;
        
        //! Get UAV descriptor handle (if unordered access enabled)
        virtual RHIDescriptorHandle GetUAV() const = 0;
        
        //! Resize render target
        virtual void Resize(u32 width, u32 height) = 0;
    };

    //=============================================================================
    // RHIDescriptorHeap - Descriptor heap management
    //=============================================================================
    class RHIDescriptorHeap : public RHIResource
    {
    public:
        virtual ~RHIDescriptorHeap() = default;
        
        //! Get descriptor heap type
        virtual RHIDescriptorHeapType GetType() const = 0;
        
        //! Get descriptor count
        virtual u32 GetDescriptorCount() const = 0;
        
        //! Get CPU handle at index
        virtual RHIDescriptorHandle GetCPUHandle(u32 index) const = 0;
        
        //! Get GPU handle at index
        virtual RHIDescriptorHandle GetGPUHandle(u32 index) const = 0;
        
        //! Allocate descriptor and return index
        virtual u32 Allocate() = 0;
        
        //! Free descriptor at index
        virtual void Free(u32 index) = 0;
    };

    //=============================================================================
    // RHIPipelineState - Pipeline state object
    //=============================================================================
    class RHIPipelineState : public RHIResource
    {
    public:
        virtual ~RHIPipelineState() = default;
    };

    //=============================================================================
    // RHIRootSignature - Root signature / Pipeline layout
    //=============================================================================
    class RHIRootSignature : public RHIResource
    {
    public:
        virtual ~RHIRootSignature() = default;
    };

    //=============================================================================
    // RHIFence - GPU/CPU synchronization
    //=============================================================================
    class RHIFence : public RHIResource
    {
    public:
        virtual ~RHIFence() = default;
        
        //! Get current completed value
        virtual u64 GetCompletedValue() const = 0;
        
        //! Signal fence with value
        virtual void Signal(u64 value) = 0;
        
        //! Wait for fence to reach value (CPU blocking)
        virtual void Wait(u64 value) = 0;
    };

} // namespace Sea
