#pragma once
#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <vector>
#include <memory>
#include "Button Bar/button_bar.h"
#include "Rewind Bar/rewind_bar.h"
#include "Video Bar/video_bar.h"
#include "file_reader.h"
#include "Events/events.h"
namespace Window
{
    class Window
    {
    public:
        Window();
        ~Window();
        void display();
        void openVideo( const std::string& filename );
        void SDLLoop();
    private:
        SDL_Window* m_window;
        SDL_Renderer* m_renderer;
        std::unique_ptr<ButtonBar> m_buttonBar;
        int m_width = 1024, m_height = 800;
        SDL_Rect m_buttonBarRect, m_rewindRect, m_videoBarRect;
        std::unique_ptr<RewindBar> m_rewindBar;
        std::unique_ptr<VideoBar> m_videoBar;
        Player::FileReader m_fileReader;
        bool m_pause = false;
        void countRects();
        void resizeWindow( int w, int h );
    };
}
