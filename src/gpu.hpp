//
// Created by Maksym Pasichnyk on 06.03.2023.
//

#pragma once

#define VK_NO_PROTOTYPES
#define VK_ENABLE_BETA_EXTENSIONS

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_hash.hpp>

#include "WindowPlatform.hpp"

template<typename T>
using Slice = vk::ArrayProxyNoTemporaries<T>;

enum class GpuStorageMode {
    ePrivate,
    eManaged,
    eShared,
    eLazy,
};

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
    vk::Buffer          buffer      = {};
    vk::DeviceSize      size        = {};
    vk::DeviceSize      offset      = {};
    vk::DeviceAddress   address     = {};
    GpuAllocation       allocation  = {};
};

struct GpuTexture {
    vk::Image     image         = {};
    vk::ImageView view          = {};
    vk::Sampler   sampler       = {};
    GpuAllocation allocation    = {};
};

struct GpuShaderObjectCreateInfo {
    vk::ShaderStageFlagBits         stage           = {};
    u64                             codeSize        = {};
    void*                           pCode           = {};
    const char*                     pName           = {};
    Slice<vk::DescriptorSetLayout>  set_layouts     = {};
    Slice<vk::PushConstantRange>    push_constants  = {};
};

struct GpuShaderObject {
    vk::ShaderStageFlagBits                 stage           = {};
    vk::ShaderModule                        shader_module   = {};
    std::string                             name            = {};
    std::vector<vk::DescriptorSetLayout>    set_layouts     = {};
    std::vector<vk::PushConstantRange>      push_constants  = {};
};

struct GpuGraphicsPipelineStateCreateInfo {
    Slice<GpuShaderObject*>            shader_objects          = {};
    GpuInputAssemblyState           input_assembly_state    = {};
    GpuRasterizationState           rasterization_state     = {};
    GpuDepthStencilState            depth_stencil_state     = {};
    GpuColorBlendState              color_blend_state       = {};
    GpuVertexInputState             vertex_input_state      = {};
    Slice<vk::DescriptorSetLayout>  bind_group_layouts      = {};
    Slice<vk::PushConstantRange>    push_constant_ranges    = {};
};

struct GpuGraphicsPipelineState {
    vk::Pipeline        pipeline        = {};
    vk::PipelineLayout  pipeline_layout = {};
};

struct GpuComputePipelineStateCreateInfo {
    GpuShaderObject*                shader_object           = {};
    Slice<vk::DescriptorSetLayout>  bind_group_layouts      = {};
    Slice<vk::PushConstantRange>    push_constant_ranges    = {};
};

struct GpuComputePipelineState {
    vk::Pipeline        pipeline        = {};
    vk::PipelineLayout  pipeline_layout = {};
};

struct GpuLinearAllocator {
    GpuBufferInfo   storage = {};
    vk::DeviceSize  offset  = {};
};

struct GpuCommandBuffer {
    vk::CommandPool     cmd_pool                = {};
    vk::CommandBuffer   cmd_buffer              = {};
    GpuLinearAllocator  buffer_allocator        = {};
    vk::DescriptorPool  bind_group_allocator    = {};
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

void gpu_allocate_memory(GpuContext* context, GpuAllocation* allocation, vk::MemoryRequirements memory_requirements, GpuStorageMode gpu_storage_mode, vk::MemoryAllocateFlags memory_allocate_flags) {
    vk::MemoryPropertyFlags memory_property_flags = {};
    switch (gpu_storage_mode) {
        case GpuStorageMode::ePrivate: {
            memory_property_flags |= vk::MemoryPropertyFlagBits::eDeviceLocal;
            break;
        }
        case GpuStorageMode::eManaged: {
            memory_property_flags |= vk::MemoryPropertyFlagBits::eHostVisible;
            memory_property_flags |= vk::MemoryPropertyFlagBits::eHostCached;
            break;
        }
        case GpuStorageMode::eShared: {
            memory_property_flags |= vk::MemoryPropertyFlagBits::eHostVisible;
            memory_property_flags |= vk::MemoryPropertyFlagBits::eHostCoherent;
            break;
        }
        case GpuStorageMode::eLazy: {
            memory_property_flags |= vk::MemoryPropertyFlagBits::eDeviceLocal;
            memory_property_flags |= vk::MemoryPropertyFlagBits::eLazilyAllocated;
            break;
        }
    }

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

void gpu_buffer_storage(GpuContext* context, GpuBufferInfo* info, GpuStorageMode storage_mode, vk::MemoryAllocateFlags memory_allocate_flags) {
    auto memory_requirements = context->logical_device.getBufferMemoryRequirements(info->buffer);
    gpu_allocate_memory(context, &info->allocation, memory_requirements, storage_mode, memory_allocate_flags);
    context->logical_device.bindBufferMemory(info->buffer, info->allocation.device_memory, 0);

    if (memory_allocate_flags & vk::MemoryAllocateFlagBits::eDeviceAddress) {
        auto device_address_info = vk::BufferDeviceAddressInfo().setBuffer(info->buffer);
        info->address = context->logical_device.getBufferAddress(device_address_info);
    } else {
        info->address = 0;
    }
}

void gpu_buffer_destroy(GpuContext* context, GpuBufferInfo* info) {
    if (info->buffer) {
        context->logical_device.destroyBuffer(info->buffer);
    }
    gpu_free_memory(context, &info->allocation);
}

auto gpu_buffer_contents(GpuBufferInfo* buffer_info) -> void* {
    return reinterpret_cast<u8*>(buffer_info->allocation.mapped) + buffer_info->offset;
}

auto gpu_buffer_device_address(GpuBufferInfo* buffer_info) -> vk::DeviceAddress {
    return buffer_info->address + buffer_info->offset;
}

void gpu_texture_storage(GpuContext* context, GpuAllocation* allocation, vk::Image image, GpuStorageMode gpu_storage_mode, vk::MemoryAllocateFlags memory_allocate_flags) {
    auto memory_requirements = context->logical_device.getImageMemoryRequirements(image);
    gpu_allocate_memory(context, allocation, memory_requirements, gpu_storage_mode, memory_allocate_flags);
    context->logical_device.bindImageMemory(image, allocation->device_memory, 0);
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

    allocator->offset = 0u;
    allocator->storage.size = capacity;
    allocator->storage.offset = 0;
    vk::resultCheck(context->logical_device.createBuffer(&buffer_create_info, nullptr, &allocator->storage.buffer), "Failed to create buffer.");

    gpu_buffer_storage(context, &allocator->storage, GpuStorageMode::eShared, vk::MemoryAllocateFlagBits::eDeviceAddress);
}

void gpu_destroy_allocator(GpuContext* context, GpuLinearAllocator* allocator) {
    gpu_buffer_destroy(context, &allocator->storage);
}

void gpu_create_context(GpuContext* context, WindowPlatform* platform, PFN_vkGetInstanceProcAddr vk_get_instance_proc_addr) {
    vk::defaultDispatchLoaderDynamic.init(vk_get_instance_proc_addr);

    std::vector<const char*> instance_extensions;

    u32 count;
    const char** extensions = glfwGetRequiredInstanceExtensions(&count);
    for (u32 i = 0; i < count; i++) {
        instance_extensions.push_back(extensions[i]);
    }

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

    vk::resultCheck(platform->CreateWindowSurface(context->instance, &context->surface), "Failed to create window surface");

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
    if (aligned_offset + size > allocator->storage.size) {
        return false;
    }
    info->buffer            = allocator->storage.buffer;
    info->size              = size;
    info->offset            = aligned_offset;
    info->address           = allocator->storage.address;
    info->allocation        = allocator->storage.allocation;
    allocator->offset       = aligned_offset + size;
    return true;
}

void gpu_create_graphics_pipeline_state(GpuContext* context, GpuGraphicsPipelineState* state, GpuGraphicsPipelineStateCreateInfo* info, void* pNext) {
    auto layout_create_info = vk::PipelineLayoutCreateInfo()
        .setSetLayouts(info->bind_group_layouts)
        .setPushConstantRanges(info->push_constant_ranges);

    vk::resultCheck(context->logical_device.createPipelineLayout(&layout_create_info, nullptr, &state->pipeline_layout), "Failed to create pipeline layout");

    std::vector<vk::PipelineShaderStageCreateInfo> shader_stages = {};
    for (auto& shader_object : info->shader_objects) {
        vk::PipelineShaderStageCreateInfo stage_create_info = {};
        stage_create_info.setStage(shader_object->stage);
        stage_create_info.setModule(shader_object->shader_module);
        stage_create_info.setPName(shader_object->name.c_str());
        shader_stages.emplace_back(stage_create_info);
    }

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
        .setSubpass(0)
        .setBasePipelineHandle(nullptr)
        .setBasePipelineIndex(-1);

    vk::resultCheck(context->logical_device.createGraphicsPipelines(nullptr, 1, &graphics_pipeline_create_info, nullptr, &state->pipeline), "Failed to create graphics pipeline");
}

void gpu_destroy_graphics_pipeline_state(GpuContext* context, GpuGraphicsPipelineState* state) {
    context->logical_device.destroyPipeline(state->pipeline);
    context->logical_device.destroyPipelineLayout(state->pipeline_layout);
}

void gpu_create_compute_pipeline_state(GpuContext* context, GpuComputePipelineStateCreateInfo* info, GpuComputePipelineState* state) {
    auto shader_stage_create_info = vk::PipelineShaderStageCreateInfo()
        .setStage(info->shader_object->stage)
        .setModule(info->shader_object->shader_module)
        .setPName(info->shader_object->name.c_str());

    auto layout_create_info = vk::PipelineLayoutCreateInfo()
        .setSetLayouts(info->bind_group_layouts)
        .setPushConstantRanges(info->push_constant_ranges);

    state->pipeline_layout = context->logical_device.createPipelineLayout(layout_create_info);

    auto compute_pipeline_create_info = vk::ComputePipelineCreateInfo()
        .setStage(shader_stage_create_info)
        .setLayout(state->pipeline_layout)
        .setBasePipelineHandle(nullptr)
        .setBasePipelineIndex(-1);

    vk::resultCheck(context->logical_device.createComputePipelines(nullptr, 1, &compute_pipeline_create_info, nullptr, &state->pipeline), "Failed to create compute pipeline");
}

void gpu_destroy_compute_pipeline_state(GpuContext* context, GpuComputePipelineState* state) {
    context->logical_device.destroyPipeline(state->pipeline);
    context->logical_device.destroyPipelineLayout(state->pipeline_layout);
}

void gpu_update_buffer(vk::CommandBuffer cmd, GpuBufferInfo* info, void* src, vk::DeviceSize size) {
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

void gpu_create_command_buffer(GpuContext* context, GpuCommandBuffer* command_buffer) {
    // todo: lazy init ???
    gpu_create_allocator(context, &command_buffer->buffer_allocator, 5ull * 1024ull * 1024ull);

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

    vk::DescriptorPoolCreateInfo descriptor_pool_create_info = {};
    descriptor_pool_create_info.setMaxSets(1024);
    descriptor_pool_create_info.setPoolSizes(pool_sizes);

    vk::resultCheck(context->logical_device.createDescriptorPool(&descriptor_pool_create_info, nullptr, &command_buffer->bind_group_allocator), "Failed to create descriptor pool");

    vk::CommandPoolCreateInfo command_pool_create_info = {};
    command_pool_create_info.setQueueFamilyIndex(context->graphics_queue_family_index);
    command_pool_create_info.setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer);

    vk::resultCheck(context->logical_device.createCommandPool(&command_pool_create_info, nullptr, &command_buffer->cmd_pool), "Failed to create command pool");

    vk::CommandBufferAllocateInfo command_buffer_allocate_info = {};
    command_buffer_allocate_info.setCommandPool(command_buffer->cmd_pool);
    command_buffer_allocate_info.setLevel(vk::CommandBufferLevel::ePrimary);
    command_buffer_allocate_info.setCommandBufferCount(1);

    vk::resultCheck(context->logical_device.allocateCommandBuffers(&command_buffer_allocate_info, &command_buffer->cmd_buffer), "Failed to allocate command buffer");
}

void gpu_destroy_command_buffer(GpuContext* context, GpuCommandBuffer* command_buffer) {
    gpu_destroy_allocator(context, &command_buffer->buffer_allocator);
    context->logical_device.destroyDescriptorPool(command_buffer->bind_group_allocator);
    context->logical_device.destroyCommandPool(command_buffer->cmd_pool);
}

void gpu_reset_command_buffer(GpuContext* context, GpuCommandBuffer* command_buffer) {
    command_buffer->buffer_allocator.offset = 0;

//    context->logical_device.resetCommandPool(command_buffer->cmd_pool, {});
    context->logical_device.resetDescriptorPool(command_buffer->bind_group_allocator, {});
}

auto gpu_command_buffer_allocate(GpuContext* context, GpuCommandBuffer* command_buffer, GpuBufferInfo* info, vk::DeviceSize size, vk::DeviceSize alignment) -> bool {
    return gpu_allocator_allocate(&command_buffer->buffer_allocator, info, size, alignment);
}

auto gpu_command_buffer_allocate_bind_group(GpuContext* context, GpuCommandBuffer* command_buffer, vk::DescriptorSetLayout bind_group_layout) -> vk::DescriptorSet {
    vk::DescriptorSetAllocateInfo allocate_info = {};
    allocate_info.setDescriptorPool(command_buffer->bind_group_allocator);
    allocate_info.setDescriptorSetCount(1);
    allocate_info.setPSetLayouts(&bind_group_layout);

    vk::DescriptorSet bind_group;
    vk::resultCheck(context->logical_device.allocateDescriptorSets(&allocate_info, &bind_group), "Failed to allocate descriptor set");
    return bind_group;
}

void gpu_create_shader_object(GpuContext* context, GpuShaderObject* shader_object, const GpuShaderObjectCreateInfo* create_info) {
    vk::ShaderModuleCreateInfo module_create_info = {};
    module_create_info.setCodeSize(create_info->codeSize);
    module_create_info.setPCode(reinterpret_cast<const u32*>(create_info->pCode));

    vk::ShaderModule shader_module;
    vk::resultCheck(context->logical_device.createShaderModule(&module_create_info, nullptr, &shader_module), "Failed to create shader module");

    shader_object->stage = create_info->stage;
    shader_object->shader_module = shader_module;
    shader_object->name = create_info->pName;
    shader_object->set_layouts.assign(create_info->set_layouts.begin(), create_info->set_layouts.end());
    shader_object->push_constants.assign(create_info->push_constants.begin(), create_info->push_constants.end());
}

void gpu_destroy_shader_object(GpuContext* context, GpuShaderObject* shader_object) {
    shader_object->name.clear();
    shader_object->set_layouts.clear();
    shader_object->push_constants.clear();
    context->logical_device.destroyShaderModule(shader_object->shader_module);
}