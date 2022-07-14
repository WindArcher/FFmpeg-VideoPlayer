#pragma once
class DecodeThreadHandler
{
public:
    virtual void startThread() = 0;
    virtual void stopThread() = 0;
};