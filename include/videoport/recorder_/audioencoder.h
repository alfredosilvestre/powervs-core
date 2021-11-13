#ifndef AUDIOENCODER_H
#define AUDIOENCODER_H

#include <QString>

extern "C"
{
    #include <libavformat/avformat.h>
    #include <libswresample/swresample.h>
}

class AudioEncoder
{
    public:
        AudioEncoder();
        ~AudioEncoder();

    public:
        bool initialize(QString format, AVFormatContext* outputContext);
        double muxAudioFrame(QByteArray audioBuffer);
        void cleanup();

    private:
        AVFormatContext* outputContext;
        AVStream* audioStream;
        AVCodecContext* codecContext;
        AVFrame* frame;
        SwrContext* swrContext;

    private:
        void initIMX();
};

#endif // AUDIOENCODER_H
