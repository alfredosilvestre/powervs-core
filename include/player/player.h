#ifndef PLAYER_H
#define PLAYER_H

#include "audiothread.h"
#include "ffdecoder.h"
#ifdef GUI
#include "previewerrgb.h"
#endif
#include "videothread.h"

enum PlayingState
{
    IDLE,
    PAUSED,
    PLAYING,
    STOPPING
};

class Player : public QObject
{
    Q_OBJECT

    public:
        explicit Player(QObject *parent = 0);
        ~Player();

    // FFMpeg variables
    private:
        FFDecoder* decoder;
        AudioThread* audioThread;
#ifdef PORTAUDIO
        AudioThread* audioThreadPreview;
#endif
        VideoThread* videoThread;

        int64_t startFrame;
        int64_t startMillisecond;
        double videoRate;
        QString currentMediaFormat;
        QString currentCG;

    // Vars
    private:
        QMutex videoMutex;
        QMutex audioMutex;
        QList<AVDecodedFrame*> videoFramesList;
        QList<AVDecodedFrame*> audioSamplesList;
        bool loop;
        bool loaded;
        PlayingState playing;
#ifdef GUI
        PreviewerRGB* preview;
#endif
#ifdef DECKLINK
        DeckLinkOutput* deckLinkOutput;
#endif

    // FFMpeg functions
    private:
        const int64_t initFFMpeg(const QString& path);
        void cleanupFFMpeg();

    public:
        const bool pushVideoFrame(AVDecodedFrame* v);
        const bool pushVideoFramePreview(AVDecodedFrame* vp);
        const bool pushAudioFrame(AVDecodedFrame* a);
        const bool pushAudioFrameVU(AVDecodedFrame* aVU);
#ifdef PORTAUDIO
        const bool pushAudioFramePreview(AVDecodedFrame* ap);
#endif
        AVDecodedFrame* getNextVideoFrame();
        AVDecodedFrame* getNextAudioSample();
        void setStartMillisecond(const int64_t start_millisecond);
        const int64_t getStartMillisecond() const;
        void setRecueBuffers(AVDecodedFrame* v, AVDecodedFrame* vp);

    // Player functions
    public:
#ifdef GUI
        void togglePreviewVideo(PreviewerRGB* previewer);
        void togglePreviewAudio(bool enabled);
#endif
#ifdef DECKLINK
        const QString setDeckLinkOutput(QString inter);
        const DeckLinkOutput* getDecklinkOutput() const;
#endif

        const int64_t loadMedia(const QString& media_path);
        void waitForDecoding();
        void changeFormat(const QString& format);
        void toggleLoop(const bool loop);

        void stop(const bool immediate);
        void recue();
        void take();
        void pause(const bool resetRate);
        void dropMedia(const bool immediate);

        void seek(const int64_t pos);
        void nextFrame();
        void previousFrame();
        double forward();
        double reverse();

        const int64_t getCurrentPlayTime() const;
        const int64_t getCurrentTimeCode() const;
        const bool isLoaded() const;
        const bool isPlaying() const;
        const bool isPaused() const;
        const bool isEOF() const;

    // CG functions
    public:
        void activateFilter(QString cg);
};

#endif // PLAYER_H
