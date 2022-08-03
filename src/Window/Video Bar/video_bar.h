#pragma once
#include <SDL2/SDL.h>
#include "Window/Events/events.h"
#include "file_reader.h"
#include "Window/Rewind Bar/rewind_bar.h"
namespace Window
{
    Uint32 refresh( Uint32 interval, void* videoBar );
    class VideoBar
    {
    public:
        VideoBar( SDL_Renderer* renderer, SDL_Rect& rect, Player::FileReader* reader );
        ~VideoBar();
        void draw();
        void updateVideoBar();
        void killRefreshTimer();
        void sheduleRefresh( int delay );
        void setTextureSize( int w, int h );
        SDL_Texture* getTexture();
    private:
        Player::FileReader* m_fileReader;
        SDL_Renderer* m_renderer;
        SDL_Rect& m_drawRect;
        SDL_TimerID m_refreshTimer;
        SDL_Texture* m_texture = nullptr;
        friend Uint32 refresh( Uint32 interval, void* videoBar );
    };
}
