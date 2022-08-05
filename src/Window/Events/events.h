#pragma once
#include <SDL2/SDL.h>

namespace Player
{
    namespace Events
    {
        constexpr auto g_eventNum = 6;

        void registerEvents();

        enum class Events : Uint32
        {
            PAUSE_BUTTON_PUSHED = SDL_USEREVENT,
            PLAY_BUTTON_PUSHED,
            STOP_BUTTON_PUSHED,
            REWIND_FORWARD_BUTTON_PUSHED,
            REWIND_BACKWARD_BUTTON_PUSHED,
            REWIND_BAR_POINT_CHANGED
        };
    };
};
