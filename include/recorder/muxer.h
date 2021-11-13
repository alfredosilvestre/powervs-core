#ifndef MUXER_H
#define MUXER_H

#include "videoencoder.h"
#include "audioencoder.h"

class Muxer
{
    public:
        Muxer();
        ~Muxer();

    public:
        bool initOutputFile(const char* filename, QString format, const char* timecode);
        double muxAudioFrame(AVDecodedFrame* audioBuffer);
        double muxVideoFrame(AVDecodedFrame* videoBuffer);
        int64_t getCurrentRecordTime();
        int64_t closeOutputFile();
		int getVideoCodecAddress();

    private:
        void cleanup();

    private:
        AVFormatContext* outputContext;
        VideoEncoder videoEncoder;
        AudioEncoder audioEncoder;
        double videoTimeBase;
        AVStream* videoStream;
};

#endif // MUXER_H
