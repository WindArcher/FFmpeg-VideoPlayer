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
#include <libavutil/time.h>
}
#include <SDL2/SDL.h>
#include <SDL2/SDL_thread.h>

#include <iostream>
#include "AudioStream.h"
#include "VideoStream.h"

#define FF_REFRESH_EVENT (SDL_USEREVENT)
#define FF_QUIT_EVENT (SDL_USEREVENT + 1)

#define AV_SYNC_THRESHOLD 0.01
#define AV_NOSYNC_THRESHOLD 1.0

extern bool g_quitFlag;

int decodeThread( void* player );
Uint32 sdlRefreshTimer( Uint32 interval, void* player );

class Player
{
public:
    Player();
    ~Player();
    void setFileName( const std::string& filename );
    bool startDecodeThread();
    void refreshVideo();
    void sheduleRefresh( int delay );
private:
    AVFormatContext* m_formatCtx;
    std::string m_fileName;
    VideoStream m_videoStream;
    AudioStream m_audioStream;
    SDL_Thread* m_decodeThread;
    int currentFrameIndex;
    friend int decodeThread( void* player );
    friend Uint32 sdlRefreshTimer( Uint32 interval, void* player );
    int fail();
    void videoDisplay();
};

