#ifndef VIDEOENCODER_H
#define VIDEOENCODER_H

#include <QString>

extern "C"
{
    #include <libavformat/avformat.h>
    #include <libswscale/swscale.h>
}

class VideoEncoder
{
    public:
        VideoEncoder();
        ~VideoEncoder();

    public:
        double initialize(QString format, AVFormatContext* outputContext);
        double muxVideoFrame(uint8_t* videoBuffer);
        void cleanup();

    private:
        AVFormatContext* outputContext;
        AVStream* videoStream;
        AVCodecContext* codecContext;

        int videoWidth;
        int videoHeight;

        AVPixelFormat inputPixFmt;
        AVPixelFormat outputPixFmt;

        AVFrame* frame;
        AVFrame* tmpFrame;
        SwsContext* swsContext;

        int frameSize;

    private:
        void initIMX();
        void initIMX30();
        void initIMX50();
        void initXDCAMHD422();
        void initXDCAMHD422_720p(int rate);
        void initXDCAMHD422_1080i(int rate);

};

#endif // VIDEOENCODER_H
