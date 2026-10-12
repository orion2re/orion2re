#include <SDL3/SDL.h>
#include <vulkan/vulkan.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>
#include "game/scalers/scaler.h"
#include "vulkan_scaler_spv.h"
#include "vulkan_palette_spv.h"

// An isolated Vulkan workload using the production shaders. SDL's public GPU
// interface has no timestamp queries; this tool never accesses its private state.
namespace {
    struct s_buffer {
        VkBuffer handle;
        VkDeviceMemory memory;
        void* mapped;
    };
    struct s_image {
        VkImage handle;
        VkDeviceMemory memory;
        VkImageView view;
        uint32_t width, height;
    };
    struct s_profile {
        VkInstance instance;
        VkPhysicalDevice physical;
        VkDevice device;
        VkQueue queue;
        uint32_t family, timestamp_bits;
        VkPhysicalDeviceProperties properties;
        VkPhysicalDeviceMemoryProperties memory;
        VkCommandPool commands;
        VkCommandBuffer command;
        VkFence fence;
        VkQueryPool queries;
        VkSampler sampler;
        VkDescriptorPool descriptors;
        VkDescriptorSetLayout layouts[4];
        VkPipelineLayout pipeline_layouts[2];
        VkPipeline pipelines[2];
        // Sets: ARGB source, converted source, indexed sources, converted output,
        // scaled output, scaler uniform, palette uniform.
        VkDescriptorSet sets[7];
        // Images: ARGB, indices, palette, cursor, converted, scaled.
        s_image images[6];
        s_buffer upload, download, uniforms;
        uint32_t palette_offset, cursor_offset, uniform_offset;
        bool images_initialized;
    };

    bool Check_(VkResult result, const char* operation) {
        if (result == VK_SUCCESS) return true;
        std::fprintf(stderr, "%s failed: Vulkan result %d\n", operation, (int)result);
        return false;
    }
    uint32_t Memory_Type_(s_profile* state, uint32_t bits, VkMemoryPropertyFlags flags) {
        for (uint32_t i = 0; i < state->memory.memoryTypeCount; ++i)
            if ((bits & (1u << i)) && (state->memory.memoryTypes[i].propertyFlags & flags) == flags) return i;
        return UINT32_MAX;
    }
    bool Allocate_(s_profile* state, VkMemoryRequirements requirements, VkMemoryPropertyFlags flags, VkDeviceMemory* memory) {
        const uint32_t type = Memory_Type_(state, requirements.memoryTypeBits, flags);
        if (type == UINT32_MAX) {
            std::fprintf(stderr, "Required Vulkan memory type unavailable\n");
            return false;
        }
        VkMemoryAllocateInfo info = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        info.allocationSize = requirements.size;
        info.memoryTypeIndex = type;
        return Check_(vkAllocateMemory(state->device, &info, nullptr, memory), "allocate memory");
    }
    bool Buffer_(s_profile* state, s_buffer* buffer, VkDeviceSize size, VkBufferUsageFlags usage) {
        VkBufferCreateInfo info = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        info.size = size;
        info.usage = usage;
        if (!Check_(vkCreateBuffer(state->device, &info, nullptr, &buffer->handle), "create buffer")) return false;
        VkMemoryRequirements requirements;
        vkGetBufferMemoryRequirements(state->device, buffer->handle, &requirements);
        return Allocate_(state, requirements, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &buffer->memory) &&
            Check_(vkBindBufferMemory(state->device, buffer->handle, buffer->memory, 0), "bind buffer") &&
            Check_(vkMapMemory(state->device, buffer->memory, 0, VK_WHOLE_SIZE, 0, &buffer->mapped), "map buffer");
    }
    bool Image_(s_profile* state, s_image* image, uint32_t width, uint32_t height, VkFormat format, bool storage) {
        image->width = width;
        image->height = height;
        VkImageCreateInfo info = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        info.imageType = VK_IMAGE_TYPE_2D;
        info.format = format;
        info.extent = {width, height, 1};
        info.mipLevels = info.arrayLayers = 1;
        info.samples = VK_SAMPLE_COUNT_1_BIT;
        info.tiling = VK_IMAGE_TILING_OPTIMAL;
        info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        if (storage) info.usage |= VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        if (!Check_(vkCreateImage(state->device, &info, nullptr, &image->handle), "create image")) return false;
        VkMemoryRequirements requirements;
        vkGetImageMemoryRequirements(state->device, image->handle, &requirements);
        if (!Allocate_(state, requirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &image->memory) ||
            !Check_(vkBindImageMemory(state->device, image->handle, image->memory, 0), "bind image")) return false;
        VkImageViewCreateInfo view = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        view.image = image->handle;
        view.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view.format = format;
        view.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        return Check_(vkCreateImageView(state->device, &view, nullptr, &image->view), "create image view");
    }
    void Destroy_(s_profile* state) {
        if (state->device) {
            vkDeviceWaitIdle(state->device);
            for (VkPipeline pipeline : state->pipelines)
                if (pipeline) vkDestroyPipeline(state->device, pipeline, nullptr);
            for (VkPipelineLayout layout : state->pipeline_layouts)
                if (layout) vkDestroyPipelineLayout(state->device, layout, nullptr);
            if (state->descriptors) vkDestroyDescriptorPool(state->device, state->descriptors, nullptr);
            for (VkDescriptorSetLayout layout : state->layouts)
                if (layout) vkDestroyDescriptorSetLayout(state->device, layout, nullptr);
            if (state->sampler) vkDestroySampler(state->device, state->sampler, nullptr);
            for (s_image& image : state->images) {
                if (image.view) vkDestroyImageView(state->device, image.view, nullptr);
                if (image.handle) vkDestroyImage(state->device, image.handle, nullptr);
                if (image.memory) vkFreeMemory(state->device, image.memory, nullptr);
            }
            for (s_buffer* buffer : {&state->upload, &state->download, &state->uniforms}) {
                if (buffer->mapped) vkUnmapMemory(state->device, buffer->memory);
                if (buffer->handle) vkDestroyBuffer(state->device, buffer->handle, nullptr);
                if (buffer->memory) vkFreeMemory(state->device, buffer->memory, nullptr);
            }
            if (state->queries) vkDestroyQueryPool(state->device, state->queries, nullptr);
            if (state->fence) vkDestroyFence(state->device, state->fence, nullptr);
            if (state->commands) vkDestroyCommandPool(state->device, state->commands, nullptr);
            vkDestroyDevice(state->device, nullptr);
        }
        if (state->instance) vkDestroyInstance(state->instance, nullptr);
        *state = {};
    }
    bool Pipeline_(s_profile* state, int index, const unsigned char* code, size_t size) {
        // Embedded byte arrays have no guaranteed uint32_t alignment.
        std::vector<uint32_t> aligned(size / 4);
        if (size % 4 != 0) return false;
        std::memcpy(aligned.data(), code, size);
        VkShaderModuleCreateInfo module_info = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        module_info.codeSize = size;
        module_info.pCode = aligned.data();
        VkShaderModule module;
        if (!Check_(vkCreateShaderModule(state->device, &module_info, nullptr, &module), "create shader")) return false;
        VkDescriptorSetLayout layouts[] = {state->layouts[index], state->layouts[2], state->layouts[3]};
        VkPipelineLayoutCreateInfo layout_info = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        layout_info.setLayoutCount = 3;
        layout_info.pSetLayouts = layouts;
        bool success = Check_(vkCreatePipelineLayout(state->device, &layout_info, nullptr, &state->pipeline_layouts[index]), "create pipeline layout");
        if (success) {
            VkComputePipelineCreateInfo info = {VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
            info.layout = state->pipeline_layouts[index];
            info.stage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
            info.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
            info.stage.module = module;
            info.stage.pName = "main";
            success = Check_(vkCreateComputePipelines(state->device, VK_NULL_HANDLE, 1, &info, nullptr, &state->pipelines[index]), "create compute pipeline");
        }
        vkDestroyShaderModule(state->device, module, nullptr);
        return success;
    }
    void Image_Descriptor_(s_profile* state, int set, int binding, int image, bool storage) {
        VkDescriptorImageInfo value = {storage ? VK_NULL_HANDLE : state->sampler, state->images[image].view, VK_IMAGE_LAYOUT_GENERAL};
        VkWriteDescriptorSet write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        write.dstSet = state->sets[set];
        write.dstBinding = binding;
        write.descriptorCount = 1;
        write.descriptorType = storage ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE : VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.pImageInfo = &value;
        vkUpdateDescriptorSets(state->device, 1, &write, 0, nullptr);
    }
    bool Initialize_(s_profile* state, int width, int height) {
        VkApplicationInfo app = {VK_STRUCTURE_TYPE_APPLICATION_INFO};
        app.pApplicationName = "orion2re scaler profiler";
        app.apiVersion = VK_API_VERSION_1_0;
        VkInstanceCreateInfo instance = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
        instance.pApplicationInfo = &app;
        if (!Check_(vkCreateInstance(&instance, nullptr, &state->instance), "create Vulkan 1.0 instance")) return false;
        uint32_t count = 0;
        if (!Check_(vkEnumeratePhysicalDevices(state->instance, &count, nullptr), "enumerate devices") || count == 0) return false;
        std::vector<VkPhysicalDevice> devices(count);
        if (!Check_(vkEnumeratePhysicalDevices(state->instance, &count, devices.data()), "enumerate devices")) return false;
        for (VkPhysicalDevice device : devices) {
            VkPhysicalDeviceProperties properties;
            vkGetPhysicalDeviceProperties(device, &properties);
            if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU && SDL_getenv("ORION2RE_VULKAN_ALLOW_SOFTWARE") == nullptr) continue;
            uint32_t families = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(device, &families, nullptr);
            std::vector<VkQueueFamilyProperties> queues(families);
            vkGetPhysicalDeviceQueueFamilyProperties(device, &families, queues.data());
            for (uint32_t i = 0; i < families; ++i) {
                if ((queues[i].queueFlags & VK_QUEUE_COMPUTE_BIT) && queues[i].timestampValidBits > 0) {
                    state->physical = device;
                    state->properties = properties;
                    state->family = i;
                    state->timestamp_bits = queues[i].timestampValidBits;
                    break;
                }
            }
            if (state->physical) break;
        }
        if (!state->physical) {
            std::fprintf(stderr, "No compute device with timestamp support\n");
            return false;
        }
        std::fprintf(stderr, "Profile device: %s; Vulkan 1.0, %u timestamp bits\n", state->properties.deviceName, state->timestamp_bits);
        vkGetPhysicalDeviceMemoryProperties(state->physical, &state->memory);
        const float priority = 1.0f;
        VkDeviceQueueCreateInfo queue = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
        queue.queueFamilyIndex = state->family;
        queue.queueCount = 1;
        queue.pQueuePriorities = &priority;
        VkDeviceCreateInfo device = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
        device.queueCreateInfoCount = 1;
        device.pQueueCreateInfos = &queue;
        if (!Check_(vkCreateDevice(state->physical, &device, nullptr, &state->device), "create device")) return false;
        vkGetDeviceQueue(state->device, state->family, 0, &state->queue);
        VkCommandPoolCreateInfo pool = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        pool.queueFamilyIndex = state->family;
        pool.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        if (!Check_(vkCreateCommandPool(state->device, &pool, nullptr, &state->commands), "create command pool")) return false;
        VkCommandBufferAllocateInfo command = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        command.commandPool = state->commands;
        command.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        command.commandBufferCount = 1;
        if (!Check_(vkAllocateCommandBuffers(state->device, &command, &state->command), "allocate command")) return false;
        VkFenceCreateInfo fence = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        VkQueryPoolCreateInfo queries = {VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
        queries.queryType = VK_QUERY_TYPE_TIMESTAMP;
        queries.queryCount = 5;
        if (!Check_(vkCreateFence(state->device, &fence, nullptr, &state->fence), "create fence") ||
            !Check_(vkCreateQueryPool(state->device, &queries, nullptr, &state->queries), "create queries")) return false;
        const uint32_t bytes = width * height;
        state->palette_offset = (bytes + 3) & ~3u;
        state->cursor_offset = state->palette_offset + 1024;
        state->uniform_offset = (uint32_t)std::max<VkDeviceSize>(32, state->properties.limits.minUniformBufferOffsetAlignment);
        if (!Buffer_(state, &state->upload, std::max(bytes * 4, state->cursor_offset + 576), VK_BUFFER_USAGE_TRANSFER_SRC_BIT) ||
            !Buffer_(state, &state->download, bytes * 16, VK_BUFFER_USAGE_TRANSFER_DST_BIT) ||
            !Buffer_(state, &state->uniforms, state->uniform_offset + 32, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)) return false;
        const int32_t scaler_parameters[8] = {width, height, scalers::SCALER_MMPX2, 2};
        const int32_t palette_parameters[8] = {width, height, 0, 0, 0};
        std::memcpy(state->uniforms.mapped, scaler_parameters, 32);
        std::memcpy((uint8_t*)state->uniforms.mapped + state->uniform_offset, palette_parameters, 32);
        if (!Image_(state, &state->images[0], width, height, VK_FORMAT_B8G8R8A8_UNORM, false) ||
            !Image_(state, &state->images[1], width, height, VK_FORMAT_R8_UNORM, false) ||
            !Image_(state, &state->images[2], 256, 1, VK_FORMAT_R8G8B8A8_UNORM, false) ||
            !Image_(state, &state->images[3], 24, 24, VK_FORMAT_R8_UNORM, false) ||
            !Image_(state, &state->images[4], width, height, VK_FORMAT_R8G8B8A8_UNORM, true) ||
            !Image_(state, &state->images[5], width * 2, height * 2, VK_FORMAT_R8G8B8A8_UNORM, true)) return false;
        VkSamplerCreateInfo sampler = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        sampler.magFilter = sampler.minFilter = VK_FILTER_NEAREST;
        sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        sampler.addressModeU = sampler.addressModeV = sampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        if (!Check_(vkCreateSampler(state->device, &sampler, nullptr, &state->sampler), "create sampler")) return false;
        for (int layout = 0; layout < 4; ++layout) {
            VkDescriptorSetLayoutBinding bindings[3] = {};
            const uint32_t count = layout == 1 ? 3 : 1;
            for (uint32_t i = 0; i < count; ++i) {
                bindings[i].binding = i;
                bindings[i].descriptorCount = 1;
                bindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
                bindings[i].descriptorType = layout < 2 ? VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER : layout == 2 ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE
                                                                                                                  : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            }
            VkDescriptorSetLayoutCreateInfo info = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
            info.bindingCount = count;
            info.pBindings = bindings;
            if (!Check_(vkCreateDescriptorSetLayout(state->device, &info, nullptr, &state->layouts[layout]), "create descriptor layout")) return false;
        }
        const VkDescriptorPoolSize sizes[] = {{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 5}, {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 2}, {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2}};
        VkDescriptorPoolCreateInfo descriptors = {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        descriptors.maxSets = 7;
        descriptors.poolSizeCount = 3;
        descriptors.pPoolSizes = sizes;
        if (!Check_(vkCreateDescriptorPool(state->device, &descriptors, nullptr, &state->descriptors), "create descriptor pool")) return false;
        const VkDescriptorSetLayout layouts[] = {state->layouts[0], state->layouts[0], state->layouts[1], state->layouts[2], state->layouts[2], state->layouts[3], state->layouts[3]};
        VkDescriptorSetAllocateInfo sets = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        sets.descriptorPool = state->descriptors;
        sets.descriptorSetCount = 7;
        sets.pSetLayouts = layouts;
        if (!Check_(vkAllocateDescriptorSets(state->device, &sets, state->sets), "allocate descriptor sets")) return false;
        Image_Descriptor_(state, 0, 0, 0, false);
        Image_Descriptor_(state, 1, 0, 4, false);
        for (int i = 0; i < 3; ++i) Image_Descriptor_(state, 2, i, i + 1, false);
        Image_Descriptor_(state, 3, 0, 4, true);
        Image_Descriptor_(state, 4, 0, 5, true);
        for (int i = 0; i < 2; ++i) {
            VkDescriptorBufferInfo buffer = {state->uniforms.handle, i == 0 ? 0u : state->uniform_offset, 32};
            VkWriteDescriptorSet write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            write.dstSet = state->sets[5 + i];
            write.descriptorCount = 1;
            write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            write.pBufferInfo = &buffer;
            vkUpdateDescriptorSets(state->device, 1, &write, 0, nullptr);
        }
        return Pipeline_(state, 0, k_vulkan_scaler_data, k_vulkan_scaler_size) && Pipeline_(state, 1, k_vulkan_palette_data, k_vulkan_palette_size);
    }
    void Barrier_(s_profile* state, int image, bool initial, VkAccessFlags source, VkAccessFlags destination) {
        VkImageMemoryBarrier barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        barrier.srcAccessMask = source;
        barrier.dstAccessMask = destination;
        barrier.oldLayout = initial ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_GENERAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = state->images[image].handle;
        barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        vkCmdPipelineBarrier(state->command, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    }
    void Upload_(s_profile* state, int image, uint32_t offset) {
        const s_image& target = state->images[image];
        VkBufferImageCopy region = {};
        region.bufferOffset = offset;
        region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        region.imageExtent = {target.width, target.height, 1};
        vkCmdCopyBufferToImage(state->command, state->upload.handle, target.handle, VK_IMAGE_LAYOUT_GENERAL, 1, &region);
        Barrier_(state, image, false, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
    }
    void Dispatch_(s_profile* state, int pipeline, int input, int output, int uniform) {
        VkDescriptorSet sets[] = {state->sets[input], state->sets[output], state->sets[uniform]};
        vkCmdBindPipeline(state->command, VK_PIPELINE_BIND_POINT_COMPUTE, state->pipelines[pipeline]);
        vkCmdBindDescriptorSets(state->command, VK_PIPELINE_BIND_POINT_COMPUTE, state->pipeline_layouts[pipeline], 0, 3, sets, 0, nullptr);
        vkCmdDispatch(state->command, (state->images[0].width + 7) / 8, (state->images[0].height + 7) / 8, 1);
    }
    bool Run_(s_profile* state, const std::vector<uint32_t>& pixels, const std::vector<uint8_t>& indices, const uint32_t* palette, const uint32_t* expected, int expected_stride, bool indexed) {
        const int width = state->images[0].width, height = state->images[0].height;
        double averages[6] = {};
        for (int iteration = 0; iteration < 35; ++iteration) {
            const uint64_t pack_start = SDL_GetTicksNS();
            if (indexed) {
                std::memcpy(state->upload.mapped, indices.data(), indices.size());
                auto* colors = (uint8_t*)state->upload.mapped + state->palette_offset;
                for (int i = 0; i < 256; ++i) {
                    colors[i * 4] = palette[i] >> 16;
                    colors[i * 4 + 1] = palette[i] >> 8;
                    colors[i * 4 + 2] = palette[i];
                    colors[i * 4 + 3] = palette[i] >> 24;
                }
                std::memset((uint8_t*)state->upload.mapped + state->cursor_offset, 0, 576);
            } else
                std::memcpy(state->upload.mapped, pixels.data(), pixels.size() * 4);
            const double pack_ms = (SDL_GetTicksNS() - pack_start) / 1e6;
            if (!Check_(vkResetCommandBuffer(state->command, 0), "reset command")) return false;
            VkCommandBufferBeginInfo begin = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
            begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            if (!Check_(vkBeginCommandBuffer(state->command, &begin), "begin command")) return false;
            for (int i = 0; i < 6; ++i) Barrier_(state, i, !state->images_initialized, state->images_initialized ? VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT : 0, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT);
            vkCmdResetQueryPool(state->command, state->queries, 0, 5);
            vkCmdWriteTimestamp(state->command, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, state->queries, 0);
            if (indexed) {
                Upload_(state, 1, 0);
                Upload_(state, 2, state->palette_offset);
                Upload_(state, 3, state->cursor_offset);
            } else
                Upload_(state, 0, 0);
            vkCmdWriteTimestamp(state->command, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, state->queries, 1);
            if (indexed) {
                Dispatch_(state, 1, 2, 3, 6);
                Barrier_(state, 4, false, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
            }
            vkCmdWriteTimestamp(state->command, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, state->queries, 2);
            Dispatch_(state, 0, indexed ? 1 : 0, 4, 5);
            Barrier_(state, 5, false, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
            vkCmdWriteTimestamp(state->command, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, state->queries, 3);
            VkBufferImageCopy copy = {};
            copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
            copy.imageExtent = {(uint32_t)width * 2, (uint32_t)height * 2, 1};
            vkCmdCopyImageToBuffer(state->command, state->images[5].handle, VK_IMAGE_LAYOUT_GENERAL, state->download.handle, 1, &copy);
            VkMemoryBarrier host = {VK_STRUCTURE_TYPE_MEMORY_BARRIER};
            host.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            host.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
            vkCmdPipelineBarrier(state->command, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &host, 0, nullptr, 0, nullptr);
            vkCmdWriteTimestamp(state->command, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, state->queries, 4);
            if (!Check_(vkEndCommandBuffer(state->command), "end command") || !Check_(vkResetFences(state->device, 1, &state->fence), "reset fence")) return false;
            VkSubmitInfo submit = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
            submit.commandBufferCount = 1;
            submit.pCommandBuffers = &state->command;
            const uint64_t submit_start = SDL_GetTicksNS();
            if (!Check_(vkQueueSubmit(state->queue, 1, &submit, state->fence), "submit") ||
                !Check_(vkWaitForFences(state->device, 1, &state->fence, VK_TRUE, UINT64_MAX), "wait fence")) return false;
            const double submit_ms = (SDL_GetTicksNS() - submit_start) / 1e6;
            state->images_initialized = true;
            uint64_t timestamps[5];
            if (!Check_(vkGetQueryPoolResults(state->device, state->queries, 0, 5, sizeof(timestamps), timestamps, sizeof(uint64_t), VK_QUERY_RESULT_64_BIT), "query results")) return false;
            if (iteration >= 5) {
                averages[0] += pack_ms / 30;
                averages[5] += submit_ms / 30;
                const uint64_t mask = state->timestamp_bits == 64 ? UINT64_MAX : (UINT64_C(1) << state->timestamp_bits) - 1;
                for (int i = 0; i < 4; ++i) averages[i + 1] += ((timestamps[i + 1] - timestamps[i]) & mask) * state->properties.limits.timestampPeriod / 1e6 / 30;
            }
        }
        const uint8_t* result = (const uint8_t*)state->download.mapped;
        for (int y = 0; y < height * 2; ++y)
            for (int x = 0; x < width * 2; ++x) {
                const uint8_t* rgba = result + ((size_t)y * width * 2 + x) * 4;
                const uint32_t argb = ((uint32_t)rgba[3] << 24) | ((uint32_t)rgba[0] << 16) | ((uint32_t)rgba[1] << 8) | rgba[2];
                if (argb != expected[y * expected_stride + x]) {
                    std::fprintf(stderr, "Profiler parity failed at %d,%d\n", x, y);
                    return false;
                }
            }
        std::printf("%s,mmpx2,%d,%d,%zu,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f\n", indexed ? "indexed" : "argb", width, height, indexed ? indices.size() + 1024 + 576 : pixels.size() * 4, averages[0], averages[1], averages[2], averages[3], averages[4], averages[5]);
        return true;
    }
} // namespace

int main(int argc, char** argv) {
    if (argc > 2) {
        std::fprintf(stderr, "Usage: vulkan-scaler-profile [INPUT.bmp]\n");
        return 1;
    }
    int width = 640, height = 480;
    std::vector<uint32_t> pixels;
    if (argc == 2) {
        SDL_Surface* loaded = SDL_LoadBMP(argv[1]);
        SDL_Surface* source = loaded ? SDL_ConvertSurface(loaded, SDL_PIXELFORMAT_ARGB8888) : nullptr;
        SDL_DestroySurface(loaded);
        if (!source || source->w > 640 || source->h > 480) {
            std::fprintf(stderr, "Expected BMP at most 640x480: %s\n", SDL_GetError());
            SDL_DestroySurface(source);
            return 1;
        }
        width = source->w;
        height = source->h;
        pixels.resize(width * height);
        for (int y = 0; y < height; ++y) std::memcpy(pixels.data() + y * width, (uint8_t*)source->pixels + y * source->pitch, width * 4);
        SDL_DestroySurface(source);
    } else {
        pixels.resize(width * height);
        const uint32_t colors[] = {0xff000000u, 0xffffffffu, 0xff2050a0u, 0xff40a020u};
        for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x) pixels[y * width + x] = colors[((x / 7) ^ (y / 5)) & 3];
    }
    uint32_t palette[256] = {};
    int palette_count = 0;
    std::vector<uint8_t> indices(pixels.size());
    for (size_t i = 0; i < pixels.size(); ++i) {
        int color = 0;
        while (color < palette_count && palette[color] != pixels[i]) ++color;
        if (color == palette_count) {
            if (palette_count == 256) {
                std::fprintf(stderr, "Indexed comparison requires at most 256 distinct colors\n");
                return 1;
            }
            palette[palette_count++] = pixels[i];
        }
        indices[i] = (uint8_t)color;
    }
    scalers::s_pipeline cpu = {};
    s_profile profile = {};
    bool success = scalers::Initialize(&cpu, width, height) && scalers::Configure(&cpu, scalers::SCALER_MMPX2) &&
        scalers::Process(&cpu, pixels.data(), width) && Initialize_(&profile, width, height);
    std::puts("path,scaler,width,height,upload_bytes,host_pack_ms,upload_gpu_ms,palette_gpu_ms,scaler_gpu_ms,readback_gpu_ms,submit_wait_ms");
    if (success) success = Run_(&profile, pixels, indices, palette, scalers::Get_Output_Pixels(&cpu), scalers::Get_Output_Stride(&cpu), false) &&
        Run_(&profile, pixels, indices, palette, scalers::Get_Output_Pixels(&cpu), scalers::Get_Output_Stride(&cpu), true);
    Destroy_(&profile);
    scalers::Shutdown(&cpu);
    return success ? 0 : 1;
}
