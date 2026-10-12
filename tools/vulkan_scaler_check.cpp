#include "game/scalers/vulkan_scaler.h"
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <vector>

namespace {
    bool Check_Presentation_(SDL_GPUDevice* device, vulkan_scalers::s_pipeline* gpu) {
        constexpr int width = 64, height = 48;
        SDL_Window* window = SDL_CreateWindow("Orion2re GPU presentation check", 640, 480, SDL_WINDOW_RESIZABLE);
        SDL_Renderer* renderer = window != nullptr ? SDL_CreateGPURenderer(device, window) : nullptr;
        bool valid = renderer != nullptr && SDL_SetRenderLogicalPresentation(renderer, width, height, SDL_LOGICAL_PRESENTATION_LETTERBOX);
        std::vector<uint8_t> indices(width * height);
        std::vector<uint32_t> pixels(width * height);
        uint32_t palette[256];
        uint8_t cursor[24 * 24];
        for (int i = 0; i < width * height; ++i) indices[i] = (uint8_t)(i * 17);
        for (int i = 0; i < 24 * 24; ++i) cursor[i] = i % 3 == 0 ? 0 : (uint8_t)(i * 7);
        const scalers::e_scaler modes[] = {scalers::SCALER_PIXELART, scalers::SCALER_BILINEAR, scalers::SCALER_HQ2X, scalers::SCALER_HQ3X, scalers::SCALER_HQ4X, scalers::SCALER_MMPX2, scalers::SCALER_MMPX4};
        for (auto mode : modes) {
            if (!valid) break;
            valid = vulkan_scalers::Configure(gpu, renderer, width, height, mode);
            for (int frame = 0; frame < 32 && valid; ++frame) {
                for (int i = 0; i < 256; ++i) palette[i] = 0xff000000u | ((uint32_t)((i + frame) & 255) << 16) | ((uint32_t)(i ^ 0x55) << 8) | i;
                const int cursor_x = frame * 3 % width - 4, cursor_y = frame * 2 % height - 3;
                for (int y = 0; y < height; ++y)
                    for (int x = 0; x < width; ++x) {
                        int index = indices[y * width + x];
                        const int cx = x - cursor_x, cy = y - cursor_y;
                        if (cx >= 0 && cy >= 0 && cx < 24 && cy < 24 && cursor[cy * 24 + cx] != 0) index = cursor[cy * 24 + cx];
                        pixels[y * width + x] = palette[index];
                    }
                if (frame == 8 || frame == 24) valid = SDL_SetWindowSize(window, frame == 8 ? 512 : 640, frame == 8 ? 384 : 480);
                SDL_PumpEvents();
                valid = valid && vulkan_scalers::Process_Indexed(gpu, indices.data(), width, palette, cursor, cursor_x, cursor_y) &&
                    SDL_RenderClear(renderer) && SDL_RenderTexture(renderer, vulkan_scalers::Get_Texture(gpu), nullptr, nullptr) &&
                    (frame == 31 || SDL_RenderPresent(renderer));
            }
            // The frame loop deliberately has no readback or GPU idle wait. Check
            // the last output only after exercising cycling with queued draws.
            const int factor = scalers::Get_Factor(mode);
            std::vector<uint32_t> result(width * height * factor * factor);
            scalers::s_pipeline cpu = {};
            valid = valid && scalers::Initialize(&cpu, width, height) && scalers::Configure(&cpu, mode) &&
                scalers::Process(&cpu, pixels.data(), width) && vulkan_scalers::Readback(gpu, result.data(), width * factor);
            if (valid) {
                const uint32_t* expected = scalers::Get_Output_Pixels(&cpu);
                const int stride = scalers::Get_Output_Stride(&cpu);
                for (int y = 0; y < height * factor; ++y)
                    for (int x = 0; x < width * factor; ++x) valid &= result[y * width * factor + x] == expected[y * stride + x];
                // Also check what the renderer sampled, not just the compute
                // output. The first texel is clamped under every display filter.
                SDL_FRect viewport;
                valid = valid && SDL_GetRenderLogicalPresentationRect(renderer, &viewport);
                const SDL_Rect corner = {valid ? (int)SDL_ceilf(viewport.x) : 0, valid ? (int)SDL_ceilf(viewport.y) : 0, 1, 1};
                SDL_Surface* sampled = valid ? SDL_RenderReadPixels(renderer, &corner) : nullptr;
                SDL_Surface* argb = sampled != nullptr ? SDL_ConvertSurface(sampled, SDL_PIXELFORMAT_ARGB8888) : nullptr;
                valid = valid && argb != nullptr && *(const uint32_t*)argb->pixels == expected[0];
                SDL_DestroySurface(argb);
                SDL_DestroySurface(sampled);
                valid = valid && SDL_RenderPresent(renderer);
            }
            scalers::Shutdown(&cpu);
            if (valid) std::fprintf(stderr, "Presentation cycling/resize parity passed: %s\n", scalers::Get_Name(mode));
        }
        // Destroy wrapped textures while the renderer is still alive.
        vulkan_scalers::Shutdown(gpu);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        return valid;
    }
    bool Check_Indexed_(vulkan_scalers::s_pipeline* gpu, int width, int height) {
        const int stride = width + 3;
        std::vector<uint8_t> indices(stride * height, 0xcd);
        std::vector<uint32_t> pixels(width * height);
        uint32_t palette[256];
        uint8_t cursor[24 * 24];
        for (int i = 0; i < 256; ++i) palette[i] = ((uint32_t)i << 24) | ((uint32_t)(255 - i) << 16) | ((uint32_t)(i ^ 0x55) << 8) | i;
        for (int i = 0; i < 24 * 24; ++i) cursor[i] = i % 3 == 0 ? 0 : (uint8_t)(i * 7);
        for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x) indices[y * stride + x] = (uint8_t)(x * 17 + y * 31);
        for (int mode = 0; mode < 7; ++mode) {
            const scalers::e_scaler modes[] = {scalers::SCALER_PIXELART, scalers::SCALER_BILINEAR, scalers::SCALER_HQ2X, scalers::SCALER_HQ3X, scalers::SCALER_HQ4X, scalers::SCALER_MMPX2, scalers::SCALER_MMPX4};
            const auto scaler = modes[mode];
            const int factor = scalers::Get_Factor(scaler);
            if (!vulkan_scalers::Configure(gpu, nullptr, width, height, scaler)) return false;
            for (int variation = 0; variation < 5; ++variation) {
                const int cursor_x = variation == 4 ? INT32_MIN : variation == 1 ? -3
                                                                                 : width - 2;
                const int cursor_y = variation == 4 ? INT32_MAX : variation == 1 ? -5
                                                                                 : height - 1;
                const uint8_t* active_cursor = variation == 0 || variation == 3 ? nullptr : cursor;
                // Palette-only changes must be visible on the next submission.
                palette[5] ^= 0x00ffffffu;
                for (int y = 0; y < height; ++y)
                    for (int x = 0; x < width; ++x) {
                        int index = indices[y * stride + x];
                        const int64_t cx = (int64_t)x - cursor_x, cy = (int64_t)y - cursor_y;
                        if (active_cursor != nullptr && cx >= 0 && cy >= 0 && cx < 24 && cy < 24 && cursor[cy * 24 + cx] != 0) index = cursor[cy * 24 + cx];
                        pixels[y * width + x] = palette[index];
                    }
                scalers::s_pipeline cpu = {};
                std::vector<uint32_t> result(width * height * factor * factor);
                bool valid = scalers::Initialize(&cpu, width, height) && scalers::Configure(&cpu, scaler) &&
                    scalers::Process(&cpu, pixels.data(), width) &&
                    vulkan_scalers::Process_Indexed(gpu, indices.data(), stride, palette, active_cursor, cursor_x, cursor_y) &&
                    vulkan_scalers::Readback(gpu, result.data(), width * factor);
                if (valid) {
                    const uint32_t* expected = scalers::Get_Output_Pixels(&cpu);
                    const int expected_stride = scalers::Get_Output_Stride(&cpu);
                    for (int y = 0; y < height * factor; ++y)
                        for (int x = 0; x < width * factor; ++x)
                            valid &= result[y * width * factor + x] == expected[y * expected_stride + x];
                }
                scalers::Shutdown(&cpu);
                if (!valid) {
                    std::fprintf(stderr, "Indexed parity failed: %dx%d %s variation %d (%s)\n", width, height, scalers::Get_Name(scaler), variation, SDL_GetError());
                    return false;
                }
            }
            if (vulkan_scalers::Process_Indexed(gpu, nullptr, stride, palette, nullptr, 0, 0) ||
                vulkan_scalers::Process_Indexed(gpu, indices.data(), width - 1, palette, nullptr, 0, 0) ||
                vulkan_scalers::Process_Indexed(gpu, indices.data(), stride, nullptr, nullptr, 0, 0)) return false;
        }
        std::fprintf(stderr, "Indexed palette/cursor parity passed: %dx%d, all seven scalers\n", width, height);
        return true;
    }
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
    const bool present = argc == 2 && SDL_strcmp(argv[1], "--present") == 0;
    if (argc != 1 && argc != 3 && !present) {
        std::fprintf(stderr, "Usage: vulkan-scaler-check [INPUT.bmp OUTPUT_DIRECTORY | --present]\n");
        return 1;
    }
    if (!SDL_Init(SDL_INIT_VIDEO)) return 1;
    SDL_GPUDevice* device = vulkan_scalers::Create_Device();
    vulkan_scalers::s_pipeline gpu = {};
    bool success = device != nullptr && vulkan_scalers::Initialize(&gpu, device);
    if (!present) std::puts("fixture,scaler,status,mismatches,submission_and_gpu_wait_ms");
    if (success && present) {
        success = Check_Presentation_(device, &gpu);
    } else if (success && argc == 3) {
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
            if (success) success = Check_Indexed_(&gpu, width, height);
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
