#pragma once
extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavdevice/avdevice.h>
#include <libavfilter/avfilter.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/avutil.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
#include <libavutil/frame.h>
#include <libavutil/imgutils.h>
}
#include <array>
#include <vector>
#include <iostream>
#include "AltPacketQueue.h"
#define MAX_AUDIO_FRAME_SIZE 192000
#define SDL_AUDIO_BUFFER_SIZE 1024

extern bool g_quitFlag;
void AudioCallback( void* userdata, Uint8* stream, int len );
class AudioStream
{
public:
    AudioStream();
    ~AudioStream();
    bool openStream( AVFormatContext* formatCtx );
    int getIndex();
    bool findAudioStreamIndex( AVFormatContext* formatCtx );
    int queueSize() { return m_audioQueue.getSize(); }
    bool addToQueue( AVPacket* pkt );
    double getAudioClock();
private:
    int m_index = -1;
    AVPacket m_packet;
    AVStream* m_audioStream;
    AVFrame m_audioFrame;
    AVCodecContext* m_audioCtx;
    AltPacketQueue m_audioQueue;
    uint8_t m_audioBuffer[(MAX_AUDIO_FRAME_SIZE * 3) / 2];
    int m_audioBufferIndex = 0, m_audioBufferSize = 0;
    double m_audioClock;
    friend void AudioCallback( void* userdata, Uint8* stream, int len );
    friend unsigned int AudioDecodeFrame( AudioStream* videoState );
    friend int audioResample( AudioStream* audioStream, AVFrame* decodedFrame, AVSampleFormat outputFormat );
    friend class Player;
};

