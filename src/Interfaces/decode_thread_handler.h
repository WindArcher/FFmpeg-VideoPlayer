#pragma once
namespace Player
{
    class IDecodeThreadHandler
    {
    public:
        virtual bool stopDecoding() = 0;
        virtual bool startDecoding() = 0;
    };
};