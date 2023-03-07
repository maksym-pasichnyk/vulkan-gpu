#include "vulkan.hpp"

#include <SDL_video.h>
#include <SDL_events.h>

#include <imgui_internal.h>
#include <backends/imgui_impl_sdl2.cpp>
#include <backends/imgui_impl_vulkan.cpp>

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

struct ImGuiRenderer {
    VulkanRenderer* vulkan;

    GpuTexture texture;

    vk::DescriptorSetLayout bind_group_layout;
    GpuGraphicsPipelineState graphics_pipeline_state;

    ImGuiRenderer(VulkanRenderer* vulkan, void* window) : vulkan(vulkan) {
        ImGui::CreateContext();

        ImGui_ImplSDL2_InitForVulkan(static_cast<SDL_Window*>(window));

        CreateFontTexture();
        CreateDeviceObjects();
    }

    ~ImGuiRenderer() {
        vulkan->context.logical_device.destroyDescriptorSetLayout(bind_group_layout);

        vulkan->DestroyTexture(&texture);

        gpu_destroy_graphics_pipeline_state(&vulkan->context, &graphics_pipeline_state);

        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();
    }

    void CreateFontTexture() {
        auto& io = ImGui::GetIO();

        u8* pixels;
        i32 width, height;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

        vulkan->CreateTextureFromMemory(&texture, width, height, pixels);

        io.Fonts->SetTexID(&texture);
    }

    void CreateDeviceObjects() {
        auto vertex_shader_module = vulkan->context.logical_device.createShaderModule(vk::ShaderModuleCreateInfo({}, __glsl_shader_vert_spv));
        auto fragment_shader_module = vulkan->context.logical_device.createShaderModule(vk::ShaderModuleCreateInfo({}, __glsl_shader_frag_spv));

        auto entries = std::array{
            vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment)
        };

        bind_group_layout = vulkan->context.logical_device.createDescriptorSetLayout(vk::DescriptorSetLayoutCreateInfo({}, entries));

        auto bindings = std::array{
            vk::VertexInputBindingDescription(0, sizeof(ImDrawVert), vk::VertexInputRate::eVertex)
        };
        auto attributes = std::array{
            vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32Sfloat, offsetof(ImDrawVert, pos)),
            vk::VertexInputAttributeDescription(1, 0, vk::Format::eR32G32Sfloat, offsetof(ImDrawVert, uv)),
            vk::VertexInputAttributeDescription(2, 0, vk::Format::eR8G8B8A8Unorm, offsetof(ImDrawVert, col)),
        };

        auto color_blend_attachments = std::array{
            vk::PipelineColorBlendAttachmentState()
                .setBlendEnable(VK_TRUE)
                .setSrcColorBlendFactor(vk::BlendFactor::eSrcAlpha)
                .setDstColorBlendFactor(vk::BlendFactor::eOneMinusSrcAlpha)
                .setColorBlendOp(vk::BlendOp::eAdd)
                .setSrcAlphaBlendFactor(vk::BlendFactor::eOne)
                .setDstAlphaBlendFactor(vk::BlendFactor::eOneMinusSrcAlpha)
                .setAlphaBlendOp(vk::BlendOp::eAdd)
                .setColorWriteMask(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA)
        };

        auto bind_group_layouts = std::array{
            bind_group_layout
        };

        auto push_constant_ranges = std::array{
            vk::PushConstantRange(vk::ShaderStageFlagBits::eVertex, 0, sizeof(f32[4]))
        };

        auto state_create_info = GpuGraphicsPipelineStateCreateInfo{
            .vertex_shader_module = vertex_shader_module,
            .vertex_shader_entry_point = "main",
            .fragment_shader_module = fragment_shader_module,
            .fragment_shader_entry_point = "main",
            .rasterization_state = {
                .cull_mode = vk::CullModeFlagBits::eNone
            },
            .depth_stencil_state = {
                .depth_test_enable = false,
                .depth_write_enable = false,
            },
            .color_blend_state = {
                .logic_op_enable = VK_FALSE,
                .attachments = color_blend_attachments
            },
            .vertex_input_state = {
                .bindings = bindings,
                .attributes = attributes
            },
            .bind_group_layouts = bind_group_layouts,
            .push_constant_ranges = push_constant_ranges,
            .render_pass = vulkan->swapchain_render_pass,
        };

        gpu_create_graphics_pipeline_state(&vulkan->context, &graphics_pipeline_state, &state_create_info, nullptr);

        vulkan->context.logical_device.destroyShaderModule(vertex_shader_module);
        vulkan->context.logical_device.destroyShaderModule(fragment_shader_module);
    }

    void RenderDrawData(vk::CommandBuffer command_buffer, ImDrawData* draw_data) {
//    ImGui_ImplVulkan_RenderDrawData(draw_data, command_buffer, pipeline.pipeline);
        auto fb_width = static_cast<i32>(draw_data->DisplaySize.x * draw_data->FramebufferScale.x);
        auto fb_height = static_cast<i32>(draw_data->DisplaySize.y * draw_data->FramebufferScale.y);
        if (fb_width <= 0 || fb_height <= 0) {
            return;
        }

        GpuBufferInfo ia = {};
        GpuBufferInfo va = {};

        if (draw_data->TotalVtxCount > 0) {
            // Create or resize the vertex/index buffers
            if (!vulkan->AllocateTemporary(&ia, draw_data->TotalVtxCount * sizeof(ImDrawVert), alignof(ImDrawVert))) {
                fprintf(stderr, "Failed to allocate vertex buffer for ImGui\n");
                return;
            }
            if (!vulkan->AllocateTemporary(&va, draw_data->TotalIdxCount * sizeof(ImDrawIdx), alignof(ImDrawIdx))) {
                fprintf(stderr, "Failed to allocate index buffer for ImGui\n");
                return;
            }

            // Upload vertex/index data into a single contiguous GPU buffer
            auto* vtx_dst = static_cast<ImDrawVert*>(va.mapped);
            auto* idx_dst = static_cast<ImDrawIdx*>(ia.mapped);
            for (auto cmd_list : std::span(draw_data->CmdLists, draw_data->CmdListsCount)) {
                std::memcpy(vtx_dst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
                std::memcpy(idx_dst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
                vtx_dst += cmd_list->VtxBuffer.Size;
                idx_dst += cmd_list->IdxBuffer.Size;
            }
        }

        SetupRenderState(command_buffer, draw_data, &va, &ia, fb_width, fb_height);

        auto clip_off = draw_data->DisplayPos;
        auto clip_scale = draw_data->FramebufferScale;

        i32 global_vtx_offset = 0;
        i32 global_idx_offset = 0;
        for (auto cmd_list : std::span(draw_data->CmdLists, draw_data->CmdListsCount)) {
            for (auto& draw_cmd : std::span(cmd_list->CmdBuffer.Data, cmd_list->CmdBuffer.Size)) {
                if (draw_cmd.UserCallback != nullptr) {
                    if (draw_cmd.UserCallback == ImDrawCallback_ResetRenderState) {
                        SetupRenderState(command_buffer, draw_data, &va, &ia, fb_width, fb_height);
                    } else {
                        draw_cmd.UserCallback(cmd_list, &draw_cmd);
                    }
                    continue;
                }

                auto clip_rect = ImRect(
                    (ImVec2(draw_cmd.ClipRect.x, draw_cmd.ClipRect.y) - clip_off) * clip_scale,
                    (ImVec2(draw_cmd.ClipRect.z, draw_cmd.ClipRect.w) - clip_off) * clip_scale
                );
                clip_rect.ClipWith(ImRect(0, 0, static_cast<f32>(fb_width), static_cast<f32>(fb_height)));

                if (clip_rect.Min.x >= clip_rect.Max.x || clip_rect.Min.y >= clip_rect.Max.y) {
                    continue;
                }

                auto scissor = vk::Rect2D{
                    vk::Offset2D{
                        static_cast<i32>(clip_rect.Min.x),
                        static_cast<i32>(clip_rect.Min.y),
                    },
                    vk::Extent2D{
                        static_cast<u32>(clip_rect.GetWidth()),
                        static_cast<u32>(clip_rect.GetHeight())
                    }
                };
                command_buffer.setScissor(0, 1, &scissor);

                vk::DescriptorSet bind_group;
                {
                    auto allocate_info = vk::DescriptorSetAllocateInfo()
                        .setDescriptorPool(vulkan->context.bind_group_allocators[vulkan->frame_index])
                        .setDescriptorSetCount(1)
                        .setPSetLayouts(&bind_group_layout);

                    auto result = vulkan->context.logical_device.allocateDescriptorSets(&allocate_info, &bind_group);
                    if (result != vk::Result::eSuccess) {
                        fprintf(stderr, "Failed to allocate descriptor set for ImGui\n");
                        return;
                    }

                    auto p_texture = static_cast<GpuTexture*>(draw_cmd.TextureId);

                    auto image_info = vk::DescriptorImageInfo()
                        .setSampler(p_texture->sampler)
                        .setImageView(p_texture->view)
                        .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

                    auto writes = std::array{
                        vk::WriteDescriptorSet()
                            .setDstSet(bind_group)
                            .setDstBinding(0)
                            .setDstArrayElement(0)
                            .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                            .setDescriptorCount(1)
                            .setPImageInfo(&image_info),
                    };

                    vulkan->context.logical_device.updateDescriptorSets(writes, nullptr);
                }

                command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, graphics_pipeline_state.pipeline_layout, 0, 1, &bind_group, 0, nullptr);
                command_buffer.drawIndexed(draw_cmd.ElemCount, 1, draw_cmd.IdxOffset + global_idx_offset, static_cast<i32>(draw_cmd.VtxOffset + global_vtx_offset), 0);
            }
            global_idx_offset += cmd_list->IdxBuffer.Size;
            global_vtx_offset += cmd_list->VtxBuffer.Size;
        }
    }

    void SetupRenderState(vk::CommandBuffer command_buffer, ImDrawData* draw_data, GpuBufferInfo* va, GpuBufferInfo* ia, int fb_width, int fb_height) {
        command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, graphics_pipeline_state.pipeline);

        if (draw_data->TotalVtxCount > 0) {
            command_buffer.bindVertexBuffers(0, 1, &va->buffer, &va->offset);
            command_buffer.bindIndexBuffer(ia->buffer, ia->offset, sizeof(ImDrawIdx) == 2 ? vk::IndexType::eUint16 : vk::IndexType::eUint32);
        }

        auto viewport = vk::Viewport(0, 0, static_cast<f32>(fb_width), static_cast<f32>(fb_height), 0.0f, 1.0f);
        command_buffer.setViewport(0, 1, &viewport);

        float scale[2];
        scale[0] = 2.0f / draw_data->DisplaySize.x;
        scale[1] = 2.0f / draw_data->DisplaySize.y;
        float translate[2];
        translate[0] = -1.0f - draw_data->DisplayPos.x * scale[0];
        translate[1] = -1.0f - draw_data->DisplayPos.y * scale[1];
        command_buffer.pushConstants(graphics_pipeline_state.pipeline_layout, vk::ShaderStageFlagBits::eVertex, sizeof(float) * 0, sizeof(float) * 2, scale);
        command_buffer.pushConstants(graphics_pipeline_state.pipeline_layout, vk::ShaderStageFlagBits::eVertex, sizeof(float) * 2, sizeof(float) * 2, translate);
    }
};

struct App {
    SDL_Window* window;
    VulkanRenderer* vulkan;
    ImGuiRenderer* imgui;

    vk::DescriptorSetLayout compute_bind_group_layout;
    GpuComputePipelineState compute_pipeline_state;

    vk::DescriptorSetLayout graphics_bind_group_layout;
    GpuGraphicsPipelineState graphics_pipeline_state;

    vk::Sampler sampler;

    App() {
        window = SDL_CreateWindow("Hello, world!", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 600, SDL_WINDOW_VULKAN | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE);
        vulkan = new VulkanRenderer(window);
        imgui = new ImGuiRenderer(vulkan, window);

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

        delete imgui;
        delete vulkan;

        SDL_DestroyWindow(window);
    }

    void Start() {
        while (PumpEvents()) {
            ImGui_ImplSDL2_NewFrame();
            ImGui::NewFrame();

            ImGui::Begin("Debug");
            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
            ImGui::End();

            ImGui::Render();

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

                        auto result = vulkan->context.logical_device.allocateDescriptorSets(&allocate_info, &bind_group);
                        if (result != vk::Result::eSuccess) {
                            fprintf(stderr, "Failed to allocate descriptor set %s\n", vk::to_string(result).c_str());
                            exit(1);
                        }

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

                    auto result = vulkan->context.logical_device.allocateDescriptorSets(&allocate_info, &bind_group);
                    if (result != vk::Result::eSuccess) {
                        fprintf(stderr, "Failed to allocate descriptor set %s\n", vk::to_string(result).c_str());
                        exit(1);
                    }

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

                imgui->RenderDrawData(cmd, ImGui::GetDrawData());

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
        gpu_create_graphics_pipeline_state(&vulkan->context, &graphics_pipeline_state, &state_create_info, nullptr);

        vulkan->context.logical_device.destroyShaderModule(vertex_shader_module);
        vulkan->context.logical_device.destroyShaderModule(fragment_shader_module);
    }
};

auto main() -> i32 {
    auto app = App();
    app.Start();
    return 0;
}
