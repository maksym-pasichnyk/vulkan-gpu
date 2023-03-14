//
// Created by Maksym Pasichnyk on 06.03.2023.
//

#pragma once

#define VK_NO_PROTOTYPES
#define VK_ENABLE_BETA_EXTENSIONS

#include "numeric.hpp"

#include <vulkan/vulkan.hpp>
#include <SDL_vulkan.h>

template<typename T>
using Slice = vk::ArrayProxyNoTemporaries<T>;

struct GpuInputAssemblyState {
    vk::PrimitiveTopology   topology                    = vk::PrimitiveTopology::eTriangleList;
    bool                    primitive_restart_enable    = false;
};

struct GpuRasterizationState {
    bool                    depth_clamp_enable          = false;
    bool                    discard_enable              = false;
    vk::PolygonMode         polygon_mode                = vk::PolygonMode::eFill;
    f32                     line_width                  = 1.0F;
    vk::CullModeFlagBits    cull_mode                   = vk::CullModeFlagBits::eNone;
    vk::FrontFace           front_face                  = vk::FrontFace::eClockwise;
    bool                    depth_bias_enable           = false;
    f32                     depth_bias_constant_factor  = 0.0F;
    f32                     depth_bias_clamp            = 0.0F;
    f32                     depth_bias_slope_factor     = 0.0F;
};

struct GpuDepthStencilState {
    bool                depth_test_enable           = false;
    bool                depth_write_enable          = false;
    vk::CompareOp       depth_compare_op            = vk::CompareOp::eAlways;
    bool                depth_bounds_test_enable    = false;
    bool                stencil_test_enable         = false;
    vk::StencilOpState  front                       = {};
    vk::StencilOpState  back                        = {};
    f32                 min_depth_bounds            = 0.0F;
    f32                 max_depth_bounds            = 1.0F;
};

struct GpuColorBlendState {
    bool                                            logic_op_enable = false;
    vk::LogicOp                                     logic_op        = vk::LogicOp::eCopy;
    std::array<f32, 4>                              blend_constants = { 0.0F, 0.0F, 0.0F, 0.0F };
    Slice<vk::PipelineColorBlendAttachmentState>    attachments     = {};
};

struct GpuVertexInputState {
    Slice<vk::VertexInputBindingDescription>    bindings    = {};
    Slice<vk::VertexInputAttributeDescription>  attributes  = {};
};

struct GpuAllocation {
    void*                   mapped                  = {};
    vk::DeviceMemory        device_memory           = {};
    vk::MemoryRequirements  memory_requirements     = {};
    vk::MemoryPropertyFlags memory_property_flags   = {};
};

struct GpuBufferInfo {
    vk::Buffer          buffer          = {};
    void*               mapped          = {};
    vk::DeviceSize      size            = {};
    vk::DeviceSize      offset          = {};
    GpuAllocation       allocation      = {};
    vk::DeviceAddress   device_address  = {};
};

struct GpuTexture {
    vk::Image     image         = {};
    vk::ImageView view          = {};
    vk::Sampler   sampler       = {};
    GpuAllocation allocation    = {};
};

struct GpuGraphicsPipelineStateCreateInfo {
    vk::ShaderModule                vertex_shader_module        = {};
    std::string                     vertex_shader_entry_point   = {};
    vk::ShaderModule                fragment_shader_module      = {};
    std::string                     fragment_shader_entry_point = {};
    GpuInputAssemblyState           input_assembly_state        = {};
    GpuRasterizationState           rasterization_state         = {};
    GpuDepthStencilState            depth_stencil_state         = {};
    GpuColorBlendState              color_blend_state           = {};
    GpuVertexInputState             vertex_input_state          = {};
    Slice<vk::DescriptorSetLayout>  bind_group_layouts          = {};
    Slice<vk::PushConstantRange>    push_constant_ranges        = {};
    vk::RenderPass                  render_pass                 = {};
};

struct GpuGraphicsPipelineState {
    vk::Pipeline        pipeline;
    vk::PipelineLayout  pipeline_layout;
};

struct GpuComputePipelineStateCreateInfo {
    vk::ShaderModule                compute_shader_module       = {};
    std::string                     compute_shader_entry_point  = {};
    Slice<vk::DescriptorSetLayout>  bind_group_layouts          = {};
    Slice<vk::PushConstantRange>    push_constant_ranges        = {};
};

struct GpuComputePipelineState {
    vk::Pipeline        pipeline;
    vk::PipelineLayout  pipeline_layout;
};

struct GpuLinearAllocator {
    vk::Buffer          buffer;
    vk::DeviceSize      capacity;
    vk::DeviceSize      offset;
    GpuAllocation       allocation;
    vk::DeviceAddress   device_address;
};

struct GpuContext {
    vk::Instance                    instance;
    vk::SurfaceKHR                  surface;
    vk::DebugUtilsMessengerEXT      messenger;
    vk::Device                      logical_device;
    vk::PhysicalDevice              physical_device;
    vk::Queue                       graphics_queue;
    uint32_t                        graphics_queue_family_index;
    vk::Queue                       present_queue;
    uint32_t                        present_queue_family_index;
    vk::Queue                       compute_queue;
    uint32_t                        compute_queue_family_index;
};

auto debug_utils_messenger_callback(vk::DebugUtilsMessageSeverityFlagBitsEXT messageSeverity, unsigned int messageType, const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) -> vk::Bool32 {
    if (messageSeverity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose) {
        fprintf(stdout, "%s\n", pCallbackData->pMessage);
        return VK_FALSE;
    }
    if (messageSeverity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo) {
        fprintf(stdout, "%s\n", pCallbackData->pMessage);
        return VK_FALSE;
    }
    if (messageSeverity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning) {
        fprintf(stdout, "%s\n", pCallbackData->pMessage);
        return VK_FALSE;
    }
    if (messageSeverity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eError) {
        fprintf(stderr, "%s\n", pCallbackData->pMessage);
        return VK_TRUE;
    }
    return VK_FALSE;
}

auto gpu_find_memory_type_index(GpuContext* context, u32 memory_type_bits, vk::MemoryPropertyFlags memory_property_flags) -> u32 {
    auto memory_properties = context->physical_device.getMemoryProperties();
    for (u32 i = 0; i < memory_properties.memoryTypeCount; ++i) {
        if ((memory_type_bits & (1 << i)) && (memory_properties.memoryTypes[i].propertyFlags & memory_property_flags) == memory_property_flags) {
            return i;
        }
    }
    return std::numeric_limits<u32>::max();
}

void gpu_allocate_memory(GpuContext* context, GpuAllocation* allocation, vk::MemoryRequirements memory_requirements, vk::MemoryPropertyFlags memory_property_flags, vk::MemoryAllocateFlags memory_allocate_flags) {
    auto memory_type_index = gpu_find_memory_type_index(context, memory_requirements.memoryTypeBits, memory_property_flags);

    auto memory_allocate_flags_info = vk::MemoryAllocateFlagsInfo()
        .setFlags(memory_allocate_flags);

    auto memory_allocate_info = vk::MemoryAllocateInfo()
        .setPNext(&memory_allocate_flags_info)
        .setAllocationSize(memory_requirements.size)
        .setMemoryTypeIndex(memory_type_index);

    allocation->device_memory = context->logical_device.allocateMemory(memory_allocate_info);
    allocation->memory_requirements = memory_requirements;
    allocation->memory_property_flags = memory_property_flags;

    if (memory_property_flags & vk::MemoryPropertyFlagBits::eHostVisible) {
        allocation->mapped = context->logical_device.mapMemory(allocation->device_memory, 0, VK_WHOLE_SIZE);
    } else {
        allocation->mapped = nullptr;
    }
}

void gpu_free_memory(GpuContext* context, GpuAllocation* allocation) {
    if (allocation->mapped) {
        context->logical_device.unmapMemory(allocation->device_memory);
    }
    context->logical_device.freeMemory(allocation->device_memory);
}

void gpu_create_allocator(GpuContext* context, GpuLinearAllocator* allocator, vk::DeviceSize capacity) {
    vk::BufferUsageFlags buffer_usage_flags = {};
    buffer_usage_flags |= vk::BufferUsageFlagBits::eTransferSrc;
    buffer_usage_flags |= vk::BufferUsageFlagBits::eTransferDst;
    buffer_usage_flags |= vk::BufferUsageFlagBits::eUniformTexelBuffer;
    buffer_usage_flags |= vk::BufferUsageFlagBits::eStorageTexelBuffer;
    buffer_usage_flags |= vk::BufferUsageFlagBits::eUniformBuffer;
    buffer_usage_flags |= vk::BufferUsageFlagBits::eStorageBuffer;
    buffer_usage_flags |= vk::BufferUsageFlagBits::eIndexBuffer;
    buffer_usage_flags |= vk::BufferUsageFlagBits::eVertexBuffer;
    buffer_usage_flags |= vk::BufferUsageFlagBits::eIndirectBuffer;
    buffer_usage_flags |= vk::BufferUsageFlagBits::eShaderDeviceAddress;

    auto buffer_create_info = vk::BufferCreateInfo()
        .setSize(capacity)
        .setUsage(buffer_usage_flags)
        .setSharingMode(vk::SharingMode::eExclusive);

    allocator->buffer = context->logical_device.createBuffer(buffer_create_info);
    allocator->capacity = capacity;
    allocator->offset = 0u;

    vk::MemoryAllocateFlags memory_allocate_flags = {};
    memory_allocate_flags |= vk::MemoryAllocateFlagBits::eDeviceAddress;

    auto memory_requirements = context->logical_device.getBufferMemoryRequirements(allocator->buffer);

    vk::MemoryPropertyFlags memory_property_flags = {};
    memory_property_flags |= vk::MemoryPropertyFlagBits::eHostVisible;
    memory_property_flags |= vk::MemoryPropertyFlagBits::eHostCoherent;

    gpu_allocate_memory(context, &allocator->allocation, memory_requirements, memory_property_flags, memory_allocate_flags);
    context->logical_device.bindBufferMemory(allocator->buffer, allocator->allocation.device_memory, 0);

    auto buffer_device_address_info = vk::BufferDeviceAddressInfo().setBuffer(allocator->buffer);
    allocator->device_address = context->logical_device.getBufferAddress(buffer_device_address_info);
}

void gpu_destroy_allocator(GpuContext* context, GpuLinearAllocator* allocator) {
    context->logical_device.destroyBuffer(allocator->buffer);
    gpu_free_memory(context, &allocator->allocation);
}

void gpu_create_context(GpuContext* context, void* window, PFN_vkGetInstanceProcAddr vk_get_instance_proc_addr) {
    vk::defaultDispatchLoaderDynamic.init(vk_get_instance_proc_addr);

    std::vector<const char*> instance_extensions;
    instance_extensions.push_back("VK_KHR_surface");
#if __APPLE__
    instance_extensions.push_back("VK_MVK_macos_surface");
#elif _WIN32
    instance_extensions.push_back("VK_KHR_win32_surface");
#endif
    instance_extensions.push_back("VK_EXT_debug_utils");
    instance_extensions.push_back("VK_KHR_device_group_creation");
#if __APPLE__
    instance_extensions.push_back("VK_KHR_portability_enumeration");
#endif
    instance_extensions.push_back("VK_KHR_get_physical_device_properties2");

    std::vector<const char*> instance_layers;
#if __APPLE__
     instance_layers.push_back("VK_LAYER_KHRONOS_validation");
     instance_layers.push_back("VK_LAYER_KHRONOS_synchronization2");
#endif
     
    auto app_info = vk::ApplicationInfo()
        .setPApplicationName("Dragon")
        .setApplicationVersion(VK_MAKE_VERSION(1, 0, 0))
        .setPEngineName("Dragon")
        .setEngineVersion(VK_MAKE_VERSION(1, 0, 0))
        .setApiVersion(VK_API_VERSION_1_3);

    auto enables = std::array{
        vk::ValidationFeatureEnableEXT::eSynchronizationValidation,
    };

    auto validation_features = vk::ValidationFeaturesEXT()
        .setEnabledValidationFeatureCount(0)
        .setPEnabledValidationFeatures(nullptr)
        .setDisabledValidationFeatureCount(0)
        .setPDisabledValidationFeatures(nullptr);

    auto instance_create_info = vk::InstanceCreateInfo()
        .setFlags(vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR)
        .setPApplicationInfo(&app_info)
        .setPEnabledExtensionNames(instance_extensions)
        .setPEnabledLayerNames(instance_layers);

    context->instance = vk::createInstance(instance_create_info);
    vk::defaultDispatchLoaderDynamic.init(context->instance);

    auto messenger_create_info = vk::DebugUtilsMessengerCreateInfoEXT()
        .setMessageSeverity(vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose | vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError)
        .setMessageType(vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance)
        .setPfnUserCallback([](VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) -> VkBool32 {
            return debug_utils_messenger_callback(static_cast<vk::DebugUtilsMessageSeverityFlagBitsEXT>(messageSeverity), messageType, reinterpret_cast<const vk::DebugUtilsMessengerCallbackDataEXT*>(pCallbackData), pUserData);
        });

    context->messenger = context->instance.createDebugUtilsMessengerEXT(messenger_create_info);

    SDL_Vulkan_CreateSurface(static_cast<SDL_Window*>(window), context->instance, reinterpret_cast<VkSurfaceKHR*>(&context->surface));

    context->physical_device = context->instance.enumeratePhysicalDevices().front();

    auto queue_families = context->physical_device.getQueueFamilyProperties();

    std::vector<vk::DeviceQueueCreateInfo> queue_create_infos;
    queue_create_infos.reserve(queue_families.size());

    f32 priorities[1] { 1.0F };
    for (uint32_t i = 0; i < queue_families.size(); i++) {
        queue_create_infos.emplace_back(vk::DeviceQueueCreateInfo({}, i, priorities));
    }

    vk::PhysicalDeviceBufferDeviceAddressFeatures buffer_device_address_features{};
    buffer_device_address_features.bufferDeviceAddress = VK_TRUE;

    vk::PhysicalDevicePortabilitySubsetFeaturesKHR portability_subset_features{};
    portability_subset_features.pNext = &buffer_device_address_features;

    vk::PhysicalDeviceDynamicRenderingFeatures dynamic_rendering_features{};
    dynamic_rendering_features.pNext = &portability_subset_features;
    dynamic_rendering_features.dynamicRendering = VK_TRUE;

    vk::PhysicalDeviceSynchronization2Features synchronization_2_features{};
    synchronization_2_features.pNext = &dynamic_rendering_features;
    synchronization_2_features.synchronization2 = VK_TRUE;

    auto features2 = context->physical_device.getFeatures2();
    features2.pNext = &synchronization_2_features;

    auto device_extensions = std::vector<const char*>();
    device_extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    device_extensions.push_back(VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME);
    device_extensions.push_back(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME);
    device_extensions.push_back(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);
#if __APPLE__
    device_extensions.push_back(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
#endif

    auto device_create_info = vk::DeviceCreateInfo()
        .setPNext(&features2)
        .setQueueCreateInfos(queue_create_infos)
        .setPEnabledExtensionNames(device_extensions);

    context->logical_device = context->physical_device.createDevice(device_create_info);

    vk::defaultDispatchLoaderDynamic.init(context->logical_device);

    context->graphics_queue_family_index = std::numeric_limits<uint32_t>::max();
    for (uint32_t i = 0; i < queue_families.size(); i++) {
        if (queue_families[i].queueFlags & vk::QueueFlagBits::eGraphics) {
            context->graphics_queue_family_index = i;
            break;
        }
    }
    context->graphics_queue = context->logical_device.getQueue(context->graphics_queue_family_index, 0);

    context->present_queue_family_index = std::numeric_limits<uint32_t>::max();
    for (uint32_t i = 0; i < queue_families.size(); i++) {
        if (context->physical_device.getSurfaceSupportKHR(i, context->surface)) {
            context->present_queue_family_index = i;
            break;
        }
    }
    context->present_queue = context->logical_device.getQueue(context->present_queue_family_index, 0);

    context->compute_queue_family_index = std::numeric_limits<uint32_t>::max();
    for (uint32_t i = 0; i < queue_families.size(); i++) {
        if (queue_families[i].queueFlags & vk::QueueFlagBits::eCompute) {
            context->compute_queue_family_index = i;
            break;
        }
    }
    context->compute_queue = context->logical_device.getQueue(context->compute_queue_family_index, 0);
}

void gpu_destroy_context(GpuContext* context) {
    context->logical_device.destroy();

    context->instance.destroyDebugUtilsMessengerEXT(context->messenger);
    context->instance.destroySurfaceKHR(context->surface);
    context->instance.destroy();
}

auto gpu_calculate_alignment(vk::DeviceSize offset, vk::DeviceSize alignment) -> vk::DeviceSize {
    return (offset + alignment - 1) & ~(alignment - 1);
}

auto gpu_allocator_allocate(GpuLinearAllocator* allocator, GpuBufferInfo* info, vk::DeviceSize size, vk::DeviceSize alignment) -> bool {
    vk::DeviceSize aligned_offset = gpu_calculate_alignment(allocator->offset, alignment);
    if (aligned_offset + size > allocator->capacity) {
        return false;
    }
    info->buffer            = allocator->buffer;
    info->mapped            = reinterpret_cast<u8*>(allocator->allocation.mapped) + aligned_offset;
    info->size              = size;
    info->offset            = aligned_offset;
    info->allocation        = allocator->allocation;
    info->device_address    = allocator->device_address + aligned_offset;

    allocator->offset       = aligned_offset + size;
    return true;
}

void gpu_allocator_reset(GpuLinearAllocator* allocator) {
    allocator->offset = 0;
}

void gpu_create_graphics_pipeline_state(GpuContext* context, GpuGraphicsPipelineState* state, GpuGraphicsPipelineStateCreateInfo* info, void* pNext) {
    auto shader_stages = std::array{
        vk::PipelineShaderStageCreateInfo()
            .setStage(vk::ShaderStageFlagBits::eVertex)
            .setModule(info->vertex_shader_module)
            .setPName(info->vertex_shader_entry_point.c_str()),
        vk::PipelineShaderStageCreateInfo()
            .setStage(vk::ShaderStageFlagBits::eFragment)
            .setModule(info->fragment_shader_module)
            .setPName(info->fragment_shader_entry_point.c_str())
    };

    auto vertex_input_state_create_info = vk::PipelineVertexInputStateCreateInfo()
        .setVertexBindingDescriptions(info->vertex_input_state.bindings)
        .setVertexAttributeDescriptions(info->vertex_input_state.attributes);

    auto input_assembly_state_create_info = vk::PipelineInputAssemblyStateCreateInfo()
        .setTopology(info->input_assembly_state.topology)
        .setPrimitiveRestartEnable(info->input_assembly_state.primitive_restart_enable);

    auto dynamic_states = std::array{
        vk::DynamicState::eViewport,
        vk::DynamicState::eScissor
    };

    auto dynamic_state_create_info = vk::PipelineDynamicStateCreateInfo()
        .setDynamicStates(dynamic_states);

    auto viewport_state_create_info = vk::PipelineViewportStateCreateInfo()
        .setViewportCount(1)
        .setPViewports(nullptr)
        .setScissorCount(1)
        .setPScissors(nullptr);

    auto rasterization_state_create_info = vk::PipelineRasterizationStateCreateInfo()
        .setDepthClampEnable(info->rasterization_state.depth_clamp_enable)
        .setRasterizerDiscardEnable(info->rasterization_state.discard_enable)
        .setPolygonMode(info->rasterization_state.polygon_mode)
        .setLineWidth(info->rasterization_state.line_width)
        .setCullMode(info->rasterization_state.cull_mode)
        .setFrontFace(info->rasterization_state.front_face)
        .setDepthBiasEnable(info->rasterization_state.depth_bias_enable)
        .setDepthBiasConstantFactor(info->rasterization_state.depth_bias_constant_factor)
        .setDepthBiasClamp(info->rasterization_state.depth_bias_clamp)
        .setDepthBiasSlopeFactor(info->rasterization_state.depth_bias_slope_factor);

    auto multisample_state_create_info = vk::PipelineMultisampleStateCreateInfo()
        .setSampleShadingEnable(VK_FALSE)
        .setRasterizationSamples(vk::SampleCountFlagBits::e1)
        .setMinSampleShading(1)
        .setPSampleMask(nullptr)
        .setAlphaToCoverageEnable(VK_FALSE)
        .setAlphaToOneEnable(VK_FALSE);

    auto depth_stencil_state_create_info = vk::PipelineDepthStencilStateCreateInfo()
        .setDepthTestEnable(info->depth_stencil_state.depth_test_enable)
        .setDepthWriteEnable(info->depth_stencil_state.depth_write_enable)
        .setDepthCompareOp(info->depth_stencil_state.depth_compare_op)
        .setDepthBoundsTestEnable(info->depth_stencil_state.depth_bounds_test_enable)
        .setMinDepthBounds(info->depth_stencil_state.min_depth_bounds)
        .setMaxDepthBounds(info->depth_stencil_state.max_depth_bounds)
        .setStencilTestEnable(info->depth_stencil_state.stencil_test_enable)
        .setFront(info->depth_stencil_state.front)
        .setBack(info->depth_stencil_state.back);

    auto color_blend_state_create_info = vk::PipelineColorBlendStateCreateInfo()
        .setLogicOpEnable(info->color_blend_state.logic_op_enable)
        .setLogicOp(info->color_blend_state.logic_op)
        .setAttachments(info->color_blend_state.attachments)
        .setBlendConstants(info->color_blend_state.blend_constants);

    auto layout_create_info = vk::PipelineLayoutCreateInfo()
        .setSetLayouts(info->bind_group_layouts)
        .setPushConstantRanges(info->push_constant_ranges);

    state->pipeline_layout = context->logical_device.createPipelineLayout(layout_create_info);

    auto graphics_pipeline_create_info = vk::GraphicsPipelineCreateInfo()
        .setPNext(pNext)
        .setStages(shader_stages)
        .setPVertexInputState(&vertex_input_state_create_info)
        .setPInputAssemblyState(&input_assembly_state_create_info)
        .setPViewportState(&viewport_state_create_info)
        .setPRasterizationState(&rasterization_state_create_info)
        .setPMultisampleState(&multisample_state_create_info)
        .setPDepthStencilState(&depth_stencil_state_create_info)
        .setPColorBlendState(&color_blend_state_create_info)
        .setPDynamicState(&dynamic_state_create_info)
        .setLayout(state->pipeline_layout)
        .setRenderPass(info->render_pass)
        .setSubpass(0)
        .setBasePipelineHandle(nullptr)
        .setBasePipelineIndex(-1);

    context->logical_device.createGraphicsPipelines(nullptr, 1, &graphics_pipeline_create_info, nullptr, &state->pipeline);
}

void gpu_destroy_graphics_pipeline_state(GpuContext* context, GpuGraphicsPipelineState* state) {
    context->logical_device.destroyPipeline(state->pipeline);
    context->logical_device.destroyPipelineLayout(state->pipeline_layout);
}

void gpu_create_compute_pipeline_state(GpuContext* context, GpuComputePipelineStateCreateInfo* info, GpuComputePipelineState* state) {
    auto shader_stage_create_info = vk::PipelineShaderStageCreateInfo()
        .setStage(vk::ShaderStageFlagBits::eCompute)
        .setModule(info->compute_shader_module)
        .setPName(info->compute_shader_entry_point.c_str());

    auto layout_create_info = vk::PipelineLayoutCreateInfo()
        .setSetLayouts(info->bind_group_layouts)
        .setPushConstantRanges(info->push_constant_ranges);

    state->pipeline_layout = context->logical_device.createPipelineLayout(layout_create_info);

    auto compute_pipeline_create_info = vk::ComputePipelineCreateInfo()
        .setStage(shader_stage_create_info)
        .setLayout(state->pipeline_layout)
        .setBasePipelineHandle(nullptr)
        .setBasePipelineIndex(-1);

    context->logical_device.createComputePipelines(nullptr, 1, &compute_pipeline_create_info, nullptr, &state->pipeline);
}

void gpu_destroy_compute_pipeline_state(GpuContext* context, GpuComputePipelineState* state) {
    context->logical_device.destroyPipeline(state->pipeline);
    context->logical_device.destroyPipelineLayout(state->pipeline_layout);
}
