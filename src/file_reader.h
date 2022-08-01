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
#include <SDL2/SDL.h>

#include <memory>
#include <string>
#include <thread>
#include <memory>

#include "Interfaces/player.h"
#include "Interfaces/decode_thread_handler.h"
#include "Video/video.h"
#include "Audio/audio.h"
namespace Player
{
    namespace CodecData
    {
        struct CodecData
        {
            CodecData( AVCodecParameters* params );
            ~CodecData();
            AVCodecParameters* params = nullptr;
            AVCodecContext* codecCtx = nullptr;
            AVCodec* codec = nullptr;
        };
    }
    
    class FileReader : public IPlayer, public IDecodeThreadHandler
    {
    public:
        FileReader() = default;
        ~FileReader();
        bool open( const std::string& filename ) override;
        void stop() override;
        void play() override;
        void pause() override;
        void rewindRelative( int time ) override;
        void rewindProgress( int progress ) override;
        bool startDecoding() override;
        bool stopDecoding() override;
        void resetFile();
        bool isFinished();
        int getFileReadingProgress();
        void getVideoSize( int& w, int& h );
        bool updateTextureAndGetDelay( SDL_Texture* texture, int& delay );
    private:
        AVFormatContext* m_formatCtx;
        std::mutex m_mutex;
        AVFrame* m_tempFrame;
        int m_videoStreamNum = -1, m_audioStreamNum = -1;
        std::unique_ptr<CodecData::CodecData> m_videoCodec, m_audioCodec;
        std::unique_ptr<Audio::Audio> m_audio;
        std::unique_ptr<Video::Video> m_video;
        std::thread m_decodeThread;
        bool m_finish = false, m_threadInit = false, m_decodeFinished = false, m_quitFlag = false;
        void clearQueues();
        void findStreamNumbers();
        void decodeThread();
        bool updateTextureFromFrame( SDL_Texture* texture, AVFrame* frame );
        int64_t getCurrentPos();
    };
}
