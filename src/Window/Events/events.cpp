#include "events.h"
namespace Player
{
    namespace Events
    {
        void registerEvents()
        {
            SDL_RegisterEvents( g_eventNum );
        }
    };
};
