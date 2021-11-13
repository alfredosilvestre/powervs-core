#ifndef AUDIOENCODER_H
#define AUDIOENCODER_H

#include "avdecodedframe.h"

#include <QString>
#include <QList>

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
        double muxAudioFrame(AVDecodedFrame* audioBuffer);
        void cleanup();

    private:
        AVFormatContext* outputContext;
        AVStream* audioStream;
        AVCodecContext* audioCodecContext;
        AVFrame* frame;

        SwrContext* swrContext;
        int inChannelCount;
        int outChannelCount;
        uint64_t inChannelLayout;
        uint64_t outChannelLayout;
        int inSampleRate;
        int outSampleRate;
        AVSampleFormat inSampleFormat;
        AVSampleFormat outSampleFormat;

        int bytesPerSample;
        int frameSize;

        QList<AVStream*> streamsList;
        QList<AVCodecContext*> audioCodecContextList;

    private:
        void initIMX(AVCodecContext* newCodecContext);
        void initXDCam(AVStream* newAudioStream, AVCodecContext* newCodecContext);

        void convertAudioBuffer(const uint8_t* audioData, int nb_samples, AVCodecContext* codecContext);
        void encodeAudioFrame(AVCodecContext* codecContext, AVFrame* audioFrame, const AVStream* stream);
};

#endif // AUDIOENCODER_H
