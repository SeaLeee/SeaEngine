# FrameGraph 系统迁移指南

## 概述

SeaEngine 的新 FrameGraph 系统基于 Messiah 引擎的设计模式，提供了一个现代化的渲染图管理解决方案。本指南介绍如何从旧的 RenderGraph 迁移到新的 FrameGraph 系统。

## 核心概念

### 1. FrameGraph 资源句柄
```cpp
FrameGraphResourceHandle handle;
handle.id;       // 资源唯一标识
handle.version;  // 资源版本（每次写入后递增）
```

### 2. 资源描述
```cpp
// 纹理描述
FrameGraphTextureDesc textureDesc;
textureDesc.width = 1920;
textureDesc.height = 1080;
textureDesc.format = RHIFormat::R16G16B16A16_FLOAT;
textureDesc.usage = RHITextureUsage::RenderTarget | RHITextureUsage::ShaderResource;
textureDesc.name = "HDRSceneColor";

// 屏幕相对尺寸
textureDesc.useScreenSize = true;
textureDesc.screenSizeScale = 0.5f;  // 半分辨率
```

### 3. Pass 类型
- `FrameGraphPassType::Graphics` - 图形渲染 Pass
- `FrameGraphPassType::Compute` - 计算着色器 Pass
- `FrameGraphPassType::Copy` - 资源拷贝 Pass
- `FrameGraphPassType::Present` - 呈现 Pass

## 基本用法

### 创建 FrameGraph
```cpp
#include "RenderGraph/FrameGraph.h"

// 初始化
FrameGraph frameGraph;
frameGraph.Initialize(rhiDevice);
frameGraph.SetScreenSize(1920, 1080);
```

### 添加 Pass（带数据结构）
```cpp
struct MyPassData
{
    FrameGraphResourceHandle colorInput;
    FrameGraphResourceHandle colorOutput;
    FrameGraphResourceHandle depth;
};

auto& passData = frameGraph.AddPass<MyPassData>(
    "MyPass",
    FrameGraphPassType::Graphics,
    // Setup lambda - 声明资源依赖
    [](FrameGraphBuilder& builder, MyPassData& data)
    {
        // 创建输出纹理
        RHITextureDesc outputDesc;
        outputDesc.width = 1920;
        outputDesc.height = 1080;
        outputDesc.format = RHIFormat::R8G8B8A8_UNORM;
        outputDesc.usage = RHITextureUsage::RenderTarget;
        outputDesc.name = "Output";
        
        data.colorOutput = builder.CreateTexture("Output", outputDesc);
        data.colorOutput = builder.Write(data.colorOutput);
        
        // 读取深度
        data.depth = builder.UseDepthStencil(existingDepth, true);  // 只读
        
        builder.SetSideEffect(true);  // 防止被剔除
    },
    // Execute lambda - 实际渲染逻辑
    [](RHICommandList& cmdList, const MyPassData& data)
    {
        // 执行渲染命令
        cmdList.Draw(3, 1, 0, 0);
    }
);
```

### 添加简单 Pass（无数据结构）
```cpp
frameGraph.AddPassSimple(
    "SimplePass",
    FrameGraphPassType::Compute,
    [](FrameGraphBuilder& builder)
    {
        // 设置资源依赖
    },
    [](RHICommandList& cmdList, const FrameGraphPass& pass)
    {
        // 执行命令
        cmdList.Dispatch(64, 64, 1);
    }
);
```

### 标记输出资源
```cpp
// 确保产生此资源的 Pass 不会被剔除
frameGraph.MarkOutput(finalOutputHandle);
```

### 编译和执行
```cpp
// 编译图（分析依赖、剔除无用 Pass、分配资源）
if (frameGraph.Compile())
{
    // 执行渲染
    frameGraph.Execute(cmdList);
}

// 帧结束后重置
frameGraph.Reset();
```

## 延迟渲染示例

使用 `DeferredFrameGraph` 类可以快速搭建延迟渲染管线：

```cpp
#include "RenderGraph/DeferredFrameGraph.h"

DeferredFrameGraph deferredRenderer;
deferredRenderer.SetBloomEnabled(true);
deferredRenderer.SetFXAAEnabled(true);
deferredRenderer.SetShadowsEnabled(true);

// 设置渲染上下文
RenderContext context;
context.screenWidth = 1920;
context.screenHeight = 1080;
context.renderOpaqueObjects = [](RHICommandList& cmd) { /* ... */ };
context.renderSkybox = [](RHICommandList& cmd) { /* ... */ };

// 构建图
FrameGraph frameGraph;
frameGraph.Initialize(device);
deferredRenderer.Setup(frameGraph, context);

// 编译并执行
frameGraph.Compile();
frameGraph.Execute(cmdList);
```

## 资源生命周期管理

FrameGraph 自动管理临时资源的生命周期：

1. **资源创建**：在 Pass 的 Setup 阶段声明所需资源
2. **资源分配**：编译时根据生命周期分配物理资源
3. **资源复用**：具有非重叠生命周期的资源可以复用同一物理资源
4. **资源释放**：在资源不再使用后自动归还到资源池

## 从旧系统迁移

### 使用资源包装器

如果需要在 FrameGraph 中使用现有的 Graphics 资源：

```cpp
#include "RHI/RHIResourceWrappers.h"

// 包装现有纹理
Texture* existingTexture = ...;
RHITextureWrapper textureWrapper(existingTexture);

// 包装现有命令列表
CommandList* existingCmdList = ...;
RHICommandListWrapper cmdListWrapper(existingCmdList);

// 现在可以在 FrameGraph 中使用
cmdListWrapper.Draw(3, 1, 0, 0);
```

### 使用适配器

使用 RHIAdapter 进行类型转换：

```cpp
#include "RHI/RHIAdapter.h"

// 转换旧类型到 RHI 类型
ResourceState legacyState = ResourceState::RenderTarget;
RHIResourceState rhiState = ConvertToRHIState(legacyState);

// 转换视口
Viewport legacyViewport = { 0, 0, 1920, 1080, 0, 1 };
RHIViewport rhiViewport = ConvertToRHIViewport(legacyViewport);
```

## Pass 剔除

FrameGraph 会自动剔除没有贡献的 Pass：

- **有副作用的 Pass**：调用 `builder.SetSideEffect(true)` 的 Pass 不会被剔除
- **输出资源依赖**：使用 `MarkOutput()` 标记的资源的生产者 Pass 不会被剔除
- **链式依赖**：被非剔除 Pass 依赖的 Pass 也会被保留

## 最佳实践

1. **资源命名**：给每个资源起有意义的名字，便于调试
2. **屏幕相对尺寸**：使用 `useScreenSize` 让资源自动适应分辨率变化
3. **资源复用**：FrameGraph 会自动复用资源，无需手动管理
4. **Pass 粒度**：合理划分 Pass，既不要太细（开销大）也不要太粗（失去并行机会）
5. **调试标记**：使用 `BeginEvent/EndEvent` 在 PIX 等工具中查看 Pass 边界

## 文件结构

```
Source/RenderGraph/
├── FrameGraph.h              # 核心头文件
├── FrameGraph.cpp            # 核心实现
├── DeferredFrameGraph.h      # 延迟渲染示例

Source/RHI/
├── RHITypes.h                # RHI 类型定义
├── RHICommandList.h          # 命令列表接口
├── RHIDevice.h               # 设备接口
├── RHIResource.h             # 资源基类
├── RHIAdapter.h              # 类型转换适配器
├── RHIResourceWrappers.h     # 资源包装器
```
