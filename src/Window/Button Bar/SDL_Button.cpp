#include "SDL_Button.h"
namespace Window
{
    SDLButton::SDLButton( SDL_Texture* texture, const SDL_Rect& textRect, const SDL_Rect& drawRect ) : m_texture(texture), m_textRect( textRect ), m_drawRect( drawRect )
    {

    }

    void SDLButton::draw( SDL_Renderer* renderer )
    {
        if( !m_hidden )
            SDL_RenderCopy( renderer, m_texture, &m_textRect, &m_drawRect );
    }

    void SDLButton::update( const SDL_Point& point )
    {
        m_selected = SDL_PointInRect( &point, &m_drawRect );
    }

    bool SDLButton::selected()
    {
        return m_selected;
    }

    void SDLButton::hide()
    {
        m_hidden = true;
    }

    void SDLButton::show()
    {
        m_hidden = false;
    }
};
