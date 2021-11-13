#include "avdecodedframe.h"

AVDecodedFrame::AVDecodedFrame(AVMediaType type, uint8_t* data, const int size, const int64_t pts, const int64_t clockPts)
{
    this->type = type;
    this->pts = pts * 1000;
    this->clockPts = clockPts;

    frameSize = size;
    buffer = new uint8_t[frameSize];
    memcpy(buffer, data, frameSize);
}

AVDecodedFrame::~AVDecodedFrame()
{
    delete buffer;
}

const int64_t AVDecodedFrame::getPTS()
{
    return pts;
}

const int64_t AVDecodedFrame::getClockPTS()
{
    return clockPts;
}

const uint8_t* AVDecodedFrame::getBuffer()
{
    return buffer;
}

const int AVDecodedFrame::getSize()
{
    return frameSize;
}
