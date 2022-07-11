#include "Audio.h"
#include <functional>
#include "Utils.h"
#include "AudioCallback.h"
#include <assert.h>
Audio* Audio::m_instance = 0;

Audio* Audio::getInstance()
{
    if( m_instance == nullptr )
        m_instance = new Audio();
    return m_instance;
}

Audio::~Audio()
{
    SDL_CloseAudioDevice( m_dev );
}

void Audio::init( AVCodecContext* codecContext, AVStream* stream )
{
    if( !m_init )
    {
        AudioCallback::setAudioInstance( this );
        m_audioStream = stream;
        m_audioContext = codecContext;
        
        m_wanted_specs.freq = codecContext->sample_rate;
        m_wanted_specs.format = AUDIO_S16SYS;
        m_wanted_specs.channels = codecContext->channels;
        m_wanted_specs.silence = 0;
        m_wanted_specs.samples = SDL_AUDIO_BUFFER_SIZE;
        m_wanted_specs.callback = AudioCallback;
        m_wanted_specs.userdata = codecContext;
        int ret = SDL_OpenAudio( &m_wanted_specs, &m_specs );
        if( ret < 0 )
        {
            printf( "SDL_OpenAudio: %s.\n", SDL_GetError() );
            return;
        }
        SDL_PauseAudio( 0 );
        m_init = true;
    }
}

void Audio::startPlaying()
{
    SDL_PauseAudioDevice( m_dev, 0 );
}

double Audio::getAudioClock( )
{
    double pts = m_audioClock;
    int hwBuffSize = audioBufSize - audioBufIndex;
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

static int audio_resampling(
    AVCodecContext* audio_decode_ctx,
    AVFrame* decoded_audio_frame,
    enum AVSampleFormat out_sample_fmt,
    int out_channels,
    int out_sample_rate,
    uint8_t* out_buf
);

int Audio::decodeFrame( AVCodecContext* codecContext, uint8_t* audioBuffer, int audioBufferSize )
{
    AVPacket* avPacket = av_packet_alloc();
    double pts;
    static uint8_t* audio_pkt_data = NULL;
    static int audio_pkt_size = 0;
    static AVFrame* frame = av_frame_alloc();
    if( !frame )
        Utils::displayException( "Cannot allocate frame" );
    int len1 = 0, dataSize = 0;
    while( true )
    {
        while( audio_pkt_size > 0 )
        {
            int got_frame = 0;
            int ret = avcodec_receive_frame(codecContext, frame );
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
                dataSize = audio_resampling( codecContext, frame, AV_SAMPLE_FMT_S16, codecContext->channels, codecContext->sample_rate, audioBuffer );
                assert( dataSize <= sizeof( audioBuf ) );
            }
            pts = m_audioClock;
            int n = 2 * codecContext->channels;
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


int Audio::resampleAudio( AVCodecContext* codecContext, AVFrame* decodedFrame, AVSampleFormat outFormat, uint8_t* outBuffer )
{
    SwrContext* swrCtx = swr_alloc();
    int64_t inChannelLayout = 0;
    int64_t outChannelLayout = 0;
    int outNumChannels = 0;
    int outLinesize = 0;
    int inNumSamples = 0;
    int64_t outNumSamples = 0;
    int64_t maxOutNumSamples = 0;
    uint8_t** resampledData = NULL;
    int resampledDataSize = 0;
    
    if( codecContext->channels == av_get_channel_layout_nb_channels( codecContext->channel_layout ) )
        inChannelLayout = codecContext->channel_layout;
    else
        inChannelLayout = av_get_default_channel_layout( codecContext->channels );
    if( inChannelLayout <= 0 )
        Utils::displayException( "Input channel error" );
    if( codecContext->channels == 1 )
    {
        outChannelLayout = AV_CH_LAYOUT_MONO;
    }
    else if( codecContext->channels == 2 )
    {
        outChannelLayout = AV_CH_LAYOUT_STEREO;
    }
    else
    {
        outChannelLayout = AV_CH_LAYOUT_SURROUND;
    }

    inNumSamples = decodedFrame->nb_samples;
    if( inNumSamples <= 0 )
    {
        printf( "in_nb_samples error.\n" );
        return -1;
    }
    av_opt_set_int( swrCtx,"in_channel_layout",inChannelLayout,0 );
    av_opt_set_int( swrCtx, "in_sample_rate", codecContext->sample_rate, 0);
    av_opt_set_sample_fmt( swrCtx,"in_sample_fmt", codecContext->sample_fmt,0);
    av_opt_set_int( swrCtx, "out_channel_layout", outChannelLayout, 0 );
    av_opt_set_int( swrCtx, "out_sample_rate", codecContext->sample_rate, 0 );
    av_opt_set_sample_fmt(swrCtx,"out_sample_fmt",outFormat, 0 );
    if( swr_init( swrCtx ) < 0 )
        Utils::displayException( "Failed to initialize the resampling context.\n" );
    maxOutNumSamples = outNumSamples = av_rescale_rnd( inNumSamples,codecContext->sample_rate, codecContext->sample_rate,AV_ROUND_UP );
    if( maxOutNumSamples <= 0 )
        Utils::displayException( "av_rescale_rnd error.\n" );
    outNumSamples = av_get_channel_layout_nb_channels( outChannelLayout );
    if( av_samples_alloc_array_and_samples( &resampledData, &outLinesize, outNumChannels, outNumSamples, outFormat, 0 ) < 0 )
    {
        return - 1;
    }
        //Utils::displayException( "av_samples_alloc_array_and_samples() error: Could not allocate destination samples.\n" );
    outNumSamples = av_rescale_rnd(
        swr_get_delay( swrCtx, codecContext->sample_rate ) + inNumSamples,
        codecContext->sample_rate,
        codecContext->sample_rate,
        AV_ROUND_UP
    );
    if( outNumSamples <= 0 )
        Utils::displayException( "av_rescale_rnd error" );
    if( outNumSamples > maxOutNumSamples )
    {
        av_free( resampledData[0] );
        if( av_samples_alloc( resampledData, &outLinesize, outNumChannels, outNumSamples, outFormat, 1 ) < 0 )
            Utils::displayException( "av_samples_allocate failed" );
        maxOutNumSamples = outNumSamples;
    }
    if( swrCtx )
    {
        int sampleNumber;
        sampleNumber = swr_convert( swrCtx, resampledData, outNumSamples, (const uint8_t**)decodedFrame->data, decodedFrame->nb_samples );
        if( sampleNumber < 0 )
            Utils::displayException( "swr_convert error" );
        resampledDataSize = av_samples_get_buffer_size( &outLinesize, outNumChannels, sampleNumber, outFormat,1);
        if( resampledDataSize < 0 )
            Utils::displayException( "av_samples_get_buffer_size error" );

    }
    else
        Utils::displayException( "swrContext == NULL" );
    memcpy( outBuffer, resampledData[0], resampledDataSize );
    if( resampledData )
    {
        av_freep( &resampledData[0] );
    }
    av_freep( &resampledData );
    resampledData = NULL;
    if( swrCtx )
    {
        swr_free( &swrCtx );
    }
    return resampledDataSize;
}


void AudioCallback( void* codecContext, Uint8* buffer, int bufferSize )
{
    int len1 = -1;
    unsigned int audioSize = -1;
    double pts;
    AVCodecContext* context = reinterpret_cast<AVCodecContext*>(codecContext);
    while( bufferSize > 0 )
    {
        if( Audio::getInstance()->audioBufIndex >= Audio::getInstance()->audioBufSize )
        {
            audioSize = Audio::getInstance()->decodeFrame( context, Audio::getInstance()->audioBuf, Audio::getInstance()->audioBufSize );
            if( audioSize < 0 )
            {
                Audio::getInstance()->audioBufSize = 1024;
                memset( Audio::getInstance()->audioBuf, 0, Audio::getInstance()->audioBufSize );
                printf( "audio_decode_frame() failed.\n" );
            }
            else
                Audio::getInstance()->audioBufSize = audioSize;
            Audio::getInstance()->audioBufIndex = 0;
        }
        len1 = Audio::getInstance()->audioBufSize - Audio::getInstance()->audioBufIndex;
        if( len1 > bufferSize )
            len1 = bufferSize;
        memcpy( buffer, (uint8_t*)Audio::getInstance()->audioBuf + Audio::getInstance()->audioBufIndex, len1 );
        bufferSize -= len1;
        buffer += len1;
        Audio::getInstance()->audioBufIndex += len1;
    }
}

static int audio_resampling(
    AVCodecContext* audio_decode_ctx,
    AVFrame* decoded_audio_frame,
    enum AVSampleFormat out_sample_fmt,
    int out_channels,
    int out_sample_rate,
    uint8_t* out_buf
)
{
    SwrContext* swr_ctx = NULL;
    int ret = 0;
    int64_t in_channel_layout = audio_decode_ctx->channel_layout;
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

    // get input audio channels
    in_channel_layout = (audio_decode_ctx->channels ==
        av_get_channel_layout_nb_channels( audio_decode_ctx->channel_layout )) ?   // 2
        audio_decode_ctx->channel_layout :
        av_get_default_channel_layout( audio_decode_ctx->channels );

    // check input audio channels correctly retrieved
    if( in_channel_layout <= 0 )
    {
        printf( "in_channel_layout error.\n" );
        return -1;
    }

    // set output audio channels based on the input audio channels
    if( out_channels == 1 )
    {
        out_channel_layout = AV_CH_LAYOUT_MONO;
    }
    else if( out_channels == 2 )
    {
        out_channel_layout = AV_CH_LAYOUT_STEREO;
    }
    else
    {
        out_channel_layout = AV_CH_LAYOUT_SURROUND;
    }

    // retrieve number of audio samples (per channel)
    in_nb_samples = decoded_audio_frame->nb_samples;
    if( in_nb_samples <= 0 )
    {
        printf( "in_nb_samples error.\n" );
        return -1;
    }

    // Set SwrContext parameters for resampling
    av_opt_set_int(   // 3
        swr_ctx,
        "in_channel_layout",
        in_channel_layout,
        0
    );

    // Set SwrContext parameters for resampling
    av_opt_set_int(
        swr_ctx,
        "in_sample_rate",
        audio_decode_ctx->sample_rate,
        0
    );

    // Set SwrContext parameters for resampling
    av_opt_set_sample_fmt(
        swr_ctx,
        "in_sample_fmt",
        audio_decode_ctx->sample_fmt,
        0
    );

    // Set SwrContext parameters for resampling
    av_opt_set_int(
        swr_ctx,
        "out_channel_layout",
        out_channel_layout,
        0
    );

    // Set SwrContext parameters for resampling
    av_opt_set_int(
        swr_ctx,
        "out_sample_rate",
        out_sample_rate,
        0
    );

    // Set SwrContext parameters for resampling
    av_opt_set_sample_fmt(
        swr_ctx,
        "out_sample_fmt",
        out_sample_fmt,
        0
    );

    // Once all values have been set for the SwrContext, it must be initialized
    // with swr_init().
    ret = swr_init( swr_ctx );;
    if( ret < 0 )
    {
        printf( "Failed to initialize the resampling context.\n" );
        return -1;
    }

    max_out_nb_samples = out_nb_samples = av_rescale_rnd(
        in_nb_samples,
        out_sample_rate,
        audio_decode_ctx->sample_rate,
        AV_ROUND_UP
    );

    // check rescaling was successful
    if( max_out_nb_samples <= 0 )
    {
        printf( "av_rescale_rnd error.\n" );
        return -1;
    }

    // get number of output audio channels
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

    // retrieve output samples number taking into account the progressive delay
    out_nb_samples = av_rescale_rnd(
        swr_get_delay( swr_ctx, audio_decode_ctx->sample_rate ) + in_nb_samples,
        out_sample_rate,
        audio_decode_ctx->sample_rate,
        AV_ROUND_UP
    );

    // check output samples number was correctly retrieved
    if( out_nb_samples <= 0 )
    {
        printf( "av_rescale_rnd error\n" );
        return -1;
    }

    if( out_nb_samples > max_out_nb_samples )
    {
        // free memory block and set pointer to NULL
        av_free( resampled_data[0] );

        // Allocate a samples buffer for out_nb_samples samples
        ret = av_samples_alloc(
            resampled_data,
            &out_linesize,
            out_nb_channels,
            out_nb_samples,
            out_sample_fmt,
            1
        );

        // check samples buffer correctly allocated
        if( ret < 0 )
        {
            printf( "av_samples_alloc failed.\n" );
            return -1;
        }

        max_out_nb_samples = out_nb_samples;
    }

    if( swr_ctx )
    {
        // do the actual audio data resampling
        ret = swr_convert(
            swr_ctx,
            resampled_data,
            out_nb_samples,
            (const uint8_t**)decoded_audio_frame->data,
            decoded_audio_frame->nb_samples
        );

        // check audio conversion was successful
        if( ret < 0 )
        {
            printf( "swr_convert_error.\n" );
            return -1;
        }

        // Get the required buffer size for the given audio parameters
        resampled_data_size = av_samples_get_buffer_size(
            &out_linesize,
            out_nb_channels,
            ret,
            out_sample_fmt,
            1
        );

        // check audio buffer size
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

    // copy the resampled data to the output buffer
    memcpy( out_buf, resampled_data[0], resampled_data_size );

    /*
     * Memory Cleanup.
     */
    if( resampled_data )
    {
        // free memory block and set pointer to NULL
        av_freep( &resampled_data[0] );
    }

    av_freep( &resampled_data );
    resampled_data = NULL;

    if( swr_ctx )
    {
        // Free the given SwrContext and set the pointer to NULL
        swr_free( &swr_ctx );
    }

    return resampled_data_size;
}