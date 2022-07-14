#include "Video.h"
#include "SDLWrapper.h"
#include <iostream>
#define AV_SYNC_THRESHOLD 0.01
#define AV_NOSYNC_THRESHOLD 1.0

Video::Video( DecodeThreadHandler* handler, AVCodecContext* codecCtx, AVFormatContext* formatCtx, int streamNum  ) : m_decodeHandler(handler)
{
    if( !SDLWrapper::init() )
        SDLWrapper::init();
    m_videoStream = formatCtx->streams[streamNum];
    m_videoContext = codecCtx;
    m_swsCtx = sws_getContext( codecCtx->width,
        codecCtx->height,
        codecCtx->pix_fmt,
        codecCtx->width,
        codecCtx->height,
        AV_PIX_FMT_YUV420P,
        SWS_BILINEAR,
        NULL,
        NULL,
        NULL );
}

void Video::quit()
{
    m_quitFlag = true;
    if( m_videoFrameThread.joinable() )
        m_videoFrameThread.join();
}

void Video::startVideoThread()
{
    if( !m_videoThreadFinished )
        return;
    if( m_videoFrameThread.joinable() )
        m_videoFrameThread.join();
    m_videoFrameThread = std::thread( &Video::videoThread, this );
}

double Video::getRealDelay( const VideoPicture& pict, double audioClock )
{
    printf( "Current Frame PTS:\t\t%f\n", pict.pts );
    printf( "Last Frame PTS:\t\t\t%f\n", m_frameLastPts );
    double ptsDelay = pict.pts - m_frameLastPts;

    printf( "PTS Delay:\t\t\t\t%f\n", ptsDelay );
    if( ptsDelay <= 0 || ptsDelay >= 1.0 )
    {
        ptsDelay = m_frameLastPts;
    }
    printf( "Corrected PTS Delay:\t%f\n", ptsDelay );
    m_frameLastDelay = ptsDelay;
    m_frameLastPts = pict.pts;

    printf( "Audio Ref Clock:\t\t%f\n", audioClock );

    double audio_video_delay = pict.pts - audioClock;

    printf( "Audio Video Delay:\t\t%f\n", audio_video_delay );
    double sync_threshold = (ptsDelay > AV_SYNC_THRESHOLD) ? ptsDelay : AV_SYNC_THRESHOLD;

    printf( "Sync Threshold:\t\t\t%f\n", sync_threshold );

    if( fabs( audio_video_delay ) < AV_NOSYNC_THRESHOLD )
    {
        if( audio_video_delay <= -sync_threshold )
        {
            ptsDelay = 0;
        }
        else if( audio_video_delay > sync_threshold )
        {
            ptsDelay = 2 * ptsDelay;
        }
    }

    printf( "Corrected PTS delay:\t%f\n", ptsDelay );

    m_frameDelay += ptsDelay;

    double realDelay = m_frameDelay - (av_gettime() / 1000000.0);

    printf( "Real Delay:\t\t\t\t%f\n", realDelay );

    if( realDelay < 0.010 )
    {
        realDelay = 0.010;
    }
    printf( "Corrected Real Delay:\t%f\n", realDelay );
    return realDelay;
}

void Video::videoThread()
{
    AVPacket* pkt = av_packet_alloc();
   // m_videoThreadFinished = false;
    static AVFrame* frame = av_frame_alloc();
    try
    {
        bool frameFinished = false;
        if( !pkt )
            throw std::exception( "Cannot allocate packet\n" );
        if( !frame )
            throw std::exception( "Cannot allocate frame\n" );
        double pts = 0;
        while( !m_quitFlag )
        {
            if( m_pictQ.size() >= 100 )
            {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                continue;
            }
            pts = 0;
            if( m_videoQueue.size() <= MIN_VIDEOQ_SIZE )
                m_decodeHandler->startThread();
            pkt = m_videoQueue.get( true );
            int ret = avcodec_send_packet( m_videoContext, pkt );
            if( ret < 0 )
            {
                av_packet_free( &pkt );
                av_frame_free( &frame );
                throw std::exception( "Error sending packet for decoding" );
            }
            while( ret >= 0 )
            {
                ret = avcodec_receive_frame( m_videoContext, frame );
                if( ret == AVERROR( EAGAIN ) || ret == AVERROR_EOF )
                    break;
                else if( ret < 0 )
                {
                    av_packet_free( &pkt );
                    av_frame_free( &frame );
                    throw std::exception( "Error while decoding" );
                }
                else
                    frameFinished = true;
                pts = guesPts( frame->pts, frame->pkt_dts );
                if( pts == AV_NOPTS_VALUE )
                    pts = 0;
                pts *= av_q2d( m_videoStream->time_base );
                if( frameFinished )
                {
                    pts = syncVideo( frame, pts );
                    m_pictQ.putFrame( frame, m_swsCtx, m_videoContext->width, m_videoContext->height, pts );
                    break;
                }
            }
            av_packet_unref( pkt );
        }
        av_frame_free( &frame );
        av_free( frame );
        //m_videoThreadFinished = true;
    }
    catch( std::exception& e )
    {
        std::cerr << e.what();
    }
}

double Video::syncVideo(  AVFrame* frame, double pts )
{
    double frameDelay;

    if( pts != 0 )
    {
        m_videoClock = pts;
    }
    else
    {
        pts = m_videoClock;
    }
    frameDelay = av_q2d( m_videoContext->time_base );
    frameDelay += frame->repeat_pict * (frameDelay * 0.5);
    m_videoClock += frameDelay;
    return pts;
}

double Video::guesPts( int64_t reoderedPts, int64_t dts )
{
    int64_t pts = AV_NOPTS_VALUE;

    if( dts != AV_NOPTS_VALUE )
    {
        m_videoContext->pts_correction_num_faulty_dts += dts <= m_videoContext->pts_correction_last_dts;
        m_videoContext->pts_correction_last_dts = dts;
    }
    else if( reoderedPts != AV_NOPTS_VALUE )
    {
        m_videoContext->pts_correction_last_dts = reoderedPts;
    }

    if( reoderedPts != AV_NOPTS_VALUE )
    {
        m_videoContext->pts_correction_num_faulty_pts += reoderedPts <= m_videoContext->pts_correction_last_pts;
        m_videoContext->pts_correction_last_pts = reoderedPts;
    }
    else if( dts != AV_NOPTS_VALUE )
    {
        m_videoContext->pts_correction_last_pts = dts;
    }

    if( (m_videoContext->pts_correction_num_faulty_pts <= m_videoContext->pts_correction_num_faulty_dts || dts == AV_NOPTS_VALUE) && reoderedPts != AV_NOPTS_VALUE )
    {
        pts = reoderedPts;
    }
    else
    {
        pts = dts;
    }
    return pts;
}
