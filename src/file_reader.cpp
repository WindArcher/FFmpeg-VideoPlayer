#include "file_reader.h"
#include "Exceptions/ffmpeg_exception.h"
namespace Player
{
	static constexpr auto approximateFPS = 60;

	namespace CodecData
	{
		CodecData::CodecData( AVCodecParameters* params )
		{
			params = params;
			codec = avcodec_find_decoder( params->codec_id );
			if( !codec )
				throw std::exception( "Cannot find decoder" );
			codecCtx = avcodec_alloc_context3( codec );
			if( codecCtx == nullptr )
				throw std::exception( "Failed to allocate context decoder" );
			if( avcodec_parameters_to_context( codecCtx, params ) < 0 )
				throw std::exception( "Failed to transfer parameters to context" );
			if( avcodec_open2( codecCtx, codec, NULL ) < 0 )
				throw std::exception( "Failed to open codec" );
		}

		CodecData::~CodecData()
		{
			avcodec_free_context( &codecCtx );
			avcodec_parameters_free( &params );
		}
	};

	FileReader::~FileReader()
	{
		avformat_close_input( &m_formatCtx );
	}

	bool FileReader::open( const std::string& filename )
	{
		int ret = avformat_open_input( &m_formatCtx, filename.c_str(), NULL, NULL );
		if( ret != 0 )
			return false;
		ret = avformat_find_stream_info( m_formatCtx, NULL );
		if( ret < 0 )
			throw FFmpegException( "Find stream input error\n", ret );
		findStreamNumbers();
		m_audioCodec = std::make_unique<CodecData::CodecData>( m_formatCtx->streams[m_audioStreamNum]->codecpar );
		m_videoCodec = std::make_unique<CodecData::CodecData>( m_formatCtx->streams[m_videoStreamNum]->codecpar );
		m_audio = std::make_unique<Audio::Audio>( this, m_audioCodec->codecCtx, m_formatCtx, m_audioStreamNum );
		m_video = std::make_unique<Video::Video>( this, m_videoCodec->codecCtx, m_formatCtx, m_videoStreamNum );
		return true;
	}

	void FileReader::findStreamNumbers()
	{
		for( unsigned int i = 0; i < m_formatCtx->nb_streams; i++ )
		{
			if( m_formatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO )
				m_videoStreamNum = i;
			else if( m_formatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO )
				m_audioStreamNum = i;
			if( m_audioStreamNum != -1 && m_videoStreamNum != -1 )
				break;
		}
		if( m_videoStreamNum == -1 )
			throw std::exception( "Video stream not found" );
		if( m_audioStreamNum == -1 )
			throw std::exception( "Audio stream not found" );
	}
	void FileReader::resetFile()
	{
		avformat_seek_file( m_formatCtx, m_videoStreamNum, m_formatCtx->start_time, m_formatCtx->start_time, m_formatCtx->start_time, NULL );
	}

	bool FileReader::isFinished()
	{
		return m_video->m_videoQueue.size() == 0 && m_video->m_pictQ.size() == 0 && m_finish;
	}

	int FileReader::getFileReadingProgress()
	{
		return ( getCurrentPos() / (float) m_formatCtx->duration ) * 100;
	}

	void FileReader::stop()
	{
		m_audio->stop();
		stopDecoding();
		m_video->stopVideoThread();
		m_finish = false;
		clearQueues();
	}

	void FileReader::play()
	{
		startDecoding();
		m_video->startVideoThread();
		m_video->resetFrameDelay();
		m_audio->start();
	}

	void FileReader::pause()
	{
		m_audio->stop();
	}

	void FileReader::rewindRelative( int time )
	{
		int seekFlag = time < 0 ? AVSEEK_FLAG_BACKWARD : 0;
		int64_t videoPos, audioPos, currentPos;
		AVRational timeBaseQ{ 1, AV_TIME_BASE };
		currentPos = getCurrentPos();
		currentPos += time * AV_TIME_BASE;
		if( currentPos < m_formatCtx->duration && currentPos >= 0 )
		{
			m_audio->stop();
			pauseDecoding();
			m_video->pauseVideoThread();
			m_finish = false;
			clearQueues();
			videoPos = av_rescale_q( currentPos, timeBaseQ, m_formatCtx->streams[m_videoStreamNum]->time_base );
			audioPos = av_rescale_q( currentPos, timeBaseQ, m_formatCtx->streams[m_audioStreamNum]->time_base );
			int ret;
			if( m_formatCtx->streams[m_audioStreamNum] && ( ret = av_seek_frame( m_formatCtx, m_audioStreamNum, audioPos, seekFlag ) ) < 0 )
				throw FFmpegException( "Seek exception\n", ret );
			if( m_formatCtx->streams[m_videoStreamNum] && ( ret = av_seek_frame( m_formatCtx, m_videoStreamNum, videoPos, seekFlag ) ) < 0 )
				throw FFmpegException( "Seek exception\n", ret );
			avcodec_flush_buffers( m_videoCodec->codecCtx );
			avcodec_flush_buffers( m_audioCodec->codecCtx );
			m_audio->start();
			resumeDecoding();
			m_video->resumeVideoThread();
		}
	}

	void FileReader::rewindProgress( int progress )
	{
		m_audio->stop();
		pauseDecoding();
		m_video->pauseVideoThread();
		m_finish = false;
		clearQueues();
		int64_t videoPos, audioPos;
		AVRational timeBaseQ{ 1, AV_TIME_BASE };
		int64_t time = (m_formatCtx->duration * progress) / 100;
		avcodec_flush_buffers( m_videoCodec->codecCtx );
		avcodec_flush_buffers( m_audioCodec->codecCtx );
		videoPos = av_rescale_q( time, timeBaseQ, m_formatCtx->streams[m_videoStreamNum]->time_base );
		audioPos = av_rescale_q( time, timeBaseQ, m_formatCtx->streams[m_audioStreamNum]->time_base );
		int ret;
		if( m_formatCtx->streams[m_audioStreamNum] && (ret = av_seek_frame( m_formatCtx, m_audioStreamNum, audioPos, NULL )) > 0 )
			throw FFmpegException( "Seek exception\n", ret );
		if( m_formatCtx->streams[m_videoStreamNum] && (ret = av_seek_frame( m_formatCtx, m_videoStreamNum, videoPos, NULL )) > 0 )
			throw FFmpegException( "Seek exception\n", ret );
		m_audio->start();
		resumeDecoding();
		m_video->resumeVideoThread();
	}

	bool Player::FileReader::startDecoding()
	{
		if( !m_decodeActive )
		{
			if( m_decodeThread.joinable() )
				m_decodeThread.join();
			m_decodeActive = true;
			m_quitFlag = false;
			m_decodeThread = std::thread( &FileReader::decodeThread, this );
		}
		else
			resumeDecoding();
		return true;
	}

	bool FileReader::stopDecoding()
	{
		m_quitFlag = true;
		m_cond.notify_one();
		if( m_decodeThread.joinable() )
			m_decodeThread.join();
		return true;
	}

	bool FileReader::pauseDecoding()
	{
		m_decodePause = true;
		return true;
	}

	bool FileReader::resumeDecoding()
	{
		m_decodePause = false;
		m_cond.notify_one();
		return true;
	}

	bool FileReader::notifyDecoding()
	{
		m_cond.notify_one();
		return true;
	}

	void FileReader::clearQueues()
	{
		m_video->m_pictQ.clear();
		m_video->m_videoQueue.clear();
		m_audio->m_audioQueue.clear();
	}

	void FileReader::decodeThread()
	{
		AVPacket* packet = av_packet_alloc();
		if( packet == NULL )
			throw std::exception( "Could not alloc packet.\n" );
		while( !m_quitFlag )
		{
			std::unique_lock<std::mutex> lock( m_threadMutex );//m_audio->m_audioQueue.size() <= MAX_AUDIOQ_SIZE && m_video->m_videoQueue.size() <= MAX_VIDEOQ_SIZE)
			m_cond.wait( lock, [&] { return true || m_quitFlag || m_decodePause; } );
			if( m_quitFlag )
				break;
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
				m_audio->m_audioQueue.put( packet );
			else if( packet->stream_index == m_videoStreamNum )
				m_video->m_videoQueue.put( packet );
			else
				av_packet_unref( packet );
		}
		av_packet_free( &packet );
		m_decodeActive = false;
	}

	bool FileReader::updateTextureFromFrame( SDL_Texture* texture, AVFrame* frame )
	{
		float aspect_ratio;
		int w, h, x, y;
		int screen_height, screen_width;
		if( frame )
		{
				if( m_videoCodec->codecCtx->sample_aspect_ratio.num == 0 )
				{
					aspect_ratio = 0;
				}
				else
				{
					aspect_ratio = av_q2d( m_videoCodec->codecCtx->sample_aspect_ratio ) * m_videoCodec->codecCtx->width / m_videoCodec->codecCtx->height;
				}

				if( aspect_ratio <= 0.0 )
				{
					aspect_ratio = (float)m_videoCodec->codecCtx->width / (float)m_videoCodec->codecCtx->height;
				}

				int screen_width = m_videoCodec->codecCtx->width / 2;
				int screen_height = (m_videoCodec->codecCtx->height / 2);
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
				m_videoCodec->codecCtx->frame_number,
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
			m_textureMutex.lock();
			SDL_UpdateYUVTexture(
				texture,
				&rect,
				frame->data[0],
				frame->linesize[0],
				frame->data[1],
				frame->linesize[1],
				frame->data[2],
				frame->linesize[2]
			);
			m_textureMutex.unlock();
			return true;
		}
		return false;
	}

	void FileReader::getVideoSize( int& w, int& h )
	{
		if( m_videoCodec )
		{
			w = m_videoCodec->codecCtx->width;
			h = m_videoCodec->codecCtx->height;
		}
	}

	bool FileReader::updateTextureAndGetDelay( SDL_Texture* texture, int& delay )
	{
		if( m_video->m_pictQ.size() == 0 )
		{
			delay = 1;
			return false;
		}
		else
		{
			VideoPicture pict{ m_video->m_pictQ.getPicture( true ) };
			double realDelay = m_video->getRealDelay( pict, m_audio->getAudioClock() );
			if( realDelay == -1 )
			{
				delay = 1;
				return false;
			}
			delay = ( (int)(realDelay * 1000 + 0.5) );
			printf( "Next Scheduled Refresh:\t%f\n\n", (double)(realDelay * 1000 + 0.5) );
			updateTextureFromFrame( texture, pict.frame.get() );
			if( m_video->m_pictQ.size() <= 10 )
				m_video->notifyVideoThread();
			if( m_audio->m_audioQueue.size() <= MIN_AUDIOQ_SIZE || m_video->m_videoQueue.size() <= MIN_VIDEOQ_SIZE )
				notifyDecoding();
			return true;
		}
	}
	
	int64_t FileReader::getCurrentPos()
	{
		return static_cast<int64_t>( m_audio->getAudioClock() * AV_TIME_BASE );
	}
}