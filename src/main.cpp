#include "VulkanRenderer.hpp"
#include "ImGuiRenderer.hpp"
#include "ecs.hpp"
#include "math.hpp"

#include <SDL_video.h>
#include <SDL_events.h>
#include <imgui_demo.cpp>
#include <backends/imgui_impl_sdl2.cpp>

vk::DispatchLoaderDynamic vk::defaultDispatchLoaderDynamic;

struct Name : std::string {
    using std::string::string;
};

struct Character {

};

struct Camera {
    f32         OrthographicSize;
    Vec3f       Location;
    ecs::Entity Target;
};

struct SpriteQuadInfo {
    Vec2f Location;
    Vec2f Size;
    Vec4f TexCoords;
};

struct SpriteRendererUniforms {
    Mat4x4<f32> WorldToScreenMatrix;
};

struct SpriteRendererBatch {
    GpuTexture* texture;
    std::vector<SpriteQuadInfo> quads;
};

struct Sprite {
    Vec2f       size;
    Vec2f       pivot;
    Vec4f       tex_coords;
    f32         pixels_per_unit;

    GpuTexture* texture;
};

struct Location : Vec2f {
};

struct SpriteRenderer {
    VulkanRenderer* vulkan;
    vk::DescriptorSetLayout  bind_group_layout;
    GpuGraphicsPipelineState graphics_pipeline_state;

    GpuTexture* previous_texture = nullptr;
    std::vector<SpriteRendererBatch> batches;

    SpriteRenderer(VulkanRenderer* vulkan) : vulkan(vulkan) {
        CreateGraphicsPipelineState();
    }

    ~SpriteRenderer() {
        gpu_destroy_graphics_pipeline_state(&vulkan->context, &graphics_pipeline_state);
        vulkan->context.logical_device.destroyDescriptorSetLayout(bind_group_layout);
    }

    void CreateGraphicsPipelineState() {
        auto entries = std::array{
            vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment)
        };

        bind_group_layout = vulkan->context.logical_device.createDescriptorSetLayout(vk::DescriptorSetLayoutCreateInfo({}, entries));

        auto attachments = std::array {
            vk::PipelineColorBlendAttachmentState{}
                .setBlendEnable(false)
                .setColorWriteMask(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA)
        };

        auto vertex_shader_module = vulkan->LoadShaderModule("shaders/sprite.vert.spv").value();
        auto fragment_shader_module = vulkan->LoadShaderModule("shaders/sprite.frag.spv").value();

        auto bind_group_layouts = std::array{
            bind_group_layout
        };

        auto bindings = std::array{
            vk::VertexInputBindingDescription(0, sizeof(SpriteQuadInfo), vk::VertexInputRate::eInstance)
        };

        auto attributes = std::array{
            vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32Sfloat, offsetof(SpriteQuadInfo, Location)),
            vk::VertexInputAttributeDescription(1, 0, vk::Format::eR32G32Sfloat, offsetof(SpriteQuadInfo, Size)),
            vk::VertexInputAttributeDescription(2, 0, vk::Format::eR32G32B32A32Sfloat, offsetof(SpriteQuadInfo, TexCoords)),
        };

        auto push_constant_ranges = std::array{
            vk::PushConstantRange(vk::ShaderStageFlagBits::eVertex, 0, sizeof(SpriteRendererUniforms))
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
            .vertex_input_state          = {
                .bindings   = bindings,
                .attributes = attributes,
            },
            .bind_group_layouts          = bind_group_layouts,
            .push_constant_ranges        = push_constant_ranges,
            .render_pass                 = vulkan->swapchain_render_pass,
        };
        gpu_create_graphics_pipeline_state(&vulkan->context, &graphics_pipeline_state, &state_create_info, nullptr);

        vulkan->context.logical_device.destroyShaderModule(vertex_shader_module);
        vulkan->context.logical_device.destroyShaderModule(fragment_shader_module);
    }

    void ClearSpriteBatches() {
        batches.clear();
        previous_texture = nullptr;
    }

    void DrawSprite(Vec2f location, const Sprite& sprite) {
        if (sprite.texture != previous_texture) {
            previous_texture = sprite.texture;
            batches.push_back({ sprite.texture, {} });
        }

        auto size = sprite.size / sprite.pixels_per_unit;
        auto offset = location - sprite.pivot * size;

        auto quad = SpriteQuadInfo {
            .Location   = offset,
            .Size       = size,
            .TexCoords  = sprite.tex_coords,
        };

        batches.back().quads.push_back(quad);
    }

    void EncodeCommands(vk::CommandBuffer cmd, Mat4x4<f32> world_to_screen_matrix) {
        auto uniforms = SpriteRendererUniforms {
            .WorldToScreenMatrix = world_to_screen_matrix,
        };
        cmd.pushConstants(graphics_pipeline_state.pipeline_layout, vk::ShaderStageFlagBits::eVertex, 0, sizeof(SpriteRendererUniforms), &uniforms);

        for (auto& batch : batches) {
            GpuBufferInfo vertex_buffer_info;
            if (!vulkan->AllocateTemporary(&vertex_buffer_info, batch.quads.size() * sizeof(SpriteQuadInfo), alignof(SpriteQuadInfo))) {
                fprintf(stderr, "Failed to allocate temporary vertex buffer for sprite renderer\n");
                continue;
            }
            std::memcpy(vertex_buffer_info.mapped, batch.quads.data(), batch.quads.size() * sizeof(SpriteQuadInfo));

            vk::DescriptorSet bind_group;
            {
                auto allocate_info = vk::DescriptorSetAllocateInfo()
                    .setDescriptorPool(vulkan->bind_group_allocators[vulkan->frame_index])
                    .setDescriptorSetCount(1)
                    .setPSetLayouts(&bind_group_layout);

                auto result = vulkan->context.logical_device.allocateDescriptorSets(&allocate_info, &bind_group);
                if (result != vk::Result::eSuccess) {
                    fprintf(stderr, "Failed to allocate descriptor set for sprite renderer: %s\n", vk::to_string(result).c_str());
                    continue;
                }

                auto image_info = vk::DescriptorImageInfo()
                    .setSampler(batch.texture->sampler)
                    .setImageView(batch.texture->view)
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

            cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, graphics_pipeline_state.pipeline);
            cmd.bindVertexBuffers(0, 1, &vertex_buffer_info.buffer, &vertex_buffer_info.offset);
            cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, graphics_pipeline_state.pipeline_layout, 0, 1, &bind_group, 0, nullptr);
            cmd.draw(6, batch.quads.size(), 0, 0);
        }
    }
};

auto Orthographic(float left, float right, float bottom, float top, float near, float far) -> Mat4x4<f32> {
    auto x = 2.0f / (right - left);
    auto y = 2.0f / (top - bottom);
    auto z = 2.0f / (near - far);

    auto u = (left + right) / (left - right);
    auto v = (bottom + top) / (bottom - top);
    auto w = (near + far) / (near - far);

    return Mat4x4<f32> {
        x, 0, 0, 0,
        0, y, 0, 0,
        0, 0, z, 0,
        u, v, w, 1,
    };
}

struct App {
    SDL_Window*     window;
    VulkanRenderer* vulkan;
    ImGuiRenderer*  imgui;
    SpriteRenderer* sprite_renderer;

    vk::DescriptorSetLayout  graphics_bind_group_layout;
    GpuGraphicsPipelineState graphics_pipeline_state;

    ecs::World  ecs;
    Camera      Camera;
    GpuTexture  GrassSprite;
    Mat4x4<f32> WorldToScreenMatrix;

    App() {
        window = SDL_CreateWindow("Hello, world!", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 600, SDL_WINDOW_VULKAN | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE);

        ImGui::CreateContext();
        ImGui_ImplSDL2_InitForVulkan(window);

        vulkan = new VulkanRenderer(window);
        imgui = new ImGuiRenderer(vulkan);

        sprite_renderer = new SpriteRenderer(vulkan);

        CreateGraphicsPipelineState();

        u32 RED = 0xFF0000FF;
        vulkan->CreateTextureFromMemory(&GrassSprite, 1, 1, &RED);

        ecs.spawn(
            Name("Grass"),
            Location { 0, 0 },
            Sprite{
                .size               = { 32, 32 },
                .pivot              = { 0.5, 0.5 },
                .tex_coords         = { 0, 0, 1, 1 },
                .pixels_per_unit    = 100,
                .texture            = &GrassSprite,
            }
        );
        Camera.OrthographicSize     = 5;
    }

    ~App() {
        vulkan->CleanupTexture(&GrassSprite);

        gpu_destroy_graphics_pipeline_state(&vulkan->context, &graphics_pipeline_state);
        vulkan->context.logical_device.destroyDescriptorSetLayout(graphics_bind_group_layout);

        delete sprite_renderer;
        delete imgui;
        delete vulkan;

        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();
        SDL_DestroyWindow(window);
    }

    void Start() {
        while (true) {
            if (!PumpEvents()) {
                break;
            }

            Update();

            sprite_renderer->ClearSpriteBatches();
            for (auto [_, location, sprite] : ecs::Query<Location, Sprite>(ecs).iter()) {
                sprite_renderer->DrawSprite(location, sprite);
            }

            vulkan->WaitAndBeginNewFrame();

            EncodeSwapchain(vulkan->command_buffers[vulkan->frame_index]);

            vulkan->SubmitFrameAndPresent();
        }

        vulkan->context.logical_device.waitIdle();
    }

    void Update() {
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Stats");
        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
        ImGui::DragFloat("Orthographic Size", &Camera.OrthographicSize, 0.01F);
        ImGui::End();
        ImGui::ShowDemoWindow(nullptr);
        ImGui::Render();

        auto AspectRatio = (f32)vulkan->configuration.extent.width / (f32)vulkan->configuration.extent.height;

        auto Left   = Camera.Location.x - Camera.OrthographicSize * AspectRatio;
        auto Right  = Camera.Location.x + Camera.OrthographicSize * AspectRatio;
        auto Bottom = Camera.Location.y - Camera.OrthographicSize;
        auto Top    = Camera.Location.y + Camera.OrthographicSize;

        WorldToScreenMatrix = Orthographic(Left, Right, Bottom, Top, -1.0F, 1.0F);
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

        sprite_renderer->EncodeCommands(cmd, WorldToScreenMatrix);

        imgui->RenderDrawData(cmd, ImGui::GetDrawData());

        cmd.endRenderPass();
    }

    void EncodeFullScreenQuad(vk::CommandBuffer cmd, GpuTexture* texture) {
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
                .setSampler(texture->sampler)
                .setImageView(texture->view)
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
        cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, graphics_pipeline_state.pipeline);
        cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, graphics_pipeline_state.pipeline_layout, 0, 1, &bind_group, 0, nullptr);

        cmd.draw(6, 1, 0, 0);
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
