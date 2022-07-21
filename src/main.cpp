#include "Player.h"
#include <iostream>
#include "sdl_exception.h"
#include "ffmpeg_exception.h"
int main( int argc, char* argv[] )
{
    if( argc < 2 )
    {
        std::cout << "Enter video file path as an argument.\n";
        return -1;
    }
    try
    {
        Player player;
        player.run( argv[1] );
    }
    catch( SDLException& e )
    {
        std::cerr << e.what();
    }
    catch( FFmpegException& e )
    {
        std::cerr << e.what();
    }
    catch( std::exception& e )
    {
        std::cerr << e.what();
    }
}