#pragma once
#include <SDL3/SDL.h>
#include "game/scalers/scaler.h"

namespace vulkan_scalers {
    struct s_pipeline {
        void* implementation;
    };

    SDL_GPUDevice* Create_Device();
    bool Initialize(s_pipeline* pipeline, SDL_GPUDevice* device);
    bool Configure(s_pipeline* pipeline, SDL_Renderer* renderer, int32_t width, int32_t height, scalers::e_scaler scaler);
    bool Process(s_pipeline* pipeline, const uint32_t* pixels, int32_t stride);
    // Palette colors are ARGB8888. A null cursor disables cursor composition;
    // otherwise it is a tightly packed 24x24 indexed image with color key zero.
    bool Process_Indexed(s_pipeline* pipeline, const uint8_t* pixels, int32_t stride, const uint32_t* palette, const uint8_t* cursor, int32_t cursor_x, int32_t cursor_y);
    SDL_Texture* Get_Texture(s_pipeline* pipeline);
    bool Readback(s_pipeline* pipeline, uint32_t* pixels, int32_t stride);
    void Shutdown(s_pipeline* pipeline);
} // namespace vulkan_scalers
