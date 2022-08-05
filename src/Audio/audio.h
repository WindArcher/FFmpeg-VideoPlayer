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
#include <thread>
#include <stdexcept>
#include <limits>
#include <functional>

#include "Queues/packet_queue.h"
namespace Player
{
    namespace Audio
    {
        void AudioCallback( void* userdata, Uint8* stream, int len );
        class Audio
        {
        public:
            Audio( AVCodecContext* codecContext, AVFormatContext* formatCtx, int steamNum );
            ~Audio() = default;
            void start();
            void stop();
            double getAudioClock();
            PacketQueue m_audioQueue;
        private:
            static constexpr auto m_maxAudioFrameSize = 192000;
            SDL_AudioSpec m_wanted_specs;
            SDL_AudioSpec m_specs;
            uint8_t m_audioBuf[(m_maxAudioFrameSize * 3) / 2]{ 0 };
            int m_audioBufSize = 0;
            int m_audioBufIndex = 0;
            AVCodecContext* m_audioContext;
            AVStream* m_audioStream = nullptr;
            double m_audioClock{ 0.0 };
            int decodeFrame( AVCodecContext* codecContext, uint8_t* audioBuffer, int audioBufferSize );
            int audioResampling( AVFrame* decoded_audio_frame, enum AVSampleFormat out_sample_fmt, uint8_t* out_buf );
            friend void AudioCallback( void* codecContext, Uint8* buffer, int bufferSize );
        };
    }
}