#pragma once
extern "C"
{
#include <libavutil/avutil.h>
}
#include <stdexcept>
class FFmpegException : public std::exception
{
public:
    FFmpegException( const char* msg, int code ) : m_msg( msg ),m_code( code ) {}
    const char* what()
    {
        char error_buf[128];
        av_strerror( m_code, error_buf, 128 );
        snprintf( m_buffer, sizeof m_buffer, "%s%s%s", "SDL Exeption:\n", m_msg, error_buf );
        return m_buffer;
    }

private:
    const char* m_msg;
    int m_code;
    char m_buffer[512]{'\0'};
};