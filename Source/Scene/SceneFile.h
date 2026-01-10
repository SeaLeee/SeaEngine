#pragma once

#include "Core/Types.h"
#include "Scene/Mesh.h"
#include "Graphics/Material.h"
#include <DirectXMath.h>
#include <string>
#include <vector>
#include <unordered_map>

namespace Sea
{
    using namespace DirectX;

    // 场景对象定义
    struct SceneObjectDef
    {
        std::string name;
        std::string meshPath;           // OBJ 文件路径
        std::string meshType;           // "sphere", "cube", "plane", "torus", "mesh"
        XMFLOAT3 position = { 0, 0, 0 };
        XMFLOAT3 rotation = { 0, 0, 0 };  // 欧拉角（度）
        XMFLOAT3 scale = { 1, 1, 1 };
        
        // 材质属性
        XMFLOAT4 color = { 1, 1, 1, 1 };
        f32 metallic = 0.0f;
        f32 roughness = 0.5f;
        f32 ao = 1.0f;
        XMFLOAT3 emissiveColor = { 0, 0, 0 };
        f32 emissiveIntensity = 0.0f;
    };

    // 光源定义
    struct LightDef
    {
        std::string type = "directional";  // "directional", "point", "spot"
        XMFLOAT3 position = { 0, 10, 0 };
        XMFLOAT3 direction = { -0.5f, -1.0f, 0.5f };
        XMFLOAT3 color = { 1, 1, 1 };
        f32 intensity = 1.0f;
    };

    // 相机定义
    struct CameraDef
    {
        XMFLOAT3 position = { 0, 5, -10 };
        XMFLOAT3 target = { 0, 0, 0 };
        f32 fov = 60.0f;
        f32 nearPlane = 0.1f;
        f32 farPlane = 1000.0f;
    };

    // 场景定义
    struct SceneDef
    {
        std::string name;
        std::string description;
        std::string author;
        
        CameraDef camera;
        std::vector<LightDef> lights;
        std::vector<SceneObjectDef> objects;
        
        // 环境设置
        XMFLOAT3 ambientColor = { 0.1f, 0.1f, 0.15f };
        XMFLOAT4 clearColor = { 0.1f, 0.1f, 0.15f, 1.0f };
        bool showGrid = true;
    };

    class Device;
    class Camera;

    // 场景文件读写器
    class SceneFile
    {
    public:
        // 加载 .iworld 场景文件
        static bool Load(const std::string& filepath, SceneDef& outScene);
        
        // 保存为 .iworld 场景文件
        static bool Save(const std::string& filepath, const SceneDef& scene);
        
        // 获取场景文件列表
        static std::vector<std::string> GetSceneFiles(const std::string& directory);
    };
}
