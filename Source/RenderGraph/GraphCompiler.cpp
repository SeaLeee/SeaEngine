#include "RenderGraph/GraphCompiler.h"
#include "RenderGraph/RenderGraph.h"
#include "Core/Log.h"
#include <queue>
#include <algorithm>
#include <stack>

namespace Sea
{
    CompileResult GraphCompiler::Compile(RenderGraph& graph)
    {
        CompileResult result;
        m_Dependencies.clear();
        m_Dependents.clear();
        m_ExecutionPlan.clear();

        // 验证图
        if (!ValidateGraph(graph, result.errorMessage))
        {
            result.success = false;
            return result;
        }

        // 构建依赖图
        BuildDependencyGraph(graph);

        // 检测循环
        if (HasCycle())
        {
            result.success = false;
            result.errorMessage = "Render graph contains cyclic dependencies";
            return result;
        }

        // 拓扑排序
        if (!TopologicalSort(result.executionOrder))
        {
            result.success = false;
            result.errorMessage = "Failed to perform topological sort";
            return result;
        }

        // 剔除无用Pass
        CullUnusedPasses(graph, result.culledPasses);

        // 分析资源生命周期
        AnalyzeResourceLifetimes(graph);

        // 计算资源状态转换
        ComputeResourceTransitions(graph);

        // 更新统计
        m_TotalPasses = static_cast<u32>(graph.GetPasses().size());
        m_CulledPasses = static_cast<u32>(result.culledPasses.size());
        m_TotalResources = static_cast<u32>(graph.GetResources().size());

        result.success = true;
        SEA_CORE_INFO("Graph compiled: {} passes, {} resources", 
                      m_TotalPasses - m_CulledPasses, m_TotalResources);

        return result;
    }

    const PassExecutionInfo* GraphCompiler::GetPassExecutionInfo(u32 passId) const
    {
        for (const auto& info : m_ExecutionPlan)
        {
            if (info.passId == passId)
                return &info;
        }
        return nullptr;
    }

    bool GraphCompiler::ValidateGraph(const RenderGraph& graph, std::string& outError) const
    {
        const auto& passes = graph.GetPasses();
        const auto& resources = graph.GetResources();

        // 检查所有Pass的输入输出是否有效
        for (const auto& pass : passes)
        {
            for (const auto& input : pass.GetInputs())
            {
                if (input.IsConnected() && input.resourceId >= resources.size())
                {
                    outError = "Pass '" + pass.GetName() + "' references invalid input resource";
                    return false;
                }
            }
            for (const auto& output : pass.GetOutputs())
            {
                if (output.IsConnected() && output.resourceId >= resources.size())
                {
                    outError = "Pass '" + pass.GetName() + "' references invalid output resource";
                    return false;
                }
            }
        }

        return true;
    }

    void GraphCompiler::BuildDependencyGraph(const RenderGraph& graph)
    {
        const auto& passes = graph.GetPasses();

        // 构建资源 -> 生产者Pass 的映射
        std::unordered_map<u32, u32> resourceProducers;
        for (u32 i = 0; i < passes.size(); ++i)
        {
            for (const auto& output : passes[i].GetOutputs())
            {
                if (output.IsConnected())
                {
                    resourceProducers[output.resourceId] = i;
                }
            }
        }

        // 构建依赖关系
        for (u32 i = 0; i < passes.size(); ++i)
        {
            m_Dependencies[i] = {};
            for (const auto& input : passes[i].GetInputs())
            {
                if (input.IsConnected())
                {
                    auto it = resourceProducers.find(input.resourceId);
                    if (it != resourceProducers.end() && it->second != i)
                    {
                        m_Dependencies[i].push_back(it->second);
                        m_Dependents[it->second].push_back(i);
                    }
                }
            }
        }
    }

    bool GraphCompiler::TopologicalSort(std::vector<u32>& outOrder)
    {
        outOrder.clear();
        
        std::unordered_map<u32, u32> inDegree;
        for (const auto& [node, deps] : m_Dependencies)
        {
            if (inDegree.find(node) == inDegree.end())
                inDegree[node] = 0;
            inDegree[node] += static_cast<u32>(deps.size());
        }

        std::queue<u32> queue;
        for (const auto& [node, degree] : inDegree)
        {
            if (degree == 0)
                queue.push(node);
        }

        while (!queue.empty())
        {
            u32 node = queue.front();
            queue.pop();
            outOrder.push_back(node);

            auto it = m_Dependents.find(node);
            if (it != m_Dependents.end())
            {
                for (u32 dependent : it->second)
                {
                    if (--inDegree[dependent] == 0)
                        queue.push(dependent);
                }
            }
        }

        return outOrder.size() == m_Dependencies.size();
    }

    void GraphCompiler::AnalyzeResourceLifetimes(RenderGraph& graph)
    {
        auto& resources = graph.GetResources();
        const auto& passes = graph.GetPasses();

        // 重置生命周期
        for (auto& res : resources)
        {
            res.SetLifetime(UINT32_MAX, 0);
        }

        // 遍历所有Pass，更新资源生命周期
        for (u32 i = 0; i < passes.size(); ++i)
        {
            for (const auto& input : passes[i].GetInputs())
            {
                if (input.IsConnected() && input.resourceId < resources.size())
                {
                    auto& res = resources[input.resourceId];
                    res.SetLifetime(
                        std::min(res.GetFirstUsePass(), i),
                        std::max(res.GetLastUsePass(), i)
                    );
                }
            }
            for (const auto& output : passes[i].GetOutputs())
            {
                if (output.IsConnected() && output.resourceId < resources.size())
                {
                    auto& res = resources[output.resourceId];
                    res.SetLifetime(
                        std::min(res.GetFirstUsePass(), i),
                        std::max(res.GetLastUsePass(), i)
                    );
                }
            }
        }

        // 统计transient资源
        m_TransientResources = 0;
        for (const auto& res : resources)
        {
            if (!res.IsExternal() && res.GetFirstUsePass() != UINT32_MAX)
            {
                m_TransientResources++;
            }
        }
    }

    void GraphCompiler::CullUnusedPasses(RenderGraph& graph, std::vector<u32>& culledPasses)
    {
        culledPasses.clear();
        // 简单实现：禁用的Pass会被剔除
        const auto& passes = graph.GetPasses();
        for (u32 i = 0; i < passes.size(); ++i)
        {
            if (!passes[i].IsEnabled())
            {
                culledPasses.push_back(i);
            }
        }
    }

    void GraphCompiler::ComputeResourceTransitions(const RenderGraph& graph)
    {
        m_ExecutionPlan.clear();
        
        const auto& passes = graph.GetPasses();
        for (u32 i = 0; i < passes.size(); ++i)
        {
            if (!passes[i].IsEnabled()) continue;

            PassExecutionInfo info;
            info.passId = i;

            // 输入资源需要转换到SRV状态
            for (const auto& input : passes[i].GetInputs())
            {
                if (input.IsConnected())
                {
                    ResourceTransition trans;
                    trans.resourceId = input.resourceId;
                    trans.stateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
                    trans.stateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
                    info.transitionsBefore.push_back(trans);
                }
            }

            // 输出资源需要转换到RTV状态
            for (const auto& output : passes[i].GetOutputs())
            {
                if (output.IsConnected())
                {
                    ResourceTransition trans;
                    trans.resourceId = output.resourceId;
                    trans.stateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
                    trans.stateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
                    info.transitionsBefore.push_back(trans);
                }
            }

            m_ExecutionPlan.push_back(info);
        }
    }

    bool GraphCompiler::HasCycle() const
    {
        std::unordered_set<u32> visited;
        std::unordered_set<u32> recursionStack;

        std::function<bool(u32)> dfs = [&](u32 node) -> bool {
            visited.insert(node);
            recursionStack.insert(node);

            auto it = m_Dependents.find(node);
            if (it != m_Dependents.end())
            {
                for (u32 neighbor : it->second)
                {
                    if (visited.find(neighbor) == visited.end())
                    {
                        if (dfs(neighbor)) return true;
                    }
                    else if (recursionStack.find(neighbor) != recursionStack.end())
                    {
                        return true;
                    }
                }
            }

            recursionStack.erase(node);
            return false;
        };

        for (const auto& [node, _] : m_Dependencies)
        {
            if (visited.find(node) == visited.end())
            {
                if (dfs(node)) return true;
            }
        }

        return false;
    }
}
