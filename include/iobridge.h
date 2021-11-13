#ifndef IOBRIDGE_H
#define IOBRIDGE_H

#ifdef DECKLINK
#include "decklinkcontrol.h"
#endif
#ifdef GUI
#include "previewerrgb.h"
#endif
#include "recorder.h"

#include <QMutex>

extern "C"
{
    #include <libavformat/avformat.h>
    #include <libswscale/swscale.h>
    #include <libavfilter/avfiltergraph.h>
}

class IOBridge : public QObject
{
    Q_OBJECT

    public:
        explicit IOBridge(QObject* parent = 0);
        ~IOBridge();

    // FFMpeg functions
    private:
        void initFFMpeg();
        void cleanupFFMpeg();

    // CG functions
    private:
        const bool initFilters(const QString& cg, const QString& format);
        void cleanupFilters();
    public:
        const void activateFilter(const QString& cg);

    private:
#ifdef GUI
        PreviewerRGB* preview;
#endif
#ifdef DECKLINK
        DeckLinkOutput* deckLinkOutput;
        DeckLinkInput* deckLinkInput;
#endif
        Recorder* recorder;
        IOBridge* ioBridge;

        QMutex videoMutex;
        QMutex audioMutex;
        QList<AVDecodedFrame*> videoFramesList;
        QList<AVDecodedFrame*> audioSamplesList;
        bool recording;

        int width;
        int height;
        int videoOffset;
        int frameSize;
        AVFrame* frameUYVY;
#ifdef GUI
        AVFrame* framePreview;
        int previewWidth;
        int previewSize;
        SwsContext* swsContextPreview;
#endif

        // Filtergraph variables
        AVFrame* filterFrame;
        AVFilterGraph* filterGraph;
        AVFilterContext* buffersinkContext;
        AVFilterContext* buffersrcContext;
        int filterFPS;

        QString currentMediaFormat;
        QString currentCG;

    public:
#ifdef GUI
        void togglePreviewVideo(PreviewerRGB* previewer);
#endif
#ifdef DECKLINK
        const QString setDeckLinkInput(QString inter);
        const DeckLinkInput* getDecklinkInput() const;
        const QString setDeckLinkOutput(QString inter);
        const DeckLinkOutput* getDecklinkOutput() const;
#endif
        const void setIOBridge(IOBridge* ioBridge);
        const void rebootInterface();
        const Recorder* getRecorder() const;
        void changeFormat(const QString& format);

        bool isRecording() const;

        void startRecording();
        void stopRecording();
        void clearBuffers();

        const void pushVideoFrame(uint8_t* v, const int frameSize);
        const void pushAudioFrame(uint8_t* a, const int audioSize);
        AVDecodedFrame* getNextVideoFrame();
        AVDecodedFrame* getNextAudioSample();

        void cleanup();

    // Recorder functions
    public:
        bool startRecording(QString path, QString filename, QString extension, const char* timecode);
        int64_t stopRecording(bool recordRestart);
        int64_t getCurrentRecordTime();
        QString checkFFError(int addr, const char* timecode);
    
    signals:
        void previewVideo(QByteArray videoBuffer);
        void previewAudio(QByteArray audioBuffer);
    
};

#endif // IOBRIDGE_H
