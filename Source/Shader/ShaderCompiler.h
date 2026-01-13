#pragma once
#include "Core/Types.h"
#include <filesystem>

namespace Sea
{
    enum class ShaderStage { Vertex, Pixel, Geometry, Hull, Domain, Compute };
    enum class ShaderModel { SM_5_0, SM_5_1, SM_6_0, SM_6_1, SM_6_2, SM_6_3, SM_6_4, SM_6_5, SM_6_6 };

    struct ShaderCompileDesc
    {
        std::filesystem::path filePath;
        std::string entryPoint = "main";
        ShaderStage stage = ShaderStage::Vertex;
        ShaderModel model = ShaderModel::SM_6_0;
        std::vector<std::pair<std::string, std::string>> defines;
        std::vector<std::filesystem::path> includePaths;
        bool debug = false;
        bool optimize = true;
    };

    struct ShaderCompileResult
    {
        std::vector<u8> bytecode;
        std::string errors;
        std::string warnings;
        bool success = false;
    };

    class ShaderCompiler
    {
    public:
        static bool Initialize();
        static void Shutdown();
        
        static ShaderCompileResult Compile(const ShaderCompileDesc& desc);
        static ShaderCompileResult CompileFromSource(const std::string& source, const ShaderCompileDesc& desc);
        
        // 使用DXC (DirectX Shader Compiler) 编译 SM6.0+
        static ShaderCompileResult CompileDXC(const ShaderCompileDesc& desc);
        // 使用FXC编译 SM5.x
        static ShaderCompileResult CompileFXC(const ShaderCompileDesc& desc);
        
        // 全局调试设置 - 用于 RenderDoc 等工具
        // 注意：需要在 shader 编译前设置，或设置后重新编译 shader
        static void SetGlobalDebugEnabled(bool enabled) { s_GlobalDebugEnabled = enabled; }
        static bool IsGlobalDebugEnabled() { return s_GlobalDebugEnabled; }

    private:
        static std::string GetTargetProfile(ShaderStage stage, ShaderModel model);
        // 默认启用调试模式，方便 RenderDoc 查看源码
        static inline bool s_GlobalDebugEnabled = true;
    };
}
