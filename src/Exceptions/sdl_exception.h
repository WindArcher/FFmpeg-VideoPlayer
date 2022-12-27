#pragma once
#include <stdexcept>
#include <string>
#include <SDL2/SDL.h>
class SDLException : public std::exception
{
public:
    SDLException( const std::string& msg ) 
    {
        m_msg = "SDL Exeption:\n";
        m_msg += msg;
        m_msg += SDL_GetError();
    }

    const char* what() const override
    {
        return m_msg.c_str();
    }

private:
    std::string m_msg;
};