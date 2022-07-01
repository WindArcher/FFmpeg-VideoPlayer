#include "AltPacketQueue.h"
#include <stdio.h>
extern "C"
{
#include <libavutil/mem.h>
}
AltPacketQueue::AltPacketQueue()
{
    m_mutex = SDL_CreateMutex();
    if( !m_mutex )
    {
        printf( "AltPacketQueue: SDL_CreateMutex Error: %s.\n", SDL_GetError() );
        return;
    }
    m_cond = SDL_CreateCond();
    if( !m_cond )
    {
        printf( "AltPacketQueue: SDL_CreateCond Error: %s.\n", SDL_GetError() );
        return;
    }
}

AltPacketQueue::~AltPacketQueue()
{
    SDL_DestroyMutex( m_mutex );
    SDL_DestroyCond( m_cond );
}

int AltPacketQueue::put( AVPacket* pkt )
{
    PacketList* avPacketList;
    avPacketList = reinterpret_cast<PacketList*>( av_malloc( sizeof( PacketList ) ));

    // check the PacketList was allocated
    if( !avPacketList )
    {
        return -1;
    }

    // add reference to the given AVPacket
    avPacketList->pkt = *pkt;

    // the new PacketList will be inserted at the end of the queue
    avPacketList->next = NULL;

    // lock mutex
    SDL_LockMutex( m_mutex );

    // check the queue is empty
    if( !m_last_pkt )
    {
        // if it is, insert as first
        m_first_pkt = avPacketList;
    }
    else
    {
        // if not, insert as last
        m_last_pkt->next = avPacketList;
    }

    // point the last PacketList in the queue to the newly created PacketList
    m_last_pkt = avPacketList;

    // increase by 1 the number of AVPackets in the queue
    m_pkgNum++;

    // increase queue size by adding the size of the newly inserted AVPacket
    m_size += avPacketList->pkt.size;

    // notify packet_queue_get which is waiting that a new packet is available
    SDL_CondSignal( m_cond );

    // unlock mutex
    SDL_UnlockMutex( m_mutex );

    return 0;
}

int AltPacketQueue::get( AVPacket* pkt, int blocking )
{
    int ret;

    PacketList* avPacketList;

    // lock mutex
    SDL_LockMutex( m_mutex );

    for( ;;)
    {
        // check quit flag
        if( g_quitFlag )
        {
            ret = -1;
            break;
        }

        // point to the first PacketList in the queue
        avPacketList = m_first_pkt;

        // if the first packet is not NULL, the queue is not empty
        if( avPacketList )
        {
            // place the second packet in the queue at first position
            m_first_pkt = avPacketList->next;

            // check if queue is empty after removal
            if( !m_first_pkt )
            {
                // first_pkt = last_pkt = NULL = empty queue
                m_last_pkt = NULL;
            }

            // decrease the number of packets in the queue
            m_pkgNum--;

            // decrease the size of the packets in the queue
            m_size -= avPacketList->pkt.size;

            // point packet to the extracted packet, this will return to the calling function
            *pkt = avPacketList->pkt;

            // free memory
            av_free( avPacketList );

            ret = 1;
            break;
        }
        else if( !blocking )
        {
            ret = 0;
            break;
        }
        else
        {
            // unlock mutex and wait for cond signal, then lock mutex again
            SDL_CondWait( m_cond, m_mutex );
        }
    }

    // unlock mutex
    SDL_UnlockMutex( m_mutex );

    return ret;
}
