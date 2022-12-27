#include <iostream>

#include "Window/window.h"
#include "Exceptions/sdl_exception.h"
#include "Exceptions/ffmpeg_exception.h"

#include <windows.h>

int main()
{
    try
    {
        
        OPENFILENAME ofn;
        TCHAR szFile[260] = { 0 };
        ZeroMemory( &ofn, sizeof( ofn ) );
        ofn.lStructSize = sizeof( ofn );
        ofn.hwndOwner = NULL;
        ofn.lpstrFile = szFile;
        ofn.nMaxFile = sizeof( szFile );
        ofn.lpstrFilter = L"Video files (*.mp4;*.avi;*.mkv)\0*.mp4;*.avi;*.mkv\0""All files\0*.*\0";
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
}