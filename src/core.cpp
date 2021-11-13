#include "core.h"

#include <QDateTime>
#include <QDir>
#include <QDebug>
#include <QtGlobal>

QTextStream* Core::stream;
QFile* Core::file;

void Core::LogHandler(QtMsgType type, const char* msg)
{
    QString message(msg);
    if(message.contains("QGLShader::link: \"No errors"))
        return;

    message = message.replace("\"", "").trimmed();
    QString date = QDateTime::currentDateTime().toString("[yyyy-MM-dd HH:mm:ss.zzz]");

    switch(type)
    {
        case QtDebugMsg:
            *stream << date << " Info: " << message << "\n";
            break;
        case QtCriticalMsg:
            *stream << date << " Critical: " << message << "\n";
            break;
        case QtWarningMsg:
            *stream << date << " Warning: " << message << "\n";
            break;
        case QtFatalMsg:
            *stream << date <<  " Fatal: " << message << "\n";
            break;
        default:
            break;
    }

    stream->flush();
}

Core::Core()
{
    // Initialize application core
    int argc = 0;
    //char* argv = "";
    app = new QCoreApplication(argc, NULL);
    av_register_all();
	//avformat_network_init();

    // Create log file, directory if necessary and initialize log handler
    QString path = QCoreApplication::applicationDirPath() + "/logs/";
    QDir().mkpath(path);
    path += "log " + QDateTime::currentDateTime().toString("yyyy-MM-dd HHmmss") + ".log";

    file = new QFile(path);
    file->open(QIODevice::WriteOnly | QIODevice::Text);
    stream = new QTextStream();
    stream->setDevice(file);
    //FIXME qInstallMessageHandler(LogHandler);

    this->initChannels();

    qDebug() << "Server started";
}

Core::~Core()
{
    while(!videoPortList.isEmpty())
        delete videoPortList.takeFirst();

    file->close();
	//avformat_network_deinit();
    delete file;
    delete app;
}

void Core::initChannels()
{
    if(DeckLinkControl::init())
    {
        // Initialize the devices list
        QStringList inputList = DeckLinkControl::getInputList();
        QStringList outputList = DeckLinkControl::getOutputList();
        if(inputList.size() == outputList.size())
        {
            for(int i=0; i<inputList.size(); i++)
            {
                VideoPort* vp = new VideoPort(i+1);
                videoPortList.append(vp);
            }
        }
    }
}

// VideoPort functions

const int Core::getVideoPortCount() const
{
    return videoPortList.size();
}

VideoPort* Core::getVideoPort(const int port) const
{
    if(port > 0 && port <= videoPortList.size())
        return videoPortList[port-1];

    return NULL;
}

const int Core::activatePlayout(const int port) const
{
    VideoPort* videoPort = getVideoPort(port);
    if(videoPort != NULL)
        return videoPort->activatePlayout();

    return -1;
}

const int Core::deactivatePlayout(const int port) const
{
    VideoPort* videoPort = getVideoPort(port);
    if(videoPort != NULL)
        return videoPort->deactivatePlayout();

    return -1;
}

// Playout functions

const int64_t Core::loadPortItem(const int port, const QString& path, const bool canTake) const
{
    VideoPort* videoPort = getVideoPort(port);
    if(videoPort != NULL)
        return videoPort->loadItem(path, canTake);

    return -1;
}

const int Core::changeFormat(const int port, const QString& format) const
{
    VideoPort* videoPort = getVideoPort(port);
    if(videoPort != NULL)
        return videoPort->changeFormat(format);

    return -1;
}

const int64_t Core::getCurrentPlayTime(const int port) const
{
    VideoPort* videoPort = getVideoPort(port);
    if(videoPort != NULL)
        return videoPort->getCurrentPlayTime();

    return -1;
}

const int64_t Core::getCurrentTimeCode(const int port) const
{
    VideoPort* videoPort = getVideoPort(port);
    if(videoPort != NULL)
        return videoPort->getCurrentTimeCode();

    return -1;
}

const int Core::recue(const int port, const bool loop) const
{
    VideoPort* videoPort = getVideoPort(port);
    if(videoPort != NULL)
        return videoPort->recue(loop);

    return -1;
}

const int Core::take(const int port) const
{
    VideoPort* videoPort = getVideoPort(port);
    if(videoPort != NULL)
        return videoPort->take();

    return -1;
}

const int Core::pause(const int port) const
{
    VideoPort* videoPort = getVideoPort(port);
    if(videoPort != NULL)
        return videoPort->pause();

    return -1;
}

const int Core::dropMedia(const int port, const bool immediate) const
{
    VideoPort* videoPort = getVideoPort(port);
    if(videoPort != NULL)
        return videoPort->dropMedia(immediate);

    return -1;
}

const int Core::seek(const int port, const int64_t pos) const
{
    VideoPort* videoPort = getVideoPort(port);
    if(videoPort != NULL)
        return videoPort->seek(pos);

    return -1;
}

const int Core::nextFrame(const int port) const
{
    VideoPort* videoPort = getVideoPort(port);
    if(videoPort != NULL)
        return videoPort->nextFrame();

    return -1;
}

const int Core::previousFrame(const int port) const
{
    VideoPort* videoPort = getVideoPort(port);
    if(videoPort != NULL)
        return videoPort->previousFrame();

    return -1;
}

const double Core::forward(const int port) const
{
    VideoPort* videoPort = getVideoPort(port);
    if(videoPort != NULL)
        return videoPort->forward();

    return -1;
}

const double Core::reverse(const int port) const
{
    VideoPort* videoPort = getVideoPort(port);
    if(videoPort != NULL)
        return videoPort->reverse();

    return -1;
}
