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
#include <thread>
#include <functional>
#include <SDL2/SDL.h>
#include <mutex>
#include <condition_variable>

#include "Queues/packet_queue.h"
#include "Queues/picture_queue.h"
namespace Player
{
    namespace Video
    {
        class Video
        {
        public:
            Video( AVCodecContext* codecCtx, AVFormatContext* formatCtx, int streamNum );
            void startVideoThread();
            void stopVideoThread();
            void pauseVideoThread();
            void resumeVideoThread();
            void notifyVideoThread();
            bool videoThreadStatus() { return m_videoThreadActive; }
            void resetFrameDelay();
            double getRealDelay( const VideoPicture& pict, double audioClock );
            PictureQueue m_pictQ;
            PacketQueue m_videoQueue;
        private:
            bool m_quitFlag = false, m_videoThreadActive = false, m_videoThreadPause = false;
            double m_videoClock{ 0.0 }, m_frameLastPts{ 0.0 }, m_frameDelay = (av_gettime() / 1000000.0), m_frameLastDelay = 40e-3;
            AVStream* m_videoStream;
            AVCodecContext* m_videoContext;
            AVFrame* m_frame;
            SwsContext* m_swsCtx;
            std::thread m_videoFrameThread, m_decodeThread;
            std::mutex m_mutex;
            std::condition_variable m_cond;
            void videoThread();
            double syncVideo( AVFrame* frame, double pts );
            double guesPts( int64_t reoderedPts, int64_t dts );
        };
    }
}