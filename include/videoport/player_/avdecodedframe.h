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
        QByteArray* buffer;

    public:
        const int64_t getPTS();
        const int64_t getClockPTS();
        const QByteArray* getBuffer();
};

#endif // AVDECODEDFRAME_H
