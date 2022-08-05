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
#include <mutex>
#include <condition_variable>
#include <deque>
#include <memory>

struct AVPacketDeleter
{
    void operator()( AVPacket* pkt ) { av_packet_free( &pkt ); }
};
using AVPacketPtr = std::unique_ptr<AVPacket, AVPacketDeleter>;
class PacketQueue
{
public:
    bool put( AVPacket* packet );
    AVPacket* get( bool blocking, const bool* quitFlag = nullptr );
    int size() { return m_totalSize; }
    void clear();
private:
    std::deque<AVPacketPtr> m_packetQueue;
    int m_totalSize = 0;
    std::mutex m_mutex;
    std::condition_variable m_cond;
};