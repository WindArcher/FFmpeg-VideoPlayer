#include "Player.h"
#include "ffmpeg_exception.h"
#include "sdl_exception.h"
Player::Player()
{
	if( !SDLWrapper::init() )
		SDLWrapper::initSdl();
}

Player::~Player()
{
	if( m_video )
		m_video->quit();
	stopThread();
	avformat_close_input( &m_formatCtx );
	avcodec_free_context( &m_audioCodec.codecCtx );
	avcodec_free_context( &m_videoCodec.codecCtx );
	avcodec_parameters_free( &m_audioCodec.params);
	avcodec_parameters_free( &m_videoCodec.params );
	avcodec_close( m_audioCodec.codecCtx );
	avcodec_close( m_videoCodec.codecCtx );
	avformat_close_input( &m_formatCtx );
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
		throw std::exception( "Cannot find decoder" );
	codecData.codecCtx = avcodec_alloc_context3( codecData.codec );
	if( codecData.codecCtx == nullptr )
		throw std::exception( "Failed to allocate context decoder" );
	if( avcodec_parameters_to_context( codecData.codecCtx, codecData.params ) < 0 )
		throw std::exception( "Failed to transfer parameters to context" );
	if( avcodec_open2( codecData.codecCtx,codecData.codec,NULL) < 0 )
		throw std::exception( "Failed to open codec" );
}

void Player::decodeThread()
{
	m_decodeFinished = false;
	AVPacket* packet = av_packet_alloc();
	if( packet == NULL )
		throw std::exception( "Could not alloc packet.\n" );
	while( !m_quitFlag || ( m_audio->m_audioQueue.size() >= MAX_AUDIOQ_SIZE  && m_video->m_videoQueue.size() >= MAX_VIDEOQ_SIZE) )
	{
		int ret = av_read_frame( m_formatCtx, packet );
		if( ret < 0 )
		{
			if( ret == AVERROR_EOF )
			{
				m_finish = true;
				break;
			}
			else if( m_formatCtx->pb->error == 0 )
			{
				SDL_Delay( 10 );
				continue;
			}
			else
				break;
		}
		if( packet->stream_index == m_audioStreamNum )
			m_audio->m_audioQueue.put(packet);
		else if( packet->stream_index == m_videoStreamNum )
			m_video->m_videoQueue.put( packet );
		else
			av_packet_unref( packet );
	}
	av_packet_free( &packet );
	m_decodeFinished = true;
}

void Player::refreshVideo()
{
	if( m_video->m_pictQ.size() == 0 || m_pause )
	{
		if( m_finish && (m_video->m_videoQueue.size() == 0 ) )
		{
			SDL_Event event;
			event.type = SDL_QUIT;
			SDL_PushEvent( &event );
		}
		else
			sheduleRefresh( 1 );
	}
	else
	{
		VideoPicture pict{ m_video->m_pictQ.getPicture( true ) };
		double realDelay = m_video->getRealDelay( pict, m_audio->getAudioClock() );
		if( realDelay == -1 )
		{
			sheduleRefresh( 1 );
			return;
		}
		sheduleRefresh( (int)(realDelay * 1000 + 0.5) );
		printf( "Next Scheduled Refresh:\t%f\n\n", (double)(realDelay * 1000 + 0.5) );
		displayFrame( pict.frame.get() );
		if( m_video->m_pictQ.size() <= 60 )
			m_video->startVideoThread();
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
				m_pause = pause;
				if( pause )
				{
					SDL_RemoveTimer( m_timer );
					m_audio->stop();
				}
				else
				{
					sheduleRefresh( 1);
					m_video->resetFrameDelay();
					m_audio->start();
				}
				break;
			}
			case SDLK_RIGHT:
			{
				streamSeak( 10 );
				break;
			}
			case SDLK_LEFT:
			{
				streamSeak( -10 );
				break;
			}
			case SDLK_UP:
			{
				streamSeak( 60 );
				break;
			}
			case SDLK_DOWN:
			{
				streamSeak( -60 );
				break;
			}
			}
			break;
		}
		case SDL_QUIT:
		{
			quit();
			SDL_DestroyWindow( m_display.screen );
			SDL_Quit();
			exit( 0 );
			break;
		}
		case FF_REFRESH_EVENT:
		{
			refreshVideo();
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
		throw std::exception( "Couldn't show display window" );
	m_display.renderer = SDL_CreateRenderer(
		m_display.screen,
		-1,
		SDL_RENDERER_ACCELERATED | 
		SDL_RENDERER_PRESENTVSYNC | 
		SDL_RENDERER_TARGETTEXTURE );
	if( !m_display.renderer )
		throw std::exception( "Couldn't create renderer" );
	m_display.texture = SDL_CreateTexture(
		m_display.renderer,
		SDL_PIXELFORMAT_YV12,
		SDL_TEXTUREACCESS_STREAMING,
		m_videoCodec.codecCtx->width,
		m_videoCodec.codecCtx->height
		);
	if( !m_display.texture )
		throw std::exception( "Couldn't create texture" );
}


void Player::run( const std::string& filename )
{
    m_filename = filename;
	open();
	m_audio = std::make_unique<Audio>( this, m_audioCodec.codecCtx, m_formatCtx, m_audioStreamNum );
	m_video = std::make_unique<Video>( this, m_videoCodec.codecCtx, m_formatCtx, m_videoStreamNum );
	startThread();
	m_video->startVideoThread();
	m_audio->start();
	sheduleRefresh( 39 );
	SDLEventLoop();
}

void Player::quit()
{
	//m_audio->stop();
	m_video->quit();
	m_quitFlag == true;
	if( m_decodeThread.joinable() )
		m_decodeThread.join();
}

int64_t Player::getCurrentPos()
{
	return static_cast<int64_t>( m_audio->getAudioClock() * AV_TIME_BASE );
}

void Player::streamSeak( int shift )
{
	int seekFlag = shift < 0 ? AVSEEK_FLAG_BACKWARD : 0;
	int64_t videoPos, audioPos, currentPos;
	AVRational timeBaseQ{ 1, AV_TIME_BASE };
	currentPos = getCurrentPos();
	currentPos += shift* AV_TIME_BASE;
	if( currentPos < m_formatCtx->duration && currentPos >= 0)
	{
		m_finish = false;
		stopDisplay();
		m_video->stopVideoThread();
		stopThread();
		m_video->m_videoQueue.clear();
		m_video->m_pictQ.clear();
		m_audio->m_audioQueue.clear();
		videoPos = av_rescale_q( currentPos, timeBaseQ, m_formatCtx->streams[m_videoStreamNum]->time_base );
		audioPos = av_rescale_q( currentPos, timeBaseQ, m_formatCtx->streams[m_audioStreamNum]->time_base );
		int ret;
		if( m_formatCtx->streams[m_audioStreamNum] && (ret = av_seek_frame( m_formatCtx, m_audioStreamNum, audioPos, seekFlag )) > 0 )
			throw FFmpegException( "Seek exception\n", ret );
		if( m_formatCtx->streams[m_videoStreamNum] && (ret = av_seek_frame( m_formatCtx, m_videoStreamNum, videoPos, seekFlag )) > 0 )
			throw FFmpegException( "Seek exception\n", ret );
		startThread();
		m_video->startVideoThread();
		std::this_thread::sleep_for( std::chrono::milliseconds( 500 ) );
		startDisplay();
	}
	
}

bool Player::startThread()
{
	if( m_init && m_finish )
		return false;
	if( !m_decodeFinished && m_init )
		return false;
	if( m_decodeThread.joinable() )
		m_decodeThread.join();
	m_init = true;
	m_quitFlag = false;
	m_decodeThread = std::thread( &Player::decodeThread, this );
	return true;
}

bool Player::stopThread()
{
	m_quitFlag = true;
	if( m_decodeThread.joinable() )
		m_decodeThread.join();
	return true;
}

void Player::open()
{
	int ret = avformat_open_input( &m_formatCtx, m_filename.c_str(), NULL, NULL );
	if( ret != 0 )
		throw FFmpegException( "Opent input error\n", ret);
	ret = avformat_find_stream_info( m_formatCtx, NULL );
	if( ret < 0 )
		throw FFmpegException( "Find stream input error\n", ret );
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
		throw std::exception( "Video stream not found" );
	if( m_audioStreamNum == -1 )
		throw std::exception( "Audio stream not found" );
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
	m_timer = SDL_AddTimer( delay, SDLRefreshTimer, this );
	if(  !m_timer )
		throw SDLException("Could not schedule refresh callback");
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
			"Frame %c (%d) pts %d dts %d key_frame %d [coded_picture_number %d, display_picture_number %d, %dx%d]\n\n---------------------------------------------\n",
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
	}
	if( m_videoCodec.codecCtx->frame_number >= 160 );
}

void Player::stopDisplay()
{
	SDL_RemoveTimer( m_timer );
	m_audio->stop();
}

void Player::startDisplay()
{
	m_video->resetFrameDelay();
	m_audio->start();
	sheduleRefresh( 1 );	
}