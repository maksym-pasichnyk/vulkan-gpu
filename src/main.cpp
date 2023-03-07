#include "vulkan.hpp"

#include <SDL_video.h>
#include <SDL_events.h>

vk::DispatchLoaderDynamic vk::defaultDispatchLoaderDynamic;

struct RasterizerPushConstants {
    vk::DeviceAddress   index_buffer_reference;
    vk::DeviceAddress   vertex_buffer_reference;
    vk::Extent2D        viewport_size;
    u32                 index_count;
};

struct Vertex {
    float x, y;
};

struct App {
    SDL_Window* window;
    Vulkan* vulkan;

    vk::DescriptorSetLayout compute_bind_group_layout;
    GpuComputePipelineState compute_pipeline_state;

    vk::DescriptorSetLayout graphics_bind_group_layout;
    GpuGraphicsPipelineState graphics_pipeline_state;

    vk::Sampler sampler;

    App() {
        window = SDL_CreateWindow("Hello, world!", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 600, SDL_WINDOW_VULKAN | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE);
        vulkan = new Vulkan(window);

        CreateComputePipelineState();
        CreateGraphicsPipelineState();

        sampler = vulkan->context.logical_device.createSampler(vk::SamplerCreateInfo()
            .setMagFilter(vk::Filter::eNearest)
            .setMinFilter(vk::Filter::eNearest)
            .setAddressModeU(vk::SamplerAddressMode::eClampToEdge)
            .setAddressModeV(vk::SamplerAddressMode::eClampToEdge)
            .setAddressModeW(vk::SamplerAddressMode::eClampToEdge)
            .setAnisotropyEnable(false)
            .setMaxAnisotropy(1.0f)
            .setBorderColor(vk::BorderColor::eFloatOpaqueWhite)
            .setUnnormalizedCoordinates(false)
            .setCompareEnable(false)
            .setCompareOp(vk::CompareOp::eAlways)
            .setMipmapMode(vk::SamplerMipmapMode::eNearest)
            .setMipLodBias(0.0f)
            .setMinLod(0.0f)
            .setMaxLod(0.0f)
        );
    }

    ~App() {
        vulkan->context.logical_device.destroySampler(sampler);

        gpu_destroy_compute_pipeline_state(&vulkan->context, &compute_pipeline_state);
        gpu_destroy_graphics_pipeline_state(&vulkan->context, &graphics_pipeline_state);

        vulkan->context.logical_device.destroyDescriptorSetLayout(compute_bind_group_layout);
        vulkan->context.logical_device.destroyDescriptorSetLayout(graphics_bind_group_layout);

        delete vulkan;
        SDL_DestroyWindow(window);
    }

    void Start() {
        while (PumpEvents()) {
            vulkan->WaitAndBeginNewFrame();
            {
                auto cmd = vulkan->context.command_buffers[vulkan->frame_index];

                auto vertices = std::array{
                    Vertex{  0,   0},
                    Vertex{100,   0},
                    Vertex{  0, 100},
                    Vertex{100, 100},
                };

                auto indices = std::array{
                    0u, 1u, 2u,
                    2u, 1u, 3u
                };

                GpuBufferInfo index_buffer_info{};
                GpuBufferInfo vertex_buffer_info{};
                vulkan->AllocateTemporary(&index_buffer_info, sizeof(indices), alignof(u32));
                vulkan->AllocateTemporary(&vertex_buffer_info, sizeof(vertices), alignof(Vertex));

                cmd.updateBuffer(index_buffer_info.buffer, index_buffer_info.offset, sizeof(indices), indices.data());
                cmd.updateBuffer(vertex_buffer_info.buffer, vertex_buffer_info.offset, sizeof(vertices), vertices.data());

//                std::memcpy(index_buffer_info.mapped, indices.data(), sizeof(indices));
//                std::memcpy(vertex_buffer_info.mapped, vertices.data(), sizeof(vertices));

                // Rasterizer
                {
                    auto push_constants = RasterizerPushConstants{
                        .index_buffer_reference = index_buffer_info.device_address,
                        .vertex_buffer_reference = vertex_buffer_info.device_address,
                        .viewport_size = vulkan->configuration.extent,
                        .index_count = 6,
                    };

                    cmd.bindPipeline(vk::PipelineBindPoint::eCompute, compute_pipeline_state.pipeline);
                    cmd.pushConstants(compute_pipeline_state.pipeline_layout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(push_constants), &push_constants);

                    vk::DescriptorSet bind_group;
                    {
                        auto allocate_info = vk::DescriptorSetAllocateInfo()
                            .setDescriptorPool(vulkan->context.bind_group_allocators[vulkan->frame_index])
                            .setDescriptorSetCount(1)
                            .setPSetLayouts(&compute_bind_group_layout);

                        vulkan->context.logical_device.allocateDescriptorSets(&allocate_info, &bind_group);

                        auto color_image_info = vk::DescriptorImageInfo()
                            .setImageView(vulkan->color_textures[vulkan->image_index].view)
                            .setImageLayout(vk::ImageLayout::eGeneral);

                        auto writes = std::array{
                            vk::WriteDescriptorSet()
                                .setDstSet(bind_group)
                                .setDstBinding(0)
                                .setDstArrayElement(0)
                                .setDescriptorType(vk::DescriptorType::eStorageImage)
                                .setDescriptorCount(1)
                                .setPImageInfo(&color_image_info),
                        };

                        vulkan->context.logical_device.updateDescriptorSets(writes, nullptr);
                    }
                    cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, compute_pipeline_state.pipeline_layout, 0, 1, &bind_group, 0, nullptr);

                    // change image layout to General
                    {
                        auto barriers = std::array{
                            vk::ImageMemoryBarrier2()
                                .setSrcStageMask(vk::PipelineStageFlagBits2::eTopOfPipe)
                                .setDstStageMask(vk::PipelineStageFlagBits2::eComputeShader)
                                .setSrcAccessMask(vk::AccessFlagBits2{})
                                .setDstAccessMask(vk::AccessFlagBits2::eShaderWrite)
                                .setOldLayout(vk::ImageLayout::eUndefined)
                                .setNewLayout(vk::ImageLayout::eGeneral)
                                .setImage(vulkan->color_textures[vulkan->image_index].image)
                                .setSubresourceRange(vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)),
                        };

                        cmd.pipelineBarrier2(vk::DependencyInfo({}, {}, {}, barriers));
                    }
                    i32 group_count_x = static_cast<i32>(vulkan->configuration.extent.width + 15) / 16;
                    i32 group_count_y = static_cast<i32>(vulkan->configuration.extent.height + 15) / 16;

                    cmd.dispatch(group_count_x, group_count_y, 1);
                    // change image layout ShaderReadOnlyOptimal
                    {
                        auto barriers = std::array{
                            vk::ImageMemoryBarrier2()
                                .setSrcStageMask(vk::PipelineStageFlagBits2::eComputeShader)
                                .setDstStageMask(vk::PipelineStageFlagBits2::eFragmentShader)
                                .setSrcAccessMask(vk::AccessFlagBits2::eShaderWrite)
                                .setDstAccessMask(vk::AccessFlagBits2::eShaderRead)
                                .setOldLayout(vk::ImageLayout::eGeneral)
                                .setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                                .setImage(vulkan->color_textures[vulkan->image_index].image)
                                .setSubresourceRange(vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)),
                        };

                        cmd.pipelineBarrier2(vk::DependencyInfo({}, {}, {}, barriers));
                    }
                }

                auto render_area = vk::Rect2D(vk::Offset2D(0, 0), vulkan->configuration.extent);
                auto render_viewport = vk::Viewport()
                    .setX(0)
                    .setY(0)
                    .setWidth(static_cast<f32>(vulkan->configuration.extent.width))
                    .setHeight(static_cast<f32>(vulkan->configuration.extent.height))
                    .setMinDepth(0)
                    .setMaxDepth(1);

                auto clear_values = std::array{
                    vk::ClearValue(vk::ClearColorValue(std::array{0.0f, 0.0f, 0.0f, 1.0f})),
                };

                auto render_pass_begin_info = vk::RenderPassBeginInfo()
                    .setRenderPass(vulkan->swapchain_render_pass)
                    .setFramebuffer(vulkan->swapchain_framebuffers[vulkan->image_index])
                    .setRenderArea(render_area)
                    .setClearValues(clear_values);

                cmd.beginRenderPass(render_pass_begin_info, vk::SubpassContents::eInline);
                cmd.setViewport(0, render_viewport);
                cmd.setScissor(0, render_area);

                cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, graphics_pipeline_state.pipeline);

                vk::DescriptorSet bind_group;
                {
                    auto allocate_info = vk::DescriptorSetAllocateInfo()
                        .setDescriptorPool(vulkan->context.bind_group_allocators[vulkan->frame_index])
                        .setDescriptorSetCount(1)
                        .setPSetLayouts(&graphics_bind_group_layout);

                    vulkan->context.logical_device.allocateDescriptorSets(&allocate_info, &bind_group);

                    auto image_info = vk::DescriptorImageInfo()
                        .setSampler(sampler)
                        .setImageView(vulkan->color_textures[vulkan->image_index].view)
                        .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

                    auto writes = std::array{
                        vk::WriteDescriptorSet()
                            .setDstSet(bind_group)
                            .setDstBinding(0)
                            .setDstArrayElement(0)
                            .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                            .setDescriptorCount(1)
                            .setPImageInfo(&image_info)
                    };

                    vulkan->context.logical_device.updateDescriptorSets(writes, nullptr);
                }
                cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, graphics_pipeline_state.pipeline_layout, 0, 1, &bind_group, 0, nullptr);

                cmd.draw(6, 1, 0, 0);
                cmd.endRenderPass();
            }
            vulkan->SubmitFrameAndPresent();
            vulkan->IncrementFrameIndex();
        }

        vulkan->context.logical_device.waitIdle();
    }

    auto PumpEvents() -> bool {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                return false;
            }
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                vulkan->RebuildSwapchain();
                continue;
            }
        }
        return true;
    }

    void CreateComputePipelineState() {
        auto entries = std::array{
            vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eStorageImage, 1, vk::ShaderStageFlagBits::eCompute),
        };
        compute_bind_group_layout = vulkan->context.logical_device.createDescriptorSetLayout(vk::DescriptorSetLayoutCreateInfo({}, entries));

        auto compute_shader_module = vulkan->LoadShaderModule("shaders/rasterizer.comp.spv").value();

        auto bind_group_layouts = std::array{
            compute_bind_group_layout
        };

        auto push_constant_ranges = std::array{
            vk::PushConstantRange(vk::ShaderStageFlagBits::eCompute, 0, sizeof(RasterizerPushConstants))
        };

        auto state_create_info = GpuComputePipelineStateCreateInfo{
            .compute_shader_module = compute_shader_module,
            .compute_shader_entry_point = "main",
            .bind_group_layouts = bind_group_layouts,
            .push_constant_ranges = push_constant_ranges
        };

        gpu_create_compute_pipeline_state(&vulkan->context, &state_create_info, &compute_pipeline_state);

        vulkan->context.logical_device.destroyShaderModule(compute_shader_module);
    }

    void CreateGraphicsPipelineState() {
        auto entries = std::array{
            vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment)
        };

        graphics_bind_group_layout = vulkan->context.logical_device.createDescriptorSetLayout(vk::DescriptorSetLayoutCreateInfo({}, entries));

        auto attachments = std::array {
            vk::PipelineColorBlendAttachmentState{}
                .setBlendEnable(false)
                .setColorWriteMask(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA)
        };

        auto vertex_shader_module = vulkan->LoadShaderModule("shaders/full_screen_quad.vert.spv").value();
        auto fragment_shader_module = vulkan->LoadShaderModule("shaders/full_screen_quad.frag.spv").value();

        auto bind_group_layouts = std::array{
            graphics_bind_group_layout
        };

        auto state_create_info = GpuGraphicsPipelineStateCreateInfo {
            .vertex_shader_module        = vertex_shader_module,
            .vertex_shader_entry_point   = "main",
            .fragment_shader_module      = fragment_shader_module,
            .fragment_shader_entry_point = "main",
            .input_assembly_state        = {},
            .rasterization_state         = {},
            .depth_stencil_state         = {
                .depth_test_enable  = false,
                .depth_write_enable = false,
            },
            .color_blend_state           = {
                .attachments = attachments
            },
            .bind_group_layouts          = bind_group_layouts,
            .push_constant_ranges        = {},
            .render_pass                 = vulkan->swapchain_render_pass,
        };
        gpu_create_graphics_pipeline_state(&vulkan->context, &graphics_pipeline_state, &state_create_info);

        vulkan->context.logical_device.destroyShaderModule(vertex_shader_module);
        vulkan->context.logical_device.destroyShaderModule(fragment_shader_module);
    }
};

auto main() -> i32 {
    auto app = App();
    app.Start();
    return 0;
}
