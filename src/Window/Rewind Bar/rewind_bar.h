#pragma once
#include <SDL2/SDL.h>
#include <mutex>
#include "file_reader.h"
namespace Window
{
    class RewindBar
    {
    public:
        RewindBar( SDL_Renderer* rederer, SDL_Rect& rect, Player::FileReader* reader );
        ~RewindBar();
        void enableTimer();
        void disableTimer();
        void draw();
        void update( SDL_Point point );
        void updateProgres( int progress );
        int getProgress() { return m_progress; };
    private:
        std::mutex m_mutex;
        Player::FileReader* m_fileReader;
        SDL_Renderer* m_renderer;
        SDL_Rect& m_drawRect;
        SDL_Point startPoint;
        int m_progress = 0, m_maxLineLenght = 0;
        SDL_Texture* m_texture;
        SDL_TimerID m_timer;
        static Uint32 updateBar( Uint32 delay, void* rewindBar );
    };
};
