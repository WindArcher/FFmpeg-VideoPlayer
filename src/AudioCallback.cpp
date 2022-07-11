#include "AudioCallback.h"
#include "Audio.h"
Audio* AudioCallback::m_audioInstance = 0;

void AudioCallback::setAudioInstance( Audio* audioInstance )
{
    m_audioInstance = audioInstance;
}

void AudioCallback::audioCallback( void* codecContext, Uint8* buffer, int bufferSize )
{
   
}