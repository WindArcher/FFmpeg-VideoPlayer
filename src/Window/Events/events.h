#pragma once
#include <SDL2/SDL.h>

namespace Player
{
    namespace Events
    {
        constexpr auto g_eventNum = 7;

        void registerEvents();

        enum class Events : Uint32
        {
            REFRESH_VIDEO = SDL_USEREVENT,
            PAUSE_BUTTON_PUSHED,
            PLAY_BUTTON_PUSHED,
            STOP_BUTTON_PUSHED,
            REWIND_FORWARD_BUTTON_PUSHED,
            REWIND_BACKWARD_BUTTON_PUSHED,
            REWIND_BAR_POINT_CHANGED
        };
    };
};
