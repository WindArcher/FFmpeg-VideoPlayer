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
#include "DecodeThreadHandler.h"
#include <thread>
#include <functional>
#include <SDL2/SDL.h>

class Video
{
public:
    Video( DecodeThreadHandler* handler, AVCodecContext* codecCtx, AVFormatContext* formatCtx, int streamNum );
    void startVideoThread();
    void stopVideoThread();
    void resetFrameDelay();
    double getRealDelay( const VideoPicture& pict, double audioClock );
    void quit();
    bool videoThreadStatus() { return m_videoThreadFinished; }
    PictureQueue m_pictQ;
    PacketQueue m_videoQueue;
private:
    bool m_quitFlag = false, m_videoThreadFinished = false, m_init = false;
    double m_videoClock{ 0.0 }, m_frameLastPts{ 0.0 }, m_frameDelay = (av_gettime() / 1000000.0), m_frameLastDelay = 40e-3;
    DecodeThreadHandler* m_decodeHandler;
    AVStream* m_videoStream;
    AVCodecContext* m_videoContext;
    AVFrame* m_frame;
    SwsContext* m_swsCtx;
    std::thread m_videoFrameThread,m_decodeThread;
    void videoThread();

    double syncVideo( AVFrame* frame, double pts );
    double guesPts( int64_t reoderedPts,int64_t dts );
};

