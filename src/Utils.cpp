#include "Utils.h"
namespace Utils
{
	void displayException( const char* message )
	{
		Player::getInstance()->quit();
		Player::getInstance()->clear();
		SDL_Quit();
		throw std::runtime_error( message );
	}

	void displayFFMpegException( int error_code )
	{
		char error_buf[ERROR_SIZE];
		av_strerror( error_code, error_buf, ERROR_SIZE );
		displayException( error_buf );
	}
}