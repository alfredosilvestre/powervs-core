#ifndef AUDIOTHREAD_H
#define AUDIOTHREAD_H

#include "avdecodedframe.h"
#include "decklinkcontrol.h"

#include <QMutex>
#include <QThread>

class AudioThread : public QThread
{
    Q_OBJECT

    public:
        explicit AudioThread(QObject *parent = 0);
        ~AudioThread();

    public:
        void setDeckLinkOutput(DeckLinkOutput* deckLinkOutput);
        void startPlaying();
        void stopPlaying();
        void cleanup();
        const bool pushAudioFrame(AVDecodedFrame* a);

    private:
        bool playing;
        QMutex audioMutex;
        QList<AVDecodedFrame*> audioSamplesList;

        DeckLinkOutput* deckLinkOutput;

    private:
        void run();

    signals:
        void previewAudio(const QByteArray& audioBuffer);
};

#endif // AUDIOTHREAD_H
