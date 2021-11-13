#ifndef VIDEOTHREAD_H
#define VIDEOTHREAD_H

#include "avdecodedframe.h"
#include "decklinkcontrol.h"

#include <QMutex>
#include <QThread>

class VideoThread : public QThread
{
    Q_OBJECT

    public:
        explicit VideoThread(QObject *parent = 0);
        ~VideoThread();

    public:
        void setDeckLinkOutput(DeckLinkOutput* deckLinkOutput);
        void startPlaying();
        void stopPlaying();
        void cleanup(const int64_t pos);

        const bool pushVideoFrame(AVDecodedFrame* v, AVDecodedFrame* vp);
        void setStartFrame(const int64_t start_frame);
        void setRate(const double rate);
        const int64_t getCurrentPlayTime() const;

    private:
        bool playing;
        double ratio;
        int64_t startFrame;
        int64_t video_clock;
        QMutex videoMutex;
        QList<AVDecodedFrame*> videoFramesList;
        QList<AVDecodedFrame*> previewFramesList;

        DeckLinkOutput* deckLinkOutput;

    private:
        void run();

    signals:
        void previewVideo(const QByteArray& videoBuffer);
};

#endif // VIDEOTHREAD_H
