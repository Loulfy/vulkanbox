cmake_minimum_required(VERSION 3.28)
project(cominou)

set(CMAKE_CXX_STANDARD 23)

find_package(Vulkan)
find_package(glfw3 CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)
find_package(directx-dxc CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(unofficial-spirv-reflect CONFIG REQUIRED)
find_package(SPIRV-Headers CONFIG REQUIRED)
find_package(glslang CONFIG REQUIRED)

add_definitions(-DPROJECT_DIR=\"${PROJECT_SOURCE_DIR}\")

set(CODE_SRC
    source/logger.hpp
    source/logger.cpp
    source/system.hpp
    source/system.cpp
    source/vulkan_device.hpp
    source/vulkan_device.cpp
    source/vulkan_texture.cpp
    source/vulkan_buffer.cpp
    source/vulkan_frame.cpp
    source/vulkan_command.cpp
    source/vulkan_swapchain.cpp
    source/vulkan_pipeline.cpp
    source/vulkan_compiler.cpp
)

add_executable(cominou main.cpp ${CODE_SRC})
target_link_libraries(cominou PRIVATE glfw)
target_link_libraries(cominou PRIVATE Vulkan::Vulkan)
target_link_libraries(cominou PRIVATE spdlog::spdlog)
target_link_libraries(cominou PRIVATE Vulkan::Headers GPUOpen::VulkanMemoryAllocator)
target_link_libraries(cominou PRIVATE Microsoft::DirectXShaderCompiler)
target_link_libraries(cominou PRIVATE nlohmann_json::nlohmann_json)
target_link_libraries(cominou PRIVATE SPIRV-Headers::SPIRV-Headers)
target_link_libraries(cominou PRIVATE unofficial::spirv-reflect)
target_link_libraries(cominou PRIVATE glslang::glslang glslang::glslang-default-resource-limits glslang::SPIRV)
target_include_directories(cominou PRIVATE "source/")
