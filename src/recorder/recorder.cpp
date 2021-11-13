#include "recorder.h"
#include "iobridge.h"

#include <QTime>
#include <QTimer>

#include <log4cxx/logger.h>

using namespace log4cxx;

Recorder::Recorder(IOBridge* ioBridge) : QThread(ioBridge)
{
    this->ioBridge = ioBridge;
    currentMediaFormat = "imx30 4:3";
    currentPath = "";
    currentFilename = "";
    currentExtension = "";
    recording = false;
    audioPts = 0;
    videoPts = 0;
    restartTimes = 0;
}

Recorder::~Recorder()
{

}

void Recorder::changeFormat(const QString& format)
{
    if(format == "PAL")
        currentMediaFormat = "imx30 4:3";
    else if(format == "PAL 16:9")
        currentMediaFormat = "imx50 16:9";
    else if(format == "720p50")
        currentMediaFormat = "xdcamHD422_720p 50";
    else if(format == "720p5994")
        currentMediaFormat = "xdcamHD422_720p 60";
    else if(format == "1080p25")
        currentMediaFormat = "xdcamHD422_1080p 25";
    else if(format == "1080i50")
        currentMediaFormat = "xdcamHD422_1080i 25";
    else if(format == "1080i5994")
        currentMediaFormat = "xdcamHD422_1080i 30";
}

bool Recorder::startRecording(QString path, QString filename, QString extension, const char* timecode)
{
    currentPath = path;
    currentFilename = filename;
    currentExtension = extension;

    QString name = path + filename + extension;
    if(!muxer.initOutputFile(name.toStdString().c_str(), currentMediaFormat, timecode))
        return false;

    recording = true;

    return true;
}

void Recorder::run()
{
    if(ioBridge != NULL)
    {
        if(!recording)
            ioBridge->startRecording();

        recording = true;
        while(recording)
        {
            if(audioPts < videoPts)
            {
                AVDecodedFrame* audioBuffer = ioBridge->getNextAudioSample();
                if(audioBuffer != NULL)
                {
                    audioPts = muxer.muxAudioFrame(audioBuffer);
                    delete audioBuffer;
                }
            }
            else
            {
                AVDecodedFrame* videoBuffer = ioBridge->getNextVideoFrame();
                if(videoBuffer != NULL)
                {
                    videoPts = muxer.muxVideoFrame(videoBuffer);
                    delete videoBuffer;
                }
            }
        }
    }
}

int64_t Recorder::getCurrentRecordTime()
{
    return muxer.getCurrentRecordTime();
}

int64_t Recorder::stopRecording(bool recordRestart)
{
    recording = false;
    this->wait();
    recording = recordRestart;

    if(ioBridge != NULL)
    {
        if(!recording)
        {
            ioBridge->stopRecording();

            while(true)
            {
                if(audioPts < videoPts)
                {
                    AVDecodedFrame* audioBuffer = ioBridge->getNextAudioSample();
                    if(audioBuffer != NULL)
                    {
                        audioPts = muxer.muxAudioFrame(audioBuffer);
                        delete audioBuffer;
                    }
                    else break;
                }
                else
                {
                    AVDecodedFrame* videoBuffer = ioBridge->getNextVideoFrame();
                    if(videoBuffer != NULL)
                    {
                        videoPts = muxer.muxVideoFrame(videoBuffer);
                        delete videoBuffer;
                    }
                    else break;
                }
            }

            ioBridge->clearBuffers();
        }
    }

    audioPts = 0;
    videoPts = 0;

    return muxer.closeOutputFile();
}

void Recorder::restartRecording(QString path, QString filename, QString extension, const char* timecode)
{
    restartTimes++;
    this->stopRecording(true);
    // FIXME: there is a random ffmpeg error when recording
    // This workaround restarts the recording on a new file
    // since I did not yet find any other way to detect and fix this issue
    if(restartTimes > 5)
    {
        ioBridge->stopRecording();
        ioBridge->clearBuffers();
        LOG4CXX_ERROR(Logger::getLogger("Recorder"), "Restarted recording 5 times: FFError");
        restartTimes = 0;
    }
    this->startRecording(path, filename, extension, timecode);
    QTimer::singleShot(2000, this, SLOT(start()));
}

QString Recorder::checkFFError(int addr, const char* timecode)
{
    if(recording && addr == muxer.getVideoCodecAddress())
    {
        QString originalFileName = currentFilename;
        QString newFilename = currentFilename + "_" + QDate::currentDate().toString("yyyyMMdd") + "_" + QTime::currentTime().toString("hhmmss");
        this->restartRecording(currentPath, newFilename, currentExtension, timecode);
        currentFilename = originalFileName;
        return currentPath + newFilename + currentExtension;
    }

    return "";
}
