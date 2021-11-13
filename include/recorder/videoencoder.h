#ifndef VIDEOENCODER_H
#define VIDEOENCODER_H

#include "avdecodedframe.h"

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
        double muxVideoFrame(AVDecodedFrame* videoBuffer);
        void cleanup();
		int getVideoCodecAddress();

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
        void initXDCAMHD422_1080p(int rate);
        void initXDCAMHD422_1080i(int rate);

};

#endif // VIDEOENCODER_H
