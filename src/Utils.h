#pragma once
#include <stdexcept>
#include "Player.h"
#define ERROR_SIZE 128
namespace Utils
{
	void displayException( const char* message );
	void displayFFMpegException( int error_code );
}