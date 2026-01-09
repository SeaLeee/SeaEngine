#include "Graphics/GPUResource.h"

namespace Sea
{
    void GPUResource::SetName(const std::string& name)
    {
        m_Name = name;
        if (m_Resource)
        {
            std::wstring wname(name.begin(), name.end());
            m_Resource->SetName(wname.c_str());
        }
    }
}
