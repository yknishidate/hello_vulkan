#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <fstream>
#include <iostream>
#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

std::vector<uint32_t> readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "failed to open file." << std::endl;
    }
    size_t fileSize = file.tellg();
    std::vector<uint32_t> buffer(fileSize / 4);
    file.seekg(0);
    file.read(reinterpret_cast<char*>(buffer.data()), fileSize);
    file.close();
    return buffer;
}

VKAPI_ATTR VkBool32 VKAPI_CALL debugUtilsMessengerCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageTypes,
    VkDebugUtilsMessengerCallbackDataEXT const* pCallbackData,
    void* pUserData) {
    std::cerr << pCallbackData->pMessage << "\n\n";
    return VK_FALSE;
}

int main() {
    const uint32_t width = 800;
    const uint32_t height = 600;
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    GLFWwindow* window = glfwCreateWindow(width, height, "Vulkan", nullptr, nullptr);

    static vk::DynamicLoader dl;
    auto vkGetInstanceProcAddr = dl.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
    VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);

    std::vector instanceLayers{"VK_LAYER_KHRONOS_validation", "VK_LAYER_LUNARG_monitor"};

    uint32_t glfwExtensionCount;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    std::vector instanceExtensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
    instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    auto appInfo = vk::ApplicationInfo{}
                       .setApiVersion(VK_API_VERSION_1_3);

    vk::UniqueInstance instance = vk::createInstanceUnique(
        vk::InstanceCreateInfo{}
            .setPApplicationInfo(&appInfo)
            .setPEnabledLayerNames(instanceLayers)
            .setPEnabledExtensionNames(instanceExtensions));
    VULKAN_HPP_DEFAULT_DISPATCHER.init(*instance);

    vk::UniqueDebugUtilsMessengerEXT debugUtilsMessenger = instance->createDebugUtilsMessengerEXTUnique(
        vk::DebugUtilsMessengerCreateInfoEXT{}
            .setMessageSeverity(vk::DebugUtilsMessageSeverityFlagBitsEXT::eError)
            .setMessageType(vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation)
            .setPfnUserCallback(&debugUtilsMessengerCallback));

    vk::PhysicalDevice physicalDevice = instance->enumeratePhysicalDevices().front();

    VkSurfaceKHR _surface;
    if (glfwCreateWindowSurface(*instance, window, nullptr, &_surface) != VK_SUCCESS) {
        std::cerr << "failed to create window surface." << std::endl;
    }
    vk::UniqueSurfaceKHR surface = vk::UniqueSurfaceKHR{_surface, {*instance}};

    uint32_t queueFamilyIndex;
    std::vector queueFamilies = physicalDevice.getQueueFamilyProperties();
    for (int i = 0; i < queueFamilies.size(); i++) {
        auto supportGraphics = queueFamilies[i].queueFlags & vk::QueueFlagBits::eGraphics;
        auto supportPresent = physicalDevice.getSurfaceSupportKHR(i, *surface);
        if (supportGraphics && supportPresent) {
            queueFamilyIndex = i;
        }
    }

    float queuePriority = 1.0f;
    auto queueCreateInfo = vk::DeviceQueueCreateInfo{}
                               .setQueueFamilyIndex(queueFamilyIndex)
                               .setQueuePriorities(queuePriority);

    vk::PhysicalDeviceDynamicRenderingFeatures dynamicRenderingFeatures{true};
    std::vector deviceExtensions{VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    vk::UniqueDevice device = physicalDevice.createDeviceUnique(
        vk::DeviceCreateInfo{}
            .setQueueCreateInfos(queueCreateInfo)
            .setPEnabledExtensionNames(deviceExtensions)
            .setPNext(&dynamicRenderingFeatures));
    vk::Queue queue = device->getQueue(queueFamilyIndex, 0);

    uint32_t swapchainImageCount = 3;
    vk::Format swapchainImageFormat = vk::Format::eB8G8R8A8Unorm;
    vk::UniqueSwapchainKHR swapchain = device->createSwapchainKHRUnique(
        vk::SwapchainCreateInfoKHR{}
            .setSurface(*surface)
            .setMinImageCount(swapchainImageCount)
            .setImageFormat(swapchainImageFormat)
            .setImageExtent({width, height})
            .setImageArrayLayers(1)
            .setImageUsage(vk::ImageUsageFlagBits::eColorAttachment));
    std::vector swapchainImages = device->getSwapchainImagesKHR(*swapchain);

    std::vector<vk::UniqueImageView> swapchainImageViews(swapchainImageCount);
    for (size_t i = 0; i < swapchainImages.size(); i++) {
        swapchainImageViews[i] = device->createImageViewUnique(
            vk::ImageViewCreateInfo{}
                .setImage(swapchainImages[i])
                .setViewType(vk::ImageViewType::e2D)
                .setFormat(swapchainImageFormat)
                .setSubresourceRange({vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}));
    }

    auto vertCode = readFile("../shaders/base.vert.spv");
    auto fragCode = readFile("../shaders/base.frag.spv");
    vk::UniqueShaderModule vertShaderModule = device->createShaderModuleUnique({{}, vertCode});
    vk::UniqueShaderModule fragShaderModule = device->createShaderModuleUnique({{}, fragCode});

    std::array shaderStages{
        vk::PipelineShaderStageCreateInfo{{}, vk::ShaderStageFlagBits::eVertex, *vertShaderModule, "main"},
        vk::PipelineShaderStageCreateInfo{{}, vk::ShaderStageFlagBits::eFragment, *fragShaderModule, "main"}};

    vk::PipelineVertexInputStateCreateInfo vertexInput;
    vk::PipelineInputAssemblyStateCreateInfo inputAssembly{{}, vk::PrimitiveTopology::eTriangleList};

    vk::Viewport viewport{0.0f, 0.0f, width, height};
    vk::Rect2D scissor{{0, 0}, {width, height}};
    vk::PipelineViewportStateCreateInfo viewportState{{}, viewport, scissor};

    vk::UniquePipelineLayout pipelineLayout = device->createPipelineLayoutUnique(vk::PipelineLayoutCreateInfo());

    vk::PipelineMultisampleStateCreateInfo multisampling;
    auto rasterization = vk::PipelineRasterizationStateCreateInfo{}
                             .setLineWidth(1.0f);
    auto colorBlendAttachment = vk::PipelineColorBlendAttachmentState{}
                                    .setColorWriteMask(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                                                       vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);
    auto colorBlending = vk::PipelineColorBlendStateCreateInfo{}
                             .setAttachments(colorBlendAttachment);
    auto pipelineRenderingInfo = vk::PipelineRenderingCreateInfo{}
                                     .setColorAttachmentFormats(swapchainImageFormat);
    auto pipelineInfo = vk::GraphicsPipelineCreateInfo{}
                            .setStages(shaderStages)
                            .setPVertexInputState(&vertexInput)
                            .setPInputAssemblyState(&inputAssembly)
                            .setPViewportState(&viewportState)
                            .setPRasterizationState(&rasterization)
                            .setPMultisampleState(&multisampling)
                            .setPColorBlendState(&colorBlending)
                            .setLayout(*pipelineLayout)
                            .setPNext(&pipelineRenderingInfo);
    vk::UniquePipeline graphicsPipeline = device->createGraphicsPipelineUnique({}, pipelineInfo).value;

    vk::UniqueCommandPool commandPool = device->createCommandPoolUnique(
        vk::CommandPoolCreateInfo{}
            .setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer)
            .setQueueFamilyIndex(queueFamilyIndex));

    std::vector commandBuffers = device->allocateCommandBuffersUnique(
        vk::CommandBufferAllocateInfo{}
            .setCommandPool(*commandPool)
            .setCommandBufferCount(swapchainImageCount));

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        vk::UniqueSemaphore acquired = device->createSemaphoreUnique({});
        uint32_t imageIndex = device->acquireNextImageKHR(*swapchain, UINT64_MAX, *acquired).value;

        auto colorAttachment = vk::RenderingAttachmentInfo{}
                                   .setImageView(*swapchainImageViews[imageIndex])
                                   .setImageLayout(vk::ImageLayout::eAttachmentOptimal);

        commandBuffers[imageIndex]->begin(vk::CommandBufferBeginInfo{});
        commandBuffers[imageIndex]->beginRendering(vk::RenderingInfo{}
                                                       .setRenderArea({{0, 0}, {width, height}})
                                                       .setLayerCount(1)
                                                       .setColorAttachments(colorAttachment));
        commandBuffers[imageIndex]->bindPipeline(vk::PipelineBindPoint::eGraphics, *graphicsPipeline);
        commandBuffers[imageIndex]->draw(3, 1, 0, 0);
        commandBuffers[imageIndex]->endRendering();

        auto imageMemoryBarrier = vk::ImageMemoryBarrier{}
                                      .setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite)
                                      .setNewLayout(vk::ImageLayout::ePresentSrcKHR)
                                      .setImage(swapchainImages[imageIndex])
                                      .setSubresourceRange({vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1});
        commandBuffers[imageIndex]->pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput,
                                                    vk::PipelineStageFlagBits::eBottomOfPipe,
                                                    {}, {}, {}, imageMemoryBarrier);
        commandBuffers[imageIndex]->end();

        vk::UniqueSemaphore rendered = device->createSemaphoreUnique({});
        vk::PipelineStageFlags waitStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
        queue.submit(vk::SubmitInfo{}
                         .setWaitSemaphores(*acquired)
                         .setSignalSemaphores(*rendered)
                         .setWaitDstStageMask(waitStage)
                         .setCommandBuffers(*commandBuffers[imageIndex]));

        auto presentInfo = vk::PresentInfoKHR{}
                               .setWaitSemaphores(*rendered)
                               .setSwapchains(*swapchain)
                               .setImageIndices(imageIndex);
        if (queue.presentKHR(presentInfo) != vk::Result::eSuccess) {
            std::cerr << "failed to present." << std::endl;
        }
        queue.waitIdle();
    }
    device->waitIdle();
    glfwDestroyWindow(window);
    glfwTerminate();
}
