#pragma once
namespace Player
{
    class IDecodeThreadHandler
    {
    public:
        virtual bool stopDecoding() = 0;
        virtual bool startDecoding() = 0;
        virtual bool pauseDecoding() = 0;
        virtual bool resumeDecoding() = 0;
        virtual bool notifyDecoding() = 0;
    };
};