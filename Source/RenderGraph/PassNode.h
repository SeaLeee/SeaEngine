#pragma once
#include "Core/Types.h"
#include "RenderGraph/ResourceNode.h"
#include <string>
#include <vector>
#include <functional>

namespace Sea
{
    class CommandList;
    class RenderPassContext;

    // Pass类型
    enum class PassType
    {
        Graphics,   // 光栅化渲染
        Compute,    // 计算着色器
        Copy,       // 资源拷贝
        AsyncCompute // 异步计算
    };

    // Pass输入输出槽
    struct PassSlot
    {
        u32 resourceId = UINT32_MAX;
        std::string name;
        bool isRequired = true;

        bool IsConnected() const { return resourceId != UINT32_MAX; }
    };

    // Pass执行回调
    using PassExecuteCallback = std::function<void(CommandList& cmdList, RenderPassContext& ctx)>;

    // Pass节点 - 表示RenderGraph中的一个渲染Pass
    class PassNode
    {
    public:
        PassNode() = default;
        PassNode(u32 id, const std::string& name, PassType type = PassType::Graphics);
        ~PassNode() = default;

        // 基本属性
        u32 GetId() const { return m_Id; }
        const std::string& GetName() const { return m_Name; }
        void SetName(const std::string& name) { m_Name = name; }

        PassType GetType() const { return m_Type; }
        void SetType(PassType type) { m_Type = type; }

        // 输入输出管理
        u32 AddInput(const std::string& name, bool required = true);
        u32 AddOutput(const std::string& name);
        
        void SetInput(u32 slot, u32 resourceId);
        void SetOutput(u32 slot, u32 resourceId);
        
        void ClearInput(u32 slot);
        void ClearOutput(u32 slot);

        const std::vector<PassSlot>& GetInputs() const { return m_Inputs; }
        const std::vector<PassSlot>& GetOutputs() const { return m_Outputs; }

        u32 GetInputResourceId(u32 slot) const;
        u32 GetOutputResourceId(u32 slot) const;

        // 执行回调
        void SetExecuteCallback(PassExecuteCallback callback) { m_ExecuteCallback = std::move(callback); }
        const PassExecuteCallback& GetExecuteCallback() const { return m_ExecuteCallback; }
        bool HasExecuteCallback() const { return m_ExecuteCallback != nullptr; }

        // 节点编辑器位置
        void SetPosition(float x, float y) { m_PosX = x; m_PosY = y; }
        float GetPosX() const { return m_PosX; }
        float GetPosY() const { return m_PosY; }

        // 启用/禁用
        void SetEnabled(bool enabled) { m_Enabled = enabled; }
        bool IsEnabled() const { return m_Enabled; }

        // 依赖的Pass列表（编译后填充）
        void SetDependencies(const std::vector<u32>& deps) { m_Dependencies = deps; }
        const std::vector<u32>& GetDependencies() const { return m_Dependencies; }

        static const char* GetTypeString(PassType type);

    private:
        u32 m_Id = UINT32_MAX;
        std::string m_Name;
        PassType m_Type = PassType::Graphics;

        std::vector<PassSlot> m_Inputs;
        std::vector<PassSlot> m_Outputs;

        PassExecuteCallback m_ExecuteCallback;

        float m_PosX = 0.0f;
        float m_PosY = 0.0f;

        bool m_Enabled = true;
        std::vector<u32> m_Dependencies;
    };
}
