#include "Video.h"
#include "Utils.h"
#define AV_SYNC_THRESHOLD 0.01
#define AV_NOSYNC_THRESHOLD 1.0

Video* Video::m_videoInstance = 0;

Video* Video::getInstance()
{
    if( Video::m_videoInstance == nullptr )
        Video::m_videoInstance = new Video();
    return Video::m_videoInstance;
}

bool Video::init( AVCodecContext* codecCtx, AVStream* stream )
{
    if( !m_init )
    {
        m_videoStream = stream;
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
        m_init = true;
    }
    return m_init;
}


void Video::quit()
{
    m_quitFlag = true;
    if( m_videoFrameThread.joinable() )
        m_videoFrameThread.join();
}

bool Video::startVideoThread()
{
    if( !m_init )
        return false;
    m_videoFrameThread = std::thread( &Video::videoThread, this);  
    return true;
}

bool Video::refreshVideo()
{
    if( !m_init )
        return false;
    if( !m_videoStream )
    {
        Player::getInstance()->sheduleRefresh( 100 );
        return true;
    }
    if( m_pictQ.size() == 0 || m_pause == true )
        Player::getInstance()->sheduleRefresh( 1 );
    else
    {
        VideoPicture pict{ m_pictQ.getPicture( true) };
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
        double audio_ref_clock = Audio::getInstance()->getAudioClock();

        printf( "Audio Ref Clock:\t\t%f\n", audio_ref_clock );

        double audio_video_delay = pict.pts - audio_ref_clock;

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

        Player::getInstance()->sheduleRefresh( (int)(realDelay * 1000 + 0.5) );

        printf( "Next Scheduled Refresh:\t%f\n\n", (double)(realDelay * 1000 + 0.5) );
        Player::getInstance()->displayFrame( pict.frame.get() );
    }
    return true;
}

void Video::videoThread()
{
    bool frameFinished = false;
    AVPacket* pkt;
    static AVFrame* frame = av_frame_alloc();
    if( !frame )
        Utils::displayException( "Cannot allocate frame" );
    double pts =0;
    while( !m_quitFlag )
    {
        pts = 0;
        pkt = m_videoQueue.get( true );
        int ret = avcodec_send_packet( m_videoContext, pkt );
        if( ret < 0 )
            Utils::displayException( "Error sending packet for decoding" );
        while( ret >= 0 )
        {
            ret = avcodec_receive_frame( m_videoContext, frame );
            if( ret == AVERROR( EAGAIN ) || ret == AVERROR_EOF )
                break;
            else if( ret < 0 )
                Utils::displayException( "Error while decoding" );
            else
                frameFinished = true;
            pts = guesPts( frame->pts, frame->pkt_dts );
            if( pts == AV_NOPTS_VALUE )
                pts = 0;
            pts *= av_q2d( m_videoStream->time_base );
            if( frameFinished )
            {
                pts = syncVideo( frame,pts );
                m_pictQ.putFrame( frame,m_swsCtx, m_videoContext->width, m_videoContext->height, pts );
                break;
            }
        }
        av_packet_unref( pkt );
    }
    av_frame_free( &frame );
    av_free( frame );
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
