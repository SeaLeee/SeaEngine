#pragma once
#include "Core/Types.h"
#include "Graphics/Device.h"
#include "Graphics/Buffer.h"
#include "Scene/Mesh.h"
#include "Scene/Camera.h"
#include <DirectXMath.h>
#include <vector>

namespace Sea
{
    using namespace DirectX;

    // 四叉树节点 - 代表海面的一个区块
    struct OceanQuadNode
    {
        XMFLOAT2 center;       // 节点中心位置
        f32 size;              // 节点大小（边长）
        u32 lod;               // LOD 级别 (0 = 最高精度)
        bool isLeaf;           // 是否是叶子节点
        u32 childIndices[4];   // 子节点索引 (如果不是叶子)
        
        // 边界检测用
        bool inFrustum;        // 是否在视锥体内
        f32 distanceToCamera;  // 到相机的距离
    };

    // 四叉树配置
    struct OceanQuadTreeConfig
    {
        f32 worldSize = 4000.0f;        // 海面总大小（米）
        u32 maxLOD = 6;                 // 最大 LOD 级别 (0-6, 共7级)
        u32 baseMeshResolution = 32;    // 基础网格分辨率 (每个节点的顶点数)
        f32 lodDistanceMultiplier = 2.0f;  // LOD 距离倍增系数
        f32 lodBaseDistance = 50.0f;    // LOD 0 的距离阈值
        bool enableMorphing = true;     // 是否启用 LOD 过渡（变形）
        f32 morphRange = 0.3f;          // 变形范围 (0-1, 节点大小的比例)
    };

    // 渲染数据 - 传递给 GPU
    struct OceanQuadInstance
    {
        XMFLOAT4 positionScale;  // xyz = 世界位置, w = 缩放
        XMFLOAT4 lodMorph;       // x = LOD, y = morph factor, zw = reserved
        XMFLOAT4 neighborLOD;    // 四个邻居的 LOD (用于缝合)
    };

    class OceanQuadTree
    {
    public:
        OceanQuadTree(Device& device);
        ~OceanQuadTree() = default;

        bool Initialize(const OceanQuadTreeConfig& config = OceanQuadTreeConfig());
        void Shutdown();

        // 每帧更新 - 根据相机位置重建四叉树
        void Update(const Camera& camera);

        // 获取渲染实例
        const std::vector<OceanQuadInstance>& GetRenderInstances() const { return m_RenderInstances; }
        u32 GetInstanceCount() const { return static_cast<u32>(m_RenderInstances.size()); }

        // 获取基础网格 (所有实例共享)
        Mesh* GetBaseMesh() const { return m_BaseMesh.get(); }

        // 获取实例缓冲区
        Buffer* GetInstanceBuffer() const { return m_InstanceBuffer.get(); }

        // 配置
        OceanQuadTreeConfig& GetConfig() { return m_Config; }
        const OceanQuadTreeConfig& GetConfig() const { return m_Config; }

        // 调试信息
        u32 GetNodeCount() const { return static_cast<u32>(m_Nodes.size()); }
        u32 GetLeafCount() const;

    private:
        void BuildTree(const Camera& camera);
        void SubdivideNode(u32 nodeIndex, const Camera& camera);
        bool ShouldSubdivide(const OceanQuadNode& node, const Camera& camera) const;
        f32 CalculateLODDistance(u32 lod) const;
        void CollectLeafNodes();
        void CalculateNeighborLODs();
        void UpdateInstanceBuffer();
        bool CreateBaseMesh();

        // 视锥体剔除
        bool IsInFrustum(const XMFLOAT2& center, f32 size, const Camera& camera) const;

    private:
        Device& m_Device;
        OceanQuadTreeConfig m_Config;

        // 节点池
        std::vector<OceanQuadNode> m_Nodes;
        u32 m_RootNodeIndex = 0;

        // 叶子节点索引 (用于渲染)
        std::vector<u32> m_LeafNodeIndices;

        // 渲染实例数据
        std::vector<OceanQuadInstance> m_RenderInstances;

        // GPU 资源
        Scope<Mesh> m_BaseMesh;           // 基础网格 (所有实例共享)
        Scope<Buffer> m_InstanceBuffer;   // 实例数据缓冲区

        // 缓存的相机数据
        XMFLOAT3 m_LastCameraPos = { 0, 0, 0 };
        XMFLOAT4X4 m_LastViewProj = {};
        bool m_NeedsRebuild = true;
    };
}
