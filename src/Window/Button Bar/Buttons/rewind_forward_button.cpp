#include "rewind_forward_button.h"
#include "Window/Events/events.h"
#include <stdio.h>
namespace Window
{
    namespace Button
    {
        RewindForwardButton::RewindForwardButton( SDL_Texture* texture, const SDL_Rect& textRect, const SDL_Rect& drawRect ) : SDLButton( texture, textRect, drawRect )
        {

        }

        void RewindForwardButton::clicked()
        {
            printf( "Rewind forward clicked\n" );
            SDL_Event event;
            event.type = static_cast<Uint32>( Player::Events::Events::REWIND_FORWARD_BUTTON_PUSHED );
            SDL_PushEvent( &event );
        }
    };
};
