#include "play_pause_button.h"
#include "Window/Events/events.h"
#include <stdio.h>
namespace Window
{
    namespace Button
    {
        PlayPauseButton::PlayPauseButton( SDL_Texture* texture, const SDL_Rect& playRect, const SDL_Rect& pauseRect, const SDL_Rect& drawRect, bool& mode ) : 
            SDLButton( texture, {0,0,0,0}, drawRect ), m_pauseMode( mode )
        {
            m_pauseRect = pauseRect;
            m_playRect = playRect;
        }

        void PlayPauseButton::clicked()
        {
            SDL_Event event;
            if( m_pauseMode )
            {
                printf( "Pause button clicked!\n" );
                event.type = static_cast<Uint32>(Player::Events::Events::PAUSE_BUTTON_PUSHED);
                SDL_PushEvent( &event );
                m_pauseMode = !m_pauseMode;
            }
            else
            {
                event.type = static_cast<Uint32>(Player::Events::Events::PLAY_BUTTON_PUSHED);
                SDL_PushEvent( &event );
                printf( "Play button clicked!\n" );
                m_pauseMode = !m_pauseMode;
            }
            SDL_SetRenderDrawColor( m_renderer, 255, 255, 255, 255 );
            SDL_RenderSetViewport( m_renderer, &m_viewPort );
            draw( m_renderer );
            SDL_RenderPresent( m_renderer );
        }

        void PlayPauseButton::draw( SDL_Renderer* renderer )
        {
            if( renderer != m_renderer )
            {
                m_renderer = renderer;
                SDL_RenderGetViewport( renderer, &m_viewPort );
            }
            if( !m_hidden )
                SDL_RenderCopy( renderer, m_texture, (m_pauseMode) ? &m_pauseRect : &m_playRect, &m_drawRect);
            
        }
    };
};
