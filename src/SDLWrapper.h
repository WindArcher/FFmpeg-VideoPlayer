#pragma once
#include <SDL2/SDL.h>
#include <SDL2/SDL_thread.h>
#include <SDL2/SDL_syswm.h>
#include <SDL2/SDL_render.h>
#include <SDL2/SDL_audio.h>
class SDLWrapper
{
public:
    static void initSdl();
    static bool init() { return m_init; }
private:
    SDLWrapper() = default;
    static bool m_init;
};

