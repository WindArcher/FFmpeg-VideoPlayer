#include "SDLWrapper.h"
#include <iostream>
void SDLWrapper::initSdl()
{
	if( SDL_Init( SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER ) )
		std::cout << ("There is something wrong with your SDL Libs. Couldn't run\n");
#ifdef _WIN32
	SDL_AudioInit( "directsound" );
#endif
}

void SDLWrapper::openAudio( SDL_AudioSpec* desired, SDL_AudioSpec* obtained )
{
	if( SDL_OpenAudio( desired, obtained ) < 0 )
	 	std::cout << "Failed to open audio\n";
}
