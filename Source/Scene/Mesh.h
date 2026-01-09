#pragma once

#include "Core/Types.h"
#include "Graphics/Buffer.h"
#include <DirectXMath.h>
#include <vector>
#include <string>

namespace Sea
{
    using namespace DirectX;

    struct Vertex
    {
        XMFLOAT3 position;
        XMFLOAT3 normal;
        XMFLOAT2 texCoord;
        XMFLOAT4 color;
    };

    struct SubMesh
    {
        u32 indexOffset = 0;
        u32 indexCount = 0;
        u32 materialIndex = 0;
    };

    struct Material
    {
        std::string name;
        XMFLOAT4 albedo = { 1.0f, 1.0f, 1.0f, 1.0f };
        f32 metallic = 0.0f;
        f32 roughness = 0.5f;
        std::string albedoTexture;
        std::string normalTexture;
    };

    class Device;

    class Mesh : public NonCopyable
    {
    public:
        Mesh() = default;
        ~Mesh() = default;

        bool LoadFromOBJ(Device& device, const std::string& filepath);
        bool CreateFromVertices(Device& device, 
                               const std::vector<Vertex>& vertices, 
                               const std::vector<u32>& indices);

        // 几何体生成
        static Scope<Mesh> CreateCube(Device& device, f32 size = 1.0f);
        static Scope<Mesh> CreateSphere(Device& device, f32 radius = 1.0f, u32 slices = 32, u32 stacks = 16);
        static Scope<Mesh> CreatePlane(Device& device, f32 width = 10.0f, f32 depth = 10.0f);
        static Scope<Mesh> CreateTorus(Device& device, f32 outerRadius = 1.0f, f32 innerRadius = 0.3f, u32 sides = 32, u32 rings = 24);

        Buffer* GetVertexBuffer() const { return m_VertexBuffer.get(); }
        Buffer* GetIndexBuffer() const { return m_IndexBuffer.get(); }
        
        u32 GetVertexCount() const { return m_VertexCount; }
        u32 GetIndexCount() const { return m_IndexCount; }
        u32 GetVertexStride() const { return sizeof(Vertex); }
        
        const std::vector<SubMesh>& GetSubMeshes() const { return m_SubMeshes; }
        const std::vector<Material>& GetMaterials() const { return m_Materials; }

        const XMFLOAT3& GetBoundsMin() const { return m_BoundsMin; }
        const XMFLOAT3& GetBoundsMax() const { return m_BoundsMax; }

    private:
        void CalculateBounds(const std::vector<Vertex>& vertices);

    private:
        Scope<Buffer> m_VertexBuffer;
        Scope<Buffer> m_IndexBuffer;
        u32 m_VertexCount = 0;
        u32 m_IndexCount = 0;

        std::vector<SubMesh> m_SubMeshes;
        std::vector<Material> m_Materials;

        XMFLOAT3 m_BoundsMin = { 0, 0, 0 };
        XMFLOAT3 m_BoundsMax = { 0, 0, 0 };
    };
}
