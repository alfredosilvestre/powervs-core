#ifndef RECORDER_H
#define RECORDER_H

#include "decklinkcontrol.h"
#include "muxer.h"

#include <QThread>

class Recorder : public QThread
{
    Q_OBJECT

    public:
        explicit Recorder(QObject *parent = 0);
        ~Recorder();

    private:
        Muxer muxer;
        DeckLinkInput* deckLinkInput;
        QString currentMediaFormat;

        bool recording;
        double videoPts;
        double audioPts;

    public:
        void setDeckLinkInput(DeckLinkInput* deckLinkInput);
        void changeFormat(const QString& format);
        bool startRecording(const char* filename, const char* timecode);
        int64_t getCurrentRecordTime();
        int64_t stopRecording();

    private:
        void run();
};

#endif // RECORDER_H
