#include <SDL3/SDL.h>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include "game/scalers/scaler.h"

// Compare the production scaler implementations on one lossless logical frame.
int main(int argc, char** argv) {
    const bool single_pass = argc == 4 && std::strcmp(argv[3], "--single-pass") == 0;
    if (argc != 3 && !single_pass) {
        std::fprintf(stderr, "Usage: scaler-compare INPUT.bmp OUTPUT_DIRECTORY [--single-pass]\n");
        return 1;
    }
    SDL_Surface* loaded = SDL_LoadBMP(argv[1]);
    SDL_Surface* input = loaded != nullptr
        ? SDL_ConvertSurface(loaded, SDL_PIXELFORMAT_ARGB8888)
        : nullptr;
    SDL_DestroySurface(loaded);
    if (input == nullptr || input->w > scalers::MAX_SOURCE_WIDTH ||
        input->h > scalers::MAX_SOURCE_HEIGHT) {
        std::fprintf(stderr, "Expected a BMP no larger than 640x480: %s\n", SDL_GetError());
        SDL_DestroySurface(input);
        return 1;
    }
    std::error_code error;
    std::filesystem::create_directories(argv[2], error);
    if (error) {
        std::fprintf(stderr, "Cannot create output directory: %s\n", error.message().c_str());
        SDL_DestroySurface(input);
        return 1;
    }
    const scalers::e_scaler modes[] = {
        scalers::SCALER_PIXELART,
        scalers::SCALER_BILINEAR,
        scalers::SCALER_HQ2X,
        scalers::SCALER_HQ3X,
        scalers::SCALER_HQ4X,
        scalers::SCALER_MMPX2,
        scalers::SCALER_MMPX4,
    };
    std::puts("scaler,factor,full_frame_ms,idle_frame_ms,output_mib");
    bool success = true;
    for (const auto mode : modes) {
        scalers::s_pipeline pipeline = {};
        if (!scalers::Initialize(&pipeline, input->w, input->h) ||
            !scalers::Configure(&pipeline, mode)) {
            scalers::Shutdown(&pipeline);
            success = false;
            break;
        }
        const auto pixels = static_cast<const uint32_t*>(input->pixels);
        const int stride = input->pitch / sizeof(uint32_t);
        double times[2] = {};
        const int warmups = single_pass ? 0 : 5;
        const int iterations = single_pass ? 1 : 30;
        for (int pass = 0; pass < 2 && success; ++pass) {
            for (int iteration = 0; iteration < warmups + iterations; ++iteration) {
                if (pass == 0) scalers::Invalidate(&pipeline);
                const auto start = std::chrono::steady_clock::now();
                if (!scalers::Process(&pipeline, pixels, stride)) {
                    success = false;
                    break;
                }
                if (iteration >= warmups) {
                    times[pass] += std::chrono::duration<double, std::milli>(
                                       std::chrono::steady_clock::now() - start)
                                       .count() /
                        iterations;
                }
            }
        }
        if (success) {
            SDL_Surface* output = SDL_CreateSurfaceFrom(
                scalers::Get_Output_Width(&pipeline), scalers::Get_Output_Height(&pipeline), SDL_PIXELFORMAT_ARGB8888, const_cast<uint32_t*>(scalers::Get_Output_Pixels(&pipeline)), scalers::Get_Output_Stride(&pipeline) * sizeof(uint32_t));
            const auto path = std::filesystem::path(argv[2]) /
                (std::string(scalers::Get_Name(mode)) + ".bmp");
            success = output != nullptr && SDL_SaveBMP(output, path.string().c_str());
            SDL_DestroySurface(output);
            std::printf("%s,%d,%.6f,%.6f,%.3f\n", scalers::Get_Name(mode), scalers::Get_Factor(mode), times[0], times[1], scalers::Get_Output_Width(&pipeline) * (double)scalers::Get_Output_Height(&pipeline) * 4 / (1024 * 1024));
        }
        scalers::Shutdown(&pipeline);
        if (!success) break;
    }
    SDL_DestroySurface(input);
    return success ? 0 : 1;
}
