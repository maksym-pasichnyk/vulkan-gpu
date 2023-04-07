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

    vk::DynamicLoader                           loader;
    GpuContext                                  context;

    vk::SwapchainKHR                            swapchain;
    SurfaceConfiguration                        configuration;

    std::vector<vk::Image>                      swapchain_images;
    std::vector<vk::ImageView>                  swapchain_views;

    vk::RenderPass                              swapchain_render_pass;
    std::vector<vk::Framebuffer>                swapchain_framebuffers;

    std::vector<vk::Fence>                      in_flight_fences;
    std::vector<vk::Semaphore>                  image_available_semaphores;
    std::vector<vk::Semaphore>                  render_finished_semaphores;

    std::vector<vk::CommandPool>                command_pools;
    std::vector<GpuLinearAllocator>             frame_allocators;
    std::vector<vk::DescriptorPool>             bind_group_allocators;

    u32                                         current_image_index = 0;
    usize                                       current_frame_index = 0;

    vk::CommandBuffer                           current_command_buffer = {};

public:
    VulkanRenderer(WindowPlatform* platform) {
        gpu_create_context(&context, platform, loader.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr"));

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
        CreateDeviceResources();
    }

    ~VulkanRenderer() override {
        CleanupDeviceResources();
        CleanupSwapchain();

        context.logical_device.destroyRenderPass(swapchain_render_pass);
        gpu_destroy_context(&context);
    }

    void CreateDeviceResources() {
        command_pools.resize(max_frames_in_flight);
        frame_allocators.resize(max_frames_in_flight);
        bind_group_allocators.resize(max_frames_in_flight);

        in_flight_fences.resize(max_frames_in_flight);
        image_available_semaphores.resize(max_frames_in_flight);
        render_finished_semaphores.resize(max_frames_in_flight);

        auto pool_sizes = std::array{
            vk::DescriptorPoolSize{vk::DescriptorType::eSampler, 1024},
            vk::DescriptorPoolSize{vk::DescriptorType::eSampledImage, 1024},
            vk::DescriptorPoolSize{vk::DescriptorType::eCombinedImageSampler, 1024},
            vk::DescriptorPoolSize{vk::DescriptorType::eStorageImage, 1024},
            vk::DescriptorPoolSize{vk::DescriptorType::eUniformTexelBuffer, 1024},
            vk::DescriptorPoolSize{vk::DescriptorType::eStorageTexelBuffer, 1024},
            vk::DescriptorPoolSize{vk::DescriptorType::eUniformBuffer, 1024},
            vk::DescriptorPoolSize{vk::DescriptorType::eStorageBuffer, 1024},
            vk::DescriptorPoolSize{vk::DescriptorType::eUniformBufferDynamic, 1024},
            vk::DescriptorPoolSize{vk::DescriptorType::eStorageBufferDynamic, 1024}
        };

        vk::CommandPoolCreateInfo command_pool_create_info = {};
        command_pool_create_info.setQueueFamilyIndex(context.graphics_queue_family_index);
        command_pool_create_info.setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer);

        for (size_t i = 0; i < max_frames_in_flight; i++) {
            gpu_create_allocator(&context, &frame_allocators[i], 5ull * 1024ull * 1024ull);

            command_pools[i] = context.logical_device.createCommandPool(command_pool_create_info);
            bind_group_allocators[i] = context.logical_device.createDescriptorPool(vk::DescriptorPoolCreateInfo({}, 1024, pool_sizes));

            in_flight_fences[i] = context.logical_device.createFence(vk::FenceCreateInfo(vk::FenceCreateFlagBits::eSignaled));
            image_available_semaphores[i] = context.logical_device.createSemaphore(vk::SemaphoreCreateInfo());
            render_finished_semaphores[i] = context.logical_device.createSemaphore(vk::SemaphoreCreateInfo());
        }
    }

    void CleanupDeviceResources() {
        for (size_t i = 0; i < max_frames_in_flight; i++) {
            gpu_destroy_allocator(&context, &frame_allocators[i]);

            context.logical_device.destroyCommandPool(command_pools[i]);
            context.logical_device.destroyDescriptorPool(bind_group_allocators[i]);

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
        swapchain_framebuffers.resize(swapchain_images.size());

        for (size_t i = 0; i < swapchain_images.size(); i++) {
            vk::ImageViewCreateInfo image_view_info = {};
            image_view_info.setImage(swapchain_images[i]);
            image_view_info.setViewType(vk::ImageViewType::e2D);
            image_view_info.setFormat(configuration.format);
            image_view_info.setSubresourceRange(vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1));

            vk::resultCheck(context.logical_device.createImageView(&image_view_info, nullptr, &swapchain_views[i]), "Failed to create image view");

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
        vk::resultCheck(context.logical_device.waitForFences(in_flight_fences[current_frame_index], VK_TRUE, UINT64_MAX), "Failed to wait for fence");

        auto result = context.logical_device.acquireNextImageKHR(swapchain, UINT64_MAX, image_available_semaphores[current_frame_index], nullptr, &current_image_index);
        if (result != vk::Result::eErrorOutOfDateKHR && result != vk::Result::eSuboptimalKHR) {
            vk::resultCheck(result, "Failed to acquire swapchain image");
        }

        gpu_allocator_reset(&frame_allocators[current_frame_index]);

        context.logical_device.resetCommandPool(command_pools[current_frame_index], {});
        context.logical_device.resetDescriptorPool(bind_group_allocators[current_frame_index]);

        vk::CommandBufferAllocateInfo command_buffer_allocate_info = {};
        command_buffer_allocate_info.setCommandPool(command_pools[current_frame_index]);
        command_buffer_allocate_info.setLevel(vk::CommandBufferLevel::ePrimary);
        command_buffer_allocate_info.setCommandBufferCount(1);

        vk::resultCheck(context.logical_device.allocateCommandBuffers(&command_buffer_allocate_info, &current_command_buffer), "Failed to allocate command buffers.");

        current_command_buffer.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));
    }

    void SubmitFrameAndPresent() {
        current_command_buffer.end();

        auto wait_stages = std::array{
            vk::PipelineStageFlags(vk::PipelineStageFlagBits::eColorAttachmentOutput)
        };

        auto submit_info = vk::SubmitInfo()
            .setWaitSemaphoreCount(1)
            .setPWaitSemaphores(&image_available_semaphores[current_frame_index])
            .setPWaitDstStageMask(wait_stages.data())
            .setCommandBufferCount(1)
            .setPCommandBuffers(&current_command_buffer)
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

    auto GetTemporaryBuffer(GpuBufferInfo* info, vk::DeviceSize size, vk::DeviceSize alignment) -> bool {
        return gpu_allocator_allocate(&frame_allocators[current_frame_index], info, size, alignment);
    }

    auto GetTemporaryBindGroup(vk::DescriptorSetLayout bind_group_layout) -> vk::DescriptorSet {
        vk::DescriptorSetAllocateInfo allocate_info = {};
        allocate_info.setDescriptorPool(bind_group_allocators[current_frame_index]);
        allocate_info.setDescriptorSetCount(1);
        allocate_info.setPSetLayouts(&bind_group_layout);

        vk::DescriptorSet bind_group;
        vk::resultCheck(context.logical_device.allocateDescriptorSets(&allocate_info, &bind_group), "Failed to allocate descriptor set");
        return bind_group;
    }

    auto LoadShaderModule(const std::string& filename) -> Result<vk::ShaderModule, std::runtime_error> {
        std::ifstream file(filename, std::ios::binary);
        if (!file.is_open()) {
            return std::runtime_error("Failed to open file");
        }

        std::vector<char> code(std::istreambuf_iterator<char>(file), {});

        vk::ShaderModuleCreateInfo create_info = {};
        create_info.setCodeSize(code.size());
        create_info.setPCode(reinterpret_cast<const u32*>(code.data()));

        return context.logical_device.createShaderModule(create_info);
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

            auto buffer_create_info = vk::BufferCreateInfo()
                .setSize(size_in_bytes)
                .setUsage(vk::BufferUsageFlagBits::eTransferSrc)
                .setSharingMode(vk::SharingMode::eExclusive);

            vk::Buffer tmp_buf;
            vk::resultCheck(context.logical_device.createBuffer(&buffer_create_info, nullptr, &tmp_buf), "Failed to create buffer");

            GpuAllocation tmp_mem;
            gpu_buffer_storage(&context, &tmp_mem, tmp_buf, GpuStorageMode::eShared, {});

            std::memcpy(tmp_mem.mapped, pixels, size_in_bytes);

            vk::CommandPoolCreateInfo cmd_pool_info = {};
            cmd_pool_info.setQueueFamilyIndex(context.graphics_queue_family_index);
            cmd_pool_info.setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer);

            auto cmd_pool = context.logical_device.createCommandPool(cmd_pool_info);

            auto cmd_allocate_info = vk::CommandBufferAllocateInfo()
                .setCommandPool(cmd_pool)
                .setLevel(vk::CommandBufferLevel::ePrimary)
                .setCommandBufferCount(1);

            vk::CommandBuffer cmd;
            vk::resultCheck(context.logical_device.allocateCommandBuffers(&cmd_allocate_info, &cmd), "Failed to allocate command buffer");
            cmd.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));

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
                cmd.pipelineBarrier2(vk::DependencyInfo({}, {}, {}, barriers));
            }

//            vk::BufferImageCopy2 copy_region = {};
//            copy_region.setBufferOffset(0);
//            copy_region.setBufferRowLength(0);
//            copy_region.setBufferImageHeight(0);
//            copy_region.setImageOffset(vk::Offset3D(0, 0, 0));
//            copy_region.setImageExtent(vk::Extent3D(width, height, 1));
//            copy_region.setImageSubresource(vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1));
//
//            vk::CopyBufferToImageInfo2 copy_info = {};
//            copy_info.dstImage = texture->image;
//            copy_info.dstImageLayout = vk::ImageLayout::eTransferDstOptimal;
//            copy_info.regionCount = 1;
//            copy_info.pRegions = &copy_region;
//
//            cmd.copyBufferToImage2(copy_info);

            auto region = vk::BufferImageCopy()
                .setImageSubresource(vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1))
                .setImageExtent(vk::Extent3D(width, height, 1));

            cmd.copyBufferToImage(tmp_buf, texture->image, vk::ImageLayout::eTransferDstOptimal, 1, &region);

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
                cmd.pipelineBarrier2(vk::DependencyInfo({}, {}, {}, barriers));
            }
            cmd.end();

            auto fence = context.logical_device.createFence(vk::FenceCreateInfo());
            auto submit_info = vk::SubmitInfo(0, nullptr, nullptr, 1, &cmd, 0, nullptr);

            vk::resultCheck(context.graphics_queue.submit(1, &submit_info, fence), "Failed to submit command buffer");
            vk::resultCheck(context.logical_device.waitForFences(1, &fence, VK_TRUE, UINT64_MAX), "Failed to wait for fence");
            context.logical_device.destroyFence(fence);
            context.logical_device.destroyBuffer(tmp_buf);
            gpu_free_memory(&context, &tmp_mem);

            context.logical_device.destroyCommandPool(cmd_pool);
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
