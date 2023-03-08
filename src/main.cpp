#include "VulkanRenderer.hpp"
#include "ImGuiRenderer.hpp"

#include <SDL_video.h>
#include <SDL_events.h>
#include <imgui_demo.cpp>
#include <backends/imgui_impl_sdl2.cpp>

vk::DispatchLoaderDynamic vk::defaultDispatchLoaderDynamic;

struct RasterizerPushConstants {
    vk::DeviceAddress   index_buffer_reference;
    vk::DeviceAddress   vertex_buffer_reference;
    ImVec2              viewport_scale;
    u32                 index_offset;
    f32                 clip_rect_min_x;
    f32                 clip_rect_min_y;
    f32                 clip_rect_max_x;
    f32                 clip_rect_max_y;
};

struct float2 {
    float x, y;
};

struct float4 {
    float x, y, z, w;
};

struct App {
    SDL_Window*     window;
    VulkanRenderer* vulkan;
    ImGuiRenderer*  imgui;

    vk::DescriptorSetLayout compute_bind_group_layout;
    GpuComputePipelineState compute_pipeline_state;

    vk::DescriptorSetLayout  graphics_bind_group_layout;
    GpuGraphicsPipelineState graphics_pipeline_state;

    bool use_memcpy = false;

    App() {
        window = SDL_CreateWindow("Hello, world!", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 600, SDL_WINDOW_VULKAN | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE);
        vulkan = new VulkanRenderer(window);
        imgui = new ImGuiRenderer(vulkan, window);

        CreateComputePipelineState();
        CreateGraphicsPipelineState();
    }

    ~App() {
        gpu_destroy_compute_pipeline_state(&vulkan->context, &compute_pipeline_state);
        gpu_destroy_graphics_pipeline_state(&vulkan->context, &graphics_pipeline_state);

        vulkan->context.logical_device.destroyDescriptorSetLayout(compute_bind_group_layout);
        vulkan->context.logical_device.destroyDescriptorSetLayout(graphics_bind_group_layout);

        delete imgui;
        delete vulkan;

        SDL_DestroyWindow(window);
    }

    void Start() {
        while (PumpEvents()) {
            Update();

            vulkan->WaitAndBeginNewFrame();

            auto cmd = vulkan->command_buffers[vulkan->frame_index];

            EncodeRasterizer(cmd);
            EncodeSwapchain(cmd);

            vulkan->SubmitFrameAndPresent();
            vulkan->IncrementFrameIndex();
        }

        vulkan->context.logical_device.waitIdle();
    }

    void Update() {
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Stats");
        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
        ImGui::Checkbox("Use memcpy", &use_memcpy);
        ImGui::End();
        ImGui::ShowDemoWindow(nullptr);
        ImGui::Render();
    }

    auto PumpEvents() -> bool {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);

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

    void UpdateBuffer(vk::CommandBuffer cmd, GpuBufferInfo* info, void* src, vk::DeviceSize size) {
        vk::DeviceSize max_data_size = 65536;
        vk::DeviceSize remaining = size;
        vk::DeviceSize offset = 0;

        while (remaining > max_data_size) {
            cmd.updateBuffer(info->buffer, info->offset + offset, max_data_size, reinterpret_cast<u8*>(src) + offset);
            remaining -= max_data_size;
            offset += max_data_size;
        }

        if (remaining > 0) {
            cmd.updateBuffer(info->buffer, info->offset + offset, remaining, reinterpret_cast<u8*>(src) + offset);
        }
    }

    void EncodeRasterizer(vk::CommandBuffer cmd) {
        auto draw_data = ImGui::GetDrawData();

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

        // clear image
        {
            auto clear_value = vk::ClearColorValue(std::array{ 0.0f, 0.0f, 0.0f, 1.0f });
            auto subresource = vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);

            cmd.clearColorImage(vulkan->color_textures[vulkan->image_index].image, vk::ImageLayout::eGeneral, clear_value, subresource);
        }

        vk::DescriptorSet bind_group;
        {
            auto allocate_info = vk::DescriptorSetAllocateInfo()
                .setDescriptorPool(vulkan->bind_group_allocators[vulkan->frame_index])
                .setDescriptorSetCount(1)
                .setPSetLayouts(&compute_bind_group_layout);

            auto result = vulkan->context.logical_device.allocateDescriptorSets(&allocate_info, &bind_group);
            if (result != vk::Result::eSuccess) {
                fprintf(stderr, "Failed to allocate descriptor set %s\n", vk::to_string(result).c_str());
                exit(1);
            }

            auto color_image_info = vk::DescriptorImageInfo()
                .setImageView(vulkan->color_textures[vulkan->image_index].view)
                .setImageLayout(vk::ImageLayout::eGeneral);

            auto texture_image_info = vk::DescriptorImageInfo()
                .setSampler(imgui->texture.sampler)
                .setImageView(imgui->texture.view)
                .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

            auto writes = std::array{
                vk::WriteDescriptorSet()
                    .setDstSet(bind_group)
                    .setDstBinding(0)
                    .setDstArrayElement(0)
                    .setDescriptorType(vk::DescriptorType::eStorageImage)
                    .setDescriptorCount(1)
                    .setPImageInfo(&color_image_info),
                vk::WriteDescriptorSet()
                    .setDstSet(bind_group)
                    .setDstBinding(1)
                    .setDstArrayElement(0)
                    .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                    .setDescriptorCount(1)
                    .setPImageInfo(&texture_image_info)
            };

            vulkan->context.logical_device.updateDescriptorSets(writes, nullptr);
        }

        cmd.bindPipeline(vk::PipelineBindPoint::eCompute, compute_pipeline_state.pipeline);
        cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, compute_pipeline_state.pipeline_layout, 0, 1, &bind_group, 0, nullptr);

        if (draw_data->TotalVtxCount > 0) {
            auto fb_width = static_cast<i32>(draw_data->DisplaySize.x * draw_data->FramebufferScale.x);
            auto fb_height = static_cast<i32>(draw_data->DisplaySize.y * draw_data->FramebufferScale.y);

            auto clip_off = draw_data->DisplayPos;
            auto clip_scale = draw_data->FramebufferScale;

            for (auto cmd_list : std::span(draw_data->CmdLists, draw_data->CmdListsCount)) {
                auto vtx_buffer_size = cmd_list->VtxBuffer.Size * sizeof(ImDrawVert);
                auto idx_buffer_size = cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx);

                GpuBufferInfo vtx_buffer_info;
                GpuBufferInfo idx_buffer_info;
                if (!vulkan->AllocateTemporary(&vtx_buffer_info, vtx_buffer_size, alignof(ImDrawVert))) {
                    fprintf(stderr, "Failed to allocate vertex buffer for ImGui\n");
                    return;
                }

                if (!vulkan->AllocateTemporary(&idx_buffer_info, idx_buffer_size, alignof(ImDrawIdx))) {
                    fprintf(stderr, "Failed to allocate index buffer for ImGui\n");
                    return;
                }

                if (use_memcpy) {
                    std::memcpy(vtx_buffer_info.mapped, cmd_list->VtxBuffer.Data, vtx_buffer_size);
                    std::memcpy(idx_buffer_info.mapped, cmd_list->IdxBuffer.Data, idx_buffer_size);
                } else {
                    UpdateBuffer(cmd, &vtx_buffer_info, cmd_list->VtxBuffer.Data, vtx_buffer_size);
                    UpdateBuffer(cmd, &idx_buffer_info, cmd_list->IdxBuffer.Data, idx_buffer_size);
                }

                auto viewport_scale = ImGui::GetIO().DisplayFramebufferScale;

                for (auto& draw_cmd : std::span(cmd_list->CmdBuffer.Data, cmd_list->CmdBuffer.Size)) {
                    auto clip_rect = ImRect(
                        (ImVec2(draw_cmd.ClipRect.x, draw_cmd.ClipRect.y) - clip_off) * clip_scale,
                        (ImVec2(draw_cmd.ClipRect.z, draw_cmd.ClipRect.w) - clip_off) * clip_scale
                    );
                    clip_rect.ClipWith(ImRect(0, 0, static_cast<f32>(fb_width), static_cast<f32>(fb_height)));

                    if (clip_rect.Min.x >= clip_rect.Max.x || clip_rect.Min.y >= clip_rect.Max.y) {
                        continue;
                    }
                    assert(draw_cmd.VtxOffset == 0);

                    i32 group_count_x = static_cast<i32>(clip_rect.GetWidth() + 7) / 8;
                    i32 group_count_y = static_cast<i32>(clip_rect.GetHeight() + 7) / 8;
                    for (u32 i = 0; i < draw_cmd.ElemCount; i += 3) {
                        auto push_constants = RasterizerPushConstants{
                            .index_buffer_reference = idx_buffer_info.device_address,
                            .vertex_buffer_reference = vtx_buffer_info.device_address,
                            .viewport_scale = viewport_scale,
                            .index_offset = draw_cmd.IdxOffset + i,
                            .clip_rect_min_x = clip_rect.Min.x,
                            .clip_rect_min_y = clip_rect.Min.y,
                            .clip_rect_max_x = clip_rect.Max.x,
                            .clip_rect_max_y = clip_rect.Max.y,
                        };

                        cmd.pushConstants(compute_pipeline_state.pipeline_layout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(push_constants), &push_constants);

                        cmd.dispatch(group_count_x, group_count_y, 1);
                    }
                }
            }
        }

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

    void EncodeSwapchain(vk::CommandBuffer cmd) {
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
                .setDescriptorPool(vulkan->bind_group_allocators[vulkan->frame_index])
                .setDescriptorSetCount(1)
                .setPSetLayouts(&graphics_bind_group_layout);

            auto result = vulkan->context.logical_device.allocateDescriptorSets(&allocate_info, &bind_group);
            if (result != vk::Result::eSuccess) {
                fprintf(stderr, "Failed to allocate descriptor set %s\n", vk::to_string(result).c_str());
                exit(1);
            }

            auto image_info = vk::DescriptorImageInfo()
                .setSampler(vulkan->color_textures[vulkan->image_index].sampler)
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

//        imgui->RenderDrawData(cmd, ImGui::GetDrawData());

        cmd.endRenderPass();
    }

    void CreateComputePipelineState() {
        auto entries = std::array{
            vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eStorageImage, 1, vk::ShaderStageFlagBits::eCompute),
            vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eCompute),
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
        gpu_create_graphics_pipeline_state(&vulkan->context, &graphics_pipeline_state, &state_create_info, nullptr);

        vulkan->context.logical_device.destroyShaderModule(vertex_shader_module);
        vulkan->context.logical_device.destroyShaderModule(fragment_shader_module);
    }
};

auto main() -> i32 {
    setenv("MVK_DEBUG", "1", 1);

    auto app = App();
    app.Start();
    return 0;
}
