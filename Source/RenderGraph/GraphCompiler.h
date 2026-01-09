#pragma once
#include "Core/Types.h"
#include "RenderGraph/ResourceNode.h"
#include "RenderGraph/PassNode.h"
#include <vector>
#include <unordered_map>
#include <unordered_set>

namespace Sea
{
    class RenderGraph;

    // 编译结果
    struct CompileResult
    {
        bool success = false;
        std::string errorMessage;
        std::vector<u32> executionOrder;
        std::vector<u32> culledPasses;  // 被剔除的Pass
    };

    // 资源状态转换
    struct ResourceTransition
    {
        u32 resourceId;
        D3D12_RESOURCE_STATES stateBefore;
        D3D12_RESOURCE_STATES stateAfter;
    };

    // Pass执行信息
    struct PassExecutionInfo
    {
        u32 passId;
        std::vector<ResourceTransition> transitionsBefore;
        std::vector<ResourceTransition> transitionsAfter;
    };

    // 图编译器 - 负责分析和优化RenderGraph
    class GraphCompiler
    {
    public:
        GraphCompiler() = default;
        ~GraphCompiler() = default;

        // 编译RenderGraph
        CompileResult Compile(RenderGraph& graph);

        // 获取Pass的执行信息
        const PassExecutionInfo* GetPassExecutionInfo(u32 passId) const;
        const std::vector<PassExecutionInfo>& GetExecutionPlan() const { return m_ExecutionPlan; }

        // 检查图是否有效
        bool ValidateGraph(const RenderGraph& graph, std::string& outError) const;

        // 获取统计信息
        u32 GetTotalResourceCount() const { return m_TotalResources; }
        u32 GetTransientResourceCount() const { return m_TransientResources; }
        u32 GetTotalPassCount() const { return m_TotalPasses; }
        u32 GetCulledPassCount() const { return m_CulledPasses; }

    private:
        // 构建依赖图
        void BuildDependencyGraph(const RenderGraph& graph);
        
        // 拓扑排序
        bool TopologicalSort(std::vector<u32>& outOrder);
        
        // 资源生命周期分析
        void AnalyzeResourceLifetimes(RenderGraph& graph);
        
        // Pass剔除（移除无效果的Pass）
        void CullUnusedPasses(RenderGraph& graph, std::vector<u32>& culledPasses);
        
        // 计算资源状态转换
        void ComputeResourceTransitions(const RenderGraph& graph);

        // 检测循环依赖
        bool HasCycle() const;

    private:
        // 依赖图：passId -> 依赖的passIds
        std::unordered_map<u32, std::vector<u32>> m_Dependencies;
        // 反向依赖图：passId -> 被哪些pass依赖
        std::unordered_map<u32, std::vector<u32>> m_Dependents;
        
        std::vector<PassExecutionInfo> m_ExecutionPlan;

        // 统计
        u32 m_TotalResources = 0;
        u32 m_TransientResources = 0;
        u32 m_TotalPasses = 0;
        u32 m_CulledPasses = 0;
    };
}
