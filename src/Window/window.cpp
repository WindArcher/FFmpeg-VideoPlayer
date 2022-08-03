#include "window.h"
#include "Exceptions/sdl_exception.h"

namespace Window
{
	Window::Window()
	{
		if( SDL_Init( SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER ) )
			throw SDLException( "SDL Init failed;" );
#ifdef _WIN32
		SDL_AudioInit( "directsound" );
#endif
		m_window = SDL_CreateWindow( "Interface", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, m_width, m_height, SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI );
		SDL_GL_SetSwapInterval( 1 );
		if( !m_window )
			throw std::exception( "Couldn't show display window" );
		m_renderer = SDL_CreateRenderer( m_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_TARGETTEXTURE );
		if( !m_renderer )
			throw SDLException( "Couldn't create renderer;" );
		SDL_SetRenderDrawColor( m_renderer, 255, 255, 255, 255 );
		m_buttonBarRect = { 0, (m_height * 9) / 10, m_width, (m_height) / 10 };
		m_buttonBar = std::make_unique<ButtonBar>( m_renderer, m_buttonBarRect );
		m_rewindRect = { 0, (m_height * 85) / 100, m_width, (m_height)* 5 / 100 };
		m_rewindBar = std::make_unique<RewindBar>( m_renderer, m_rewindRect );
		m_videoBarRect = { 0,0,m_width,(m_height) * 85 / 100 };
		m_videoBar = std::make_unique<VideoBar>( m_renderer, m_videoBarRect );
		Player::Events::registerEvents();
	}

	Window::~Window()
	{
		SDL_DestroyRenderer( m_renderer );
		SDL_DestroyWindow( m_window );
	}

	void Window::display()
	{
		countRects();
		SDL_RenderClear( m_renderer );
		m_buttonBar->draw();
		m_rewindBar->draw();
		m_videoBar->draw();
	}

	void Window::openVideo( const std::string& filename )
	{
		if( m_fileReader.open( filename ) )
		{
			int w, h;
			m_fileReader.getVideoSize( w, h );
			resizeWindow( w/2, (h / 2) * 1.17 ); // 
			m_videoBar->setTextureSize( w, h );
			m_fileReader.play();
			m_videoBar->sheduleRefresh( 39 );
		}
	}

	void Window::SDLLoop()
	{
		SDL_Event event;
		int i = 0;
		while( true )
		{
			SDL_Point point;
			SDL_GetMouseState( &point.x, &point.y );
			m_buttonBar->update( point );
			if( SDL_WaitEvent( &event ) == 0 )
			{
				printf( "SDL_WaitEvent failed: %s.\n", SDL_GetError() );
			}
			switch( event.type )
			{
			case SDL_MOUSEBUTTONUP:
			{
				if( SDL_PointInRect( &point, &m_buttonBarRect ) )
					m_buttonBar->clicked();
				else if( SDL_PointInRect( &point, &m_rewindRect ) )
					m_rewindBar->update( point );
				break;
			}
			case static_cast<Uint32>(Player::Events::Events::PAUSE_BUTTON_PUSHED):
			{
				m_fileReader.pause();
				m_pause = true;
				m_videoBar->killRefreshTimer();
				break;
			}
			case static_cast<Uint32>(Player::Events::Events::PLAY_BUTTON_PUSHED):
			{
				m_fileReader.play();
				m_pause = false;
				m_videoBar->sheduleRefresh(39 );
				break;
			}
			case static_cast<Uint32>(Player::Events::Events::STOP_BUTTON_PUSHED):
			{
				m_pause = true;
				m_videoBar->killRefreshTimer();
				m_fileReader.stop();
				m_fileReader.resetFile();
				m_buttonBar->setPauseMode( false );
				break;
			}
			case static_cast<Uint32>(Player::Events::Events::REWIND_FORWARD_BUTTON_PUSHED ):
			{
				m_pause = true;
				m_videoBar->killRefreshTimer();
				m_fileReader.rewindRelative( 60 );
				m_pause = false;
				m_videoBar->sheduleRefresh( 39 );
				break;
			}
			case static_cast<Uint32>(Player::Events::Events::REWIND_BACKWARD_BUTTON_PUSHED):
			{
				m_pause = true;
				m_videoBar->killRefreshTimer();
				m_fileReader.rewindRelative( -60 );
				m_pause = false;
				m_videoBar->sheduleRefresh( 39 );
				break;
			}
			case static_cast<Uint32>(Player::Events::Events::REFRESH_VIDEO):
			{
				if( m_fileReader.isFinished() )
				{
					m_videoBar->killRefreshTimer();
					m_pause = true;
					m_fileReader.stop();
				}
				if( !m_pause )
				{
					int delay;
					if( m_fileReader.updateTextureAndGetDelay( m_videoBar->getTexture(), delay ) )
						m_videoBar->updateVideoBar();
					m_rewindBar->updateProgres( m_fileReader.getFileReadingProgress() );
					m_videoBar->sheduleRefresh( delay );
				}
				break;
			}
			case static_cast<Uint32>(Player::Events::Events::REWIND_BAR_POINT_CHANGED):
			{
				m_pause = true;
				m_videoBar->killRefreshTimer();
				int progress = *reinterpret_cast<int*>( event.user.data1 );
				m_fileReader.rewindProgress( progress );
				m_pause = false;
				m_videoBar->sheduleRefresh( 39 );
				break;
			}
			case SDL_QUIT:
			{
				m_videoBar->killRefreshTimer();
				m_fileReader.stop();
				SDL_Quit();
				break;
			}
			}
			if( event.type == SDL_QUIT )
				break;
		}
	}

	void Window::countRects()
	{
		m_buttonBarRect = { 0, (m_height * 9) / 10, m_width, (m_height) / 10 };
		m_buttonBar = std::make_unique<ButtonBar>( m_renderer, m_buttonBarRect );
		m_rewindRect = { 0, (m_height * 85) / 100, m_width, (m_height) * 5 / 100 };
		m_rewindBar = std::make_unique<RewindBar>( m_renderer, m_rewindRect );
		m_videoBarRect = { 0,0,m_width,(m_height) * 85 / 100 };
		m_videoBar = std::make_unique<VideoBar>( m_renderer, m_videoBarRect );
	}

	void Window::resizeWindow( int w, int h )
	{
		m_width = w;
		m_height = h;
		SDL_SetWindowSize( m_window, m_width, m_height );
		display();
	}
}