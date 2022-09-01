
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
    std::vector<vk::Image> swapchainImages;
    uint32_t swapchainImageCount;
    vk::Format swapchainImageFormat;
    vk::Extent2D swapchainExtent;

    std::vector<vk::UniqueImageView> swapchainImageViews;

    vk::UniquePipelineLayout pipelineLayout;
    vk::UniquePipeline graphicsPipeline;

    vk::UniqueCommandPool commandPool;
    std::vector<vk::UniqueCommandBuffer> commandBuffers;

    std::vector<vk::UniqueSemaphore> imageAvailableSemaphores;
    std::vector<vk::UniqueSemaphore> renderFinishedSemaphores;
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

        std::vector layers{ "VK_LAYER_KHRONOS_validation", "VK_LAYER_LUNARG_monitor" };
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
        vk::DeviceQueueCreateInfo queueCreateInfo;
        queueCreateInfo.setQueueFamilyIndex(queueFamilyIndex);
        queueCreateInfo.setQueuePriorities(queuePriority);

        vk::PhysicalDeviceDynamicRenderingFeatures dynamicRenderingFeatures;
        dynamicRenderingFeatures.setDynamicRendering(true);

        std::vector extensions{ VK_KHR_SWAPCHAIN_EXTENSION_NAME };
        vk::DeviceCreateInfo createInfo;
        createInfo.setQueueCreateInfos(queueCreateInfo);
        createInfo.setPEnabledExtensionNames(extensions);
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
        vk::UniqueShaderModule vertShaderModule = createShaderModule("../shaders/vert.spv");
        vk::UniqueShaderModule fragShaderModule = createShaderModule("../shaders/frag.spv");

        std::array<vk::PipelineShaderStageCreateInfo, 2> shaderStages;
        shaderStages[0].setStage(vk::ShaderStageFlagBits::eVertex);
        shaderStages[0].setModule(vertShaderModule.get());
        shaderStages[0].setPName("main");
        shaderStages[1].setStage(vk::ShaderStageFlagBits::eFragment);
        shaderStages[1].setModule(fragShaderModule.get());
        shaderStages[1].setPName("main");

        vk::PipelineVertexInputStateCreateInfo vertexInput;

        vk::PipelineInputAssemblyStateCreateInfo inputAssembly;
        inputAssembly.setTopology(vk::PrimitiveTopology::eTriangleList);

        vk::Viewport viewport;
        viewport.setWidth(swapchainExtent.width);
        viewport.setHeight(swapchainExtent.height);

        vk::Rect2D scissor;
        scissor.setExtent(swapchainExtent);

        vk::PipelineViewportStateCreateInfo viewportState;
        viewportState.setViewports(viewport);
        viewportState.setScissors(scissor);

        vk::PipelineRasterizationStateCreateInfo rasterization;
        rasterization.setLineWidth(1.0f);

        vk::PipelineMultisampleStateCreateInfo multisampling;

        vk::PipelineColorBlendAttachmentState colorBlendAttachment;
        colorBlendAttachment.setColorWriteMask(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                                               vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);

        vk::PipelineColorBlendStateCreateInfo colorBlending;
        colorBlending.setAttachments(colorBlendAttachment);

        vk::PipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayout = device->createPipelineLayoutUnique(pipelineLayoutInfo);

        vk::PipelineRenderingCreateInfo renderingInfo;
        renderingInfo.setColorAttachmentFormats(swapchainImageFormat);

        vk::GraphicsPipelineCreateInfo pipelineInfo;
        pipelineInfo.setStages(shaderStages);
        pipelineInfo.setPVertexInputState(&vertexInput);
        pipelineInfo.setPInputAssemblyState(&inputAssembly);
        pipelineInfo.setPViewportState(&viewportState);
        pipelineInfo.setPRasterizationState(&rasterization);
        pipelineInfo.setPMultisampleState(&multisampling);
        pipelineInfo.setPColorBlendState(&colorBlending);
        pipelineInfo.setLayout(pipelineLayout.get());
        pipelineInfo.setPNext(&renderingInfo);
        graphicsPipeline = std::move(device->createGraphicsPipelineUnique({}, pipelineInfo).value);
    }

    void createCommandPool()
    {
        vk::CommandPoolCreateInfo poolInfo;
        poolInfo.setQueueFamilyIndex(queueFamilyIndex);
        commandPool = device->createCommandPoolUnique(poolInfo);
    }

    void createCommandBuffers()
    {
        vk::CommandBufferAllocateInfo allocInfo;
        allocInfo.setCommandPool(commandPool.get());
        allocInfo.setCommandBufferCount(swapchainImageCount);
        commandBuffers = device->allocateCommandBuffersUnique(allocInfo);

        for (size_t i = 0; i < commandBuffers.size(); i++) {
            commandBuffers[i]->begin(vk::CommandBufferBeginInfo());

            vk::RenderingAttachmentInfo colorAttachment;
            colorAttachment.setImageView(swapchainImageViews[i].get());
            colorAttachment.setImageLayout(vk::ImageLayout::eAttachmentOptimal);

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

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            imageAvailableSemaphores[i] = device->createSemaphoreUnique({});
            renderFinishedSemaphores[i] = device->createSemaphoreUnique({});
        }
    }

    void drawFrame()
    {
        uint32_t imageIndex = device->acquireNextImageKHR(swapchain.get(), UINT64_MAX, imageAvailableSemaphores[currentFrame].get()).value;
        vk::PipelineStageFlags waitStage(vk::PipelineStageFlagBits::eColorAttachmentOutput);
        vk::SubmitInfo submitInfo(imageAvailableSemaphores[currentFrame].get(), waitStage,
                                  commandBuffers[imageIndex].get(), renderFinishedSemaphores[currentFrame].get());

        queue.submit(submitInfo);

        vk::PresentInfoKHR presentInfo(renderFinishedSemaphores[currentFrame].get(), swapchain.get(), imageIndex);

        if (queue.presentKHR(presentInfo) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to present.");
        }

        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
        queue.waitIdle();
    }

    vk::UniqueShaderModule createShaderModule(const std::string& path)
    {
        auto code = readFile(path);
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
        const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

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
        std::cerr << pCallbackData->pMessage << "\n\n";
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
