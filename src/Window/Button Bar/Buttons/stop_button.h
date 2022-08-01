#pragma once
#include "Window/Button Bar/SDL_Button.h"
namespace Window
{
    namespace Button
    {
        class StopButton : public SDLButton
        {
        public:
            StopButton( SDL_Texture* texture, const SDL_Rect& textRect, const SDL_Rect& drawRect );
            void clicked() override;
        };
    };
};
