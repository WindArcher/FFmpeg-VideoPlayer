#pragma once
#include <SDL2/SDL.h>
#include <SDL2/SDL_thread.h>
class Screen
{
public:
    Screen()
    {
        m_screenMutex = SDL_CreateMutex();
    }
    ~Screen()
    {
        SDL_DestroyMutex( m_screenMutex );
    }
    void lock() { SDL_LockMutex( m_screenMutex ); }
    void unlock() { SDL_UnlockMutex( m_screenMutex ); }
    SDL_Window* m_screen;
private:
    SDL_mutex* m_screenMutex;
};

