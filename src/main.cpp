#include <cstdio>
#include <filesystem>
#include <iomanip>
#include <ios>
#include <iostream>
#include <system_error>
#include <thread>

#include <SDL2/SDL.h>
#include <SDL2/SDL_error.h>
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_log.h>
#include <SDL2/SDL_render.h>
#include <SDL2/SDL_video.h>

#include "cartridge.hpp"
#include "cpu.hpp"
#include "memory.hpp"

namespace fs = std::filesystem;

std::error_code load_cart(const fs::path& path, gb::cartridge& cart);

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        std::cerr << "must specify a rom file to execute" << std::endl;
        return 1;
    }

    const fs::path rom_file = fs::path(argv[1]);

    gb::cartridge cart;
    if (auto err = load_cart(rom_file, cart); err)
    {
        std::cerr << "unable to load " << std::quoted(rom_file.string()) << ": " << err.message() << std::endl;
        return 1;
    }

    int res = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS);
    if (res != 0)
    {
        SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "init failure: %s", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("gbemu",
                                          SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED,
                                          800,
                                          600,
                                          SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL);
    if (window == nullptr)
    {
        SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "failure to create window: %s", SDL_GetError());
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC);
    if (renderer == nullptr)
    {
        SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "failure to create renderer: %s", SDL_GetError());
        return 1;
    }

    {
        SDL_RendererInfo info;
        SDL_GetRendererInfo(renderer, &info);
        SDL_Log("renderer: %s; texture max size: %d x %d; num texture formats: %d",
                info.name,
                info.max_texture_width,
                info.max_texture_height,
                info.num_texture_formats);

        if ((info.flags & SDL_RENDERER_SOFTWARE) != 0) SDL_Log("renderer type: software");
        if ((info.flags & SDL_RENDERER_ACCELERATED) != 0) SDL_Log("renderer type: accelerated");
    }

    {
        // TODO determine memory_bank_controller to use
        auto controller = nullptr;

        auto         mem = std::make_unique<gb::memory>(controller, cart);
        gb::cpu      cpu = gb::cpu{std::move(mem), gb::model::original};
        std::jthread cpu_thread{&gb::cpu::run, &cpu};

        bool run = true;
        while (run)
        {
            SDL_Event event;
            while (SDL_PollEvent(&event) != 0)
            {
                switch (event.type)
                {
                case SDL_QUIT:
                    run = false;
                    cpu.stop();
                    break;
                }
            }

            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderClear(renderer);
            // TODO
            SDL_RenderPresent(renderer);
        }
    }

    if (window != nullptr) SDL_DestroyWindow(window);
    if (renderer != nullptr) SDL_DestroyRenderer(renderer);

    SDL_Quit();

    return 0;
}

std::error_code load_cart(const fs::path& path, gb::cartridge& cart)
{
    std::error_code err;
    fs::file_status sts = fs::status(path, err);
    if (err) return err;

    uintmax_t size = fs::file_size(path, err);
    if (err) return err;

    auto* file = std::fopen(path.c_str(), "rb");
    if (file == nullptr) return std::error_code{errno, std::iostream_category()};

    cart.data.reserve(size);
    size_t actual = std::fread(cart.data.data(), 1, size, file);
    if (actual != size) std::error_code(errno, std::iostream_category());

    return {};
}
