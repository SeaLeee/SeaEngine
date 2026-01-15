#pragma once

#include "Core/Types.h"
#include <string>

namespace Sea
{
    //=============================================================================
    // Forward Declarations
    //=============================================================================
    class RHIDevice;
    class RHICommandList;
    class RHICommandQueue;
    class RHISwapChain;
    class RHIBuffer;
    class RHITexture;
    class RHIDescriptorHeap;
    class RHIPipelineState;
    class RHIRootSignature;
    class RHIFence;
    class RHIRenderTarget;

    //=============================================================================
    // Enums
    //=============================================================================

    //! Pixel format enumeration
    enum class RHIFormat : u32
    {
        Unknown = 0,
        
        // 8-bit formats
        R8_UNORM,
        R8_SNORM,
        R8_UINT,
        R8_SINT,
        
        // 16-bit formats
        R16_FLOAT,
        R16_UNORM,
        R16_SNORM,
        R16_UINT,
        R16_SINT,
        R8G8_UNORM,
        R8G8_SNORM,
        R8G8_UINT,
        R8G8_SINT,
        
        // 32-bit formats
        R32_FLOAT,
        R32_UINT,
        R32_SINT,
        R16G16_FLOAT,
        R16G16_UNORM,
        R16G16_SNORM,
        R16G16_UINT,
        R16G16_SINT,
        R8G8B8A8_UNORM,
        R8G8B8A8_UNORM_SRGB,
        R8G8B8A8_SNORM,
        R8G8B8A8_UINT,
        R8G8B8A8_SINT,
        B8G8R8A8_UNORM,
        B8G8R8A8_UNORM_SRGB,
        R10G10B10A2_UNORM,
        R10G10B10A2_UINT,
        R11G11B10_FLOAT,
        
        // 64-bit formats
        R16G16B16A16_FLOAT,
        R16G16B16A16_UNORM,
        R16G16B16A16_SNORM,
        R16G16B16A16_UINT,
        R16G16B16A16_SINT,
        R32G32_FLOAT,
        R32G32_UINT,
        R32G32_SINT,
        
        // 96-bit formats
        R32G32B32_FLOAT,
        R32G32B32_UINT,
        R32G32B32_SINT,
        
        // 128-bit formats
        R32G32B32A32_FLOAT,
        R32G32B32A32_UINT,
        R32G32B32A32_SINT,
        
        // Depth-stencil formats
        D16_UNORM,
        D24_UNORM_S8_UINT,
        D32_FLOAT,
        D32_FLOAT_S8X24_UINT,
        
        // Compressed formats
        BC1_UNORM,
        BC1_UNORM_SRGB,
        BC2_UNORM,
        BC2_UNORM_SRGB,
        BC3_UNORM,
        BC3_UNORM_SRGB,
        BC4_UNORM,
        BC4_SNORM,
        BC5_UNORM,
        BC5_SNORM,
        BC6H_UF16,
        BC6H_SF16,
        BC7_UNORM,
        BC7_UNORM_SRGB,
        
        Count
    };

    //! Resource state for transitions
    enum class RHIResourceState : u32
    {
        Common = 0,
        VertexBuffer,
        IndexBuffer,
        ConstantBuffer,
        RenderTarget,
        UnorderedAccess,
        DepthWrite,
        DepthRead,
        ShaderResource,
        StreamOut,
        IndirectArgument,
        CopyDest,
        CopySource,
        Present,
        GenericRead
    };

    //! Command queue type
    enum class RHICommandQueueType : u8
    {
        Direct = 0,     // Graphics queue
        Compute,
        Copy,
        Count
    };

    //! Primitive topology
    enum class RHIPrimitiveTopology : u8
    {
        PointList = 0,
        LineList,
        LineStrip,
        TriangleList,
        TriangleStrip,
        Count
    };

    //! Blend factor
    enum class RHIBlendFactor : u8
    {
        Zero = 0,
        One,
        SrcColor,
        InvSrcColor,
        SrcAlpha,
        InvSrcAlpha,
        DstColor,
        InvDstColor,
        DstAlpha,
        InvDstAlpha,
        Count
    };

    //! Blend operation
    enum class RHIBlendOp : u8
    {
        Add = 0,
        Subtract,
        RevSubtract,
        Min,
        Max,
        Count
    };

    //! Comparison function
    enum class RHIComparisonFunc : u8
    {
        Never = 0,
        Less,
        Equal,
        LessEqual,
        Greater,
        NotEqual,
        GreaterEqual,
        Always,
        Count
    };

    //! Cull mode
    enum class RHICullMode : u8
    {
        None = 0,
        Front,
        Back,
        Count
    };

    //! Fill mode
    enum class RHIFillMode : u8
    {
        Wireframe = 0,
        Solid,
        Count
    };

    //! Texture dimension
    enum class RHITextureDimension : u8
    {
        Texture1D = 0,
        Texture2D,
        Texture3D,
        TextureCube,
        Count
    };

    //! Descriptor heap type
    enum class RHIDescriptorHeapType : u8
    {
        CBV_SRV_UAV = 0,
        Sampler,
        RTV,
        DSV,
        Count
    };

    //! Shader stage flags
    enum class RHIShaderStage : u8
    {
        Vertex = 1 << 0,
        Hull = 1 << 1,
        Domain = 1 << 2,
        Geometry = 1 << 3,
        Pixel = 1 << 4,
        Compute = 1 << 5,
        
        AllGraphics = Vertex | Hull | Domain | Geometry | Pixel,
        All = AllGraphics | Compute
    };

    //! Buffer memory type (heap type)
    enum class RHIBufferUsage : u32
    {
        Default = 0,        // GPU-local memory, fastest for GPU read/write
        Upload = 1,         // CPU-writable, GPU-readable (for upload buffers)
        Readback = 2        // GPU-writable, CPU-readable (for readback buffers)
    };

    //! Buffer usage flags
    enum class RHIBufferFlags : u32
    {
        None = 0,
        VertexBuffer = 1 << 0,
        IndexBuffer = 1 << 1,
        ConstantBuffer = 1 << 2,
        ShaderResource = 1 << 3,
        UnorderedAccess = 1 << 4,
        IndirectBuffer = 1 << 5,
        CopyDest = 1 << 6,
        CopySource = 1 << 7
    };
    inline RHIBufferFlags operator|(RHIBufferFlags a, RHIBufferFlags b)
    {
        return static_cast<RHIBufferFlags>(static_cast<u32>(a) | static_cast<u32>(b));
    }
    inline bool operator&(RHIBufferFlags a, RHIBufferFlags b)
    {
        return (static_cast<u32>(a) & static_cast<u32>(b)) != 0;
    }

    //! Texture usage flags
    enum class RHITextureUsage : u32
    {
        None = 0,
        ShaderResource = 1 << 0,
        RenderTarget = 1 << 1,
        DepthStencil = 1 << 2,
        UnorderedAccess = 1 << 3,
        CopyDest = 1 << 4,
        CopySource = 1 << 5
    };
    inline RHITextureUsage operator|(RHITextureUsage a, RHITextureUsage b)
    {
        return static_cast<RHITextureUsage>(static_cast<u32>(a) | static_cast<u32>(b));
    }
    inline bool operator&(RHITextureUsage a, RHITextureUsage b)
    {
        return (static_cast<u32>(a) & static_cast<u32>(b)) != 0;
    }

    //! Render target initialization
    enum class RHIRenderTargetInit : u8
    {
        None = 0,       // No initialization
        Discard = 1,    // Discard contents
        Clear = 2,      // Clear to specified color
        Load = 3        // Load previous contents
    };

    //=============================================================================
    // Structures
    //=============================================================================

    //! Viewport
    struct RHIViewport
    {
        f32 x = 0.0f;
        f32 y = 0.0f;
        f32 width = 0.0f;
        f32 height = 0.0f;
        f32 minDepth = 0.0f;
        f32 maxDepth = 1.0f;
    };

    //! Scissor rectangle
    struct RHIScissorRect
    {
        i32 left = 0;
        i32 top = 0;
        i32 right = 0;
        i32 bottom = 0;
    };

    //! Clear value
    struct RHIClearValue
    {
        union
        {
            f32 color[4];
            struct
            {
                f32 depth;
                u8 stencil;
            } depthStencil;
        };
        
        static RHIClearValue CreateColor(f32 r, f32 g, f32 b, f32 a = 1.0f)
        {
            RHIClearValue cv;
            cv.color[0] = r;
            cv.color[1] = g;
            cv.color[2] = b;
            cv.color[3] = a;
            return cv;
        }
        
        static RHIClearValue CreateDepthStencil(f32 d, u8 s = 0)
        {
            RHIClearValue cv;
            cv.depthStencil.depth = d;
            cv.depthStencil.stencil = s;
            return cv;
        }
    };

    //! Buffer descriptor
    struct RHIBufferDesc
    {
        u64 size = 0;
        RHIBufferUsage usage = RHIBufferUsage::Default;
        u32 structureByteStride = 0;  // For structured buffers
        bool allowUAV = false;
        std::string name;
    };

    //! Texture descriptor
    struct RHITextureDesc
    {
        u32 width = 1;
        u32 height = 1;
        u16 depth = 1;
        u16 mipLevels = 1;
        u32 sampleCount = 1;
        RHIFormat format = RHIFormat::R8G8B8A8_UNORM;
        RHITextureDimension dimension = RHITextureDimension::Texture2D;
        RHITextureUsage usage = RHITextureUsage::ShaderResource;
        RHIClearValue clearValue = {};
        std::string name;
    };

    //! Vertex buffer view
    struct RHIVertexBufferView
    {
        u64 gpuAddress = 0;
        u32 sizeInBytes = 0;
        u32 strideInBytes = 0;
    };

    //! Index buffer view
    struct RHIIndexBufferView
    {
        u64 gpuAddress = 0;
        u32 sizeInBytes = 0;
        bool is32Bit = true;
    };

    //! Descriptor handle (opaque, platform-specific)
    struct RHIDescriptorHandle
    {
        u64 cpuHandle = 0;
        u64 gpuHandle = 0;
        
        bool IsValid() const { return cpuHandle != 0; }
    };

    //! Sub-resource specification
    struct RHISubResource
    {
        u32 mipLevel = 0;
        u32 arraySlice = 0;
        u32 planeSlice = 0;
    };

    //=============================================================================
    // Helper Functions
    //=============================================================================
    
    //! Check if format is depth-stencil
    inline bool IsDepthStencilFormat(RHIFormat format)
    {
        switch (format)
        {
        case RHIFormat::D16_UNORM:
        case RHIFormat::D24_UNORM_S8_UINT:
        case RHIFormat::D32_FLOAT:
        case RHIFormat::D32_FLOAT_S8X24_UINT:
            return true;
        default:
            return false;
        }
    }
    
    //! Check if format has stencil
    inline bool HasStencil(RHIFormat format)
    {
        switch (format)
        {
        case RHIFormat::D24_UNORM_S8_UINT:
        case RHIFormat::D32_FLOAT_S8X24_UINT:
            return true;
        default:
            return false;
        }
    }

    //! Get format byte size
    u32 GetFormatByteSize(RHIFormat format);
    
    //! Get format name
    const char* GetFormatName(RHIFormat format);

} // namespace Sea
