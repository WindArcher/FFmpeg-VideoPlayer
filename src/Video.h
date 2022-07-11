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
#include "PacketQueue.h"
#include "PictureQueue.h"
#include <thread>
#include <SDL2/SDL.h>

class Video
{
public:
    static Video* getInstance();
    bool init( AVCodecContext* codecCtx, AVStream* stream );
    bool startVideoThread();
    bool refreshVideo();
    void setPause( bool pause ) { m_pause = pause; }
    void quit();
    PacketQueue m_videoQueue;
private:
    static Video* m_videoInstance;
    bool m_init = false, m_quitFlag = false,m_pause = false;
    double m_videoClock, m_frameLastPts = 0,m_frameDelay = (av_gettime() / 1000000.0), m_frameLastDelay = 40e-3;
    AVStream* m_videoStream;
    PictureQueue m_pictQ;
    AVCodecContext* m_videoContext;
    SwsContext* m_swsCtx;
    std::thread m_videoFrameThread;
    void videoThread();
    double syncVideo( AVFrame* frame, double pts );
    double guesPts( int64_t reoderedPts,int64_t dts );
};

