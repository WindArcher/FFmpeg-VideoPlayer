#pragma once
#include <SDL2/SDL.h>

#include <vector>
#include <memory>

#include "SDL_Button.h"
namespace Window
{
    class ButtonBar
    {
        using ButtonVector = std::vector<std::unique_ptr<SDLButton>>;
    public:
        ButtonBar( SDL_Renderer* renderer, SDL_Rect& rect );
        ~ButtonBar();
        void draw();
        void update( SDL_Point point );
        void clicked();
        void setPauseMode( bool mode );
    private:
        static constexpr auto m_filename = "C:\\Users\\Archer\\source\\repos\\SDLinterfacetest\\buttons_vector.png";
        SDL_Renderer* m_renderer;
        SDL_Rect& m_buttonBarRect;
        SDL_Texture* m_buttonsTexture;
        ButtonVector m_buttons;
        bool m_pauseMode = true;
    };
};
