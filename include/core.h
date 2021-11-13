#ifndef CORE_H
#define CORE_H

#include "videoport.h"

#include <QList>
#include <QCoreApplication>
#include <QTextStream>
#include <QFile>

class Core
{
    public:
        Core();
        ~Core();

    public:
        static void LogHandler(QtMsgType type, const char* msg);

    // VideoPort functions
    public:
        const int getVideoPortCount() const;
        const int activatePlayout(const int port) const;
        const int deactivatePlayout(const int port) const;

    private:
        VideoPort* getVideoPort(const int port) const;

    // Playout functions
    public:
        const int64_t loadPortItem(const int port, const QString& path, const bool canTake) const;
        const int changeFormat(const int port, const QString& format) const;
        const int64_t getCurrentPlayTime(const int port) const;
        const int64_t getCurrentTimeCode(const int port) const;

        const int recue(const int port, const bool loop) const;
        const int take(const int port) const;
        const int pause(const int port) const;
        const int dropMedia(const int port, const bool immediate) const;

        const int seek(const int port, const int64_t pos) const;
        const int nextFrame(const int port) const;
        const int previousFrame(const int port) const;
        const double forward(const int port) const;
        const double reverse(const int port) const;

    private:
        QCoreApplication* app;
        static QTextStream* stream;
        static QFile* file;
        QList<VideoPort*> videoPortList;

    private:
        void initChannels();
};

#endif // CORE_H
