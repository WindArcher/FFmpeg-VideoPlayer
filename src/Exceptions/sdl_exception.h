#pragma once
#include <stdexcept>
#include <SDL2/SDL.h>
class SDLException : public std::exception
{
public:
    SDLException( const char* msg ) : m_msg( msg ) {}
    const char* what()
    {
        snprintf( m_buffer, sizeof m_buffer, "%s%s%s","SDL Exeption:\n", m_msg, SDL_GetError());
        return m_buffer;
    }
private:
    const char* m_msg;
    char m_buffer[512];
};