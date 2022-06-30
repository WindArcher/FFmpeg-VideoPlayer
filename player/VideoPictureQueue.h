#pragma once
#include <SDL2/SDL.h>
#include <SDL2/SDL_thread.h>
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

#define VIDEO_PICTURE_QUEUE_SIZE 12
extern bool g_quitFlag;

struct VideoPicture
{
    AVFrame* frame;
    int         width;
    int         height;
    int         allocated;
    double      pts;
};
class VideoPictureQueue
{
public:
    VideoPictureQueue();
    ~VideoPictureQueue();
    int size() { return m_queueSize; }
    int addToQueue( AVCodecContext* videoCtx, AVFrame* frame, SwsContext* swsCtx, double pts );
    VideoPicture* getVideoPicture();
    void decreasePictQueueSize();
private:
    VideoPicture m_pictQueue[VIDEO_PICTURE_QUEUE_SIZE];
    int m_queueSize = 0;
    int m_rIndex = 0;
    int m_wIndex = 0;
    SDL_mutex* m_mutex = nullptr;
    SDL_cond* m_cond = nullptr;
    void allocatePict( AVCodecContext* videoCtx );
};

