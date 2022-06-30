#include "VideoPictureQueue.h"
#include "Screen.h"
extern Screen g_screen;

VideoPictureQueue::VideoPictureQueue()
{
    m_mutex = SDL_CreateMutex();
    if( !m_mutex )
        printf( "Video Picture Queue mutex fail:%s\n", SDL_GetError() );
    m_cond = SDL_CreateCond();
    if(!m_cond )
        printf( "Video Picture Queue cond fail:%s\n", SDL_GetError() );
}

VideoPictureQueue::~VideoPictureQueue()
{
    SDL_DestroyMutex( m_mutex );
    SDL_DestroyCond( m_cond );
}

void VideoPictureQueue::allocatePict( AVCodecContext* videoCtx )
{
    VideoPicture* videoPicture = &m_pictQueue[m_wIndex];
    if( videoPicture->frame )
    {
        av_frame_free( &videoPicture->frame );
        av_free( videoPicture->frame );
    }
    g_screen.lock();
    int numBytes = av_image_get_buffer_size(
        AV_PIX_FMT_YUV420P,
        videoCtx->width,
        videoCtx->height,
        32
    );
    uint8_t* buffer = (uint8_t*)av_malloc( numBytes * sizeof( uint8_t ) );
    videoPicture->frame = av_frame_alloc();
    if( videoPicture->frame == NULL )
    {
        printf( "Could not allocate frame.\n" );
        return;
    }
    av_image_fill_arrays(
        videoPicture->frame->data,
        videoPicture->frame->linesize,
        buffer,
        AV_PIX_FMT_YUV420P,
        videoCtx->width,
        videoCtx->height,
        32
    );
    g_screen.unlock();
    videoPicture->width = videoCtx->width;
    videoPicture->height = videoCtx->height;
    videoPicture->allocated = 1;
}

int VideoPictureQueue::addToQueue( AVCodecContext* videoCtx, AVFrame* frame, SwsContext* swsCtx, double pts )
{
    SDL_LockMutex( m_mutex );
    while( m_queueSize >= VIDEO_PICTURE_QUEUE_SIZE && !g_quitFlag )
    {
        SDL_CondWait( m_cond, m_mutex );
    }
    SDL_UnlockMutex( m_mutex );
    if( g_quitFlag )
    {
        return -1;
    }
    VideoPicture* videoPicture = &m_pictQueue[m_wIndex];
    if( !videoPicture->frame || videoPicture->width != videoCtx->width || videoPicture->height != videoCtx->height )
    {
        videoPicture->allocated = 0;
        allocatePict( videoCtx );
        if( g_quitFlag )
            return -1;
    }
    if( videoPicture->frame )
    {
        videoPicture->pts = pts;
        videoPicture->frame->pict_type = frame->pict_type;
        videoPicture->frame->pts = frame->pts;
        videoPicture->frame->pkt_dts = frame->pkt_dts;
        videoPicture->frame->key_frame = frame->key_frame;
        videoPicture->frame->coded_picture_number = frame->coded_picture_number;
        videoPicture->frame->display_picture_number = frame->display_picture_number;
        videoPicture->frame->width = frame->width;
        videoPicture->frame->height = frame->height;
        sws_scale(
            swsCtx,
            (uint8_t const* const*)frame->data,
            frame->linesize,
            0,
            videoCtx->height,
            videoPicture->frame->data,
            videoPicture->frame->linesize
        );
        ++m_wIndex;
        if( m_wIndex == VIDEO_PICTURE_QUEUE_SIZE )
        {
            m_wIndex = 0;
        }
        SDL_LockMutex( m_mutex );
        m_queueSize++;
        SDL_UnlockMutex( m_mutex );
    }
    return 0;
}

VideoPicture* VideoPictureQueue::getVideoPicture()
{
    return &m_pictQueue[m_rIndex];
}

void VideoPictureQueue::decreasePictQueueSize()
{
    if( ++m_rIndex == VIDEO_PICTURE_QUEUE_SIZE )
    {
        m_rIndex = 0;
    }
    SDL_LockMutex(m_mutex );
    m_queueSize--;
    if( m_queueSize == 0 )
        printf( "Queue empty!\n" );
    SDL_CondSignal( m_cond );
    SDL_UnlockMutex( m_mutex );
}
