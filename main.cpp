#define GLFW_INCLUDE_NONE // Do not include any OpenGL/Vulkan headers
#include <GLFW/glfw3.h>

#include <vector>

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>

#include "vulkan_device.hpp"

constexpr uint32_t WIDTH = 800;
constexpr uint32_t HEIGHT = 600;

constexpr bool enableValidationLayers = true;

void glfw_size_callback(GLFWwindow* window, int width, int height)
{
    const auto app = static_cast<VulkanDevice*>(glfwGetWindowUserPointer(window));
    app->resizeSwapChain(width, height);
}

void glfw_key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GLFW_TRUE);
}

class HelloTriangleApplication
{
  public:
    void run()
    {
        initWindow();
        mainLoop();
        cleanup();
    }

  private:
    GLFWwindow* m_window = nullptr;

    void initWindow()
    {
        // Initialisation GLFW
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
        m_window = glfwCreateWindow(WIDTH, HEIGHT, "Triangle", nullptr, nullptr);
    }

    void mainLoop()
    {
        uint32_t count;
        const char** extensions = glfwGetRequiredInstanceExtensions(&count);

        VulkanDevice::shaderAutoCompile();

        DeviceConfig config;
        config.m_window = m_window;
        config.m_debug = enableValidationLayers;
        config.m_extensions.assign(extensions, extensions + count);
        VulkanDevice device(config);

        glfwSetWindowUserPointer(m_window, &device);
        glfwSetWindowSizeCallback(m_window, glfw_size_callback);
        glfwSetKeyCallback(m_window, glfw_key_callback);

        PipelinePtr pipeline = device.createGraphicsPipeline();

        struct PCB
        {
            float iResolution[2];
            float iTime;
            float iTimeDelta;
            float iMouse[4];
        };

        PCB pcb;

        while (!glfwWindowShouldClose(m_window))
        {
            glfwPollEvents();

            const vk::Extent2D winSize = getWindowsSize();
            pcb.iResolution[0] = winSize.width;
            pcb.iResolution[1] = winSize.height;
            pcb.iTime = glfwGetTime();

            FrameContext& frame = device.acquireNextFrame();

            // frame.clearBackBufferColor(Color::Green);
            RenderingInfo renderingInfo;
            renderingInfo.viewport = { 0, 0, winSize.width, winSize.height };
            renderingInfo.colors.emplace_back(frame.getBackBuffer());
            renderingInfo.colors.back().m_loadOp = vk::AttachmentLoadOp::eClear;
            renderingInfo.colors.back().m_clearValue = vk::ClearColorValue(Color::White);
            frame.bindPipeline(pipeline);
            frame.beginRenderPass(renderingInfo);
            frame.setScissor({ { 0, 0 }, winSize });
            frame.pushConstant(vk::ShaderStageFlagBits::eFragment, &pcb, sizeof(PCB));
            frame.drawPrimitives(4, 0);
            frame.endRenderPass();

            device.present(frame);
        }

        device.waitIdle();
    }

    vk::Extent2D getWindowsSize()
    {
        int w, h;
        glfwGetWindowSize(m_window, &w, &h);
        return { static_cast<uint32_t>(w), static_cast<uint32_t>(h) };
    }

    void cleanup() const
    {
        glfwDestroyWindow(m_window);
        glfwTerminate();
    }
};

int main()
{
    HelloTriangleApplication app;

    try
    {
        app.run();
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
