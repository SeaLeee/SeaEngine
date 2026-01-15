#include "RHI/RHITypes.h"

namespace Sea
{
    u32 GetFormatByteSize(RHIFormat format)
    {
        switch (format)
        {
        case RHIFormat::R8_UNORM:
        case RHIFormat::R8_SNORM:
        case RHIFormat::R8_UINT:
        case RHIFormat::R8_SINT:
            return 1;
            
        case RHIFormat::R16_FLOAT:
        case RHIFormat::R16_UNORM:
        case RHIFormat::R16_UINT:
        case RHIFormat::R16_SINT:
        case RHIFormat::R8G8_UNORM:
        case RHIFormat::R8G8_SNORM:
        case RHIFormat::R8G8_UINT:
        case RHIFormat::R8G8_SINT:
            return 2;
            
        case RHIFormat::R32_FLOAT:
        case RHIFormat::R32_UINT:
        case RHIFormat::R32_SINT:
        case RHIFormat::R16G16_FLOAT:
        case RHIFormat::R16G16_UNORM:
        case RHIFormat::R16G16_UINT:
        case RHIFormat::R16G16_SINT:
        case RHIFormat::R8G8B8A8_UNORM:
        case RHIFormat::R8G8B8A8_UNORM_SRGB:
        case RHIFormat::R8G8B8A8_SNORM:
        case RHIFormat::R8G8B8A8_UINT:
        case RHIFormat::R8G8B8A8_SINT:
        case RHIFormat::B8G8R8A8_UNORM:
        case RHIFormat::B8G8R8A8_UNORM_SRGB:
        case RHIFormat::R10G10B10A2_UNORM:
        case RHIFormat::R10G10B10A2_UINT:
        case RHIFormat::R11G11B10_FLOAT:
        case RHIFormat::D24_UNORM_S8_UINT:
        case RHIFormat::D32_FLOAT:
            return 4;
            
        case RHIFormat::R16G16B16A16_FLOAT:
        case RHIFormat::R16G16B16A16_UNORM:
        case RHIFormat::R16G16B16A16_UINT:
        case RHIFormat::R16G16B16A16_SINT:
        case RHIFormat::R32G32_FLOAT:
        case RHIFormat::R32G32_UINT:
        case RHIFormat::R32G32_SINT:
        case RHIFormat::D32_FLOAT_S8X24_UINT:
            return 8;
            
        case RHIFormat::R32G32B32A32_FLOAT:
        case RHIFormat::R32G32B32A32_UINT:
        case RHIFormat::R32G32B32A32_SINT:
            return 16;
            
        case RHIFormat::D16_UNORM:
            return 2;
            
        default:
            return 0;
        }
    }
    
    const char* GetFormatName(RHIFormat format)
    {
        switch (format)
        {
        case RHIFormat::Unknown: return "Unknown";
        case RHIFormat::R8_UNORM: return "R8_UNORM";
        case RHIFormat::R8_SNORM: return "R8_SNORM";
        case RHIFormat::R8_UINT: return "R8_UINT";
        case RHIFormat::R8_SINT: return "R8_SINT";
        case RHIFormat::R16_FLOAT: return "R16_FLOAT";
        case RHIFormat::R16_UNORM: return "R16_UNORM";
        case RHIFormat::R16_UINT: return "R16_UINT";
        case RHIFormat::R16_SINT: return "R16_SINT";
        case RHIFormat::R8G8_UNORM: return "R8G8_UNORM";
        case RHIFormat::R32_FLOAT: return "R32_FLOAT";
        case RHIFormat::R32_UINT: return "R32_UINT";
        case RHIFormat::R8G8B8A8_UNORM: return "R8G8B8A8_UNORM";
        case RHIFormat::R8G8B8A8_UNORM_SRGB: return "R8G8B8A8_UNORM_SRGB";
        case RHIFormat::B8G8R8A8_UNORM: return "B8G8R8A8_UNORM";
        case RHIFormat::R16G16B16A16_FLOAT: return "R16G16B16A16_FLOAT";
        case RHIFormat::R32G32B32A32_FLOAT: return "R32G32B32A32_FLOAT";
        case RHIFormat::D16_UNORM: return "D16_UNORM";
        case RHIFormat::D24_UNORM_S8_UINT: return "D24_UNORM_S8_UINT";
        case RHIFormat::D32_FLOAT: return "D32_FLOAT";
        case RHIFormat::D32_FLOAT_S8X24_UINT: return "D32_FLOAT_S8X24_UINT";
        default: return "Unknown";
        }
    }

} // namespace Sea
