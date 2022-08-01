#include "stop_button.h"
#include "Window/Events/events.h"
namespace Window
{
    namespace Button
    {
        StopButton::StopButton( SDL_Texture* texture, const SDL_Rect& textRect, const SDL_Rect& drawRect ) : SDLButton( texture, textRect, drawRect )
        {

        }

        void StopButton::clicked()
        {
            printf( "Stop button clicked\n" );
            SDL_Event event;
            event.type = static_cast<Uint32>( Player::Events::Events::STOP_BUTTON_PUSHED );
            SDL_PushEvent( &event );
        }
    };
};
