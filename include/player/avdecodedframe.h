#ifndef AVDECODEDFRAME_H
#define AVDECODEDFRAME_H

#include <QByteArray>

extern "C"
{
    #include <libavformat/avformat.h>
}

class AVDecodedFrame
{
    public:
        AVDecodedFrame(AVMediaType type, uint8_t* data, const int size, const int64_t pts, const int64_t clockPts);
        ~AVDecodedFrame();

    private:
        AVMediaType type;
        int64_t pts;
        int64_t clockPts;
        uint8_t* buffer;
        int frameSize;

    public:
        const int64_t getPTS();
        const int64_t getClockPTS();
        const uint8_t* getBuffer();
        const int getSize();
};

#endif // AVDECODEDFRAME_H
