#pragma once
#include <SDL2/SDL.h>
#include <SDL2/SDL_thread.h>
extern "C"
{
#include <libavcodec/packet.h>
}
#define MAX_AUDIOQ_SIZE ( 5 * 16 * 1024 )
#define MAX_VIDEOQ_SIZE (5 * 256 * 1024)
extern bool g_quitFlag;
typedef struct PacketList
{
    AVPacket pkt;
    struct PacketList* next;
} PacketList;

class AltPacketQueue
{
public:
    AltPacketQueue();
    ~AltPacketQueue();
    int put( AVPacket* pkt );
    int get( AVPacket* pkt,int blocking );
    int getSize() { return m_size; }
private:
    PacketList* m_first_pkt = nullptr;
    PacketList* m_last_pkt = nullptr;
    int m_size;
    int m_pkgNum;
    SDL_mutex* m_mutex;
    SDL_cond* m_cond;
};

