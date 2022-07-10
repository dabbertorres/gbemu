#include <iostream>
#include <fstream>
#include <filesystem>
#include <thread>
#include <system_error>

#include <SDL2/SDL.h>
#include <SDL2/SDL_render.h>
#include <SDL2/SDL_video.h>
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_log.h>
#include <SDL2/SDL_error.h>

#include "rom.hpp"
#include "cpu.hpp"
#include "memory.hpp"

namespace fs = std::filesystem;

gb::rom load_rom(fs::path path);

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "must specify a rom file to execute" << std::endl;
        return 1;
    }

    const auto rom_file = fs::path(argv[1]);
    const auto rom = load_rom(rom_file);

    auto mem = std::make_unique<gb::memory>();
    mem->load_rom(rom);

    auto res = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS);
    if (res != 0)
    {
        SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "init failure: %s", SDL_GetError());
        std::exit(1);
    }

    auto* window = SDL_CreateWindow("gbemu",
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            800, 600,
            SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL);
    if (window == nullptr)
    {
        SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "failure to create window: %s", SDL_GetError());
        std::exit(1);
    }

    auto* renderer = SDL_CreateRenderer(window, -1,
            SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (renderer == nullptr)
    {
        SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "failure to create renderer: %s", SDL_GetError());
        std::exit(1);
    }

    {
        SDL_RendererInfo info;
        SDL_GetRendererInfo(renderer, &info);
        SDL_Log("renderer: %s", info.name);

        if ((info.flags & SDL_RENDERER_SOFTWARE) != 0)
            SDL_Log("renderer type: software");
        if ((info.flags & SDL_RENDERER_ACCELERATED) != 0)
            SDL_Log("renderer type: accelerated");
    }

    {
        auto cpu = gb::cpu{std::move(mem)};
        std::jthread cpu_thread{&gb::cpu::run, &cpu};

        bool run = true;
        while (run)
        {
            SDL_Event event;
            while(SDL_PollEvent(&event))
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

    if (window != nullptr)
        SDL_DestroyWindow(window);

    if (renderer != nullptr)
        SDL_DestroyRenderer(renderer);

    SDL_Quit();

    return 0;
}

gb::rom load_rom(fs::path path)
{
    std::error_code err;
    auto sts = fs::status(path, err);
    if (err)
    {
        std::cerr << path.filename() << " is inaccessible: " << err.message() << std::endl;
        std::exit(1);
    }

    if (sts.type() != fs::file_type::regular)
    {
        std::cerr << path.filename() << " is not a file" << std::endl;
        std::exit(1);
    }

    err.clear();
    auto size = fs::file_size(path, err);
    if (err)
    {
        std::cerr << path.filename() << " is inaccessible: " << err.message() << std::endl;
        std::exit(1);
    }

    gb::rom rom{size};
    std::ifstream fin{path};
    fin.read(reinterpret_cast<char*>(rom.data.data()), size);
    if (static_cast<decltype(size)>(fin.gcount()) != size)
    {
        std::cerr << "failed to read " << path.filename() << std::endl;
        std::exit(1);
    }

    return rom;
}
