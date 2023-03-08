//
// Created by Maksym Pasichnyk on 07.03.2023.
//

#pragma once

#include "VulkanRenderer.hpp"

#include <imgui_internal.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>

struct ImGuiRenderer {
    VulkanRenderer* vulkan;
    GpuTexture texture;

    vk::DescriptorSetLayout bind_group_layout;
    GpuGraphicsPipelineState graphics_pipeline_state;

    ImGuiRenderer(VulkanRenderer* vulkan, void* window) : vulkan(vulkan) {
        ImGui::CreateContext();

        ImGui_ImplGlfw_InitForVulkan(static_cast<GLFWwindow*>(window), true);
//        ImGui_ImplSDL2_InitForVulkan(static_cast<SDL_Window*>(window));

        CreateFontTexture();
        CreateDeviceObjects();
    }

    ~ImGuiRenderer() {
        vulkan->context.logical_device.destroyDescriptorSetLayout(bind_group_layout);
        vulkan->CleanupTexture(&texture);

        gpu_destroy_graphics_pipeline_state(&vulkan->context, &graphics_pipeline_state);

//        ImGui_ImplSDL2_Shutdown();
        ImGui_ImplGlfw_Shutdown();
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
        auto vertex_shader_module = vulkan->LoadShaderModule("shaders/imgui.vert.spv").value();
        auto fragment_shader_module = vulkan->LoadShaderModule("shaders/imgui.frag.spv").value();

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
                        .setDescriptorPool(vulkan->bind_group_allocators[vulkan->frame_index])
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
            command_buffer.bindIndexBuffer(ia->buffer, ia->offset, vk::IndexType::eUint32);
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