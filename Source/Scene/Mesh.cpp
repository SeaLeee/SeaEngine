#include "Scene/Mesh.h"
#include "Graphics/Device.h"
#include "Core/Log.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include <unordered_map>
#include <cmath>

namespace Sea
{
    bool Mesh::LoadFromOBJ(Device& device, const std::string& filepath)
    {
        tinyobj::attrib_t attrib;
        std::vector<tinyobj::shape_t> shapes;
        std::vector<tinyobj::material_t> materials;
        std::string warn, err;

        // 获取目录路径用于材质加载
        std::string directory = filepath.substr(0, filepath.find_last_of("/\\") + 1);

        if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filepath.c_str(), directory.c_str()))
        {
            SEA_CORE_ERROR("Failed to load OBJ: {} {}", warn, err);
            return false;
        }

        if (!warn.empty())
            SEA_CORE_WARN("OBJ Warning: {}", warn);

        std::vector<Vertex> vertices;
        std::vector<u32> indices;
        std::unordered_map<std::string, u32> uniqueVertices;

        // 加载材质
        for (const auto& mat : materials)
        {
            Material m;
            m.name = mat.name;
            m.albedo = { mat.diffuse[0], mat.diffuse[1], mat.diffuse[2], 1.0f };
            m.metallic = mat.metallic;
            m.roughness = mat.roughness;
            m.albedoTexture = mat.diffuse_texname;
            m.normalTexture = mat.normal_texname;
            m_Materials.push_back(m);
        }

        // 如果没有材质，添加默认材质
        if (m_Materials.empty())
        {
            Material defaultMat;
            defaultMat.name = "Default";
            m_Materials.push_back(defaultMat);
        }

        // 处理每个形状
        for (const auto& shape : shapes)
        {
            SubMesh subMesh;
            subMesh.indexOffset = static_cast<u32>(indices.size());
            subMesh.materialIndex = 0;

            for (const auto& index : shape.mesh.indices)
            {
                Vertex vertex{};

                // 位置
                vertex.position = {
                    attrib.vertices[3 * index.vertex_index + 0],
                    attrib.vertices[3 * index.vertex_index + 1],
                    attrib.vertices[3 * index.vertex_index + 2]
                };

                // 法线
                if (index.normal_index >= 0)
                {
                    vertex.normal = {
                        attrib.normals[3 * index.normal_index + 0],
                        attrib.normals[3 * index.normal_index + 1],
                        attrib.normals[3 * index.normal_index + 2]
                    };
                }

                // 纹理坐标
                if (index.texcoord_index >= 0)
                {
                    vertex.texCoord = {
                        attrib.texcoords[2 * index.texcoord_index + 0],
                        1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
                    };
                }

                // 顶点颜色
                vertex.color = { 1.0f, 1.0f, 1.0f, 1.0f };

                // 使用唯一顶点
                std::string key = std::to_string(index.vertex_index) + "_" + 
                                  std::to_string(index.normal_index) + "_" + 
                                  std::to_string(index.texcoord_index);

                if (uniqueVertices.count(key) == 0)
                {
                    uniqueVertices[key] = static_cast<u32>(vertices.size());
                    vertices.push_back(vertex);
                }

                indices.push_back(uniqueVertices[key]);
            }

            subMesh.indexCount = static_cast<u32>(indices.size()) - subMesh.indexOffset;
            if (!shape.mesh.material_ids.empty() && shape.mesh.material_ids[0] >= 0)
            {
                subMesh.materialIndex = shape.mesh.material_ids[0];
            }
            m_SubMeshes.push_back(subMesh);
        }

        SEA_CORE_INFO("Loaded OBJ: {} vertices, {} indices, {} submeshes", 
                      vertices.size(), indices.size(), m_SubMeshes.size());

        return CreateFromVertices(device, vertices, indices);
    }

    bool Mesh::CreateFromVertices(Device& device, 
                                  const std::vector<Vertex>& vertices, 
                                  const std::vector<u32>& indices)
    {
        m_VertexCount = static_cast<u32>(vertices.size());
        m_IndexCount = static_cast<u32>(indices.size());

        // 创建顶点缓冲
        BufferDesc vbDesc{};
        vbDesc.size = vertices.size() * sizeof(Vertex);
        vbDesc.stride = sizeof(Vertex);
        vbDesc.type = BufferType::Vertex;
        
        m_VertexBuffer = MakeScope<Buffer>(device, vbDesc);
        if (!m_VertexBuffer->Initialize(vertices.data()))
        {
            SEA_CORE_ERROR("Failed to create vertex buffer");
            return false;
        }

        // 创建索引缓冲
        BufferDesc ibDesc{};
        ibDesc.size = indices.size() * sizeof(u32);
        ibDesc.stride = sizeof(u32);
        ibDesc.type = BufferType::Index;
        
        m_IndexBuffer = MakeScope<Buffer>(device, ibDesc);
        if (!m_IndexBuffer->Initialize(indices.data()))
        {
            SEA_CORE_ERROR("Failed to create index buffer");
            return false;
        }

        // 如果没有子网格，创建一个
        if (m_SubMeshes.empty())
        {
            SubMesh subMesh;
            subMesh.indexOffset = 0;
            subMesh.indexCount = m_IndexCount;
            subMesh.materialIndex = 0;
            m_SubMeshes.push_back(subMesh);
        }

        // 如果没有材质，创建默认材质
        if (m_Materials.empty())
        {
            Material defaultMat;
            defaultMat.name = "Default";
            m_Materials.push_back(defaultMat);
        }

        CalculateBounds(vertices);
        return true;
    }

    void Mesh::CalculateBounds(const std::vector<Vertex>& vertices)
    {
        if (vertices.empty()) return;

        m_BoundsMin = m_BoundsMax = vertices[0].position;

        for (const auto& v : vertices)
        {
            m_BoundsMin.x = std::min(m_BoundsMin.x, v.position.x);
            m_BoundsMin.y = std::min(m_BoundsMin.y, v.position.y);
            m_BoundsMin.z = std::min(m_BoundsMin.z, v.position.z);
            m_BoundsMax.x = std::max(m_BoundsMax.x, v.position.x);
            m_BoundsMax.y = std::max(m_BoundsMax.y, v.position.y);
            m_BoundsMax.z = std::max(m_BoundsMax.z, v.position.z);
        }
    }

    Scope<Mesh> Mesh::CreateCube(Device& device, f32 size)
    {
        f32 h = size * 0.5f;

        std::vector<Vertex> vertices = {
            // Front face
            {{ -h, -h,  h }, {  0,  0,  1 }, { 0, 1 }, { 1, 1, 1, 1 }},
            {{  h, -h,  h }, {  0,  0,  1 }, { 1, 1 }, { 1, 1, 1, 1 }},
            {{  h,  h,  h }, {  0,  0,  1 }, { 1, 0 }, { 1, 1, 1, 1 }},
            {{ -h,  h,  h }, {  0,  0,  1 }, { 0, 0 }, { 1, 1, 1, 1 }},
            // Back face
            {{  h, -h, -h }, {  0,  0, -1 }, { 0, 1 }, { 1, 1, 1, 1 }},
            {{ -h, -h, -h }, {  0,  0, -1 }, { 1, 1 }, { 1, 1, 1, 1 }},
            {{ -h,  h, -h }, {  0,  0, -1 }, { 1, 0 }, { 1, 1, 1, 1 }},
            {{  h,  h, -h }, {  0,  0, -1 }, { 0, 0 }, { 1, 1, 1, 1 }},
            // Top face
            {{ -h,  h,  h }, {  0,  1,  0 }, { 0, 1 }, { 1, 1, 1, 1 }},
            {{  h,  h,  h }, {  0,  1,  0 }, { 1, 1 }, { 1, 1, 1, 1 }},
            {{  h,  h, -h }, {  0,  1,  0 }, { 1, 0 }, { 1, 1, 1, 1 }},
            {{ -h,  h, -h }, {  0,  1,  0 }, { 0, 0 }, { 1, 1, 1, 1 }},
            // Bottom face
            {{ -h, -h, -h }, {  0, -1,  0 }, { 0, 1 }, { 1, 1, 1, 1 }},
            {{  h, -h, -h }, {  0, -1,  0 }, { 1, 1 }, { 1, 1, 1, 1 }},
            {{  h, -h,  h }, {  0, -1,  0 }, { 1, 0 }, { 1, 1, 1, 1 }},
            {{ -h, -h,  h }, {  0, -1,  0 }, { 0, 0 }, { 1, 1, 1, 1 }},
            // Right face
            {{  h, -h,  h }, {  1,  0,  0 }, { 0, 1 }, { 1, 1, 1, 1 }},
            {{  h, -h, -h }, {  1,  0,  0 }, { 1, 1 }, { 1, 1, 1, 1 }},
            {{  h,  h, -h }, {  1,  0,  0 }, { 1, 0 }, { 1, 1, 1, 1 }},
            {{  h,  h,  h }, {  1,  0,  0 }, { 0, 0 }, { 1, 1, 1, 1 }},
            // Left face
            {{ -h, -h, -h }, { -1,  0,  0 }, { 0, 1 }, { 1, 1, 1, 1 }},
            {{ -h, -h,  h }, { -1,  0,  0 }, { 1, 1 }, { 1, 1, 1, 1 }},
            {{ -h,  h,  h }, { -1,  0,  0 }, { 1, 0 }, { 1, 1, 1, 1 }},
            {{ -h,  h, -h }, { -1,  0,  0 }, { 0, 0 }, { 1, 1, 1, 1 }},
        };

        std::vector<u32> indices = {
            0, 1, 2, 0, 2, 3,       // Front
            4, 5, 6, 4, 6, 7,       // Back
            8, 9, 10, 8, 10, 11,    // Top
            12, 13, 14, 12, 14, 15, // Bottom
            16, 17, 18, 16, 18, 19, // Right
            20, 21, 22, 20, 22, 23  // Left
        };

        auto mesh = MakeScope<Mesh>();
        if (!mesh->CreateFromVertices(device, vertices, indices))
            return nullptr;
        return mesh;
    }

    Scope<Mesh> Mesh::CreateSphere(Device& device, f32 radius, u32 slices, u32 stacks)
    {
        std::vector<Vertex> vertices;
        std::vector<u32> indices;

        const f32 PI = 3.14159265359f;

        for (u32 i = 0; i <= stacks; ++i)
        {
            f32 phi = PI * static_cast<f32>(i) / static_cast<f32>(stacks);
            f32 y = radius * std::cos(phi);
            f32 r = radius * std::sin(phi);

            for (u32 j = 0; j <= slices; ++j)
            {
                f32 theta = 2.0f * PI * static_cast<f32>(j) / static_cast<f32>(slices);
                f32 x = r * std::cos(theta);
                f32 z = r * std::sin(theta);

                Vertex v;
                v.position = { x, y, z };
                v.normal = { x / radius, y / radius, z / radius };
                v.texCoord = { static_cast<f32>(j) / slices, static_cast<f32>(i) / stacks };
                v.color = { 1, 1, 1, 1 };
                vertices.push_back(v);
            }
        }

        for (u32 i = 0; i < stacks; ++i)
        {
            for (u32 j = 0; j < slices; ++j)
            {
                u32 first = i * (slices + 1) + j;
                u32 second = first + slices + 1;

                indices.push_back(first);
                indices.push_back(second);
                indices.push_back(first + 1);

                indices.push_back(second);
                indices.push_back(second + 1);
                indices.push_back(first + 1);
            }
        }

        auto mesh = MakeScope<Mesh>();
        if (!mesh->CreateFromVertices(device, vertices, indices))
            return nullptr;
        return mesh;
    }

    Scope<Mesh> Mesh::CreatePlane(Device& device, f32 width, f32 depth)
    {
        f32 hw = width * 0.5f;
        f32 hd = depth * 0.5f;

        std::vector<Vertex> vertices = {
            {{ -hw, 0, -hd }, { 0, 1, 0 }, { 0, 0 }, { 1, 1, 1, 1 }},
            {{  hw, 0, -hd }, { 0, 1, 0 }, { 1, 0 }, { 1, 1, 1, 1 }},
            {{  hw, 0,  hd }, { 0, 1, 0 }, { 1, 1 }, { 1, 1, 1, 1 }},
            {{ -hw, 0,  hd }, { 0, 1, 0 }, { 0, 1 }, { 1, 1, 1, 1 }},
        };

        std::vector<u32> indices = { 0, 2, 1, 0, 3, 2 };

        auto mesh = MakeScope<Mesh>();
        if (!mesh->CreateFromVertices(device, vertices, indices))
            return nullptr;
        return mesh;
    }

    Scope<Mesh> Mesh::CreateTorus(Device& device, f32 outerRadius, f32 innerRadius, u32 sides, u32 rings)
    {
        std::vector<Vertex> vertices;
        std::vector<u32> indices;

        const f32 PI = 3.14159265359f;

        for (u32 i = 0; i <= rings; ++i)
        {
            f32 phi = 2.0f * PI * static_cast<f32>(i) / static_cast<f32>(rings);
            f32 cosPhi = std::cos(phi);
            f32 sinPhi = std::sin(phi);

            for (u32 j = 0; j <= sides; ++j)
            {
                f32 theta = 2.0f * PI * static_cast<f32>(j) / static_cast<f32>(sides);
                f32 cosTheta = std::cos(theta);
                f32 sinTheta = std::sin(theta);

                f32 x = (outerRadius + innerRadius * cosTheta) * cosPhi;
                f32 y = innerRadius * sinTheta;
                f32 z = (outerRadius + innerRadius * cosTheta) * sinPhi;

                f32 nx = cosTheta * cosPhi;
                f32 ny = sinTheta;
                f32 nz = cosTheta * sinPhi;

                Vertex v;
                v.position = { x, y, z };
                v.normal = { nx, ny, nz };
                v.texCoord = { static_cast<f32>(i) / rings, static_cast<f32>(j) / sides };
                v.color = { 1, 1, 1, 1 };
                vertices.push_back(v);
            }
        }

        for (u32 i = 0; i < rings; ++i)
        {
            for (u32 j = 0; j < sides; ++j)
            {
                u32 first = i * (sides + 1) + j;
                u32 second = first + sides + 1;

                indices.push_back(first);
                indices.push_back(second);
                indices.push_back(first + 1);

                indices.push_back(second);
                indices.push_back(second + 1);
                indices.push_back(first + 1);
            }
        }

        auto mesh = MakeScope<Mesh>();
        if (!mesh->CreateFromVertices(device, vertices, indices))
            return nullptr;
        return mesh;
    }
}
