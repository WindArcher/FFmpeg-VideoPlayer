#include "picture_queue.h"
extern "C"
{
#include <libavutil/imgutils.h>
}

void PictureQueue::putFrame( AVFrame* frame, SwsContext* swsCtx, int width, int height, double pts )
{
    {
        std::unique_lock<std::mutex> lock{ m_mutex };
        m_pictQueue.push_back( {AVFramePtr{ av_frame_clone( frame ) }, pts } );
        int numBytes = av_image_get_buffer_size(
            AV_PIX_FMT_YUV420P,
            width,
            height,
            32
        );
        sws_scale( swsCtx,
            (uint8_t const* const*)frame->data,
            frame->linesize,
            0,
            height,
            m_pictQueue.back().frame.get()->data,
            m_pictQueue.back().frame.get()->linesize
        );
        m_totalSize += frame->pkt_size;
    }
    m_cond.notify_one();
}

VideoPicture PictureQueue::getPicture( bool blocking )
{
    std::unique_lock<std::mutex> lock{ m_mutex };
    while( true )
    {
        if( !m_pictQueue.empty() )
        {
            VideoPicture pict;
            m_totalSize = pict.frame.get()->pkt_size;
            av_frame_move_ref(pict.frame.get(), m_pictQueue.front().frame.get());
            pict.pts = m_pictQueue.front().pts;
            m_pictQueue.pop_front();
            return pict;
        }
        else if( !blocking && m_pictQueue.empty() )
            return VideoPicture();
        else
            m_cond.wait( lock );
    }
}

void PictureQueue::clear()
{
    std::unique_lock<std::mutex> lock{ m_mutex };
    m_pictQueue.clear();
}
