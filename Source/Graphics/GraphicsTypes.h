#pragma once

#include "Core/Types.h"
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <DirectXMath.h>

using Microsoft::WRL::ComPtr;
using namespace DirectX;

namespace Sea
{
    // 前向声明
    class Device;
    class SwapChain;
    class CommandQueue;
    class CommandList;
    class DescriptorHeap;
    class Buffer;
    class Texture;
    class RenderTarget;
    class PipelineState;
    class RootSignature;
    class Fence;

    // 资源格式
    enum class Format : u32
    {
        Unknown = DXGI_FORMAT_UNKNOWN,
        R8G8B8A8_UNORM = DXGI_FORMAT_R8G8B8A8_UNORM,
        R8G8B8A8_UNORM_SRGB = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
        B8G8R8A8_UNORM = DXGI_FORMAT_B8G8R8A8_UNORM,
        R16G16B16A16_FLOAT = DXGI_FORMAT_R16G16B16A16_FLOAT,
        R32G32B32A32_FLOAT = DXGI_FORMAT_R32G32B32A32_FLOAT,
        R32G32B32_FLOAT = DXGI_FORMAT_R32G32B32_FLOAT,
        R32G32_FLOAT = DXGI_FORMAT_R32G32_FLOAT,
        R32_FLOAT = DXGI_FORMAT_R32_FLOAT,
        R16_FLOAT = DXGI_FORMAT_R16_FLOAT,
        R11G11B10_FLOAT = DXGI_FORMAT_R11G11B10_FLOAT,
        D32_FLOAT = DXGI_FORMAT_D32_FLOAT,
        D24_UNORM_S8_UINT = DXGI_FORMAT_D24_UNORM_S8_UINT,
        D16_UNORM = DXGI_FORMAT_D16_UNORM,
        R32_UINT = DXGI_FORMAT_R32_UINT,
        R16_UINT = DXGI_FORMAT_R16_UINT,
        R8_UNORM = DXGI_FORMAT_R8_UNORM,
    };

    // 资源状态
    enum class ResourceState : u32
    {
        Common = D3D12_RESOURCE_STATE_COMMON,
        VertexBuffer = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
        ConstantBuffer = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
        IndexBuffer = D3D12_RESOURCE_STATE_INDEX_BUFFER,
        RenderTarget = D3D12_RESOURCE_STATE_RENDER_TARGET,
        UnorderedAccess = D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        DepthWrite = D3D12_RESOURCE_STATE_DEPTH_WRITE,
        DepthRead = D3D12_RESOURCE_STATE_DEPTH_READ,
        ShaderResource = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
        CopyDest = D3D12_RESOURCE_STATE_COPY_DEST,
        CopySource = D3D12_RESOURCE_STATE_COPY_SOURCE,
        Present = D3D12_RESOURCE_STATE_PRESENT,
    };

    // 命令队列类型
    enum class CommandQueueType : u32
    {
        Graphics = D3D12_COMMAND_LIST_TYPE_DIRECT,
        Compute = D3D12_COMMAND_LIST_TYPE_COMPUTE,
        Copy = D3D12_COMMAND_LIST_TYPE_COPY,
    };

    // 描述符堆类型
    enum class DescriptorHeapType : u32
    {
        CBV_SRV_UAV = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        Sampler = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,
        RTV = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
        DSV = D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
    };

    // 缓冲区类型
    enum class BufferType : u32
    {
        Vertex,
        Index,
        Constant,
        Structured,
        Raw,
        Upload,
        Readback,
    };

    // 纹理类型
    enum class TextureType : u32
    {
        Texture1D,
        Texture2D,
        Texture3D,
        TextureCube,
    };

    // 纹理用途标志
    enum class TextureUsage : u32
    {
        None = 0,
        ShaderResource = 1 << 0,
        RenderTarget = 1 << 1,
        DepthStencil = 1 << 2,
        UnorderedAccess = 1 << 3,
    };
    template<> struct EnableBitmaskOperators<TextureUsage> : std::true_type {};

    // 采样器过滤
    enum class SamplerFilter : u32
    {
        Point = D3D12_FILTER_MIN_MAG_MIP_POINT,
        Linear = D3D12_FILTER_MIN_MAG_MIP_LINEAR,
        Anisotropic = D3D12_FILTER_ANISOTROPIC,
    };

    // 寻址模式
    enum class AddressMode : u32
    {
        Wrap = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        Clamp = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        Mirror = D3D12_TEXTURE_ADDRESS_MODE_MIRROR,
        Border = D3D12_TEXTURE_ADDRESS_MODE_BORDER,
    };

    // 图元拓扑
    enum class PrimitiveTopology : u32
    {
        TriangleList = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
        TriangleStrip = D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP,
        LineList = D3D_PRIMITIVE_TOPOLOGY_LINELIST,
        LineStrip = D3D_PRIMITIVE_TOPOLOGY_LINESTRIP,
        PointList = D3D_PRIMITIVE_TOPOLOGY_POINTLIST,
    };

    // 填充模式
    enum class FillMode : u32
    {
        Solid = D3D12_FILL_MODE_SOLID,
        Wireframe = D3D12_FILL_MODE_WIREFRAME,
    };

    // 剔除模式
    enum class CullMode : u32
    {
        None = D3D12_CULL_MODE_NONE,
        Front = D3D12_CULL_MODE_FRONT,
        Back = D3D12_CULL_MODE_BACK,
    };

    // 混合操作
    enum class BlendOp : u32
    {
        Add = D3D12_BLEND_OP_ADD,
        Subtract = D3D12_BLEND_OP_SUBTRACT,
        RevSubtract = D3D12_BLEND_OP_REV_SUBTRACT,
        Min = D3D12_BLEND_OP_MIN,
        Max = D3D12_BLEND_OP_MAX,
    };

    // 混合因子
    enum class BlendFactor : u32
    {
        Zero = D3D12_BLEND_ZERO,
        One = D3D12_BLEND_ONE,
        SrcColor = D3D12_BLEND_SRC_COLOR,
        InvSrcColor = D3D12_BLEND_INV_SRC_COLOR,
        SrcAlpha = D3D12_BLEND_SRC_ALPHA,
        InvSrcAlpha = D3D12_BLEND_INV_SRC_ALPHA,
        DstAlpha = D3D12_BLEND_DEST_ALPHA,
        InvDstAlpha = D3D12_BLEND_INV_DEST_ALPHA,
        DstColor = D3D12_BLEND_DEST_COLOR,
        InvDstColor = D3D12_BLEND_INV_DEST_COLOR,
    };

    // 深度比较函数
    enum class CompareFunc : u32
    {
        Never = D3D12_COMPARISON_FUNC_NEVER,
        Less = D3D12_COMPARISON_FUNC_LESS,
        Equal = D3D12_COMPARISON_FUNC_EQUAL,
        LessEqual = D3D12_COMPARISON_FUNC_LESS_EQUAL,
        Greater = D3D12_COMPARISON_FUNC_GREATER,
        NotEqual = D3D12_COMPARISON_FUNC_NOT_EQUAL,
        GreaterEqual = D3D12_COMPARISON_FUNC_GREATER_EQUAL,
        Always = D3D12_COMPARISON_FUNC_ALWAYS,
    };

    // 视口
    struct Viewport
    {
        f32 x = 0.0f;
        f32 y = 0.0f;
        f32 width = 0.0f;
        f32 height = 0.0f;
        f32 minDepth = 0.0f;
        f32 maxDepth = 1.0f;

        D3D12_VIEWPORT ToD3D12() const
        {
            return { x, y, width, height, minDepth, maxDepth };
        }
    };

    // 裁剪矩形
    struct ScissorRect
    {
        i32 left = 0;
        i32 top = 0;
        i32 right = 0;
        i32 bottom = 0;

        D3D12_RECT ToD3D12() const
        {
            return { left, top, right, bottom };
        }
    };

    // 描述符句柄
    struct DescriptorHandle
    {
        D3D12_CPU_DESCRIPTOR_HANDLE cpu = { 0 };
        D3D12_GPU_DESCRIPTOR_HANDLE gpu = { 0 };
        u32 heapIndex = 0;

        bool IsValid() const { return cpu.ptr != 0; }
        bool IsShaderVisible() const { return gpu.ptr != 0; }
    };

    // 获取格式的字节大小
    inline u32 GetFormatSize(Format format)
    {
        switch (format)
        {
        case Format::R32G32B32A32_FLOAT: return 16;
        case Format::R16G16B16A16_FLOAT: return 8;
        case Format::R32G32B32_FLOAT: return 12;
        case Format::R32G32_FLOAT: return 8;
        case Format::R11G11B10_FLOAT: return 4;
        case Format::R8G8B8A8_UNORM:
        case Format::R8G8B8A8_UNORM_SRGB:
        case Format::B8G8R8A8_UNORM: return 4;
        case Format::R32_FLOAT:
        case Format::R32_UINT:
        case Format::D32_FLOAT:
        case Format::D24_UNORM_S8_UINT: return 4;
        case Format::R16_FLOAT:
        case Format::R16_UINT:
        case Format::D16_UNORM: return 2;
        case Format::R8_UNORM: return 1;
        default: return 0;
        }
    }

    // 检查是否是深度格式
    inline bool IsDepthFormat(Format format)
    {
        return format == Format::D32_FLOAT || 
               format == Format::D24_UNORM_S8_UINT || 
               format == Format::D16_UNORM;
    }
}
