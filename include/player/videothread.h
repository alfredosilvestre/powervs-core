#ifndef VIDEOTHREAD_H
#define VIDEOTHREAD_H

#include "avdecodedframe.h"

#include <QMutex>
#include <QThread>

class VideoThread : public QThread
{
    Q_OBJECT

    public:
        explicit VideoThread(QObject* parent = 0);
        ~VideoThread();

    public:
        void toggleLoop(const bool loop, const bool active);
        void startPlaying();
        void stopPlaying();
        void cleanup(const int64_t pos);

        const bool pushVideoFrame(AVDecodedFrame* vp);
        const int64_t getCurrentPlayTime() const;

    private:
        bool playing;
        bool loop;
        bool preview;
        int64_t video_clock;
        QMutex videoMutex;
        QList<AVDecodedFrame*> previewFramesList;

    private:
        void run();

    signals:
        void previewVideo(const QByteArray& videoBuffer);
};

#endif // VIDEOTHREAD_H
