
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>

#include <set>
#include <cstdlib>
#include <fstream>
#include <optional>
#include <iostream>
#include <algorithm>
#include <stdexcept>

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

const int MAX_FRAMES_IN_FLIGHT = 2;

const std::vector layers = {
    "VK_LAYER_KHRONOS_validation"
};

const std::vector deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

struct SwapChainSupportDetails
{
    vk::SurfaceCapabilitiesKHR capabilities;
    std::vector<vk::SurfaceFormatKHR> formats;
    std::vector<vk::PresentModeKHR> presentModes;
};

class HelloTriangleApplication
{
public:
    void run()
    {
        initWindow();
        initVulkan();
        mainLoop();
        cleanup();
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

    vk::UniqueSwapchainKHR swapChain;
    std::vector<vk::Image> swapChainImages;
    vk::Format swapChainImageFormat;
    vk::Extent2D swapChainExtent;

    std::vector<vk::UniqueImageView> swapChainImageViews;
    std::vector<vk::UniqueFramebuffer> swapChainFramebuffers;

    vk::UniqueRenderPass renderPass;
    vk::UniquePipelineLayout pipelineLayout;
    vk::UniquePipeline graphicsPipeline;

    vk::UniqueCommandPool commandPool;
    std::vector<vk::UniqueCommandBuffer> commandBuffers;

    std::vector<vk::UniqueSemaphore> imageAvailableSemaphores;
    std::vector<vk::UniqueSemaphore> renderFinishedSemaphores;
    std::vector<vk::Fence> inFlightFences;
    std::vector<vk::Fence> imagesInFlight;
    size_t currentFrame = 0;

    void initWindow()
    {
        glfwInit();

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
    }

    void initVulkan()
    {
        createInstance();
        setupDebugMessenger();
        createSurface();
        pickPhysicalDevice();
        createLogicalDevice();
        createSwapChain();
        createImageViews();
        createRenderPass();
        createGraphicsPipeline();
        createFramebuffers();
        createCommandPool();
        createCommandBuffers();
        createSyncObjects();
    }

    void mainLoop()
    {
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
            drawFrame();
        }

        device->waitIdle();
    }

    void cleanup()
    {
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            device->destroyFence(inFlightFences[i]);
        }

        glfwDestroyWindow(window);

        glfwTerminate();
    }

    void createInstance()
    {
        static vk::DynamicLoader dl;
        auto vkGetInstanceProcAddr = dl.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
        VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);

        auto extensions = getRequiredExtensions();

        vk::InstanceCreateInfo createInfo;
        createInfo.setPEnabledLayerNames(layers);
        createInfo.setPEnabledExtensionNames(extensions);
        instance = vk::createInstanceUnique(createInfo);

        VULKAN_HPP_DEFAULT_DISPATCHER.init(*instance);
    }

    void setupDebugMessenger()
    {
        vk::DebugUtilsMessageSeverityFlagsEXT severityFlags(vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);
        vk::DebugUtilsMessageTypeFlagsEXT messageTypeFlags(vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);

        debugUtilsMessenger = instance->createDebugUtilsMessengerEXTUnique(
            vk::DebugUtilsMessengerCreateInfoEXT({}, severityFlags, messageTypeFlags, &debugUtilsMessengerCallback));
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

        vk::DeviceCreateInfo createInfo({}, queueCreateInfo, {}, deviceExtensions, &deviceFeatures);
        createInfo.setPEnabledLayerNames(layers);

        device = physicalDevice.createDeviceUnique(createInfo);
        queue = device->getQueue(queueFamilyIndex, 0);
    }

    void createSwapChain()
    {
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);

        vk::SurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
        vk::PresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
        vk::Extent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

        uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;

        if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
            imageCount = swapChainSupport.capabilities.maxImageCount;
        }

        vk::SwapchainCreateInfoKHR createInfo(
            {}, surface.get(), imageCount, surfaceFormat.format, surfaceFormat.colorSpace,
            extent, 1, vk::ImageUsageFlagBits::eColorAttachment,
            vk::SharingMode::eExclusive, {}, swapChainSupport.capabilities.currentTransform,
            vk::CompositeAlphaFlagBitsKHR::eOpaque, presentMode, /* clipped = */ VK_TRUE, nullptr);

        swapChain = device->createSwapchainKHRUnique(createInfo);
        swapChainImages = device->getSwapchainImagesKHR(swapChain.get());
        swapChainImageFormat = surfaceFormat.format;
        swapChainExtent = extent;
    }

    void createImageViews()
    {
        swapChainImageViews.resize(swapChainImages.size());

        vk::ComponentMapping components(vk::ComponentSwizzle::eIdentity, vk::ComponentSwizzle::eIdentity,
                                        vk::ComponentSwizzle::eIdentity, vk::ComponentSwizzle::eIdentity);
        vk::ImageSubresourceRange subresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);
        for (size_t i = 0; i < swapChainImages.size(); i++) {
            vk::ImageViewCreateInfo createInfo({}, swapChainImages[i], vk::ImageViewType::e2D,
                                               swapChainImageFormat, components, subresourceRange);
            swapChainImageViews[i] = device->createImageViewUnique(createInfo);
        }
    }

    void createRenderPass()
    {
        vk::AttachmentDescription colorAttachment({}, swapChainImageFormat,
                                                  vk::SampleCountFlagBits::e1, vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore,
                                                  vk::AttachmentLoadOp::eDontCare, vk::AttachmentStoreOp::eDontCare,
                                                  vk::ImageLayout::eUndefined, vk::ImageLayout::ePresentSrcKHR);

        vk::AttachmentReference colorAttachmentRef(0, vk::ImageLayout::eColorAttachmentOptimal);

        vk::SubpassDescription subpass({}, vk::PipelineBindPoint::eGraphics, {}, {}, 1, &colorAttachmentRef);

        vk::SubpassDependency dependency(VK_SUBPASS_EXTERNAL, 0,
                                         vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eColorAttachmentOutput,
                                         {}, vk::AccessFlagBits::eColorAttachmentWrite);

        vk::RenderPassCreateInfo renderPassInfo({}, colorAttachment, subpass, dependency);

        renderPass = device->createRenderPassUnique(renderPassInfo);
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

        vk::Viewport viewport(0.0f, 0.0f, (float)swapChainExtent.width, (float)swapChainExtent.height, 0.0f, 1.0f);

        vk::Rect2D scissor({ 0, 0 }, swapChainExtent);

        vk::PipelineViewportStateCreateInfo viewportState({}, 1, &viewport, 1, &scissor);

        vk::PipelineRasterizationStateCreateInfo rasterizer({}, VK_FALSE, VK_FALSE,
                                                            vk::PolygonMode::eFill, vk::CullModeFlagBits::eBack, vk::FrontFace::eClockwise,
                                                            VK_FALSE, 0.0f, 0.0f, 0.0f, 1.0f);

        vk::PipelineMultisampleStateCreateInfo multisampling({}, vk::SampleCountFlagBits::e1, VK_FALSE);

        vk::PipelineColorBlendAttachmentState colorBlendAttachment(VK_FALSE);
        colorBlendAttachment.setColorWriteMask(
            vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);

        vk::PipelineColorBlendStateCreateInfo colorBlending({}, VK_FALSE, vk::LogicOp::eCopy, 1, &colorBlendAttachment);

        std::array states{ vk::DynamicState::eViewport, vk::DynamicState::eLineWidth };
        vk::PipelineDynamicStateCreateInfo dynamicState({}, states);

        vk::PipelineLayoutCreateInfo pipelineLayoutInfo{};

        pipelineLayout = device->createPipelineLayoutUnique(pipelineLayoutInfo);

        vk::GraphicsPipelineCreateInfo pipelineInfo({}, shaderStages, &vertexInputInfo, &inputAssembly, {},
                                                    &viewportState, &rasterizer, &multisampling, {}, &colorBlending, {}, pipelineLayout.get(),
                                                    renderPass.get(), 0, {}, {});

        vk::ResultValue<vk::UniquePipeline> result = device->createGraphicsPipelineUnique({}, pipelineInfo);
        if (result.result == vk::Result::eSuccess) {
            graphicsPipeline = std::move(result.value);
        } else {
            throw std::runtime_error("failed to create a pipeline!");
        }
    }

    void createFramebuffers()
    {
        swapChainFramebuffers.reserve(swapChainImageViews.size());

        for (auto const& view : swapChainImageViews) {
            vk::FramebufferCreateInfo framebufferInfo({}, renderPass.get(), view.get(),
                                                      swapChainExtent.width, swapChainExtent.height, 1);
            swapChainFramebuffers.push_back(device->createFramebufferUnique(framebufferInfo));
        }
    }

    void createCommandPool()
    {
        vk::CommandPoolCreateInfo poolInfo({}, queueFamilyIndex);

        commandPool = device->createCommandPoolUnique(poolInfo);
    }

    void createCommandBuffers()
    {
        vk::CommandBufferAllocateInfo allocInfo(commandPool.get(), vk::CommandBufferLevel::ePrimary, static_cast<uint32_t>(swapChainFramebuffers.size()));

        commandBuffers = device->allocateCommandBuffersUnique(allocInfo);

        for (size_t i = 0; i < commandBuffers.size(); i++) {
            commandBuffers[i]->begin(vk::CommandBufferBeginInfo{});

            vk::ClearValue clearColor(std::array{ 0.0f, 0.0f, 0.0f, 1.0f });
            vk::RenderPassBeginInfo renderPassInfo(renderPass.get(), swapChainFramebuffers[i].get(),
                                                   { {0, 0}, swapChainExtent }, clearColor);

            commandBuffers[i]->beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);
            commandBuffers[i]->bindPipeline(vk::PipelineBindPoint::eGraphics, graphicsPipeline.get());
            commandBuffers[i]->draw(3, 1, 0, 0);
            commandBuffers[i]->endRenderPass();
            commandBuffers[i]->end();
        }
    }

    void createSyncObjects()
    {
        imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
        imagesInFlight.resize(swapChainImages.size());

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            imageAvailableSemaphores[i] = device->createSemaphoreUnique({});
            renderFinishedSemaphores[i] = device->createSemaphoreUnique({});
            inFlightFences[i] = device->createFence({ vk::FenceCreateFlagBits::eSignaled });
        }
    }

    void drawFrame()
    {
        device->waitForFences(inFlightFences[currentFrame], true, UINT64_MAX);
        device->resetFences(inFlightFences[currentFrame]);

        vk::ResultValue<uint32_t> result = device->acquireNextImageKHR(swapChain.get(), UINT64_MAX, imageAvailableSemaphores[currentFrame].get());
        uint32_t imageIndex;
        if (result.result == vk::Result::eSuccess) {
            imageIndex = result.value;
        } else {
            throw std::runtime_error("failed to acquire next image!");
        }

        if (imagesInFlight[imageIndex]) {
            device->waitForFences(imagesInFlight[imageIndex], true, UINT64_MAX);
        }
        imagesInFlight[imageIndex] = inFlightFences[currentFrame];

        vk::PipelineStageFlags waitStage(vk::PipelineStageFlagBits::eColorAttachmentOutput);
        vk::SubmitInfo submitInfo(imageAvailableSemaphores[currentFrame].get(), waitStage,
                                  commandBuffers[imageIndex].get(), renderFinishedSemaphores[currentFrame].get());

        device->resetFences(inFlightFences[currentFrame]);

        queue.submit(submitInfo, inFlightFences[currentFrame]);

        vk::PresentInfoKHR presentInfo(renderFinishedSemaphores[currentFrame].get(), swapChain.get(), imageIndex);

        queue.presentKHR(presentInfo);

        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    vk::UniqueShaderModule createShaderModule(const std::vector<char>& code)
    {
        vk::ShaderModuleCreateInfo createInfo({}, code.size(), reinterpret_cast<const uint32_t*>(code.data()));

        vk::UniqueShaderModule shaderModule = device->createShaderModuleUnique(createInfo);

        return shaderModule;
    }

    vk::SurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats)
    {
        for (const auto& availableFormat : availableFormats) {
            if (availableFormat.format == vk::Format::eB8G8R8A8Srgb && availableFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
                return availableFormat;
            }
        }

        return availableFormats[0];
    }

    vk::PresentModeKHR chooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes)
    {
        for (const auto& availablePresentMode : availablePresentModes) {
            if (availablePresentMode == vk::PresentModeKHR::eFifoRelaxed) {
                return availablePresentMode;
            }
        }

        return vk::PresentModeKHR::eFifo;
    }

    vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities)
    {
        if (capabilities.currentExtent.width != UINT32_MAX) {
            return capabilities.currentExtent;
        } else {
            int width, height;
            glfwGetFramebufferSize(window, &width, &height);

            vk::Extent2D actualExtent = {
                static_cast<uint32_t>(width),
                static_cast<uint32_t>(height) };

            // std::clamp()を使って分かりやすくしている
            actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
            actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

            return actualExtent;
        }
    }

    SwapChainSupportDetails querySwapChainSupport(vk::PhysicalDevice device)
    {
        SwapChainSupportDetails details;
        details.capabilities = device.getSurfaceCapabilitiesKHR(surface.get());
        details.formats = device.getSurfaceFormatsKHR(surface.get());
        details.presentModes = device.getSurfacePresentModesKHR(surface.get());

        return details;
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
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
