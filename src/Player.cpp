#include "Player.h"
#include "Screen.h"
extern Screen g_screen;
Player::Player()
{
    if( SDL_Init( SDL_INIT_AUDIO | SDL_INIT_VIDEO | SDL_INIT_TIMER ) != 0 )
        printf( "Could not initialize SDL - %s\n ", SDL_GetError() );
    SDL_AudioInit( "directsound" );
}

Player::~Player()
{

}

void Player::setFileName( const std::string& filename )
{
    m_fileName = filename;
}

bool Player::startDecodeThread()
{
    m_decodeThread = SDL_CreateThread( decodeThread, "Decoding Thread", this );
    if( !m_decodeThread )
    {
        std::cout << "Could not start decoding SDL_Thread - exiting.\n";
        return false;
    }
    return true;
}

void Player::refreshVideo()
{    
    VideoPicture* videoPicture;
    double pts_delay;
    double audio_ref_clock;
    double sync_threshold;
    double real_delay;
    double audio_video_delay;

    // check the video stream was correctly opened
    if( m_videoStream.m_videoStream )
    {
        if( m_videoStream.m_pictQueue.size() == 0 )
        {
            sheduleRefresh( 1 );
        }
        else
        {
            static int count = 0;
            count++;
            if( count >= 260 )
                printf("Breakpoint\n");
            // get VideoPicture reference using the queue read index
            videoPicture = m_videoStream.m_pictQueue.getVideoPicture();

            printf( "Current Frame PTS:\t\t%f\n", videoPicture->pts );
            printf( "Last Frame PTS:\t\t\t%f\n", m_videoStream.m_frameLastPTS );

            // get last frame pts
            pts_delay = videoPicture->pts - m_videoStream.m_frameLastPTS;

            printf( "PTS Delay:\t\t\t\t%f\n", pts_delay );

            // if the obtained delay is incorrect
            if( pts_delay <= 0 || pts_delay >= 1.0 )
            {
                // use the previously calculated delay
                pts_delay = m_videoStream.m_frameLastDelay;
            }

            printf( "Corrected PTS Delay:\t%f\n", pts_delay );

            // save delay information for the next time
            m_videoStream.m_frameLastDelay = pts_delay;
            m_videoStream.m_frameLastPTS = videoPicture->pts;
            // update delay to stay in sync with the audio
            audio_ref_clock = m_audioStream.getAudioClock();

            printf( "Audio Ref Clock:\t\t%f\n", audio_ref_clock );

            audio_video_delay = videoPicture->pts - audio_ref_clock;

            printf( "Audio Video Delay:\t\t%f\n", audio_video_delay );

            // skip or repeat the frame taking into account the delay
            sync_threshold = (pts_delay > AV_SYNC_THRESHOLD) ? pts_delay : AV_SYNC_THRESHOLD;

            printf( "Sync Threshold:\t\t\t%f\n", sync_threshold );

            // check audio video delay absolute value is below sync threshold
            if( fabs( audio_video_delay ) < AV_NOSYNC_THRESHOLD )
            {
                if( audio_video_delay <= -sync_threshold )
                {
                    pts_delay = 0;
                }
                else if( audio_video_delay >= sync_threshold )
                {
                    pts_delay = 2 * pts_delay;  // [2]
                }
            }

            printf( "Corrected PTS delay:\t%f\n", pts_delay );

            m_videoStream.m_frameDelay += pts_delay;   // [2]

            // compute the real delay
            real_delay = m_videoStream.m_frameDelay - (av_gettime() / 1000000.0);

            printf( "Real Delay:\t\t\t\t%f\n", real_delay );

            if( real_delay < 0.010 )
            {
                real_delay = 0.010;
            }

            printf( "Corrected Real Delay:\t%f\n", real_delay );

            sheduleRefresh( (int)(real_delay * 1000 + 0.5) );

            printf( "Next Scheduled Refresh:\t%f\n\n", (double)(real_delay * 1000 + 0.5) );

            videoDisplay();

            m_videoStream.m_pictQueue.decreasePictQueueSize();
        }
    }
    else
    {
        sheduleRefresh( 100 );
    }
}

void Player::sheduleRefresh( int delay )
{
    if( SDL_AddTimer( delay, sdlRefreshTimer, this ) == 0 )
        printf( "Could not schedule refresh callback: %s.\n.", SDL_GetError() );
}

int decodeThread( void* arg )
{
    Player* player = reinterpret_cast<Player*>( arg );
    if( avformat_open_input( &player->m_formatCtx, player->m_fileName.c_str(), nullptr, nullptr ) < 0 )
    {
        std::cout << "Could not open file " << player->m_fileName << std::endl ;
        return -1;
    }
    if( avformat_find_stream_info( player->m_formatCtx, NULL ) < 0 )
    {
        std::cout << "Could not find stream information " << player->m_fileName << std::endl;
        return -1;
    }
    av_dump_format( player->m_formatCtx, 0, player->m_fileName.c_str(), 0 );

    if( !player->m_audioStream.findAudioStreamIndex( player->m_formatCtx ) || !player->m_audioStream.openStream( player->m_formatCtx ) )
    {
        std::cout << "Could not open audio codec.\n";
        return player->fail();
    }

    if( !player->m_videoStream.findVideoStreamIndex( player->m_formatCtx ) || !player->m_videoStream.openStream( player->m_formatCtx ) )
    {
        std::cout << "Could not open video codec.\n";
        return player->fail();
    }
   
    if( player->m_audioStream.getIndex() == -1 || player->m_videoStream.getIndex() == -1 )
    {
        std::cout << "Could not open codecs " << player->m_fileName << std::endl;
        return player->fail();
    }

    AVPacket* packet = av_packet_alloc();
    if( packet == NULL )
    {
        std::cout << "Could not alloc packet.\n";
        return -1;
    }
    while( true )
    {
        if( g_quitFlag )
            break;
        if( player->m_audioStream.queueSize() > MAX_AUDIOQ_SIZE || player->m_videoStream.queueSize() > MAX_VIDEOQ_SIZE )
        {
            SDL_Delay( 10 );
            continue;
        }
        int ret = av_read_frame( player->m_formatCtx, packet );
        if( ret < 0 )
        {
            if( ret == AVERROR_EOF )
            {
                g_quitFlag = true;
                break;
            }
            else if( player->m_formatCtx->pb->error == 0 )
            {
                SDL_Delay( 10 );
                continue;
            }
            else
                break;
        }
        if( packet->stream_index == player->m_audioStream.getIndex() )
            player->m_audioStream.addToQueue( packet );
        else if( packet->stream_index == player->m_videoStream.getIndex() )
            player->m_videoStream.addToQueue( packet );
        else
            av_packet_unref( packet );
    }
    while( !g_quitFlag )
        SDL_Delay( 100 );
    avformat_close_input( &player->m_formatCtx );
    return 0;
}

Uint32 sdlRefreshTimer( Uint32 interval, void* player )
{
    SDL_Event event;
    event.type = FF_REFRESH_EVENT;
    event.user.data1 = player;
    SDL_PushEvent( &event );
    return 0;
}


int Player::fail()
{
    SDL_Event event;
    event.type = FF_QUIT_EVENT;
    SDL_PushEvent( &event );
    return -1;
}

void Player::videoDisplay()
{
    if( !g_screen.m_screen )
    {
        // create a window with the specified position, dimensions, and flags.
        g_screen.m_screen = SDL_CreateWindow(
            "FFmpeg SDL Video Player",
            SDL_WINDOWPOS_UNDEFINED,
            SDL_WINDOWPOS_UNDEFINED,
            m_videoStream.m_videoContext->width / 2,
            m_videoStream.m_videoContext->height / 2,
            SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI
        );

        SDL_GL_SetSwapInterval( 1 );

    }
    // check window was correctly created
    if( !g_screen.m_screen )
    {
        printf( "SDL: could not create window - exiting.\n" );
        return;
    }


    if( !m_videoStream.m_Renderer )
    {
        // create a 2D rendering context for the SDL_Window
        m_videoStream.m_Renderer = SDL_CreateRenderer( g_screen.m_screen, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_TARGETTEXTURE );
    }

    if( !m_videoStream.m_texture )
    {
        // create a texture for a rendering context
        m_videoStream.m_texture = SDL_CreateTexture(
            m_videoStream.m_Renderer,
            SDL_PIXELFORMAT_YV12,
            SDL_TEXTUREACCESS_STREAMING,
            m_videoStream.m_videoContext->width,
            m_videoStream.m_videoContext->height
        );
    }

    // reference for the next VideoPicture to be displayed
    VideoPicture* videoPicture;

    float aspect_ratio;

    int w, h, x, y;

    // get next VideoPicture to be displayed from the VideoPicture queue
    videoPicture = m_videoStream.m_pictQueue.getVideoPicture();

    if( videoPicture->frame )
    {
        if( m_videoStream.m_videoContext->sample_aspect_ratio.num == 0 )
        {
            aspect_ratio = 0;
        }
        else
        {
            aspect_ratio = av_q2d( m_videoStream.m_videoContext->sample_aspect_ratio ) * m_videoStream.m_videoContext->width / m_videoStream.m_videoContext->height;
        }

        if( aspect_ratio <= 0.0 )
        {
            aspect_ratio = (float)m_videoStream.m_videoContext->width /
                (float)m_videoStream.m_videoContext->height;
        }

        // get the size of a window's client area
        int screen_width;
        int screen_height;
        SDL_GetWindowSize( g_screen.m_screen, &screen_width, &screen_height );

        // global SDL_Surface height
        h = screen_height;

        // retrieve width using the calculated aspect ratio and the screen height
        w = ((int)rint( h * aspect_ratio )) & -3;

        // if the new width is bigger than the screen width
        if( w > screen_width )
        {
            // set the width to the screen width
            w = screen_width;

            // recalculate height using the calculated aspect ratio and the screen width
            h = ((int)rint( w / aspect_ratio )) & -3;
        }

        x = (screen_width - w);
        y = (screen_height - h);
            // dump information about the frame being rendered
        printf(
                "Frame %c (%d) pts %d dts %d key_frame %d [coded_picture_number %d, display_picture_number %d, %dx%d]\n",
                av_get_picture_type_char( videoPicture->frame->pict_type ),
                m_videoStream.m_videoContext->frame_number,
                videoPicture->frame->pts,
                videoPicture->frame->pkt_dts,
                videoPicture->frame->key_frame,
                videoPicture->frame->coded_picture_number,
                videoPicture->frame->display_picture_number,
                videoPicture->frame->width,
                videoPicture->frame->height
            );
            static int c = 0;
            c++;
            if( c == 260 )
                printf( "260\n" );

            // set blit area x and y coordinates, width and height
            SDL_Rect rect;
            rect.x = x;
            rect.y = y;
            rect.w = 2 * w;
            rect.h = 2 * h;

            // lock screen mutex
            g_screen.lock();

            // update the texture with the new pixel data
            SDL_UpdateYUVTexture(
                m_videoStream.m_texture,
                &rect,
                videoPicture->frame->data[0],
                videoPicture->frame->linesize[0],
                videoPicture->frame->data[1],
                videoPicture->frame->linesize[1],
                videoPicture->frame->data[2],
                videoPicture->frame->linesize[2]
            );

            // clear the current rendering target with the drawing color
            SDL_RenderClear( m_videoStream.m_Renderer );

            // copy a portion of the texture to the current rendering target
            SDL_RenderCopy( m_videoStream.m_Renderer, m_videoStream.m_texture, NULL, NULL );

            // update the screen with any rendering performed since the previous call
            SDL_RenderPresent( m_videoStream.m_Renderer );

            // unlock screen mutex
            g_screen.unlock();
    }
}

