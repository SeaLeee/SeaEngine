#pragma once
#include "Core/Types.h"
#include "RenderGraph/RenderGraph.h"

namespace Sea
{
    class PropertyPanel
    {
    public:
        PropertyPanel(RenderGraph* graph);
        void Render();
        void SetSelectedPass(u32 passId) { m_SelectedPass = passId; }
        void SetSelectedResource(u32 resId) { m_SelectedResource = resId; }
        void SetRenderGraph(RenderGraph* graph) { m_Graph = graph; }

    private:
        void RenderPassProperties(PassNode& pass);
        void RenderResourceProperties(ResourceNode& res);

        RenderGraph* m_Graph = nullptr;
        u32 m_SelectedPass = UINT32_MAX;
        u32 m_SelectedResource = UINT32_MAX;
    };
}
