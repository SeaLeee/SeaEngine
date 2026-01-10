#include "Scene/SceneManager.h"
#include "Scene/Camera.h"
#include "Core/Log.h"
#include <filesystem>

namespace Sea
{
    namespace fs = std::filesystem;

    SceneManager::SceneManager(Device& device)
        : m_Device(device)
    {
        // 创建内置几何体
        m_SphereMesh = Mesh::CreateSphere(device, 0.5f, 48, 24);
        m_CubeMesh = Mesh::CreateCube(device, 1.0f);
        m_PlaneMesh = Mesh::CreatePlane(device, 10.0f, 10.0f);
        m_TorusMesh = Mesh::CreateTorus(device, 0.6f, 0.2f, 32, 24);
        m_GridMesh = Mesh::CreatePlane(device, 100.0f, 100.0f);
        
        SEA_CORE_INFO("SceneManager initialized");
    }

    SceneManager::~SceneManager()
    {
        m_SceneObjects.clear();
        m_MeshCache.clear();
    }

    void SceneManager::ScanScenes(const std::string& directory)
    {
        m_SceneFiles = SceneFile::GetSceneFiles(directory);
        m_SceneNames.clear();
        
        for (const auto& path : m_SceneFiles)
        {
            fs::path p(path);
            m_SceneNames.push_back(p.stem().string());
        }
        
        SEA_CORE_INFO("Found {} scene files in {}", m_SceneFiles.size(), directory);
    }

    bool SceneManager::LoadScene(const std::string& filepath)
    {
        SceneDef newScene;
        if (!SceneFile::Load(filepath, newScene))
        {
            return false;
        }

        m_CurrentScene = newScene;
        m_SceneObjects.clear();

        // 创建场景对象
        for (const auto& def : m_CurrentScene.objects)
        {
            SceneObject obj;
            obj.mesh = CreateMeshFromDef(def);
            if (!obj.mesh) continue;
            
            XMMATRIX transform = ComputeTransform(def);
            XMStoreFloat4x4(&obj.transform, transform);
            
            obj.color = def.color;
            obj.metallic = def.metallic;
            obj.roughness = def.roughness;
            obj.ao = def.ao;
            obj.emissiveColor = def.emissiveColor;
            obj.emissiveIntensity = def.emissiveIntensity;
            
            m_SceneObjects.push_back(obj);
        }

        // 更新当前索引
        for (size_t i = 0; i < m_SceneFiles.size(); ++i)
        {
            if (m_SceneFiles[i] == filepath)
            {
                m_CurrentSceneIndex = static_cast<int>(i);
                break;
            }
        }

        SEA_CORE_INFO("Loaded scene: {} ({} objects)", m_CurrentScene.name, m_SceneObjects.size());
        
        if (m_OnSceneChanged)
        {
            m_OnSceneChanged(m_CurrentScene.name);
        }
        
        return true;
    }

    bool SceneManager::LoadScene(int index)
    {
        if (index < 0 || index >= static_cast<int>(m_SceneFiles.size()))
        {
            SEA_CORE_ERROR("Invalid scene index: {}", index);
            return false;
        }
        return LoadScene(m_SceneFiles[index]);
    }

    bool SceneManager::SaveCurrentScene(const std::string& filepath)
    {
        return SceneFile::Save(filepath, m_CurrentScene);
    }

    void SceneManager::CreatePBRDemoScene()
    {
        m_CurrentScene = SceneDef();
        m_CurrentScene.name = "PBR Material Demo";
        m_CurrentScene.description = "7x7 grid of spheres demonstrating PBR materials";
        m_CurrentScene.author = "SeaEngine";
        
        m_CurrentScene.camera.position = { 0, 8, -12 };
        m_CurrentScene.camera.target = { 0, 0, 5 };
        m_CurrentScene.camera.fov = 60.0f;
        
        LightDef mainLight;
        mainLight.type = "directional";
        mainLight.direction = { -0.5f, -1.0f, 0.5f };
        mainLight.color = { 1.0f, 0.98f, 0.95f };
        mainLight.intensity = 2.0f;
        m_CurrentScene.lights.push_back(mainLight);
        
        m_CurrentScene.ambientColor = { 0.15f, 0.18f, 0.22f };
        m_CurrentScene.showGrid = true;
        
        m_SceneObjects.clear();

        // 7x7 球体阵列
        const int gridSize = 7;
        const float spacing = 1.3f;
        const float startX = -spacing * (gridSize - 1) / 2.0f;
        const float startZ = -spacing * (gridSize - 1) / 2.0f + 5.0f;

        for (int row = 0; row < gridSize; ++row)
        {
            for (int col = 0; col < gridSize; ++col)
            {
                SceneObject obj;
                obj.mesh = m_SphereMesh.get();
                
                float x = startX + col * spacing;
                float z = startZ + row * spacing;
                XMStoreFloat4x4(&obj.transform, XMMatrixTranslation(x, 0.5f, z));
                
                obj.color = { 1.0f, 0.85f, 0.57f, 1.0f };  // 金色
                obj.metallic = static_cast<float>(col) / (gridSize - 1);
                obj.roughness = 0.05f + static_cast<float>(row) / (gridSize - 1) * 0.95f;
                obj.ao = 1.0f;
                
                m_SceneObjects.push_back(obj);
            }
        }

        // 金属材质展示球
        auto addMetalSphere = [&](float x, float z, XMFLOAT4 color, float roughness) {
            SceneObject obj;
            obj.mesh = m_SphereMesh.get();
            XMStoreFloat4x4(&obj.transform, 
                XMMatrixScaling(1.5f, 1.5f, 1.5f) * XMMatrixTranslation(x, 0.75f, z));
            obj.color = color;
            obj.metallic = 1.0f;
            obj.roughness = roughness;
            obj.ao = 1.0f;
            m_SceneObjects.push_back(obj);
        };

        // Gold, Silver, Copper, Iron
        addMetalSphere(-6.0f, 0.0f, { 1.0f, 0.766f, 0.336f, 1.0f }, 0.3f);
        addMetalSphere(-3.5f, 0.0f, { 0.972f, 0.960f, 0.915f, 1.0f }, 0.2f);
        addMetalSphere(-6.0f, -2.5f, { 0.955f, 0.638f, 0.538f, 1.0f }, 0.35f);
        addMetalSphere(-3.5f, -2.5f, { 0.56f, 0.57f, 0.58f, 1.0f }, 0.4f);

        // 非金属材质球
        auto addDielectricSphere = [&](float x, float z, XMFLOAT4 color, float roughness) {
            SceneObject obj;
            obj.mesh = m_SphereMesh.get();
            XMStoreFloat4x4(&obj.transform, 
                XMMatrixScaling(1.5f, 1.5f, 1.5f) * XMMatrixTranslation(x, 0.75f, z));
            obj.color = color;
            obj.metallic = 0.0f;
            obj.roughness = roughness;
            obj.ao = 1.0f;
            m_SceneObjects.push_back(obj);
        };

        // Red plastic, Blue ceramic, Green rubber, White marble
        addDielectricSphere(6.0f, 0.0f, { 0.9f, 0.1f, 0.1f, 1.0f }, 0.4f);
        addDielectricSphere(3.5f, 0.0f, { 0.2f, 0.4f, 0.8f, 1.0f }, 0.1f);
        addDielectricSphere(6.0f, -2.5f, { 0.2f, 0.7f, 0.3f, 1.0f }, 0.8f);
        addDielectricSphere(3.5f, -2.5f, { 0.95f, 0.93f, 0.88f, 1.0f }, 0.15f);

        m_CurrentSceneIndex = -1;  // 表示这是内置场景
        
        SEA_CORE_INFO("Created PBR Demo scene with {} objects", m_SceneObjects.size());
        
        if (m_OnSceneChanged)
        {
            m_OnSceneChanged(m_CurrentScene.name);
        }
    }

    void SceneManager::NextScene()
    {
        if (m_SceneFiles.empty()) return;
        
        int nextIndex = (m_CurrentSceneIndex + 1) % static_cast<int>(m_SceneFiles.size());
        LoadScene(nextIndex);
    }

    void SceneManager::PreviousScene()
    {
        if (m_SceneFiles.empty()) return;
        
        int prevIndex = m_CurrentSceneIndex - 1;
        if (prevIndex < 0) prevIndex = static_cast<int>(m_SceneFiles.size()) - 1;
        LoadScene(prevIndex);
    }

    const std::string& SceneManager::GetCurrentSceneName() const
    {
        static std::string empty = "No Scene";
        if (m_CurrentSceneIndex >= 0 && m_CurrentSceneIndex < static_cast<int>(m_SceneNames.size()))
        {
            return m_SceneNames[m_CurrentSceneIndex];
        }
        return m_CurrentScene.name.empty() ? empty : m_CurrentScene.name;
    }

    void SceneManager::ApplyToRenderer(SimpleRenderer& renderer)
    {
        if (!m_CurrentScene.lights.empty())
        {
            const auto& light = m_CurrentScene.lights[0];
            renderer.SetLightDirection(light.direction);
            renderer.SetLightColor(light.color);
            renderer.SetLightIntensity(light.intensity);
        }
        renderer.SetAmbientColor(m_CurrentScene.ambientColor);
    }

    void SceneManager::ApplyToCamera(Camera& camera)
    {
        camera.SetPosition(m_CurrentScene.camera.position);
        camera.LookAt(m_CurrentScene.camera.target);
        // FOV 需要通过 SetPerspective 设置，保持现有宽高比和近远平面
        float aspectRatio = 1920.0f / 1080.0f;  // 默认宽高比
        camera.SetPerspective(m_CurrentScene.camera.fov, aspectRatio, camera.GetNearZ(), camera.GetFarZ());
    }

    Mesh* SceneManager::CreateMeshFromDef(const SceneObjectDef& def)
    {
        // 内置几何体
        if (def.meshType == "sphere") return m_SphereMesh.get();
        if (def.meshType == "cube") return m_CubeMesh.get();
        if (def.meshType == "plane") return m_PlaneMesh.get();
        if (def.meshType == "torus") return m_TorusMesh.get();
        
        // 外部 OBJ 文件
        if (def.meshType == "mesh" && !def.meshPath.empty())
        {
            // 检查缓存
            auto it = m_MeshCache.find(def.meshPath);
            if (it != m_MeshCache.end())
            {
                return it->second.get();
            }
            
            // 加载新网格
            auto mesh = MakeScope<Mesh>();
            if (mesh->LoadFromOBJ(m_Device, def.meshPath))
            {
                Mesh* ptr = mesh.get();
                m_MeshCache[def.meshPath] = std::move(mesh);
                return ptr;
            }
            else
            {
                SEA_CORE_ERROR("Failed to load mesh: {}", def.meshPath);
                return m_SphereMesh.get();  // 回退到球体
            }
        }
        
        return m_SphereMesh.get();  // 默认返回球体
    }

    XMMATRIX SceneManager::ComputeTransform(const SceneObjectDef& def)
    {
        // 角度转弧度
        float radX = def.rotation.x * 3.14159265f / 180.0f;
        float radY = def.rotation.y * 3.14159265f / 180.0f;
        float radZ = def.rotation.z * 3.14159265f / 180.0f;
        
        return XMMatrixScaling(def.scale.x, def.scale.y, def.scale.z) *
               XMMatrixRotationRollPitchYaw(radX, radY, radZ) *
               XMMatrixTranslation(def.position.x, def.position.y, def.position.z);
    }
}
