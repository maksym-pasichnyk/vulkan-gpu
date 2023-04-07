#include "WindowPlatform.hpp"
#include "VulkanRenderer.hpp"
#include "ImGuiRenderer.hpp"

#include <imgui_demo.cpp>
#include <backends/imgui_impl_glfw.cpp>

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

struct float3 {
    float x, y, z;
};

struct float4 {
    float x, y, z, w;
};

struct App {
    WindowPlatform*             platform;
    VulkanRenderer*             vulkan;
    ImGuiRenderer*              imgui;

    vk::DescriptorSetLayout     compute_bind_group_layout;
    GpuComputePipelineState     compute_pipeline_state;
    vk::DescriptorSetLayout     graphics_bind_group_layout;
    GpuGraphicsPipelineState    graphics_pipeline_state;

    GpuTexture                  color_texture;

    bool use_memcpy = false;

    App() {
//        glfwInitVulkanLoader(vulkan->loader.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr"));
        platform = new WindowPlatform("Vulkan window", 800, 600);
        vulkan = new VulkanRenderer(platform);

        ImGui::CreateContext();
        ImGui_ImplGlfw_InitForVulkan(static_cast<GLFWwindow*>(platform->GetNativeWindow()), true);

        imgui = new ImGuiRenderer(vulkan);

        CreateRenderTargets();
        CreateComputePipelineState();
        CreateGraphicsPipelineState();
    }

    ~App() {
        vulkan->CleanupTexture(&color_texture);

        gpu_destroy_compute_pipeline_state(&vulkan->context, &compute_pipeline_state);
        gpu_destroy_graphics_pipeline_state(&vulkan->context, &graphics_pipeline_state);
        vulkan->context.logical_device.destroyDescriptorSetLayout(compute_bind_group_layout);
        vulkan->context.logical_device.destroyDescriptorSetLayout(graphics_bind_group_layout);

        imgui->release();
        vulkan->release();
        platform->release();
    }

    void Start() {
        while (PumpEvents()) {
            Update();

            vulkan->WaitAndBeginNewFrame();

            EncodeRasterizer(vulkan->current_command_buffer);
            EncodeSwapchain(vulkan->current_command_buffer);

            vulkan->SubmitFrameAndPresent();
        }

        vulkan->context.logical_device.waitIdle();
    }

    void Update() {
//        ImGui_ImplSDL2_NewFrame();
        ImGui_ImplGlfw_NewFrame();

        ImGui::NewFrame();

        ImGui::Begin("Stats");
        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
        ImGui::Checkbox("Use memcpy", &use_memcpy);
        ImGui::End();
        ImGui::ShowDemoWindow(nullptr);
        ImGui::Render();
    }

    auto PumpEvents() -> bool {
        return platform->PumpEvents();

//        SDL_Event event;
//        while (SDL_PollEvent(&event)) {
//            ImGui_ImplSDL2_ProcessEvent(&event);
//
//            if (event.type == SDL_QUIT) {
//                return false;
//            }
//            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
//                vulkan->RebuildSwapchain();
//                continue;
//            }
//        }
//        return true;
    }

    void EncodeRasterizer(GpuCommandBuffer* command_buffer) {
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
                    .setImage(color_texture.image)
                    .setSubresourceRange(vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)),
            };

            command_buffer->cmd_buffer.pipelineBarrier2(vk::DependencyInfo({}, {}, {}, barriers));
        }

        // clear image
        {
            auto clear_value = vk::ClearColorValue(std::array{ 0.0f, 0.0f, 0.0f, 1.0f });
            auto subresource = vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);

            command_buffer->cmd_buffer.clearColorImage(color_texture.image, vk::ImageLayout::eGeneral, clear_value, subresource);
        }

        auto bind_group = gpu_command_buffer_allocate_bind_group(&vulkan->context, command_buffer, compute_bind_group_layout);
        {
            auto color_image_info = vk::DescriptorImageInfo()
                .setImageView(color_texture.view)
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

        command_buffer->cmd_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, compute_pipeline_state.pipeline);
        command_buffer->cmd_buffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, compute_pipeline_state.pipeline_layout, 0, 1, &bind_group, 0, nullptr);

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
                if (!gpu_command_buffer_allocate(&vulkan->context, command_buffer, &vtx_buffer_info, vtx_buffer_size, alignof(ImDrawVert))) {
                    fprintf(stderr, "Failed to allocate vertex buffer for ImGui\n");
                    continue;
                }

                if (!gpu_command_buffer_allocate(&vulkan->context, command_buffer, &idx_buffer_info, idx_buffer_size, alignof(ImDrawIdx))) {
                    fprintf(stderr, "Failed to allocate index buffer for ImGui\n");
                    continue;
                }

                if (use_memcpy) {
                    std::memcpy(gpu_buffer_contents(&vtx_buffer_info), cmd_list->VtxBuffer.Data, vtx_buffer_size);
                    std::memcpy(gpu_buffer_contents(&idx_buffer_info), cmd_list->IdxBuffer.Data, idx_buffer_size);
                } else {
                    gpu_update_buffer(command_buffer->cmd_buffer, &vtx_buffer_info, cmd_list->VtxBuffer.Data, vtx_buffer_size);
                    gpu_update_buffer(command_buffer->cmd_buffer, &idx_buffer_info, cmd_list->IdxBuffer.Data, idx_buffer_size);
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
                            .index_buffer_reference = gpu_buffer_device_address(&idx_buffer_info),
                            .vertex_buffer_reference = gpu_buffer_device_address(&vtx_buffer_info),
                            .viewport_scale = viewport_scale,
                            .index_offset = draw_cmd.IdxOffset + i,
                            .clip_rect_min_x = clip_rect.Min.x,
                            .clip_rect_min_y = clip_rect.Min.y,
                            .clip_rect_max_x = clip_rect.Max.x,
                            .clip_rect_max_y = clip_rect.Max.y,
                        };

                        command_buffer->cmd_buffer.pushConstants(compute_pipeline_state.pipeline_layout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(push_constants), &push_constants);

                        command_buffer->cmd_buffer.dispatch(group_count_x, group_count_y, 1);
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
                    .setImage(color_texture.image)
                    .setSubresourceRange(vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)),
            };

            command_buffer->cmd_buffer.pipelineBarrier2(vk::DependencyInfo({}, {}, {}, barriers));
        }
    }

    void EncodeSwapchain(GpuCommandBuffer* command_buffer) {
        auto render_area = vk::Rect2D(vk::Offset2D(0, 0), vulkan->configuration.extent);
        auto render_viewport = vk::Viewport()
            .setX(0)
            .setY(0)
            .setWidth(static_cast<f32>(vulkan->configuration.extent.width))
            .setHeight(static_cast<f32>(vulkan->configuration.extent.height))
            .setMinDepth(0)
            .setMaxDepth(1);

        vk::ImageMemoryBarrier2 barriers_1[1] = {};
        barriers_1[0].setSrcStageMask(vk::PipelineStageFlagBits2::eTopOfPipe);
        barriers_1[0].setDstStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput);
        barriers_1[0].setSrcAccessMask(vk::AccessFlagBits2::eNone);
        barriers_1[0].setDstAccessMask(vk::AccessFlagBits2::eColorAttachmentRead | vk::AccessFlagBits2::eColorAttachmentWrite);
        barriers_1[0].setOldLayout(vk::ImageLayout::eUndefined);
        barriers_1[0].setNewLayout(vk::ImageLayout::eColorAttachmentOptimal);
        barriers_1[0].setImage(vulkan->swapchain_images[vulkan->current_image_index]);
        barriers_1[0].setSubresourceRange(vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1));

        command_buffer->cmd_buffer.pipelineBarrier2(vk::DependencyInfo({}, {}, {}, barriers_1));

        vk::RenderingAttachmentInfo color_attachment_info = {};
        color_attachment_info.setImageView(vulkan->swapchain_views[vulkan->current_image_index]);
        color_attachment_info.setImageLayout(vk::ImageLayout::eColorAttachmentOptimal);
        color_attachment_info.setLoadOp(vk::AttachmentLoadOp::eClear);
        color_attachment_info.setStoreOp(vk::AttachmentStoreOp::eStore);
        color_attachment_info.setClearValue(vk::ClearColorValue(std::array{0.0f, 0.0f, 0.0f, 1.0f}));

        vk::RenderingInfo render_info = {};
        render_info.renderArea = render_area;
        render_info.layerCount = 1;
        render_info.colorAttachmentCount = 1;
        render_info.pColorAttachments = &color_attachment_info;

        command_buffer->cmd_buffer.beginRendering(render_info);
        command_buffer->cmd_buffer.setViewport(0, render_viewport);
        command_buffer->cmd_buffer.setScissor(0, render_area);

        auto bind_group = gpu_command_buffer_allocate_bind_group(&vulkan->context, command_buffer, graphics_bind_group_layout);
        {
            auto image_info = vk::DescriptorImageInfo()
                .setSampler(color_texture.sampler)
                .setImageView(color_texture.view)
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
        command_buffer->cmd_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, graphics_pipeline_state.pipeline);
        command_buffer->cmd_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, graphics_pipeline_state.pipeline_layout, 0, 1, &bind_group, 0, nullptr);
        command_buffer->cmd_buffer.draw(6, 1, 0, 0);

//        imgui->RecordCommandBuffer(command_buffer, ImGui::GetDrawData());
        command_buffer->cmd_buffer.endRendering();

        vk::ImageMemoryBarrier2 barriers_2[1] = {};
        barriers_2[0].setSrcStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput);
        barriers_2[0].setDstStageMask(vk::PipelineStageFlagBits2::eBottomOfPipe);
        barriers_2[0].setSrcAccessMask(vk::AccessFlagBits2::eColorAttachmentRead | vk::AccessFlagBits2::eColorAttachmentWrite);
        barriers_2[0].setDstAccessMask(vk::AccessFlagBits2::eMemoryRead);
        barriers_2[0].setOldLayout(vk::ImageLayout::eColorAttachmentOptimal);
        barriers_2[0].setNewLayout(vk::ImageLayout::ePresentSrcKHR);
        barriers_2[0].setImage(vulkan->swapchain_images[vulkan->current_image_index]);
        barriers_2[0].setSubresourceRange(vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1));

        command_buffer->cmd_buffer.pipelineBarrier2(vk::DependencyInfo({}, {}, {}, barriers_2));
    }

    void CreateRenderTargets() {
        auto color_image_info = vk::ImageCreateInfo()
            .setImageType(vk::ImageType::e2D)
            .setFormat(vk::Format::eR32G32B32A32Sfloat)
            .setExtent(vk::Extent3D(vulkan->configuration.extent.width, vulkan->configuration.extent.height, 1))
            .setMipLevels(1)
            .setArrayLayers(1)
            .setSamples(vk::SampleCountFlagBits::e1)
            .setTiling(vk::ImageTiling::eOptimal)
            .setUsage(vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferDst)
            .setSharingMode(vk::SharingMode::eExclusive)
            .setQueueFamilyIndices({})
            .setInitialLayout(vk::ImageLayout::eUndefined);

        vk::resultCheck(vulkan->context.logical_device.createImage(&color_image_info, nullptr, &color_texture.image), "Failed to create image");
        gpu_texture_storage(&vulkan->context, &color_texture.allocation, color_texture.image, GpuStorageMode::ePrivate, {});

        auto color_view_info = vk::ImageViewCreateInfo()
            .setImage(color_texture.image)
            .setViewType(vk::ImageViewType::e2D)
            .setFormat(vk::Format::eR32G32B32A32Sfloat)
            .setSubresourceRange(vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1));

        vk::resultCheck(vulkan->context.logical_device.createImageView(&color_view_info, nullptr, &color_texture.view), "Failed to create image view");
        
        auto color_sampler_info = vk::SamplerCreateInfo()
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
            .setMaxLod(0.0f);
        
        vk::resultCheck(vulkan->context.logical_device.createSampler(&color_sampler_info, nullptr, &color_texture.sampler), "Failed to create sampler");
    }

    void CreateComputePipelineState() {
        auto entries = std::array{
            vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eStorageImage, 1, vk::ShaderStageFlagBits::eCompute),
            vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eCompute),
        };
        compute_bind_group_layout = vulkan->context.logical_device.createDescriptorSetLayout(vk::DescriptorSetLayoutCreateInfo({}, entries));

        auto bind_group_layouts = std::array{
            compute_bind_group_layout
        };

        auto push_constant_ranges = std::array{
            vk::PushConstantRange(vk::ShaderStageFlagBits::eCompute, 0, sizeof(RasterizerPushConstants))
        };

        auto comp_bytes = vulkan->ReadBytes("shaders/rasterizer.comp.spv").value();

        GpuShaderObjectCreateInfo shader_object_infos[1] = {};
        shader_object_infos[0].stage = vk::ShaderStageFlagBits::eCompute;
        shader_object_infos[0].codeSize = comp_bytes.size();
        shader_object_infos[0].pCode = comp_bytes.data();
        shader_object_infos[0].pName = "main";

        GpuShaderObject compute_shader_object;
        gpu_create_shader_object(&vulkan->context, &compute_shader_object, &shader_object_infos[0]);

        auto state_create_info = GpuComputePipelineStateCreateInfo{
            .shader_object = &compute_shader_object,
            .bind_group_layouts = bind_group_layouts,
            .push_constant_ranges = push_constant_ranges
        };

        gpu_create_compute_pipeline_state(&vulkan->context, &state_create_info, &compute_pipeline_state);

        gpu_destroy_shader_object(&vulkan->context, &compute_shader_object);
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

        auto vert_bytes = vulkan->ReadBytes("shaders/full_screen_quad.vert.spv").value();
        auto frag_bytes = vulkan->ReadBytes("shaders/full_screen_quad.frag.spv").value();

        GpuShaderObject vert_shader_object;
        GpuShaderObject frag_shader_object;

        GpuShaderObjectCreateInfo shader_object_infos[2] = {};
        shader_object_infos[0].stage = vk::ShaderStageFlagBits::eVertex;
        shader_object_infos[0].codeSize = vert_bytes.size();
        shader_object_infos[0].pCode = vert_bytes.data();
        shader_object_infos[0].pName = "main";

        shader_object_infos[1].stage = vk::ShaderStageFlagBits::eFragment;
        shader_object_infos[1].codeSize = frag_bytes.size();
        shader_object_infos[1].pCode = frag_bytes.data();
        shader_object_infos[1].pName = "main";

        gpu_create_shader_object(&vulkan->context, &vert_shader_object, &shader_object_infos[0]);
        gpu_create_shader_object(&vulkan->context, &frag_shader_object, &shader_object_infos[1]);

        auto bind_group_layouts = std::array{
            graphics_bind_group_layout
        };

        auto shader_objects = std::array{
            &vert_shader_object,
            &frag_shader_object
        };

        auto state_create_info = GpuGraphicsPipelineStateCreateInfo {
            .shader_objects         = shader_objects,
            .input_assembly_state   = {},
            .rasterization_state    = {},
            .depth_stencil_state    = {
                .depth_test_enable  = false,
                .depth_write_enable = false,
            },
            .color_blend_state      = {
                .attachments = attachments
            },
            .bind_group_layouts     = bind_group_layouts,
            .push_constant_ranges   = {}
        };

        vk::PipelineRenderingCreateInfo rendering_create_info = {};
        rendering_create_info.colorAttachmentCount = 1;
        rendering_create_info.pColorAttachmentFormats = &vulkan->configuration.format;
        gpu_create_graphics_pipeline_state(&vulkan->context, &graphics_pipeline_state, &state_create_info, &rendering_create_info);

        gpu_destroy_shader_object(&vulkan->context, &vert_shader_object);
        gpu_destroy_shader_object(&vulkan->context, &frag_shader_object);
    }
};

auto main() -> i32 {
    // setenv("MVK_DEBUG", "1", 1);

    try {
        auto app = App();
        app.Start();
        return 0;
    } catch(const std::exception& e) {
        fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
