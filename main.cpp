#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <fstream>
#include <iostream>
#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

std::vector<char> readFile(const std::string& filename)
{
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "failed to open file." << std::endl;
    }

    size_t fileSize = file.tellg();
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();
    return buffer;
}

VKAPI_ATTR VkBool32 VKAPI_CALL debugUtilsMessengerCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageTypes,
    VkDebugUtilsMessengerCallbackDataEXT const* pCallbackData,
    void* pUserData)
{
    std::cerr << pCallbackData->pMessage << "\n\n";
    return VK_FALSE;
}

int main()
{
    // create window
    const uint32_t width = 800;
    const uint32_t height = 600;
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    GLFWwindow* window = glfwCreateWindow(width, height, "Vulkan", nullptr, nullptr);

    // create instance
    static vk::DynamicLoader dl;
    auto vkGetInstanceProcAddr = dl.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
    VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);

    std::vector instanceLayers{ "VK_LAYER_KHRONOS_validation", "VK_LAYER_LUNARG_monitor" };

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

    // setup debug messenger
    vk::DebugUtilsMessengerCreateInfoEXT messengerInfo;
    messengerInfo.setMessageSeverity(vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);
    messengerInfo.setMessageType(vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);
    messengerInfo.setPfnUserCallback(&debugUtilsMessengerCallback);
    vk::UniqueDebugUtilsMessengerEXT debugUtilsMessenger = instance->createDebugUtilsMessengerEXTUnique(messengerInfo);

    // pick physical device
    vk::PhysicalDevice physicalDevice = instance->enumeratePhysicalDevices().front();

    // create surface
    VkSurfaceKHR _surface;
    if (glfwCreateWindowSurface(*instance, window, nullptr, &_surface) != VK_SUCCESS) {
        std::cerr << "failed to create window surface." << std::endl;
    }
    vk::UniqueSurfaceKHR surface = vk::UniqueSurfaceKHR{ _surface, { *instance } };

    // find queue family
    uint32_t queueFamilyIndex;
    std::vector queueFamilies = physicalDevice.getQueueFamilyProperties();
    for (int i = 0; i < queueFamilies.size(); i++) {
        auto supportGraphics = queueFamilies[i].queueFlags & vk::QueueFlagBits::eGraphics;
        auto supportPresent = physicalDevice.getSurfaceSupportKHR(i, *surface);
        if (supportGraphics && supportPresent) {
            queueFamilyIndex = i;
        }
    }

    // create logical device
    float queuePriority = 1.0f;
    vk::DeviceQueueCreateInfo queueCreateInfo;
    queueCreateInfo.setQueueFamilyIndex(queueFamilyIndex);
    queueCreateInfo.setQueuePriorities(queuePriority);

    vk::PhysicalDeviceDynamicRenderingFeatures dynamicRenderingFeatures{ true };

    std::vector deviceExtensions{ VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    vk::DeviceCreateInfo deviceInfo;
    deviceInfo.setQueueCreateInfos(queueCreateInfo);
    deviceInfo.setPEnabledExtensionNames(deviceExtensions);
    deviceInfo.setPNext(&dynamicRenderingFeatures);
    vk::UniqueDevice device = physicalDevice.createDeviceUnique(deviceInfo);
    vk::Queue queue = device->getQueue(queueFamilyIndex, 0);

    // create swapchain
    uint32_t swapchainImageCount = 3;
    vk::Format swapchainImageFormat = vk::Format::eB8G8R8A8Unorm;

    vk::SwapchainCreateInfoKHR swapchainInfo;
    swapchainInfo.setSurface(*surface);
    swapchainInfo.setMinImageCount(swapchainImageCount);
    swapchainInfo.setImageFormat(swapchainImageFormat);
    swapchainInfo.setImageExtent({ width, height });
    swapchainInfo.setImageArrayLayers(1);
    swapchainInfo.setImageUsage(vk::ImageUsageFlagBits::eColorAttachment);
    vk::UniqueSwapchainKHR swapchain = device->createSwapchainKHRUnique(swapchainInfo);
    std::vector swapchainImages = device->getSwapchainImagesKHR(*swapchain);

    // create image views
    std::vector<vk::UniqueImageView> swapchainImageViews(swapchainImageCount);
    for (size_t i = 0; i < swapchainImages.size(); i++) {
        vk::ImageViewCreateInfo viewInfo;
        viewInfo.setImage(swapchainImages[i]);
        viewInfo.setViewType(vk::ImageViewType::e2D);
        viewInfo.setFormat(swapchainImageFormat);
        viewInfo.setSubresourceRange({ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
        swapchainImageViews[i] = device->createImageViewUnique(viewInfo);
    }

    // create graphics pipeline
    auto vertCode = readFile("../shaders/vert.spv");
    auto fragCode = readFile("../shaders/frag.spv");
    vk::ShaderModuleCreateInfo vertModuleInfo{ {}, vertCode.size(), reinterpret_cast<const uint32_t*>(vertCode.data()) };
    vk::ShaderModuleCreateInfo fragModuleInfo{ {}, fragCode.size(), reinterpret_cast<const uint32_t*>(fragCode.data()) };

    vk::UniqueShaderModule vertShaderModule = device->createShaderModuleUnique(vertModuleInfo);
    vk::UniqueShaderModule fragShaderModule = device->createShaderModuleUnique(fragModuleInfo);

    std::array<vk::PipelineShaderStageCreateInfo, 2> shaderStages;
    shaderStages[0].setStage(vk::ShaderStageFlagBits::eVertex);
    shaderStages[0].setModule(*vertShaderModule);
    shaderStages[0].setPName("main");
    shaderStages[1].setStage(vk::ShaderStageFlagBits::eFragment);
    shaderStages[1].setModule(*fragShaderModule);
    shaderStages[1].setPName("main");

    vk::PipelineVertexInputStateCreateInfo vertexInput;
    vk::PipelineInputAssemblyStateCreateInfo inputAssembly{ {}, vk::PrimitiveTopology::eTriangleList };

    vk::Viewport viewport{ 0.0f, 0.0f, width, height };
    vk::Rect2D scissor{ { 0, 0 }, { width, height } };
    vk::PipelineViewportStateCreateInfo viewportState{ {}, viewport, scissor };

    vk::PipelineRasterizationStateCreateInfo rasterization;
    rasterization.setLineWidth(1.0f);

    vk::PipelineMultisampleStateCreateInfo multisampling;

    vk::PipelineColorBlendAttachmentState colorBlendAttachment;
    colorBlendAttachment.setColorWriteMask(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                                           vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);

    vk::PipelineColorBlendStateCreateInfo colorBlending;
    colorBlending.setAttachments(colorBlendAttachment);

    vk::PipelineLayoutCreateInfo pipelineLayoutInfo;
    vk::UniquePipelineLayout pipelineLayout = device->createPipelineLayoutUnique(pipelineLayoutInfo);

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
    vk::UniquePipeline graphicsPipeline = std::move(device->createGraphicsPipelineUnique({}, pipelineInfo).value);

    // create command pool
    vk::CommandPoolCreateInfo commandPoolInfo;
    commandPoolInfo.setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer);
    commandPoolInfo.setQueueFamilyIndex(queueFamilyIndex);
    vk::UniqueCommandPool commandPool = device->createCommandPoolUnique(commandPoolInfo);

    // create command buffers
    vk::CommandBufferAllocateInfo commandBufferInfo;
    commandBufferInfo.setCommandPool(*commandPool);
    commandBufferInfo.setCommandBufferCount(swapchainImageCount);
    std::vector commandBuffers = device->allocateCommandBuffersUnique(commandBufferInfo);

    // main loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        vk::UniqueSemaphore acquired = device->createSemaphoreUnique(vk::SemaphoreCreateInfo{});
        uint32_t imageIndex = device->acquireNextImageKHR(*swapchain, UINT64_MAX, *acquired).value;

        commandBuffers[imageIndex]->begin(vk::CommandBufferBeginInfo{});
        {
            vk::RenderingAttachmentInfo colorAttachment;
            colorAttachment.setImageView(*swapchainImageViews[imageIndex]);
            colorAttachment.setImageLayout(vk::ImageLayout::eAttachmentOptimal);

            vk::RenderingInfo renderingInfo;
            renderingInfo.setRenderArea({ {0, 0}, {width, height} });
            renderingInfo.setLayerCount(1);
            renderingInfo.setColorAttachments(colorAttachment);
            commandBuffers[imageIndex]->beginRendering(renderingInfo);
            commandBuffers[imageIndex]->bindPipeline(vk::PipelineBindPoint::eGraphics, *graphicsPipeline);
            commandBuffers[imageIndex]->draw(3, 1, 0, 0);
            commandBuffers[imageIndex]->endRendering();

            vk::ImageMemoryBarrier imageMemoryBarrier;
            imageMemoryBarrier.setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);
            imageMemoryBarrier.setNewLayout(vk::ImageLayout::ePresentSrcKHR);
            imageMemoryBarrier.setImage(swapchainImages[imageIndex]);
            imageMemoryBarrier.setSubresourceRange({ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });

            commandBuffers[imageIndex]->pipelineBarrier(
                vk::PipelineStageFlagBits::eColorAttachmentOutput,
                vk::PipelineStageFlagBits::eBottomOfPipe,
                {}, {}, {}, imageMemoryBarrier);
        }
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
