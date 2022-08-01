#include "rewind_back_button.h"
#include "Window/Events/events.h"
#include <stdio.h>
namespace Window
{
    namespace Button
    {
        RewindBackButton::RewindBackButton( SDL_Texture* texture, const SDL_Rect& textRect, const SDL_Rect& drawRect ) : SDLButton( texture, textRect, drawRect )
        {

        }

        void RewindBackButton::clicked()
        {
            printf( "Rewind back clicked\n" );
            SDL_Event event;
            event.type = static_cast<Uint32>( Player::Events::Events::REWIND_BACKWARD_BUTTON_PUSHED );
            SDL_PushEvent( &event );
        }
    };
};
