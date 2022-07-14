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
        char ret[512];
        snprintf( ret, sizeof ret, "%s%s%s", "SDL Exeption:\n", m_msg, error_buf );
        return ret;
    }

private:
    const char* m_msg;
    int m_code;
};