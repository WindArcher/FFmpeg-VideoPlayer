#include "video_bar.h"
#include "Exceptions/sdl_exception.h"

namespace Window
{
    VideoBar::VideoBar( SDL_Renderer* renderer, SDL_Rect& rect ) : m_drawRect( rect )
    {
        m_drawRect = rect;
        m_renderer = renderer;
    }

    VideoBar::~VideoBar()
    {
        SDL_DestroyTexture( m_texture );
    }

    void VideoBar::updateVideoBar()
    {
        SDL_RenderSetViewport( m_renderer, &m_drawRect);
        SDL_SetRenderDrawColor( m_renderer, 0, 0, 0, 255 );
        SDL_RenderFillRect( m_renderer, NULL );
        SDL_RenderCopy( m_renderer, m_texture, NULL, NULL );
        SDL_RenderPresent( m_renderer );
    }

    void VideoBar::killRefreshTimer()
    {
        SDL_RemoveTimer( m_refreshTimer );
    }

    void VideoBar::draw()
    {
        SDL_RenderSetViewport( m_renderer, &m_drawRect );
        SDL_SetRenderDrawColor( m_renderer, 0, 0, 0, 255 );
        SDL_RenderFillRect( m_renderer, &m_drawRect );
        SDL_RenderPresent( m_renderer );
    }

    void VideoBar::sheduleRefresh( int delay )
    {
        printf( "\n\n\n--------\nDelay: %d\n------------\n\n", delay );
        m_refreshTimer = SDL_AddTimer( delay, refresh, this );
        if( !m_refreshTimer )
            throw SDLException( "Could not schedule refresh callback" );
    }

    void VideoBar::setTextureSize( int w, int h )
    {
        if( !m_texture )
            m_texture = SDL_CreateTexture( m_renderer, SDL_PIXELFORMAT_YV12, SDL_TEXTUREACCESS_STREAMING, w, h );
        else
        {
            SDL_DestroyTexture( m_texture );
            m_texture = SDL_CreateTexture( m_renderer, SDL_PIXELFORMAT_YV12, SDL_TEXTUREACCESS_STREAMING, w, h );
        }
    }

    SDL_Texture* VideoBar::getTexture()
    {
        return m_texture;
    }

    Uint32 VideoBar::refresh( Uint32 interval, void* )
    {
        SDL_Event event;
        event.type = static_cast<Uint32>( Player::Events::Events::REFRESH_VIDEO );
        SDL_PushEvent( &event );
        return 0;
    }
}