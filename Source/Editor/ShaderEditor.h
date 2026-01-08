#pragma once
#include "Core/Types.h"

namespace Sea
{
    class ShaderEditor
    {
    public:
        void Render();
        void OpenFile(const std::string& path);
        void SaveFile();
        void Compile();

    private:
        std::string m_CurrentFile;
        std::string m_Source;
        std::string m_CompileOutput;
        bool m_Modified = false;
    };
}
