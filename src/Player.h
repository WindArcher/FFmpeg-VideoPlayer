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
#include <memory>
#include "audio.h"
#include "video.h"
#include "decode_thread_handler.h"
#include "sdl_wrapper.h"

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

class Player : public DecodeThreadHandler
{
public:
    Player();
    ~Player();
    void run( const std::string& filename );
    virtual bool startThread();
    virtual bool stopThread();
private:
    std::string m_filename;
    std::string m_windowName = "SDLPlayer";
    int m_videoStreamNum = -1, m_audioStreamNum = -1;
    AVFormatContext* m_formatCtx = nullptr;
    CodecData m_audioCodec,m_videoCodec;
    SDLDisplay m_display;
    std::unique_ptr<Video> m_video;
    std::unique_ptr<Audio> m_audio;
    bool m_quitFlag = false, m_decodeFinished = false, m_pause = false, m_init = false, m_finish = false;
    std::thread m_decodeThread;
    SDL_TimerID m_timer;
    void open();
    void findStreamNumbers();
    void getCodecParams();
    void readCodec( CodecData& codecData );
    void createDisplay();
    void decodeThread();
    void refreshVideo();
    void stopDisplay();
    void startDisplay();
    void SDLEventLoop();
    void sheduleRefresh( int delay );
    void displayFrame( AVFrame* frame );
    void quit();
    int64_t getCurrentPos();
    void streamSeak( int shift );
    static Uint32 SDLRefreshTimer( Uint32 interval, void* );    
};
