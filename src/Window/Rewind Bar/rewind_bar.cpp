#include "rewind_bar.h"

#include <SDL2/SDL_image.h>

#include <Windows.h>

#include "Window/Events/events.h"

namespace Window
{

    static std::string GetCurrentDirectory()
    {
        char buffer[MAX_PATH];
        GetModuleFileNameA( NULL, buffer, MAX_PATH );
        std::string::size_type pos = std::string( buffer ).find_last_of( "\\/" );

        return std::string( buffer ).substr( 0, pos );
    }

    RewindBar::RewindBar( SDL_Renderer* renderer, SDL_Rect& rect ) : m_drawRect( rect )
    {
        m_renderer = renderer;
        startPoint.x = m_drawRect.x + (m_drawRect.w / 10);
        startPoint.y = m_drawRect.h / 2;
        m_maxLineLenght = (m_drawRect.w * 8) / 10;
        WCHAR path[MAX_PATH];
        GetModuleFileNameW( NULL, path, MAX_PATH );
        m_texture = IMG_LoadTexture( m_renderer, std::string ( GetCurrentDirectory() + "\\circle.png" ).c_str() );
    }

    RewindBar::~RewindBar()
    {
        SDL_DestroyTexture( m_texture );
    }

    void RewindBar::draw()
    {
        SDL_RenderSetViewport( m_renderer, &m_drawRect );
        SDL_SetRenderDrawColor( m_renderer, 255, 255, 255, SDL_ALPHA_OPAQUE );
        SDL_RenderFillRect( m_renderer, NULL );
        SDL_SetRenderDrawColor( m_renderer, 0, 0, 0, SDL_ALPHA_OPAQUE );
        int w1 = (m_maxLineLenght * m_progress) / 100;
        SDL_RenderDrawLineF( m_renderer, startPoint.x, startPoint.y, startPoint.x + w1, startPoint.y );
        SDL_SetRenderDrawColor( m_renderer, 192, 192, 192, SDL_ALPHA_OPAQUE );
        int x1 = startPoint.x + (m_maxLineLenght * m_progress) / 100;
        int w2 = m_maxLineLenght - (m_maxLineLenght * m_progress) / 100;
        if( w2 != 0 )
            SDL_RenderDrawLineF( m_renderer, x1, startPoint.y, x1 + w2, startPoint.y );
        int radius = 8;
        SDL_Rect circle{ x1 - radius, startPoint.y - radius, radius * 2, radius * 2 };
        SDL_RenderCopy( m_renderer, m_texture, NULL, &circle );
        SDL_RenderPresent( m_renderer );
    }

    void RewindBar::update( SDL_Point point )
    {
        if( SDL_PointInRect( &point, &m_drawRect ) )
        {
            m_progress = ((point.x - startPoint.x) / (float)m_maxLineLenght) * 100;
            static int tempValue = 0;
            SDL_Event event;
            event.type = static_cast<Uint32>( Player::Events::Events::REWIND_BAR_POINT_CHANGED );
            tempValue = m_progress;
            event.user.data1 = &tempValue;
            SDL_PushEvent( &event );
            draw();
        }
    }

    void RewindBar::updateProgres( int progress )
    {
        if( progress >= 0 && progress <= 100 )
        {
            m_progress = progress;
            draw();
        }
    }
};
