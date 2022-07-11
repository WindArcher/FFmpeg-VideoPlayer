#pragma once
extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/avstring.h>
#include <libavutil/time.h>
#include <libavutil/opt.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}
#include <string>
#include <thread>
#include <mutex>
#include "SDLWrapper.h"
#include "PacketQueue.h"
#include "Audio.h"
#include "Video.h"

#define FF_QUIT_EVENT (SDL_USEREVENT + 1)
#define FF_REFRESH_EVENT (SDL_USEREVENT)
struct SDLDisplay
{
    SDL_Window* screen = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_Texture* texture = nullptr;
    std::mutex mutex;
};
struct CodecData
{
    AVCodecParameters* params = nullptr;
    AVCodecContext* codecCtx = nullptr;
    AVCodec* codec = nullptr;
};
class Player
{
public:
    static Player* getInstance();
    void run( const std::string& filename );
    void sheduleRefresh( int delay );
    void displayFrame( AVFrame* frame );
    void quit();
    void clear();
private:
    Player();
    static Player* m_instance;
    std::string m_filename;
    std::string m_windowName = "SDLPlayer";
    int m_videoStreamNum = -1, m_audioStreamNum = -1;
    AVFormatContext* m_formatCtx = nullptr;
    CodecData m_audioCodec,m_videoCodec;
    SDLDisplay m_display;
    bool m_quitFlag;
    std::thread m_decodeThread;
    void open();
    void findStreamNumbers();
    void getCodecParams();
    void readCodec( CodecData& codecData );
    void createDisplay();
    void decodeThread();
    void SDLEventLoop();
    static Uint32 SDLRefreshTimer( Uint32 interval, void* );    
};
