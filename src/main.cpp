#include "Player.h"
#include <iostream>
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
    catch( std::exception& e )
    {
        std::cerr << e.what();
    }
}