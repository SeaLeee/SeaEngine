# SeaEngine

一个用于预研游戏渲染效果的小型渲染引擎，类似 NVIDIA Falcor，支持可视化连线搭建渲染管线。

## 特性

- **图形化管线编辑器**: 使用 ImNodes 实现的节点编辑器，可以通过拖拽连线的方式搭建渲染管线
- **RenderGraph 系统**: 自动管理资源生命周期和执行顺序
- **HLSL Shader 支持**: 支持 SM5.0-SM6.6，使用 DXC 编译器
- **Shader 热重载**: 自动检测 shader 文件变化并重新编译
- **RenderDoc 集成**: 按 F12 触发截帧，便于调试
- **DirectX 12 渲染后端**: 现代化的图形 API

## 构建

### 依赖

- Windows 10/11
- Visual Studio 2022 (或更高版本)
- CMake 3.20+
- Windows SDK (10.0.19041.0 或更高)

### 构建步骤

```powershell
# 克隆仓库
git clone https://github.com/your-repo/SeaEngine.git
cd SeaEngine

# 创建构建目录
mkdir build
cd build

# 生成项目文件
cmake .. -G "Visual Studio 17 2022" -A x64

# 构建
cmake --build . --config Release

# 或者用 Visual Studio 打开
start SeaEngine.sln
```

## 项目结构

```
SeaEngine/
├── Source/
│   ├── Core/           # 核心系统 (窗口、日志、文件系统等)
│   ├── Graphics/       # DX12 图形抽象层
│   ├── RenderGraph/    # 渲染图系统
│   ├── Shader/         # Shader 编译和管理
│   └── Editor/         # 节点编辑器 UI
├── Shaders/            # HLSL 着色器
├── Samples/            # 示例应用
└── Assets/             # 资源文件
```

## 使用方法

### 1. 节点编辑器

启动应用后，会看到一个节点编辑器界面:

- **右键菜单**: 添加新的 Pass 或 Resource 节点
- **拖拽**: 移动节点位置
- **连线**: 从输出引脚拖到输入引脚创建连接
- **编译**: 点击 "Compile Graph" 验证管线

### 2. Pass 类型

- **Graphics Pass**: 光栅化渲染 Pass
- **Compute Pass**: 计算着色器 Pass
- **Copy Pass**: 资源拷贝 Pass

### 3. 创建自定义 Pass

```cpp
RenderPassDesc myPass;
myPass.name = "MyCustomPass";
myPass.type = PassType::Graphics;
myPass.executeFunc = [](CommandList& cmdList, RenderPassContext& ctx) {
    // 你的渲染代码
    cmdList.SetPipelineState(myPSO);
    cmdList.Draw(3, 1);
};

graph.AddPass(myPass);
```

### 4. Shader 开发

在 `Shaders/` 目录下创建 HLSL 文件:

```hlsl
// MyShader_VS.hlsl
#include "Common.hlsli"

PSInput main(VSInput input)
{
    PSInput output;
    // ...
    return output;
}
```

### 5. RenderDoc 截帧

- 确保 RenderDoc 已安装并注入应用
- 按 **F12** 触发截帧
- 在 RenderDoc 中分析帧数据

## 配置

### CMake 选项

- `SEA_ENABLE_RENDERDOC`: 启用 RenderDoc 集成 (默认 ON)
- `SEA_BUILD_SAMPLES`: 构建示例应用 (默认 ON)

### 应用配置

```cpp
ApplicationConfig config;
config.window.title = "My App";
config.window.width = 1920;
config.window.height = 1080;
config.enableValidation = true;  // D3D12 调试层
config.enableRenderDoc = true;
```

## 快捷键

| 快捷键 | 功能 |
|--------|------|
| F12 | RenderDoc 截帧 |
| Ctrl+S | 保存 Shader |
| F5 | 编译 Shader |
| ESC | 退出应用 |

## 架构

### RenderGraph

RenderGraph 系统自动:
1. 拓扑排序确定 Pass 执行顺序
2. 检测资源依赖关系
3. 插入正确的资源屏障
4. 管理临时资源的生命周期

### 图形抽象层

对 DX12 API 进行了轻量封装:
- `Device`: D3D12 设备管理
- `CommandQueue/CommandList`: 命令提交
- `Buffer/Texture`: GPU 资源
- `PipelineState/RootSignature`: 管线状态

## 许可证

MIT License
