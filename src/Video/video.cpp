#include "video.h"
#include <iostream>
namespace Player
{
    namespace Video
    {
        static constexpr auto AV_SYNC_THRESHOLD = 0.01;
        static constexpr auto AV_NOSYNC_THRESHOLD = 1.0;

        Video::Video( AVCodecContext* codecCtx, AVFormatContext* formatCtx, int streamNum )
        {
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

        void Video::startVideoThread()
        {
            if( !m_videoThreadActive )
            {
                if( m_videoFrameThread.joinable() )
                    m_videoFrameThread.join();
                m_quitFlag = false;
                m_videoThreadActive = true;
                m_videoFrameThread = std::thread( &Video::videoThread, this );
            }
            else
                resumeVideoThread();
        }

        void Video::stopVideoThread()
        {
            if( m_videoThreadActive )
            {
                m_quitFlag = true;
                m_cond.notify_one();
                if( m_videoFrameThread.joinable() )
                    m_videoFrameThread.join();
            }
        }

        void Video::pauseVideoThread()
        {
            std::unique_lock<std::mutex> lock( m_mutex );
            m_videoThreadPause = true;
        }

        void Video::resumeVideoThread()
        {
            m_videoThreadPause = false;
            m_cond.notify_one();
        }

        void Video::notifyVideoThread()
        {
            m_cond.notify_one();
        }

        void Video::resetFrameDelay()
        {
            m_frameDelay = (av_gettime() / 1000000.0);
        }

        double Video::getRealDelay( const VideoPicture& pict, double audioClock )
        {
            printf( "--------------------------------------\nCurrent Frame PTS:\t\t%f\n", pict.pts );
            printf( "Last Frame PTS:\t\t\t%f\n", m_frameLastPts );
            double ptsDelay = pict.pts - m_frameLastPts;
            printf( "PTS Distance:\t\t\t\t%f\n", ptsDelay );
            if( ptsDelay <= 0 || ptsDelay >= 1.0 )
            {
                ptsDelay = m_frameLastDelay;
            }
            printf( "Corrected PTS Delay:\t%f\n\n", ptsDelay );
            m_frameLastDelay = ptsDelay;
            m_frameLastPts = pict.pts;
            printf( "Current Frame PTS:\t\t%f\n", pict.pts );
            printf( "Audio Ref Clock:\t\t%f\n", audioClock );
            double audio_video_delay = pict.pts - audioClock;
            printf( "Audio Video Delay:\t\t%f\n\n", audio_video_delay );
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

            printf( "Frame delay:\t%f\n", m_frameDelay );
            double clock = (av_gettime() / 1000000.0);
            printf( "Current time:\t%f\n", clock );
            
            static double lastTime = (av_gettime() / 1000000.0);
            printf( "Time diff:\t%f\n", clock - lastTime );
            lastTime = clock;
            printf( "Delay:\t%f\n", (m_frameDelay - clock) );

            double realDelay = m_frameDelay - clock;

            printf( "Real Delay:\t\t\t\t%f\n", realDelay );

            if( realDelay < 0 )
            {
                return -1;
            }
            else if( realDelay < 0.01 )
            {
                realDelay = 0.01;
            }
            printf( "Corrected Real Delay:\t%f\n", realDelay );
            return  realDelay * 1000;
        }

        void Video::videoThread()
        {
            AVPacket* pkt = av_packet_alloc();
            m_frame = av_frame_alloc();
            try
            {
                bool frameFinished = false;
                if( !pkt )
                    throw std::exception( "Cannot allocate packet\n" );
                if( !m_frame )
                    throw std::exception( "Cannot allocate frame\n" );
                double pts = 0;
                while( !m_quitFlag )
                {
                    std::unique_lock<std::mutex> lock( m_mutex );
                    m_cond.wait( lock, [&] { return m_pictQ.size() <= 20 || m_videoThreadPause || m_quitFlag; } );
                    if( m_quitFlag )
                        break;
                    pts = 0;
                    pkt = m_videoQueue.get( true, &m_quitFlag );
                    if( pkt == nullptr )
                        continue;
                    int ret = avcodec_send_packet( m_videoContext, pkt );
                    if( ret < 0 )
                    {
                        throw std::exception( "Error sending packet for decoding" );
                    }
                    while( ret >= 0 )
                    {
                        ret = avcodec_receive_frame( m_videoContext, m_frame );

                        if( ret == AVERROR_EOF || ret == AVERROR( EAGAIN ) )
                            break;
                        else if( ret < 0 )
                        {
                            throw std::exception( "Error while decoding" );
                        }
                        else
                            frameFinished = true;
                        pts = guesPts( m_frame->pts, m_frame->pkt_dts );
                        if( pts == AV_NOPTS_VALUE )
                            pts = 0;
                        pts *= av_q2d( m_videoStream->time_base );
                        if( frameFinished )
                        {
                            pts = syncVideo( m_frame, pts );
                            m_pictQ.putFrame( m_frame, m_swsCtx, m_videoContext->width, m_videoContext->height, pts );
                            break;
                        }
                    }
                    av_packet_unref( pkt );
                }
                av_frame_free( &m_frame );
                av_free( m_frame );
                m_videoThreadActive = false;
                printf( "VideoThread Ended" );
            }
            catch( std::exception& e )
            {
                av_packet_free( &pkt );
                av_frame_free( &m_frame );
                std::cerr << e.what();
            }
        }

        double Video::syncVideo( AVFrame* frame, double pts )
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
    }
}