//
// Created by Maksym Pasichnyk on 06.03.2023.
//

#pragma once

#include "gpu.hpp"
#include "result.hpp"
#include "ManagedObject.hpp"
#include "WindowPlatform.hpp"

struct SurfaceConfiguration {
    vk::Extent2D        extent          = {};
    vk::Format          format          = {};
    vk::ColorSpaceKHR   color_space     = {};
    vk::PresentModeKHR  present_mode    = {};
    u32                 min_image_count = {};
};

class VulkanRenderer : public ManagedObject {
public:
    i32 max_frames_in_flight = 3;

    vk::DynamicLoader               loader;
    GpuContext                      context;

    vk::SwapchainKHR                swapchain;
    SurfaceConfiguration            configuration;

    std::vector<vk::Image>          swapchain_images;
    std::vector<vk::ImageView>      swapchain_views;

    std::vector<vk::Fence>          in_flight_fences;
    std::vector<vk::Semaphore>      image_available_semaphores;
    std::vector<vk::Semaphore>      render_finished_semaphores;

    std::vector<GpuCommandBuffer>   command_buffers;
    
    u32                             current_image_index = 0;
    usize                           current_frame_index = 0;
    GpuCommandBuffer*               current_command_buffer = {};

public:
    VulkanRenderer(WindowPlatform* platform) {
        gpu_create_context(&context, platform, loader.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr"));

        configuration.format = vk::Format::eB8G8R8A8Unorm;
        configuration.color_space = vk::ColorSpaceKHR::eSrgbNonlinear;
        configuration.min_image_count = 3u;

        ConfigureSwapchain();
        CreateDeviceResources();
    }

    ~VulkanRenderer() override {
        CleanupDeviceResources();
        CleanupSwapchain();

        gpu_destroy_context(&context);
    }

    void CreateDeviceResources() {
        command_buffers.resize(max_frames_in_flight);
        in_flight_fences.resize(max_frames_in_flight);
        image_available_semaphores.resize(max_frames_in_flight);
        render_finished_semaphores.resize(max_frames_in_flight);

        for (size_t i = 0; i < max_frames_in_flight; i++) {
            gpu_create_command_buffer(&context, &command_buffers[i]);

            in_flight_fences[i] = context.logical_device.createFence(vk::FenceCreateInfo(vk::FenceCreateFlagBits::eSignaled));
            image_available_semaphores[i] = context.logical_device.createSemaphore(vk::SemaphoreCreateInfo());
            render_finished_semaphores[i] = context.logical_device.createSemaphore(vk::SemaphoreCreateInfo());
        }
    }

    void CleanupDeviceResources() {
        for (size_t i = 0; i < max_frames_in_flight; i++) {
            gpu_destroy_command_buffer(&context, &command_buffers[i]);

            context.logical_device.destroyFence(in_flight_fences[i]);
            context.logical_device.destroySemaphore(image_available_semaphores[i]);
            context.logical_device.destroySemaphore(render_finished_semaphores[i]);
        }
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
//        swapchain_framebuffers.resize(swapchain_images.size());

        for (size_t i = 0; i < swapchain_images.size(); i++) {
            vk::ImageViewCreateInfo image_view_info = {};
            image_view_info.setImage(swapchain_images[i]);
            image_view_info.setViewType(vk::ImageViewType::e2D);
            image_view_info.setFormat(configuration.format);
            image_view_info.setSubresourceRange(vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1));

            vk::resultCheck(context.logical_device.createImageView(&image_view_info, nullptr, &swapchain_views[i]), "Failed to create image view");
        }
    }

    void CleanupSwapchain() {
        for (size_t i = 0; i < swapchain_images.size(); i++) {
            context.logical_device.destroyImageView(swapchain_views[i]);
//            context.logical_device.destroyFramebuffer(swapchain_framebuffers[i]);
        }
        context.logical_device.destroySwapchainKHR(swapchain);
    }

    void RebuildSwapchain() {
        context.logical_device.waitIdle();

        CleanupSwapchain();
        ConfigureSwapchain();
    }

    void WaitAndBeginNewFrame() {
        vk::resultCheck(context.logical_device.waitForFences(in_flight_fences[current_frame_index], VK_TRUE, UINT64_MAX), "Failed to wait for fence");

        auto result = context.logical_device.acquireNextImageKHR(swapchain, UINT64_MAX, image_available_semaphores[current_frame_index], nullptr, &current_image_index);
        if (result != vk::Result::eErrorOutOfDateKHR && result != vk::Result::eSuboptimalKHR) {
            vk::resultCheck(result, "Failed to acquire swapchain image");
        }

        current_command_buffer = &command_buffers[current_frame_index];

        gpu_reset_command_buffer(&context, current_command_buffer);
        current_command_buffer->cmd_buffer.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));
    }

    void SubmitFrameAndPresent() {
        current_command_buffer->cmd_buffer.end();

        auto wait_stages = std::array{
            vk::PipelineStageFlags(vk::PipelineStageFlagBits::eColorAttachmentOutput)
        };

        auto submit_info = vk::SubmitInfo()
            .setWaitSemaphoreCount(1)
            .setPWaitSemaphores(&image_available_semaphores[current_frame_index])
            .setPWaitDstStageMask(wait_stages.data())
            .setCommandBufferCount(1)
            .setPCommandBuffers(&current_command_buffer->cmd_buffer)
            .setSignalSemaphoreCount(1)
            .setPSignalSemaphores(&render_finished_semaphores[current_frame_index]);

        context.logical_device.resetFences(in_flight_fences[current_frame_index]);
        context.graphics_queue.submit(submit_info, in_flight_fences[current_frame_index]);

        auto present_info = vk::PresentInfoKHR()
            .setWaitSemaphoreCount(1)
            .setPWaitSemaphores(&render_finished_semaphores[current_frame_index])
            .setSwapchainCount(1)
            .setPSwapchains(&swapchain)
            .setPImageIndices(&current_image_index);

        auto result = context.present_queue.presentKHR(present_info);
        if (result != vk::Result::eErrorOutOfDateKHR && result != vk::Result::eSuboptimalKHR) {
            vk::resultCheck(result, "Failed to present swapchain image");
        }
        current_frame_index = (current_frame_index + 1) % max_frames_in_flight;
    }

    auto ReadBytes(const std::string& filename) -> Result<std::vector<char>, std::runtime_error> {
        std::ifstream file(filename, std::ios::binary);
        if (!file.is_open()) {
            return std::runtime_error("Failed to open file");
        }

        return std::vector(std::istreambuf_iterator<char>(file), {});
    }

    void CreateTextureFromMemory(GpuTexture* texture, u32 width, u32 height, void* pixels) {
        vk::ImageCreateInfo image_create_info = {};
        image_create_info.setImageType(vk::ImageType::e2D);
        image_create_info.setFormat(vk::Format::eR8G8B8A8Unorm);
        image_create_info.setExtent(vk::Extent3D(width, height, 1));
        image_create_info.setMipLevels(1);
        image_create_info.setArrayLayers(1);
        image_create_info.setSamples(vk::SampleCountFlagBits::e1);
        image_create_info.setTiling(vk::ImageTiling::eOptimal);
        image_create_info.setUsage(vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst);
        image_create_info.setSharingMode(vk::SharingMode::eExclusive);
        image_create_info.setQueueFamilyIndices({});
        image_create_info.setInitialLayout(vk::ImageLayout::eUndefined);

        vk::resultCheck(context.logical_device.createImage(&image_create_info, nullptr, &texture->image), "Failed to create image");

        gpu_texture_storage(&context, &texture->allocation, texture->image, GpuStorageMode::ePrivate, {});

        vk::ImageViewCreateInfo view_create_info = {};
        view_create_info.setImage(texture->image);
        view_create_info.setViewType(vk::ImageViewType::e2D);
        view_create_info.setFormat(vk::Format::eR8G8B8A8Unorm);
        view_create_info.setSubresourceRange(vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1));

        vk::resultCheck(context.logical_device.createImageView(&view_create_info, nullptr, &texture->view), "Failed to create image view");

        vk::SamplerCreateInfo sampler_create_info = {};
        sampler_create_info.setMagFilter(vk::Filter::eNearest);
        sampler_create_info.setMinFilter(vk::Filter::eNearest);
        sampler_create_info.setAddressModeU(vk::SamplerAddressMode::eClampToEdge);
        sampler_create_info.setAddressModeV(vk::SamplerAddressMode::eClampToEdge);
        sampler_create_info.setAddressModeW(vk::SamplerAddressMode::eClampToEdge);
        sampler_create_info.setAnisotropyEnable(false);
        sampler_create_info.setMaxAnisotropy(1.0f);
        sampler_create_info.setBorderColor(vk::BorderColor::eFloatOpaqueWhite);
        sampler_create_info.setUnnormalizedCoordinates(false);
        sampler_create_info.setCompareEnable(false);
        sampler_create_info.setCompareOp(vk::CompareOp::eAlways);
        sampler_create_info.setMipmapMode(vk::SamplerMipmapMode::eNearest);
        sampler_create_info.setMipLodBias(0.0f);
        sampler_create_info.setMinLod(0.0f);
        sampler_create_info.setMaxLod(0.0f);

        vk::resultCheck(context.logical_device.createSampler(&sampler_create_info, nullptr, &texture->sampler), "Failed to create sampler");

//        // TODO: simplify this
        auto size_in_bytes = static_cast<vk::DeviceSize>(width * height * 4);
        {
            GpuCommandBuffer command_buffer;
            gpu_create_command_buffer(&context, &command_buffer);

            GpuBufferInfo tmp;
            gpu_command_buffer_allocate(&context, &command_buffer, &tmp, size_in_bytes, context.physical_device.getProperties().limits.optimalBufferCopyOffsetAlignment);

            std::memcpy(gpu_buffer_contents(&tmp), pixels, size_in_bytes);

            command_buffer.cmd_buffer.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));
            {
                auto barriers = std::array{
                    vk::ImageMemoryBarrier2()
                        .setSrcAccessMask(vk::AccessFlagBits2KHR::eHostWrite)
                        .setDstAccessMask(vk::AccessFlagBits2KHR::eTransferWrite)
                        .setSrcStageMask(vk::PipelineStageFlagBits2KHR::eHost)
                        .setDstStageMask(vk::PipelineStageFlagBits2KHR::eTransfer)
                        .setOldLayout(vk::ImageLayout::eUndefined)
                        .setNewLayout(vk::ImageLayout::eTransferDstOptimal)
                        .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                        .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                        .setImage(texture->image)
                        .setSubresourceRange(vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)),
                };
                command_buffer.cmd_buffer.pipelineBarrier2(vk::DependencyInfo({}, {}, {}, barriers));
            }

            auto region = vk::BufferImageCopy()
                .setBufferOffset(tmp.offset)
                .setImageSubresource(vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1))
                .setImageExtent(vk::Extent3D(width, height, 1));

            command_buffer.cmd_buffer.copyBufferToImage(tmp.buffer, texture->image, vk::ImageLayout::eTransferDstOptimal, 1, &region);

            {
                auto barriers = std::array{
                    vk::ImageMemoryBarrier2()
                        .setSrcAccessMask(vk::AccessFlagBits2KHR::eTransferWrite)
                        .setDstAccessMask(vk::AccessFlagBits2KHR::eShaderRead)
                        .setSrcStageMask(vk::PipelineStageFlagBits2KHR::eTransfer)
                        .setDstStageMask(vk::PipelineStageFlagBits2KHR::eFragmentShader | vk::PipelineStageFlagBits2KHR::eComputeShader)
                        .setOldLayout(vk::ImageLayout::eTransferDstOptimal)
                        .setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                        .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                        .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                        .setImage(texture->image)
                        .setSubresourceRange(vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)),
                };
                command_buffer.cmd_buffer.pipelineBarrier2(vk::DependencyInfo({}, {}, {}, barriers));
            }

            command_buffer.cmd_buffer.end();

            auto fence = context.logical_device.createFence(vk::FenceCreateInfo());
            auto submit_info = vk::SubmitInfo(0, nullptr, nullptr, 1, &command_buffer.cmd_buffer, 0, nullptr);

            vk::resultCheck(context.graphics_queue.submit(1, &submit_info, fence), "Failed to submit command buffer");
            vk::resultCheck(context.logical_device.waitForFences(1, &fence, VK_TRUE, UINT64_MAX), "Failed to wait for fence");
            context.logical_device.destroyFence(fence);

            gpu_destroy_command_buffer(&context, &command_buffer);
        }
    }

    void CleanupTexture(GpuTexture* texture) {
        if (texture->sampler) {
            context.logical_device.destroySampler(texture->sampler);
        }
        if (texture->view) {
            context.logical_device.destroyImageView(texture->view);
        }
        if (texture->image) {
            context.logical_device.destroyImage(texture->image);
        }
        gpu_free_memory(&context, &texture->allocation);
    }
};
