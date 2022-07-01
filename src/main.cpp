#include <SDL2/SDL.h>
#include <SDL2/SDL_thread.h>
#include <iostream>
#include "Player.h"
#include "Screen.h"

#define FF_REFRESH_EVENT (SDL_USEREVENT)

#define FF_QUIT_EVENT (SDL_USEREVENT + 1)

extern bool g_quitFlag = false;
Screen g_screen;
int main( int argc, char* argv[] )
{
    Player player;
    player.setFileName( argv[1] );
    player.sheduleRefresh( 39 );
    if( !player.startDecodeThread() )
        return -1;
    SDL_Event event;
    for( ;;)
    {
        SDL_WaitEvent( &event );
        switch( event.type )
        {
        case FF_QUIT_EVENT:
        case SDL_QUIT:
        {
            g_quitFlag = true;
            SDL_Quit();
        }
        break;

        case FF_REFRESH_EVENT:
        {
            player.refreshVideo();
        }
        break;

        default:
        {
           
        }
        break;
        }
        if( g_quitFlag )
        {
            break;
        }
    }
    return 0;
}