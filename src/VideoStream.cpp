#include "VideoStream.h"
extern "C"{
#include <libavutil/time.h>
}
#include <iostream>
VideoStream::VideoStream( )
{
    
}

bool VideoStream::findVideoStreamIndex( AVFormatContext* formatCtx )
{
    for( int i = 0; i < formatCtx->nb_streams; i++ )
    {
        if( formatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && m_index < 0 )
        {
            m_index = i;
            return true;
        }
    }
    return false;
}

bool VideoStream::openStream( AVFormatContext* formatCtx )
{
    AVCodec* codec = NULL;
    codec = avcodec_find_decoder( formatCtx->streams[m_index]->codecpar->codec_id );
    if( codec == NULL )
    {
        std::cout << "Unsupported codec.\n";
        return false;
    }
    AVCodecContext* codecCtx = NULL;
    codecCtx = avcodec_alloc_context3( codec );
    int ret = avcodec_parameters_to_context( codecCtx, formatCtx->streams[m_index]->codecpar );
    if( ret != 0 )
    {
        std::cout << "Could not copy codec context.\n";
        return false;
    }
    if( avcodec_open2( codecCtx, codec, NULL ) < 0 )
    {
        std::cout << "Unsupported codec.\n";
        return -1;
    }
    if( codecCtx->codec_type == AVMEDIA_TYPE_VIDEO )
    {
        m_videoStream = formatCtx->streams[m_index];
        m_videoContext = codecCtx;
        m_frameDelay = (av_gettime() / 1000000.0);
        m_frameLastDelay = 40e-3;
        m_videoThread = SDL_CreateThread(videoThread,"Video Thread",this);
        m_swsCtx = sws_getContext( m_videoContext->width,
            m_videoContext->height,
            m_videoContext->pix_fmt,
            m_videoContext->width,
            m_videoContext->height,
            AV_PIX_FMT_YUV420P,
            SWS_BILINEAR,
            NULL,
            NULL,
            NULL
        );
    }
    return true;
}

bool VideoStream::addToQueue( AVPacket* packet )
{
    return m_videoQueue.put( packet );
}

double VideoStream::synchronizeVideo( AVCodecContext* videoCtx ,double& videoClock, AVFrame* src_frame, double pts )
{
    double frame_delay;

    if( pts != 0 )
    {
        videoClock = pts;
    }
    else
    {
        pts = videoClock;
    }
    frame_delay = av_q2d( videoCtx->time_base );
    frame_delay += src_frame->repeat_pict * (frame_delay * 0.5);
    videoClock += frame_delay;
    return pts;
}

double VideoStream::guessPts( AVCodecContext* ctx, int64_t reordered_pts, int64_t dts )
{
    int64_t pts = AV_NOPTS_VALUE;

    if( dts != AV_NOPTS_VALUE )
    {
        ctx->pts_correction_num_faulty_dts += dts <= ctx->pts_correction_last_dts;
        ctx->pts_correction_last_dts = dts;
    }
    else if( reordered_pts != AV_NOPTS_VALUE )
    {
        ctx->pts_correction_last_dts = reordered_pts;
    }

    if( reordered_pts != AV_NOPTS_VALUE )
    {
        ctx->pts_correction_num_faulty_pts += reordered_pts <= ctx->pts_correction_last_pts;
        ctx->pts_correction_last_pts = reordered_pts;
    }
    else if( dts != AV_NOPTS_VALUE )
    {
        ctx->pts_correction_last_pts = dts;
    }

    if( (ctx->pts_correction_num_faulty_pts <= ctx->pts_correction_num_faulty_dts || dts == AV_NOPTS_VALUE) && reordered_pts != AV_NOPTS_VALUE )
    {
        pts = reordered_pts;
    }
    else
    {
        pts = dts;
    }

    return pts;
}

int videoThread( void* arg )
{
    VideoStream* videoStream = reinterpret_cast<VideoStream*>( arg );
    bool frameFinished = false;
    AVPacket* packet = av_packet_alloc();
    if( packet == NULL )
    {
        printf( "Could not alloc packet.\n" );
        return -1;
    }
    static AVFrame* frame = av_frame_alloc();
    if( !frame )
    {
        std::cout << "Could not allocate AVFrame.\n";
        return -1;
    }
    double pts;
    while( true )
    {
        pts = 0;
        int ret = videoStream->m_videoQueue.get( packet,1 );
        if( ret < 0 )
        {
            std::cout << "Error sending packet for decoding.\n";
            return -1;
        }
        ret = avcodec_send_packet( videoStream->m_videoContext, packet );
        if( ret < 0 )
        {
            printf( "Error sending packet for decoding.\n" );
            return -1;
        }
        while( ret >= 0 )
        {
            ret = avcodec_receive_frame( videoStream->m_videoContext, frame );

            if( ret == AVERROR( EAGAIN ) || ret == AVERROR_EOF )
            {
                break;
            }
            else if( ret < 0 )
            {
                printf( "Error while decoding.\n" );
                return -1;
            }
            else
            {
                frameFinished = 1;
            }

            pts = VideoStream::guessPts( videoStream->m_videoContext, frame->pts, frame->pkt_dts );

            if( pts == AV_NOPTS_VALUE )
            {
                pts = 0;
            }
            pts *= av_q2d( videoStream->m_videoStream->time_base );
            if( frameFinished )
            {
                static int c = 0;
                c++;
                pts = VideoStream::synchronizeVideo( videoStream->m_videoContext, videoStream->m_videoClock, frame, pts );

                if( videoStream->m_pictQueue.addToQueue( videoStream->m_videoContext, frame, videoStream->m_swsCtx, pts ) < 0 )
                {
                    break;
                }
            }
        }
        av_packet_unref( packet );
    }
    av_frame_free( &frame );
    av_free( frame );
    return 0;
}
