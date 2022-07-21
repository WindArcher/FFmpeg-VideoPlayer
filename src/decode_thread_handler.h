#pragma once
class DecodeThreadHandler
{
public:
    virtual bool startThread() = 0;
    virtual bool stopThread() = 0;
};