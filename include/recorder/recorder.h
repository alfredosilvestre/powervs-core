#ifndef RECORDER_H
#define RECORDER_H

#include "muxer.h"

#include <QThread>

class IOBridge;

class Recorder : public QThread
{
    Q_OBJECT

    public:
        explicit Recorder(IOBridge* ioBridge);
        ~Recorder();

    private:
        Muxer muxer;
        IOBridge* ioBridge;
        QString currentMediaFormat;
        QString currentPath;
        QString currentFilename;
        QString currentExtension;

        bool recording;
        double videoPts;
        double audioPts;
        int restartTimes;

    public:
        void changeFormat(const QString& format);
        bool startRecording(QString path, QString filename, QString extension, const char* timecode);
        int64_t getCurrentRecordTime();
        int64_t stopRecording(bool recordRestart);
        void restartRecording(QString path, QString filename, QString extension, const char* timecode);
        QString checkFFError(int addr, const char* timecode);

    private:
        void run();
};

#endif // RECORDER_H
