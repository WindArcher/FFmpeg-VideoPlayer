#pragma once
#include <SDL2/SDL.h>
extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavdevice/avdevice.h>
#include <libavfilter/avfilter.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/avutil.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
#include <libavutil/frame.h>
#include <libavutil/imgutils.h>
}
#include <deque>
#include <memory>
#include <iostream>
#define MAX_AUDIOQ_SIZE ( 5 * 16 * 1024 )
#define MAX_VIDEOQ_SIZE (5 * 256 * 1024)
struct AVPacketDeleter
{
    void operator()( AVPacket* pkt ) { av_packet_free( &pkt ); }
};
using AVPacketPtr = std::unique_ptr<AVPacket, AVPacketDeleter>;

template<size_t SizeLimit>
class PacketQueue
{
public:
    PacketQueue()
    {
        m_mutex = SDL_CreateMutex();
        if( !m_mutex )
        {
            printf( "PacketQueue: SDL_CreateMutex Error: %s.\n", SDL_GetError() );
            exit(1);
        }
        m_condVar = SDL_CreateCond();
        if( !m_condVar )
        {

            printf( "PacketQueue: SDL_CreateCond Error: %s.\n", SDL_GetError() );
            exit(1);
        }
    }
    ~PacketQueue()
    {
        SDL_DestroyMutex( m_mutex );
        SDL_DestroyCond( m_condVar );
    }

    size_t getSize() { return m_totalSize; }

    int sendTo( AVCodecContext* codecctx )
    {
        SDL_LockMutex( m_mutex );
        AVPacket* pkt{ getPacket() };
        const int ret{ avcodec_send_packet( codecctx, pkt ) };
        if( !pkt )
        {
            if( !ret ) return AVERROR_EOF;
            std::cerr << "Failed to send flush packet: " << ret << std::endl;
            return ret;
        }
        if( ret != AVERROR( EAGAIN ) )
        {
            if( ret < 0 )
                std::cerr << "Failed to send packet: " << ret << std::endl;
            pop();
        }
        SDL_UnlockMutex( m_mutex );
        return ret;
    }

    void setFinished()
    {
        SDL_LockMutex( m_mutex );
        m_finished = true;
        SDL_UnlockMutex( m_mutex );
        SDL_CondSignal( m_condVar );
    }

    bool put( const AVPacket* pkt )
    {
        {
            SDL_LockMutex( m_mutex );
            if( m_totalSize >= SizeLimit )
                return false;

            m_packets.push_back( AVPacketPtr{ av_packet_alloc() } );
            if( av_packet_ref( m_packets.back().get(), pkt ) != 0 )
            {
                m_packets.pop_back();
                return true;
            }

            m_totalSize += static_cast<unsigned int>(m_packets.back()->size);
            SDL_UnlockMutex( m_mutex );
        }
        SDL_CondSignal( m_condVar );
        return true;
    }
private:
    SDL_mutex* m_mutex;
    SDL_cond* m_condVar;
    std::deque<AVPacketPtr> m_packets;
    size_t m_totalSize{ 0 };
    bool m_finished{ false };

    AVPacket* getPacket()
    {
        SDL_LockMutex( m_mutex );
        while( m_packets.empty() && !m_finished )
            SDL_CondWait( m_condVar, m_mutex );
        SDL_UnlockMutex( m_mutex );
        return m_packets.empty() ? nullptr : m_packets.front().get();
    }

    void pop()
    {
        m_totalSize -= static_cast<unsigned int>(m_packets.front()->size);
        m_packets.pop_front();
    }
};
