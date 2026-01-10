#include "Scene/SceneFile.h"
#include "Core/Log.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>

namespace Sea
{
    using json = nlohmann::json;
    namespace fs = std::filesystem;

    // JSON 序列化辅助函数
    static void to_json(json& j, const XMFLOAT3& v)
    {
        j = json::array({ v.x, v.y, v.z });
    }

    static void from_json(const json& j, XMFLOAT3& v)
    {
        if (j.is_array() && j.size() >= 3)
        {
            v.x = j[0].get<float>();
            v.y = j[1].get<float>();
            v.z = j[2].get<float>();
        }
    }

    static void to_json(json& j, const XMFLOAT4& v)
    {
        j = json::array({ v.x, v.y, v.z, v.w });
    }

    static void from_json(const json& j, XMFLOAT4& v)
    {
        if (j.is_array() && j.size() >= 4)
        {
            v.x = j[0].get<float>();
            v.y = j[1].get<float>();
            v.z = j[2].get<float>();
            v.w = j[3].get<float>();
        }
        else if (j.is_array() && j.size() >= 3)
        {
            v.x = j[0].get<float>();
            v.y = j[1].get<float>();
            v.z = j[2].get<float>();
            v.w = 1.0f;
        }
    }

    bool SceneFile::Load(const std::string& filepath, SceneDef& outScene)
    {
        std::ifstream file(filepath);
        if (!file.is_open())
        {
            SEA_CORE_ERROR("Failed to open scene file: {}", filepath);
            return false;
        }

        try
        {
            json root;
            file >> root;

            // 基本信息
            outScene.name = root.value("name", "Untitled");
            outScene.description = root.value("description", "");
            outScene.author = root.value("author", "");

            // 相机
            if (root.contains("camera"))
            {
                auto& cam = root["camera"];
                if (cam.contains("position")) from_json(cam["position"], outScene.camera.position);
                if (cam.contains("target")) from_json(cam["target"], outScene.camera.target);
                outScene.camera.fov = cam.value("fov", 60.0f);
                outScene.camera.nearPlane = cam.value("near", 0.1f);
                outScene.camera.farPlane = cam.value("far", 1000.0f);
            }

            // 环境
            if (root.contains("environment"))
            {
                auto& env = root["environment"];
                if (env.contains("ambient")) from_json(env["ambient"], outScene.ambientColor);
                if (env.contains("clearColor")) from_json(env["clearColor"], outScene.clearColor);
                outScene.showGrid = env.value("showGrid", true);
            }

            // 光源
            if (root.contains("lights") && root["lights"].is_array())
            {
                for (auto& lightJson : root["lights"])
                {
                    LightDef light;
                    light.type = lightJson.value("type", "directional");
                    if (lightJson.contains("position")) from_json(lightJson["position"], light.position);
                    if (lightJson.contains("direction")) from_json(lightJson["direction"], light.direction);
                    if (lightJson.contains("color")) from_json(lightJson["color"], light.color);
                    light.intensity = lightJson.value("intensity", 1.0f);
                    outScene.lights.push_back(light);
                }
            }

            // 对象
            if (root.contains("objects") && root["objects"].is_array())
            {
                for (auto& objJson : root["objects"])
                {
                    SceneObjectDef obj;
                    obj.name = objJson.value("name", "Object");
                    obj.meshType = objJson.value("type", "sphere");
                    obj.meshPath = objJson.value("mesh", "");
                    
                    if (objJson.contains("position")) from_json(objJson["position"], obj.position);
                    if (objJson.contains("rotation")) from_json(objJson["rotation"], obj.rotation);
                    if (objJson.contains("scale")) from_json(objJson["scale"], obj.scale);
                    
                    // 材质
                    if (objJson.contains("material"))
                    {
                        auto& mat = objJson["material"];
                        if (mat.contains("color")) from_json(mat["color"], obj.color);
                        obj.metallic = mat.value("metallic", 0.0f);
                        obj.roughness = mat.value("roughness", 0.5f);
                        obj.ao = mat.value("ao", 1.0f);
                        if (mat.contains("emissive")) from_json(mat["emissive"], obj.emissiveColor);
                        obj.emissiveIntensity = mat.value("emissiveIntensity", 0.0f);
                    }
                    
                    outScene.objects.push_back(obj);
                }
            }

            SEA_CORE_INFO("Loaded scene: {} ({} objects)", outScene.name, outScene.objects.size());
            return true;
        }
        catch (const std::exception& e)
        {
            SEA_CORE_ERROR("Failed to parse scene file {}: {}", filepath, e.what());
            return false;
        }
    }

    bool SceneFile::Save(const std::string& filepath, const SceneDef& scene)
    {
        json root;

        // 基本信息
        root["name"] = scene.name;
        root["description"] = scene.description;
        root["author"] = scene.author;
        root["version"] = "1.0";

        // 相机
        json camera;
        to_json(camera["position"], scene.camera.position);
        to_json(camera["target"], scene.camera.target);
        camera["fov"] = scene.camera.fov;
        camera["near"] = scene.camera.nearPlane;
        camera["far"] = scene.camera.farPlane;
        root["camera"] = camera;

        // 环境
        json env;
        to_json(env["ambient"], scene.ambientColor);
        to_json(env["clearColor"], scene.clearColor);
        env["showGrid"] = scene.showGrid;
        root["environment"] = env;

        // 光源
        json lights = json::array();
        for (const auto& light : scene.lights)
        {
            json l;
            l["type"] = light.type;
            to_json(l["position"], light.position);
            to_json(l["direction"], light.direction);
            to_json(l["color"], light.color);
            l["intensity"] = light.intensity;
            lights.push_back(l);
        }
        root["lights"] = lights;

        // 对象
        json objects = json::array();
        for (const auto& obj : scene.objects)
        {
            json o;
            o["name"] = obj.name;
            o["type"] = obj.meshType;
            if (!obj.meshPath.empty())
                o["mesh"] = obj.meshPath;
            
            to_json(o["position"], obj.position);
            to_json(o["rotation"], obj.rotation);
            to_json(o["scale"], obj.scale);
            
            json mat;
            to_json(mat["color"], obj.color);
            mat["metallic"] = obj.metallic;
            mat["roughness"] = obj.roughness;
            mat["ao"] = obj.ao;
            to_json(mat["emissive"], obj.emissiveColor);
            mat["emissiveIntensity"] = obj.emissiveIntensity;
            o["material"] = mat;
            
            objects.push_back(o);
        }
        root["objects"] = objects;

        // 写入文件
        std::ofstream file(filepath);
        if (!file.is_open())
        {
            SEA_CORE_ERROR("Failed to create scene file: {}", filepath);
            return false;
        }

        file << root.dump(2);  // 格式化输出
        SEA_CORE_INFO("Saved scene: {}", filepath);
        return true;
    }

    std::vector<std::string> SceneFile::GetSceneFiles(const std::string& directory)
    {
        std::vector<std::string> scenes;
        
        try
        {
            for (const auto& entry : fs::directory_iterator(directory))
            {
                if (entry.is_regular_file())
                {
                    std::string ext = entry.path().extension().string();
                    if (ext == ".iworld" || ext == ".IWORLD")
                    {
                        scenes.push_back(entry.path().string());
                    }
                }
            }
        }
        catch (const std::exception& e)
        {
            SEA_CORE_WARN("Failed to scan scene directory {}: {}", directory, e.what());
        }
        
        std::sort(scenes.begin(), scenes.end());
        return scenes;
    }
}
