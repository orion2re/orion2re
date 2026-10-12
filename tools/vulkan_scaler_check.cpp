#include "game/scalers/vulkan_scaler.h"
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <vector>

namespace {
    bool Check_Frame_(SDL_GPUDevice* device, vulkan_scalers::s_pipeline* gpu, const uint32_t* pixels, int32_t width, int32_t height, int32_t stride, const char* label, const char* output_directory) {
        const scalers::e_scaler modes[] = {
            scalers::SCALER_PIXELART,
            scalers::SCALER_BILINEAR,
            scalers::SCALER_HQ2X,
            scalers::SCALER_HQ3X,
            scalers::SCALER_HQ4X,
            scalers::SCALER_MMPX2,
            scalers::SCALER_MMPX4,
        };
        for (auto mode : modes) {
            scalers::s_pipeline cpu = {};
            const int factor = scalers::Get_Factor(mode);
            const int out_width = width * factor, out_height = height * factor;
            std::vector<uint32_t> result(out_width * out_height);
            bool valid = scalers::Initialize(&cpu, width, height) &&
                scalers::Configure(&cpu, mode) && scalers::Process(&cpu, pixels, stride) &&
                vulkan_scalers::Configure(gpu, nullptr, width, height, mode) &&
                vulkan_scalers::Process(gpu, pixels, stride) &&
                vulkan_scalers::Readback(gpu, result.data(), out_width);
            int mismatches = 0;
            if (valid) {
                const uint32_t* expected = scalers::Get_Output_Pixels(&cpu);
                const int expected_stride = scalers::Get_Output_Stride(&cpu);
                for (int y = 0; y < out_height; ++y)
                    for (int x = 0; x < out_width; ++x) {
                        if (result[y * out_width + x] != expected[y * expected_stride + x]) {
                            if (mismatches == 0) std::fprintf(stderr, "%s %s first mismatch (%d,%d): GPU=%08x CPU=%08x\n", label, scalers::Get_Name(mode), x, y, result[y * out_width + x], expected[y * expected_stride + x]);
                            ++mismatches;
                        }
                    }
                valid = mismatches == 0;
            }
            if (valid && output_directory != nullptr) {
                const auto path = std::filesystem::path(output_directory) / (std::string(scalers::Get_Name(mode)) + ".bmp");
                SDL_Surface* surface = SDL_CreateSurfaceFrom(out_width, out_height, SDL_PIXELFORMAT_ARGB8888, result.data(), out_width * sizeof(uint32_t));
                valid = surface != nullptr && SDL_SaveBMP(surface, path.string().c_str());
                SDL_DestroySurface(surface);
            }
            double elapsed = 0;
            if (valid) {
                for (int iteration = 0; iteration < 15; ++iteration) {
                    const auto start = std::chrono::steady_clock::now();
                    valid = vulkan_scalers::Process(gpu, pixels, stride) && SDL_WaitForGPUIdle(device);
                    if (!valid) break;
                    if (iteration >= 5) elapsed += std::chrono::duration<double, std::milli>(
                                                       std::chrono::steady_clock::now() - start)
                                                       .count() /
                        10;
                }
            }
            std::printf("%s,%s,%s,%d,%.4f\n", label, scalers::Get_Name(mode), valid ? "PASS" : "FAIL", mismatches, elapsed);
            scalers::Shutdown(&cpu);
            if (!valid) {
                std::fprintf(stderr, "Scaler check failed: %s\n", SDL_GetError());
                return false;
            }
        }
        return true;
    }
} // namespace

int main(int argc, char** argv) {
    if (argc != 1 && argc != 3) {
        std::fprintf(stderr, "Usage: vulkan-scaler-check [INPUT.bmp OUTPUT_DIRECTORY]\n");
        return 1;
    }
    if (!SDL_Init(SDL_INIT_VIDEO)) return 1;
    SDL_GPUDevice* device = vulkan_scalers::Create_Device();
    vulkan_scalers::s_pipeline gpu = {};
    bool success = device != nullptr && vulkan_scalers::Initialize(&gpu, device);
    std::puts("fixture,scaler,status,mismatches,submission_and_gpu_wait_ms");
    if (success && argc == 3) {
        SDL_Surface* loaded = SDL_LoadBMP(argv[1]);
        SDL_Surface* source = loaded != nullptr ? SDL_ConvertSurface(loaded, SDL_PIXELFORMAT_ARGB8888) : nullptr;
        SDL_DestroySurface(loaded);
        std::error_code error;
        std::filesystem::create_directories(argv[2], error);
        success = source != nullptr && !error && Check_Frame_(device, &gpu, static_cast<const uint32_t*>(source->pixels), source->w, source->h, source->pitch / sizeof(uint32_t), "capture", argv[2]);
        SDL_DestroySurface(source);
    } else if (success) {
        const int sizes[][2] = {{1, 1}, {2, 2}, {31, 23}, {64, 48}};
        for (const auto& size : sizes) {
            const int width = size[0], height = size[1], stride = width + 3;
            std::vector<uint32_t> pixels(stride * height, 0xdeadbeefu);
            uint32_t random = 17;
            const uint32_t palette[] = {0xff000000u, 0xffffffffu, 0xff2a70c0u, 0xff107021u, 0x00000000u, 0x80345678u, 0xff808080u, 0xff817f7fu};
            for (int y = 0; y < height; ++y)
                for (int x = 0; x < width; ++x) {
                    random = random * 1664525u + 1013904223u;
                    pixels[y * stride + x] = palette[((x / 3 + y / 2) ^ (random >> 29)) & 7];
                }
            char label[64];
            SDL_snprintf(label, sizeof(label), "%dx%d-padded-alpha", width, height);
            success = Check_Frame_(device, &gpu, pixels.data(), width, height, stride, label, nullptr);
            if (!success) break;
        }
        if (success) {
            success = !vulkan_scalers::Configure(&gpu, nullptr, 641, 480, scalers::SCALER_HQ2X) &&
                !vulkan_scalers::Configure(&gpu, nullptr, 640, 480, static_cast<scalers::e_scaler>(255)) &&
                !vulkan_scalers::Process(&gpu, nullptr, 640);
        }
    }
    if (!success) std::fprintf(stderr, "Vulkan scaler validation failed: %s\n", SDL_GetError());
    vulkan_scalers::Shutdown(&gpu);
    if (device != nullptr) SDL_DestroyGPUDevice(device);
    SDL_Quit();
    return success ? 0 : 1;
}
