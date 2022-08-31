
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>

#include <fstream>
#include <iostream>
#include <algorithm>
#include <stdexcept>

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

const int MAX_FRAMES_IN_FLIGHT = 2;

class HelloTriangleApplication
{
public:
    void run()
    {
        // initWindow
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);

        initVulkan();

        // main loop
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
            drawFrame();
        }
        device->waitIdle();

        // cleanup
        glfwDestroyWindow(window);
        glfwTerminate();
    }

private:
    GLFWwindow* window;

    vk::UniqueInstance instance;
    vk::UniqueDebugUtilsMessengerEXT debugUtilsMessenger;
    vk::UniqueSurfaceKHR surface;

    vk::PhysicalDevice physicalDevice;
    vk::UniqueDevice device;

    uint32_t queueFamilyIndex;
    vk::Queue queue;

    vk::UniqueSwapchainKHR swapchain;
    uint32_t swapchainImageCount;
    std::vector<vk::Image> swapchainImages;
    vk::Format swapchainImageFormat;
    vk::Extent2D swapchainExtent;

    std::vector<vk::UniqueImageView> swapchainImageViews;

    vk::UniquePipelineLayout pipelineLayout;
    vk::UniquePipeline graphicsPipeline;

    vk::UniqueCommandPool commandPool;
    std::vector<vk::UniqueCommandBuffer> commandBuffers;

    std::vector<vk::UniqueSemaphore> imageAvailableSemaphores;
    std::vector<vk::UniqueSemaphore> renderFinishedSemaphores;
    std::vector<vk::UniqueFence> inFlightFences;
    size_t currentFrame = 0;

    void initVulkan()
    {
        createInstance();
        setupDebugMessenger();
        createSurface();
        pickPhysicalDevice();
        createLogicalDevice();
        createSwapChain();
        createImageViews();
        createGraphicsPipeline();
        createCommandPool();
        createCommandBuffers();
        createSyncObjects();
    }

    void createInstance()
    {
        static vk::DynamicLoader dl;
        auto vkGetInstanceProcAddr = dl.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
        VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);

        std::vector layers = { "VK_LAYER_KHRONOS_validation", "VK_LAYER_LUNARG_monitor" };
        auto extensions = getRequiredExtensions();

        vk::ApplicationInfo appInfo;
        appInfo.setApiVersion(VK_API_VERSION_1_3);

        vk::InstanceCreateInfo createInfo;
        createInfo.setPApplicationInfo(&appInfo);
        createInfo.setPEnabledLayerNames(layers);
        createInfo.setPEnabledExtensionNames(extensions);
        instance = vk::createInstanceUnique(createInfo);

        VULKAN_HPP_DEFAULT_DISPATCHER.init(*instance);
    }

    void setupDebugMessenger()
    {
        vk::DebugUtilsMessengerCreateInfoEXT createInfo;
        createInfo.setMessageSeverity(vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);
        createInfo.setMessageType(vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);
        createInfo.setPfnUserCallback(&debugUtilsMessengerCallback);
        debugUtilsMessenger = instance->createDebugUtilsMessengerEXTUnique(createInfo);
    }

    void createSurface()
    {
        VkSurfaceKHR _surface;
        if (glfwCreateWindowSurface(instance.get(), window, nullptr, &_surface) != VK_SUCCESS) {
            throw std::runtime_error("failed to create window surface!");
        }
        surface = vk::UniqueSurfaceKHR(_surface, { instance.get() });
    }

    void pickPhysicalDevice()
    {
        physicalDevice = instance->enumeratePhysicalDevices().front();
    }

    void createLogicalDevice()
    {
        findQueueFamily(physicalDevice);

        float queuePriority = 1.0f;
        vk::DeviceQueueCreateInfo queueCreateInfo({}, queueFamilyIndex, 1, &queuePriority);

        vk::PhysicalDeviceFeatures deviceFeatures{};
        vk::PhysicalDeviceDynamicRenderingFeatures dynamicRenderingFeatures{ true };

        const std::vector extensions{ VK_KHR_SWAPCHAIN_EXTENSION_NAME };
        vk::DeviceCreateInfo createInfo({}, queueCreateInfo, {}, extensions, &deviceFeatures);
        createInfo.setPNext(&dynamicRenderingFeatures);

        device = physicalDevice.createDeviceUnique(createInfo);
        queue = device->getQueue(queueFamilyIndex, 0);
    }

    void createSwapChain()
    {
        swapchainImageCount = 3;
        swapchainImageFormat = vk::Format::eB8G8R8A8Unorm;
        swapchainExtent.width = WIDTH;
        swapchainExtent.height = HEIGHT;

        vk::SwapchainCreateInfoKHR createInfo;
        createInfo.setSurface(surface.get());
        createInfo.setMinImageCount(swapchainImageCount);
        createInfo.setImageFormat(swapchainImageFormat);
        createInfo.setImageExtent(swapchainExtent);
        createInfo.setImageArrayLayers(1);
        createInfo.setImageUsage(vk::ImageUsageFlagBits::eColorAttachment);
        swapchain = device->createSwapchainKHRUnique(createInfo);
        swapchainImages = device->getSwapchainImagesKHR(swapchain.get());
    }

    void createImageViews()
    {
        swapchainImageViews.resize(swapchainImages.size());
        for (size_t i = 0; i < swapchainImages.size(); i++) {
            vk::ImageViewCreateInfo createInfo;
            createInfo.setImage(swapchainImages[i]);
            createInfo.setViewType(vk::ImageViewType::e2D);
            createInfo.setFormat(swapchainImageFormat);
            createInfo.setSubresourceRange({ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
            swapchainImageViews[i] = device->createImageViewUnique(createInfo);
        }
    }

    void createGraphicsPipeline()
    {
        auto vertShaderCode = readFile("../shaders/vert.spv");
        auto fragShaderCode = readFile("../shaders/frag.spv");

        vk::UniqueShaderModule vertShaderModule = createShaderModule(vertShaderCode);
        vk::UniqueShaderModule fragShaderModule = createShaderModule(fragShaderCode);

        vk::PipelineShaderStageCreateInfo vertShaderStageInfo({}, vk::ShaderStageFlagBits::eVertex,
                                                              vertShaderModule.get(), "main");
        vk::PipelineShaderStageCreateInfo fragShaderStageInfo({}, vk::ShaderStageFlagBits::eFragment,
                                                              fragShaderModule.get(), "main");

        std::array shaderStages = { vertShaderStageInfo, fragShaderStageInfo };

        vk::PipelineVertexInputStateCreateInfo vertexInputInfo({}, 0, nullptr, 0, nullptr);

        vk::PipelineInputAssemblyStateCreateInfo inputAssembly({}, vk::PrimitiveTopology::eTriangleList);

        vk::Viewport viewport(0.0f, 0.0f, swapchainExtent.width, swapchainExtent.height, 0.0f, 1.0f);

        vk::Rect2D scissor({ 0, 0 }, swapchainExtent);

        vk::PipelineViewportStateCreateInfo viewportState({}, 1, &viewport, 1, &scissor);

        vk::PipelineRasterizationStateCreateInfo rasterizer({}, VK_FALSE, VK_FALSE,
                                                            vk::PolygonMode::eFill, vk::CullModeFlagBits::eBack, vk::FrontFace::eClockwise,
                                                            VK_FALSE, 0.0f, 0.0f, 0.0f, 1.0f);

        vk::PipelineMultisampleStateCreateInfo multisampling({}, vk::SampleCountFlagBits::e1, VK_FALSE);

        vk::PipelineColorBlendAttachmentState colorBlendAttachment(VK_FALSE);
        colorBlendAttachment.setColorWriteMask(
            vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);

        vk::PipelineColorBlendStateCreateInfo colorBlending({}, VK_FALSE, vk::LogicOp::eCopy, 1, &colorBlendAttachment);

        vk::PipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayout = device->createPipelineLayoutUnique(pipelineLayoutInfo);

        vk::GraphicsPipelineCreateInfo pipelineInfo({}, shaderStages, &vertexInputInfo, &inputAssembly, {},
                                                    &viewportState, &rasterizer, &multisampling, {}, &colorBlending, {}, pipelineLayout.get(),
                                                    nullptr, 0, {}, {});

        vk::PipelineRenderingCreateInfo renderingInfo;
        renderingInfo.setColorAttachmentCount(1);
        renderingInfo.setColorAttachmentFormats(swapchainImageFormat);
        pipelineInfo.setPNext(&renderingInfo);

        vk::ResultValue<vk::UniquePipeline> result = device->createGraphicsPipelineUnique({}, pipelineInfo);
        if (result.result != vk::Result::eSuccess) {
            throw std::runtime_error("failed to create a pipeline!");
        }
        graphicsPipeline = std::move(result.value);
    }

    void createCommandPool()
    {
        vk::CommandPoolCreateInfo poolInfo({}, queueFamilyIndex);

        commandPool = device->createCommandPoolUnique(poolInfo);
    }

    void createCommandBuffers()
    {
        vk::CommandBufferAllocateInfo allocInfo(commandPool.get(), vk::CommandBufferLevel::ePrimary, swapchainImageCount);

        commandBuffers = device->allocateCommandBuffersUnique(allocInfo);

        for (size_t i = 0; i < commandBuffers.size(); i++) {
            commandBuffers[i]->begin(vk::CommandBufferBeginInfo{});

            vk::ClearValue clearColorValue;
            clearColorValue.setColor(vk::ClearColorValue{ std::array{0.0f, 0.0f, 0.0f, 1.0f} });

            vk::RenderingAttachmentInfoKHR colorAttachment;
            colorAttachment.setImageView(swapchainImageViews[i].get());
            colorAttachment.setImageLayout(vk::ImageLayout::eAttachmentOptimal);
            colorAttachment.setClearValue(clearColorValue);
            colorAttachment.setLoadOp(vk::AttachmentLoadOp::eClear);
            colorAttachment.setStoreOp(vk::AttachmentStoreOp::eStore);

            vk::RenderingInfo renderingInfo;
            renderingInfo.setRenderArea({ {0, 0}, swapchainExtent });
            renderingInfo.setLayerCount(1);
            renderingInfo.setColorAttachments(colorAttachment);

            commandBuffers[i]->beginRendering(renderingInfo);

            commandBuffers[i]->bindPipeline(vk::PipelineBindPoint::eGraphics, graphicsPipeline.get());
            commandBuffers[i]->draw(3, 1, 0, 0);

            commandBuffers[i]->endRendering();

            vk::ImageMemoryBarrier imageMemoryBarrier;
            imageMemoryBarrier.setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);
            imageMemoryBarrier.setOldLayout(vk::ImageLayout::eUndefined);
            imageMemoryBarrier.setNewLayout(vk::ImageLayout::ePresentSrcKHR);
            imageMemoryBarrier.setImage(swapchainImages[i]);
            imageMemoryBarrier.setSubresourceRange({ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });

            commandBuffers[i]->pipelineBarrier(
                vk::PipelineStageFlagBits::eColorAttachmentOutput,
                vk::PipelineStageFlagBits::eBottomOfPipe,
                {}, {}, {}, imageMemoryBarrier);

            commandBuffers[i]->end();
        }
    }

    void createSyncObjects()
    {
        imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            imageAvailableSemaphores[i] = device->createSemaphoreUnique({});
            renderFinishedSemaphores[i] = device->createSemaphoreUnique({});
            inFlightFences[i] = device->createFenceUnique({ vk::FenceCreateFlagBits::eSignaled });
        }
    }

    void drawFrame()
    {
        device->waitForFences(inFlightFences[currentFrame].get(), true, UINT64_MAX);
        device->resetFences(inFlightFences[currentFrame].get());

        uint32_t imageIndex = device->acquireNextImageKHR(swapchain.get(), UINT64_MAX, imageAvailableSemaphores[currentFrame].get()).value;
        vk::PipelineStageFlags waitStage(vk::PipelineStageFlagBits::eColorAttachmentOutput);
        vk::SubmitInfo submitInfo(imageAvailableSemaphores[currentFrame].get(), waitStage,
                                  commandBuffers[imageIndex].get(), renderFinishedSemaphores[currentFrame].get());

        queue.submit(submitInfo, inFlightFences[currentFrame].get());

        vk::PresentInfoKHR presentInfo(renderFinishedSemaphores[currentFrame].get(), swapchain.get(), imageIndex);

        queue.presentKHR(presentInfo);

        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    vk::UniqueShaderModule createShaderModule(const std::vector<char>& code)
    {
        vk::ShaderModuleCreateInfo createInfo({}, code.size(), reinterpret_cast<const uint32_t*>(code.data()));
        return device->createShaderModuleUnique(createInfo);
    }

    void findQueueFamily(vk::PhysicalDevice physicalDevice)
    {
        auto queueFamilies = physicalDevice.getQueueFamilyProperties();
        for (int i = 0; i < queueFamilies.size(); i++) {
            auto supportGraphics = queueFamilies[i].queueFlags & vk::QueueFlagBits::eGraphics;
            auto supportPresent = physicalDevice.getSurfaceSupportKHR(i, *surface);
            if (supportGraphics && supportPresent) {
                queueFamilyIndex = i;
                return;
            }
        }
        throw std::runtime_error("failed to find queue family!");
    }

    std::vector<const char*> getRequiredExtensions()
    {
        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions;
        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        std::vector extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

        return extensions;
    }

    static std::vector<char> readFile(const std::string& filename)
    {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);
        if (!file.is_open()) {
            throw std::runtime_error("failed to open file!");
        }

        size_t fileSize = file.tellg();
        std::vector<char> buffer(fileSize);
        file.seekg(0);
        file.read(buffer.data(), fileSize);
        file.close();
        return buffer;
    }

    static VKAPI_ATTR VkBool32 VKAPI_CALL
        debugUtilsMessengerCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                    VkDebugUtilsMessageTypeFlagsEXT messageTypes,
                                    VkDebugUtilsMessengerCallbackDataEXT const* pCallbackData,
                                    void* pUserData)
    {
        std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

        return VK_FALSE;
    }
};

int main()
{
    HelloTriangleApplication app;

    try {
        app.run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }
}
