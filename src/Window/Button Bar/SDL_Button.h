#pragma once
#include <SDL2/SDL.h>
namespace Window
{
    class SDLButton
    {
    public:
        SDLButton( SDL_Texture* texture, const SDL_Rect& textRect, const SDL_Rect& drawRect );
        virtual ~SDLButton() = default;
        virtual void draw( SDL_Renderer* rend );
        void update( const SDL_Point& point );
        bool selected();
        void hide();
        void show();
        virtual void clicked() = 0;
    protected:
        SDL_Texture* m_texture;
        const SDL_Rect m_textRect, m_drawRect;
        bool m_hidden = false, m_selected = false;
    };
};
