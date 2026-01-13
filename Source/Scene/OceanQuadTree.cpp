#include "Scene/OceanQuadTree.h"
#include "Core/Log.h"
#include <algorithm>
#include <cmath>

namespace Sea
{
    // Helper function
    static inline f32 Saturate(f32 x)
    {
        return std::max(0.0f, std::min(1.0f, x));
    }

    OceanQuadTree::OceanQuadTree(Device& device)
        : m_Device(device)
    {
    }

    bool OceanQuadTree::Initialize(const OceanQuadTreeConfig& config)
    {
        m_Config = config;

        // 创建基础网格
        if (!CreateBaseMesh())
        {
            SEA_CORE_ERROR("OceanQuadTree: Failed to create base mesh");
            return false;
        }

        // 创建实例缓冲区 (预分配最大可能的实例数)
        // 最大实例数 = 4^maxLOD (最坏情况)
        u32 maxInstances = 1;
        for (u32 i = 0; i < m_Config.maxLOD; ++i)
            maxInstances *= 4;
        maxInstances = std::min(maxInstances, 4096u);  // 限制最大实例数

        BufferDesc instanceBufferDesc;
        instanceBufferDesc.size = sizeof(OceanQuadInstance) * maxInstances;
        instanceBufferDesc.type = BufferType::Structured;
        instanceBufferDesc.stride = sizeof(OceanQuadInstance);

        m_InstanceBuffer = MakeScope<Buffer>(m_Device, instanceBufferDesc);
        if (!m_InstanceBuffer->Initialize(nullptr))
        {
            SEA_CORE_ERROR("OceanQuadTree: Failed to create instance buffer");
            return false;
        }

        // 预分配节点池
        m_Nodes.reserve(maxInstances * 2);

        SEA_CORE_INFO("OceanQuadTree initialized: worldSize={}, maxLOD={}, maxInstances={}",
                      m_Config.worldSize, m_Config.maxLOD, maxInstances);
        return true;
    }

    void OceanQuadTree::Shutdown()
    {
        m_BaseMesh.reset();
        m_InstanceBuffer.reset();
        m_Nodes.clear();
        m_LeafNodeIndices.clear();
        m_RenderInstances.clear();
    }

    bool OceanQuadTree::CreateBaseMesh()
    {
        // 创建一个单位大小的网格，渲染时通过实例数据缩放和定位
        const u32 res = m_Config.baseMeshResolution;
        const f32 size = 1.0f;  // 单位大小
        const f32 halfSize = size * 0.5f;
        const f32 cellSize = size / static_cast<f32>(res);

        std::vector<Vertex> vertices;
        std::vector<u32> indices;

        // 生成顶点
        for (u32 z = 0; z <= res; ++z)
        {
            for (u32 x = 0; x <= res; ++x)
            {
                Vertex v;
                v.position = {
                    -halfSize + x * cellSize,
                    0.0f,
                    -halfSize + z * cellSize
                };
                v.normal = { 0.0f, 1.0f, 0.0f };
                v.texCoord = {
                    static_cast<f32>(x) / res,
                    static_cast<f32>(z) / res
                };
                v.color = { 1.0f, 1.0f, 1.0f, 1.0f };
                vertices.push_back(v);
            }
        }

        // 生成索引
        for (u32 z = 0; z < res; ++z)
        {
            for (u32 x = 0; x < res; ++x)
            {
                u32 topLeft = z * (res + 1) + x;
                u32 topRight = topLeft + 1;
                u32 bottomLeft = (z + 1) * (res + 1) + x;
                u32 bottomRight = bottomLeft + 1;

                indices.push_back(topLeft);
                indices.push_back(bottomLeft);
                indices.push_back(topRight);

                indices.push_back(topRight);
                indices.push_back(bottomLeft);
                indices.push_back(bottomRight);
            }
        }

        m_BaseMesh = MakeScope<Mesh>();
        if (!m_BaseMesh->CreateFromVertices(m_Device, vertices, indices))
        {
            SEA_CORE_ERROR("OceanQuadTree: Failed to create base mesh geometry");
            return false;
        }

        SEA_CORE_INFO("OceanQuadTree: Created base mesh with {} vertices, {} triangles",
                      vertices.size(), indices.size() / 3);
        return true;
    }

    void OceanQuadTree::Update(const Camera& camera)
    {
        // 检查是否需要重建
        XMFLOAT3 camPos = camera.GetPosition();
        f32 moveDist = std::sqrt(
            (camPos.x - m_LastCameraPos.x) * (camPos.x - m_LastCameraPos.x) +
            (camPos.y - m_LastCameraPos.y) * (camPos.y - m_LastCameraPos.y) +
            (camPos.z - m_LastCameraPos.z) * (camPos.z - m_LastCameraPos.z)
        );

        // 只有相机移动足够距离时才重建
        if (moveDist > m_Config.lodBaseDistance * 0.1f || m_NeedsRebuild)
        {
            BuildTree(camera);
            CollectLeafNodes();
            CalculateNeighborLODs();
            UpdateInstanceBuffer();

            m_LastCameraPos = camPos;
            m_NeedsRebuild = false;
        }
    }

    void OceanQuadTree::BuildTree(const Camera& camera)
    {
        m_Nodes.clear();

        // 创建根节点
        OceanQuadNode root;
        root.center = { 0.0f, 0.0f };
        root.size = m_Config.worldSize;
        root.lod = m_Config.maxLOD;
        root.isLeaf = true;
        root.inFrustum = true;
        root.distanceToCamera = 0.0f;

        m_Nodes.push_back(root);
        m_RootNodeIndex = 0;

        // 递归细分
        SubdivideNode(m_RootNodeIndex, camera);
    }

    void OceanQuadTree::SubdivideNode(u32 nodeIndex, const Camera& camera)
    {
        OceanQuadNode& node = m_Nodes[nodeIndex];

        // 视锥体剔除
        if (!IsInFrustum(node.center, node.size, camera))
        {
            node.inFrustum = false;
            return;
        }
        node.inFrustum = true;

        // 计算到相机的距离
        XMFLOAT3 camPos = camera.GetPosition();
        f32 dx = node.center.x - camPos.x;
        f32 dz = node.center.y - camPos.z;  // center.y 是 world Z
        node.distanceToCamera = std::sqrt(dx * dx + dz * dz);

        // 检查是否需要细分
        if (!ShouldSubdivide(node, camera))
        {
            node.isLeaf = true;
            return;
        }

        // 细分节点
        node.isLeaf = false;
        f32 childSize = node.size * 0.5f;
        f32 offset = childSize * 0.5f;

        // 四个子节点的偏移
        XMFLOAT2 offsets[4] = {
            { -offset, -offset },  // 左下
            {  offset, -offset },  // 右下
            { -offset,  offset },  // 左上
            {  offset,  offset }   // 右上
        };

        for (int i = 0; i < 4; ++i)
        {
            OceanQuadNode child;
            child.center = {
                node.center.x + offsets[i].x,
                node.center.y + offsets[i].y
            };
            child.size = childSize;
            child.lod = node.lod - 1;
            child.isLeaf = true;
            child.inFrustum = true;

            u32 childIndex = static_cast<u32>(m_Nodes.size());
            node.childIndices[i] = childIndex;
            m_Nodes.push_back(child);

            // 递归细分子节点
            SubdivideNode(childIndex, camera);
        }
    }

    bool OceanQuadTree::ShouldSubdivide(const OceanQuadNode& node, const Camera& camera) const
    {
        // 已经达到最高精度
        if (node.lod == 0)
            return false;

        // 基于距离的 LOD 判断
        f32 lodDistance = CalculateLODDistance(node.lod);
        return node.distanceToCamera < lodDistance;
    }

    f32 OceanQuadTree::CalculateLODDistance(u32 lod) const
    {
        // 每个 LOD 级别的距离阈值
        // LOD 0 = baseDistance, LOD 1 = baseDistance * multiplier, etc.
        f32 distance = m_Config.lodBaseDistance;
        for (u32 i = 0; i < lod; ++i)
        {
            distance *= m_Config.lodDistanceMultiplier;
        }
        return distance;
    }

    bool OceanQuadTree::IsInFrustum(const XMFLOAT2& center, f32 size, const Camera& camera) const
    {
        // 简单的 AABB-视锥体检测
        // 完整实现需要提取视锥体平面
        // 这里使用简化版本：基于到相机的距离
        
        XMFLOAT3 camPos = camera.GetPosition();
        f32 dx = center.x - camPos.x;
        f32 dz = center.y - camPos.z;
        f32 dist = std::sqrt(dx * dx + dz * dz);

        // 如果距离太远，剔除
        f32 maxRenderDistance = m_Config.worldSize * 1.5f;
        if (dist > maxRenderDistance + size * 0.707f)  // 对角线距离
            return false;

        return true;
    }

    void OceanQuadTree::CollectLeafNodes()
    {
        m_LeafNodeIndices.clear();
        m_RenderInstances.clear();

        for (u32 i = 0; i < m_Nodes.size(); ++i)
        {
            const OceanQuadNode& node = m_Nodes[i];
            if (node.isLeaf && node.inFrustum)
            {
                m_LeafNodeIndices.push_back(i);

                OceanQuadInstance instance;
                instance.positionScale = {
                    node.center.x,
                    0.0f,  // Y position
                    node.center.y,  // World Z
                    node.size
                };

                // 计算变形因子
                f32 lodDistance = CalculateLODDistance(node.lod);
                f32 nextLodDistance = node.lod > 0 ? CalculateLODDistance(node.lod - 1) : 0.0f;
                f32 morphStart = nextLodDistance + (lodDistance - nextLodDistance) * (1.0f - m_Config.morphRange);
                f32 morphFactor = 0.0f;
                if (m_Config.enableMorphing && node.distanceToCamera > morphStart)
                {
                    morphFactor = Saturate((node.distanceToCamera - morphStart) / (lodDistance - morphStart));
                }

                instance.lodMorph = {
                    static_cast<f32>(node.lod),
                    morphFactor,
                    0.0f,
                    0.0f
                };

                // 邻居 LOD 将在 CalculateNeighborLODs 中填充
                instance.neighborLOD = { 0.0f, 0.0f, 0.0f, 0.0f };

                m_RenderInstances.push_back(instance);
            }
        }
    }

    void OceanQuadTree::CalculateNeighborLODs()
    {
        // 为每个叶子节点计算邻居的 LOD
        // 这用于边缘缝合，防止 T-junction 裂缝
        
        for (size_t i = 0; i < m_LeafNodeIndices.size(); ++i)
        {
            u32 nodeIdx = m_LeafNodeIndices[i];
            const OceanQuadNode& node = m_Nodes[nodeIdx];
            OceanQuadInstance& instance = m_RenderInstances[i];

            // 四个方向的邻居偏移
            XMFLOAT2 neighborOffsets[4] = {
                { -node.size, 0.0f },   // 左
                {  node.size, 0.0f },   // 右
                { 0.0f, -node.size },   // 下
                { 0.0f,  node.size }    // 上
            };

            for (int dir = 0; dir < 4; ++dir)
            {
                XMFLOAT2 neighborCenter = {
                    node.center.x + neighborOffsets[dir].x,
                    node.center.y + neighborOffsets[dir].y
                };

                // 查找该位置的叶子节点
                f32 neighborLOD = static_cast<f32>(node.lod);  // 默认相同 LOD

                for (size_t j = 0; j < m_LeafNodeIndices.size(); ++j)
                {
                    if (i == j) continue;
                    const OceanQuadNode& other = m_Nodes[m_LeafNodeIndices[j]];

                    // 检查是否相邻
                    f32 dx = std::abs(other.center.x - neighborCenter.x);
                    f32 dz = std::abs(other.center.y - neighborCenter.y);
                    f32 tolerance = (node.size + other.size) * 0.5f * 0.1f;

                    if (dx < tolerance && dz < tolerance)
                    {
                        neighborLOD = static_cast<f32>(other.lod);
                        break;
                    }
                }

                // 存储邻居 LOD
                switch (dir)
                {
                case 0: instance.neighborLOD.x = neighborLOD; break;
                case 1: instance.neighborLOD.y = neighborLOD; break;
                case 2: instance.neighborLOD.z = neighborLOD; break;
                case 3: instance.neighborLOD.w = neighborLOD; break;
                }
            }
        }
    }

    void OceanQuadTree::UpdateInstanceBuffer()
    {
        if (m_RenderInstances.empty())
            return;

        m_InstanceBuffer->Update(
            m_RenderInstances.data(),
            sizeof(OceanQuadInstance) * m_RenderInstances.size()
        );
    }

    u32 OceanQuadTree::GetLeafCount() const
    {
        u32 count = 0;
        for (const auto& node : m_Nodes)
        {
            if (node.isLeaf && node.inFrustum)
                ++count;
        }
        return count;
    }
}
