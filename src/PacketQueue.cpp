#include "PacketQueue.h"

bool PacketQueue::put( AVPacket* packet )
{
    {
        std::unique_lock<std::mutex> lock{ m_mutex };
        m_packetQueue.push_back( AVPacketPtr{ av_packet_alloc() } );
        if( av_packet_ref( m_packetQueue.back().get(), packet ) != 0 )
        {
            m_packetQueue.pop_back();
            return false;
        }
    }
    m_cond.notify_one();
    return true;
}

AVPacket* PacketQueue::get( bool blocking )
{
    std::unique_lock<std::mutex> lock{ m_mutex };
    while( true )
    {
        if( !m_packetQueue.empty() )
        {
            AVPacket* pkt = av_packet_alloc();
            av_packet_move_ref( pkt, m_packetQueue.front().get() );
            m_packetQueue.pop_front();
            return pkt;
        }
        else if( !blocking && m_packetQueue.empty() )
            return nullptr;
        else
            m_cond.wait( lock );
    }
}