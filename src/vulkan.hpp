//
// Created by Maksym Pasichnyk on 06.03.2023.
//

#pragma once

#include "gpu.hpp"
#include "result.hpp"

#include <fstream>

struct SurfaceConfiguration {
    vk::Extent2D        extent          = {};
    vk::Format          format          = {};
    vk::ColorSpaceKHR   color_space     = {};
    vk::PresentModeKHR  present_mode    = {};
    u32                 min_image_count = {};
};

struct Vulkan {
    vk::DynamicLoader                           loader;
    GpuContext                                  context;

    vk::SwapchainKHR                            swapchain;
    SurfaceConfiguration                        configuration;

    std::vector<vk::Image>                      swapchain_images;
    std::vector<vk::ImageView>                  swapchain_views;

    vk::RenderPass                              swapchain_render_pass;
    std::vector<vk::Framebuffer>                swapchain_framebuffers;

    std::vector<GpuTexture>                     color_textures;

    u32                                         image_index = 0;
    usize                                       frame_index = 0;

    Vulkan(SDL_Window* window) {
        gpu_create_context(&context, window, loader.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr"));

        configuration.format = vk::Format::eB8G8R8A8Unorm;
        configuration.color_space = vk::ColorSpaceKHR::eSrgbNonlinear;
        configuration.min_image_count = 3u;

        auto attachments = std::array{
            vk::AttachmentDescription()
                .setFormat(configuration.format)
                .setSamples(vk::SampleCountFlagBits::e1)
                .setLoadOp(vk::AttachmentLoadOp::eClear)
                .setStoreOp(vk::AttachmentStoreOp::eStore)
                .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
                .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
                .setInitialLayout(vk::ImageLayout::eUndefined)
                .setFinalLayout(vk::ImageLayout::ePresentSrcKHR)
        };

        auto color_attachment_references = std::array{
            vk::AttachmentReference()
                .setAttachment(0)
                .setLayout(vk::ImageLayout::eColorAttachmentOptimal)
        };

        auto subpasses = std::array{
            vk::SubpassDescription()
                .setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
                .setColorAttachments(color_attachment_references)
        };

        auto dependencies = std::array{
            vk::SubpassDependency()
                .setSrcSubpass(VK_SUBPASS_EXTERNAL)
                .setDstSubpass(0)
                .setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
                .setSrcAccessMask(vk::AccessFlags())
                .setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
                .setDstAccessMask(vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite),
            vk::SubpassDependency()
                .setSrcSubpass(0)
                .setDstSubpass(VK_SUBPASS_EXTERNAL)
                .setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
                .setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite)
                .setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
                .setDstAccessMask(vk::AccessFlags()),
        };

        auto render_pass_create_info = vk::RenderPassCreateInfo()
            .setAttachments(attachments)
            .setSubpasses(subpasses)
            .setDependencies(dependencies);

        swapchain_render_pass = context.logical_device.createRenderPass(render_pass_create_info);

        ConfigureSwapchain();
    }

    ~Vulkan() {
        CleanupSwapchain();

        context.logical_device.destroyRenderPass(swapchain_render_pass);
        gpu_destroy_context(&context);
    }

    void ConfigureSwapchain() {
        auto formats = context.physical_device.getSurfaceFormatsKHR(context.surface);
        auto capabilities = context.physical_device.getSurfaceCapabilitiesKHR(context.surface);

        configuration.extent = capabilities.currentExtent;

        std::vector<uint32_t> queue_family_indices = {};
        if (context.graphics_queue_family_index != context.present_queue_family_index) {
            queue_family_indices.push_back(context.present_queue_family_index);
            queue_family_indices.push_back(context.graphics_queue_family_index);
        }

        auto image_sharing_mode = queue_family_indices.empty() ? vk::SharingMode::eExclusive : vk::SharingMode::eConcurrent;

        auto swapchain_create_info = vk::SwapchainCreateInfoKHR()
            .setSurface(context.surface)
            .setMinImageCount(configuration.min_image_count)
            .setImageFormat(configuration.format)
            .setImageColorSpace(configuration.color_space)
            .setImageExtent(configuration.extent)
            .setImageArrayLayers(1)
            .setImageUsage(capabilities.supportedUsageFlags)
            .setImageSharingMode(image_sharing_mode)
            .setQueueFamilyIndices(queue_family_indices)
            .setPreTransform(capabilities.currentTransform)
            .setCompositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque)
            .setPresentMode(vk::PresentModeKHR::eFifo)
            .setClipped(VK_TRUE);

        swapchain = context.logical_device.createSwapchainKHR(swapchain_create_info);

        swapchain_images = context.logical_device.getSwapchainImagesKHR(swapchain);

        swapchain_views.resize(swapchain_images.size());
        swapchain_framebuffers.resize(swapchain_images.size());

        color_textures.resize(swapchain_images.size());

        for (size_t i = 0; i < swapchain_images.size(); i++) {
            auto color_image_create_info = vk::ImageCreateInfo()
                .setImageType(vk::ImageType::e2D)
                .setFormat(configuration.format)
                .setExtent(vk::Extent3D(configuration.extent.width, configuration.extent.height, 1))
                .setMipLevels(1)
                .setArrayLayers(1)
                .setSamples(vk::SampleCountFlagBits::e1)
                .setTiling(vk::ImageTiling::eOptimal)
                .setUsage(vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage)
                .setSharingMode(vk::SharingMode::eExclusive)
                .setQueueFamilyIndices({})
                .setInitialLayout(vk::ImageLayout::eUndefined);

            color_textures[i].image = context.logical_device.createImage(color_image_create_info);;

            gpu_allocate_memory(
                &context,
                &color_textures[i].allocation,
                context.logical_device.getImageMemoryRequirements(color_textures[i].image),
                vk::MemoryPropertyFlagBits::eDeviceLocal,
                vk::MemoryAllocateFlagBits{}
            );
            context.logical_device.bindImageMemory(color_textures[i].image, color_textures[i].allocation.device_memory, 0);

            auto color_view_create_info = vk::ImageViewCreateInfo()
                .setImage(color_textures[i].image)
                .setViewType(vk::ImageViewType::e2D)
                .setFormat(configuration.format)
                .setComponents(vk::ComponentMapping()
                    .setR(vk::ComponentSwizzle::eIdentity)
                    .setG(vk::ComponentSwizzle::eIdentity)
                    .setB(vk::ComponentSwizzle::eIdentity)
                    .setA(vk::ComponentSwizzle::eIdentity))
                .setSubresourceRange(vk::ImageSubresourceRange()
                    .setAspectMask(vk::ImageAspectFlagBits::eColor)
                    .setBaseMipLevel(0)
                    .setLevelCount(1)
                    .setBaseArrayLayer(0)
                    .setLayerCount(1));

            color_textures[i].view = context.logical_device.createImageView(color_view_create_info);

            auto swapchain_view_create_info = vk::ImageViewCreateInfo()
                .setImage(swapchain_images[i])
                .setViewType(vk::ImageViewType::e2D)
                .setFormat(configuration.format)
                .setComponents(vk::ComponentMapping()
                    .setR(vk::ComponentSwizzle::eIdentity)
                    .setG(vk::ComponentSwizzle::eIdentity)
                    .setB(vk::ComponentSwizzle::eIdentity)
                    .setA(vk::ComponentSwizzle::eIdentity))
                .setSubresourceRange(vk::ImageSubresourceRange()
                    .setAspectMask(vk::ImageAspectFlagBits::eColor)
                    .setBaseMipLevel(0)
                    .setLevelCount(1)
                    .setBaseArrayLayer(0)
                    .setLayerCount(1));

            swapchain_views[i] = context.logical_device.createImageView(swapchain_view_create_info);

            auto attachments = std::array{
                swapchain_views[i]
            };

            auto framebuffer_create_info = vk::FramebufferCreateInfo()
                .setRenderPass(swapchain_render_pass)
                .setAttachments(attachments)
                .setWidth(configuration.extent.width)
                .setHeight(configuration.extent.height)
                .setLayers(1);

            swapchain_framebuffers[i] = context.logical_device.createFramebuffer(framebuffer_create_info);
        }
    }

    void CleanupSwapchain() {
        for (size_t i = 0; i < swapchain_images.size(); i++) {
            context.logical_device.destroyImage(color_textures[i].image);
            context.logical_device.destroyImageView(color_textures[i].view);
            gpu_free_memory(&context, &color_textures[i].allocation);

            context.logical_device.destroyImageView(swapchain_views[i]);
            context.logical_device.destroyFramebuffer(swapchain_framebuffers[i]);
        }
        context.logical_device.destroySwapchainKHR(swapchain);
    }

    void RebuildSwapchain() {
        context.logical_device.waitIdle();

        CleanupSwapchain();
        ConfigureSwapchain();
    }

    void WaitAndBeginNewFrame() {
        vk::resultCheck(context.logical_device.waitForFences(context.in_flight_fences[frame_index], VK_TRUE, UINT64_MAX), "Failed to wait for fence");

        auto result = context.logical_device.acquireNextImageKHR(swapchain, UINT64_MAX, context.image_available_semaphores[frame_index], nullptr, &image_index);
        if (result != vk::Result::eErrorOutOfDateKHR && result != vk::Result::eSuboptimalKHR) {
            vk::resultCheck(result, "Failed to acquire swapchain image");
        }

        gpu_allocator_reset(&context.frame_allocators[frame_index]);

        context.command_buffers[frame_index].begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));
    }

    void SubmitFrameAndPresent() {
        context.command_buffers[frame_index].end();

        auto wait_stages = std::array{
            vk::PipelineStageFlags(vk::PipelineStageFlagBits::eColorAttachmentOutput)
        };

        auto submit_info = vk::SubmitInfo()
            .setWaitSemaphoreCount(1)
            .setPWaitSemaphores(&context.image_available_semaphores[frame_index])
            .setPWaitDstStageMask(wait_stages.data())
            .setCommandBufferCount(1)
            .setPCommandBuffers(&context.command_buffers[frame_index])
            .setSignalSemaphoreCount(1)
            .setPSignalSemaphores(&context.render_finished_semaphores[frame_index]);

        context.logical_device.resetFences(context.in_flight_fences[frame_index]);
        context.graphics_queue.submit(submit_info, context.in_flight_fences[frame_index]);

        auto present_info = vk::PresentInfoKHR()
            .setWaitSemaphoreCount(1)
            .setPWaitSemaphores(&context.render_finished_semaphores[frame_index])
            .setSwapchainCount(1)
            .setPSwapchains(&swapchain)
            .setPImageIndices(&image_index);

        auto result = context.present_queue.presentKHR(present_info);
        if (result != vk::Result::eErrorOutOfDateKHR && result != vk::Result::eSuboptimalKHR) {
            vk::resultCheck(result, "Failed to present swapchain image");
        }
    }

    void IncrementFrameIndex() {
        frame_index = (frame_index + 1) % context.max_frames_in_flight;
    }

    auto AllocateTemporary(GpuBufferInfo* info, vk::DeviceSize size, vk::DeviceSize alignment) -> bool {
        return gpu_allocator_allocate(&context.frame_allocators[frame_index], info, size, alignment);
    }

    auto LoadShaderModule(const std::string& filename) -> Result<vk::ShaderModule, std::runtime_error> {
        std::ifstream file(filename, std::ios::binary);
        if (!file.is_open()) {
            return std::runtime_error("Failed to open file");
        }

        std::vector<char> code(std::istreambuf_iterator<char>(file), {});

        auto create_info = vk::ShaderModuleCreateInfo()
            .setCodeSize(code.size())
            .setPCode(reinterpret_cast<const u32*>(code.data()));

        return context.logical_device.createShaderModule(create_info);
    }
};
