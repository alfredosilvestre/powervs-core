#ifndef FFDECODER_H
#define FFDECODER_H

#include <QThread>

extern "C"
{
    #include <libavformat/avformat.h>
    #include <libswresample/swresample.h>
    #include <libswscale/swscale.h>
}

class Player;

class FFDecoder : public QThread
{
    Q_OBJECT

    public:
        explicit FFDecoder(Player* channel);
        ~FFDecoder();

    public:
        const int64_t init(const QString& path, const QString& format);
        void startDecoding(const bool preview);
        void stopDecoding();
        void seek(const int64_t pos, const int seek_flag);
        void setRate(const double rate);

    private:
        void run();
        int convertOutput(const uint8_t** inputSamples, const int& input_nb_samples, uint8_t*** outputBuffer);
        void pushAudioFrame(const int& data_size, const double& sr, uint8_t** outputBuffer);
        void createFrames();
        void cleanup();

    // Decoder variables
    private:
        Player* player;

        AVFormatContext* avFormatContext;
        AVCodecContext* videoCodecContext;
        AVCodecContext* audioCodecContext;

        AVStream* videoStream;
        AVStream* audioStream;

        AVFrame* tmpFrame;
        AVFrame* frameUYVY;
        SwsContext* swsContext;

        AVFrame* framePreview;
        SwsContext* swsContextPreview;

        AVFrame* frameAudio;
        SwrContext* swrContext;

        bool decoding;
        double rate;
        bool preview;

    private:
        // Video output variables
        int outputWidth;
        int outputHeight;
        int outputFrameSize;
        AVPixelFormat outputFormat;

        // Preview variables
        int previewWidth;
        int previewHeight;
        int previewOffset;
        int previewFrameSize;
        AVPixelFormat previewFormat;

        // Audio output variables
        int outputChannelCount;
        uint64_t outputChannelLayout;
        int outputSampleRate;
        AVSampleFormat outputSampleFormat;
};

#endif // FFDECODER_H
