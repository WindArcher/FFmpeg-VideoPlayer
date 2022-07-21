#include "packet_queue.h"

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
        m_totalSize += static_cast<unsigned int>(m_packetQueue.back()->size);
    }
    m_cond.notify_one();
    return true;
}

AVPacket* PacketQueue::get( bool blocking, const bool* quitFlag )
{
    std::unique_lock<std::mutex> lock{ m_mutex };
    while( true )
    {
        if( !m_packetQueue.empty() )
        {
            AVPacket* pkt = av_packet_alloc();
            m_totalSize -= static_cast<unsigned int>(m_packetQueue.front()->size);
            av_packet_move_ref( pkt, m_packetQueue.front().get() );
            m_packetQueue.pop_front();
            return pkt;
        }
        else if( !blocking || quitFlag ? &quitFlag : false )
            return nullptr;
        else
            m_cond.wait( lock );
    }
}

void PacketQueue::clear()
{
    std::unique_lock<std::mutex> lock{ m_mutex };
    m_packetQueue.clear();
    m_totalSize = 0;
}
