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
#include <deque>
#include <memory>
#include <mutex>
#include <condition_variable>
struct AVFrameDeleter
{
    void operator()( AVFrame* pkt ) { av_frame_free( &pkt ); }
};
using AVFramePtr = std::unique_ptr<AVFrame, AVFrameDeleter>;
struct VideoPicture
{
    AVFramePtr frame{ av_frame_alloc() };
    double pts = 0;
};
class PictureQueue
{
public:
    void putFrame( AVFrame* frame,SwsContext* swsCtx, int width, int height, double pts );
    VideoPicture getPicture( bool blocking );
    int size() { return m_pictQueue.size(); }
    void clear();
    void pop();
private:
    int m_totalSize{0};
    std::deque<VideoPicture> m_pictQueue;
    std::mutex m_mutex;
    std::condition_variable m_cond;
};