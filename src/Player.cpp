#include "Player.h"
#include "Utils.h"
Player* Player::m_instance = 0;

Player::Player()
{
    SDLWrapper::initSdl();
}

void Player::getCodecParams()
{
	m_videoCodec.params = m_formatCtx->streams[m_videoStreamNum]->codecpar;
	m_audioCodec.params = m_formatCtx->streams[m_audioStreamNum]->codecpar;
}

void Player::readCodec( CodecData& codecData )
{
	codecData.codec = avcodec_find_decoder( codecData.params->codec_id );
	if( !codecData.codec )
		Utils::displayException( "Cannot find decoder" );
	codecData.codecCtx = avcodec_alloc_context3( codecData.codec );
	if( codecData.codecCtx == nullptr )
		Utils::displayException( "Failed to allocate context decoder" );
	if( avcodec_parameters_to_context( codecData.codecCtx, codecData.params ) < 0 )
		Utils::displayException( "Failed to transfer parameters to context" );
	if( avcodec_open2( codecData.codecCtx,codecData.codec,NULL) < 0 )
		Utils::displayException( "Failed to open codec" );
}

void Player::decodeThread()
{
	AVPacket* packet = av_packet_alloc();
	if( packet == NULL )
		Utils::displayException( "Could not alloc packet.\n" );
	while( !m_quitFlag )
	{
		int ret = av_read_frame( m_formatCtx, packet );
		if( ret < 0 )
		{
			if( ret == AVERROR_EOF )
				break;
			else if( m_formatCtx->pb->error == 0 )
			{
				SDL_Delay( 10 );
				continue;
			}
			else
				break;
		}
		static int packeetNum = 0;
		if( packet->stream_index == m_audioStreamNum )
			Audio::getInstance()->m_audioQueue.put(packet);
		else if( packet->stream_index == m_videoStreamNum )
			Video::getInstance()->m_videoQueue.put( packet );
		else
			av_packet_unref( packet );
	}
}

void Player::SDLEventLoop()
{
	bool pause = false;
	SDL_Event event;
	while( true )
	{
		if( SDL_WaitEvent( &event ) == 0 )
		{
			printf( "SDL_WaitEvent failed: %s.\n", SDL_GetError() );
		}
		switch( event.type )
		{
		case SDL_KEYDOWN:
		{
			switch( event.key.keysym.sym )
			{
			case SDLK_SPACE:
			{
				pause = !pause;
				Video::getInstance()->setPause( pause );
				SDL_PauseAudio( pause );
				break;
			}
			}
			break;
		}
		case FF_QUIT_EVENT:
		case SDL_QUIT:
		{
			quit();
			SDL_Quit();
			clear();
			exit(0);
			break;
		}
		case FF_REFRESH_EVENT:
		{
			Video::getInstance()->refreshVideo();
			break;
		}
		if( m_quitFlag )
			break;
		}
	}
}

void Player::createDisplay()
{
	if( m_display.screen )
		return;
	m_display.screen = SDL_CreateWindow(
		m_windowName.c_str(),
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		m_videoCodec.codecCtx->width / 2,
		m_videoCodec.codecCtx->height / 2,
		SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI );
	SDL_GL_SetSwapInterval( 1 );
	if( !m_display.screen )
		Utils::displayException( "Couldn't show display window" );
	m_display.renderer = SDL_CreateRenderer(
		m_display.screen,
		-1,
		SDL_RENDERER_ACCELERATED | 
		SDL_RENDERER_PRESENTVSYNC | 
		SDL_RENDERER_TARGETTEXTURE );
	if( !m_display.renderer )
		Utils::displayException( "Couldn't create renderer" );
	m_display.texture = SDL_CreateTexture(
		m_display.renderer,
		SDL_PIXELFORMAT_YV12,
		SDL_TEXTUREACCESS_STREAMING,
		m_videoCodec.codecCtx->width,
		m_videoCodec.codecCtx->height
		);
	if( !m_display.texture )
		Utils::displayException( "Couldn't create texture" );
}

Player* Player::getInstance()
{
    if( Player::m_instance == nullptr )
		Player::m_instance = new Player();
    return Player::m_instance;
}

void Player::run( const std::string& filename )
{
    m_filename = filename;
	open();
	Audio::getInstance()->init( m_audioCodec.codecCtx, m_formatCtx->streams[m_audioStreamNum]);
	Video::getInstance()->init( m_videoCodec.codecCtx, m_formatCtx->streams[m_videoStreamNum] );
	m_decodeThread = std::thread( &Player::decodeThread, this );
	Audio::getInstance()->startPlaying();
	Video::getInstance()->startVideoThread();
	sheduleRefresh( 39 );
	SDLEventLoop();
}

void Player::quit()
{
	Video::getInstance()->quit();
	m_quitFlag == true;
	if( m_decodeThread.joinable() )
		m_decodeThread.join();
}


void Player::open()
{
	int ret = avformat_open_input( &m_formatCtx, m_filename.c_str(), NULL, NULL );
	if( ret != 0 )
		Utils::displayFFMpegException( ret );
	ret = avformat_find_stream_info( m_formatCtx, NULL );
	if( ret < 0 )
		Utils::displayFFMpegException( ret );
	findStreamNumbers();
	getCodecParams();
	readCodec( m_videoCodec );
	readCodec( m_audioCodec );

}


void Player::findStreamNumbers()
{
	for( unsigned int i = 0; i < m_formatCtx->nb_streams; i++ )
		if( m_formatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO )
		{
			m_videoStreamNum = i;
			break;
		}
	for( unsigned int i = 0; i < m_formatCtx->nb_streams; i++ )
		if( m_formatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO )
		{
			m_audioStreamNum = i;
			break;
		}
	if( m_videoStreamNum == -1 )
		Utils::displayException( "Video stream not found" );
	if( m_audioStreamNum == -1 )
		Utils::displayException( "Audio stream not found" );
}

void Player::clear()
{
	// close context info
	avformat_close_input( &m_formatCtx );
	avcodec_free_context( &m_audioCodec.codecCtx );
	avcodec_free_context( &m_videoCodec.codecCtx );

	// Close the codecs
	avcodec_close( m_audioCodec.codecCtx );
	avcodec_close( m_videoCodec.codecCtx );

	// Close the video file
	avformat_close_input( &m_formatCtx );
	delete Player::getInstance();
}

Uint32 Player::SDLRefreshTimer( Uint32 interval, void* )
{
	SDL_Event event;
	event.type = FF_REFRESH_EVENT;
	SDL_PushEvent( &event );
	return 0;
}

void Player::sheduleRefresh( int delay )
{
	if( SDL_AddTimer( delay, SDLRefreshTimer, this ) == 0 )
		printf( "Could not schedule refresh callback: %s.\n.", SDL_GetError() );
}

void Player::displayFrame( AVFrame* frame )
{
	createDisplay();
	float aspect_ratio;
	int w, h, x, y;
	if( frame )
	{
		if( m_videoCodec.codecCtx->sample_aspect_ratio.num == 0 )
		{
			aspect_ratio = 0;
		}
		else
		{
			aspect_ratio = av_q2d( m_videoCodec.codecCtx->sample_aspect_ratio ) * m_videoCodec.codecCtx->width / m_videoCodec.codecCtx->height;
		}

		if( aspect_ratio <= 0.0 )
		{
			aspect_ratio = (float)m_videoCodec.codecCtx->width /
				(float)m_videoCodec.codecCtx->height;
		}

		int screen_width;
		int screen_height;
		SDL_GetWindowSize( m_display.screen, &screen_width, &screen_height );
		h = screen_height;
		w = ((int)rint( h * aspect_ratio )) & -3;
		if( w > screen_width )
		{
			w = screen_width;
			h = ((int)rint( w / aspect_ratio )) & -3;
		}

		x = (screen_width - w);
		y = (screen_height - h);
		printf(
			"Frame %c (%d) pts %d dts %d key_frame %d [coded_picture_number %d, display_picture_number %d, %dx%d]\n",
			av_get_picture_type_char( frame->pict_type ),
			m_videoCodec.codecCtx->frame_number,
			frame->pts,
			frame->pkt_dts,
			frame->key_frame,
			frame->coded_picture_number,
			frame->display_picture_number,
			frame->width,
			frame->height
		);
		SDL_Rect rect;
		rect.x = x;
		rect.y = y;
		rect.w = 2 * w;
		rect.h = 2 * h;
		m_display.mutex.lock();
		SDL_UpdateYUVTexture(
			m_display.texture,
			&rect,
			frame->data[0],
			frame->linesize[0],
			frame->data[1],
			frame->linesize[1],
			frame->data[2],
			frame->linesize[2]
		);
		SDL_RenderClear( m_display.renderer );
		SDL_RenderCopy( m_display.renderer, m_display.texture, NULL, NULL );
		SDL_RenderPresent( m_display.renderer );
		m_display.mutex.unlock();
		av_frame_unref( frame );
	}
}
