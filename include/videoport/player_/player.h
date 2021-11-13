#ifndef PLAYER_H
#define PLAYER_H

#include "audiothread.h"
#include "decklinkcontrol.h"
#include "ffdecoder.h"
#include "videothread.h"

enum PlayingState
{
    IDLE,
    PAUSED,
    PLAYING
};

class Player : public QObject
{
    Q_OBJECT

    public:
        Player();
        ~Player();

    // FFMpeg variables
    private:
        FFDecoder* decoder;
        AudioThread* audioThread;
        VideoThread* videoThread;

        int64_t startFrame;
        int64_t startMillisecond;
        double videoRate;
        QString currentMediaFormat;

        QByteArray recueBuffer;
        QByteArray recueBufferPreview;

    // Vars
    private:
        bool loaded;
        PlayingState playing;
        DeckLinkOutput* deckLinkOutput;

    // FFMpeg functions
    private:
        const int64_t initFFMpeg(const QString& path);
        void cleanupFFMpeg();

    public:
        const bool pushVideoFrame(AVDecodedFrame* v, AVDecodedFrame* vp);
        const bool pushAudioFrame(AVDecodedFrame* a);
        void setStartMillisecond(const int64_t start_millisecond);
        void setRecueBuffers(AVDecodedFrame* v, AVDecodedFrame* vp);

    // Player functions
    public:
        void setDeckLinkInterface(const int inter);

        const int64_t loadMedia(const QString& media_path, const bool loop);
        void changeFormat(const QString& format);
        const int64_t getCurrentPlayTime() const;
        const int64_t getCurrentTimeCode() const;

        void recue(const bool loop);
        void take();
        void pause();
        void dropMedia(const bool immediate);

        void seek(const int64_t pos);
        void nextFrame();
        void previousFrame();
        double forward();
        double reverse();

        const bool isLoaded() const;
        const bool isPlaying() const;
        const bool isPaused() const;
        const bool isEOF() const;
};

#endif // PLAYER_H
