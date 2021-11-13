#ifndef AUDIOTHREAD_H
#define AUDIOTHREAD_H

#include "avdecodedframe.h"

#include <QMutex>
#include <QThread>

#ifdef PORTAUDIO
#include <portaudio.h>
#endif

class AudioThread : public QThread
{
    Q_OBJECT

    public:
        explicit AudioThread(bool preview, QObject* parent = 0);
        ~AudioThread();

    public:
        void toggleLoop(const bool loop, const bool active);
        void startPlaying();
        void stopPlaying();
        void cleanup();
        const bool pushAudioFrame(AVDecodedFrame* a);

    private:
        bool playing;
        bool loop;
        bool preview;
        QMutex audioMutex;
        QList<AVDecodedFrame*> audioSamplesList;
    private:
        void run();
#ifdef PORTAUDIO
        static int paCallback(const void* inputBuffer, void* outputBuffer, unsigned long framesPerBuffer, const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags, void* userData);
#endif

    signals:
        void previewAudio(const QByteArray& audioBuffer);
};

#endif // AUDIOTHREAD_H
