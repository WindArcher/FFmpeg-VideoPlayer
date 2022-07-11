#pragma once
#include <SDL2/SDL_stdinc.h>
class Audio;
class AudioCallback
{
public:
    ~AudioCallback() = default;
    static void setAudioInstance( Audio* audioInstance );
    static void audioCallback( void* codecContext, Uint8* buffer, int bufferSize );
private:
    AudioCallback() = default;
    static Audio* m_audioInstance;
};

