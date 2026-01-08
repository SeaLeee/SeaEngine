#pragma once
#include "Core/Types.h"

namespace Sea
{
    class RenderGraph;
    class Device;
    class Window;

    class Editor : public NonCopyable
    {
    public:
        Editor(Device& device, Window& window, RenderGraph& graph);
        ~Editor();

        bool Initialize();
        void Shutdown();
        void Update(f32 deltaTime);
        void Render();

        void ShowNodeEditor(bool* open = nullptr);
        void ShowPropertyPanel(bool* open = nullptr);
        void ShowShaderEditor(bool* open = nullptr);

    private:
        Device& m_Device;
        Window& m_Window;
        RenderGraph& m_Graph;
        bool m_ShowDemo = false;
    };
}
