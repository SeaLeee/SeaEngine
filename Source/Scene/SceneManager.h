#pragma once

#include "Core/Types.h"
#include "Scene/SceneFile.h"
#include "Scene/SimpleRenderer.h"
#include <functional>
#include <memory>

namespace Sea
{
    class Device;
    class Camera;
    class Mesh;

    // 场景管理器 - 负责加载、切换和管理场景
    class SceneManager
    {
    public:
        SceneManager(Device& device);
        ~SceneManager();

        // 扫描场景目录
        void ScanScenes(const std::string& directory);
        
        // 加载场景
        bool LoadScene(const std::string& filepath);
        bool LoadScene(int index);
        
        // 保存当前场景
        bool SaveCurrentScene(const std::string& filepath);
        
        // 创建默认 PBR Demo 场景
        void CreatePBRDemoScene();
        
        // 切换到下一个/上一个场景
        void NextScene();
        void PreviousScene();
        
        // 获取场景列表
        const std::vector<std::string>& GetSceneFiles() const { return m_SceneFiles; }
        const std::vector<std::string>& GetSceneNames() const { return m_SceneNames; }
        
        // 当前场景
        int GetCurrentSceneIndex() const { return m_CurrentSceneIndex; }
        const std::string& GetCurrentSceneName() const;
        const SceneDef& GetCurrentSceneDef() const { return m_CurrentScene; }
        
        // 获取场景对象
        const std::vector<SceneObject>& GetSceneObjects() const { return m_SceneObjects; }
        std::vector<SceneObject>& GetSceneObjects() { return m_SceneObjects; }
        
        // 获取网格
        Mesh* GetGridMesh() const { return m_GridMesh.get(); }
        
        // 应用场景设置到渲染器和相机
        void ApplyToRenderer(SimpleRenderer& renderer);
        void ApplyToCamera(Camera& camera);
        
        // 回调
        using SceneChangedCallback = std::function<void(const std::string&)>;
        void SetOnSceneChanged(SceneChangedCallback callback) { m_OnSceneChanged = callback; }

    private:
        Mesh* CreateMeshFromDef(const SceneObjectDef& def);
        XMMATRIX ComputeTransform(const SceneObjectDef& def);

    private:
        Device& m_Device;
        
        // 场景文件列表
        std::vector<std::string> m_SceneFiles;
        std::vector<std::string> m_SceneNames;
        int m_CurrentSceneIndex = -1;
        
        // 当前场景数据
        SceneDef m_CurrentScene;
        std::vector<SceneObject> m_SceneObjects;
        
        // 缓存的网格
        std::unordered_map<std::string, Scope<Mesh>> m_MeshCache;
        Scope<Mesh> m_GridMesh;
        
        // 内置几何体
        Scope<Mesh> m_SphereMesh;
        Scope<Mesh> m_CubeMesh;
        Scope<Mesh> m_PlaneMesh;
        Scope<Mesh> m_TorusMesh;
        
        SceneChangedCallback m_OnSceneChanged;
    };
}
