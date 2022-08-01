#pragma once
#include <string>
namespace Player
{
    class IPlayer
    {
    public:
        virtual void stop() = 0;
        virtual void play() = 0;
        virtual void pause() = 0;
        virtual bool open( const std::string& ) = 0;
        virtual void rewindRelative( int time ) = 0;
        virtual void rewindProgress( int progress ) = 0;
    };
}