#include "game/scalers/vulkan_scaler.h"
#include <vulkan/vulkan.h>
#include <cstring>
#include <new>
#include <vector>
#include "vulkan_scaler_spv.h"

namespace vulkan_scalers {
    namespace {
        struct s_pipeline_state {
            SDL_GPUDevice* device = nullptr;
            SDL_Renderer* renderer = nullptr;
            SDL_GPUComputePipeline* kernel = nullptr;
            SDL_GPUSampler* sampler = nullptr;
            SDL_GPUTexture* input = nullptr;
            SDL_GPUTexture* intermediate = nullptr;
            SDL_GPUTexture* output = nullptr;
            SDL_GPUTransferBuffer* upload = nullptr;
            SDL_Texture* presentation = nullptr;
            int32_t width = 0;
            int32_t height = 0;
            int32_t factor = 0;
            scalers::e_scaler scaler = scalers::SCALER_PIXELART;
        };

        // Match SDL's selected device to Vulkan's own reported name, without parsing it.
        bool Check_Device_Version_(SDL_GPUDevice* device) {
            VkApplicationInfo application = {};
            application.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
            application.apiVersion = VK_API_VERSION_1_4;
            VkInstanceCreateInfo create = {};
            create.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
            create.pApplicationInfo = &application;
            VkInstance instance = VK_NULL_HANDLE;
            if (vkCreateInstance(&create, nullptr, &instance) != VK_SUCCESS) return false;
            uint32_t count = 0;
            bool valid = vkEnumeratePhysicalDevices(instance, &count, nullptr) == VK_SUCCESS;
            std::vector<VkPhysicalDevice> devices(count);
            valid = valid && vkEnumeratePhysicalDevices(instance, &count, devices.data()) == VK_SUCCESS;
            const char* name = SDL_GetStringProperty(SDL_GetGPUDeviceProperties(device), SDL_PROP_GPU_DEVICE_NAME_STRING, "");
            bool found = false;
            for (uint32_t i = 0; valid && i < count; ++i) {
                VkPhysicalDeviceProperties properties = {};
                vkGetPhysicalDeviceProperties(devices[i], &properties);
                if (std::strcmp(properties.deviceName, name) == 0) {
                    found = true;
                    valid = properties.apiVersion >= VK_API_VERSION_1_4;
                }
            }
            vkDestroyInstance(instance, nullptr);
            return valid && found;
        }

        SDL_GPUTexture* Create_Texture_(s_pipeline_state* state, int32_t width, int32_t height, bool storage) {
            SDL_GPUTextureCreateInfo info = {};
            info.type = SDL_GPU_TEXTURETYPE_2D;
            info.format = storage ? SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM : SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM;
            info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
            if (storage) info.usage |= SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE;
            info.width = width;
            info.height = height;
            info.layer_count_or_depth = 1;
            info.num_levels = 1;
            info.sample_count = SDL_GPU_SAMPLECOUNT_1;
            return SDL_CreateGPUTexture(state->device, &info);
        }

        void Destroy_Output_(s_pipeline_state* state) {
            SDL_DestroyTexture(state->presentation);
            state->presentation = nullptr;
            if (state->input) SDL_ReleaseGPUTexture(state->device, state->input);
            if (state->intermediate) SDL_ReleaseGPUTexture(state->device, state->intermediate);
            if (state->output) SDL_ReleaseGPUTexture(state->device, state->output);
            if (state->upload) SDL_ReleaseGPUTransferBuffer(state->device, state->upload);
            state->input = state->intermediate = state->output = nullptr;
            state->upload = nullptr;
            state->width = state->height = state->factor = 0;
        }

        void Dispatch_(s_pipeline_state* state, SDL_GPUCommandBuffer* commands, SDL_GPUTexture* input, SDL_GPUTexture* output, int32_t width, int32_t height, int32_t factor) {
            struct s_parameters {
                int32_t width, height, mode, factor;
            } parameters = {width, height, state->scaler, factor};
            SDL_PushGPUComputeUniformData(commands, 0, &parameters, sizeof(parameters));
            SDL_GPUStorageTextureReadWriteBinding destination = {};
            destination.texture = output;
            destination.cycle = true;
            SDL_GPUComputePass* pass = SDL_BeginGPUComputePass(commands, &destination, 1, nullptr, 0);
            SDL_BindGPUComputePipeline(pass, state->kernel);
            const SDL_GPUTextureSamplerBinding source = {input, state->sampler};
            SDL_BindGPUComputeSamplers(pass, 0, &source, 1);
            SDL_DispatchGPUCompute(pass, (width + 7) / 8, (height + 7) / 8, 1);
            SDL_EndGPUComputePass(pass);
        }
    } // namespace

    SDL_GPUDevice* Create_Device() {
        if (SDL_getenv("ORION2RE_VULKAN_VALIDATION") != nullptr) {
            const char* layers = SDL_getenv("VK_INSTANCE_LAYERS");
            if (layers == nullptr || SDL_strstr(layers, "VK_LAYER_KHRONOS_validation") == nullptr) {
                char requested_layers[4096];
                const int length = SDL_snprintf(requested_layers, sizeof(requested_layers), "%s%sVK_LAYER_KHRONOS_validation", layers != nullptr ? layers : "", layers != nullptr && layers[0] != '\0' ? ":" : "");
                if (length < 0 || static_cast<size_t>(length) >= sizeof(requested_layers) ||
                    SDL_setenv_unsafe("VK_INSTANCE_LAYERS", requested_layers, 1) != 0) {
                    SDL_SetError("Unable to enable Vulkan validation layers");
                    return nullptr;
                }
            }
        }
        SDL_GPUVulkanOptions options = {};
        options.vulkan_api_version = VK_API_VERSION_1_4;
        SDL_PropertiesID properties = SDL_CreateProperties();
        SDL_SetStringProperty(properties, SDL_PROP_GPU_DEVICE_CREATE_NAME_STRING, "vulkan");
        SDL_SetBooleanProperty(properties, SDL_PROP_GPU_DEVICE_CREATE_SHADERS_SPIRV_BOOLEAN, true);
        SDL_SetBooleanProperty(properties, SDL_PROP_GPU_DEVICE_CREATE_VULKAN_REQUIRE_HARDWARE_ACCELERATION_BOOLEAN, SDL_getenv("ORION2RE_VULKAN_ALLOW_SOFTWARE") == nullptr);
        SDL_SetPointerProperty(properties, SDL_PROP_GPU_DEVICE_CREATE_VULKAN_OPTIONS_POINTER, &options);
        // Use Khronos validation instead of SDL's additional bounds assertions:
        // Vulkan permits scissors/viewports extending beyond a render target.
        SDL_SetBooleanProperty(properties, SDL_PROP_GPU_DEVICE_CREATE_DEBUGMODE_BOOLEAN, false);
        SDL_GPUDevice* device = SDL_CreateGPUDeviceWithProperties(properties);
        SDL_DestroyProperties(properties);
        if (device != nullptr && !Check_Device_Version_(device)) {
            SDL_DestroyGPUDevice(device);
            SDL_SetError("The selected GPU does not support Vulkan 1.4");
            return nullptr;
        }
        if (device != nullptr) {
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Vulkan 1.4 GPU scaler: %s", SDL_GetStringProperty(SDL_GetGPUDeviceProperties(device), SDL_PROP_GPU_DEVICE_NAME_STRING, "unknown"));
        }
        return device;
    }

    bool Initialize(s_pipeline* pipeline, SDL_GPUDevice* device) {
        if (pipeline == nullptr || device == nullptr || pipeline->implementation != nullptr) return false;
        s_pipeline_state* state = new (std::nothrow) s_pipeline_state;
        if (state == nullptr) return false;
        pipeline->implementation = state;
        state->device = device;
        SDL_GPUComputePipelineCreateInfo info = {};
        info.code = k_vulkan_scaler_data;
        info.code_size = k_vulkan_scaler_size;
        info.entrypoint = "main";
        info.format = SDL_GPU_SHADERFORMAT_SPIRV;
        info.num_samplers = 1;
        info.num_readwrite_storage_textures = 1;
        info.num_uniform_buffers = 1;
        info.threadcount_x = info.threadcount_y = 8;
        info.threadcount_z = 1;
        state->kernel = SDL_CreateGPUComputePipeline(device, &info);
        SDL_GPUSamplerCreateInfo sampler = {};
        sampler.min_filter = sampler.mag_filter = SDL_GPU_FILTER_NEAREST;
        sampler.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
        sampler.address_mode_u = sampler.address_mode_v = sampler.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        state->sampler = SDL_CreateGPUSampler(device, &sampler);
        if (state->kernel == nullptr || state->sampler == nullptr) {
            Shutdown(pipeline);
            return false;
        }
        return true;
    }

    bool Configure(s_pipeline* pipeline, SDL_Renderer* renderer, int32_t width, int32_t height, scalers::e_scaler scaler) {
        if (pipeline == nullptr || pipeline->implementation == nullptr || width <= 0 || height <= 0 ||
            width > scalers::MAX_SOURCE_WIDTH || height > scalers::MAX_SOURCE_HEIGHT || scalers::Get_Factor(scaler) == 0) return false;
        auto* state = static_cast<s_pipeline_state*>(pipeline->implementation);
        if (state->output != nullptr && state->width == width && state->height == height && state->scaler == scaler && state->renderer == renderer) return true;
        if (renderer != nullptr && !SDL_FlushRenderer(renderer)) return false;
        if (!SDL_WaitForGPUIdle(state->device)) return false;
        Destroy_Output_(state);
        state->width = width;
        state->height = height;
        state->factor = scalers::Get_Factor(scaler);
        state->scaler = scaler;
        state->renderer = renderer;
        state->input = Create_Texture_(state, width, height, false);
        state->output = Create_Texture_(state, width * state->factor, height * state->factor, true);
        if (scaler == scalers::SCALER_MMPX4) state->intermediate = Create_Texture_(state, width * 2, height * 2, true);
        SDL_GPUTransferBufferCreateInfo upload = {};
        upload.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        upload.size = width * height * sizeof(uint32_t);
        state->upload = SDL_CreateGPUTransferBuffer(state->device, &upload);
        if (state->input == nullptr || state->output == nullptr || state->upload == nullptr ||
            (scaler == scalers::SCALER_MMPX4 && state->intermediate == nullptr)) {
            Destroy_Output_(state);
            return false;
        }
        if (renderer != nullptr) {
            SDL_PropertiesID properties = SDL_CreateProperties();
            SDL_SetNumberProperty(properties, SDL_PROP_TEXTURE_CREATE_FORMAT_NUMBER, SDL_PIXELFORMAT_ABGR8888);
            SDL_SetNumberProperty(properties, SDL_PROP_TEXTURE_CREATE_WIDTH_NUMBER, width * state->factor);
            SDL_SetNumberProperty(properties, SDL_PROP_TEXTURE_CREATE_HEIGHT_NUMBER, height * state->factor);
            SDL_SetPointerProperty(properties, SDL_PROP_TEXTURE_CREATE_GPU_TEXTURE_POINTER, state->output);
            state->presentation = SDL_CreateTextureWithProperties(renderer, properties);
            SDL_DestroyProperties(properties);
            if (state->presentation == nullptr || !SDL_SetTextureBlendMode(state->presentation, SDL_BLENDMODE_NONE) ||
                !SDL_SetTextureScaleMode(state->presentation, scalers::Uses_Linear_Filter(scaler) ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_PIXELART)) {
                Destroy_Output_(state);
                return false;
            }
        }
        return true;
    }

    bool Process(s_pipeline* pipeline, const uint32_t* pixels, int32_t stride) {
        if (pipeline == nullptr || pipeline->implementation == nullptr || pixels == nullptr) return false;
        auto* state = static_cast<s_pipeline_state*>(pipeline->implementation);
        if (state->output == nullptr || stride < state->width) return false;
        if (state->renderer != nullptr && !SDL_FlushRenderer(state->renderer)) return false;
        auto* mapped = static_cast<uint32_t*>(SDL_MapGPUTransferBuffer(state->device, state->upload, true));
        if (mapped == nullptr) return false;
        for (int32_t y = 0; y < state->height; ++y) {
            std::memcpy(mapped + y * state->width, pixels + y * stride, state->width * sizeof(uint32_t));
        }
        SDL_UnmapGPUTransferBuffer(state->device, state->upload);
        SDL_GPUCommandBuffer* commands = SDL_AcquireGPUCommandBuffer(state->device);
        if (commands == nullptr) return false;
        SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(commands);
        SDL_GPUTextureTransferInfo source = {};
        source.transfer_buffer = state->upload;
        source.pixels_per_row = state->width;
        source.rows_per_layer = state->height;
        SDL_GPUTextureRegion destination = {};
        destination.texture = state->input;
        destination.w = state->width;
        destination.h = state->height;
        destination.d = 1;
        SDL_UploadToGPUTexture(copy, &source, &destination, true);
        SDL_EndGPUCopyPass(copy);
        if (state->intermediate != nullptr) {
            Dispatch_(state, commands, state->input, state->intermediate, state->width, state->height, 2);
            Dispatch_(state, commands, state->intermediate, state->output, state->width * 2, state->height * 2, 2);
        } else {
            Dispatch_(state, commands, state->input, state->output, state->width, state->height, state->factor);
        }
        return SDL_SubmitGPUCommandBuffer(commands);
    }

    SDL_Texture* Get_Texture(s_pipeline* pipeline) {
        if (pipeline == nullptr || pipeline->implementation == nullptr) return nullptr;
        return static_cast<s_pipeline_state*>(pipeline->implementation)->presentation;
    }

    bool Readback(s_pipeline* pipeline, uint32_t* pixels, int32_t stride) {
        if (pipeline == nullptr || pipeline->implementation == nullptr || pixels == nullptr) return false;
        auto* state = static_cast<s_pipeline_state*>(pipeline->implementation);
        const int32_t width = state->width * state->factor, height = state->height * state->factor;
        if (state->output == nullptr || stride < width) return false;
        SDL_GPUTransferBufferCreateInfo info = {};
        info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
        info.size = width * height * sizeof(uint32_t);
        SDL_GPUTransferBuffer* transfer = SDL_CreateGPUTransferBuffer(state->device, &info);
        if (transfer == nullptr) return false;
        SDL_GPUCommandBuffer* commands = SDL_AcquireGPUCommandBuffer(state->device);
        if (commands == nullptr) {
            SDL_ReleaseGPUTransferBuffer(state->device, transfer);
            return false;
        }
        SDL_GPUCopyPass* pass = SDL_BeginGPUCopyPass(commands);
        SDL_GPUTextureRegion source = {};
        source.texture = state->output;
        source.w = width;
        source.h = height;
        source.d = 1;
        SDL_GPUTextureTransferInfo destination = {};
        destination.transfer_buffer = transfer;
        destination.pixels_per_row = width;
        destination.rows_per_layer = height;
        SDL_DownloadFromGPUTexture(pass, &source, &destination);
        SDL_EndGPUCopyPass(pass);
        SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(commands);
        bool success = fence != nullptr && SDL_WaitForGPUFences(state->device, true, &fence, 1);
        const auto* mapped = success ? static_cast<const uint32_t*>(SDL_MapGPUTransferBuffer(state->device, transfer, false)) : nullptr;
        success = success && mapped != nullptr;
        if (mapped != nullptr) {
            for (int32_t y = 0; y < height; ++y)
                for (int32_t x = 0; x < width; ++x) {
                    const uint32_t rgba = mapped[y * width + x];
                    pixels[y * stride + x] = (rgba & 0xff00ff00u) | ((rgba & 0xffu) << 16) | ((rgba >> 16) & 0xffu);
                }
            SDL_UnmapGPUTransferBuffer(state->device, transfer);
        }
        if (fence != nullptr) SDL_ReleaseGPUFence(state->device, fence);
        SDL_ReleaseGPUTransferBuffer(state->device, transfer);
        return success;
    }

    void Shutdown(s_pipeline* pipeline) {
        if (pipeline == nullptr || pipeline->implementation == nullptr) return;
        auto* state = static_cast<s_pipeline_state*>(pipeline->implementation);
        SDL_WaitForGPUIdle(state->device);
        Destroy_Output_(state);
        if (state->kernel) SDL_ReleaseGPUComputePipeline(state->device, state->kernel);
        if (state->sampler) SDL_ReleaseGPUSampler(state->device, state->sampler);
        delete state;
        pipeline->implementation = nullptr;
    }
} // namespace vulkan_scalers
