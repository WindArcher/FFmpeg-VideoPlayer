#include "AudioStream.h"
#include <assert.h>

struct AudioResapmling
{
    AudioResapmling( int64_t inChannelLayout );
    SwrContext* swr_ctx;
    int64_t in_channel_layout;
    uint64_t out_channel_layout;
    int out_nb_channels;
    int out_linesize;
    int in_nb_samples;
    int64_t out_nb_samples;
    int64_t max_out_nb_samples;
    uint8_t** resampled_data;
    int resampled_data_size;
};

AudioResapmling::AudioResapmling( int64_t inChannelLayout )
{
    swr_ctx = swr_alloc();
    in_channel_layout = inChannelLayout;
    out_channel_layout = AV_CH_LAYOUT_STEREO;
    out_nb_channels = 0;
    out_linesize = 0;
    in_nb_samples = 0;
    out_nb_samples = 0;
    max_out_nb_samples = 0;
    resampled_data = NULL;
    resampled_data_size = 0;
}

AudioStream::AudioStream( )
{

}

AudioStream::~AudioStream()
{

}

bool AudioStream::findAudioStreamIndex( AVFormatContext* formatCtx )
{
    for( int i = 0; i < formatCtx->nb_streams; i++ )
    {
        if( formatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && m_index < 0 )
        {
            m_index = i;
            return true;
        } 
    }
    return false;
}

int AudioStream::getIndex()
{
    return m_index;
}

bool AudioStream::addToQueue( AVPacket* packet )
{
    return m_audioQueue.put( packet );
}

double AudioStream::getAudioClock()
{
    int pts = m_audioClock;
    if( m_audioStream )
    {
        int hwBufferSize = m_audioBufferSize - m_audioBufferIndex;
        int n = 2 * m_audioCtx->channels;
        int bytesPerSecond = m_audioCtx->sample_rate * n;
        if( bytesPerSecond )
        {
            pts -= (double)hwBufferSize / bytesPerSecond;
        }
    }
    return pts;
}

bool AudioStream::openStream( AVFormatContext* formatCtx )
{
    if( m_index < 0 || m_index >= formatCtx->nb_streams )
    {
        std::cout << "Invalid stream index.\n";
        return false;
    }
    AVCodec* codec = NULL;
    codec = avcodec_find_decoder( formatCtx->streams[m_index]->codecpar->codec_id );
    if( codec == NULL )
    {
        std::cout << "Unsupported codec.\n";
        return false;
    }

    AVCodecContext* codecCtx = NULL;
    codecCtx = avcodec_alloc_context3( codec );
    if( avcodec_parameters_to_context( codecCtx, formatCtx->streams[m_index]->codecpar ) != 0 )
    {
        printf( "Could not copy codec context.\n" );
        return false;
    }

    if( codecCtx->codec_type == AVMEDIA_TYPE_AUDIO )
    {
        SDL_AudioSpec wanted_specs;
        SDL_AudioSpec specs;
        wanted_specs.freq = codecCtx->sample_rate;
        wanted_specs.format = AUDIO_S16SYS;
        wanted_specs.channels = codecCtx->channels;
        wanted_specs.silence = 0;
        wanted_specs.samples = SDL_AUDIO_BUFFER_SIZE;
        wanted_specs.callback = AudioCallback;
        wanted_specs.userdata = this;
        if( SDL_OpenAudio( &wanted_specs, &specs ) < 0 )
        {
            std::cout << "SDL_OpenAudio: %s.\n", SDL_GetError();
            return false;
        }
    }
    else
    {
        std::cout << "Not audio codec context type.\n";
        return false;
    }
    if( avcodec_open2( codecCtx, codec, NULL ) < 0 )
    {
        std::cout << "Unsupported codec.\n";
        return false;
    }
    m_audioStream = formatCtx->streams[m_index];
    m_audioCtx = codecCtx;
    memset( &m_packet, 0, sizeof( m_packet ) );
    SDL_PauseAudio( 0 );
    return true;
}

int audioResample( AudioStream* audioStream, AVFrame* decodedFrame, AVSampleFormat outputFormat )
{
    AudioResapmling* arState = new AudioResapmling( audioStream->m_audioCtx->channel_layout );
    if( !arState->swr_ctx )
    {
        std::cout << "swr_alloc error.\n";
        return -1;
    }
    arState->in_channel_layout = (audioStream->m_audioCtx->channels ==
        av_get_channel_layout_nb_channels( audioStream->m_audioCtx->channel_layout )) ?
        audioStream->m_audioCtx->channel_layout :
        av_get_default_channel_layout( audioStream->m_audioCtx->channels );

    if( arState->in_channel_layout <= 0 )
    {
        std::cout << "in_channel_layout error.\n";
        return -1;
    }
    if( audioStream->m_audioCtx->channels == 1 )
    {
        arState->out_channel_layout = AV_CH_LAYOUT_MONO;
    }
    else if( audioStream->m_audioCtx->channels == 2 )
    {
        arState->out_channel_layout = AV_CH_LAYOUT_STEREO;
    }
    else
    {
        arState->out_channel_layout = AV_CH_LAYOUT_SURROUND;
    }
    arState->in_nb_samples = decodedFrame->nb_samples;
    if( arState->in_nb_samples <= 0 )
    {
        printf( "in_nb_samples error.\n" );
        return -1;
    }
    av_opt_set_int(   
        arState->swr_ctx,
        "in_channel_layout",
        arState->in_channel_layout,
        0
    );

    av_opt_set_int(
        arState->swr_ctx,
        "in_sample_rate",
        audioStream->m_audioCtx->sample_rate,
        0
    );
    av_opt_set_sample_fmt(
        arState->swr_ctx,
        "in_sample_fmt",
        audioStream->m_audioCtx->sample_fmt,
        0
    );

    // Set SwrContext parameters for resampling
    av_opt_set_int(
        arState->swr_ctx,
        "out_channel_layout",
        arState->out_channel_layout,
        0
    );

    // Set SwrContext parameters for resampling
    av_opt_set_int(
        arState->swr_ctx,
        "out_sample_rate",
        audioStream->m_audioCtx->sample_rate,
        0
    );

    // Set SwrContext parameters for resampling
    av_opt_set_sample_fmt(
        arState->swr_ctx,
        "out_sample_fmt",
        outputFormat,
        0
    );
    int ret = swr_init( arState->swr_ctx );;
    if( ret < 0 )
    {
        printf( "Failed to initialize the resampling context.\n" );
        return -1;
    }

    arState->max_out_nb_samples = arState->out_nb_samples = av_rescale_rnd(
        arState->in_nb_samples,
        audioStream->m_audioCtx->sample_rate,
        audioStream->m_audioCtx->sample_rate,
        AV_ROUND_UP
    );

    // check rescaling was successful
    if( arState->max_out_nb_samples <= 0 )
    {
        std::cout << "av_rescale_rnd error.\n";
        return -1;
    }

    // get number of output audio channels
    arState->out_nb_channels = av_get_channel_layout_nb_channels( arState->out_channel_layout );

    // allocate data pointers array for arState->resampled_data and fill data
    // pointers and linesize accordingly
    ret = av_samples_alloc_array_and_samples(
        &arState->resampled_data,
        &arState->out_linesize,
        arState->out_nb_channels,
        arState->out_nb_samples,
        outputFormat,
        0
    );
    if( ret < 0 )
    {
        printf( "av_samples_alloc_array_and_samples() error: Could not allocate destination samples.\n" );
        return -1;
    }

    // retrieve output samples number taking into account the progressive delay
    arState->out_nb_samples = av_rescale_rnd(
        swr_get_delay( arState->swr_ctx, audioStream->m_audioCtx->sample_rate ) + arState->in_nb_samples,
        audioStream->m_audioCtx->sample_rate,
        audioStream->m_audioCtx->sample_rate,
        AV_ROUND_UP
    );

    // check output samples number was correctly rescaled
    if( arState->out_nb_samples <= 0 )
    {
        printf( "av_rescale_rnd error\n" );
        return -1;
    }

    if( arState->out_nb_samples > arState->max_out_nb_samples )
    {
        // free memory block and set pointer to NULL
        av_free( arState->resampled_data[0] );

        // Allocate a samples buffer for out_nb_samples samples
        ret = av_samples_alloc(
            arState->resampled_data,
            &arState->out_linesize,
            arState->out_nb_channels,
            arState->out_nb_samples,
            outputFormat,
            1
        );

        // check samples buffer correctly allocated
        if( ret < 0 )
        {
            printf( "av_samples_alloc failed.\n" );
            return -1;
        }

        arState->max_out_nb_samples = arState->out_nb_samples;
    }

    if( arState->swr_ctx )
    {
        // do the actual audio data resampling
        ret = swr_convert(
            arState->swr_ctx,
            arState->resampled_data,
            arState->out_nb_samples,
            (const uint8_t**)decodedFrame->data,
            decodedFrame->nb_samples
        );

        // check audio conversion was successful
        if( ret < 0 )
        {
            printf( "swr_convert_error.\n" );
            return -1;
        }

        // get the required buffer size for the given audio parameters
        arState->resampled_data_size = av_samples_get_buffer_size(
            &arState->out_linesize,
            arState->out_nb_channels,
            ret,
            outputFormat,
            1
        );
        if( arState->resampled_data_size < 0 )
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
    // copy the resampled data to the output buffer
    memcpy( audioStream->m_audioBuffer, arState->resampled_data[0], arState->resampled_data_size );

    /*
     * Memory Cleanup.
     */
    if( arState->resampled_data )
    {
        // free memory block and set pointer to NULL
        av_freep( &arState->resampled_data[0] );
    }

    av_freep( &arState->resampled_data );
    arState->resampled_data = NULL;

    if( arState->swr_ctx )
    {
        // free the allocated SwrContext and set the pointer to NULL
        swr_free( &arState->swr_ctx );
    }

    int dataSize = arState->resampled_data_size;
    delete arState;
    return dataSize;
}

unsigned int AudioDecodeFrame( AudioStream* audioStream )
{
    AVPacket* avPacket = av_packet_alloc();
    double pts;
    static uint8_t* audio_pkt_data = NULL;
    static int audio_pkt_size = 0;
    static AVFrame* frame = av_frame_alloc();
    if( !frame )
    {
        std::cout << "Could not allocate AVFrame.\n";
        return -1;
    }    
    int len1 = 0, dataSize = 0;
    while( true )
    {
        if( g_quitFlag )
        {
            return -1;
        }
        while( audio_pkt_size  > 0 )
        {
            int got_frame = 0;
            int ret = avcodec_receive_frame( audioStream->m_audioCtx, frame );
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
                ret = avcodec_send_packet( audioStream->m_audioCtx, avPacket );
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
                dataSize = audioResample(
                    audioStream,
                    frame,
                    AV_SAMPLE_FMT_S16
                );

                assert( dataSize <= sizeof( audioStream->m_audioBuffer ) );
            }
            pts = audioStream->m_audioClock;
            int n = 2 * audioStream->m_audioCtx->channels;
            audioStream->m_audioClock += (double)dataSize / (double)(n * audioStream->m_audioCtx->sample_rate);

            // we have the data, return it and come back for more later
            return dataSize;
        }
        if( avPacket->data )
        {
            av_packet_unref( avPacket );
        }
        int ret = audioStream->m_audioQueue.get( avPacket, 1 );
        if( ret < 0 )
        {
            return -1;
        }
        audio_pkt_data = avPacket->data;
        audio_pkt_size = avPacket->size;
        if( avPacket->pts != AV_NOPTS_VALUE )
        {
            audioStream->m_audioClock = av_q2d( audioStream->m_audioStream->time_base ) * avPacket->pts;
        }
    } 
    //av_frame_free( &frame );
    return 0;
}

void AudioCallback( void* userdata, Uint8* stream, int len )
{
    AudioStream* audioStream = reinterpret_cast<AudioStream*>( userdata);
    int len1 = -1;
    unsigned int audioSize = -1;
    while( len > 0 )
    {
        if( g_quitFlag )
            return;
        if( audioStream->m_audioBufferIndex >= audioStream->m_audioBufferSize )
            audioSize = AudioDecodeFrame( audioStream );
        if( audioSize < 0 )
        {
            audioStream->m_audioBufferSize = 1024;
            memset( audioStream->m_audioBuffer, 0, audioStream->m_audioBufferSize );
            std::cout << "audio_decode_frame() failed.\n";
        }
        else
        {
            audioStream->m_audioBufferSize = audioSize;
        }
        audioStream->m_audioBufferIndex = 0;
    }
    len1 = audioStream->m_audioBufferSize - audioStream->m_audioBufferIndex;
    if( len1 > len )
    {
        len1 = len;
    }
    memcpy( stream, (uint8_t*)audioStream->m_audioBuffer + audioStream->m_audioBufferIndex, len1 );
    len -= len1;
    stream += len1;
    audioStream->m_audioBufferIndex += len1;
}
