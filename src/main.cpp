#include <iostream>

#include "Window/window.h"
#include "Exceptions/sdl_exception.h"
#include "Exceptions/ffmpeg_exception.h"

#include <windows.h>

int main()
{
    try
    {
        
        OPENFILENAME ofn;       // common dialog box structure
        TCHAR szFile[260] = { 0 };       // if using TCHAR macros
        ZeroMemory( &ofn, sizeof( ofn ) );
        ofn.lStructSize = sizeof( ofn );
        ofn.hwndOwner = NULL;
        ofn.lpstrFile = szFile;
        ofn.nMaxFile = sizeof( szFile );
        ofn.lpstrFilter = LPCWSTR( "All\0*.*\0Text\0*.TXT\0" );
        ofn.nFilterIndex = 1;
        ofn.lpstrFileTitle = NULL;
        ofn.nMaxFileTitle = 0;
        ofn.lpstrInitialDir = NULL;
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

        if( GetOpenFileName( &ofn ) == TRUE )
        {
            Window::Window inter;
            char buffer[500];
            wcstombs( buffer, ofn.lpstrFile, sizeof( buffer ) );
            std::wstring ws( ofn.lpstrFile );
            std::string myVarS = std::string( ws.begin(), ws.end() );
            inter.openVideo( buffer );
            inter.SDLLoop();
        }
        
    }
    catch( std::exception& e )
    {
        std::cerr << e.what();
    }
    catch( FFmpegException& e )
    {
        std::cerr << e.what();
    }
    catch( SDLException& e )
    {
        std::cerr << e.what();
    }
}