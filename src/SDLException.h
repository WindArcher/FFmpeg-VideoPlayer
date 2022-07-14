#pragma once
#include <stdexcept>
#include <SDL2/SDL.h>
class SDLException : public std::exception
{
public:
    SDLException( const char* msg ) : m_msg( msg ) {}
    const char* what()
    {
        char ret[512];
        snprintf( ret, sizeof ret, "%s%s%s","SDL Exeption:\n", m_msg, SDL_GetError());
        return ret;
    }
private:
    const char* m_msg;
};