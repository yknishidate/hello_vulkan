#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#define VULKAN_HPP_NO_CONSTRUCTORS
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
    std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));
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
    uint32_t width = 800;
    uint32_t height = 600;
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

    vk::ApplicationInfo appInfo;
    appInfo.setApiVersion(VK_API_VERSION_1_3);
    vk::InstanceCreateInfo instanceInfo;
    instanceInfo.setPApplicationInfo(&appInfo);
    instanceInfo.setPEnabledLayerNames(instanceLayers);
    instanceInfo.setPEnabledExtensionNames(instanceExtensions);
    vk::UniqueInstance instance = vk::createInstanceUnique(instanceInfo);
    VULKAN_HPP_DEFAULT_DISPATCHER.init(*instance);

    vk::DebugUtilsMessengerCreateInfoEXT messengerInfo;
    messengerInfo.setMessageSeverity(vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);
    messengerInfo.setMessageType(vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);
    messengerInfo.setPfnUserCallback(&debugUtilsMessengerCallback);
    vk::UniqueDebugUtilsMessengerEXT debugUtilsMessenger = instance->createDebugUtilsMessengerEXTUnique(messengerInfo);

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
    vk::DeviceQueueCreateInfo queueCreateInfo;
    queueCreateInfo.setQueueFamilyIndex(queueFamilyIndex);
    queueCreateInfo.setQueuePriorities(queuePriority);

    vk::PhysicalDeviceDynamicRenderingFeatures dynamicRenderingFeatures;
    dynamicRenderingFeatures.setDynamicRendering(true);

    std::vector deviceExtensions{VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    vk::DeviceCreateInfo deviceInfo;
    deviceInfo.setQueueCreateInfos(queueCreateInfo);
    deviceInfo.setPEnabledExtensionNames(deviceExtensions);
    deviceInfo.setPNext(&dynamicRenderingFeatures);
    vk::UniqueDevice device = physicalDevice.createDeviceUnique(deviceInfo);
    vk::Queue queue = device->getQueue(queueFamilyIndex, 0);

    uint32_t swapchainImageCount = 3;
    vk::Format swapchainImageFormat = vk::Format::eB8G8R8A8Unorm;
    vk::SwapchainCreateInfoKHR swapchainInfo;
    swapchainInfo.setSurface(*surface);
    swapchainInfo.setMinImageCount(swapchainImageCount);
    swapchainInfo.setImageFormat(swapchainImageFormat);
    swapchainInfo.setImageExtent({width, height});
    swapchainInfo.setImageArrayLayers(1);
    swapchainInfo.setImageUsage(vk::ImageUsageFlagBits::eColorAttachment);
    vk::UniqueSwapchainKHR swapchain = device->createSwapchainKHRUnique(swapchainInfo);
    std::vector swapchainImages = device->getSwapchainImagesKHR(*swapchain);

    std::vector<vk::UniqueImageView> swapchainImageViews(swapchainImageCount);
    for (size_t i = 0; i < swapchainImages.size(); i++) {
        vk::ImageViewCreateInfo viewInfo;
        viewInfo.setImage(swapchainImages[i]);
        viewInfo.setViewType(vk::ImageViewType::e2D);
        viewInfo.setFormat(swapchainImageFormat);
        viewInfo.setSubresourceRange({vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1});
        swapchainImageViews[i] = device->createImageViewUnique(viewInfo);
    }

    std::vector vertCode = readFile("../shaders/base.vert.spv");
    std::vector fragCode = readFile("../shaders/base.frag.spv");
    vk::ShaderModuleCreateInfo vertShaderInfo;
    vk::ShaderModuleCreateInfo fragShaderInfo;
    vertShaderInfo.setCode(vertCode);
    fragShaderInfo.setCode(fragCode);
    vk::UniqueShaderModule vertShaderModule = device->createShaderModuleUnique(vertShaderInfo);
    vk::UniqueShaderModule fragShaderModule = device->createShaderModuleUnique(fragShaderInfo);

    std::array<vk::PipelineShaderStageCreateInfo, 2> shaderStages;
    shaderStages[0].setStage(vk::ShaderStageFlagBits::eVertex);
    shaderStages[0].setModule(*vertShaderModule);
    shaderStages[0].setPName("main");
    shaderStages[1].setStage(vk::ShaderStageFlagBits::eFragment);
    shaderStages[1].setModule(*fragShaderModule);
    shaderStages[1].setPName("main");

    vk::UniquePipelineLayout pipelineLayout = device->createPipelineLayoutUnique(vk::PipelineLayoutCreateInfo());

    vk::PipelineVertexInputStateCreateInfo vertexInput;
    vk::PipelineInputAssemblyStateCreateInfo inputAssembly;
    inputAssembly.setTopology(vk::PrimitiveTopology::eTriangleList);

    vk::Viewport viewport{0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)};
    vk::Rect2D scissor{{0, 0}, {width, height}};
    vk::PipelineViewportStateCreateInfo viewportState;
    viewportState.setViewports(viewport);
    viewportState.setScissors(scissor);

    vk::PipelineMultisampleStateCreateInfo multisampling;
    vk::PipelineRasterizationStateCreateInfo rasterization;
    rasterization.setLineWidth(1.0f);
    vk::PipelineColorBlendAttachmentState colorBlendAttachment;
    colorBlendAttachment.setColorWriteMask(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                                           vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);
    vk::PipelineColorBlendStateCreateInfo colorBlending;
    colorBlending.setAttachments(colorBlendAttachment);
    vk::PipelineRenderingCreateInfo pipelineRenderingInfo;
    pipelineRenderingInfo.setColorAttachmentFormats(swapchainImageFormat);

    vk::GraphicsPipelineCreateInfo pipelineInfo;
    pipelineInfo.setStages(shaderStages);
    pipelineInfo.setPVertexInputState(&vertexInput);
    pipelineInfo.setPInputAssemblyState(&inputAssembly);
    pipelineInfo.setPViewportState(&viewportState);
    pipelineInfo.setPRasterizationState(&rasterization);
    pipelineInfo.setPMultisampleState(&multisampling);
    pipelineInfo.setPColorBlendState(&colorBlending);
    pipelineInfo.setLayout(*pipelineLayout);
    pipelineInfo.setPNext(&pipelineRenderingInfo);
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

        vk::UniqueSemaphore acquired = device->createSemaphoreUnique(vk::SemaphoreCreateInfo{});
        uint32_t imageIndex = device->acquireNextImageKHR(*swapchain, UINT64_MAX, *acquired).value;

        vk::RenderingAttachmentInfo colorAttachment;
        colorAttachment.setImageView(*swapchainImageViews[imageIndex]);
        colorAttachment.setImageLayout(vk::ImageLayout::eAttachmentOptimal);

        vk::RenderingInfo renderingInfo;
        renderingInfo.setRenderArea({{0, 0}, {width, height}});
        renderingInfo.setLayerCount(1);
        renderingInfo.setColorAttachments(colorAttachment);

        vk::ImageMemoryBarrier imageMemoryBarrier;
        imageMemoryBarrier.setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);
        imageMemoryBarrier.setNewLayout(vk::ImageLayout::ePresentSrcKHR);
        imageMemoryBarrier.setImage(swapchainImages[imageIndex]);
        imageMemoryBarrier.setSubresourceRange({vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1});

        commandBuffers[imageIndex]->begin(vk::CommandBufferBeginInfo{});
        commandBuffers[imageIndex]->beginRendering(renderingInfo);
        commandBuffers[imageIndex]->bindPipeline(vk::PipelineBindPoint::eGraphics, *graphicsPipeline);
        commandBuffers[imageIndex]->draw(3, 1, 0, 0);
        commandBuffers[imageIndex]->endRendering();
        commandBuffers[imageIndex]->pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput,
                                                    vk::PipelineStageFlagBits::eBottomOfPipe,
                                                    {}, {}, {}, imageMemoryBarrier);
        commandBuffers[imageIndex]->end();

        vk::UniqueSemaphore rendered = device->createSemaphoreUnique(vk::SemaphoreCreateInfo{});
        vk::PipelineStageFlags waitStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
        vk::SubmitInfo submitInfo;
        submitInfo.setWaitSemaphores(*acquired);
        submitInfo.setSignalSemaphores(*rendered);
        submitInfo.setWaitDstStageMask(waitStage);
        submitInfo.setCommandBuffers(*commandBuffers[imageIndex]);
        queue.submit(submitInfo);

        vk::PresentInfoKHR presentInfo;
        presentInfo.setWaitSemaphores(*rendered);
        presentInfo.setSwapchains(*swapchain);
        presentInfo.setImageIndices(imageIndex);
        if (queue.presentKHR(presentInfo) != vk::Result::eSuccess) {
            std::cerr << "failed to present." << std::endl;
        }
        queue.waitIdle();
    }
    device->waitIdle();
    glfwDestroyWindow(window);
    glfwTerminate();
}
