#ifndef FFDECODER_H
#define FFDECODER_H

#include "audiosamplearray.h"

#include <QThread>

extern "C"
{
    #include <libavformat/avformat.h>
    #include <libswresample/swresample.h>
    #include <libswscale/swscale.h>
    #include <libavfilter/avfiltergraph.h>
}

#ifdef DECKLINK
#include "decklinkcontrol.h"
#endif

class Player;

class FFDecoder : public QThread
{
    Q_OBJECT

    public:
        explicit FFDecoder(Player* player, bool loop);
        ~FFDecoder();

    public:
#ifdef DECKLINK
        const int64_t init(const QString& path, const QString& format, DeckLinkOutput* deckLinkOutput);
#else
        const int64_t init(const QString& path, const QString& format);
#endif
        void startDecoding();
        void decodeVideo(AVPacket* packet);
        void decodeAudio(AVPacket* packet);
        void stopDecoding();
        void seek(const int64_t pos, const int seek_flag);
        void setRate(const double rate);
        void toggleLoop(const bool loop, const bool active);
        void changeFormat(const QString& format);
        const bool initFilters(const QString& cg, const QString& format);
        void cleanupFilters();

    private:
        void run();
        int convertOutput(SwrContext* swrContext, const uint8_t** inputSamples, const int& input_nb_samples, int nb_channels, uint8_t*** outputBuffer);
#ifdef PORTAUDIO
        void pushAudioFrame(const double& sr, const int& data_size, uint8_t** outputBuffer, const int& data_size_preview, uint8_t** previewBuffer);
#else
		void pushAudioFrame(const double& sr, const int& data_size, uint8_t** outputBuffer);
#endif
        void createFrames(const QString& format);
        void cleanup();

    // Decoder variables
    private:
        Player* player;

        AVFormatContext* avFormatContext;
        QList<AVCodecContext*> codecContextList;
        AVCodecContext* videoCodecContext;
        AVCodecContext* audioCodecContext;

        AVStream* videoStream;
        AVStream* audioStream;

        AVFrame* tmpFrame;
        AVFrame* frameUYVY;
        SwsContext* swsContext;

#ifdef GUI
        AVFrame* framePreview;
        SwsContext* swsContextPreview;
#endif

        AVFrame* frameAudio;
        SwrContext* swrContext;

#ifdef PORTAUDIO
        SwrContext* swrContextPreview;
#endif

        // Filtergraph variables
        AVFrame* filterFrame;
        AVFilterGraph* filterGraph;
        AVFilterContext* buffersinkContext;
        AVFilterContext* buffersrcContext;

        bool decoding;
        bool loop;
        double fps;
        double rate;

        double videoTB;
        double fpsx2;
        double lastPTS;
        bool firstDecodedFrame;
        int64_t totalFrames;

        double sr;
        int bytes_per_sample;

        uint8_t** inputSamples;
        int input_linesize;
        int input_nb_samples;
        SwrContext* swrContextMono;

        int last_audio_index;
        int index;
        QList<AudioSampleArray*> audioSamplesList;

    private:
        // Video output variables
        int outputWidth;
        int outputHeight;
        int outputFrameSize;
        AVPixelFormat outputFormat;

#ifdef GUI
        // Preview variables
        int previewWidth;
        int previewHeight;
        int previewOffset;
        int previewFrameSize;
        AVPixelFormat previewFormat;
#endif

        // Audio output variables
        int outputChannelCount;
        uint64_t outputChannelLayout;
        int outputSampleRate;
        AVSampleFormat outputSampleFormat;
};

#endif // FFDECODER_H
