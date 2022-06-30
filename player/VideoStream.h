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
#include <SDL2/SDL.h>
#include <SDL2/SDL_thread.h>
#include "AltPacketQueue.h"
#include "VideoPictureQueue.h"

extern bool g_quitFlag;
int videoThread( void* arg );
class VideoStream
{
public:
    VideoStream();
    bool findVideoStreamIndex( AVFormatContext* ctx );
    int getIndex() { return m_index; }
    bool openStream( AVFormatContext* formatCtx );
    int queueSize() { return m_videoQueue.getSize(); }
    bool addToQueue( AVPacket* pkt );
private:
    AltPacketQueue m_videoQueue;
    VideoPictureQueue m_pictQueue;
    int m_index = -1;
    AVStream* m_videoStream;
    AVCodecContext* m_videoContext;
    double m_frameDelay=0, m_frameLastDelay=0;
    friend int videoThread( void* arg );
    SDL_Thread* m_videoThread;
    double m_videoClock, m_frameLastPTS = 0;
    SwsContext* m_swsCtx;
    SDL_Texture* m_texture;
    SDL_Renderer* m_Renderer;
    static double synchronizeVideo( AVCodecContext* videoCtx, double& videoClock, AVFrame* src_frame, double pts );
    static double guessPts( AVCodecContext* ctx, int64_t reordered_pts, int64_t dts );
    friend class Player;
};

