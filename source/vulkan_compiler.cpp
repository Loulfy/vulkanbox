//
// Created by lorian on 15/02/2026.
//

#include "ref_ptr.hpp"
#include "vulkan_device.hpp"

// clang-format off
#include <directx-dxc/dxcapi.h>
// clang-format on

#include <glslang/Public/ResourceLimits.h>
#include <glslang/Public/ShaderLang.h>
#include <glslang/SPIRV/GlslangToSpv.h>
#include <nlohmann/json.hpp>
using json = nlohmann::json;

template <typename T> using ComPtr = RefPtr<T>;

struct KindMapping
{
    fs::path ext;
    vk::ShaderStageFlagBits stage;
};

static const std::array<KindMapping, 4> c_KindMap = { {
    { ".vert", vk::ShaderStageFlagBits::eVertex },
    { ".frag", vk::ShaderStageFlagBits::eFragment },
    { ".geom", vk::ShaderStageFlagBits::eGeometry },
    { ".comp", vk::ShaderStageFlagBits::eCompute },
} };

std::optional<KindMapping> convertShaderStageToExtension(const vk::ShaderStageFlagBits stage)
{
    for (auto& map : c_KindMap)
        if (map.stage == stage)
            return map;
    return {};
}

static std::wstring_view convertShaderStageHlsl(const vk::ShaderStageFlagBits shaderStage)
{
    switch (shaderStage)
    {
    case vk::ShaderStageFlagBits::eCompute:
        return L"cs_6_6";
    case vk::ShaderStageFlagBits::eVertex:
        return L"vs_6_6";
    case vk::ShaderStageFlagBits::eTessellationControl:
        return L"hs_6_6";
    case vk::ShaderStageFlagBits::eTessellationEvaluation:
        return L"ds_6_6";
    case vk::ShaderStageFlagBits::eGeometry:
        return L"gs_6_6";
    case vk::ShaderStageFlagBits::eFragment:
        return L"ps_6_6";
    case vk::ShaderStageFlagBits::eMeshEXT:
        return L"ms_6_6";
    case vk::ShaderStageFlagBits::eRaygenKHR:
    case vk::ShaderStageFlagBits::eAnyHitKHR:
    case vk::ShaderStageFlagBits::eClosestHitKHR:
    case vk::ShaderStageFlagBits::eMissKHR:
    case vk::ShaderStageFlagBits::eIntersectionKHR:
    case vk::ShaderStageFlagBits::eCallableKHR:
        return L"as_6_6";
    default:
    case vk::ShaderStageFlagBits::eAll:
    case vk::ShaderStageFlagBits::eAllGraphics:
        return L"lib_6_6";
    }
}

void compileShaderHlsl(const ShaderModule& shaderModule, std::vector<uint32_t>& output, bool debug)
{
    std::wstring_view targetProfile = convertShaderStageHlsl(shaderModule.stage);
    std::wstring entryPoint = sys::toUtf16(shaderModule.entryPoint);

    ComPtr<IDxcCompiler> compiler;
    ComPtr<IDxcUtils> utils;
    ComPtr<IDxcIncludeHandler> includeHandler;
    DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils));
    DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler));
    utils->CreateDefaultIncludeHandler(&includeHandler);

    std::vector<LPCWSTR> compilationArguments{
        DXC_ARG_PACK_MATRIX_COLUMN_MAJOR,
        DXC_ARG_WARNINGS_ARE_ERRORS,
        DXC_ARG_ALL_RESOURCES_BOUND,
        L"-Qembed_debug",
    };

    if (debug)
        compilationArguments.emplace_back(DXC_ARG_DEBUG);
    else
        compilationArguments.emplace_back(DXC_ARG_OPTIMIZATION_LEVEL3);

    uint32_t defineCount = 0;
    DxcDefine defineSpirv(L"__spirv__", L"1");

    compilationArguments.emplace_back(L"-fspv-extension=SPV_KHR_ray_tracing");
    compilationArguments.emplace_back(L"-fspv-extension=SPV_KHR_multiview");
    compilationArguments.emplace_back(L"-fspv-extension=SPV_KHR_shader_draw_parameters");
    compilationArguments.emplace_back(L"-fspv-extension=SPV_EXT_descriptor_indexing");
    compilationArguments.emplace_back(L"-fspv-extension=SPV_KHR_ray_query");
    compilationArguments.emplace_back(L"-fspv-target-env=vulkan1.3");
    compilationArguments.emplace_back(L"-spirv");
    if (shaderModule.stage == vk::ShaderStageFlagBits::eVertex)
        compilationArguments.emplace_back(L"-fvk-invert-y");
    defineCount = 1;

    const fs::path path = sys::ASSETS_DIR / shaderModule.path;

    // Load the shader source file to a blob.
    ComPtr<IDxcBlobEncoding> sourceBlob;
    std::wstring source = sys::toUtf16(path.string());
    HRESULT hr = utils->LoadFile(source.c_str(), nullptr, &sourceBlob);
    if (FAILED(hr))
        log::error("Failed to open shader with path : {}", shaderModule.path.string());

    // Compile the shader.
    ComPtr<IDxcOperationResult> result;
    hr = compiler->Compile(sourceBlob.Get(), source.c_str(), entryPoint.c_str(), targetProfile.data(),
                           compilationArguments.data(), static_cast<uint32_t>(compilationArguments.size()),
                           &defineSpirv, defineCount, includeHandler.Get(), &result);
    if (FAILED(hr))
    {
        log::error("Failed to compile shader with path : {}", shaderModule.path.string());
    }

    // Get compilation errors (if any).
    ComPtr<IDxcBlobEncoding> errors;
    result->GetErrorBuffer(&errors);
    if (errors && errors->GetBufferSize() > 0)
    {
        LPVOID errorMessage = errors->GetBufferPointer();
        log::error(std::string(static_cast<LPCSTR>(errorMessage)));
        return;
    }

    ComPtr<IDxcBlob> bytecode;
    result->GetResult(&bytecode);

    // std::ofstream file(output, std::ios::out | std::ios::binary);
    // file.write(static_cast<char*>(bytecode->GetBufferPointer()), static_cast<int32_t>(bytecode->GetBufferSize()));
    // file.close();
    const uint32_t* data = static_cast<uint32_t*>(bytecode->GetBufferPointer());
    output.assign(data, data + static_cast<int64_t>(bytecode->GetBufferSize() / 4));
}

static EShLanguage convertShaderStageGlsl(const vk::ShaderStageFlagBits shaderStage)
{
    switch (shaderStage)
    {
    case vk::ShaderStageFlagBits::eCompute:
        return EShLanguage::EShLangCompute;
    case vk::ShaderStageFlagBits::eVertex:
        return EShLanguage::EShLangVertex;
    case vk::ShaderStageFlagBits::eTessellationControl:
        return EShLanguage::EShLangTessControl;
    case vk::ShaderStageFlagBits::eTessellationEvaluation:
        return EShLanguage::EShLangTessEvaluation;
    case vk::ShaderStageFlagBits::eGeometry:
        return EShLanguage::EShLangGeometry;
    case vk::ShaderStageFlagBits::eFragment:
        return EShLanguage::EShLangFragment;
    case vk::ShaderStageFlagBits::eMeshEXT:
        return EShLanguage::EShLangMesh;
    case vk::ShaderStageFlagBits::eRaygenKHR:
        return EShLanguage::EShLangRayGen;
    case vk::ShaderStageFlagBits::eAnyHitKHR:
        return EShLanguage::EShLangAnyHit;
    case vk::ShaderStageFlagBits::eClosestHitKHR:
        return EShLanguage::EShLangClosestHit;
    case vk::ShaderStageFlagBits::eMissKHR:
        return EShLanguage::EShLangMiss;
    case vk::ShaderStageFlagBits::eIntersectionKHR:
        return EShLanguage::EShLangIntersect;
    case vk::ShaderStageFlagBits::eCallableKHR:
        return EShLanguage::EShLangCallable;

    default:
    case vk::ShaderStageFlagBits::eAll:
    case vk::ShaderStageFlagBits::eAllGraphics:
        return EShLanguage::EShLangCount;
    }
}

static bool compile(glslang::TShader& shader, const std::string& code, EShMessages controls,
                    const std::string& shaderName, const std::string& entryPointName)
{
    const char* shaderStrings = code.c_str();
    const int shaderLengths = static_cast<int>(code.size());
    const char* shaderNames = shaderName.c_str();

    if (controls & EShMsgDebugInfo)
    {
        shaderNames = shaderName.data();
        shader.setStringsWithLengthsAndNames(&shaderStrings, &shaderLengths, &shaderNames, 1);
    }
    else
    {
        shader.setStringsWithLengths(&shaderStrings, &shaderLengths, 1);
    }

    shader.setEntryPoint(entryPointName.c_str());
    shader.setSourceEntryPoint(entryPointName.c_str());
    return shader.parse(GetDefaultResources(), 140, false, controls);
}

static void compileShaderGlsl(const ShaderModule& shaderModule, std::vector<uint32_t>& output, bool debug)
{
    EShLanguage stage = convertShaderStageGlsl(shaderModule.stage);

    const fs::path path = sys::ASSETS_DIR / shaderModule.path;
    std::stringstream code;
    std::ifstream file(path, std::ios::binary);
    code << file.rdbuf();

    bool success = true;
    glslang::TShader shader(stage);
    shader.setInvertY(false);
    EShMessages controls = EShMsgCascadingErrors;
    controls = static_cast<EShMessages>(controls | EShMsgDebugInfo);
    controls = static_cast<EShMessages>(controls | EShMsgSpvRules);
    controls = static_cast<EShMessages>(controls | EShMsgKeepUncalled);
    controls = static_cast<EShMessages>(controls | EShMsgVulkanRules | EShMsgSpvRules);
    shader.setEnvTarget(glslang::EShTargetLanguage::EShTargetSpv, glslang::EShTargetLanguageVersion::EShTargetSpv_1_6);
    success &= compile(shader, code.str(), controls, shaderModule.path.string(), shaderModule.entryPoint);

    if (!success)
    {
        log::error(shader.getInfoLog());
        return;
    }

    // Link all of them.
    glslang::TProgram program;
    program.addShader(&shader);
    success &= program.link(controls);

    if (!success)
    {
        log::error(program.getInfoLog());
        return;
    }

    glslang::SpvOptions options;
    spv::SpvBuildLogger logger;
    // options.disableOptimizer = false;
    // options.optimizeSize = true;
    options.stripDebugInfo = false;
    options.emitNonSemanticShaderDebugInfo = true;
    options.emitNonSemanticShaderDebugSource = true;
    glslang::GlslangToSpv(*program.getIntermediate(shader.getStage()), output, &logger, &options);
    if (!logger.getAllMessages().empty())
        log::error(logger.getAllMessages());
}

void from_json(const json& j, ShaderModule& s)
{
    if (j.contains("path"))
        s.path = fs::path(j["path"].get<std::string>());
    if (j.contains("entryPoint"))
        s.entryPoint = j["entryPoint"];
    if (j.contains("stage"))
    {
        const std::string& stage = j["stage"];
        if (stage == "Vertex")
            s.stage = vk::ShaderStageFlagBits::eVertex;
        else if (stage == "Pixel")
            s.stage = vk::ShaderStageFlagBits::eFragment;
        else if (stage == "Compute")
            s.stage = vk::ShaderStageFlagBits::eCompute;
    }
}

void VulkanDevice::shaderAutoCompile()
{
    log::info("Assets Path: {}", sys::ASSETS_DIR.string());
    fs::create_directories(sys::CACHED_DIR);

    std::ifstream bundle(sys::ASSETS_DIR / "shaders.json");
    std::stringstream buffer;
    buffer << bundle.rdbuf();
    json j = json::parse(buffer.str());
    std::vector<ShaderModule> modules = j;

    for (const ShaderModule& m : modules)
    {
        const fs::path& entry = m.path;
        const std::optional<KindMapping>& res = convertShaderStageToExtension(m.stage);

        fs::path f = sys::CACHED_DIR / m.path.stem();
        f += res.value().ext;
        f += ".spv";

        if (fs::exists(f) && fs::last_write_time(f) > fs::last_write_time(sys::ASSETS_DIR / entry))
            continue;

        log::warn("Compile shader: {} to {}", entry.string(), f.string());
        std::vector<uint32_t> entryContent;

        if (entry.extension() == ".hlsl")
            compileShaderHlsl(m, entryContent, true);
        if (entry.extension() == ".glsl")
            compileShaderGlsl(m, entryContent, true);

        std::ofstream output(f, std::ios::out | std::ios::binary);
        output.write(reinterpret_cast<const char*>(entryContent.data()), entryContent.size() * 4);
        output.close();
    }
}