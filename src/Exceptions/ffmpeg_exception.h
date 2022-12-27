#pragma once
extern "C"
{
#include <libavutil/avutil.h>
}
#include <string>
#include <stdexcept>
class FFmpegException : public std::exception
{
public:
    FFmpegException( const std::string& msg, int code ) 
    {
        m_msg = "FFmpeg exception\n";
        m_msg += msg;
        char error_buf[128];
        av_strerror( code, error_buf, 128 );
        m_msg += error_buf;
    }

    const char* what() const override
    {
        return m_msg.c_str();
    }

private:
    std::string m_msg;
};