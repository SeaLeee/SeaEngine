#include "RenderGraph/PassNode.h"

namespace Sea
{
    PassNode::PassNode(u32 id, const std::string& name, PassType type)
        : m_Id(id), m_Name(name), m_Type(type)
    {
    }

    u32 PassNode::AddInput(const std::string& name, bool required)
    {
        PassSlot slot;
        slot.name = name;
        slot.isRequired = required;
        u32 index = static_cast<u32>(m_Inputs.size());
        m_Inputs.push_back(slot);
        return index;
    }

    u32 PassNode::AddOutput(const std::string& name)
    {
        PassSlot slot;
        slot.name = name;
        u32 index = static_cast<u32>(m_Outputs.size());
        m_Outputs.push_back(slot);
        return index;
    }

    void PassNode::SetInput(u32 slot, u32 resourceId)
    {
        if (slot < m_Inputs.size())
        {
            m_Inputs[slot].resourceId = resourceId;
        }
    }

    void PassNode::SetOutput(u32 slot, u32 resourceId)
    {
        if (slot < m_Outputs.size())
        {
            m_Outputs[slot].resourceId = resourceId;
        }
    }

    void PassNode::ClearInput(u32 slot)
    {
        if (slot < m_Inputs.size())
        {
            m_Inputs[slot].resourceId = UINT32_MAX;
        }
    }

    void PassNode::ClearOutput(u32 slot)
    {
        if (slot < m_Outputs.size())
        {
            m_Outputs[slot].resourceId = UINT32_MAX;
        }
    }

    u32 PassNode::GetInputResourceId(u32 slot) const
    {
        return slot < m_Inputs.size() ? m_Inputs[slot].resourceId : UINT32_MAX;
    }

    u32 PassNode::GetOutputResourceId(u32 slot) const
    {
        return slot < m_Outputs.size() ? m_Outputs[slot].resourceId : UINT32_MAX;
    }

    const char* PassNode::GetTypeString(PassType type)
    {
        switch (type)
        {
            case PassType::Graphics:     return "Graphics";
            case PassType::Compute:      return "Compute";
            case PassType::Copy:         return "Copy";
            case PassType::AsyncCompute: return "AsyncCompute";
            default: return "Unknown";
        }
    }
}
