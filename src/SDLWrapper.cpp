#include "SDLWrapper.h"
#include "SDLException.h"

bool SDLWrapper::m_init = false;
void SDLWrapper::initSdl()
{
	if( !m_init )
	{
		if( SDL_Init( SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER ) )
			throw SDLException( "There is something wrong with your SDL Libs. Couldn't run\n" );
#ifdef _WIN32
		SDL_AudioInit( "directsound" );
#endif
		m_init = true;
		//atexit( SDL_Quit );
	}
}