#include "RenderGraph/ResourceNode.h"

namespace Sea
{
    ResourceNode::ResourceNode(u32 id, const std::string& name, ResourceNodeType type)
        : m_Id(id), m_Name(name), m_Type(type)
    {
    }

    void ResourceNode::SetDimensions(u32 width, u32 height, u32 depth)
    {
        m_Width = width;
        m_Height = height;
        m_Depth = depth;
    }

    const char* ResourceNode::GetTypeString(ResourceNodeType type)
    {
        switch (type)
        {
            case ResourceNodeType::Texture2D:    return "Texture2D";
            case ResourceNodeType::Texture3D:    return "Texture3D";
            case ResourceNodeType::TextureCube:  return "TextureCube";
            case ResourceNodeType::Buffer:       return "Buffer";
            case ResourceNodeType::DepthStencil: return "DepthStencil";
            default: return "Unknown";
        }
    }
}
