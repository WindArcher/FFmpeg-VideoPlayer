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
    static void openAudio( SDL_AudioSpec* desired, SDL_AudioSpec* obtained );
private:
    SDLWrapper() = default;
};

