#pragma once
extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/avstring.h>
#include <libavutil/time.h>
#include <libavutil/opt.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}
#include <SDL2/SDL.h>
#include "PacketQueue.h"
#define SDL_AUDIO_BUFFER_SIZE 1024
#define MAX_AUDIO_FRAME_SIZE 192000
void AudioCallback( void* userdata, Uint8* stream, int len );
class Audio
{
public:
    static Audio* getInstance();
    ~Audio();
    void init( AVCodecContext* codecContext, AVStream* stream  );
    void startPlaying();
    double getAudioClock();
    //Queue
    PacketQueue m_audioQueue;
private:

    bool m_init = false;
    friend void AudioCallback( void* codecContext, Uint8* buffer, int bufferSize );
    static Audio* m_instance;

    SDL_AudioSpec m_wanted_specs;
    SDL_AudioSpec m_specs;
    //decoded frame buffer
    uint8_t audioBuf[(MAX_AUDIO_FRAME_SIZE * 3) / 2];
    int audioBufSize = 0;
    int audioBufIndex = 0;

    AVCodecContext* m_audioContext;
    //Sync data
    AVStream* m_audioStream = nullptr;
    double m_audioClock{ 0.0 };

    //Audio device???
    SDL_AudioDeviceID m_dev;
    int decodeFrame( AVCodecContext* codecContext, uint8_t* audioBuffer, int audioBufferSize);
    int resampleAudio( AVCodecContext* codecContext, AVFrame* decodedFrame, AVSampleFormat outFormat, uint8_t* outBuffer );
};

