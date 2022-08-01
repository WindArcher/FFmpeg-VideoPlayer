#pragma once
#include <SDL2/SDL.h>
#include "Window/Events/events.h"
namespace Window
{
    class VideoBar
    {
    public:
        VideoBar( SDL_Renderer* renderer, SDL_Rect& rect );
        ~VideoBar();
        void draw();
        void updateVideoBar();
        void killRefreshTimer();
        void sheduleRefresh( int delay );
        void setTextureSize( int w, int h );
        SDL_Texture* getTexture();
    private:
        SDL_Renderer* m_renderer;
        SDL_Rect& m_drawRect;
        SDL_TimerID m_refreshTimer;
        SDL_Texture* m_texture = nullptr;
        static Uint32 refresh( Uint32 interval, void* );
    };
}
