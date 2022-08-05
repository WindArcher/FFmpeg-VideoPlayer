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
#include <mutex>
#include <condition_variable>

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
    
    class FileReader
    {
    public:
        FileReader() = default;
        ~FileReader();
        bool open( const std::string& filename );
        void stop();
        void play();
        void pause();
        void rewindRelative( int time );
        void rewindProgress( int progress );
        bool startDecoding();
        bool stopDecoding();
        bool pauseDecoding();
        bool resumeDecoding();
        bool notifyDecoding();
        void resetFile();
        bool isFinished();
        int getFileReadingProgress();
        void getVideoSize( int& w, int& h );
        bool updateTextureAndGetDelay( SDL_Texture* texture, int& delay );
    private:
        AVFormatContext* m_formatCtx;
        std::mutex m_textureMutex, m_threadMutex;
        AVFrame* m_tempFrame;
        int m_videoStreamNum = -1, m_audioStreamNum = -1;
        std::unique_ptr<CodecData::CodecData> m_videoCodec, m_audioCodec;
        std::unique_ptr<Audio::Audio> m_audio;
        std::unique_ptr<Video::Video> m_video;
        std::thread m_decodeThread;
        std::condition_variable m_cond;
        bool m_finish = false, m_decodePause = false, m_decodeActive = false, m_quitFlag = false;
        void clearQueues();
        void findStreamNumbers();
        void decodeThread();
        bool updateTextureFromFrame( SDL_Texture* texture, AVFrame* frame );
        int64_t getCurrentPos();
    };
}
