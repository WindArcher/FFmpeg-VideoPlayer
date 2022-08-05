#include "audio.h"
#include <functional>
#include <assert.h>
#include "Exceptions/sdl_exception.h"
namespace Player
{
    namespace Audio
    {
        static constexpr auto SDL_AUDIO_BUFFER_SIZE = 1024;

        Audio::Audio( AVCodecContext* codecContext, AVFormatContext* formatCtx, int steamNum )
        {
            m_audioStream = formatCtx->streams[steamNum];
            m_audioContext = codecContext;
            m_wanted_specs.freq = codecContext->sample_rate;
            m_wanted_specs.format = AUDIO_S16SYS;
            m_wanted_specs.channels = codecContext->channels;
            m_wanted_specs.silence = 0;
            m_wanted_specs.samples = SDL_AUDIO_BUFFER_SIZE;
            m_wanted_specs.callback = AudioCallback;
            m_wanted_specs.userdata = this;
            int ret = SDL_OpenAudio( &m_wanted_specs, &m_specs );
            if( ret < 0 )
                throw SDLException( "SDL_OpenAudio func.\n" );
        }

        void Audio::start()
        {
            SDL_PauseAudio( 0 );
        }

        void Audio::stop()
        {
            SDL_PauseAudio( 1 );
        }

        double Audio::getAudioClock()
        {
            double pts = m_audioClock;
            int hwBuffSize = m_audioBufSize - m_audioBufIndex;
            int bytesPerSecond = 0;
            int n = 2 * m_audioContext->channels;
            if( m_audioStream )
            {
                bytesPerSecond = m_audioContext->sample_rate * n;
                if( bytesPerSecond )
                {
                    pts -= (double)hwBuffSize / bytesPerSecond;
                }
            }
            return pts;
        }


        int Audio::decodeFrame( AVCodecContext* codecContext, uint8_t* audioBuffer, int audioBufferSize )
        {
            AVPacket* avPacket = av_packet_alloc();
            if( !avPacket )
                throw std::exception( "Cannot allocate packet" );
            double pts;
            static uint8_t* audio_pkt_data = NULL;
            static int audio_pkt_size = 0;
            static AVFrame* frame = av_frame_alloc();
            if( !frame )
            {
                av_packet_free( &avPacket );
                throw std::exception( "Cannot allocate frame" );
            }
            int len1 = 0, dataSize = 0;
            while( true )
            {
                while( audio_pkt_size > 0 )
                {
                    int got_frame = 0;
                    int ret = avcodec_receive_frame( codecContext, frame );
                    if( ret == 0 )
                    {
                        got_frame = 1;
                    }
                    if( ret == AVERROR( EAGAIN ) )
                    {
                        ret = 0;
                    }
                    if( ret == 0 )
                    {
                        ret = avcodec_send_packet( codecContext, avPacket );
                    }
                    if( ret == AVERROR( EAGAIN ) )
                    {
                        ret = 0;
                    }
                    else if( ret < 0 )
                    {
                        printf( "avcodec_receive_frame error\n" );
                        return -1;
                    }
                    else
                    {
                        len1 = avPacket->size;
                    }

                    if( len1 < 0 )
                    {
                        audio_pkt_size = 0;
                        break;
                    }
                    audio_pkt_data += len1;
                    audio_pkt_size -= len1;
                    dataSize = 0;

                    if( got_frame )
                    {
                        dataSize = audioResampling( frame, AV_SAMPLE_FMT_S16, audioBuffer );
                        assert( dataSize <= sizeof( m_audioBuf ) );
                    }
                    pts = m_audioClock;
                    int n = 2 *codecContext->channels;
                    m_audioClock += (double)dataSize / (double)(n * codecContext->sample_rate);
                    return dataSize;
                }
                if( avPacket->data )
                {
                    av_packet_unref( avPacket );
                }
                avPacket = m_audioQueue.get( 1 );
                if( avPacket == nullptr )
                {
                    return -1;
                }
                audio_pkt_data = avPacket->data;
                audio_pkt_size = avPacket->size;
                if( avPacket->pts != AV_NOPTS_VALUE )
                {
                    m_audioClock = av_q2d( m_audioStream->time_base ) * avPacket->pts;
                }
            }
            av_frame_free( &frame );
            return 0;
        }

        void AudioCallback( void* audio, Uint8* buffer, int bufferSize )
        {
            Audio* pAudio = reinterpret_cast<Audio*>(audio);
            int len1 = -1;
            unsigned int audioSize = -1;
            double pts;
            while( bufferSize > 0 )
            {
                if( pAudio->m_audioBufIndex >= pAudio->m_audioBufSize )
                {
                    audioSize = pAudio->decodeFrame( pAudio->m_audioContext, pAudio->m_audioBuf, pAudio->m_audioBufSize );
                    if( audioSize < 0 )
                    {
                        pAudio->m_audioBufSize = 1024;
                        memset( pAudio->m_audioBuf, 0, pAudio->m_audioBufSize );
                        printf( "audio_decode_frame() failed.\n" );
                    }
                    else
                        pAudio->m_audioBufSize = audioSize;
                    pAudio->m_audioBufIndex = 0;
                }
                len1 = pAudio->m_audioBufSize - pAudio->m_audioBufIndex;
                if( len1 > bufferSize )
                    len1 = bufferSize;
                memcpy( buffer, (uint8_t*)pAudio->m_audioBuf + pAudio->m_audioBufIndex, len1 );
                bufferSize -= len1;
                buffer += len1;
                pAudio->m_audioBufIndex += len1;
            }
        }

        int Audio::audioResampling( AVFrame* decoded_audio_frame, enum AVSampleFormat out_sample_fmt, uint8_t* out_buf )
        {
            SwrContext* swr_ctx = NULL;
            int ret = 0;
            int64_t in_channel_layout = m_audioContext->channel_layout;
            int64_t out_channel_layout = AV_CH_LAYOUT_STEREO;
            int out_nb_channels = 0;
            int out_linesize = 0;
            int in_nb_samples = 0;
            int out_nb_samples = 0;
            int max_out_nb_samples = 0;
            uint8_t** resampled_data = NULL;
            int resampled_data_size = 0;

            swr_ctx = swr_alloc();

            if( !swr_ctx )
            {
                printf( "swr_alloc error.\n" );
                return -1;
            }
            in_channel_layout = (m_audioContext->channels ==
                av_get_channel_layout_nb_channels( m_audioContext->channel_layout )) ?
                m_audioContext->channel_layout :
                av_get_default_channel_layout( m_audioContext->channels );
            if( in_channel_layout <= 0 )
            {
                printf( "in_channel_layout error.\n" );
                return -1;
            }
            if( m_audioContext->channels == 1 )
            {
                out_channel_layout = AV_CH_LAYOUT_MONO;
            }
            else if( m_audioContext->channels == 2 )
            {
                out_channel_layout = AV_CH_LAYOUT_STEREO;
            }
            else
            {
                out_channel_layout = AV_CH_LAYOUT_SURROUND;
            }
            in_nb_samples = decoded_audio_frame->nb_samples;
            if( in_nb_samples <= 0 )
            {
                printf( "in_nb_samples error.\n" );
                return -1;
            }
            av_opt_set_int(
                swr_ctx,
                "in_channel_layout",
                in_channel_layout,
                0
            );
            av_opt_set_int(
                swr_ctx,
                "in_sample_rate",
                m_audioContext->sample_rate,
                0
            );
            av_opt_set_sample_fmt(
                swr_ctx,
                "in_sample_fmt",
                m_audioContext->sample_fmt,
                0
            );
            av_opt_set_int(
                swr_ctx,
                "out_channel_layout",
                out_channel_layout,
                0
            );
            av_opt_set_int(
                swr_ctx,
                "out_sample_rate",
                m_audioContext->sample_rate,
                0
            );
            av_opt_set_sample_fmt(
                swr_ctx,
                "out_sample_fmt",
                out_sample_fmt,
                0
            );
            ret = swr_init( swr_ctx );;
            if( ret < 0 )
            {
                printf( "Failed to initialize the resampling context.\n" );
                return -1;
            }

            max_out_nb_samples = out_nb_samples = av_rescale_rnd(
                in_nb_samples,
                m_audioContext->sample_rate,
                m_audioContext->sample_rate,
                AV_ROUND_UP
            );
            if( max_out_nb_samples <= 0 )
            {
                printf( "av_rescale_rnd error.\n" );
                return -1;
            }

            out_nb_channels = av_get_channel_layout_nb_channels( out_channel_layout );

            ret = av_samples_alloc_array_and_samples(
                &resampled_data,
                &out_linesize,
                out_nb_channels,
                out_nb_samples,
                out_sample_fmt,
                0
            );

            if( ret < 0 )
            {
                printf( "av_samples_alloc_array_and_samples() error: Could not allocate destination samples.\n" );
                return -1;
            }

            out_nb_samples = av_rescale_rnd(
                swr_get_delay( swr_ctx, m_audioContext->sample_rate ) + in_nb_samples,
                m_audioContext->sample_rate,
                m_audioContext->sample_rate,
                AV_ROUND_UP
            );

            if( out_nb_samples <= 0 )
            {
                printf( "av_rescale_rnd error\n" );
                return -1;
            }

            if( out_nb_samples > max_out_nb_samples )
            {
                av_free( resampled_data[0] );
                ret = av_samples_alloc(
                    resampled_data,
                    &out_linesize,
                    out_nb_channels,
                    out_nb_samples,
                    out_sample_fmt,
                    1
                );
                if( ret < 0 )
                {
                    printf( "av_samples_alloc failed.\n" );
                    return -1;
                }

                max_out_nb_samples = out_nb_samples;
            }

            if( swr_ctx )
            {
                ret = swr_convert(
                    swr_ctx,
                    resampled_data,
                    out_nb_samples,
                    (const uint8_t**)decoded_audio_frame->data,
                    decoded_audio_frame->nb_samples
                );

                if( ret < 0 )
                {
                    printf( "swr_convert_error.\n" );
                    return -1;
                }
                resampled_data_size = av_samples_get_buffer_size(
                    &out_linesize,
                    out_nb_channels,
                    ret,
                    out_sample_fmt,
                    1
                );

                if( resampled_data_size < 0 )
                {
                    printf( "av_samples_get_buffer_size error.\n" );
                    return -1;
                }
            }
            else
            {
                printf( "swr_ctx null error.\n" );
                return -1;
            }

            memcpy( out_buf, resampled_data[0], resampled_data_size );

            if( resampled_data )
            {
                av_freep( &resampled_data[0] );
            }
            av_freep( &resampled_data );
            resampled_data = NULL;
            if( swr_ctx )
            {
                swr_free( &swr_ctx );
            }
            return resampled_data_size;
        }
    }
}